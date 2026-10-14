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
#include "../framerate_type.h"
#include "../station_base.h"
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
static_assert(PROP_AIRCRAFT_SPEED == 0x0C);
static_assert(HVOT_BUS == 4 && HVOT_TRUCK == 8);

static void OPENTTD_VEHICLE_CALL VehicleSvcWriteCounters(OpenTTDVehicle *v, uint8_t tick, uint8_t running, int32_t order_time) noexcept
{
	Vehicle *u = VehicleOf(v);
	RustWriteVehicleTick(u, tick);
	u->running_ticks = running;
	u->current_order_time = order_time;
}
static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcBreakdown(OpenTTDVehicle *v) noexcept
{
	Vehicle *u = VehicleOf(v);
	u->HandleBreakdown();
	return RustVehicleStatus(u);
}
static void OPENTTD_VEHICLE_CALL VehicleSvcCheckOrdersOnly(OpenTTDVehicle *v) noexcept { CheckOrders(VehicleOf(v)); }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteCoordinates(OpenTTDVehicle *v, int32_t x, int32_t y, int32_t z) noexcept
{
	Vehicle *u = VehicleOf(v);
	u->x_pos = x;
	u->y_pos = y;
	u->z_pos = z;
}
static OpenTTDVehicleBreakdown OPENTTD_VEHICLE_CALL VehicleSvcHandleBreakdown(OpenTTDVehicle *v) noexcept
{
	Vehicle *u = VehicleOf(v);
	bool broken = u->HandleBreakdown();
	return {.broken = broken, .status = RustVehicleStatus(u)};
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
	return RustVehicleStatus(u);
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
static void OPENTTD_VEHICLE_CALL VehicleSvcUnknownLengthResult(const OpenTTDEngine *e, uint16_t result) noexcept
{
	ErrorUnknownCallbackResult(EngineOf(e)->GetGRFID(), CBID_VEHICLE_LENGTH, result);
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
static uint16_t OPENTTD_VEHICLE_CALL VehicleSvcLastStation(OpenTTDVehicle *v) noexcept { return VehicleOf(v)->last_station_visited.base(); }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteLastStation(OpenTTDVehicle *v, uint16_t station) noexcept { VehicleOf(v)->last_station_visited = StationID(station); }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteDest(OpenTTDVehicle *v, uint32_t tile) noexcept { VehicleOf(v)->dest_tile = TileIndex(tile); }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteProgress(OpenTTDVehicle *v, uint8_t progress) noexcept { RustWriteVehicleProgress(VehicleOf(v), progress); }

static OpenTTDVehiclePreviousTile OPENTTD_VEHICLE_CALL VehicleSvcPreviousTile(OpenTTDVehicle *v) noexcept
{
	const Vehicle *previous = VehicleOf(v)->Previous();
	return {.tile = previous == nullptr ? 0 : RustVehicleTile(previous), .exists = previous != nullptr};
}
/** `DeleteLastRoadVeh` before the road stop check: unlink the last part and copy the station. */
static void OPENTTD_VEHICLE_CALL VehicleSvcDetachLast(OpenTTDVehicle *previous, OpenTTDVehicle *last, OpenTTDVehicle *first) noexcept
{
	VehicleOf(previous)->SetNext(nullptr);
	VehicleOf(last)->last_station_visited = VehicleOf(first)->last_station_visited;
}
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteMaxSpeed(OpenTTDVehicle *v, uint16_t speed) noexcept { VehicleOf(v)->vcache.cached_max_speed = speed; }

static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcProgress(OpenTTDVehicle *v) noexcept { return RustVehicleProgress(VehicleOf(v)); }
static uint32_t OPENTTD_VEHICLE_CALL VehicleSvcDestTile(OpenTTDVehicle *v) noexcept { return RustVehicleDestTile(VehicleOf(v)); }
static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcOrderType(OpenTTDVehicle *v) noexcept { return RustVehicleOrderType(VehicleOf(v)); }
static uint16_t OPENTTD_VEHICLE_CALL VehicleSvcOrderDestination(OpenTTDVehicle *v) noexcept { return RustVehicleOrderDestination(VehicleOf(v)); }
static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcOrderNonStop(OpenTTDVehicle *v) noexcept { return RustVehicleOrderNonStop(VehicleOf(v)); }
static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcDirection(OpenTTDVehicle *v) noexcept { return RustVehicleDirection(VehicleOf(v)); }
static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcStatus(OpenTTDVehicle *v) noexcept { return RustVehicleStatus(VehicleOf(v)); }
static uint32_t OPENTTD_VEHICLE_CALL VehicleSvcTile(OpenTTDVehicle *v) noexcept { return RustVehicleTile(VehicleOf(v)); }
static OpenTTDVehicle *OPENTTD_VEHICLE_CALL VehicleSvcNext(OpenTTDVehicle *v) noexcept { return RustVehicle(RustVehicleNext(VehicleOf(v))); }
static uint16_t OPENTTD_VEHICLE_CALL VehicleSvcEngineID(OpenTTDVehicle *v) noexcept { return RustVehicleEngineID(VehicleOf(v)); }
static OpenTTDEngineForLength OPENTTD_VEHICLE_CALL VehicleSvcEngineForLength(OpenTTDVehicle *v) noexcept
{
	const Engine *e = VehicleOf(v)->GetEngine();
	return {.engine = RustEngine(e), .grf_version = RustEngineGRFVersion(e)};
}

static void OPENTTD_VEHICLE_CALL VehicleSvcWriteTick(OpenTTDVehicle *v, uint8_t tick) noexcept { RustWriteVehicleTick(VehicleOf(v), tick); }

static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcSubtype(OpenTTDVehicle *v) noexcept { return RustVehicleSubtype(VehicleOf(v)); }
static int32_t OPENTTD_VEHICLE_CALL VehicleSvcX(OpenTTDVehicle *v) noexcept { return RustVehicleX(VehicleOf(v)); }
static int32_t OPENTTD_VEHICLE_CALL VehicleSvcY(OpenTTDVehicle *v) noexcept { return RustVehicleY(VehicleOf(v)); }
static int32_t OPENTTD_VEHICLE_CALL VehicleSvcZ(OpenTTDVehicle *v) noexcept { return RustVehicleZ(VehicleOf(v)); }
static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcTick(OpenTTDVehicle *v) noexcept { return RustVehicleTick(VehicleOf(v)); }
static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcOwner(OpenTTDVehicle *v) noexcept { return RustVehicleOwner(VehicleOf(v)); }
static uint16_t OPENTTD_VEHICLE_CALL VehicleSvcSpeed(OpenTTDVehicle *v) noexcept { return RustVehicleSpeed(VehicleOf(v)); }
static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcSubspeed(OpenTTDVehicle *v) noexcept { return RustVehicleSubspeed(VehicleOf(v)); }
static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcRunning(OpenTTDVehicle *v) noexcept { return RustVehicleRunning(VehicleOf(v)); }
static int32_t OPENTTD_VEHICLE_CALL VehicleSvcOrderTime(OpenTTDVehicle *v) noexcept { return RustVehicleOrderTime(VehicleOf(v)); }
static uint8_t OPENTTD_VEHICLE_CALL VehicleSvcVehicleType(OpenTTDVehicle *v) noexcept { return RustVehicleVehicleType(VehicleOf(v)); }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteX(OpenTTDVehicle *v, int32_t value) noexcept { VehicleOf(v)->x_pos = value; }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteY(OpenTTDVehicle *v, int32_t value) noexcept { VehicleOf(v)->y_pos = value; }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteZ(OpenTTDVehicle *v, int32_t value) noexcept { VehicleOf(v)->z_pos = value; }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteTile(OpenTTDVehicle *v, uint32_t value) noexcept { VehicleOf(v)->tile = TileIndex(value); }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteStatus(OpenTTDVehicle *v, uint8_t value) noexcept { VehicleOf(v)->vehstatus = VehStates{value}; }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteSubspeed(OpenTTDVehicle *v, uint8_t value) noexcept { VehicleOf(v)->subspeed = value; }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteBreakdownCounter(OpenTTDVehicle *v, uint8_t value) noexcept { VehicleOf(v)->breakdown_ctr = value; }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteRunning(OpenTTDVehicle *v, uint8_t value) noexcept { VehicleOf(v)->running_ticks = value; }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteOrderTime(OpenTTDVehicle *v, int32_t value) noexcept { VehicleOf(v)->current_order_time = value; }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteProfit(OpenTTDVehicle *v, int64_t value) noexcept { VehicleOf(v)->profit_this_year = value; }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteEconomyService(OpenTTDVehicle *v, int32_t value) noexcept { VehicleOf(v)->date_of_last_service = TimerGameEconomy::Date{value}; }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteCalendarService(OpenTTDVehicle *v, int32_t value) noexcept { VehicleOf(v)->date_of_last_service_newgrf = TimerGameCalendar::Date{value}; }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteBreakdowns(OpenTTDVehicle *v, uint8_t value) noexcept { VehicleOf(v)->breakdowns_since_last_service = value; }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteReliability(OpenTTDVehicle *v, uint16_t value) noexcept { VehicleOf(v)->reliability = value; }
static void OPENTTD_VEHICLE_CALL VehicleSvcWriteCargoAge(OpenTTDVehicle *v, uint16_t value) noexcept { VehicleOf(v)->vcache.cached_cargo_age_period = value; }
static OpenTTDVehicleCoordinates OPENTTD_VEHICLE_CALL VehicleSvcCoordinates(OpenTTDVehicle *v) noexcept
{
	const Vehicle *u = VehicleOf(v);
	return {.x = RustVehicleX(u), .y = RustVehicleY(u), .z = RustVehicleZ(u)};
}
static OpenTTDVehicleXY OPENTTD_VEHICLE_CALL VehicleSvcXy(OpenTTDVehicle *v) noexcept
{
	const Vehicle *u = VehicleOf(v);
	return {.x = RustVehicleX(u), .y = RustVehicleY(u)};
}
static OpenTTDVehicleSpeedInputs OPENTTD_VEHICLE_CALL VehicleSvcSpeedInputs(OpenTTDVehicle *v) noexcept
{
	const Vehicle *u = VehicleOf(v);
	return {.speed = RustVehicleSpeed(u), .max_speed = RustVehicleMaxSpeed(u), .acceleration = RustVehicleAcceleration(u), .status = RustVehicleStatus(u), .subspeed = RustVehicleSubspeed(u)};
}
static OpenTTDVehicleDirectionProgress OPENTTD_VEHICLE_CALL VehicleSvcDirectionProgress(OpenTTDVehicle *v) noexcept
{
	const Vehicle *u = VehicleOf(v);
	return {.direction = RustVehicleDirection(u), .progress = RustVehicleProgress(u)};
}
static OpenTTDVehicleTickStatus OPENTTD_VEHICLE_CALL VehicleSvcTickStatus(OpenTTDVehicle *v) noexcept
{
	const Vehicle *u = VehicleOf(v);
	return {.tick = RustVehicleTick(u), .status = RustVehicleStatus(u)};
}
static OpenTTDVehicleDayCost OPENTTD_VEHICLE_CALL VehicleSvcDayCost(OpenTTDVehicle *v) noexcept
{
	const Vehicle *u = VehicleOf(v);
	return {.profit = RustVehicleProfit(u), .running = RustVehicleRunning(u)};
}
static void OPENTTD_VEHICLE_CALL VehicleSvcMakeDummy(OpenTTDVehicle *v) noexcept { VehicleOf(v)->current_order.MakeDummy(); }
static void OPENTTD_VEHICLE_CALL VehicleSvcLoading(OpenTTDVehicle *v, bool second) noexcept { VehicleOf(v)->HandleLoading(second); }

static OpenTTDVehicleAltitudeInputs OPENTTD_VEHICLE_CALL VehicleSvcAltitudeInputs(OpenTTDVehicle *v) noexcept
{
	const Vehicle *u = VehicleOf(v);
	return {.max_speed = RustVehicleMaxSpeed(u), .direction = RustVehicleDirection(u)};
}

const OpenTTDVehicleServices &GetRustVehicleServices() noexcept
{
	static const OpenTTDVehicleServices services{
		.write_counters = VehicleSvcWriteCounters,
		.handle_breakdown = VehicleSvcHandleBreakdown,
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
		.last_station = VehicleSvcLastStation,
		.write_last_station = VehicleSvcWriteLastStation,
		.write_dest = VehicleSvcWriteDest,
		.write_progress = VehicleSvcWriteProgress,
		.write_max_speed = VehicleSvcWriteMaxSpeed,
		.previous_tile = VehicleSvcPreviousTile,
		.detach_last = VehicleSvcDetachLast,
		.next = VehicleSvcNext,
		.engine_id = VehicleSvcEngineID,
		.engine_for_length = VehicleSvcEngineForLength,
		.write_tick = VehicleSvcWriteTick,
		.direction = VehicleSvcDirection,
		.status = VehicleSvcStatus,
		.tile = VehicleSvcTile,
		.progress = VehicleSvcProgress,
		.dest_tile = VehicleSvcDestTile,
		.order_type = VehicleSvcOrderType,
		.order_destination = VehicleSvcOrderDestination,
		.order_nonstop = VehicleSvcOrderNonStop,
		.subtype = VehicleSvcSubtype,
		.x = VehicleSvcX,
		.y = VehicleSvcY,
		.z = VehicleSvcZ,
		.tick = VehicleSvcTick,
		.owner = VehicleSvcOwner,
		.speed = VehicleSvcSpeed,
		.subspeed = VehicleSvcSubspeed,
		.running = VehicleSvcRunning,
		.order_time = VehicleSvcOrderTime,
		.vehicle_type = VehicleSvcVehicleType,
		.write_x = VehicleSvcWriteX,
		.write_y = VehicleSvcWriteY,
		.write_z = VehicleSvcWriteZ,
		.write_tile = VehicleSvcWriteTile,
		.write_status = VehicleSvcWriteStatus,
		.write_subspeed = VehicleSvcWriteSubspeed,
		.write_breakdown_counter = VehicleSvcWriteBreakdownCounter,
		.write_running = VehicleSvcWriteRunning,
		.write_order_time = VehicleSvcWriteOrderTime,
		.write_profit = VehicleSvcWriteProfit,
		.write_economy_service = VehicleSvcWriteEconomyService,
		.write_calendar_service = VehicleSvcWriteCalendarService,
		.write_breakdowns = VehicleSvcWriteBreakdowns,
		.write_reliability = VehicleSvcWriteReliability,
		.write_cargo_age = VehicleSvcWriteCargoAge,
		.coordinates = VehicleSvcCoordinates,
		.xy = VehicleSvcXy,
		.speed_inputs = VehicleSvcSpeedInputs,
		.direction_progress = VehicleSvcDirectionProgress,
		.tick_status = VehicleSvcTickStatus,
		.day_cost = VehicleSvcDayCost,
		.make_dummy = VehicleSvcMakeDummy,
		.loading = VehicleSvcLoading,
		.breakdown = VehicleSvcBreakdown,
		.check_orders_only = VehicleSvcCheckOrdersOnly,
		.write_coordinates = VehicleSvcWriteCoordinates,
		.altitude_inputs = VehicleSvcAltitudeInputs,
	};
	return services;
}


static OpenTTDVehiclePoolCursor OPENTTD_VEHICLE_CALL VehicleSchedulerPool() noexcept
{
	return {.slots = _vehicle_pool.data.data(), .slots_len = _vehicle_pool.data.size(), .first_unused = &_vehicle_pool.first_unused};
}
/* Existing virtual facades own the original controller timing scopes. The
 * timing destructor completes before this callback returns to Rust assertions. */
static bool OPENTTD_VEHICLE_CALL VehicleSchedulerTick(OpenTTDVehicle *shell) noexcept { return VehicleOf(shell)->Tick(); }
static void OPENTTD_VEHICLE_CALL VehicleSchedulerCalendarDay(OpenTTDVehicle *shell) noexcept { VehicleOf(shell)->OnNewCalendarDay(); }
static void OPENTTD_VEHICLE_CALL VehicleSchedulerEconomyDay(OpenTTDVehicle *shell) noexcept { VehicleOf(shell)->OnNewEconomyDay(); }
static OpenTTDVehiclePostTick OPENTTD_VEHICLE_CALL VehicleSchedulerPostTick(OpenTTDVehicle *shell) noexcept
{
	Vehicle *v = VehicleOf(shell);
	const VehicleType type = v->type; // Fresh after Tick survives and its assertion.
	if (type > VEH_AIRCRAFT) return {.type = type};
	Vehicle *front = v->First();
	return {.type = type, .front = RustVehicle(front), .period = RustVehicleCargoPeriodAddress(v), .counter = RustVehicleCargoCounterAddress(v), .motion = RustVehicleMotionAddress(v), .speed = RustVehicleSpeedAddress(front), .front_type = RustVehicleVehicleTypeAddress(front), .subtype = RustVehicleSubtypeAddress(v), .tick = RustVehicleTickAddress(v), .front_status = RustVehicleStatusAddress(front), .status = RustVehicleStatusAddress(v)};
}
static OpenTTDCargoList *OPENTTD_VEHICLE_CALL VehicleSchedulerCargoOwner(OpenTTDVehicle *shell) noexcept { return VehicleOf(shell)->cargo.RustOwner(); }
static OpenTTDVehicleDayCallback OPENTTD_VEHICLE_CALL VehicleSchedulerDayCallbackRead(OpenTTDVehicle *shell) noexcept
{
	const Vehicle *v = VehicleOf(shell);
	return {.day = v->day_counter, .has_engine = v->HasEngineType()};
}
static uint16_t OPENTTD_VEHICLE_CALL VehicleSchedulerDayCallback(OpenTTDVehicle *shell) noexcept
{
	Vehicle *v = VehicleOf(shell);
	return GetVehicleCallback(CBID_VEHICLE_32DAY_CALLBACK, 0, 0, v->engine_type, v);
}
static void OPENTTD_VEHICLE_CALL VehicleSchedulerRandomiseCallback(OpenTTDVehicle *shell) noexcept
{
	TriggerVehicleRandomisation(VehicleOf(shell), VehicleRandomTrigger::Callback32);
}
static void OPENTTD_VEHICLE_CALL VehicleSchedulerFirstDirty(OpenTTDVehicle *shell) noexcept { VehicleOf(shell)->First()->MarkDirty(); }
static void OPENTTD_VEHICLE_CALL VehicleSchedulerUnknownDayCallback(OpenTTDVehicle *shell, uint16_t result) noexcept
{
	ErrorUnknownCallbackResult(VehicleOf(shell)->GetGRFID(), CBID_VEHICLE_32DAY_CALLBACK, result);
}
static void OPENTTD_VEHICLE_CALL VehicleSchedulerRunningSound(OpenTTDVehicle *shell) noexcept { PlayVehicleSound(VehicleOf(shell), VSE_RUNNING); }
static void OPENTTD_VEHICLE_CALL VehicleSchedulerPeriodicSound(OpenTTDVehicle *shell, bool running) noexcept { PlayVehicleSound(VehicleOf(shell), running ? VSE_RUNNING_16 : VSE_STOPPED_16); }
static OpenTTDVehicleStationLookup OPENTTD_VEHICLE_CALL VehicleSchedulerNextStation(uint32_t from) noexcept
{
	auto range = Station::Iterate(from);
	auto it = range.begin();
	if (it == range.end()) return {};
	Station *st = *it;
	return {.station = st->rust_service.get(), .id = st->index.base()};
}
static uint64_t OPENTTD_VEHICLE_CALL VehicleSchedulerEconomyBegin() noexcept { return PerformanceMeasurer::Begin(PFE_GL_ECONOMY); }
static void OPENTTD_VEHICLE_CALL VehicleSchedulerEconomyEnd(uint64_t start) noexcept { PerformanceMeasurer::End(PFE_GL_ECONOMY, start); }
static void OPENTTD_VEHICLE_CALL VehicleSchedulerResetAccumulators() noexcept
{
	PerformanceAccumulator::Reset(PFE_GL_TRAINS);
	PerformanceAccumulator::Reset(PFE_GL_ROADVEHS);
	PerformanceAccumulator::Reset(PFE_GL_SHIPS);
	PerformanceAccumulator::Reset(PFE_GL_AIRCRAFT);
}

const OpenTTDVehicleSchedulerServices &GetRustVehicleSchedulerServices() noexcept
{
	static const OpenTTDVehicleSchedulerServices services{
		.pool = VehicleSchedulerPool,
#if !defined(NDEBUG) || defined(WITH_ASSERT)
		.assertions = true,
#else
		.assertions = false,
#endif
		.tick = VehicleSchedulerTick,
		.post_tick = VehicleSchedulerPostTick,
		.calendar_day = VehicleSchedulerCalendarDay,
		.economy_day = VehicleSchedulerEconomyDay,
		.day_callback_read = VehicleSchedulerDayCallbackRead,
		.day_callback = VehicleSchedulerDayCallback,
		.randomise_callback = VehicleSchedulerRandomiseCallback,
		.first_dirty = VehicleSchedulerFirstDirty,
		.unknown_day_callback = VehicleSchedulerUnknownDayCallback,
		.cargo_owner = VehicleSchedulerCargoOwner,
		.running_sound = VehicleSchedulerRunningSound,
		.periodic_sound = VehicleSchedulerPeriodicSound,
		.next_station = VehicleSchedulerNextStation,
		.economy_begin = VehicleSchedulerEconomyBegin,
		.economy_end = VehicleSchedulerEconomyEnd,
		.reset_accumulators = VehicleSchedulerResetAccumulators,
	};
	return services;
}

#endif /* WITH_RUST */
