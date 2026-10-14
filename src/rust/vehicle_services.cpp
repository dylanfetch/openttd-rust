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
#include "../train.h"
#include "../roadveh.h"
#include "../ship.h"
#include "../aircraft.h"
#include "../station_base.h"
#include "../framerate_type.h"
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
	RustWriteVehicleTick(u, tick);
	u->running_ticks = running;
	u->current_order_time = order_time;
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
	};
	return services;
}

static_assert(VEH_TRAIN == 0 && VEH_ROAD == 1 && VEH_SHIP == 2 && VEH_AIRCRAFT == 3 && VEH_EFFECT == 4 && VEH_DISASTER == 5);
static_assert(GVSF_FRONT == 0 && GVSF_ENGINE == 3 && AIR_AIRCRAFT == 2);
static_assert(VehStates{VehState::TrainSlowing}.base() == 16);

static OpenTTDVehicleDispatch OPENTTD_VEHICLE_CALL VehicleSchedulerDispatch(OpenTTDVehicle *shell) noexcept
{
	Vehicle *v = VehicleOf(shell);
	OpenTTDVehicleDispatch result{.vehicle = shell, .owner = {.road = nullptr}, .id = v->index.base(), .type = v->type, .subtype = RustVehicleSubtype(v), .road_tick = 0};
	switch (v->type) {
		case VEH_ROAD: result.owner.road = RoadVehicle::From(v)->GetRustState(); break;
		case VEH_TRAIN: result.owner.train = Train::From(v)->GetRustState(); break;
		case VEH_SHIP: result.owner.ship = Ship::From(v)->GetRustState(); break;
		case VEH_AIRCRAFT: result.owner.aircraft = Aircraft::From(v)->rust_state.get(); break;
		default: break;
	}
	return result;
}
static OpenTTDVehicleDispatch OPENTTD_VEHICLE_CALL VehicleSchedulerNext(uint32_t from) noexcept
{
	/* A pool leaf searches holes, with the same live bound as PoolIterator.
	 * The numeric cursor belongs to Rust; no terminal bound or pointer survives Tick. */
	auto range = Vehicle::Iterate(from);
	auto it = range.begin();
	if (it == range.end()) return {};
	Vehicle *v = *it;
	auto result = VehicleSchedulerDispatch(RustVehicle(v));
	if (v->type == VEH_ROAD) result.road_tick = RustVehicleTick(v);
	return result;
}
static OpenTTDVehicleDayLookup OPENTTD_VEHICLE_CALL VehicleSchedulerDayAt(uint32_t id) noexcept
{
	if (id >= Vehicle::GetPoolSize()) return {.vehicle = nullptr, .end = true};
	return {.vehicle = RustVehicle(Vehicle::Get(id)), .end = false};
}
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
/* Effect/disaster retain their existing facade as the explicit #168 tail. */
static bool OPENTTD_VEHICLE_CALL VehicleSchedulerOtherTick(OpenTTDVehicle *shell) noexcept { return VehicleOf(shell)->Tick(); }
#if !defined(NDEBUG) || defined(WITH_ASSERT)
static void OPENTTD_VEHICLE_CALL VehicleSchedulerAssertSurvival(OpenTTDVehicle *shell, uint32_t id, bool survives) noexcept
{
	assert(Vehicle::Get(id) == (survives ? VehicleOf(shell) : nullptr));
}
#endif
static OpenTTDVehicleCargoAddresses OPENTTD_VEHICLE_CALL VehicleSchedulerCargoAddresses(OpenTTDVehicle *shell) noexcept
{
	Vehicle *v = VehicleOf(shell);
	return {.front = RustVehicle(v->First()), .period = RustVehicleCargoPeriodAddress(v), .counter = RustVehicleCargoCounterAddress(v)};
}
/* One canonical shell-to-cargo-owner leaf, used only when aging actually runs. */
static OpenTTDCargoList *OPENTTD_VEHICLE_CALL VehicleSchedulerCargoOwner(OpenTTDVehicle *shell) noexcept { return VehicleOf(shell)->cargo.RustOwner(); }
static OpenTTDVehicleMotionAddresses OPENTTD_VEHICLE_CALL VehicleSchedulerMotionAddresses(OpenTTDVehicle *shell, OpenTTDVehicle *first) noexcept
{
	Vehicle *v = VehicleOf(shell);
	const Vehicle *front = VehicleOf(first);
	return {.motion = RustVehicleMotionAddress(v), .speed = RustVehicleSpeedAddress(front), .front_type = RustVehicleTypeAddress(front), .subtype = RustVehicleSubtypeAddress(v), .tick = RustVehicleTickAddress(v)};
}
static void OPENTTD_VEHICLE_CALL VehicleSchedulerRunningSound(OpenTTDVehicle *shell) noexcept { PlayVehicleSound(VehicleOf(shell), VSE_RUNNING); }
static void OPENTTD_VEHICLE_CALL VehicleSchedulerPeriodicSound(OpenTTDVehicle *shell, bool running) noexcept { PlayVehicleSound(VehicleOf(shell), running ? VSE_RUNNING_16 : VSE_STOPPED_16); }
static OpenTTDVehicleStationLookup OPENTTD_VEHICLE_CALL VehicleSchedulerNextStation(uint32_t from) noexcept
{
	auto range = Station::Iterate(from);
	auto it = range.begin();
	if (it == range.end()) return {};
	Station *st = *it;
	return {.station = st, .id = st->index.base()};
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
template <PerformanceElement Element>
static uint64_t OPENTTD_VEHICLE_CALL VehicleSchedulerAccumulatorBegin() noexcept { return PerformanceAccumulator::Begin(Element); }
template <PerformanceElement Element>
static void OPENTTD_VEHICLE_CALL VehicleSchedulerAccumulatorEnd(uint64_t start) noexcept { PerformanceAccumulator::End(Element, start); }

const OpenTTDVehicleSchedulerServices &GetRustVehicleSchedulerServices() noexcept
{
	static const OpenTTDVehicleSchedulerServices services{
		.next = VehicleSchedulerNext,
		.day_at = VehicleSchedulerDayAt,
		.dispatch = VehicleSchedulerDispatch,
		.day_callback_read = VehicleSchedulerDayCallbackRead,
		.day_callback = VehicleSchedulerDayCallback,
		.randomise_callback = VehicleSchedulerRandomiseCallback,
		.first_dirty = VehicleSchedulerFirstDirty,
		.unknown_day_callback = VehicleSchedulerUnknownDayCallback,
		.other_tick = VehicleSchedulerOtherTick,
#if !defined(NDEBUG) || defined(WITH_ASSERT)
		.assert_survival = VehicleSchedulerAssertSurvival,
#else
		.assert_survival = nullptr,
#endif
		.cargo_addresses = VehicleSchedulerCargoAddresses,
		.cargo_owner = VehicleSchedulerCargoOwner,
		.motion_addresses = VehicleSchedulerMotionAddresses,
		.running_sound = VehicleSchedulerRunningSound,
		.periodic_sound = VehicleSchedulerPeriodicSound,
		.next_station = VehicleSchedulerNextStation,
		.economy_begin = VehicleSchedulerEconomyBegin,
		.economy_end = VehicleSchedulerEconomyEnd,
		.reset_accumulators = VehicleSchedulerResetAccumulators,
		.train_begin = VehicleSchedulerAccumulatorBegin<PFE_GL_TRAINS>,
		.road_begin = VehicleSchedulerAccumulatorBegin<PFE_GL_ROADVEHS>,
		.ship_begin = VehicleSchedulerAccumulatorBegin<PFE_GL_SHIPS>,
		.aircraft_begin = VehicleSchedulerAccumulatorBegin<PFE_GL_AIRCRAFT>,
		.train_end = VehicleSchedulerAccumulatorEnd<PFE_GL_TRAINS>,
		.road_end = VehicleSchedulerAccumulatorEnd<PFE_GL_ROADVEHS>,
		.ship_end = VehicleSchedulerAccumulatorEnd<PFE_GL_SHIPS>,
		.aircraft_end = VehicleSchedulerAccumulatorEnd<PFE_GL_AIRCRAFT>,
	};
	return services;
}

#endif /* WITH_RUST */
