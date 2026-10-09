/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_services.cpp Shared typed vehicle and map services for Rust ports. */
#include "../stdafx.h"
#include "vehicle_ffi.h"
#ifdef WITH_RUST
#include "vehicle_services.h"
#include "../vehicle_func.h"
#include "../order_func.h"
#include "../newgrf_engine.h"
#include "../engine_base.h"
#include "../newgrf_sound.h"
#include "../newgrf_callbacks.h"
#include "../sound_func.h"
#include "../effectvehicle_func.h"
#include "../economy_type.h"
#include "../station_map.h"
#include "../depot_map.h"
#include "../rail_map.h"
#include "../tunnelbridge_map.h"
#include "../bridge.h"
#include "../tile_cmd.h"
#include "../track_type.h"
#include "../window_func.h"
#include "../widgets/vehicle_widget.h"
#include "../sound_type.h"
#include "../safeguards.h"

/* Values the Rust side names in rust/openttd-kernels/src/vehicle.rs. */
static_assert(MP_ROAD == 2 && MP_STATION == 5 && MP_TUNNELBRIDGE == 9);
static_assert(OT_GOTO_STATION == 1 && OT_GOTO_DEPOT == 2 && OT_LOADING == 3 && OT_LEAVESTATION == 4);
static_assert(VehStates{VehState::Hidden}.base() == 1 && VehStates{VehState::Stopped}.base() == 2 && VehStates{VehState::Crashed}.base() == 128);
static_assert(VehicleEnterTileStates{VehicleEnterTileState::EnteredWormhole}.base() == 2 && VehicleEnterTileStates{VehicleEnterTileState::CannotEnter}.base() == 4);
static_assert(AM_REALISTIC == 1 && INVALID_TRACKDIR == 0xFF && INVALID_PRICE == 0xFF && CALLBACK_FAILED == 0xFFFF && VEHICLE_LENGTH == 8);
static_assert(EngineID::Invalid().base() == 0xFFFF);
static_assert(CalendarTime::DAYS_IN_YEAR * Ticks::DAY_TICKS == 365 * 74);
static_assert(SND_19_DEPARTURE_OLD_RV_1 == 23 && SND_1A_DEPARTURE_OLD_RV_2 == 24);
static_assert(HVOT_BUS == 4 && HVOT_TRUCK == 8);

static void OPENTTD_VEHICLE_CALL VehicleSvcWriteCounters(OpenTTDVehicle *v, uint8_t tick, uint8_t running, int32_t order_time) noexcept
{
	Vehicle *u = VehicleOf(v);
	u->tick_counter = tick;
	u->running_ticks = running;
	u->current_order_time = order_time;
}
static OpenTTDVehicleBreakdown OPENTTD_VEHICLE_CALL VehicleSvcHandleBreakdown(OpenTTDVehicle *v) noexcept
{
	Vehicle *u = VehicleOf(v);
	bool broken = u->HandleBreakdown();
	return {.broken = broken, .status = u->vehstatus.base()};
}
static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcProcessOrdersThenLoading(OpenTTDVehicle *v) noexcept
{
	Vehicle *u = VehicleOf(v);
	ProcessOrders(u);
	u->HandleLoading();
	return u->current_order.GetType();
}
static bool OPENTTD_VEHICLE_CALL VehicleSvcWaitingForUnbunching(OpenTTDVehicle *v) noexcept { return VehicleOf(v)->IsWaitingForUnbunching(); }
static void OPENTTD_VEHICLE_CALL VehicleSvcLeaveUnbunchingDepot(OpenTTDVehicle *v) noexcept { VehicleOf(v)->LeaveUnbunchingDepot(); }
static void OPENTTD_VEHICLE_CALL VehicleSvcResetDepotUnbunching(OpenTTDVehicle *v) noexcept { VehicleOf(v)->ResetDepotUnbunching(); }
static void OPENTTD_VEHICLE_CALL VehicleSvcServiceInDepot(OpenTTDVehicle *v) noexcept { VehicleServiceInDepot(VehicleOf(v)); }
static bool OPENTTD_VEHICLE_CALL VehicleSvcNeedsAutomaticServicing(OpenTTDVehicle *v) noexcept { return VehicleOf(v)->NeedsAutomaticServicing(); }
static void OPENTTD_VEHICLE_CALL VehicleSvcEnterDepot(OpenTTDVehicle *v) noexcept { VehicleEnterDepot(VehicleOf(v)); }
static void OPENTTD_VEHICLE_CALL VehicleSvcPathfindingResult(OpenTTDVehicle *v, bool found) noexcept { VehicleOf(v)->HandlePathfindingResult(found); }
static OpenTTDVehicleNewPosition OPENTTD_VEHICLE_CALL VehicleSvcNewPosition(OpenTTDVehicle *v) noexcept
{
	GetNewVehiclePosResult gp = GetNewVehiclePos(VehicleOf(v));
	return {.x = gp.x, .y = gp.y, .tile = gp.new_tile.base()};
}
static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcMoveThenStatus(OpenTTDVehicle *v, int32_t x, int32_t y) noexcept
{
	Vehicle *u = VehicleOf(v);
	u->x_pos = x;
	u->y_pos = y;
	u->UpdatePosition();
	return u->vehstatus.base();
}
static void OPENTTD_VEHICLE_CALL VehicleSvcBaseViewport(OpenTTDVehicle *v) noexcept { VehicleOf(v)->Vehicle::UpdateViewport(true); }
static int32_t OPENTTD_VEHICLE_CALL VehicleSvcProperty(OpenTTDVehicle *v, uint8_t property, int32_t fallback) noexcept
{
	return GetVehicleProperty(VehicleOf(v), static_cast<PropertyID>(property), fallback);
}
static uint16_t OPENTTD_VEHICLE_CALL VehicleSvcLengthCallback(OpenTTDVehicle *v) noexcept
{
	Vehicle *u = VehicleOf(v);
	return GetVehicleCallback(CBID_VEHICLE_LENGTH, 0, 0, u->engine_type, u);
}
static void OPENTTD_VEHICLE_CALL VehicleSvcUnknownLengthResult(OpenTTDVehicle *v, uint16_t result) noexcept
{
	ErrorUnknownCallbackResult(VehicleOf(v)->GetEngine()->GetGRFID(), CBID_VEHICLE_LENGTH, result);
}
static void OPENTTD_VEHICLE_CALL VehicleSvcLengthChanged(OpenTTDVehicle *v) noexcept { VehicleLengthChanged(VehicleOf(v)); }
static bool OPENTTD_VEHICLE_CALL VehicleSvcPlayStartSound(OpenTTDVehicle *v) noexcept { return PlayVehicleSound(VehicleOf(v), VSE_START); }
static void OPENTTD_VEHICLE_CALL VehicleSvcPlaySound(OpenTTDVehicle *v, uint16_t sound) noexcept { SndPlayVehicleFx(static_cast<SoundID>(sound), VehicleOf(v)); }
static void OPENTTD_VEHICLE_CALL VehicleSvcLargeExplosion(OpenTTDVehicle *v) noexcept { CreateEffectVehicleRel(VehicleOf(v), 4, 4, 8, EV_EXPLOSION_LARGE); }
static void OPENTTD_VEHICLE_CALL VehicleSvcOrderFree(OpenTTDVehicle *v) noexcept { VehicleOf(v)->current_order.Free(); }
static void OPENTTD_VEHICLE_CALL VehicleSvcOrderDummy(OpenTTDVehicle *v) noexcept
{
	Vehicle *u = VehicleOf(v);
	u->current_order.MakeDummy();
	SetWindowWidgetDirty(WC_VEHICLE_VIEW, u->index, WID_VV_START_STOP);
}
static void OPENTTD_VEHICLE_CALL VehicleSvcStartStopDirty(OpenTTDVehicle *v) noexcept { SetWindowWidgetDirty(WC_VEHICLE_VIEW, VehicleOf(v)->index, WID_VV_START_STOP); }
static void OPENTTD_VEHICLE_CALL VehicleSvcDestroy(OpenTTDVehicle *v) noexcept { delete VehicleOf(v); }
static void OPENTTD_VEHICLE_CALL VehicleSvcAge(OpenTTDVehicle *v) noexcept { AgeVehicle(VehicleOf(v)); }
static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcEconomyAge(OpenTTDVehicle *v) noexcept
{
	Vehicle *u = VehicleOf(v);
	EconomyAgeVehicle(u);
	return u->day_counter;
}
static void OPENTTD_VEHICLE_CALL VehicleSvcDecreaseValue(OpenTTDVehicle *v) noexcept { DecreaseVehicleValue(VehicleOf(v)); }
static void OPENTTD_VEHICLE_CALL VehicleSvcCheckBreakdown(OpenTTDVehicle *v) noexcept { CheckVehicleBreakdown(VehicleOf(v)); }
static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcCheckOrders(OpenTTDVehicle *v) noexcept
{
	Vehicle *u = VehicleOf(v);
	CheckOrders(u);
	return u->running_ticks;
}
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteDay(OpenTTDVehicle *v, uint8_t day) noexcept { VehicleOf(v)->day_counter = day; }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteDirection(OpenTTDVehicle *v, uint8_t direction) noexcept { VehicleOf(v)->direction = static_cast<Direction>(direction); }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteSpeed(OpenTTDVehicle *v, uint16_t speed) noexcept { VehicleOf(v)->cur_speed = speed; }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteLastStation(OpenTTDVehicle *v, uint16_t station) noexcept { VehicleOf(v)->last_station_visited = StationID(station); }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteDest(OpenTTDVehicle *v, uint32_t tile) noexcept { VehicleOf(v)->dest_tile = TileIndex(tile); }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteProgress(OpenTTDVehicle *v, uint8_t progress) noexcept { VehicleOf(v)->progress = progress; }

const OpenTTDVehicleServices &GetRustVehicleServices() noexcept
{
	static const OpenTTDVehicleServices services{
		.write_counters = VehicleSvcWriteCounters,
		.handle_breakdown = VehicleSvcHandleBreakdown,
		.process_orders_then_loading = VehicleSvcProcessOrdersThenLoading,
		.waiting_for_unbunching = VehicleSvcWaitingForUnbunching,
		.leave_unbunching_depot = VehicleSvcLeaveUnbunchingDepot,
		.reset_depot_unbunching = VehicleSvcResetDepotUnbunching,
		.service_in_depot = VehicleSvcServiceInDepot,
		.needs_automatic_servicing = VehicleSvcNeedsAutomaticServicing,
		.enter_depot = VehicleSvcEnterDepot,
		.pathfinding_result = VehicleSvcPathfindingResult,
		.new_position = VehicleSvcNewPosition,
		.move_then_status = VehicleSvcMoveThenStatus,
		.base_viewport = VehicleSvcBaseViewport,
		.property = VehicleSvcProperty,
		.length_callback = VehicleSvcLengthCallback,
		.unknown_length_result = VehicleSvcUnknownLengthResult,
		.length_changed = VehicleSvcLengthChanged,
		.play_start_sound = VehicleSvcPlayStartSound,
		.play_sound = VehicleSvcPlaySound,
		.large_explosion = VehicleSvcLargeExplosion,
		.order_free = VehicleSvcOrderFree,
		.order_dummy = VehicleSvcOrderDummy,
		.start_stop_dirty = VehicleSvcStartStopDirty,
		.destroy = VehicleSvcDestroy,
		.age = VehicleSvcAge,
		.economy_age = VehicleSvcEconomyAge,
		.decrease_value = VehicleSvcDecreaseValue,
		.check_breakdown = VehicleSvcCheckBreakdown,
		.check_orders = VehicleSvcCheckOrders,
		.write_day = VehicleSvcWriteDay,
		.write_direction = VehicleSvcWriteDirection,
		.write_speed = VehicleSvcWriteSpeed,
		.write_last_station = VehicleSvcWriteLastStation,
		.write_dest = VehicleSvcWriteDest,
		.write_progress = VehicleSvcWriteProgress,
	};
	return services;
}

static uint8_t OPENTTD_VEHICLE_CALL MapSvcTileType(uint32_t tile) noexcept { return GetTileType(TileIndex(tile)); }
static uint8_t OPENTTD_VEHICLE_CALL MapSvcTileOwner(uint32_t tile) noexcept { return GetTileOwner(TileIndex(tile)).base(); }
static uint16_t OPENTTD_VEHICLE_CALL MapSvcStationIndex(uint32_t tile) noexcept { return GetStationIndex(TileIndex(tile)).base(); }
static uint16_t OPENTTD_VEHICLE_CALL MapSvcDepotIndex(uint32_t tile) noexcept { return GetDepotIndex(TileIndex(tile)).base(); }
static bool OPENTTD_VEHICLE_CALL MapSvcIsLevelCrossing(uint32_t tile) noexcept { return IsLevelCrossingTile(TileIndex(tile)); }
static uint8_t OPENTTD_VEHICLE_CALL MapSvcTunnelBridgeDirection(uint32_t tile) noexcept { return GetTunnelBridgeDirection(TileIndex(tile)); }
static uint16_t OPENTTD_VEHICLE_CALL MapSvcBridgeSpeed(uint32_t tile) noexcept { return GetBridgeSpec(GetBridgeType(TileIndex(tile)))->speed; }

const OpenTTDMapServices &GetRustMapServices() noexcept
{
	static const OpenTTDMapServices services{
		.tile_type = MapSvcTileType,
		.tile_owner = MapSvcTileOwner,
		.station_index = MapSvcStationIndex,
		.depot_index = MapSvcDepotIndex,
		.is_level_crossing = MapSvcIsLevelCrossing,
		.tunnel_bridge_direction = MapSvcTunnelBridgeDirection,
		.bridge_speed = MapSvcBridgeSpeed,
	};
	return services;
}
#endif /* WITH_RUST */
