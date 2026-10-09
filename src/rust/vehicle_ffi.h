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
struct OpenTTDVehicleNewPosition {
	int32_t x;
	int32_t y;
	uint32_t tile;
};
struct OpenTTDVehicleBreakdown {
	bool broken;
	uint8_t status;
};
struct OpenTTDVehicleServices {
	void (OPENTTD_VEHICLE_CALL *write_counters)(OpenTTDVehicle *, uint8_t, uint8_t, int32_t) noexcept;
	OpenTTDVehicleBreakdown (OPENTTD_VEHICLE_CALL *handle_breakdown)(OpenTTDVehicle *) noexcept;
	uint8_t (OPENTTD_VEHICLE_CALL *process_orders_then_loading)(OpenTTDVehicle *) noexcept;
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
	void (OPENTTD_VEHICLE_CALL *unknown_length_result)(OpenTTDVehicle *, uint16_t) noexcept;
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
struct OpenTTDGroundLastSpeed {
	uint8_t status;
	uint8_t progress;
};
struct OpenTTDGroundCrash {
	uint32_t victims;
	bool front;
};
struct OpenTTDGroundServices {
	OpenTTDGroundMoved (OPENTTD_VEHICLE_CALL *move_incline)(OpenTTDVehicle *, int32_t, int32_t, bool, bool) noexcept;
	void (OPENTTD_VEHICLE_CALL *update_viewport)(OpenTTDVehicle *, bool, bool) noexcept;
	void (OPENTTD_VEHICLE_CALL *turn)(OpenTTDVehicle *, uint8_t) noexcept;
	OpenTTDGroundSpeed (OPENTTD_VEHICLE_CALL *do_update_speed)(OpenTTDVehicle *, uint32_t, int32_t, int32_t) noexcept;
	OpenTTDGroundLastSpeed (OPENTTD_VEHICLE_CALL *set_last_speed)(OpenTTDVehicle *) noexcept;
	int32_t (OPENTTD_VEHICLE_CALL *acceleration)(OpenTTDVehicle *) noexcept;
	OpenTTDGroundCrash (OPENTTD_VEHICLE_CALL *crash)(OpenTTDVehicle *, bool) noexcept;
	void (OPENTTD_VEHICLE_CALL *first_cargo_changed)(OpenTTDVehicle *) noexcept;
	void (OPENTTD_VEHICLE_CALL *go_to_depot_service)(OpenTTDVehicle *, uint16_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *write_first_engine)(OpenTTDVehicle *, uint16_t) noexcept;
	void (OPENTTD_VEHICLE_CALL *write_length)(OpenTTDVehicle *, uint8_t, OpenTTDVehicle *, uint16_t) noexcept;
};
struct OpenTTDMapServices {
	uint8_t (OPENTTD_VEHICLE_CALL *tile_type)(uint32_t) noexcept;
	uint8_t (OPENTTD_VEHICLE_CALL *tile_owner)(uint32_t) noexcept;
	uint16_t (OPENTTD_VEHICLE_CALL *station_index)(uint32_t) noexcept;
	uint16_t (OPENTTD_VEHICLE_CALL *depot_index)(uint32_t) noexcept;
	bool (OPENTTD_VEHICLE_CALL *is_level_crossing)(uint32_t) noexcept;
	uint8_t (OPENTTD_VEHICLE_CALL *tunnel_bridge_direction)(uint32_t) noexcept;
	uint16_t (OPENTTD_VEHICLE_CALL *bridge_speed)(uint32_t) noexcept;
};
const OpenTTDVehicleServices &GetRustVehicleServices() noexcept;
const OpenTTDMapServices &GetRustMapServices() noexcept;
#endif /* RUST_VEHICLE_FFI_H */
