/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */
/** @file aircraft_ffi.h Aircraft-private ownership and copied shared services. */
#ifndef OPENTTD_RUST_AIRCRAFT_FFI_H
#define OPENTTD_RUST_AIRCRAFT_FFI_H
#include "services_ffi.h"
#if defined(_MSC_VER)
#define OPENTTD_AIRCRAFT_CALL __cdecl
#elif defined(__i386__)
#define OPENTTD_AIRCRAFT_CALL __attribute__((cdecl))
#else
#define OPENTTD_AIRCRAFT_CALL
#endif
extern "C" {
struct OpenTTDAircraftState {
	uint32_t cached_max_range_sqr;
	uint16_t cached_max_range, cache_padding, crashed_counter, targetairport;
	uint8_t pos, previous_pos, state, last_direction, number_consecutive_turns, turn_counter, flags;
};
struct OpenTTDAircraftVehicle { void *handle; OpenTTDAircraftState *state; uint32_t id; };
struct OpenTTDAircraftNode { const void *next; uint64_t blocks; uint8_t position, next_position, heading; };
struct OpenTTDAircraftMoving { int16_t x, y; uint16_t flags; uint8_t direction; };
struct OpenTTDAircraftPosition { int32_t x, y; uint32_t tile; };
struct OpenTTDAircraftBlockNode { uint64_t blocks; uint8_t position, next_position; };
struct OpenTTDAircraftRouteNode { const void *next; uint8_t next_position, heading; };
struct OpenTTDAircraftBlockChoice { const void *next; uint64_t blocks; uint8_t heading; };
/* Immutable callback table, borrowed for synchronous entries. Handles retain the
 * original pool identity; only raw field-sized State access occurs in Rust.
 * No borrow survives a callback, including reentry and PreDestructor deletion.
 * All native services are noexcept; environmental failures and Rust panics abort.
 * Flight helpers accept actual Vehicle identity, with null state for disasters. */
struct OpenTTDAircraftLeaves {
	uint8_t (OPENTTD_AIRCRAFT_CALL *subtype)(OpenTTDAircraftVehicle);
	int32_t (OPENTTD_AIRCRAFT_CALL *x)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_x)(OpenTTDAircraftVehicle, int32_t);
	int32_t (OPENTTD_AIRCRAFT_CALL *y)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_y)(OpenTTDAircraftVehicle, int32_t);
	int32_t (OPENTTD_AIRCRAFT_CALL *z)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_z)(OpenTTDAircraftVehicle, int32_t);
	uint32_t (OPENTTD_AIRCRAFT_CALL *tile)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_tile)(OpenTTDAircraftVehicle, uint32_t);
	uint8_t (OPENTTD_AIRCRAFT_CALL *direction)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_direction)(OpenTTDAircraftVehicle, uint8_t);
	uint8_t (OPENTTD_AIRCRAFT_CALL *tick_counter)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_tick_counter)(OpenTTDAircraftVehicle, uint8_t);
	uint8_t (OPENTTD_AIRCRAFT_CALL *owner)(OpenTTDAircraftVehicle);
	uint8_t (OPENTTD_AIRCRAFT_CALL *vehicle_status)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_vehicle_status)(OpenTTDAircraftVehicle, uint8_t);
	uint16_t (OPENTTD_AIRCRAFT_CALL *current_speed)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_current_speed)(OpenTTDAircraftVehicle, uint16_t);
	uint8_t (OPENTTD_AIRCRAFT_CALL *subspeed)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_subspeed)(OpenTTDAircraftVehicle, uint8_t);
	uint8_t (OPENTTD_AIRCRAFT_CALL *progress)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_progress)(OpenTTDAircraftVehicle, uint8_t);
	uint8_t (OPENTTD_AIRCRAFT_CALL *acceleration)(OpenTTDAircraftVehicle);
	uint16_t (OPENTTD_AIRCRAFT_CALL *maximum_speed)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_maximum_speed)(OpenTTDAircraftVehicle, uint16_t);
	void (OPENTTD_AIRCRAFT_CALL *set_breakdown_counter)(OpenTTDAircraftVehicle, uint8_t);
	uint8_t (OPENTTD_AIRCRAFT_CALL *order_type)(OpenTTDAircraftVehicle);
	uint16_t (OPENTTD_AIRCRAFT_CALL *order_destination)(OpenTTDAircraftVehicle);
	uint8_t (OPENTTD_AIRCRAFT_CALL *running_ticks)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_running_ticks)(OpenTTDAircraftVehicle, uint8_t);
	int32_t (OPENTTD_AIRCRAFT_CALL *order_time)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_order_time)(OpenTTDAircraftVehicle, int32_t);
	uint8_t (OPENTTD_AIRCRAFT_CALL *day_counter)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_day_counter)(OpenTTDAircraftVehicle, uint8_t);
	int64_t (OPENTTD_AIRCRAFT_CALL *profit)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_profit)(OpenTTDAircraftVehicle, int64_t);
	uint16_t (OPENTTD_AIRCRAFT_CALL *last_station)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *set_last_station)(OpenTTDAircraftVehicle, uint16_t);
	void (OPENTTD_AIRCRAFT_CALL *set_economy_service)(OpenTTDAircraftVehicle, int32_t);
	void (OPENTTD_AIRCRAFT_CALL *set_calendar_service)(OpenTTDAircraftVehicle, int32_t);
	void (OPENTTD_AIRCRAFT_CALL *set_breakdowns)(OpenTTDAircraftVehicle, uint8_t);
	void (OPENTTD_AIRCRAFT_CALL *set_reliability)(OpenTTDAircraftVehicle, uint16_t);
	void (OPENTTD_AIRCRAFT_CALL *set_cargo_age)(OpenTTDAircraftVehicle, uint16_t);
	OpenTTDAircraftVehicle (OPENTTD_AIRCRAFT_CALL *next)(OpenTTDAircraftVehicle);
	uint32_t (OPENTTD_AIRCRAFT_CALL *map_size_x)();
	uint32_t (OPENTTD_AIRCRAFT_CALL *map_max_x)();
	uint32_t (OPENTTD_AIRCRAFT_CALL *map_max_y)();
	uint8_t (OPENTTD_AIRCRAFT_CALL *plane_speed)();
	bool (OPENTTD_AIRCRAFT_CALL *no_jetcrash)();
	uint8_t (OPENTTD_AIRCRAFT_CALL *plane_crashes)();
	bool (OPENTTD_AIRCRAFT_CALL *service_at_helipad)();
	bool (OPENTTD_AIRCRAFT_CALL *disaster_sound)();
	int32_t (OPENTTD_AIRCRAFT_CALL *economy_date)();
	int32_t (OPENTTD_AIRCRAFT_CALL *calendar_date)();
	const void * (OPENTTD_AIRCRAFT_CALL *station)(uint16_t);
	uint32_t (OPENTTD_AIRCRAFT_CALL *airport_tile)(const void *);
	uint32_t (OPENTTD_AIRCRAFT_CALL *station_tile)(const void *);
	uint8_t (OPENTTD_AIRCRAFT_CALL *rotation)(const void *);
	uint16_t (OPENTTD_AIRCRAFT_CALL *airport_width)(const void *);
	uint16_t (OPENTTD_AIRCRAFT_CALL *airport_height)(const void *);
	uint8_t (OPENTTD_AIRCRAFT_CALL *airport_type)(const void *);
	uint8_t (OPENTTD_AIRCRAFT_CALL *station_owner)(const void *);
	bool (OPENTTD_AIRCRAFT_CALL *has_hangar)(const void *);
	bool (OPENTTD_AIRCRAFT_CALL *has_airport)(const void *);
	const void * (OPENTTD_AIRCRAFT_CALL *airport_fta)(const void *);
	uint64_t * (OPENTTD_AIRCRAFT_CALL *airport_blocks)(const void *);
	uint8_t (OPENTTD_AIRCRAFT_CALL *had_vehicle)(const void *);
	const void * (OPENTTD_AIRCRAFT_CALL *dummy_airport)();
	uint8_t (OPENTTD_AIRCRAFT_CALL *airport_elements)(const void *);
	uint8_t (OPENTTD_AIRCRAFT_CALL *helipads)(const void *);
	uint8_t (OPENTTD_AIRCRAFT_CALL *airport_flags)(const void *);
	uint8_t (OPENTTD_AIRCRAFT_CALL *airport_delta_z)(const void *);
	const void * (OPENTTD_AIRCRAFT_CALL *node)(const void *, uint8_t);
	OpenTTDAircraftNode (OPENTTD_AIRCRAFT_CALL *fta)(const void *);
	OpenTTDAircraftMoving (OPENTTD_AIRCRAFT_CALL *moving)(const void *, uint8_t);
	uint16_t (OPENTTD_AIRCRAFT_CALL *engine_speed)(OpenTTDAircraftVehicle);
	uint8_t (OPENTTD_AIRCRAFT_CALL *engine_subtype)(OpenTTDAircraftVehicle);
	uint16_t (OPENTTD_AIRCRAFT_CALL *engine_sound)(OpenTTDAircraftVehicle);
	uint16_t (OPENTTD_AIRCRAFT_CALL *engine_reliability)(OpenTTDAircraftVehicle);
	uint8_t (OPENTTD_AIRCRAFT_CALL *vehicle_type)(OpenTTDAircraftVehicle);
	int32_t (OPENTTD_AIRCRAFT_CALL *slope)(int32_t, int32_t);
	int32_t (OPENTTD_AIRCRAFT_CALL *tile_height)(uint32_t);
	uint8_t (OPENTTD_AIRCRAFT_CALL *airport_entry)(const void *, uint8_t);
	uint8_t (OPENTTD_AIRCRAFT_CALL *direction_towards)(OpenTTDAircraftVehicle, int32_t, int32_t);
	OpenTTDAircraftPosition (OPENTTD_AIRCRAFT_CALL *new_position)(OpenTTDAircraftVehicle);
	int32_t (OPENTTD_AIRCRAFT_CALL *hangar_height)(uint16_t);
	uint8_t (OPENTTD_AIRCRAFT_CALL *terminal_count)(const void *, uint32_t);
	uint8_t (OPENTTD_AIRCRAFT_CALL *hangar_exit)(OpenTTDAircraftVehicle);
	bool (OPENTTD_AIRCRAFT_CALL *can_use_station)(OpenTTDAircraftVehicle, uint16_t);
	uint16_t (OPENTTD_AIRCRAFT_CALL *service_interval)(OpenTTDAircraftVehicle);
	bool (OPENTTD_AIRCRAFT_CALL *needs_service)(OpenTTDAircraftVehicle);
	bool (OPENTTD_AIRCRAFT_CALL *chain_in_depot)(OpenTTDAircraftVehicle);
	bool (OPENTTD_AIRCRAFT_CALL *waiting_unbunching)(OpenTTDAircraftVehicle);
	bool (OPENTTD_AIRCRAFT_CALL *nearest_depot_order)(OpenTTDAircraftVehicle);
	bool (OPENTTD_AIRCRAFT_CALL *part_of_orders)(OpenTTDAircraftVehicle);
	uint16_t (OPENTTD_AIRCRAFT_CALL *next_station)(uint16_t);
	OpenTTDAircraftVehicle (OPENTTD_AIRCRAFT_CALL *next_aircraft)(uint32_t);
	uint32_t (OPENTTD_AIRCRAFT_CALL *random)();
	void (OPENTTD_AIRCRAFT_CALL *update_position)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *rotor_image)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *copy_sprite)(OpenTTDAircraftVehicle, OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *position_viewport)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *create_effect)(OpenTTDAircraftVehicle, uint8_t, int32_t, int32_t, int32_t);
	void (OPENTTD_AIRCRAFT_CALL *dirty_start_stop)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *play_sound)(OpenTTDAircraftVehicle, uint16_t);
	void (OPENTTD_AIRCRAFT_CALL *truncate_cargo)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *crash_news)(OpenTTDAircraftVehicle, uint16_t, uint32_t);
	void (OPENTTD_AIRCRAFT_CALL *station_rating)(uint32_t, uint8_t, int32_t, uint32_t);
	void (OPENTTD_AIRCRAFT_CALL *landing_rating)(uint16_t, uint8_t);
	void (OPENTTD_AIRCRAFT_CALL *free_order)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *service_in_depot)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *leave_unbunching)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *dirty_depot)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *first_arrival)(OpenTTDAircraftVehicle, uint16_t);
	void (OPENTTD_AIRCRAFT_CALL *begin_loading)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *dirty_details)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *update_delta)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *touchdown_animation)(OpenTTDAircraftVehicle, uint16_t);
	void (OPENTTD_AIRCRAFT_CALL *destination_too_far)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *delete_range_news)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *handle_breakdown)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *handle_loading)(OpenTTDAircraftVehicle, bool);
	void (OPENTTD_AIRCRAFT_CALL *service_order)(OpenTTDAircraftVehicle, uint16_t);
	void (OPENTTD_AIRCRAFT_CALL *dummy_order)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *age_vehicle)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *economy_age)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *decrease_value)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *check_orders)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *check_breakdown)(OpenTTDAircraftVehicle);
	int64_t (OPENTTD_AIRCRAFT_CALL *running_cost)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *subtract_cost)(OpenTTDAircraftVehicle, int64_t);
	void (OPENTTD_AIRCRAFT_CALL *dirty_lists)(OpenTTDAircraftVehicle);
	uint16_t (OPENTTD_AIRCRAFT_CALL *next_stopping_station)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *remove_depot_orders)(uint16_t);
	void (OPENTTD_AIRCRAFT_CALL *assert_flying)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *invalid_movement)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *invalid_position)(OpenTTDAircraftVehicle, const void *);
	void (OPENTTD_AIRCRAFT_CALL *invalid_scheme)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *unreachable)(OpenTTDAircraftVehicle);
	uint32_t (OPENTTD_AIRCRAFT_CALL *speed_property)(OpenTTDAircraftVehicle);
	uint32_t (OPENTTD_AIRCRAFT_CALL *cargo_age_property)(OpenTTDAircraftVehicle);
	uint32_t (OPENTTD_AIRCRAFT_CALL *range_property)(OpenTTDAircraftVehicle);
	bool (OPENTTD_AIRCRAFT_CALL *start_sound)(OpenTTDAircraftVehicle);
	bool (OPENTTD_AIRCRAFT_CALL *touchdown_sound)(OpenTTDAircraftVehicle);
	bool (OPENTTD_AIRCRAFT_CALL *rotor_image_if_changed)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *update_rotor_image)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *process_orders)(OpenTTDAircraftVehicle);
	void (OPENTTD_AIRCRAFT_CALL *enter_depot)(OpenTTDAircraftVehicle);
	uint32_t (OPENTTD_AIRCRAFT_CALL *vehicle_crash)(OpenTTDAircraftVehicle, bool);
	void (OPENTTD_AIRCRAFT_CALL *delete_aircraft)(OpenTTDAircraftVehicle);
	bool (OPENTTD_AIRCRAFT_CALL *send_to_depot)(OpenTTDAircraftVehicle, bool);
	uint32_t (OPENTTD_AIRCRAFT_CALL *sample_count)();
	uint32_t (OPENTTD_AIRCRAFT_CALL *helicopter_sound)();
	uint32_t (OPENTTD_AIRCRAFT_CALL *explosion_sound)();
	uint32_t (OPENTTD_AIRCRAFT_CALL *skid_sound)();
	uint32_t (OPENTTD_AIRCRAFT_CALL *ticks_per_year)();
	uint64_t (OPENTTD_AIRCRAFT_CALL *fta_blocks)(const void *);
	uint8_t (OPENTTD_AIRCRAFT_CALL *fta_heading)(const void *);
	uint8_t (OPENTTD_AIRCRAFT_CALL *fta_next_position)(const void *);
	const void * (OPENTTD_AIRCRAFT_CALL *fta_next)(const void *);
	OpenTTDAircraftBlockNode (OPENTTD_AIRCRAFT_CALL *block_node)(const void *);
	OpenTTDAircraftRouteNode (OPENTTD_AIRCRAFT_CALL *route_node)(const void *);
	OpenTTDAircraftBlockChoice (OPENTTD_AIRCRAFT_CALL *block_choice)(const void *);
};
bool OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_tick(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle);
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_calendar_day(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle);
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_economy_day(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle);
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_position(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle, int32_t, int32_t, int32_t);
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_enter_hangar(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle);
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_cache(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle, bool);
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_flight_bounds(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle, int32_t *, int32_t *);
int32_t OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_flight_level(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle, uint8_t *, bool);
int32_t OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_height(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle);
int32_t OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_hold_altitude(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle);
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_missing_orders(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle);
uint32_t OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_crash(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle, bool);
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_next_airport(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle);
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_leave_hangar(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle, uint8_t);
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_order_location(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle);
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_replacement(const OpenTTDAircraftLeaves *, uint16_t);
uint16_t OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_nearest_hangar(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle);
uint16_t OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_closest_depot(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle);
bool OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_event(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle, bool);
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_release_blocks(const OpenTTDAircraftLeaves *, OpenTTDAircraftVehicle);
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_invalidate_target(const OpenTTDAircraftLeaves *, uint16_t);
OpenTTDAircraftState *OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_state_new();
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_state_destroy(OpenTTDAircraftState *);
uint64_t *OPENTTD_AIRCRAFT_CALL openttd_rust_airport_blocks_new();
void OPENTTD_AIRCRAFT_CALL openttd_rust_airport_blocks_destroy(uint64_t *);
}
#endif /* OPENTTD_RUST_AIRCRAFT_FFI_H */
