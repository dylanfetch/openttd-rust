/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file road_ffi.h Canonical road vehicle controller and private state. */
#ifndef RUST_ROAD_FFI_H
#define RUST_ROAD_FFI_H
#include <cstdint>
#include <cstddef>
#include "services_ffi.h"
#if defined(_MSC_VER)
#define OPENTTD_ROAD_CALL __cdecl
#elif defined(__i386__)
#define OPENTTD_ROAD_CALL __attribute__((cdecl))
#else
#define OPENTTD_ROAD_CALL
#endif
/** Synchronous immutable callback table; no reference into shared world storage.
 * Each read copies only fields consumed together at the source observation point.
 * All callbacks are noexcept, including nested road entries and shell destruction.
 * No owner/path borrow spans a callback; panic and environmental failures abort. */
struct OpenTTDRoadState;
struct OpenTTDRoadPathElement { uint8_t trackdir; uint32_t tile; };
struct OpenTTDRoadSpeedLimits { uint32_t max_track_speed; uint32_t order_max_speed; };
struct OpenTTDRoadConsistSpeed { uint32_t direction; uint32_t next; uint32_t status; uint32_t tile; };
struct OpenTTDRoadCloseOrigin { uint32_t first; uint32_t z; };
struct OpenTTDRoadCloseCandidate { uint32_t direction; uint32_t first; uint32_t x; uint32_t y; uint32_t z; };
struct OpenTTDRoadOvertakeOrigin { uint32_t articulated; uint32_t direction; uint32_t tile; uint32_t tram; };
struct OpenTTDRoadOvertakeSpeed { uint32_t direction; uint32_t speed; uint32_t status; uint32_t tile; };
struct OpenTTDRoadSlidingPosition { uint32_t direction; uint32_t x; uint32_t y; };
struct OpenTTDRoadHeightSpeed { uint32_t max_track_speed; uint32_t speed; uint32_t z; };
struct OpenTTDRoadCollisionPart { uint32_t next; uint32_t tile; uint32_t z; };
struct OpenTTDRoadCollisionOrigin { uint32_t x; uint32_t y; };
struct OpenTTDRoadCrashDirection { uint32_t direction; };
struct OpenTTDRoadPathVehicle { uint32_t articulated; uint32_t owner; uint32_t tile; uint32_t tram; };
struct OpenTTDRoadDepotPart { uint32_t next; uint32_t tile; };
struct OpenTTDRoadDepotOrders { uint32_t dest; uint32_t order_type; };
struct OpenTTDRoadVehicleTile { uint32_t tile; };
struct OpenTTDRoadArrivalVehicle { uint32_t owner; uint32_t tram; };
struct OpenTTDRoadTunnelVehicle { uint32_t direction; uint32_t front; };
struct OpenTTDRoadMoveVehicle { uint32_t front; uint32_t tile; uint32_t tram; };
struct OpenTTDRoadMoveTransition { uint32_t length; uint32_t next; uint32_t tile; };
struct OpenTTDRoadMovePosition { uint32_t order_type; uint32_t owner; uint32_t speed; uint32_t tile; };
struct OpenTTDRoadBlockVehicle { uint32_t direction; uint32_t front; uint32_t owner; uint32_t tile; };
struct OpenTTDRoadStopOrder { uint32_t order_destination; uint32_t order_type; uint32_t tile; };
struct OpenTTDRoadMoveStop { uint32_t order_type; uint32_t tile; };
struct OpenTTDRoadOrderClock { uint32_t order_time; };
struct OpenTTDRoadControllerPart { uint32_t next; uint32_t status; };
struct OpenTTDRoadServiceOrigin { uint32_t first; uint32_t speed; uint32_t tile; };
struct OpenTTDRoadServiceOrder { uint32_t order_nonstop; uint32_t order_type; };
struct OpenTTDRoadTrackDirection { uint32_t direction; uint32_t status; uint32_t tile; };
struct OpenTTDRoadSlopeOrigin { uint32_t direction; uint32_t first; };
struct OpenTTDRoadSlopePart { uint32_t direction; uint32_t next; };
struct OpenTTDRoadTurnVehicle { uint32_t breakdown; uint32_t direction; uint32_t order_type; uint32_t status; uint32_t tile; };
struct OpenTTDRoadPosition { int32_t x; int32_t y; };
struct OpenTTDRoadTrackChoice { uint8_t trackdir; bool found; };
struct OpenTTDRoadDepotResult { uint32_t tile; uint32_t length; };
using OpenTTDRoadVisitor = bool (OPENTTD_ROAD_CALL *)(void *, uint32_t);
struct OpenTTDRoadLeaves {
	uint32_t (OPENTTD_ROAD_CALL *read_z)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_type)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_acc_model)() noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_road_side)() noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_tile_type)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_has_road)(uint32_t, uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_track_status)(uint32_t, uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_tile_owner)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_depot_dir)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_bay_dir)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_is_depot)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_normal_road)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_road_works)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_disallowed)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_bay_stop)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_is_dt_stop)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_stop_type)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_free_bay)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_any_road_bits)(uint32_t, uint32_t, bool) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_road_bits)(uint32_t, uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_offset)(uint8_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_tile_x)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_tile_y)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_station)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_continuation)(uint32_t, uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_bridge_speed)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_max_penalty)() noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_servint)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_needs_service)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_wait_unbunch)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_order_stop)(uint32_t, uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_road_type)(uint32_t, uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_queue)() noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_tunnel_dir)(uint32_t) noexcept;
	int32_t (OPENTTD_ROAD_CALL *op_acceleration)(uint32_t) noexcept;
	int32_t (OPENTTD_ROAD_CALL *op_update_speed)(uint32_t, uint32_t, int32_t, int32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_advance)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_position)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_base_viewport)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_last_speed)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_roadstop_leave)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_entrance_set)(uint32_t, bool) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_entrance_busy)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_order_free)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_set_next)(uint32_t, uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_start_stop_dirty)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_depot_dirty)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_details_dirty)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_service)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_leave_unbunch)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_reset_unbunch)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_path_result)(uint32_t, bool) noexcept;
	void (OPENTTD_ROAD_CALL *op_order_dummy)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_order_depot)(uint32_t, uint16_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_depot_index)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_decrease_value)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_age)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_economy_age)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_check_breakdown)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_check_orders)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_pay_running)(uint32_t, int64_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_cost_class)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_cost_factor)(uint32_t) noexcept;
	int64_t (OPENTTD_ROAD_CALL *op_get_price)(uint32_t, uint64_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_grf_version)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_length_default)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_age_default)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_speed_default)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_length_error)(uint32_t, uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_disconnect)() noexcept;
	void (OPENTTD_ROAD_CALL *op_explosion)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_sound_default)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_sound)(uint32_t, uint16_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_sound_old1)() noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_sound_old2)() noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_engine_invalid)() noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_invalid_price)() noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_cost_divisor)() noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_is_crossing)(uint32_t) noexcept;
	OpenTTDRoadPosition (OPENTTD_ROAD_CALL *op_new_position)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_virt_tile)(int32_t, int32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_is_road_stop)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_set_dest)(uint32_t, uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_cache_invalidate)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_arrival)(uint32_t, uint16_t, uint32_t, bool) noexcept;
	void (OPENTTD_ROAD_CALL *op_crash_news)(uint32_t, uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_station_visits)(uint16_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_station_visit_set)(uint16_t, uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_local_company)() noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_enter_tile)(uint32_t, uint32_t, int32_t, int32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_enter_depot)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_process_orders)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_loading)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_begin_loading)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_tram_probe)(uint32_t, uint32_t, uint8_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_property)(uint32_t, uint8_t, uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_length_callback)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_play_sound)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_visual)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_update_visual)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_cargo_changed)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_length_changed)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_breakdown)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_delete)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *op_ground_crash)(uint32_t, bool) noexcept;
	void (OPENTTD_ROAD_CALL *op_stop_random)(uint32_t, uint16_t) noexcept;
	void (OPENTTD_ROAD_CALL *op_stop_animation)(uint32_t, uint16_t) noexcept;
	OpenTTDRoadTrackChoice (OPENTTD_ROAD_CALL *op_yapf)(uint32_t, uint32_t, uint8_t, uint16_t) noexcept;
	OpenTTDRoadDepotResult (OPENTTD_ROAD_CALL *op_find_depot)(uint32_t, int32_t) noexcept;
	int32_t (OPENTTD_ROAD_CALL *op_inclination)(uint32_t, bool, bool) noexcept;
	void (OPENTTD_ROAD_CALL *op_viewport)(uint32_t, bool, bool) noexcept;
	void (OPENTTD_ROAD_CALL *set_tile)(uint32_t, uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_x)(uint32_t, int32_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_y)(uint32_t, int32_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_direction)(uint32_t, uint8_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_speed)(uint32_t, uint16_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_tick)(uint32_t, uint8_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_running)(uint32_t, uint8_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_day)(uint32_t, uint8_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_order_time)(uint32_t, int32_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_progress)(uint32_t, uint8_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_last_station)(uint32_t, uint16_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_hidden)(uint32_t, bool) noexcept;
	void (OPENTTD_ROAD_CALL *set_first_engine)(uint32_t, uint16_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_length)(uint32_t, uint8_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_total_length)(uint32_t, uint16_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_cargo_age)(uint32_t, uint16_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_max_speed)(uint32_t, uint16_t) noexcept;
	void (OPENTTD_ROAD_CALL *set_suppress_implicit)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_day)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_dest)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_direction)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_engine)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_first)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_front)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_last_station)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_length)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_next)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_order_type)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_previous)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_progress)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_running)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_speed)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_status)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_tick)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_tile)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_total_length)(uint32_t) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_tram)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *speed_limits)(uint32_t, OpenTTDRoadSpeedLimits *) noexcept;
	void (OPENTTD_ROAD_CALL *consist_speed)(uint32_t, OpenTTDRoadConsistSpeed *) noexcept;
	void (OPENTTD_ROAD_CALL *close_origin)(uint32_t, OpenTTDRoadCloseOrigin *) noexcept;
	void (OPENTTD_ROAD_CALL *close_candidate)(uint32_t, OpenTTDRoadCloseCandidate *) noexcept;
	void (OPENTTD_ROAD_CALL *overtake_origin)(uint32_t, OpenTTDRoadOvertakeOrigin *) noexcept;
	void (OPENTTD_ROAD_CALL *overtake_speed)(uint32_t, OpenTTDRoadOvertakeSpeed *) noexcept;
	void (OPENTTD_ROAD_CALL *sliding_position)(uint32_t, OpenTTDRoadSlidingPosition *) noexcept;
	void (OPENTTD_ROAD_CALL *height_speed)(uint32_t, OpenTTDRoadHeightSpeed *) noexcept;
	void (OPENTTD_ROAD_CALL *collision_part)(uint32_t, OpenTTDRoadCollisionPart *) noexcept;
	void (OPENTTD_ROAD_CALL *collision_origin)(uint32_t, OpenTTDRoadCollisionOrigin *) noexcept;
	void (OPENTTD_ROAD_CALL *crash_direction)(uint32_t, OpenTTDRoadCrashDirection *) noexcept;
	void (OPENTTD_ROAD_CALL *path_vehicle)(uint32_t, OpenTTDRoadPathVehicle *) noexcept;
	void (OPENTTD_ROAD_CALL *depot_part)(uint32_t, OpenTTDRoadDepotPart *) noexcept;
	void (OPENTTD_ROAD_CALL *depot_orders)(uint32_t, OpenTTDRoadDepotOrders *) noexcept;
	void (OPENTTD_ROAD_CALL *vehicle_tile)(uint32_t, OpenTTDRoadVehicleTile *) noexcept;
	void (OPENTTD_ROAD_CALL *arrival_vehicle)(uint32_t, OpenTTDRoadArrivalVehicle *) noexcept;
	void (OPENTTD_ROAD_CALL *tunnel_vehicle)(uint32_t, OpenTTDRoadTunnelVehicle *) noexcept;
	void (OPENTTD_ROAD_CALL *move_vehicle)(uint32_t, OpenTTDRoadMoveVehicle *) noexcept;
	void (OPENTTD_ROAD_CALL *move_transition)(uint32_t, OpenTTDRoadMoveTransition *) noexcept;
	void (OPENTTD_ROAD_CALL *move_position)(uint32_t, OpenTTDRoadMovePosition *) noexcept;
	void (OPENTTD_ROAD_CALL *block_vehicle)(uint32_t, OpenTTDRoadBlockVehicle *) noexcept;
	void (OPENTTD_ROAD_CALL *stop_order)(uint32_t, OpenTTDRoadStopOrder *) noexcept;
	void (OPENTTD_ROAD_CALL *move_stop)(uint32_t, OpenTTDRoadMoveStop *) noexcept;
	void (OPENTTD_ROAD_CALL *order_clock)(uint32_t, OpenTTDRoadOrderClock *) noexcept;
	void (OPENTTD_ROAD_CALL *controller_part)(uint32_t, OpenTTDRoadControllerPart *) noexcept;
	void (OPENTTD_ROAD_CALL *service_origin)(uint32_t, OpenTTDRoadServiceOrigin *) noexcept;
	void (OPENTTD_ROAD_CALL *service_order)(uint32_t, OpenTTDRoadServiceOrder *) noexcept;
	void (OPENTTD_ROAD_CALL *track_direction)(uint32_t, OpenTTDRoadTrackDirection *) noexcept;
	void (OPENTTD_ROAD_CALL *slope_origin)(uint32_t, OpenTTDRoadSlopeOrigin *) noexcept;
	void (OPENTTD_ROAD_CALL *slope_part)(uint32_t, OpenTTDRoadSlopePart *) noexcept;
	void (OPENTTD_ROAD_CALL *turn_vehicle)(uint32_t, OpenTTDRoadTurnVehicle *) noexcept;
	OpenTTDRoadState * (OPENTTD_ROAD_CALL *owner)(uint32_t) noexcept;
	void (OPENTTD_ROAD_CALL *visit_close)(uint32_t, int32_t, int32_t, OpenTTDRoadVisitor, void *) noexcept;
	void (OPENTTD_ROAD_CALL *visit_tunnel)(uint32_t, int32_t, int32_t, OpenTTDRoadVisitor, void *) noexcept;
	void (OPENTTD_ROAD_CALL *visit_tile)(uint32_t, int32_t, int32_t, OpenTTDRoadVisitor, void *) noexcept;
	void (OPENTTD_ROAD_CALL *visit_train)(uint32_t, int32_t, int32_t, OpenTTDRoadVisitor, void *) noexcept;
	uint32_t (OPENTTD_ROAD_CALL *read_bus)(uint32_t) noexcept;

};
extern "C" {
bool OPENTTD_ROAD_CALL openttd_rust_road_tick(uint32_t, OpenTTDRoadState *, const OpenTTDRoadLeaves *, const OpenTTDSharedServices *);
bool OPENTTD_ROAD_CALL openttd_rust_road_individual(uint32_t, OpenTTDRoadState *, uint32_t, const OpenTTDRoadLeaves *, const OpenTTDSharedServices *);
bool OPENTTD_ROAD_CALL openttd_rust_road_leave_depot(uint32_t, OpenTTDRoadState *, bool, const OpenTTDRoadLeaves *, const OpenTTDSharedServices *);
uint32_t OPENTTD_ROAD_CALL openttd_rust_road_crash(uint32_t, OpenTTDRoadState *, bool, const OpenTTDRoadLeaves *, const OpenTTDSharedServices *);
void OPENTTD_ROAD_CALL openttd_rust_road_update_cache(uint32_t, OpenTTDRoadState *, bool, const OpenTTDRoadLeaves *, const OpenTTDSharedServices *);
void OPENTTD_ROAD_CALL openttd_rust_road_calendar_day(uint32_t, OpenTTDRoadState *, const OpenTTDRoadLeaves *, const OpenTTDSharedServices *);
void OPENTTD_ROAD_CALL openttd_rust_road_economy_day(uint32_t, OpenTTDRoadState *, const OpenTTDRoadLeaves *, const OpenTTDSharedServices *);
int64_t OPENTTD_ROAD_CALL openttd_rust_road_running_cost(uint32_t, OpenTTDRoadState *, const OpenTTDRoadLeaves *, const OpenTTDSharedServices *);
int32_t OPENTTD_ROAD_CALL openttd_rust_road_max_speed(uint32_t, OpenTTDRoadState *, const OpenTTDRoadLeaves *, const OpenTTDSharedServices *);
int32_t OPENTTD_ROAD_CALL openttd_rust_road_update_speed(uint32_t, OpenTTDRoadState *, const OpenTTDRoadLeaves *, const OpenTTDSharedServices *);
void OPENTTD_ROAD_CALL openttd_rust_road_set_dest(uint32_t, OpenTTDRoadState *, uint32_t, const OpenTTDRoadLeaves *, const OpenTTDSharedServices *);
bool OPENTTD_ROAD_CALL openttd_rust_road_turn(uint32_t, OpenTTDRoadState *, bool, const OpenTTDRoadLeaves *, const OpenTTDSharedServices *);
uint8_t OPENTTD_ROAD_CALL openttd_rust_road_trackdir(uint32_t, OpenTTDRoadState *, const OpenTTDRoadLeaves *, const OpenTTDSharedServices *);
bool OPENTTD_ROAD_CALL openttd_rust_road_slope_pixel(uint32_t, OpenTTDRoadState *, const OpenTTDRoadLeaves *, const OpenTTDSharedServices *);
/* Each ordinary or indexed-load shell owns one zero-created state. Destruction
 * follows PreDestructor and occurs exactly once, also during pool cleanup.
 * Scalars 0..6: state:u8, frame:u8, blocked:u16, overtaking:u8,
 * overtaking-counter:u8, crashed:u16, reverse:u8. Set narrows to source widths.
 * No returned references or persistent C++ cache. Save/load stages fields and
 * path outside Rust frames, commits partial loads on unwind and preserves nesting.
 * Path access requires the same index/nonempty preconditions as std::vector.
 * Replacing copies in forward order; count zero permits a null input.
 * Game-thread access only; panic/OOM abort and no exception crosses this ABI. */
OpenTTDRoadState *OPENTTD_ROAD_CALL openttd_rust_road_new();
void OPENTTD_ROAD_CALL openttd_rust_road_destroy(OpenTTDRoadState *);
uint16_t OPENTTD_ROAD_CALL openttd_rust_road_get(const OpenTTDRoadState *, uint8_t);
void OPENTTD_ROAD_CALL openttd_rust_road_set(OpenTTDRoadState *, uint8_t, uint16_t);
uint8_t OPENTTD_ROAD_CALL openttd_rust_road_stop_frame(uint32_t);
uint16_t OPENTTD_ROAD_CALL openttd_rust_road_drive_entry(uint8_t, uint8_t, uint8_t);
size_t OPENTTD_ROAD_CALL openttd_rust_road_path_size(const OpenTTDRoadState *);
OpenTTDRoadPathElement OPENTTD_ROAD_CALL openttd_rust_road_path_get(const OpenTTDRoadState *, size_t);
void OPENTTD_ROAD_CALL openttd_rust_road_path_replace(OpenTTDRoadState *, const OpenTTDRoadPathElement *, size_t);
void OPENTTD_ROAD_CALL openttd_rust_road_path_clear(OpenTTDRoadState *);
void OPENTTD_ROAD_CALL openttd_rust_road_path_push(OpenTTDRoadState *, OpenTTDRoadPathElement);
void OPENTTD_ROAD_CALL openttd_rust_road_path_pop(OpenTTDRoadState *);
}
#endif /* RUST_ROAD_FFI_H */
