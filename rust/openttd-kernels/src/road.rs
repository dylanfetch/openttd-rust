/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Road vehicles own their movement, consist traversal and private counters/cache.
//! World observations are copies; no owner reference survives a service call.
#![allow(
    unsafe_code,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::too_many_lines,
    clippy::too_many_arguments,
    clippy::similar_names,
    clippy::many_single_char_names,
    clippy::verbose_bit_mask,
    clippy::items_after_statements,
    clippy::if_same_then_else,
    clippy::needless_late_init,
    clippy::manual_is_power_of_two,
    clippy::if_not_else
)]
use crate::services::Services;
use std::ffi::c_void;

const INVALID: u32 = u32::MAX;
const DEPOT: u16 = 254;
const WORMHOLE: u16 = 255;
const STOP: u16 = 32;
const DT_STOP: u16 = 64;
const REVERSE: [u8; 4] = [6, 7, 14, 15];
const DIAG_TRACK: [u8; 4] = [0, 1, 8, 9];

/// Exact scalar path entry; padding is never read.
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct PathElement {
    pub trackdir: u8,
    pub tile: u32,
}
/// One allocation per ordinary/indexed-load C++ shell.
#[derive(Default)]
pub struct State {
    scalars: [u16; 7],
    pub(crate) path: Vec<PathElement>,
}

#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_road_new() -> *mut State {
    Box::into_raw(Box::default())
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_destroy(state: *mut State) {
    // SAFETY: One shell transfers its live owner exactly once after PreDestructor.
    unsafe {
        drop(Box::from_raw(state));
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_get(state: *const State, field: u8) -> u16 {
    // SAFETY: Game-thread caller supplies a live owner and selector 0..6.
    unsafe { (*state).scalars[field as usize] }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_set(state: *mut State, field: u8, value: u16) {
    // SAFETY: Exclusive game-thread access, with no retained owner reference.
    unsafe {
        (*state).scalars[field as usize] = if field == 2 || field == 5 {
            value
        } else {
            u16::from(value as u8)
        };
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_path_size(state: *const State) -> usize {
    // SAFETY: Live owner, no mutation during this accessor.
    unsafe { (*state).path.len() }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_path_get(
    state: *const State,
    index: usize,
) -> PathElement {
    // SAFETY: Original vector index precondition, live owner.
    unsafe { (&(*state).path)[index] }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_path_replace(
    state: *mut State,
    input: *const PathElement,
    count: usize,
) {
    // SAFETY: Disjoint call-scoped staging array, no owner borrow during C++ calls.
    unsafe {
        (*state).path.clear();
        for index in 0..count {
            (*state).path.push(input.add(index).read());
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_path_clear(state: *mut State) {
    // SAFETY: Exclusive game-thread owner access.
    unsafe {
        (*state).path.clear();
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_path_push(state: *mut State, item: PathElement) {
    // SAFETY: Exclusive game-thread owner access.
    unsafe {
        (*state).path.push(item);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_path_pop(state: *mut State) {
    // SAFETY: Original nonempty vector precondition, exclusive owner access.
    unsafe {
        (*state).path.pop();
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SpeedLimits {
    pub max_track_speed: u32,
    pub order_max_speed: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct ConsistSpeed {
    pub direction: u32,
    pub next: u32,
    pub status: u32,
    pub tile: u32,
}
impl ConsistSpeed {
    fn hidden(self) -> bool {
        self.status & 1 != 0
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct CloseOrigin {
    pub first: u32,
    pub z: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct CloseCandidate {
    pub direction: u32,
    pub first: u32,
    pub x: u32,
    pub y: u32,
    pub z: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct OvertakeOrigin {
    pub articulated: u32,
    pub direction: u32,
    pub tile: u32,
    pub tram: u32,
}
impl OvertakeOrigin {
    fn tram(self) -> bool {
        self.tram != 0
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct OvertakeSpeed {
    pub direction: u32,
    pub speed: u32,
    pub status: u32,
    pub tile: u32,
}
impl OvertakeSpeed {
    fn stopped(self) -> bool {
        self.status & 2 != 0
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SlidingPosition {
    pub direction: u32,
    pub x: u32,
    pub y: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct HeightSpeed {
    pub max_track_speed: u32,
    pub speed: u32,
    pub z: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct CollisionPart {
    pub next: u32,
    pub tile: u32,
    pub z: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct CollisionOrigin {
    pub x: u32,
    pub y: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct CrashDirection {
    pub direction: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct PathVehicle {
    pub articulated: u32,
    pub owner: u32,
    pub tile: u32,
    pub tram: u32,
}
impl PathVehicle {
    fn tram(self) -> bool {
        self.tram != 0
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct DepotPart {
    pub next: u32,
    pub tile: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct DepotOrders {
    pub dest: u32,
    pub order_type: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct VehicleTile {
    pub tile: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct ArrivalVehicle {
    pub owner: u32,
    pub tram: u32,
}
impl ArrivalVehicle {
    fn tram(self) -> bool {
        self.tram != 0
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct TunnelVehicle {
    pub direction: u32,
    pub front: u32,
}
impl TunnelVehicle {
    fn front(self) -> bool {
        self.front != 0
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct MoveVehicle {
    pub front: u32,
    pub tile: u32,
    pub tram: u32,
}
impl MoveVehicle {
    fn front(self) -> bool {
        self.front != 0
    }
    fn tram(self) -> bool {
        self.tram != 0
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct MoveTransition {
    pub length: u32,
    pub next: u32,
    pub tile: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct MovePosition {
    pub order_type: u32,
    pub owner: u32,
    pub speed: u32,
    pub tile: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct BlockVehicle {
    pub direction: u32,
    pub front: u32,
    pub owner: u32,
    pub tile: u32,
}
impl BlockVehicle {
    fn front(self) -> bool {
        self.front != 0
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct StopOrder {
    pub order_destination: u32,
    pub order_type: u32,
    pub tile: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct MoveStop {
    pub order_type: u32,
    pub tile: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct OrderClock {
    pub order_time: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct ControllerPart {
    pub next: u32,
    pub status: u32,
}
impl ControllerPart {
    fn hidden(self) -> bool {
        self.status & 1 != 0
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct ServiceOrigin {
    pub first: u32,
    pub speed: u32,
    pub tile: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct ServiceOrder {
    pub order_nonstop: u32,
    pub order_type: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct TrackDirection {
    pub direction: u32,
    pub status: u32,
    pub tile: u32,
}
impl TrackDirection {
    fn crashed(self) -> bool {
        self.status & 128 != 0
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SlopeOrigin {
    pub direction: u32,
    pub first: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SlopePart {
    pub direction: u32,
    pub next: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct TurnVehicle {
    pub breakdown: u32,
    pub direction: u32,
    pub order_type: u32,
    pub status: u32,
    pub tile: u32,
}
impl TurnVehicle {
    fn crashed(self) -> bool {
        self.status & 128 != 0
    }
    fn stopped(self) -> bool {
        self.status & 2 != 0
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Position {
    pub x: i32,
    pub y: i32,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct TrackChoice {
    pub trackdir: u8,
    pub found: bool,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct DepotResult {
    pub tile: u32,
    pub length: u32,
}
pub type Visitor = extern "C" fn(*mut c_void, u32) -> bool;
#[repr(C)]
pub struct Leaves {
    pub read_z: extern "C" fn(u32) -> u32,
    pub read_type: extern "C" fn(u32) -> u32,
    pub op_acc_model: extern "C" fn() -> u32,
    pub op_road_side: extern "C" fn() -> u32,
    pub op_tile_type: extern "C" fn(u32) -> u32,
    pub op_has_road: extern "C" fn(u32, u32) -> u32,
    pub op_track_status: extern "C" fn(u32, u32) -> u32,
    pub op_tile_owner: extern "C" fn(u32) -> u32,
    pub op_depot_dir: extern "C" fn(u32) -> u32,
    pub op_bay_dir: extern "C" fn(u32) -> u32,
    pub op_is_depot: extern "C" fn(u32) -> u32,
    pub op_normal_road: extern "C" fn(u32) -> u32,
    pub op_road_works: extern "C" fn(u32) -> u32,
    pub op_disallowed: extern "C" fn(u32) -> u32,
    pub op_bay_stop: extern "C" fn(u32) -> u32,
    pub op_is_dt_stop: extern "C" fn(u32) -> u32,
    pub op_stop_type: extern "C" fn(u32) -> u32,
    pub op_free_bay: extern "C" fn(u32) -> u32,
    pub op_any_road_bits: extern "C" fn(u32, u32, bool) -> u32,
    pub op_road_bits: extern "C" fn(u32, u32) -> u32,
    pub op_offset: extern "C" fn(u8) -> u32,
    pub op_tile_x: extern "C" fn(u32) -> u32,
    pub op_tile_y: extern "C" fn(u32) -> u32,
    pub op_station: extern "C" fn(u32) -> u32,
    pub op_continuation: extern "C" fn(u32, u32) -> u32,
    pub op_bridge_speed: extern "C" fn(u32) -> u32,
    pub op_max_penalty: extern "C" fn() -> u32,
    pub op_servint: extern "C" fn(u32) -> u32,
    pub op_needs_service: extern "C" fn(u32) -> u32,
    pub op_wait_unbunch: extern "C" fn(u32) -> u32,
    pub op_order_stop: extern "C" fn(u32, u32) -> u32,
    pub op_road_type: extern "C" fn(u32, u32) -> u32,
    pub op_queue: extern "C" fn() -> u32,
    pub op_tunnel_dir: extern "C" fn(u32) -> u32,
    pub op_acceleration: extern "C" fn(u32) -> i32,
    pub op_update_speed: extern "C" fn(u32, u32, i32, i32) -> i32,
    pub op_advance: extern "C" fn(u32) -> u32,
    pub op_position: extern "C" fn(u32) -> (),
    pub op_base_viewport: extern "C" fn(u32) -> (),
    pub op_last_speed: extern "C" fn(u32) -> (),
    pub op_roadstop_leave: extern "C" fn(u32) -> (),
    pub op_entrance_set: extern "C" fn(u32, bool) -> (),
    pub op_entrance_busy: extern "C" fn(u32) -> u32,
    pub op_order_free: extern "C" fn(u32) -> (),
    pub op_set_next: extern "C" fn(u32, u32) -> (),
    pub op_start_stop_dirty: extern "C" fn(u32) -> (),
    pub op_depot_dirty: extern "C" fn(u32) -> (),
    pub op_details_dirty: extern "C" fn(u32) -> (),
    pub op_service: extern "C" fn(u32) -> (),
    pub op_leave_unbunch: extern "C" fn(u32) -> (),
    pub op_reset_unbunch: extern "C" fn(u32) -> (),
    pub op_path_result: extern "C" fn(u32, bool) -> (),
    pub op_order_dummy: extern "C" fn(u32) -> (),
    pub op_order_depot: extern "C" fn(u32, u16) -> (),
    pub op_depot_index: extern "C" fn(u32) -> u32,
    pub op_decrease_value: extern "C" fn(u32) -> (),
    pub op_age: extern "C" fn(u32) -> (),
    pub op_economy_age: extern "C" fn(u32) -> (),
    pub op_check_breakdown: extern "C" fn(u32) -> (),
    pub op_check_orders: extern "C" fn(u32) -> (),
    pub op_pay_running: extern "C" fn(u32, i64) -> (),
    pub op_cost_class: extern "C" fn(u32) -> u32,
    pub op_cost_factor: extern "C" fn(u32) -> u32,
    pub op_get_price: extern "C" fn(u32, u64) -> i64,
    pub op_grf_version: extern "C" fn(u32) -> u32,
    pub op_length_default: extern "C" fn(u32) -> u32,
    pub op_age_default: extern "C" fn(u32) -> u32,
    pub op_speed_default: extern "C" fn(u32) -> u32,
    pub op_length_error: extern "C" fn(u32, u32) -> (),
    pub op_disconnect: extern "C" fn() -> (),
    pub op_explosion: extern "C" fn(u32) -> (),
    pub op_sound_default: extern "C" fn(u32) -> u32,
    pub op_sound: extern "C" fn(u32, u16) -> (),
    pub op_sound_old1: extern "C" fn() -> u32,
    pub op_sound_old2: extern "C" fn() -> u32,
    pub op_engine_invalid: extern "C" fn() -> u32,
    pub op_invalid_price: extern "C" fn() -> u32,
    pub op_cost_divisor: extern "C" fn() -> u32,
    pub op_is_crossing: extern "C" fn(u32) -> u32,
    pub op_new_position: extern "C" fn(u32) -> Position,
    pub op_virt_tile: extern "C" fn(i32, i32) -> u32,
    pub op_is_road_stop: extern "C" fn(u32) -> u32,
    pub op_set_dest: extern "C" fn(u32, u32) -> (),
    pub op_cache_invalidate: extern "C" fn(u32) -> (),
    pub op_arrival: extern "C" fn(u32, u16, u32, bool) -> (),
    pub op_crash_news: extern "C" fn(u32, u32) -> (),
    pub op_station_visits: extern "C" fn(u16) -> u32,
    pub op_station_visit_set: extern "C" fn(u16, u32) -> (),
    pub op_local_company: extern "C" fn() -> u32,
    pub op_enter_tile: extern "C" fn(u32, u32, i32, i32) -> u32,
    pub op_enter_depot: extern "C" fn(u32) -> (),
    pub op_process_orders: extern "C" fn(u32) -> (),
    pub op_loading: extern "C" fn(u32) -> (),
    pub op_begin_loading: extern "C" fn(u32) -> (),
    pub op_tram_probe: extern "C" fn(u32, u32, u8) -> u32,
    pub op_property: extern "C" fn(u32, u8, u32) -> u32,
    pub op_length_callback: extern "C" fn(u32) -> u32,
    pub op_play_sound: extern "C" fn(u32) -> u32,
    pub op_visual: extern "C" fn(u32) -> (),
    pub op_update_visual: extern "C" fn(u32) -> (),
    pub op_cargo_changed: extern "C" fn(u32) -> (),
    pub op_length_changed: extern "C" fn(u32) -> (),
    pub op_breakdown: extern "C" fn(u32) -> u32,
    pub op_delete: extern "C" fn(u32) -> (),
    pub op_ground_crash: extern "C" fn(u32, bool) -> u32,
    pub op_stop_random: extern "C" fn(u32, u16) -> (),
    pub op_stop_animation: extern "C" fn(u32, u16) -> (),
    pub op_yapf: extern "C" fn(u32, u32, u8, u16) -> TrackChoice,
    pub op_find_depot: extern "C" fn(u32, i32) -> DepotResult,
    pub op_inclination: extern "C" fn(u32, bool, bool) -> i32,
    pub op_viewport: extern "C" fn(u32, bool, bool) -> (),
    pub set_tile: extern "C" fn(u32, u32) -> (),
    pub set_x: extern "C" fn(u32, i32) -> (),
    pub set_y: extern "C" fn(u32, i32) -> (),
    pub set_direction: extern "C" fn(u32, u8) -> (),
    pub set_speed: extern "C" fn(u32, u16) -> (),
    pub set_tick: extern "C" fn(u32, u8) -> (),
    pub set_running: extern "C" fn(u32, u8) -> (),
    pub set_day: extern "C" fn(u32, u8) -> (),
    pub set_order_time: extern "C" fn(u32, i32) -> (),
    pub set_progress: extern "C" fn(u32, u8) -> (),
    pub set_last_station: extern "C" fn(u32, u16) -> (),
    pub set_hidden: extern "C" fn(u32, bool) -> (),
    pub set_first_engine: extern "C" fn(u32, u16) -> (),
    pub set_length: extern "C" fn(u32, u8) -> (),
    pub set_total_length: extern "C" fn(u32, u16) -> (),
    pub set_cargo_age: extern "C" fn(u32, u16) -> (),
    pub set_max_speed: extern "C" fn(u32, u16) -> (),
    pub set_suppress_implicit: extern "C" fn(u32) -> (),
    pub read_day: extern "C" fn(u32) -> u32,
    pub read_dest: extern "C" fn(u32) -> u32,
    pub read_direction: extern "C" fn(u32) -> u32,
    pub read_engine: extern "C" fn(u32) -> u32,
    pub read_first: extern "C" fn(u32) -> u32,
    pub read_front: extern "C" fn(u32) -> u32,
    pub read_last_station: extern "C" fn(u32) -> u32,
    pub read_length: extern "C" fn(u32) -> u32,
    pub read_next: extern "C" fn(u32) -> u32,
    pub read_order_type: extern "C" fn(u32) -> u32,
    pub read_previous: extern "C" fn(u32) -> u32,
    pub read_progress: extern "C" fn(u32) -> u32,
    pub read_running: extern "C" fn(u32) -> u32,
    pub read_speed: extern "C" fn(u32) -> u32,
    pub read_status: extern "C" fn(u32) -> u32,
    pub read_tick: extern "C" fn(u32) -> u32,
    pub read_tile: extern "C" fn(u32) -> u32,
    pub read_total_length: extern "C" fn(u32) -> u32,
    pub read_tram: extern "C" fn(u32) -> u32,
    pub speed_limits: extern "C" fn(u32, *mut SpeedLimits) -> (),
    pub consist_speed: extern "C" fn(u32, *mut ConsistSpeed) -> (),
    pub close_origin: extern "C" fn(u32, *mut CloseOrigin) -> (),
    pub close_candidate: extern "C" fn(u32, *mut CloseCandidate) -> (),
    pub overtake_origin: extern "C" fn(u32, *mut OvertakeOrigin) -> (),
    pub overtake_speed: extern "C" fn(u32, *mut OvertakeSpeed) -> (),
    pub sliding_position: extern "C" fn(u32, *mut SlidingPosition) -> (),
    pub height_speed: extern "C" fn(u32, *mut HeightSpeed) -> (),
    pub collision_part: extern "C" fn(u32, *mut CollisionPart) -> (),
    pub collision_origin: extern "C" fn(u32, *mut CollisionOrigin) -> (),
    pub crash_direction: extern "C" fn(u32, *mut CrashDirection) -> (),
    pub path_vehicle: extern "C" fn(u32, *mut PathVehicle) -> (),
    pub depot_part: extern "C" fn(u32, *mut DepotPart) -> (),
    pub depot_orders: extern "C" fn(u32, *mut DepotOrders) -> (),
    pub vehicle_tile: extern "C" fn(u32, *mut VehicleTile) -> (),
    pub arrival_vehicle: extern "C" fn(u32, *mut ArrivalVehicle) -> (),
    pub tunnel_vehicle: extern "C" fn(u32, *mut TunnelVehicle) -> (),
    pub move_vehicle: extern "C" fn(u32, *mut MoveVehicle) -> (),
    pub move_transition: extern "C" fn(u32, *mut MoveTransition) -> (),
    pub move_position: extern "C" fn(u32, *mut MovePosition) -> (),
    pub block_vehicle: extern "C" fn(u32, *mut BlockVehicle) -> (),
    pub stop_order: extern "C" fn(u32, *mut StopOrder) -> (),
    pub move_stop: extern "C" fn(u32, *mut MoveStop) -> (),
    pub order_clock: extern "C" fn(u32, *mut OrderClock) -> (),
    pub controller_part: extern "C" fn(u32, *mut ControllerPart) -> (),
    pub service_origin: extern "C" fn(u32, *mut ServiceOrigin) -> (),
    pub service_order: extern "C" fn(u32, *mut ServiceOrder) -> (),
    pub track_direction: extern "C" fn(u32, *mut TrackDirection) -> (),
    pub slope_origin: extern "C" fn(u32, *mut SlopeOrigin) -> (),
    pub slope_part: extern "C" fn(u32, *mut SlopePart) -> (),
    pub turn_vehicle: extern "C" fn(u32, *mut TurnVehicle) -> (),
    pub owner: extern "C" fn(u32) -> *mut State,
    pub visit_close: extern "C" fn(u32, i32, i32, Visitor, *mut c_void),
    pub visit_tunnel: extern "C" fn(u32, i32, i32, Visitor, *mut c_void),
    pub visit_tile: extern "C" fn(u32, i32, i32, Visitor, *mut c_void),
    pub visit_train: extern "C" fn(u32, i32, i32, Visitor, *mut c_void),
    pub read_bus: extern "C" fn(u32) -> u32,
}
struct Game<'a> {
    id: u32,
    state: *mut State,
    leaves: &'a Leaves,
    services: &'a Services,
}
impl Game<'_> {
    fn read_day(&self, id: u32) -> u32 {
        (self.leaves.read_day)(id)
    }
    fn read_dest(&self, id: u32) -> u32 {
        (self.leaves.read_dest)(id)
    }
    fn read_direction(&self, id: u32) -> u32 {
        (self.leaves.read_direction)(id)
    }
    fn read_engine(&self, id: u32) -> u32 {
        (self.leaves.read_engine)(id)
    }
    fn read_first(&self, id: u32) -> u32 {
        (self.leaves.read_first)(id)
    }
    fn read_front(&self, id: u32) -> u32 {
        (self.leaves.read_front)(id)
    }
    fn read_last_station(&self, id: u32) -> u32 {
        (self.leaves.read_last_station)(id)
    }
    fn read_length(&self, id: u32) -> u32 {
        (self.leaves.read_length)(id)
    }
    fn read_next(&self, id: u32) -> u32 {
        (self.leaves.read_next)(id)
    }
    fn read_order_type(&self, id: u32) -> u32 {
        (self.leaves.read_order_type)(id)
    }
    fn read_previous(&self, id: u32) -> u32 {
        (self.leaves.read_previous)(id)
    }
    fn read_progress(&self, id: u32) -> u32 {
        (self.leaves.read_progress)(id)
    }
    fn read_running(&self, id: u32) -> u32 {
        (self.leaves.read_running)(id)
    }
    fn read_speed(&self, id: u32) -> u32 {
        (self.leaves.read_speed)(id)
    }
    fn read_status(&self, id: u32) -> u32 {
        (self.leaves.read_status)(id)
    }
    fn read_tick(&self, id: u32) -> u32 {
        (self.leaves.read_tick)(id)
    }
    fn read_tile(&self, id: u32) -> u32 {
        (self.leaves.read_tile)(id)
    }
    fn read_total_length(&self, id: u32) -> u32 {
        (self.leaves.read_total_length)(id)
    }
    fn read_tram(&self, id: u32) -> u32 {
        (self.leaves.read_tram)(id)
    }
    fn read_stopped(&self, id: u32) -> bool {
        self.read_status(id) & 2 != 0
    }
    fn read_crashed(&self, id: u32) -> bool {
        self.read_status(id) & 128 != 0
    }
    fn read_hidden(&self, id: u32) -> bool {
        self.read_status(id) & 1 != 0
    }
    fn speed_limits(&self, id: u32) -> SpeedLimits {
        let mut out = SpeedLimits::default();
        (self.leaves.speed_limits)(id, &raw mut out);
        out
    }
    fn consist_speed(&self, id: u32) -> ConsistSpeed {
        let mut out = ConsistSpeed::default();
        (self.leaves.consist_speed)(id, &raw mut out);
        out
    }
    fn close_origin(&self, id: u32) -> CloseOrigin {
        let mut out = CloseOrigin::default();
        (self.leaves.close_origin)(id, &raw mut out);
        out
    }
    fn close_candidate(&self, id: u32) -> CloseCandidate {
        let mut out = CloseCandidate::default();
        (self.leaves.close_candidate)(id, &raw mut out);
        out
    }

    fn overtake_origin(&self, id: u32) -> OvertakeOrigin {
        let mut out = OvertakeOrigin::default();
        (self.leaves.overtake_origin)(id, &raw mut out);
        out
    }
    fn overtake_speed(&self, id: u32) -> OvertakeSpeed {
        let mut out = OvertakeSpeed::default();
        (self.leaves.overtake_speed)(id, &raw mut out);
        out
    }
    fn sliding_position(&self, id: u32) -> SlidingPosition {
        let mut out = SlidingPosition::default();
        (self.leaves.sliding_position)(id, &raw mut out);
        out
    }
    fn observe_height_speed(&self, id: u32) -> HeightSpeed {
        let mut out = HeightSpeed::default();
        (self.leaves.height_speed)(id, &raw mut out);
        out
    }
    fn collision_part(&self, id: u32) -> CollisionPart {
        let mut out = CollisionPart::default();
        (self.leaves.collision_part)(id, &raw mut out);
        out
    }
    fn collision_origin(&self, id: u32) -> CollisionOrigin {
        let mut out = CollisionOrigin::default();
        (self.leaves.collision_origin)(id, &raw mut out);
        out
    }

    fn crash_direction(&self, id: u32) -> CrashDirection {
        let mut out = CrashDirection::default();
        (self.leaves.crash_direction)(id, &raw mut out);
        out
    }
    fn path_vehicle(&self, id: u32) -> PathVehicle {
        let mut out = PathVehicle::default();
        (self.leaves.path_vehicle)(id, &raw mut out);
        out
    }
    fn depot_part(&self, id: u32) -> DepotPart {
        let mut out = DepotPart::default();
        (self.leaves.depot_part)(id, &raw mut out);
        out
    }
    fn depot_orders(&self, id: u32) -> DepotOrders {
        let mut out = DepotOrders::default();
        (self.leaves.depot_orders)(id, &raw mut out);
        out
    }
    fn vehicle_tile(&self, id: u32) -> VehicleTile {
        let mut out = VehicleTile::default();
        (self.leaves.vehicle_tile)(id, &raw mut out);
        out
    }
    fn arrival_vehicle(&self, id: u32) -> ArrivalVehicle {
        let mut out = ArrivalVehicle::default();
        (self.leaves.arrival_vehicle)(id, &raw mut out);
        out
    }
    fn tunnel_vehicle(&self, id: u32) -> TunnelVehicle {
        let mut out = TunnelVehicle::default();
        (self.leaves.tunnel_vehicle)(id, &raw mut out);
        out
    }
    fn move_vehicle(&self, id: u32) -> MoveVehicle {
        let mut out = MoveVehicle::default();
        (self.leaves.move_vehicle)(id, &raw mut out);
        out
    }
    fn move_transition(&self, id: u32) -> MoveTransition {
        let mut out = MoveTransition::default();
        (self.leaves.move_transition)(id, &raw mut out);
        out
    }
    fn move_position(&self, id: u32) -> MovePosition {
        let mut out = MovePosition::default();
        (self.leaves.move_position)(id, &raw mut out);
        out
    }
    fn block_vehicle(&self, id: u32) -> BlockVehicle {
        let mut out = BlockVehicle::default();
        (self.leaves.block_vehicle)(id, &raw mut out);
        out
    }
    fn stop_order(&self, id: u32) -> StopOrder {
        let mut out = StopOrder::default();
        (self.leaves.stop_order)(id, &raw mut out);
        out
    }
    fn move_stop(&self, id: u32) -> MoveStop {
        let mut out = MoveStop::default();
        (self.leaves.move_stop)(id, &raw mut out);
        out
    }
    fn order_clock(&self, id: u32) -> OrderClock {
        let mut out = OrderClock::default();
        (self.leaves.order_clock)(id, &raw mut out);
        out
    }
    fn controller_part(&self, id: u32) -> ControllerPart {
        let mut out = ControllerPart::default();
        (self.leaves.controller_part)(id, &raw mut out);
        out
    }
    fn service_origin(&self, id: u32) -> ServiceOrigin {
        let mut out = ServiceOrigin::default();
        (self.leaves.service_origin)(id, &raw mut out);
        out
    }
    fn service_order(&self, id: u32) -> ServiceOrder {
        let mut out = ServiceOrder::default();
        (self.leaves.service_order)(id, &raw mut out);
        out
    }
    fn track_direction(&self, id: u32) -> TrackDirection {
        let mut out = TrackDirection::default();
        (self.leaves.track_direction)(id, &raw mut out);
        out
    }
    fn slope_origin(&self, id: u32) -> SlopeOrigin {
        let mut out = SlopeOrigin::default();
        (self.leaves.slope_origin)(id, &raw mut out);
        out
    }
    fn slope_part(&self, id: u32) -> SlopePart {
        let mut out = SlopePart::default();
        (self.leaves.slope_part)(id, &raw mut out);
        out
    }
    fn turn_vehicle(&self, id: u32) -> TurnVehicle {
        let mut out = TurnVehicle::default();
        (self.leaves.turn_vehicle)(id, &raw mut out);
        out
    }
    fn owner(&self, id: u32) -> *mut State {
        if id == self.id {
            self.state
        } else {
            (self.leaves.owner)(id)
        }
    }
    fn get(&self, id: u32, field: u8) -> u16 {
        // SAFETY: Resolve the live shell each time; borrow ends within the accessor.
        unsafe { openttd_rust_road_get(self.owner(id), field) }
    }
    fn set(&self, id: u32, field: u8, value: u16) {
        // SAFETY: Serialized access, no Rust borrow spans a callback.
        unsafe {
            openttd_rust_road_set(self.owner(id), field, value);
        }
    }
    fn path_size(&self, id: u32) -> usize {
        // SAFETY: Live owner, short accessor only.
        unsafe { openttd_rust_road_path_size(self.owner(id)) }
    }
    fn path_back(&self, id: u32) -> PathElement {
        let owner = self.owner(id);
        // SAFETY: Caller establishes nonempty path; no service in this scope.
        unsafe { openttd_rust_road_path_get(owner, openttd_rust_road_path_size(owner) - 1) }
    }
    fn path_clear(&self, id: u32) {
        // SAFETY: Live serialized owner.
        unsafe {
            openttd_rust_road_path_clear(self.owner(id));
        }
    }
    fn path_push(&self, id: u32, dir: u8, tile: u32) {
        // SAFETY: Live serialized owner.
        unsafe {
            openttd_rust_road_path_push(
                self.owner(id),
                PathElement {
                    trackdir: dir,
                    tile,
                },
            );
        }
    }
    fn path_pop(&self, id: u32) {
        // SAFETY: Caller establishes nonempty path.
        unsafe {
            openttd_rust_road_path_pop(self.owner(id));
        }
    }
    fn visit<F: FnMut(u32) -> bool>(
        callback: extern "C" fn(u32, i32, i32, Visitor, *mut c_void),
        id: u32,
        x: i32,
        y: i32,
        mut visitor: F,
    ) {
        extern "C" fn dispatch<F: FnMut(u32) -> bool>(context: *mut c_void, id: u32) -> bool {
            // SAFETY: Native traversal calls synchronously, once at a time, and retains no context.
            unsafe { (&mut *context.cast::<F>())(id) }
        }
        callback(id, x, y, dispatch::<F>, (&raw mut visitor).cast());
    }
    fn random(&self) -> u32 {
        (self.services.random)(self.services.context)
    }
}
impl Game<'_> {
    fn realistic(&self) -> bool {
        ((self.leaves.op_acc_model)()) == 1
    }
    fn side(&self) -> usize {
        (((self.leaves.op_road_side)()) as usize) << 4
    }
    fn max_speed(&self, id: u32) -> i32 {
        let v = self.speed_limits(id);
        let mut speed = v.max_track_speed as i32;
        let mut uid = id;
        while uid != INVALID {
            let u = self.consist_speed(uid);
            if self.realistic() {
                let state = self.get(id, 0);
                if state <= 15 && reversing(state as u8) {
                    speed = (v.max_track_speed / 2) as i32;
                    break;
                }
                if u.direction & 1 == 0 {
                    speed = (v.max_track_speed * 3 / 4) as i32;
                }
            }
            if self.get(uid, 0) == WORMHOLE && !u.hidden() {
                speed = speed.min((((self.leaves.op_bridge_speed)(u.tile)) * 2) as i32);
            }
            uid = u.next;
        }
        speed.min((v.order_max_speed * 2) as i32)
    }
    fn update_speed(&self, id: u32) -> i32 {
        let over = self.get(id, 3) != 0;
        let (accel, min) = if self.realistic() {
            (
                ((((self.leaves.op_acceleration)(id)) as u32) as i32).wrapping_add(if over {
                    256
                } else {
                    0
                }) as u32,
                if self.read_stopped(id) { 0 } else { 4 },
            )
        } else {
            (if over { 512 } else { 256 }, 0)
        };
        (self.leaves.op_update_speed)(id, accel, min, (self.max_speed(id) as u64) as i32)
    }
    fn close(&self, id: u32, x: i32, y: i32, dir: u32, update: bool) -> u32 {
        let v = self.close_origin(id);
        let front = v.first;
        if self.get(front, 6) != 0 {
            return INVALID;
        }
        let mut best = INVALID;
        let mut best_diff = u32::MAX;
        let kind = u32::from(self.get(front, 0) == WORMHOLE);
        let callback = if kind == 0 {
            self.leaves.visit_close
        } else {
            self.leaves.visit_tunnel
        };
        Self::visit(callback, id, x, y, |uid| {
            if (self.leaves.read_type)(uid) != 1 {
                return true;
            }
            let u = self.close_candidate(uid);
            let xd = u.x as i32 - x;
            let yd = u.y as i32 - y;
            if self.get(uid, 0) == DEPOT || v.first == u.first {
                return true;
            }
            if (u.z as i32 - v.z as i32).abs() >= 6 || u.direction != dir {
                return true;
            }
            let diff = xd.unsigned_abs() + yd.unsigned_abs();
            if diff > best_diff || (diff == best_diff && uid > best) {
                return true;
            }
            const DX: [i32; 8] = [-4, -8, -4, -1, 4, 8, 4, 1];
            const DY: [i32; 8] = [-4, -1, 4, 8, 4, 1, -4, -8];
            let axis = |dist: i32, diff: i32| {
                if dist < 0 {
                    diff > dist && diff <= 0
                } else {
                    diff < dist && diff >= 0
                }
            };
            if axis(DX[u.direction as usize], xd) && axis(DY[u.direction as usize], yd) {
                best = uid;
                best_diff = diff;
            }
            true
        });
        if best_diff == u32::MAX {
            self.set(front, 2, 0);
            return INVALID;
        }
        if update {
            let counter = self.get(front, 2).wrapping_add(1);
            self.set(front, 2, counter);
            if counter > 1480 {
                return INVALID;
            }
        }
        best
    }
    fn overtake_blocked(&self, id: u32, other: u32, tile: u32, dir: u8) -> bool {
        if ((self.leaves.op_has_road)(id, tile)) == 0 {
            return true;
        }
        let status = (self.leaves.op_track_status)(id, tile);
        let tracks = status & 0x3f3f;
        let red = (status >> 16) & 0x3f3f;
        let bits = ((tracks | (tracks >> 8)) & 63) as u8;
        if tracks & (1 << dir) == 0 || bits & !3 != 0 || red != 0 {
            return true;
        }
        let mut blocked = false;
        Self::visit(self.leaves.visit_tile, id, tile as i32, 0, |uid| {
            blocked = (self.leaves.read_type)(uid) == 1
                && self.read_first(uid) == uid
                && uid != id
                && uid != other;
            !blocked
        });
        blocked
    }
    fn overtake(&self, id: u32, other: u32) {
        let v = self.overtake_origin(id);
        let u = self.overtake_speed(other);
        if v.tram()
            || ((self.leaves.op_tile_type)(v.tile)) == 5
            || ((self.leaves.op_tile_type)(u.tile)) == 5
            || v.articulated != 0
        {
            return;
        }
        if v.direction != u.direction || v.direction & 1 == 0 {
            return;
        }
        let state = self.get(id, 0);
        if state >= STOP || !straight((state & 15) as u8) {
            return;
        }
        let uspeed =
            if !self.realistic() || ((((self.leaves.op_acceleration)(other)) as u32) as i32) > 0 {
                self.max_speed(other)
            } else {
                u.speed as i32
            };
        if uspeed >= self.max_speed(id) && !u.stopped() && u.speed != 0 {
            return;
        }
        let dir = DIAG_TRACK[diag(v.direction) as usize];
        if self.overtake_blocked(id, other, v.tile, dir) {
            return;
        }
        let tile = v
            .tile
            .wrapping_add((self.leaves.op_offset)(diag(v.direction) as u8));
        if self.overtake_blocked(id, other, tile, dir) {
            return;
        }
        self.set(id, 4, if u.speed == 0 || u.stopped() { 17 } else { 0 });
        self.set(id, 3, 16);
    }
    fn sliding(&self, id: u32, x: i32, y: i32) -> u32 {
        let v = self.sliding_position(id);
        let xd = x - v.x as i32 + 1;
        let yd = y - v.y as i32 + 1;
        let new = if xd as u32 > 2 || yd as u32 > 2 {
            v.direction
        } else {
            [0, 7, 6, 255, 1, 0, 5, 255, 2, 3, 4][(yd * 4 + xd) as usize]
        };
        if new == v.direction {
            return new;
        }
        (v.direction
            + if new.wrapping_sub(v.direction) & 7 > 4 {
                7
            } else {
                1
            })
            & 7
    }
    fn height_speed(&self, id: u32, old: i32) {
        let v = self.observe_height_speed(id);
        if old == v.z as i32 || self.realistic() {
            return;
        }
        if old < (v.z as i32) {
            (self.leaves.set_speed)(id, (v.speed * 232 / 256) as u16);
        } else {
            let speed = v.speed.wrapping_add(2) as u16;
            if u32::from(speed) <= v.max_track_speed {
                (self.leaves.set_speed)(id, speed);
            }
        }
    }
    fn position(&self, id: u32, x: i32, y: i32, new_tile: bool, delta: bool) {
        (self.leaves.set_x)(id, x);
        (self.leaves.set_y)(id, y);
        (self.leaves.op_position)(id);
        let old = (self.leaves.op_inclination)(id, new_tile, delta);
        self.height_speed(id, old);
    }
    fn direction(&self, id: u32, dir: u32) {
        if dir != self.read_direction(id) {
            (self.leaves.set_direction)(id, dir as u8);
            if !self.realistic() {
                let speed = self.read_speed(id);
                (self.leaves.set_speed)(id, (speed - (speed >> 2)) as u16);
            }
        }
    }
    fn sound(&self, id: u32) {
        if ((self.leaves.op_play_sound)(id)) == 0 {
            let mut sound = (self.leaves.op_sound_default)(id);
            if sound == ((self.leaves.op_sound_old1)()) && self.read_tick(id) & 3 == 0 {
                sound = (self.leaves.op_sound_old2)();
            }
            (self.leaves.op_sound)(id, sound as u16);
        }
    }
    fn crash(&self, id: u32, flooded: bool) -> u32 {
        let mut victims = (self.leaves.op_ground_crash)(id, flooded);
        if self.read_front(id) != 0 {
            victims = victims.wrapping_add(1);
            if in_range(self.get(id, 0), DT_STOP, DT_STOP + 16) {
                (self.leaves.op_roadstop_leave)(id);
            }
        }
        self.set(id, 5, if flooded { 2000 } else { 1 });
        victims
    }
    fn train_crash(&self, id: u32) -> bool {
        let mut uid = id;
        while uid != INVALID {
            let u = self.collision_part(uid);
            if self.get(uid, 0) != WORMHOLE && ((self.leaves.op_is_crossing)(u.tile)) != 0 {
                let v = self.collision_origin(id);
                let mut collision = false;
                Self::visit(self.leaves.visit_train, id, v.x as i32, v.y as i32, |tid| {
                    collision = (self.leaves.read_type)(tid) == 0
                        && ((self.leaves.read_z)(tid) as i32 - u.z as i32).abs() <= 6;
                    !collision
                });
                if collision {
                    let victims = self.crash(id, false);
                    (self.leaves.op_crash_news)(id, victims);
                    return true;
                }
            }
            uid = u.next;
        }
        false
    }
    fn crashed(&self, id: u32) -> bool {
        let counter = self.get(id, 5).wrapping_add(1);
        self.set(id, 5, counter);
        if counter == 2 {
            (self.leaves.op_explosion)(id);
        } else if counter <= 45 {
            if self.read_tick(id) & 7 == 0 {
                let mut uid = id;
                while uid != INVALID {
                    let random = self.random();
                    let u = self.crash_direction(uid);
                    let delta = [7, 0, 0, 1][(random & 3) as usize];
                    (self.leaves.set_direction)(uid, ((u.direction + delta) & 7) as u8);
                    (self.leaves.op_viewport)(uid, true, true);
                    uid = self.read_next(uid);
                }
            }
        } else if counter >= 2220 && self.read_tick(id) & 31 == 0 {
            let front = self.read_first(id);
            let alive = self.read_next(id) != INVALID;
            let mut previous = id;
            let mut last = id;
            while self.read_next(last) != INVALID {
                previous = last;
                last = self.read_next(last);
            }
            (self.leaves.op_set_next)(previous, INVALID);
            (self.leaves.set_last_station)(last, self.read_last_station(front) as u16);
            if in_range(self.get(last, 0), STOP, STOP + 16) {
                (self.leaves.op_roadstop_leave)(last);
            }
            (self.leaves.op_delete)(last);
            return alive;
        }
        true
    }
    fn update_cache(&self, id: u32, same: bool) {
        (self.leaves.op_cache_invalidate)(id);
        (self.leaves.set_total_length)(id, 0_u16);
        let mut uid = id;
        while uid != INVALID {
            let engine = if uid == id {
                (self.leaves.op_engine_invalid)()
            } else {
                self.read_engine(id)
            };
            (self.leaves.set_first_engine)(uid, engine as u16);
            let mut length = 8;
            let factor = if ((self.leaves.op_grf_version)(uid)) >= 8 {
                let value = u32::from(((self.leaves.op_property)(uid, 0x23_u8, 65535_u32)) as u16);
                if value != 65535 && value >= 8 {
                    (self.leaves.op_length_error)(uid, value);
                }
                value
            } else {
                u32::from(((self.leaves.op_length_callback)(uid)) as u16)
            };
            let factor = if factor == 65535 {
                (self.leaves.op_length_default)(uid)
            } else {
                factor
            };
            if factor != 0 {
                length -= factor.min(7);
            }
            if same && length != self.read_length(uid) {
                (self.leaves.op_length_changed)(uid);
            }
            (self.leaves.set_length)(uid, length as u8);
            (self.leaves.set_total_length)(
                id,
                self.read_total_length(id)
                    .wrapping_add(self.read_length(uid)) as u16,
            );
            (self.leaves.op_update_visual)(uid);
            let age = (self.leaves.op_property)(uid, 0x22_u8, (self.leaves.op_age_default)(uid));
            (self.leaves.set_cargo_age)(uid, (age) as u16);
            uid = self.read_next(uid);
        }
        let speed = (self.leaves.op_property)(id, 0x15_u8, 0_u32);
        (self.leaves.set_max_speed)(
            id,
            if speed != 0 {
                speed.wrapping_mul(4)
            } else {
                (self.leaves.op_speed_default)(id)
            } as u16,
        );
    }
    fn path(&self, id: u32, tile: u32, entry: u8) -> u8 {
        let status = (self.leaves.op_track_status)(id, tile);
        let red = (status >> 16) & 0x3f3f;
        let mut tracks = status & 0x3f3f;
        let v = self.path_vehicle(id);
        let kind = (self.leaves.op_tile_type)(tile);
        if kind == 2 {
            if ((self.leaves.op_is_depot)(tile)) != 0
                && (((self.leaves.op_tile_owner)(tile)) != v.owner
                    || ((self.leaves.op_depot_dir)(tile)) == u32::from(entry))
            {
                tracks = 0;
            }
        } else if kind == 5 && ((self.leaves.op_bay_stop)(tile)) != 0 {
            if ((self.leaves.op_tile_owner)(tile)) != v.owner
                || ((self.leaves.op_bay_dir)(tile)) == u32::from(entry)
                || v.articulated != 0
            {
                tracks = 0;
            } else if ((self.leaves.op_stop_type)(tile))
                != u32::from((self.leaves.read_bus)(id) == 0)
                || (((self.leaves.op_queue)()) == 0 && ((self.leaves.op_free_bay)(tile)) == 0)
            {
                tracks = 0;
            }
        }
        tracks &= [0x1009, 0x0016, 0x0520, 0x2a00][entry as usize];
        let best;
        if tracks == 0 {
            if self.path_size(id) != 0 {
                self.path_clear(id);
            }
            best = REVERSE[entry as usize];
        } else {
            let mut forced = None;
            if self.get(id, 6) != 0 {
                let mut reverse = true;
                if v.tram() {
                    let bits = (self.leaves.op_any_road_bits)(id, tile, false);
                    let straight = if entry & 1 == 0 { 10 } else { 5 };
                    reverse = (bits & straight) == straight || bits == (8 >> entry);
                }
                if reverse {
                    self.set(id, 6, 0);
                    if v.tile != tile {
                        forced = Some(REVERSE[entry as usize]);
                    }
                }
            }
            best = if let Some(dir) = forced {
                dir
            } else if self.read_dest(id) == 0 {
                let mut n =
                    ((u64::from(self.random()) * u64::from(tracks.count_ones())) >> 32) as u32;
                let mut bits = tracks;
                let mut bit = 0;
                loop {
                    if bits & 1 != 0 {
                        if n == 0 {
                            break;
                        }
                        n -= 1;
                    }
                    bits >>= 1;
                    bit += 1;
                }
                bit
            } else if tracks & (tracks - 1) == 0 {
                if self.path_size(id) != 0 && self.path_back(id).tile == tile {
                    self.path_clear(id);
                }
                tracks.trailing_zeros() as u8
            } else {
                let mut cached = None;
                if self.path_size(id) != 0 {
                    let item = self.path_back(id);
                    if item.tile == tile && tracks & (1 << item.trackdir) != 0 {
                        self.path_pop(id);
                        cached = Some(item.trackdir);
                    } else {
                        self.path_clear(id);
                    }
                }
                if let Some(dir) = cached {
                    dir
                } else {
                    let result = (self.leaves.op_yapf)(id, tile, entry, tracks as u16);
                    (self.leaves.op_path_result)(id, result.found);
                    result.trackdir
                }
            };
        }
        if red & (1 << best) != 0 { 255 } else { best }
    }
    fn leave_depot(&self, id: u32, first: bool) -> bool {
        let tile = self.read_tile(id);
        let mut uid = id;
        while uid != INVALID {
            let u = self.depot_part(uid);
            if self.get(uid, 0) != DEPOT || u.tile != tile {
                return false;
            }
            uid = u.next;
        }
        let dir = (self.leaves.op_depot_dir)(tile);
        (self.leaves.set_direction)(id, (dir * 2 + 1) as u8);
        let tdir = DIAG_TRACK[dir as usize];
        let rd = crate::road_data::entry(self.read_tram(id) != 0, self.side() + tdir as usize, 6);
        let x = (((self.leaves.op_tile_x)(tile)) * 16 + u32::from(rd.0 & 15)) as i32;
        let y = (((self.leaves.op_tile_y)(tile)) * 16 + u32::from(rd.1 & 15)) as i32;
        if first {
            let v = self.depot_orders(id);
            if v.order_type == 2 && tile == v.dest {
                (self.leaves.op_enter_depot)(id);
                return true;
            }
            if self.close(id, x, y, self.read_direction(id), false) != INVALID {
                return true;
            }
            (self.leaves.op_service)(id);
            (self.leaves.op_leave_unbunch)(id);
            self.sound(id);
            (self.leaves.set_speed)(id, 0_u16);
        }
        (self.leaves.set_hidden)(id, false);
        self.set(id, 0, u16::from(tdir));
        self.set(id, 1, 6);
        (self.leaves.set_x)(id, x);
        (self.leaves.set_y)(id, y);
        (self.leaves.op_position)(id);
        (self.leaves.op_inclination)(id, true, true);
        (self.leaves.op_depot_dirty)(id);
        true
    }
    fn follow(&self, id: u32, previous: u32, tile: u32, entry: u8, reversed: bool) -> u8 {
        let v = self.vehicle_tile(id);
        let prev = self.vehicle_tile(previous);
        if prev.tile == v.tile && !reversed {
            return REVERSE[entry as usize];
        }
        let state = self.get(previous, 0);
        let dir;
        if state == WORMHOLE || state == DEPOT {
            let diagonal = if ((self.leaves.op_tile_type)(tile)) == 9 {
                (self.leaves.op_tunnel_dir)(tile)
            } else if ((self.leaves.op_is_depot)(tile)) != 0 {
                ((self.leaves.op_depot_dir)(tile)) ^ 2
            } else {
                255
            };
            if diagonal == 255 {
                return 255;
            }
            dir = DIAG_TRACK[diagonal as usize];
        } else if reversed && (prev.tile != tile || (state < 16 && reversing(state as u8))) {
            let north = if prev.tile != tile {
                prev.tile < tile
            } else {
                state == 15 || state == 6
            };
            dir = [[10, 13, 12, 2], [5, 11, 3, 4]][usize::from(!north)][(entry ^ 2) as usize];
        } else if state & 64 != 0 {
            dir = (state & 9) as u8;
        } else if state < 16 {
            dir = state as u8;
        } else {
            return 255;
        }
        let required = [10, 5, 9, 6, 3, 12, 10, 5][(dir & 7) as usize];
        if required & ((self.leaves.op_any_road_bits)(id, tile, true)) == 0 {
            255
        } else {
            dir
        }
    }
}
fn in_range(value: u16, start: u16, end: u16) -> bool {
    value >= start && value < end
}
fn reversing(dir: u8) -> bool {
    dir & 7 >= 6
}
fn straight(dir: u8) -> bool {
    dir & 7 <= 1
}
fn diag(dir: u32) -> u32 {
    (dir >> 1) & 3
}
impl Game<'_> {
    fn arrive_load(&self, id: u32, station: u32) {
        let v = self.arrival_vehicle(id);
        let bus = (self.leaves.read_bus)(id);
        let bit = if bus != 0 { 4 } else { 8 };
        if ((self.leaves.op_station_visits)(station as u16)) & bit == 0 {
            (self.leaves.op_station_visit_set)(station as u16, bit);
            let headline = if bus != 0 { 0 } else { 2 } + u32::from(v.tram());
            let local = v.owner == ((self.leaves.op_local_company)());
            (self.leaves.op_arrival)(id, station as u16, headline, local);
        }
        (self.leaves.op_begin_loading)(id);
        (self.leaves.op_stop_random)(id, station as u16);
        (self.leaves.op_stop_animation)(id, station as u16);
    }
    fn individual(&self, id: u32, previous: u32) -> bool {
        if self.get(id, 3) != 0 {
            if ((self.leaves.op_tile_type)(self.read_tile(id))) == 5 {
                self.set(id, 3, 0);
            } else {
                let counter = self.get(id, 4).wrapping_add(1) as u8;
                self.set(id, 4, u16::from(counter));
                if counter >= 35 && self.get(id, 0) < STOP && straight(self.get(id, 0) as u8) {
                    self.set(id, 3, 0);
                }
            }
        }
        if self.get(id, 0) == DEPOT {
            return true;
        }
        if self.get(id, 0) == WORMHOLE {
            let v = self.tunnel_vehicle(id);
            let position = (self.leaves.op_new_position)(id);
            let x = position.x;
            let y = position.y;
            let new_tile = (self.leaves.op_virt_tile)(x as u32 as i32, y as u32 as i32);
            if v.front() {
                let blocking = self.close(id, x, y, v.direction, true);
                if blocking != INVALID {
                    (self.leaves.set_speed)(id, self.read_speed(self.read_first(blocking)) as u16);
                    return false;
                }
            }
            if ((self.leaves.op_tile_type)(new_tile)) == 9
                && ((self.leaves.op_enter_tile)(id, new_tile, x, y)) & 2 != 0
            {
                (self.leaves.set_x)(id, x);
                (self.leaves.set_y)(id, y);
                (self.leaves.op_position)(id);
                (self.leaves.op_inclination)(id, true, true);
                return true;
            }
            (self.leaves.set_x)(id, x);
            (self.leaves.set_y)(id, y);
            (self.leaves.op_position)(id);
            if !self.read_hidden(id) {
                (self.leaves.op_base_viewport)(id);
            }
            return true;
        }
        let v = self.move_vehicle(id);
        let state = self.get(id, 0);
        let movement = if state & 64 != 0 { state & 9 } else { state };
        let rd = crate::road_data::entry(
            v.tram(),
            (movement as usize + self.side()) ^ self.get(id, 3) as usize,
            self.get(id, 1) as usize + 1,
        );
        if rd.0 & 128 != 0 {
            let mut tile = v
                .tile
                .wrapping_add((self.leaves.op_offset)(u32::from(rd.0 & 3) as u8));
            let mut dir = if v.front() {
                if ((self.leaves.op_has_road)(id, tile)) != 0 {
                    self.path(id, tile, rd.0 & 3)
                } else {
                    REVERSE[(rd.0 & 3) as usize]
                }
            } else {
                self.follow(id, previous, tile, rd.0 & 3, false)
            };
            if dir == 255 {
                if !v.front() {
                    (self.leaves.op_disconnect)();
                }
                (self.leaves.set_speed)(id, 0_u16);
                return false;
            }
            loop {
                let mut frame = 0;
                if reversing(dir) {
                    self.set(id, 3, 0);
                    if self.read_tram(id) != 0 {
                        let needed = [2, 1, 8, 4][match dir {
                            6 => 0,
                            7 => 1,
                            14 => 2,
                            15 => 3,
                            _ => unreachable!(),
                        }];
                        let previous_id = self.read_previous(id);
                        let big = (previous_id != INVALID && self.read_tile(previous_id) == tile)
                            || ((self.read_front(id) != 0)
                                && ((self.leaves.op_normal_road)(tile)) != 0
                                && ((self.leaves.op_road_works)(tile)) == 0
                                && ((self.leaves.op_has_road)(id, tile)) != 0
                                && needed & ((self.leaves.op_road_bits)(id, tile)) != 0);
                        if !big {
                            if (self.read_front(id) == 0)
                                || ((self.leaves.op_tram_probe)(id, tile, needed as u8)) == 0
                                || (!needed
                                    & ((self.leaves.op_any_road_bits)(
                                        id,
                                        self.read_tile(id),
                                        false,
                                    )))
                                    == 0
                            {
                                tile = self.read_tile(id);
                                frame = 16;
                            } else {
                                (self.leaves.set_speed)(id, 0_u16);
                                return false;
                            }
                        }
                    } else if ((self.leaves.op_normal_road)(self.read_tile(id))) != 0
                        && ((self.leaves.op_disallowed)(self.read_tile(id))) != 0
                    {
                        (self.leaves.set_speed)(id, 0_u16);
                        return false;
                    } else {
                        tile = self.read_tile(id);
                    }
                }
                let pos = crate::road_data::entry(
                    self.read_tram(id) != 0,
                    (dir as usize + self.side()) ^ self.get(id, 3) as usize,
                    frame,
                );
                let x = (((self.leaves.op_tile_x)(tile)) * 16 + u32::from(pos.0)) as i32;
                let y = (((self.leaves.op_tile_y)(tile)) * 16 + u32::from(pos.1)) as i32;
                let new_dir = self.sliding(id, x, y);
                if self.read_front(id) != 0 {
                    let blocking = self.close(id, x, y, new_dir, true);
                    if blocking != INVALID {
                        (self.leaves.set_speed)(
                            id,
                            self.read_speed(self.read_first(blocking)) as u16,
                        );
                        self.path_push(id, dir, tile);
                        return false;
                    }
                }
                let enter = (self.leaves.op_enter_tile)(id, tile, x, y);
                if enter & 4 != 0 {
                    if ((self.leaves.op_tile_type)(tile)) != 9 {
                        (self.leaves.set_speed)(id, 0_u16);
                        return false;
                    }
                    dir = REVERSE[(rd.0 & 3) as usize];
                    continue;
                }
                let v = self.vehicle_tile(id);
                if in_range(self.get(id, 0), STOP, DT_STOP + 16)
                    && ((self.leaves.op_tile_type)(v.tile)) == 5
                {
                    if reversing(dir) && in_range(self.get(id, 0), STOP, STOP + 16) {
                        (self.leaves.set_speed)(id, 0_u16);
                        return false;
                    }
                    if ((self.leaves.op_is_dt_stop)(v.tile)) != 0
                        && ((self.leaves.op_continuation)(v.tile, tile)) != 0
                        && v.tile != tile
                    {
                        dir = self.get(id, 0) as u8;
                    } else if ((self.leaves.op_is_road_stop)(v.tile)) != 0 {
                        (self.leaves.op_roadstop_leave)(id);
                    }
                }
                if enter & 2 == 0 {
                    let old_tile = self.read_tile(id);
                    (self.leaves.set_tile)(id, tile);
                    self.set(id, 0, u16::from(dir));
                    self.set(id, 1, frame as u16);
                    if ((self.leaves.op_road_type)(id, old_tile))
                        != ((self.leaves.op_road_type)(id, tile))
                    {
                        if self.read_front(id) != 0 {
                            self.update_cache(id, false);
                        }
                        (self.leaves.op_cargo_changed)(self.read_first(id));
                    }
                }
                self.direction(id, new_dir);
                self.position(id, x, y, true, true);
                return true;
            }
        }
        if rd.0 & 64 != 0 {
            let mut frame = 1;
            let v = self.move_vehicle(id);
            let bits = (self.leaves.op_any_road_bits)(id, v.tile, true);
            let dir = if v.tram()
                && ((self.leaves.op_is_depot)(v.tile)) == 0
                && bits != 0
                && bits & (bits - 1) == 0
            {
                frame = 21;
                [14, 15, 6, 7][(rd.0 & 3) as usize]
            } else if v.front() {
                self.path(id, v.tile, rd.0 & 3)
            } else {
                self.follow(id, previous, v.tile, rd.0 & 3, true)
            };
            if dir == 255 {
                (self.leaves.set_speed)(id, 0_u16);
                return false;
            }
            let pos =
                crate::road_data::entry(self.read_tram(id) != 0, self.side() + dir as usize, frame);
            let tile = self.read_tile(id);
            let x = (((self.leaves.op_tile_x)(tile)) * 16 + u32::from(pos.0)) as i32;
            let y = (((self.leaves.op_tile_y)(tile)) * 16 + u32::from(pos.1)) as i32;
            let new_dir = self.sliding(id, x, y);
            if self.read_front(id) != 0 {
                let blocking = self.close(id, x, y, new_dir, true);
                if blocking != INVALID {
                    (self.leaves.set_speed)(id, self.read_speed(self.read_first(blocking)) as u16);
                    self.path_push(id, dir, tile);
                    return false;
                }
            }
            if ((self.leaves.op_enter_tile)(id, tile, x, y)) & 4 != 0 {
                (self.leaves.set_speed)(id, 0_u16);
                return false;
            }
            self.set(id, 0, u16::from(dir));
            self.set(id, 1, frame as u16);
            self.direction(id, new_dir);
            self.position(id, x, y, true, true);
            return true;
        }
        let v = self.move_transition(id);
        if v.next != INVALID
            && ((self.leaves.op_is_depot)(v.tile)) != 0
            && u32::from(self.get(id, 1)) == v.length + 6
        {
            self.leave_depot(v.next, false);
        }
        let v = self.collision_origin(id);
        let x = (v.x as i32 & !15) + i32::from(rd.0 & 15);
        let y = (v.y as i32 & !15) + i32::from(rd.1 & 15);
        let new_dir = self.sliding(id, x, y);
        if (self.read_front(id) != 0) && !in_range(self.get(id, 0), STOP, STOP + 16) {
            let mut blocking = self.close(id, x, y, new_dir, true);
            if blocking != INVALID {
                blocking = self.read_first(blocking);
                if self.get(id, 3) == 0 {
                    self.overtake(id, blocking);
                }
                if self.get(id, 3) == 0 {
                    (self.leaves.set_speed)(id, self.read_speed(blocking) as u16);
                }
                let v = self.move_position(id);
                if v.speed == 0
                    && in_range(self.get(id, 0), DT_STOP, DT_STOP + 16)
                    && ((self.leaves.op_order_stop)(id, v.tile)) != 0
                    && v.owner == ((self.leaves.op_tile_owner)(v.tile))
                    && v.order_type != 4
                    && ((self.leaves.op_stop_type)(v.tile))
                        == u32::from((self.leaves.read_bus)(id) == 0)
                {
                    let station = (self.leaves.op_station)(v.tile);
                    (self.leaves.set_last_station)(id, station as u16);
                    self.arrive_load(id, station);
                }
                return false;
            }
        }
        let old_dir = self.read_direction(id);
        if new_dir != old_dir {
            self.direction(id, new_dir);
            (self.leaves.op_viewport)(id, true, true);
            return true;
        }
        let v = self.block_vehicle(id);
        let state = self.get(id, 0);
        let frame = self.get(id, 1);
        if v.front()
            && ((in_range(state, STOP, STOP + 16)
                && crate::road_data::_ROAD_STOP_STOP_FRAME[state as usize - 32 + self.side()]
                    == frame as u8)
                || (in_range(state, DT_STOP, DT_STOP + 16)
                    && ((self.leaves.op_order_stop)(id, v.tile)) != 0
                    && v.owner == ((self.leaves.op_tile_owner)(v.tile))
                    && ((self.leaves.op_stop_type)(v.tile))
                        == u32::from((self.leaves.read_bus)(id) == 0)
                    && frame == 11))
        {
            let station = (self.leaves.op_station)(v.tile);
            if state & 4 == 0 {
                if ((self.leaves.op_is_dt_stop)(v.tile)) != 0 {
                    let next = v
                        .tile
                        .wrapping_add((self.leaves.op_offset)(diag(v.direction) as u8));
                    if ((self.leaves.op_continuation)(v.tile, next)) != 0
                        && ((self.leaves.op_has_road)(id, next)) != 0
                    {
                        self.set(id, 1, self.get(id, 1).wrapping_add(1));
                        self.position(id, x, y, true, false);
                        return true;
                    }
                }
                (self.leaves.op_entrance_set)(id, false);
                self.set(id, 0, self.get(id, 0) | 4);
                (self.leaves.set_last_station)(id, station as u16);
                let v = self.stop_order(id);
                if ((self.leaves.op_is_dt_stop)(v.tile)) != 0
                    || (v.order_type == 1 && v.order_destination == station)
                {
                    self.arrive_load(id, station);
                    return false;
                }
            } else {
                if ((self.leaves.op_entrance_busy)(id)) != 0 {
                    (self.leaves.set_speed)(id, 0_u16);
                    return false;
                }
                if self.read_order_type(id) == 4 {
                    (self.leaves.op_order_free)(id);
                }
            }
            if ((self.leaves.op_bay_stop)(self.read_tile(id))) != 0 {
                (self.leaves.op_entrance_set)(id, true);
            }
            self.sound(id);
            (self.leaves.op_start_stop_dirty)(id);
        }
        let tile = self.read_tile(id);
        let enter = (self.leaves.op_enter_tile)(id, tile, x, y);
        if enter & 4 != 0 {
            (self.leaves.set_speed)(id, 0_u16);
            return false;
        }
        let v = self.move_stop(id);
        if v.order_type == 4 && ((self.leaves.op_is_dt_stop)(v.tile)) != 0 {
            (self.leaves.op_order_free)(id);
        }
        if enter & 2 == 0 {
            self.set(id, 1, self.get(id, 1).wrapping_add(1));
        }
        self.position(id, x, y, false, true);
        true
    }
    fn controller(&self, id: u32) -> bool {
        let v = self.order_clock(id);
        (self.leaves.set_order_time)(id, v.order_time.wrapping_add(1) as i32);
        let reverse = self.get(id, 6);
        if reverse != 0 {
            self.set(id, 6, reverse - 1);
        }
        if self.read_crashed(id) || self.train_crash(id) {
            return self.crashed(id);
        }
        if ((self.leaves.op_breakdown)(id)) != 0 {
            return true;
        }
        if self.read_stopped(id) {
            (self.leaves.op_last_speed)(id);
            return true;
        }
        (self.leaves.op_process_orders)(id);
        (self.leaves.op_loading)(id);
        if self.read_order_type(id) == 3 {
            return true;
        }
        if self.get(id, 0) == DEPOT {
            if ((self.leaves.op_wait_unbunch)(id)) != 0 {
                return true;
            }
            if self.leave_depot(id, true) {
                return true;
            }
        }
        (self.leaves.op_visual)(id);
        let mut distance = self.update_speed(id);
        let mut advance = ((self.leaves.op_advance)(id)) as i32;
        let mut blocked = false;
        while distance >= advance {
            distance -= advance;
            let mut uid = id;
            let mut prev = INVALID;
            while uid != INVALID {
                if !self.individual(uid, prev) {
                    blocked = true;
                    break;
                }
                prev = uid;
                uid = self.read_next(uid);
            }
            if blocked {
                break;
            }
            advance = ((self.leaves.op_advance)(id)) as i32;
            if distance >= advance && self.train_crash(id) {
                break;
            }
        }
        (self.leaves.op_last_speed)(id);
        let mut uid = id;
        while uid != INVALID {
            let u = self.controller_part(uid);
            if !u.hidden() {
                (self.leaves.op_viewport)(uid, false, false);
            }
            uid = u.next;
        }
        if self.read_progress(id) == 0 {
            (self.leaves.set_progress)(
                id,
                (if blocked { advance - 1 } else { distance } as u64) as u8,
            );
        }
        true
    }
    fn set_dest(&self, id: u32, tile: u32) {
        if tile == self.read_dest(id) {
            return;
        }
        self.path_clear(id);
        (self.leaves.op_set_dest)(id, tile);
    }
    fn running_cost(&self, id: u32) -> i64 {
        if ((self.leaves.op_cost_class)(id)) == ((self.leaves.op_invalid_price)()) {
            return 0;
        }
        let factor = (self.leaves.op_property)(id, 9_u8, (self.leaves.op_cost_factor)(id));
        if factor == 0 {
            return 0;
        }
        (self.leaves.op_get_price)(id, u64::from(factor))
    }
    fn service(&self, id: u32) {
        if ((self.leaves.op_servint)(id)) == 0 || ((self.leaves.op_needs_service)(id)) == 0 {
            return;
        }
        let v = self.service_origin(id);
        let mut all_in_depot = ((self.leaves.op_is_depot)(v.tile)) != 0 && v.speed == 0;
        let mut uid = v.first;
        if all_in_depot {
            while uid != INVALID {
                let u = self.depot_part(uid);
                if self.get(uid, 0) != DEPOT || u.tile != v.tile {
                    all_in_depot = false;
                    break;
                }
                uid = u.next;
            }
        }
        if all_in_depot {
            (self.leaves.op_service)(id);
            return;
        }
        let penalty = (self.leaves.op_max_penalty)();
        // FindClosestRoadDepot returns this tile at distance zero even while
        // moving or when only part of the consist has entered the depot.
        let depot = if ((self.leaves.op_is_depot)(v.tile)) != 0 {
            DepotResult {
                tile: v.tile,
                length: 0,
            }
        } else {
            (self.leaves.op_find_depot)(id, penalty as i32)
        };
        let tile = depot.tile;
        let length = depot.length;
        if length == u32::MAX || length > penalty {
            if self.read_order_type(id) == 2 {
                (self.leaves.op_order_dummy)(id);
                (self.leaves.op_start_stop_dirty)(id);
            }
            return;
        }
        let depot = (self.leaves.op_depot_index)(tile);
        let v = self.service_order(id);
        if v.order_type == 2
            && v.order_nonstop & 1 != 0
            && !crate::services::chance16_i(1, 20, self.random())
        {
            return;
        }
        (self.leaves.set_suppress_implicit)(id);
        (self.leaves.op_order_depot)(id, depot as u16);
        self.set_dest(id, tile);
        (self.leaves.op_start_stop_dirty)(id);
    }
    fn day(&self, id: u32, calendar: bool) {
        if self.read_front(id) == 0 {
            return;
        }
        if calendar {
            (self.leaves.op_age)(id);
            return;
        }
        (self.leaves.op_economy_age)(id);
        let day = self.read_day(id).wrapping_add(1) as u8;
        (self.leaves.set_day)(id, day);
        if day & 7 == 0 {
            (self.leaves.op_decrease_value)(id);
        }
        if self.get(id, 2) == 0 {
            (self.leaves.op_check_breakdown)(id);
        }
        self.service(id);
        (self.leaves.op_check_orders)(id);
        if self.read_running(id) == 0 {
            return;
        }
        let cost = self
            .running_cost(id)
            .saturating_mul(i64::from(self.read_running(id)))
            / i64::from((self.leaves.op_cost_divisor)());
        (self.leaves.op_pay_running)(id, cost);
        (self.leaves.op_details_dirty)(id);
    }
    fn trackdir(&self, id: u32) -> u8 {
        let v = self.track_direction(id);
        if v.crashed() {
            return 255;
        }
        if self.get(id, 0) == DEPOT {
            return DIAG_TRACK[((self.leaves.op_depot_dir)(v.tile)) as usize];
        }
        if ((self.leaves.op_bay_stop)(v.tile)) != 0 {
            return DIAG_TRACK[((self.leaves.op_bay_dir)(v.tile)) as usize];
        }
        let state = self.get(id, 0);
        if state > 15 {
            return DIAG_TRACK[diag(v.direction) as usize];
        }
        if reversing(state as u8) {
            (state - 6) as u8
        } else {
            state as u8
        }
    }
    fn slope_pixel(&self, id: u32) -> bool {
        let v = self.slope_origin(id);
        let mut uid = v.first;
        let state = self.get(uid, 0);
        if state <= 15 && reversing(state as u8) {
            return true;
        }
        while uid != id {
            let u = self.slope_part(uid);
            if v.direction != u.direction {
                return true;
            }
            uid = u.next;
        }
        false
    }
    fn turn(&self, id: u32, execute: bool) -> bool {
        let v = self.turn_vehicle(id);
        if v.stopped()
            || v.crashed()
            || v.breakdown != 0
            || self.get(id, 3) != 0
            || self.get(id, 0) == WORMHOLE
            || self.get(id, 0) == DEPOT
            || v.order_type == 3
        {
            return false;
        }
        if ((self.leaves.op_normal_road)(v.tile)) != 0 && ((self.leaves.op_disallowed)(v.tile)) != 0
        {
            return false;
        }
        if ((self.leaves.op_tile_type)(v.tile)) == 9
            && diag(v.direction) == ((self.leaves.op_tunnel_dir)(v.tile))
        {
            return false;
        }
        if execute {
            self.set(id, 6, 180);
            (self.leaves.op_reset_unbunch)(id);
        }
        true
    }
    fn tick(&self, id: u32) -> bool {
        (self.leaves.set_tick)(id, self.read_tick(id).wrapping_add(1) as u8);
        if self.read_front(id) != 0 {
            if !self.read_stopped(id) {
                (self.leaves.set_running)(id, self.read_running(id).wrapping_add(1) as u8);
            }
            self.controller(id)
        } else {
            true
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_tick(
    id: u32,
    state: *mut State,
    leaves: *const Leaves,
    services: *const Services,
) -> bool {
    // SAFETY: Immutable tables outlive this synchronous call and nested entries.
    // Only raw owner pointers survive callbacks; every scalar/path borrow ends first.
    let g = unsafe {
        Game {
            id,
            state,
            leaves: &*leaves,
            services: &*services,
        }
    };
    g.tick(id)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_individual(
    id: u32,
    state: *mut State,
    previous: u32,
    leaves: *const Leaves,
    services: *const Services,
) -> bool {
    // SAFETY: Immutable tables outlive this synchronous call and nested entries.
    // Only raw owner pointers survive callbacks; every scalar/path borrow ends first.
    let g = unsafe {
        Game {
            id,
            state,
            leaves: &*leaves,
            services: &*services,
        }
    };
    g.individual(id, previous)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_leave_depot(
    id: u32,
    state: *mut State,
    first: bool,
    leaves: *const Leaves,
    services: *const Services,
) -> bool {
    // SAFETY: Immutable tables outlive this synchronous call and nested entries.
    // Only raw owner pointers survive callbacks; every scalar/path borrow ends first.
    let g = unsafe {
        Game {
            id,
            state,
            leaves: &*leaves,
            services: &*services,
        }
    };
    g.leave_depot(id, first)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_crash(
    id: u32,
    state: *mut State,
    flooded: bool,
    leaves: *const Leaves,
    services: *const Services,
) -> u32 {
    // SAFETY: Immutable tables outlive this synchronous call and nested entries.
    // Only raw owner pointers survive callbacks; every scalar/path borrow ends first.
    let g = unsafe {
        Game {
            id,
            state,
            leaves: &*leaves,
            services: &*services,
        }
    };
    g.crash(id, flooded)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_update_cache(
    id: u32,
    state: *mut State,
    same_length: bool,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable tables outlive this synchronous call and nested entries.
    // Only raw owner pointers survive callbacks; every scalar/path borrow ends first.
    let g = unsafe {
        Game {
            id,
            state,
            leaves: &*leaves,
            services: &*services,
        }
    };
    g.update_cache(id, same_length);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_calendar_day(
    id: u32,
    state: *mut State,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable tables outlive this synchronous call and nested entries.
    // Only raw owner pointers survive callbacks; every scalar/path borrow ends first.
    let g = unsafe {
        Game {
            id,
            state,
            leaves: &*leaves,
            services: &*services,
        }
    };
    g.day(id, true);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_economy_day(
    id: u32,
    state: *mut State,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable tables outlive this synchronous call and nested entries.
    // Only raw owner pointers survive callbacks; every scalar/path borrow ends first.
    let g = unsafe {
        Game {
            id,
            state,
            leaves: &*leaves,
            services: &*services,
        }
    };
    g.day(id, false);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_running_cost(
    id: u32,
    state: *mut State,
    leaves: *const Leaves,
    services: *const Services,
) -> i64 {
    // SAFETY: Immutable tables outlive this synchronous call and nested entries.
    // Only raw owner pointers survive callbacks; every scalar/path borrow ends first.
    let g = unsafe {
        Game {
            id,
            state,
            leaves: &*leaves,
            services: &*services,
        }
    };
    g.running_cost(id)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_max_speed(
    id: u32,
    state: *mut State,
    leaves: *const Leaves,
    services: *const Services,
) -> i32 {
    // SAFETY: Immutable tables outlive this synchronous call and nested entries.
    // Only raw owner pointers survive callbacks; every scalar/path borrow ends first.
    let g = unsafe {
        Game {
            id,
            state,
            leaves: &*leaves,
            services: &*services,
        }
    };
    g.max_speed(id)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_update_speed(
    id: u32,
    state: *mut State,
    leaves: *const Leaves,
    services: *const Services,
) -> i32 {
    // SAFETY: Immutable tables outlive this synchronous call and nested entries.
    // Only raw owner pointers survive callbacks; every scalar/path borrow ends first.
    let g = unsafe {
        Game {
            id,
            state,
            leaves: &*leaves,
            services: &*services,
        }
    };
    g.update_speed(id)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_set_dest(
    id: u32,
    state: *mut State,
    tile: u32,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable tables outlive this synchronous call and nested entries.
    // Only raw owner pointers survive callbacks; every scalar/path borrow ends first.
    let g = unsafe {
        Game {
            id,
            state,
            leaves: &*leaves,
            services: &*services,
        }
    };
    g.set_dest(id, tile);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_turn(
    id: u32,
    state: *mut State,
    execute: bool,
    leaves: *const Leaves,
    services: *const Services,
) -> bool {
    // SAFETY: Immutable tables outlive this synchronous call and nested entries.
    // Only raw owner pointers survive callbacks; every scalar/path borrow ends first.
    let g = unsafe {
        Game {
            id,
            state,
            leaves: &*leaves,
            services: &*services,
        }
    };
    g.turn(id, execute)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_trackdir(
    id: u32,
    state: *mut State,
    leaves: *const Leaves,
    services: *const Services,
) -> u8 {
    // SAFETY: Immutable tables outlive this synchronous call and nested entries.
    // Only raw owner pointers survive callbacks; every scalar/path borrow ends first.
    let g = unsafe {
        Game {
            id,
            state,
            leaves: &*leaves,
            services: &*services,
        }
    };
    g.trackdir(id)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_slope_pixel(
    id: u32,
    state: *mut State,
    leaves: *const Leaves,
    services: *const Services,
) -> bool {
    // SAFETY: Immutable tables outlive this synchronous call and nested entries.
    // Only raw owner pointers survive callbacks; every scalar/path borrow ends first.
    let g = unsafe {
        Game {
            id,
            state,
            leaves: &*leaves,
            services: &*services,
        }
    };
    g.slope_pixel(id)
}
/// Shared modern/legacy afterload road-stop frame query, retaining original indexes.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_road_stop_frame(index: u32) -> u8 {
    crate::road_data::_ROAD_STOP_STOP_FRAME[index as usize]
}

/// Narrow unchanged-reference table comparison for the corpus's tram-table gap.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_road_drive_entry(tram: u8, state: u8, frame: u8) -> u16 {
    let (x, y) = crate::road_data::entry(tram != 0, state as usize, frame as usize);
    u16::from(x) | (u16::from(y) << 8)
}
