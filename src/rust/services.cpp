/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file services.cpp Direct shared-world leaves for Rust game owners. */
#include "../stdafx.h"
#include "services_ffi.h"
#include "command_services.h"
#include "company_services.h"
#ifdef WITH_RUST
#include "../industry_map.h"
#include "../bridge_map.h"
#include "../rail_map.h"
#include "../road_map.h"
#include "../station_map.h"
#include "../tunnelbridge_map.h"
#include "../water_map.h"
#include "../landscape.h"
#include "../viewport_func.h"
#include "../core/random_func.hpp"
#include "../depot_map.h"
#include "../bridge.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_type.h"
#include "../command_func.h"
#include "../landscape_cmd.h"
#include "../town.h"
#include "../timer/timer_game_calendar.h"
#include "../safeguards.h"

static CommandCost *NativeCost(OpenTTDCommandCost *cost) noexcept { return reinterpret_cast<CommandCost *>(cost); }
static const CommandCost *NativeCost(const OpenTTDCommandCost *cost) noexcept { return reinterpret_cast<const CommandCost *>(cost); }
static void CostZero(OpenTTDCommandCost *out) noexcept { *NativeCost(out) = CommandCost(); }
static void CostError(OpenTTDCommandCost *out, uint32_t message) noexcept { *NativeCost(out) = CommandCost(message); }
static void CostExpenseMoney(OpenTTDCommandCost *out, uint8_t expense, int64_t amount) noexcept { *NativeCost(out) = CommandCost(static_cast<ExpensesType>(expense), Money(amount)); }
static void CostErrorParam(OpenTTDCommandCost *out, uint32_t message, uint64_t parameter) noexcept { *NativeCost(out) = CommandCostWithParam(message, parameter); }
static void CostAdd(OpenTTDCommandCost *out, OpenTTDCommandCost *item) noexcept { NativeCost(out)->AddCost(std::move(*NativeCost(item))); }
static void CostMove(OpenTTDCommandCost *out, OpenTTDCommandCost *item) noexcept { *NativeCost(out) = std::move(*NativeCost(item)); }
static void CostAmount(OpenTTDCommandCost *out, int64_t amount) noexcept { NativeCost(out)->AddCost(Money(amount)); }
static bool CostSuccess(const OpenTTDCommandCost *cost) noexcept { return NativeCost(cost)->Succeeded(); }
static uint32_t CostErrorMessage(const OpenTTDCommandCost *cost) noexcept { return NativeCost(cost)->GetErrorMessage(); }
static int64_t CostMoney(const OpenTTDCommandCost *cost) noexcept { return NativeCost(cost)->GetCost(); }
const OpenTTDCommandCostServices &GetRustCommandCostServices() noexcept
{
	static const OpenTTDCommandCostServices services{
		.zero = CostZero, .error = CostError, .expense_money = CostExpenseMoney,
		.error_param = CostErrorParam, .add = CostAdd, .move = CostMove,
		.amount = CostAmount, .success = CostSuccess, .error_message = CostErrorMessage, .money = CostMoney,
	};
	return services;
}

/* Direct command execution and its tile callbacks do not run a script VM or
 * save/load code. They may reenter Rust owners, but ordinary play cannot unwind
 * these leaves; allocation/error-formatting failures terminate under noexcept.
 * The native returned object is moved whole into its caller's stack sink. */
static void LandscapeClear(OpenTTDCommandCost *out, uint32_t tile, uint16_t flags) noexcept
{
	*NativeCost(out) = Command<CMD_LANDSCAPE_CLEAR>::Do(DoCommandFlags(flags), TileIndex{tile});
}
const OpenTTDCommandServices &GetRustCommandServices() noexcept
{
	static const OpenTTDCommandServices services{.landscape_clear = LandscapeClear};
	return services;
}

static uint8_t SharedSnowLine() noexcept { return GetSnowLine(); }
static int32_t SharedCalendarYear() noexcept { return TimerGameCalendar::year.base(); }

static uint32_t SharedRandom(void *) noexcept { return Random(); }
static uint8_t SharedTileType(uint32_t tile) noexcept { return GetTileType(TileIndex{tile}); }
static bool SharedBridgeAbove(uint32_t tile) noexcept { return IsBridgeAbove(TileIndex{tile}); }
static uint8_t SharedTropicZone(uint32_t tile) noexcept { return GetTropicZone(TileIndex{tile}); }
static int32_t SharedTileZ(uint32_t tile) noexcept { return GetTileZ(TileIndex{tile}); }
static uint8_t SharedTileSlope(uint32_t tile) noexcept { return GetTileSlope(TileIndex{tile}); }
static void SharedMarkDirty(uint32_t tile) noexcept { MarkTileDirtyByTile(TileIndex{tile}); }
static void SharedSetTropicZone(uint32_t tile, uint8_t zone) noexcept { SetTropicZone(TileIndex{tile}, static_cast<TropicZone>(zone)); }

/** Pure map query; no allocation, reentry or C++ exception crosses into Rust. */
static uint32_t SharedIndustry(int32_t x, int32_t y, uint32_t *index) noexcept
{
	TileIndex tile = TileVirtXY(x, y);
	*index = tile.base();
	if (!IsTileType(tile, MP_INDUSTRY)) return 0;
	return GetIndustryGfx(tile) == GFX_BUBBLE_CATCHER ? 2 : 1;
}

static uint8_t MapSvcTileOwner(uint32_t tile) noexcept { return GetTileOwner(TileIndex(tile)).base(); }
static uint16_t MapSvcStationIndex(uint32_t tile) noexcept { return GetStationIndex(TileIndex(tile)).base(); }
static uint16_t MapSvcDepotIndex(uint32_t tile) noexcept { return GetDepotIndex(TileIndex(tile)).base(); }
static bool MapSvcIsLevelCrossing(uint32_t tile) noexcept { return IsLevelCrossingTile(TileIndex(tile)); }
static uint8_t MapSvcTunnelBridgeDirection(uint32_t tile) noexcept { return GetTunnelBridgeDirection(TileIndex(tile)); }
static uint16_t MapSvcBridgeSpeed(uint32_t tile) noexcept { return GetBridgeSpec(GetBridgeType(TileIndex(tile)))->speed; }

static uint8_t SharedCurrentCompany() noexcept { return OpenTTDCurrentCompany().base(); }
static bool SharedCompanyValid(uint8_t company) noexcept { return Company::IsValidID(CompanyID(company)); }
static uint8_t SharedNextCompany(uint8_t from) noexcept
{
	auto range = Company::Iterate(from);
	auto it = range.begin();
	return it == range.end() ? CompanyID::Invalid().base() : (*it)->index.base();
}

static const OpenTTDTownHandle *SharedClosestTown(uint32_t tile, uint32_t threshold) noexcept
{
	return reinterpret_cast<const OpenTTDTownHandle *>(ClosestTownFromTile(TileIndex{tile}, threshold));
}
static OpenTTDTownState *SharedTownOwner(const OpenTTDTownHandle *town) noexcept
{
	return reinterpret_cast<const Town *>(town)->growth_owner.get();
}

static bool Shared_tile_valid(uint32_t tile) noexcept { return IsValidTile(TileIndex{tile}); }
static uint32_t Shared_distance_from_edge(uint32_t tile) noexcept { return DistanceFromEdge(TileIndex{tile}); }
static int32_t Shared_tile_max_z(uint32_t tile) noexcept { return GetTileMaxZ(TileIndex{tile}); }
static uint32_t Shared_tile_height(uint32_t tile) noexcept { return TileHeight(TileIndex{tile}); }
static uint8_t Shared_bridge_axis(uint32_t tile) noexcept { return GetBridgeAxis(TileIndex{tile}); }
static bool Shared_has_water_ground(uint32_t tile) noexcept { return HasTileWaterGround(TileIndex{tile}); }
static bool Shared_is_water(uint32_t tile) noexcept { return IsWaterTile(TileIndex{tile}); }
static bool Shared_is_sea(uint32_t tile) noexcept { return IsSea(TileIndex{tile}); }
static bool Shared_is_plain_rail(uint32_t tile) noexcept { return IsPlainRailTile(TileIndex{tile}); }
static bool Shared_is_bridge(uint32_t tile) noexcept { return IsBridgeTile(TileIndex{tile}); }
static uint8_t Shared_tunnel_transport(uint32_t tile) noexcept { return GetTunnelBridgeTransportType(TileIndex{tile}); }
static uint32_t Shared_other_tunnel_end(uint32_t tile) noexcept { return GetOtherTunnelBridgeEnd(TileIndex{tile}).base(); }
static bool Shared_is_road_depot(uint32_t tile) noexcept { return IsRoadDepotTile(TileIndex{tile}); }
static uint8_t Shared_road_depot_direction(uint32_t tile) noexcept { return GetRoadDepotDirection(TileIndex{tile}); }
static bool Shared_is_normal_road(uint32_t tile) noexcept { return IsNormalRoadTile(TileIndex{tile}); }
static uint8_t Shared_disallowed_road_directions(uint32_t tile) noexcept { return GetDisallowedRoadDirections(TileIndex{tile}); }
static bool Shared_is_bay_stop(uint32_t tile) noexcept { return IsBayRoadStopTile(TileIndex{tile}); }
static uint8_t Shared_bay_stop_direction(uint32_t tile) noexcept { return GetBayRoadStopDir(TileIndex{tile}); }
static bool Shared_is_drive_through_stop(uint32_t tile) noexcept { return IsDriveThroughStopTile(TileIndex{tile}); }
static uint8_t Shared_drive_through_axis(uint32_t tile) noexcept { return GetDriveThroughStopAxis(TileIndex{tile}); }
static bool Shared_is_any_road_stop(uint32_t tile) noexcept { return IsAnyRoadStopTile(TileIndex{tile}); }
static uint8_t Shared_road_type(uint32_t tile) noexcept { return GetRoadTypeRoad(TileIndex{tile}); }
static bool Shared_has_road_type(uint32_t tile) noexcept { return HasTileRoadType(TileIndex{tile}, RTT_ROAD); }
static uint8_t Shared_road_owner(uint32_t tile) noexcept { return GetRoadOwner(TileIndex{tile}, RTT_ROAD).base(); }
static uint16_t Shared_town_index(uint32_t tile) noexcept { return GetTownIndex(TileIndex{tile}).base(); }
static uint8_t Shared_road_bits(uint32_t tile, uint8_t type, bool straight) noexcept { return GetAnyRoadBits(TileIndex{tile}, static_cast<RoadTramType>(type), straight); }
const OpenTTDSharedServices &GetRustSharedServices() noexcept
{
	static const OpenTTDSharedServices services{
		.context = nullptr,
		.random = SharedRandom,
		.industry = SharedIndustry,
		.tile_type = SharedTileType,
		.bridge_above = SharedBridgeAbove,
		.tropic_zone = SharedTropicZone,
		.tile_z = SharedTileZ,
		.tile_slope = SharedTileSlope,
		.mark_dirty = SharedMarkDirty,
		.set_tropic_zone = SharedSetTropicZone,
		.tile_owner = MapSvcTileOwner,
		.station_index = MapSvcStationIndex,
		.depot_index = MapSvcDepotIndex,
		.is_level_crossing = MapSvcIsLevelCrossing,
		.tunnel_bridge_direction = MapSvcTunnelBridgeDirection,
		.bridge_speed = MapSvcBridgeSpeed,
		.company_valid = SharedCompanyValid, .next_company = SharedNextCompany, .current_company = SharedCurrentCompany,
		.closest_town = SharedClosestTown, .town_owner = SharedTownOwner,
		.tile_valid = Shared_tile_valid,
		.distance_from_edge = Shared_distance_from_edge,
		.tile_max_z = Shared_tile_max_z,
		.tile_height = Shared_tile_height,
		.bridge_axis = Shared_bridge_axis,
		.has_water_ground = Shared_has_water_ground,
		.is_water = Shared_is_water,
		.is_sea = Shared_is_sea,
		.is_plain_rail = Shared_is_plain_rail,
		.is_bridge = Shared_is_bridge,
		.tunnel_transport = Shared_tunnel_transport,
		.other_tunnel_end = Shared_other_tunnel_end,
		.is_road_depot = Shared_is_road_depot,
		.road_depot_direction = Shared_road_depot_direction,
		.is_normal_road = Shared_is_normal_road,
		.disallowed_road_directions = Shared_disallowed_road_directions,
		.is_bay_stop = Shared_is_bay_stop,
		.bay_stop_direction = Shared_bay_stop_direction,
		.is_drive_through_stop = Shared_is_drive_through_stop,
		.drive_through_axis = Shared_drive_through_axis,
		.is_any_road_stop = Shared_is_any_road_stop,
		.road_type = Shared_road_type,
		.has_road_type = Shared_has_road_type,
		.road_owner = Shared_road_owner,
		.town_index = Shared_town_index,
		.road_bits = Shared_road_bits,
		.snow_line = SharedSnowLine,
		.calendar_year = SharedCalendarYear,
	};
	return services;
}
#endif /* WITH_RUST */
