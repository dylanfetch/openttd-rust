/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_ffi.h Shared typed vehicle, ground-vehicle and map services for Rust ports. */
#ifndef RUST_VEHICLE_FFI_H
#define RUST_VEHICLE_FFI_H
#include <cstdint>
#if defined(_MSC_VER)
#define OPENTTD_VEHICLE_CALL __cdecl
#elif defined(__i386__)
#define OPENTTD_VEHICLE_CALL __attribute__((cdecl))
#else
#define OPENTTD_VEHICLE_CALL
#endif
/** Opaque handle: a live `Vehicle *` of the port's type, valid for one synchronous call.
 * Each service is one typed noexcept function defined once; it resolves its
 * vehicle once and reads only the fields it returns. Rust holds no reference into
 * a vehicle or the map across a service call; an escaping exception terminates. */
struct OpenTTDVehicle;
/** Captured immutable engine context, valid across synchronous vehicle callbacks. */
struct OpenTTDEngine;
struct OpenTTDEngineForLength {
	const OpenTTDEngine *engine;
	uint8_t grf_version;
};
struct OpenTTDVehicleNewPosition {
	int32_t x;
	int32_t y;
	uint32_t tile;
};
struct OpenTTDVehicleBreakdown {
	bool broken;
	uint8_t status;
};
struct OpenTTDVehiclePreviousTile {
	uint32_t tile;
	bool exists;
};
struct OpenTTDVehicleServices {
	void (OPENTTD_VEHICLE_CALL *write_counters)(OpenTTDVehicle *, uint8_t, uint8_t, int32_t) noexcept;
	OpenTTDVehicleBreakdown (OPENTTD_VEHICLE_CALL *handle_breakdown)(OpenTTDVehicle *) noexcept;
	bool (OPENTTD_VEHICLE_CALL *waiting_for_unbunching)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *leave_unbunching_depot)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *reset_depot_unbunching)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *service_in_depot)(OpenTTDVehicle *) noexcept;
	bool (OPENTTD_VEHICLE_CALL *needs_automatic_servicing)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *enter_depot)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *pathfinding_result)(OpenTTDVehicle *, bool) noexcept;
	OpenTTDVehicleNewPosition (OPENTTD_VEHICLE_CALL *new_position)(OpenTTDVehicle *) noexcept;
	uint8_t (OPENTTD_VEHICLE_CALL *move_then_status)(OpenTTDVehicle *, int32_t, int32_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *base_viewport)(OpenTTDVehicle *) noexcept;
	int32_t (OPENTTD_VEHICLE_CALL *property)(OpenTTDVehicle *, uint8_t, int32_t) noexcept;
	uint16_t (OPENTTD_VEHICLE_CALL *length_callback)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *unknown_length_result)(const OpenTTDEngine *, uint16_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *length_changed)(OpenTTDVehicle *) noexcept;
	bool (OPENTTD_VEHICLE_CALL *play_start_sound)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *play_sound)(OpenTTDVehicle *, uint16_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *large_explosion)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *order_free)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *order_dummy)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *start_stop_dirty)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *destroy)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *age)(OpenTTDVehicle *) noexcept;
	uint8_t (OPENTTD_VEHICLE_CALL *economy_age)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *decrease_value)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *check_breakdown)(OpenTTDVehicle *) noexcept;
	uint8_t (OPENTTD_VEHICLE_CALL *check_orders)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *write_day)(OpenTTDVehicle *, uint8_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *write_direction)(OpenTTDVehicle *, uint8_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *write_speed)(OpenTTDVehicle *, uint16_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *write_last_station)(OpenTTDVehicle *, uint16_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *write_dest)(OpenTTDVehicle *, uint32_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *write_progress)(OpenTTDVehicle *, uint8_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *write_max_speed)(OpenTTDVehicle *, uint16_t) noexcept;
	OpenTTDVehiclePreviousTile (OPENTTD_VEHICLE_CALL *previous_tile)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *detach_last)(OpenTTDVehicle *, OpenTTDVehicle *, OpenTTDVehicle *) noexcept;
	OpenTTDVehicle *(OPENTTD_VEHICLE_CALL *next)(OpenTTDVehicle *) noexcept;
	uint16_t (OPENTTD_VEHICLE_CALL *engine_id)(OpenTTDVehicle *) noexcept;
	OpenTTDEngineForLength (OPENTTD_VEHICLE_CALL *engine_for_length)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *write_tick)(OpenTTDVehicle *, uint8_t) noexcept;
	uint8_t (OPENTTD_VEHICLE_CALL *direction)(OpenTTDVehicle *) noexcept;
	uint8_t (OPENTTD_VEHICLE_CALL *status)(OpenTTDVehicle *) noexcept;
	uint32_t (OPENTTD_VEHICLE_CALL *tile)(OpenTTDVehicle *) noexcept;
	uint8_t (OPENTTD_VEHICLE_CALL *progress)(OpenTTDVehicle *) noexcept;
	uint32_t (OPENTTD_VEHICLE_CALL *dest_tile)(OpenTTDVehicle *) noexcept;
	uint8_t (OPENTTD_VEHICLE_CALL *order_type)(OpenTTDVehicle *) noexcept;
	uint16_t (OPENTTD_VEHICLE_CALL *order_destination)(OpenTTDVehicle *) noexcept;
	uint8_t (OPENTTD_VEHICLE_CALL *order_nonstop)(OpenTTDVehicle *) noexcept;
};
struct OpenTTDGroundMoved {
	int32_t old_z;
	int32_t z;
	uint16_t cur_speed;
	uint16_t max_track_speed;
};
struct OpenTTDGroundSpeed {
	int32_t distance;
	uint16_t cur_speed;
};
struct OpenTTDGroundViewportNext {
	OpenTTDVehicle *next;
	uint8_t final_progress;
};
struct OpenTTDGroundLastSpeed {
	uint8_t status;
};
struct OpenTTDGroundCrash {
	uint32_t victims;
	bool front;
};
struct OpenTTDGroundServices {
	void (OPENTTD_VEHICLE_CALL *cache_begin)(OpenTTDVehicle *) noexcept;
	OpenTTDGroundMoved (OPENTTD_VEHICLE_CALL *move_incline)(OpenTTDVehicle *, int32_t, int32_t, bool, bool) noexcept;
	OpenTTDGroundViewportNext (OPENTTD_VEHICLE_CALL *update_viewport_then_next)(OpenTTDVehicle *, OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *turn)(OpenTTDVehicle *, uint8_t) noexcept;
	OpenTTDGroundSpeed (OPENTTD_VEHICLE_CALL *do_update_speed)(OpenTTDVehicle *, uint32_t, int32_t, int32_t) noexcept;
	OpenTTDGroundLastSpeed (OPENTTD_VEHICLE_CALL *set_last_speed)(OpenTTDVehicle *) noexcept;
	int32_t (OPENTTD_VEHICLE_CALL *acceleration)(OpenTTDVehicle *) noexcept;
	OpenTTDGroundCrash (OPENTTD_VEHICLE_CALL *crash)(OpenTTDVehicle *, bool) noexcept;
	void (OPENTTD_VEHICLE_CALL *first_cargo_changed)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *go_to_depot_service)(OpenTTDVehicle *, uint16_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *write_first_engine)(OpenTTDVehicle *, uint16_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *write_length)(OpenTTDVehicle *, uint8_t, OpenTTDVehicle *, uint16_t) noexcept;
	uint8_t (OPENTTD_VEHICLE_CALL *cached_length)(OpenTTDVehicle *) noexcept;
	bool (OPENTTD_VEHICLE_CALL *has_articulated)(OpenTTDVehicle *) noexcept;
};
const OpenTTDVehicleServices &GetRustVehicleServices() noexcept;
enum VehicleType : uint8_t;
/* ABI 740-750 pin only these consumed scheduler layouts.
 * A tagged owner is opaque: only the current type's pointer is read. */
struct OpenTTDRoadState;
struct OpenTTDTrainState;
struct OpenTTDShipState;
struct OpenTTDAircraftState;
struct OpenTTDCargoList;
union OpenTTDVehicleTickOwner {
	OpenTTDRoadState *road;
	OpenTTDTrainState *train;
	OpenTTDShipState *ship;
	OpenTTDAircraftState *aircraft;
};
struct OpenTTDVehicleDispatch {
	OpenTTDVehicle *vehicle;
	OpenTTDVehicleTickOwner owner;
	uint32_t id;
	VehicleType type;
	uint8_t subtype;
	uint8_t road_tick;
};
/* Day lookup checks the live bound; a hole has a null vehicle but end=false. */
struct OpenTTDVehicleDayLookup {
	OpenTTDVehicle *vehicle;
	bool end;
};
struct OpenTTDVehicleDayCallback {
	uint8_t day;
	bool has_engine;
};
/* First() is resolved once, after Tick survives; sound reads follow cargo aging. */
struct OpenTTDVehicleCargoTick {
	OpenTTDVehicle *front;
	OpenTTDCargoList *cargo;
	uint16_t period;
	uint16_t counter;
};
struct OpenTTDVehicleMotionTick {
	uint32_t motion;
	uint16_t speed;
	uint8_t front_status;
	uint8_t status;
	VehicleType front_type;
};
struct OpenTTDVehicleSoundTick {
	uint32_t motion;
	uint16_t speed;
	uint8_t subtype;
};
struct OpenTTDVehiclePeriodicSound {
	uint16_t speed;
	uint8_t tick;
	uint8_t front_status;
};
struct OpenTTDVehicleStationLookup {
	void *station;
	uint32_t id;
};
struct OpenTTDVehicleSchedulerServices {
	OpenTTDVehicleDispatch (OPENTTD_VEHICLE_CALL *next)(uint32_t) noexcept;
	OpenTTDVehicleDayLookup (OPENTTD_VEHICLE_CALL *day_at)(uint32_t) noexcept;
	OpenTTDVehicleDispatch (OPENTTD_VEHICLE_CALL *dispatch)(OpenTTDVehicle *) noexcept;
	OpenTTDVehicleDayCallback (OPENTTD_VEHICLE_CALL *day_callback_read)(OpenTTDVehicle *) noexcept;
	uint16_t (OPENTTD_VEHICLE_CALL *day_callback)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *randomise_callback)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *first_dirty)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *unknown_day_callback)(OpenTTDVehicle *, uint16_t) noexcept;
	bool (OPENTTD_VEHICLE_CALL *other_tick)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *assert_survival)(OpenTTDVehicle *, uint32_t, bool) noexcept;
	OpenTTDVehicleCargoTick (OPENTTD_VEHICLE_CALL *cargo_read)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *write_cargo_counter)(OpenTTDVehicle *, uint16_t) noexcept;
	uint16_t (OPENTTD_VEHICLE_CALL *cargo_period)(OpenTTDVehicle *) noexcept;
	OpenTTDVehicleMotionTick (OPENTTD_VEHICLE_CALL *motion_read)(OpenTTDVehicle *, OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *write_motion)(OpenTTDVehicle *, uint32_t) noexcept;
	OpenTTDVehicleSoundTick (OPENTTD_VEHICLE_CALL *sound_read)(OpenTTDVehicle *, OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *running_sound)(OpenTTDVehicle *) noexcept;
	OpenTTDVehiclePeriodicSound (OPENTTD_VEHICLE_CALL *periodic_read)(OpenTTDVehicle *, OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *periodic_sound)(OpenTTDVehicle *, bool) noexcept;
	OpenTTDVehicleStationLookup (OPENTTD_VEHICLE_CALL *next_station)(uint32_t) noexcept;
	uint64_t (OPENTTD_VEHICLE_CALL *economy_begin)() noexcept;
	void (OPENTTD_VEHICLE_CALL *economy_end)(uint64_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *reset_accumulators)() noexcept;
	uint64_t (OPENTTD_VEHICLE_CALL *train_begin)() noexcept;
	uint64_t (OPENTTD_VEHICLE_CALL *road_begin)() noexcept;
	uint64_t (OPENTTD_VEHICLE_CALL *ship_begin)() noexcept;
	uint64_t (OPENTTD_VEHICLE_CALL *aircraft_begin)() noexcept;
	void (OPENTTD_VEHICLE_CALL *train_end)(uint64_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *road_end)(uint64_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *ship_end)(uint64_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *aircraft_end)(uint64_t) noexcept;
};
struct OpenTTDRoadLeaves;
struct OpenTTDTrainServices;
struct OpenTTDShipLeaves;
struct OpenTTDAircraftLeaves;
struct OpenTTDSharedServices;
struct OpenTTDCargoStorageServices;
struct OpenTTDStationServices;
struct OpenTTDStationLoading;
struct OpenTTDFleetPendingServices;
struct OpenTTDFleetTransactionServices;
struct OpenTTDFleetGroupServices;
struct OpenTTDVehicleSchedulerTables {
	const OpenTTDVehicleSchedulerServices *scheduler;
	const OpenTTDRoadLeaves *road;
	const OpenTTDTrainServices *train;
	const OpenTTDShipLeaves *ship;
	const OpenTTDAircraftLeaves *aircraft;
	const OpenTTDSharedServices *shared;
	const OpenTTDCargoStorageServices *cargo;
	const OpenTTDStationServices *station;
	const OpenTTDStationLoading *loading;
	const OpenTTDFleetPendingServices *pending;
	const OpenTTDFleetTransactionServices *transaction;
	const OpenTTDFleetGroupServices *group;
};
const OpenTTDRoadLeaves &GetRoadLeaves() noexcept;
const OpenTTDTrainServices &GetTrainServices() noexcept;
const OpenTTDShipLeaves &GetShipServices() noexcept;
const OpenTTDAircraftLeaves &GetAircraftLeaves() noexcept;
const OpenTTDVehicleSchedulerServices &GetRustVehicleSchedulerServices() noexcept;
extern "C" {
void OPENTTD_VEHICLE_CALL openttd_rust_vehicle_calendar_day(const OpenTTDVehicleSchedulerTables *, bool, uint32_t);
void OPENTTD_VEHICLE_CALL openttd_rust_vehicle_ticks(const OpenTTDVehicleSchedulerTables *, bool, uint32_t, void *);
}
#endif /* RUST_VEHICLE_FFI_H */
