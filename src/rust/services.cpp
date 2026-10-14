/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file services.cpp Direct shared-world leaves for Rust game owners. */
#include "../stdafx.h"
#include "services_ffi.h"
#ifdef WITH_RUST
#include "../industry_map.h"
#include "../bridge_map.h"
#include "../landscape.h"
#include "../viewport_func.h"
#include "../core/random_func.hpp"
#include "../station_map.h"
#include "../depot_map.h"
#include "../rail_map.h"
#include "../tunnelbridge_map.h"
#include "../bridge.h"
#include "../water_map.h"
#include "tile_type_access.hpp"
#include "../safeguards.h"

OpenTTDTileTypeAllocation GetRustTileTypeAllocation() noexcept
{
	return OpenTTDNativeTileTypeAccess::Acquire();
}

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
static bool MapSvcIsWaterTile(uint32_t tile) noexcept { return IsWaterTile(TileIndex(tile)); }

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
		.is_water_tile = MapSvcIsWaterTile,
	};
	return services;
}
#endif /* WITH_RUST */
