/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Complete train control. Canonical private fields are accessed only for one scalar
//! operation; no Rust reference into an owner or world object crosses a callback.
#![allow(
    unsafe_code,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::too_many_lines,
    clippy::too_many_arguments,
    clippy::similar_names,
    clippy::many_single_char_names,
    clippy::items_after_statements,
    clippy::if_not_else,
    clippy::collapsible_if,
    clippy::verbose_bit_mask
)]
use crate::services::Services;
use crate::train_state::State;
/// Native identity and canonical scalar owner, resolved together once per entry or link.
/// Neither pointer permits a Rust borrow to survive a callback. Deletion invalidates both.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Handle {
    pub shell: *mut std::ffi::c_void,
    pub owner: *mut State,
}
impl PartialEq for Handle {
    fn eq(&self, other: &Self) -> bool {
        self.shell == other.shell
    }
}
impl Eq for Handle {}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Depot {
    pub tile: u32,
    pub length: u32,
}
impl Handle {
    pub(crate) const NONE: Self = Self {
        shell: std::ptr::null_mut(),
        owner: std::ptr::null_mut(),
    };
}
impl Default for Handle {
    fn default() -> Self {
        Self::NONE
    }
}
impl Handle {
    pub(crate) fn clear_stuck(self) {
        self.set_flags(self.get_flags() & !(1 << 8));
    }
    pub(crate) fn stuck(self) -> bool {
        self.get_flags() & (1 << 8) != 0
    }

    pub(crate) fn track(self) -> u8 {
        self.get_track() as u8
    }
    pub(crate) fn wait_inc(self) -> u16 {
        let next = (self.get_wait_counter() as u16).wrapping_add(1);
        self.set_wait_counter(u64::from(next));
        next
    }

    fn flag(self, bit: u8) -> bool {
        self.get_flags() & (1 << bit) != 0
    }

    pub(crate) fn get_flags(self) -> u64 {
        // SAFETY: A live serialized owner, scalar access ends before any callback.
        unsafe { u64::from((*self.owner).flags) }
    }
    pub(crate) fn set_flags(self, value: u64) {
        // SAFETY: Exclusive serialized scalar write, retaining no owner borrow.
        unsafe {
            (*self.owner).flags = value as u16;
        }
    }
    pub(crate) fn get_crash_anim_pos(self) -> u64 {
        // SAFETY: A live serialized owner, scalar access ends before any callback.
        unsafe { u64::from((*self.owner).crash_anim_pos) }
    }
    pub(crate) fn set_crash_anim_pos(self, value: u64) {
        // SAFETY: Exclusive serialized scalar write, retaining no owner borrow.
        unsafe {
            (*self.owner).crash_anim_pos = value as u16;
        }
    }
    pub(crate) fn get_wait_counter(self) -> u64 {
        // SAFETY: A live serialized owner, scalar access ends before any callback.
        unsafe { u64::from((*self.owner).wait_counter) }
    }
    pub(crate) fn set_wait_counter(self, value: u64) {
        // SAFETY: Exclusive serialized scalar write, retaining no owner borrow.
        unsafe {
            (*self.owner).wait_counter = value as u16;
        }
    }
    pub(crate) fn get_compatible_railtypes(self) -> u64 {
        // SAFETY: A live serialized owner, scalar access ends before any callback.
        unsafe { (*self.owner).compatible_railtypes }
    }
    pub(crate) fn set_compatible_railtypes(self, value: u64) {
        // SAFETY: Exclusive serialized scalar write, retaining no owner borrow.
        unsafe {
            (*self.owner).compatible_railtypes = value;
        }
    }
    pub(crate) fn get_railtypes(self) -> u64 {
        // SAFETY: A live serialized owner, scalar access ends before any callback.
        unsafe { (*self.owner).railtypes }
    }
    pub(crate) fn set_railtypes(self, value: u64) {
        // SAFETY: Exclusive serialized scalar write, retaining no owner borrow.
        unsafe {
            (*self.owner).railtypes = value;
        }
    }
    pub(crate) fn get_track(self) -> u64 {
        // SAFETY: A live serialized owner, scalar access ends before any callback.
        unsafe { u64::from((*self.owner).track) }
    }
    pub(crate) fn set_track(self, value: u64) {
        // SAFETY: Exclusive serialized scalar write, retaining no owner borrow.
        unsafe {
            (*self.owner).track = value as u8;
        }
    }
    pub(crate) fn get_force_proceed(self) -> u64 {
        // SAFETY: A live serialized owner, scalar access ends before any callback.
        unsafe { u64::from((*self.owner).force_proceed) }
    }
    pub(crate) fn set_force_proceed(self, value: u64) {
        // SAFETY: Exclusive serialized scalar write, retaining no owner borrow.
        unsafe {
            (*self.owner).force_proceed = value as u8;
        }
    }
    pub(crate) fn get_cached_tilt(self) -> u64 {
        // SAFETY: A live serialized owner, scalar access ends before any callback.
        unsafe { u64::from((*self.owner).cached_tilt) }
    }
    pub(crate) fn set_cached_tilt(self, value: u64) {
        // SAFETY: Exclusive serialized scalar write, retaining no owner borrow.
        unsafe {
            (*self.owner).cached_tilt = value != 0;
        }
    }
    pub(crate) fn get_user_def_data(self) -> u64 {
        // SAFETY: A live serialized owner, scalar access ends before any callback.
        unsafe { u64::from((*self.owner).user_def_data) }
    }
    pub(crate) fn set_user_def_data(self, value: u64) {
        // SAFETY: Exclusive serialized scalar write, retaining no owner borrow.
        unsafe {
            (*self.owner).user_def_data = value as u8;
        }
    }
    pub(crate) fn get_cached_curve_speed_mod(self) -> u64 {
        // SAFETY: A live serialized owner, scalar access ends before any callback.
        unsafe { u64::from((*self.owner).cached_curve_speed_mod as u16) }
    }
    pub(crate) fn set_cached_curve_speed_mod(self, value: u64) {
        // SAFETY: Exclusive serialized scalar write, retaining no owner borrow.
        unsafe {
            (*self.owner).cached_curve_speed_mod = value as i16;
        }
    }
    pub(crate) fn get_cached_max_curve_speed(self) -> u64 {
        // SAFETY: A live serialized owner, scalar access ends before any callback.
        unsafe { u64::from((*self.owner).cached_max_curve_speed) }
    }
    pub(crate) fn set_cached_max_curve_speed(self, value: u64) {
        // SAFETY: Exclusive serialized scalar write, retaining no owner borrow.
        unsafe {
            (*self.owner).cached_max_curve_speed = value as u16;
        }
    }
    pub(crate) fn set_flag(self, bit: u8, value: bool) {
        let mask = 1 << bit;
        let old = self.get_flags();
        self.set_flags(if value { old | mask } else { old & !mask });
    }
}
const INVALID: u32 = u32::MAX;
const DEPOT: u8 = 0x80;
const WORMHOLE: u8 = 0x40;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainConsistChangedRead {
    pub engine: u16,
    pub front: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainConsistChanged1Read {
    pub engine: u16,
    pub engine_part: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainConsistChanged2Read {
    pub cargo_cap: u16,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainCurveLimitRead {
    pub next: Handle,
    pub direction: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainStopLocationRead {
    pub length: u16,
    pub total_length: u16,
    pub order_destination: u16,
    pub order: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainCurrentMaxSpeedRead {
    pub tile: u32,
    pub max_track_speed: u16,
    pub speed: u16,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainCurrentMaxSpeed6Read {
    pub next: Handle,
    pub tile: u32,
    pub status: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainUpdateAccelerationRead {
    pub power: u32,
    pub weight: u32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainUpdateSpeedRead {
    pub status: u8,
    pub acceleration: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainTrackdirRead {
    pub tile: u32,
    pub direction: u8,
    pub status: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainCanLeaveRead {
    pub tile: u32,
    pub direction: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainCrossingApproachRead {
    pub status: u8,
    pub front: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainNextOffsetRead {
    pub next: Handle,
    pub length: u16,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainAfterSwapRead {
    pub tile: u32,
    pub x: i32,
    pub y: i32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainReverseSwapRead {
    pub tile: u32,
    pub x: i32,
    pub y: i32,
    pub z: i32,
    pub direction: u8,
    pub status: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainApproachingEndRead {
    pub x: i32,
    pub y: i32,
    pub length: u16,
    pub speed: u16,
    pub direction: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainLineEndsRead {
    pub tile: u32,
    pub speed: u16,
    pub breakdown: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainSpeedZRead {
    pub z: i32,
    pub max_track_speed: u16,
    pub speed: u16,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainMoveVehicleRead {
    pub tile: u32,
    pub x: i32,
    pub y: i32,
    pub direction: u8,
    pub front: u8,
    pub articulated: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainMoveVehicle20Read {
    pub speed: u16,
    pub status: u8,
    pub front: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainCollisionOneRead {
    pub first: Handle,
    pub x: i32,
    pub y: i32,
    pub z: i32,
    pub length: u16,
    pub owner: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainCollisionOne22Read {
    pub x: i32,
    pub y: i32,
    pub z: i32,
    pub length: u16,
    pub owner: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainDeleteLastRead {
    pub tile: u32,
    pub owner: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainStayDepotRead {
    pub tile: u32,
    pub power: u32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainLocoRead {
    pub speed: u16,
    pub status: u8,
    pub order: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainLoco26Read {
    pub tile: u32,
    pub order_destination: u16,
    pub order: u8,
    pub nonstop: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainTickRead {
    pub speed: u16,
    pub status: u8,
    pub running: u8,
    pub front: u8,
    pub free_wagon: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainNeedsServiceRead {
    pub order_destination: u16,
    pub order: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainNextForceRead {
    pub tile: u32,
    pub status: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrainReverseCommandRead {
    pub status: u8,
    pub breakdown: u8,
    pub front: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Leaves {
    pub read_first: extern "C" fn(Handle) -> Handle,
    pub read_next: extern "C" fn(Handle) -> Handle,
    pub read_previous: extern "C" fn(Handle) -> Handle,
    pub read_next_unit: extern "C" fn(Handle) -> Handle,
    pub read_last: extern "C" fn(Handle) -> Handle,
    pub read_tile: extern "C" fn(Handle) -> u32,
    pub read_dest: extern "C" fn(Handle) -> u32,
    pub read_order_time: extern "C" fn(Handle) -> i32,
    pub read_length: extern "C" fn(Handle) -> u16,
    pub read_total_length: extern "C" fn(Handle) -> u16,
    pub read_max_track_speed: extern "C" fn(Handle) -> u16,
    pub read_speed: extern "C" fn(Handle) -> u16,
    pub read_gv_flags: extern "C" fn(Handle) -> u16,
    pub read_refit_cap: extern "C" fn(Handle) -> u16,
    pub read_last_station: extern "C" fn(Handle) -> u16,
    pub read_direction: extern "C" fn(Handle) -> u8,
    pub read_status: extern "C" fn(Handle) -> u8,
    pub read_tick: extern "C" fn(Handle) -> u8,
    pub read_running: extern "C" fn(Handle) -> u8,
    pub read_day: extern "C" fn(Handle) -> u8,
    pub read_progress: extern "C" fn(Handle) -> u8,
    pub read_order: extern "C" fn(Handle) -> u8,
    pub read_front: extern "C" fn(Handle) -> u8,
    pub read_articulated: extern "C" fn(Handle) -> u8,
    pub read_multiheaded: extern "C" fn(Handle) -> u8,
    pub read_owner: extern "C" fn(Handle) -> u8,
    pub read_vis_effect: extern "C" fn(Handle) -> u8,
    pub read_consist_changed: extern "C" fn(Handle) -> TrainConsistChangedRead,
    pub read_consist_changed_1: extern "C" fn(Handle) -> TrainConsistChanged1Read,
    pub read_consist_changed_2: extern "C" fn(Handle) -> TrainConsistChanged2Read,
    pub read_curve_limit: extern "C" fn(Handle) -> TrainCurveLimitRead,
    pub read_stop_location: extern "C" fn(Handle) -> TrainStopLocationRead,
    pub read_current_max_speed: extern "C" fn(Handle) -> TrainCurrentMaxSpeedRead,
    pub read_current_max_speed_6: extern "C" fn(Handle) -> TrainCurrentMaxSpeed6Read,
    pub read_update_acceleration: extern "C" fn(Handle) -> TrainUpdateAccelerationRead,
    pub read_update_speed: extern "C" fn(Handle) -> TrainUpdateSpeedRead,
    pub read_trackdir: extern "C" fn(Handle) -> TrainTrackdirRead,
    pub read_can_leave: extern "C" fn(Handle) -> TrainCanLeaveRead,
    pub read_crossing_approach: extern "C" fn(Handle) -> TrainCrossingApproachRead,
    pub read_next_offset: extern "C" fn(Handle) -> TrainNextOffsetRead,
    pub read_after_swap: extern "C" fn(Handle) -> TrainAfterSwapRead,
    pub read_reverse_swap: extern "C" fn(Handle) -> TrainReverseSwapRead,
    pub read_approaching_end: extern "C" fn(Handle) -> TrainApproachingEndRead,
    pub read_line_ends: extern "C" fn(Handle) -> TrainLineEndsRead,
    pub read_speed_z: extern "C" fn(Handle) -> TrainSpeedZRead,
    pub read_move_vehicle: extern "C" fn(Handle) -> TrainMoveVehicleRead,
    pub read_move_vehicle_20: extern "C" fn(Handle) -> TrainMoveVehicle20Read,
    pub read_collision_one: extern "C" fn(Handle) -> TrainCollisionOneRead,
    pub read_collision_one_22: extern "C" fn(Handle) -> TrainCollisionOne22Read,
    pub read_delete_last: extern "C" fn(Handle) -> TrainDeleteLastRead,
    pub read_stay_depot: extern "C" fn(Handle) -> TrainStayDepotRead,
    pub read_loco: extern "C" fn(Handle) -> TrainLocoRead,
    pub read_loco_26: extern "C" fn(Handle) -> TrainLoco26Read,
    pub read_tick_state: extern "C" fn(Handle) -> TrainTickRead,
    pub read_needs_service: extern "C" fn(Handle) -> TrainNeedsServiceRead,
    pub read_next_force: extern "C" fn(Handle) -> TrainNextForceRead,
    pub read_reverse_command: extern "C" fn(Handle) -> TrainReverseCommandRead,

    pub write_tile: extern "C" fn(Handle, u32),
    pub write_dest: extern "C" fn(Handle, u32),
    pub write_x: extern "C" fn(Handle, i32),
    pub write_y: extern "C" fn(Handle, i32),
    pub write_z: extern "C" fn(Handle, i32),
    pub write_direction: extern "C" fn(Handle, u8),
    pub write_speed: extern "C" fn(Handle, u16),
    pub write_tick: extern "C" fn(Handle, u8),
    pub write_running: extern "C" fn(Handle, u8),
    pub write_day: extern "C" fn(Handle, u8),
    pub write_order_time: extern "C" fn(Handle, i32),
    pub write_progress: extern "C" fn(Handle, u8),
    pub write_subspeed: extern "C" fn(Handle, u8),
    pub write_gv_flags: extern "C" fn(Handle, u16),
    pub write_acceleration: extern "C" fn(Handle, u8),
    pub write_length: extern "C" fn(Handle, u8),
    pub write_total_length: extern "C" fn(Handle, u16),
    pub write_first_engine: extern "C" fn(Handle, u16),
    pub write_max_speed: extern "C" fn(Handle, u16),
    pub write_cargo_cap: extern "C" fn(Handle, u16),
    pub write_refit_cap: extern "C" fn(Handle, u16),
    pub write_cargo_age: extern "C" fn(Handle, u16),
    pub write_last_station: extern "C" fn(Handle, u16),
    pub write_colourmap: extern "C" fn(Handle, u64),
    pub write_status: extern "C" fn(Handle, u8),

    pub acceleration: extern "C" fn(Handle) -> u64,
    pub acc_model: extern "C" fn(Handle) -> u64,
    pub acc_type: extern "C" fn(Handle) -> u64,
    pub advance_distance: extern "C" fn(Handle) -> u64,
    pub age: extern "C" fn(Handle),
    pub all_powered: extern "C" fn(Handle, u64) -> u64,
    pub ambient_sound: extern "C" fn(Handle) -> u64,
    pub arrival_news: extern "C" fn(Handle, u16),
    pub arrival_triggers: extern "C" fn(Handle, u16),
    pub axis_diag: extern "C" fn(Handle, u8) -> u64,
    pub backoff: extern "C" fn(Handle) -> u64,
    pub base_viewport: extern "C" fn(Handle, u8),
    pub begin_loading: extern "C" fn(Handle),
    pub bridge_speed: extern "C" fn(Handle, u32) -> u64,
    pub cache_override: extern "C" fn(Handle),
    pub callback_length: extern "C" fn(Handle) -> u64,
    pub capacity: extern "C" fn(Handle) -> u64,
    pub capacity_error: extern "C" fn(Handle),
    pub cargo_age_default: extern "C" fn(Handle) -> u64,
    pub cargo_changed: extern "C" fn(Handle),
    pub chain_depot: extern "C" fn(Handle) -> u64,
    pub check_breakdown: extern "C" fn(Handle),
    pub check_next: extern "C" fn(Handle),
    pub check_orders: extern "C" fn(Handle),
    pub check_reverse: extern "C" fn(Handle) -> u64,
    pub choose_track: extern "C" fn(Handle, u32, u8, u8) -> u64,
    pub clear_reservation: extern "C" fn(Handle, u32, u8),
    pub compatible_rail_owner: extern "C" fn(Handle, u32) -> u64,
    pub consist_windows: extern "C" fn(Handle),
    pub cost_class: extern "C" fn(Handle) -> u64,
    pub cost_default: extern "C" fn(Handle) -> u64,
    pub cost_divisor: extern "C" fn(Handle) -> u64,
    pub count_chain: extern "C" fn(Handle) -> u64,
    pub crash_event: extern "C" fn(Handle, u32),
    pub crash_ground: extern "C" fn(Handle, u8) -> u64,
    pub crash_news: extern "C" fn(Handle, u32),
    pub crash_rating: extern "C" fn(Handle),
    pub crash_sound: extern "C" fn(Handle),
    pub crossing_barred: extern "C" fn(Handle, u32) -> u64,
    pub crossing_rail_axis: extern "C" fn(Handle, u32) -> u64,
    pub crossing_reserved: extern "C" fn(Handle, u32) -> u64,
    pub crossing_road_axis: extern "C" fn(Handle, u32) -> u64,
    pub crossing_sound: extern "C" fn(Handle, u32),
    pub curve_advantage: extern "C" fn(Handle) -> u64,
    pub curve_mod: extern "C" fn(Handle) -> u64,
    pub day_ticks: extern "C" fn(Handle) -> u64,
    pub decrease_value: extern "C" fn(Handle),
    pub delete_vehicle: extern "C" fn(Handle),
    pub depot_dir: extern "C" fn(Handle, u32) -> u64,
    pub depot_dirty: extern "C" fn(Handle),
    pub depot_index: extern "C" fn(Handle, u32) -> u64,
    pub depot_track: extern "C" fn(Handle, u32) -> u64,
    pub depot_window: extern "C" fn(Handle, u32),
    pub diag_axis: extern "C" fn(Handle, u8) -> u64,
    pub diag_between: extern "C" fn(Handle, u32, u32) -> u64,
    pub diag_reaches_tracks: extern "C" fn(Handle, u8) -> u64,
    pub diag_trackdir: extern "C" fn(Handle, u8) -> u64,
    pub dirty_tile: extern "C" fn(Handle, u32),
    pub dir_diag: extern "C" fn(Handle, u8) -> u64,
    pub disaster_sound: extern "C" fn(Handle) -> u64,
    pub disconnect: extern "C" fn(Handle),
    pub economy_age: extern "C" fn(Handle),
    pub engine_power: extern "C" fn(Handle) -> u64,
    pub enter_depot: extern "C" fn(Handle),
    pub enter_tile: extern "C" fn(Handle, u32, i32, i32) -> u64,
    pub find_depot: extern "C" fn(Handle, i32) -> Depot,
    pub first_track: extern "C" fn(Handle, u8) -> u64,
    pub free_reservation: extern "C" fn(Handle),
    pub grf_version: extern "C" fn(Handle) -> u64,
    pub handle_breakdown: extern "C" fn(Handle) -> u64,
    pub has_depot_res: extern "C" fn(Handle, u32) -> u64,
    pub has_reserved: extern "C" fn(Handle, u32, u8) -> u64,
    pub has_signal: extern "C" fn(Handle, u32, u8) -> u64,
    pub has_signals: extern "C" fn(Handle, u32) -> u64,
    pub has_signal_td: extern "C" fn(Handle, u32, u8) -> u64,
    pub hide_fill: extern "C" fn(Handle),
    pub inclination: extern "C" fn(Handle, u8, u8) -> u64,
    pub invalidate_grf: extern "C" fn(Handle),
    pub invalid_price: extern "C" fn(Handle) -> u64,
    pub is_bridge: extern "C" fn(Handle, u32) -> u64,
    pub is_crossing: extern "C" fn(Handle, u32) -> u64,
    pub is_depot: extern "C" fn(Handle, u32) -> u64,
    pub is_plain_rail: extern "C" fn(Handle, u32) -> u64,
    pub is_railway: extern "C" fn(Handle, u32) -> u64,
    pub is_station: extern "C" fn(Handle, u32) -> u64,
    pub is_station_any: extern "C" fn(Handle, u32) -> u64,
    pub is_tunnelbridge: extern "C" fn(Handle, u32) -> u64,
    pub large_explosion: extern "C" fn(Handle),
    pub last_speed: extern "C" fn(Handle),
    pub leave_sound: extern "C" fn(Handle),
    pub leave_station: extern "C" fn(Handle),
    pub leave_unbunch: extern "C" fn(Handle),
    pub length_callback: extern "C" fn(Handle) -> u64,
    pub length_changed: extern "C" fn(Handle),
    pub length_default: extern "C" fn(Handle) -> u64,
    pub length_error: extern "C" fn(Handle, u16),
    pub loading: extern "C" fn(Handle, u8),
    pub local_company: extern "C" fn(Handle) -> u64,
    pub lost_warn: extern "C" fn(Handle) -> u64,
    pub map_size: extern "C" fn(Handle) -> u64,
    pub max_depot_penalty: extern "C" fn(Handle) -> u64,
    pub needs_service: extern "C" fn(Handle) -> u64,
    pub no_90: extern "C" fn(Handle, u8, u8) -> u64,
    pub oneway_blocking: extern "C" fn(Handle, u32, u8) -> u64,
    pub order_depot_service: extern "C" fn(Handle, u16),
    pub order_dummy: extern "C" fn(Handle),
    pub order_free: extern "C" fn(Handle),
    pub order_max_speed: extern "C" fn(Handle) -> u64,
    pub order_stop: extern "C" fn(Handle, u16) -> u64,
    pub other_end: extern "C" fn(Handle, u32) -> u64,
    pub pay_running: extern "C" fn(Handle, i64),
    pub pbs_signal_type: extern "C" fn(Handle) -> u64,
    pub platform_ahead: extern "C" fn(Handle, u16, u32) -> u64,
    pub platform_length: extern "C" fn(Handle, u16, u32) -> u64,
    pub position: extern "C" fn(Handle),
    pub pow_wag_power: extern "C" fn(Handle) -> u64,
    pub price: extern "C" fn(Handle, u32) -> u64,
    pub process_orders: extern "C" fn(Handle) -> u64,
    pub profile: extern "C" fn(Handle, u64),
    pub property: extern "C" fn(Handle, u8, u32) -> u64,
    pub railveh_wagon: extern "C" fn(Handle) -> u64,
    pub rail_tilt: extern "C" fn(Handle) -> u64,
    pub rail_type: extern "C" fn(Handle, u32) -> u64,
    pub rail_types: extern "C" fn(Handle) -> u64,
    pub reserve_paths: extern "C" fn(Handle) -> u64,
    pub reserve_under: extern "C" fn(Handle),
    pub reset_unbunch: extern "C" fn(Handle),
    pub reverse_at_signals: extern "C" fn(Handle) -> u64,
    pub reverse_single_blocked: extern "C" fn(Handle) -> u64,
    pub reverse_windows: extern "C" fn(Handle),
    pub running_windows: extern "C" fn(Handle),
    pub service: extern "C" fn(Handle),
    pub servint: extern "C" fn(Handle) -> u64,
    pub set_depot_res: extern "C" fn(Handle, u32, u8),
    pub set_next: extern "C" fn(Handle, Handle),
    pub set_platform_res: extern "C" fn(Handle, u32, u8, u8),
    pub set_signal_state: extern "C" fn(Handle, u32, u8, u8),
    pub set_tunnel_res: extern "C" fn(Handle, u32, u8),
    pub show_effect: extern "C" fn(Handle),
    pub show_reservation: extern "C" fn(Handle) -> u64,
    pub signals_both: extern "C" fn(Handle, u32, u8, u8),
    pub signals_update: extern "C" fn(Handle, u32, u8) -> u64,
    pub signals_update_owner: extern "C" fn(Handle, u32, u8, u8) -> u64,
    pub signal_has_pbs: extern "C" fn(Handle, u32, u8) -> u64,
    pub signal_pbs: extern "C" fn(Handle, u8) -> u64,
    pub signal_type: extern "C" fn(Handle, u32, u8) -> u64,
    pub sigseg_full: extern "C" fn(Handle) -> u64,
    pub sigseg_pbs: extern "C" fn(Handle) -> u64,
    pub small_explosion: extern "C" fn(Handle, i32, i32, i32),
    pub speed_default: extern "C" fn(Handle) -> u64,
    pub start_stop_dirty: extern "C" fn(Handle),
    pub station: extern "C" fn(Handle, u32) -> u64,
    pub station_axis: extern "C" fn(Handle, u32) -> u64,
    pub station_compatible: extern "C" fn(Handle, u32, u32) -> u64,
    pub station_dest: extern "C" fn(Handle) -> u64,
    pub stopped_in_depot: extern "C" fn(Handle) -> u64,
    pub stop_location: extern "C" fn(Handle) -> u64,
    pub stuck_news: extern "C" fn(Handle),
    pub suppress_implicit: extern "C" fn(Handle),
    pub ticks_leave_depot: extern "C" fn(Handle) -> u64,
    pub tile_add_diag: extern "C" fn(Handle, u32, u8) -> u64,
    pub tile_offset_axis: extern "C" fn(Handle, u8) -> u64,
    pub tile_offset_diag: extern "C" fn(Handle, u8) -> u64,
    pub tile_owner: extern "C" fn(Handle, u32) -> u64,
    pub tile_rail_type: extern "C" fn(Handle, u32) -> u64,
    pub tile_virt: extern "C" fn(Handle, i32, i32) -> u64,
    pub trackdir_exit: extern "C" fn(Handle, u8) -> u64,
    pub trackdir_reaches: extern "C" fn(Handle, u8) -> u64,
    pub track_bits: extern "C" fn(Handle, u32) -> u64,
    pub track_crosses: extern "C" fn(Handle, u8) -> u64,
    pub track_direction: extern "C" fn(Handle, u8, u8) -> u64,
    pub track_status: extern "C" fn(Handle, u32, u8) -> u64,
    pub train_list: extern "C" fn(Handle),
    pub train_visit: extern "C" fn(Handle) -> u64,
    pub truncate_cargo: extern "C" fn(Handle, u32),
    pub try_path: extern "C" fn(Handle, u8, u8) -> u64,
    pub try_reserve: extern "C" fn(Handle, u32, u8, u8) -> u64,
    pub tunnel_dir: extern "C" fn(Handle, u32) -> u64,
    pub unreserve: extern "C" fn(Handle, u32, u8),
    pub update_delta: extern "C" fn(Handle),
    pub update_speed: extern "C" fn(Handle, u32, i32, i32) -> u64,
    pub user_default: extern "C" fn(Handle) -> u64,
    pub veh_exit_dir: extern "C" fn(Handle, u8, u8) -> u64,
    pub viewport: extern "C" fn(Handle, u8, u8),
    pub view_window: extern "C" fn(Handle),
    pub visit_type: extern "C" fn(Handle, u16) -> u64,
    pub vis_effect: extern "C" fn(Handle, u8),
    pub wagon_override: extern "C" fn(Handle) -> u64,
    pub wagon_speed_limits: extern "C" fn(Handle) -> u64,
    pub wait_oneway: extern "C" fn(Handle) -> u64,
    pub wait_pbs: extern "C" fn(Handle) -> u64,
    pub wait_twoway: extern "C" fn(Handle) -> u64,
    pub wait_unbunch: extern "C" fn(Handle) -> u64,
    pub write_crossing_bar: extern "C" fn(Handle, u32, u8),
    pub write_crossing_res: extern "C" fn(Handle, u32, u8),
    pub write_visit_type: extern "C" fn(Handle, u16),

    pub visit_tile: extern "C" fn(
        u32,
        *mut std::ffi::c_void,
        extern "C" fn(*mut std::ffi::c_void, Handle) -> u8,
    ) -> u8,
    pub visit_near: extern "C" fn(
        i32,
        i32,
        *mut std::ffi::c_void,
        extern "C" fn(*mut std::ffi::c_void, Handle) -> u8,
    ) -> u8,
}
#[derive(Clone)]
struct Game<'a> {
    leaves: &'a Leaves,
    services: &'a Services,
}
extern "C" fn visit<F: FnMut(Handle) -> bool>(context: *mut std::ffi::c_void, part: Handle) -> u8 {
    // SAFETY: Native visitors call synchronously with this live stack closure; no borrow of world state is held.
    u8::from(unsafe { (&mut *context.cast::<F>())(part) })
}
impl Game<'_> {
    fn visit_tile<F: FnMut(Handle) -> bool>(&self, tile: u32, mut visitor: F) -> bool {
        (self.leaves.visit_tile)(tile, std::ptr::from_mut(&mut visitor).cast(), visit::<F>) != 0
    }
    fn visit_near<F: FnMut(Handle) -> bool>(&self, x: i32, y: i32, mut visitor: F) -> bool {
        (self.leaves.visit_near)(x, y, std::ptr::from_mut(&mut visitor).cast(), visit::<F>) != 0
    }

    fn read_first(&self, id: Handle) -> Handle {
        (self.leaves.read_first)(id)
    }
    fn read_next(&self, id: Handle) -> Handle {
        (self.leaves.read_next)(id)
    }
    fn read_previous(&self, id: Handle) -> Handle {
        (self.leaves.read_previous)(id)
    }
    fn read_next_unit(&self, id: Handle) -> Handle {
        (self.leaves.read_next_unit)(id)
    }
    fn read_last(&self, id: Handle) -> Handle {
        (self.leaves.read_last)(id)
    }
    fn read_tile(&self, id: Handle) -> u32 {
        (self.leaves.read_tile)(id)
    }
    fn read_dest(&self, id: Handle) -> u32 {
        (self.leaves.read_dest)(id)
    }
    fn read_order_time(&self, id: Handle) -> i32 {
        (self.leaves.read_order_time)(id)
    }
    fn read_length(&self, id: Handle) -> u16 {
        (self.leaves.read_length)(id)
    }
    fn read_total_length(&self, id: Handle) -> u16 {
        (self.leaves.read_total_length)(id)
    }
    fn read_max_track_speed(&self, id: Handle) -> u16 {
        (self.leaves.read_max_track_speed)(id)
    }
    fn read_speed(&self, id: Handle) -> u16 {
        (self.leaves.read_speed)(id)
    }
    fn read_gv_flags(&self, id: Handle) -> u16 {
        (self.leaves.read_gv_flags)(id)
    }
    fn read_refit_cap(&self, id: Handle) -> u16 {
        (self.leaves.read_refit_cap)(id)
    }
    fn read_last_station(&self, id: Handle) -> u16 {
        (self.leaves.read_last_station)(id)
    }
    fn read_direction(&self, id: Handle) -> u8 {
        (self.leaves.read_direction)(id)
    }
    fn read_status(&self, id: Handle) -> u8 {
        (self.leaves.read_status)(id)
    }
    fn read_tick(&self, id: Handle) -> u8 {
        (self.leaves.read_tick)(id)
    }
    fn read_running(&self, id: Handle) -> u8 {
        (self.leaves.read_running)(id)
    }
    fn read_day(&self, id: Handle) -> u8 {
        (self.leaves.read_day)(id)
    }
    fn read_progress(&self, id: Handle) -> u8 {
        (self.leaves.read_progress)(id)
    }
    fn read_order(&self, id: Handle) -> u8 {
        (self.leaves.read_order)(id)
    }
    fn read_front(&self, id: Handle) -> u8 {
        (self.leaves.read_front)(id)
    }
    fn read_articulated(&self, id: Handle) -> u8 {
        (self.leaves.read_articulated)(id)
    }
    fn read_multiheaded(&self, id: Handle) -> u8 {
        (self.leaves.read_multiheaded)(id)
    }
    fn read_owner(&self, id: Handle) -> u8 {
        (self.leaves.read_owner)(id)
    }
    fn read_vis_effect(&self, id: Handle) -> u8 {
        (self.leaves.read_vis_effect)(id)
    }
    fn read_consist_changed(&self, id: Handle) -> TrainConsistChangedRead {
        (self.leaves.read_consist_changed)(id)
    }
    fn read_consist_changed_1(&self, id: Handle) -> TrainConsistChanged1Read {
        (self.leaves.read_consist_changed_1)(id)
    }
    fn read_consist_changed_2(&self, id: Handle) -> TrainConsistChanged2Read {
        (self.leaves.read_consist_changed_2)(id)
    }
    fn read_curve_limit(&self, id: Handle) -> TrainCurveLimitRead {
        (self.leaves.read_curve_limit)(id)
    }
    fn read_stop_location(&self, id: Handle) -> TrainStopLocationRead {
        (self.leaves.read_stop_location)(id)
    }
    fn read_current_max_speed(&self, id: Handle) -> TrainCurrentMaxSpeedRead {
        (self.leaves.read_current_max_speed)(id)
    }
    fn read_current_max_speed_6(&self, id: Handle) -> TrainCurrentMaxSpeed6Read {
        (self.leaves.read_current_max_speed_6)(id)
    }
    fn read_update_acceleration(&self, id: Handle) -> TrainUpdateAccelerationRead {
        (self.leaves.read_update_acceleration)(id)
    }
    fn read_update_speed(&self, id: Handle) -> TrainUpdateSpeedRead {
        (self.leaves.read_update_speed)(id)
    }
    fn read_trackdir(&self, id: Handle) -> TrainTrackdirRead {
        (self.leaves.read_trackdir)(id)
    }
    fn read_can_leave(&self, id: Handle) -> TrainCanLeaveRead {
        (self.leaves.read_can_leave)(id)
    }
    fn read_crossing_approach(&self, id: Handle) -> TrainCrossingApproachRead {
        (self.leaves.read_crossing_approach)(id)
    }
    fn read_next_offset(&self, id: Handle) -> TrainNextOffsetRead {
        (self.leaves.read_next_offset)(id)
    }
    fn read_after_swap(&self, id: Handle) -> TrainAfterSwapRead {
        (self.leaves.read_after_swap)(id)
    }
    fn read_reverse_swap(&self, id: Handle) -> TrainReverseSwapRead {
        (self.leaves.read_reverse_swap)(id)
    }
    fn read_approaching_end(&self, id: Handle) -> TrainApproachingEndRead {
        (self.leaves.read_approaching_end)(id)
    }
    fn read_line_ends(&self, id: Handle) -> TrainLineEndsRead {
        (self.leaves.read_line_ends)(id)
    }
    fn read_speed_z(&self, id: Handle) -> TrainSpeedZRead {
        (self.leaves.read_speed_z)(id)
    }
    fn read_move_vehicle(&self, id: Handle) -> TrainMoveVehicleRead {
        (self.leaves.read_move_vehicle)(id)
    }
    fn read_move_vehicle_20(&self, id: Handle) -> TrainMoveVehicle20Read {
        (self.leaves.read_move_vehicle_20)(id)
    }
    fn read_collision_one(&self, id: Handle) -> TrainCollisionOneRead {
        (self.leaves.read_collision_one)(id)
    }
    fn read_collision_one_22(&self, id: Handle) -> TrainCollisionOne22Read {
        (self.leaves.read_collision_one_22)(id)
    }
    fn read_delete_last(&self, id: Handle) -> TrainDeleteLastRead {
        (self.leaves.read_delete_last)(id)
    }
    fn read_stay_depot(&self, id: Handle) -> TrainStayDepotRead {
        (self.leaves.read_stay_depot)(id)
    }
    fn read_loco(&self, id: Handle) -> TrainLocoRead {
        (self.leaves.read_loco)(id)
    }
    fn read_loco_26(&self, id: Handle) -> TrainLoco26Read {
        (self.leaves.read_loco_26)(id)
    }
    fn read_tick_state(&self, id: Handle) -> TrainTickRead {
        (self.leaves.read_tick_state)(id)
    }
    fn read_needs_service(&self, id: Handle) -> TrainNeedsServiceRead {
        (self.leaves.read_needs_service)(id)
    }
    fn read_next_force(&self, id: Handle) -> TrainNextForceRead {
        (self.leaves.read_next_force)(id)
    }
    fn read_reverse_command(&self, id: Handle) -> TrainReverseCommandRead {
        (self.leaves.read_reverse_command)(id)
    }

    fn svc_acceleration(&self, id: Handle) -> u64 {
        (self.leaves.acceleration)(id)
    }
    fn svc_acc_model(&self, id: Handle) -> u64 {
        (self.leaves.acc_model)(id)
    }
    fn svc_acc_type(&self, id: Handle) -> u64 {
        (self.leaves.acc_type)(id)
    }
    fn svc_advance_distance(&self, id: Handle) -> u64 {
        (self.leaves.advance_distance)(id)
    }
    fn svc_age(&self, id: Handle) -> u64 {
        (self.leaves.age)(id);
        0
    }
    fn svc_all_powered(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.all_powered)(id, a)
    }
    fn svc_ambient_sound(&self, id: Handle) -> u64 {
        (self.leaves.ambient_sound)(id)
    }
    fn svc_arrival_news(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.arrival_news)(id, a as u16);
        0
    }
    fn svc_arrival_triggers(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.arrival_triggers)(id, a as u16);
        0
    }
    fn svc_axis_diag(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.axis_diag)(id, a as u8)
    }
    fn svc_backoff(&self, id: Handle) -> u64 {
        (self.leaves.backoff)(id)
    }
    fn svc_base_viewport(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.base_viewport)(id, a as u8);
        0
    }
    fn svc_begin_loading(&self, id: Handle) -> u64 {
        (self.leaves.begin_loading)(id);
        0
    }
    fn svc_bridge_speed(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.bridge_speed)(id, a as u32)
    }
    fn svc_cache_override(&self, id: Handle) -> u64 {
        (self.leaves.cache_override)(id);
        0
    }
    fn svc_callback_length(&self, id: Handle) -> u64 {
        (self.leaves.callback_length)(id)
    }
    fn svc_capacity(&self, id: Handle) -> u64 {
        (self.leaves.capacity)(id)
    }
    fn svc_capacity_error(&self, id: Handle) -> u64 {
        (self.leaves.capacity_error)(id);
        0
    }
    fn svc_cargo_age_default(&self, id: Handle) -> u64 {
        (self.leaves.cargo_age_default)(id)
    }
    fn svc_cargo_changed(&self, id: Handle) -> u64 {
        (self.leaves.cargo_changed)(id);
        0
    }
    fn svc_chain_depot(&self, id: Handle) -> u64 {
        (self.leaves.chain_depot)(id)
    }
    fn svc_check_breakdown(&self, id: Handle) -> u64 {
        (self.leaves.check_breakdown)(id);
        0
    }
    fn svc_check_next(&self, id: Handle) -> u64 {
        (self.leaves.check_next)(id);
        0
    }
    fn svc_check_orders(&self, id: Handle) -> u64 {
        (self.leaves.check_orders)(id);
        0
    }
    fn svc_check_reverse(&self, id: Handle) -> u64 {
        (self.leaves.check_reverse)(id)
    }
    fn svc_choose_track(&self, id: Handle, a: u64, b: u64, c: u64) -> u64 {
        (self.leaves.choose_track)(id, a as u32, b as u8, c as u8)
    }
    fn svc_clear_reservation(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.clear_reservation)(id, a as u32, b as u8);
        0
    }
    fn svc_compatible_rail_owner(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.compatible_rail_owner)(id, a as u32)
    }
    fn svc_consist_windows(&self, id: Handle) -> u64 {
        (self.leaves.consist_windows)(id);
        0
    }
    fn svc_cost_class(&self, id: Handle) -> u64 {
        (self.leaves.cost_class)(id)
    }
    fn svc_cost_default(&self, id: Handle) -> u64 {
        (self.leaves.cost_default)(id)
    }
    fn svc_cost_divisor(&self, id: Handle) -> u64 {
        (self.leaves.cost_divisor)(id)
    }
    fn svc_count_chain(&self, id: Handle) -> u64 {
        (self.leaves.count_chain)(id)
    }
    fn svc_crash_event(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.crash_event)(id, a as u32);
        0
    }
    fn svc_crash_ground(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.crash_ground)(id, a as u8)
    }
    fn svc_crash_news(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.crash_news)(id, a as u32);
        0
    }
    fn svc_crash_rating(&self, id: Handle) -> u64 {
        (self.leaves.crash_rating)(id);
        0
    }
    fn svc_crash_sound(&self, id: Handle) -> u64 {
        (self.leaves.crash_sound)(id);
        0
    }
    fn svc_crossing_barred(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.crossing_barred)(id, a as u32)
    }
    fn svc_crossing_rail_axis(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.crossing_rail_axis)(id, a as u32)
    }
    fn svc_crossing_reserved(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.crossing_reserved)(id, a as u32)
    }
    fn svc_crossing_road_axis(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.crossing_road_axis)(id, a as u32)
    }
    fn svc_crossing_sound(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.crossing_sound)(id, a as u32);
        0
    }
    fn svc_curve_advantage(&self, id: Handle) -> u64 {
        (self.leaves.curve_advantage)(id)
    }
    fn svc_curve_mod(&self, id: Handle) -> u64 {
        (self.leaves.curve_mod)(id)
    }
    fn svc_day_ticks(&self, id: Handle) -> u64 {
        (self.leaves.day_ticks)(id)
    }
    fn svc_decrease_value(&self, id: Handle) -> u64 {
        (self.leaves.decrease_value)(id);
        0
    }
    fn svc_delete_vehicle(&self, id: Handle) -> u64 {
        (self.leaves.delete_vehicle)(id);
        0
    }
    fn svc_depot_dir(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.depot_dir)(id, a as u32)
    }
    fn svc_depot_dirty(&self, id: Handle) -> u64 {
        (self.leaves.depot_dirty)(id);
        0
    }
    fn svc_depot_index(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.depot_index)(id, a as u32)
    }
    fn svc_depot_track(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.depot_track)(id, a as u32)
    }
    fn svc_depot_window(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.depot_window)(id, a as u32);
        0
    }
    fn svc_diag_axis(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.diag_axis)(id, a as u8)
    }
    fn svc_diag_between(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.diag_between)(id, a as u32, b as u32)
    }
    fn svc_diag_reaches_tracks(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.diag_reaches_tracks)(id, a as u8)
    }
    fn svc_diag_trackdir(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.diag_trackdir)(id, a as u8)
    }
    fn svc_dirty_tile(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.dirty_tile)(id, a as u32);
        0
    }
    fn svc_dir_diag(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.dir_diag)(id, a as u8)
    }
    fn svc_disaster_sound(&self, id: Handle) -> u64 {
        (self.leaves.disaster_sound)(id)
    }
    fn svc_disconnect(&self, id: Handle) -> u64 {
        (self.leaves.disconnect)(id);
        0
    }
    fn svc_economy_age(&self, id: Handle) -> u64 {
        (self.leaves.economy_age)(id);
        0
    }
    fn svc_engine_power(&self, id: Handle) -> u64 {
        (self.leaves.engine_power)(id)
    }
    fn svc_enter_depot(&self, id: Handle) -> u64 {
        (self.leaves.enter_depot)(id);
        0
    }
    fn svc_enter_tile(&self, id: Handle, a: u64, b: u64, c: u64) -> u64 {
        (self.leaves.enter_tile)(id, a as u32, b as i32, c as i32)
    }
    fn svc_find_depot(&self, id: Handle, a: u64) -> Depot {
        (self.leaves.find_depot)(id, a as i32)
    }
    fn svc_first_track(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.first_track)(id, a as u8)
    }
    fn svc_free_reservation(&self, id: Handle) -> u64 {
        (self.leaves.free_reservation)(id);
        0
    }
    fn svc_grf_version(&self, id: Handle) -> u64 {
        (self.leaves.grf_version)(id)
    }
    fn svc_handle_breakdown(&self, id: Handle) -> u64 {
        (self.leaves.handle_breakdown)(id)
    }
    fn svc_has_depot_res(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.has_depot_res)(id, a as u32)
    }
    fn svc_has_reserved(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.has_reserved)(id, a as u32, b as u8)
    }
    fn svc_has_signal(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.has_signal)(id, a as u32, b as u8)
    }
    fn svc_has_signals(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.has_signals)(id, a as u32)
    }
    fn svc_has_signal_td(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.has_signal_td)(id, a as u32, b as u8)
    }
    fn svc_hide_fill(&self, id: Handle) -> u64 {
        (self.leaves.hide_fill)(id);
        0
    }
    fn svc_inclination(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.inclination)(id, a as u8, b as u8)
    }
    fn svc_invalidate_grf(&self, id: Handle) -> u64 {
        (self.leaves.invalidate_grf)(id);
        0
    }
    fn svc_invalid_price(&self, id: Handle) -> u64 {
        (self.leaves.invalid_price)(id)
    }
    fn svc_is_bridge(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.is_bridge)(id, a as u32)
    }
    fn svc_is_crossing(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.is_crossing)(id, a as u32)
    }
    fn svc_is_depot(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.is_depot)(id, a as u32)
    }
    fn svc_is_plain_rail(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.is_plain_rail)(id, a as u32)
    }
    fn svc_is_railway(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.is_railway)(id, a as u32)
    }
    fn svc_is_station(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.is_station)(id, a as u32)
    }
    fn svc_is_station_any(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.is_station_any)(id, a as u32)
    }
    fn svc_is_tunnelbridge(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.is_tunnelbridge)(id, a as u32)
    }
    fn svc_large_explosion(&self, id: Handle) -> u64 {
        (self.leaves.large_explosion)(id);
        0
    }
    fn svc_last_speed(&self, id: Handle) -> u64 {
        (self.leaves.last_speed)(id);
        0
    }
    fn svc_leave_sound(&self, id: Handle) -> u64 {
        (self.leaves.leave_sound)(id);
        0
    }
    fn svc_leave_station(&self, id: Handle) -> u64 {
        (self.leaves.leave_station)(id);
        0
    }
    fn svc_leave_unbunch(&self, id: Handle) -> u64 {
        (self.leaves.leave_unbunch)(id);
        0
    }
    fn svc_length_callback(&self, id: Handle) -> u64 {
        (self.leaves.length_callback)(id)
    }
    fn svc_length_changed(&self, id: Handle) -> u64 {
        (self.leaves.length_changed)(id);
        0
    }
    fn svc_length_default(&self, id: Handle) -> u64 {
        (self.leaves.length_default)(id)
    }
    fn svc_length_error(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.length_error)(id, a as u16);
        0
    }
    fn svc_loading(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.loading)(id, a as u8);
        0
    }
    fn svc_local_company(&self, id: Handle) -> u64 {
        (self.leaves.local_company)(id)
    }
    fn svc_lost_warn(&self, id: Handle) -> u64 {
        (self.leaves.lost_warn)(id)
    }
    fn svc_map_size(&self, id: Handle) -> u64 {
        (self.leaves.map_size)(id)
    }
    fn svc_max_depot_penalty(&self, id: Handle) -> u64 {
        (self.leaves.max_depot_penalty)(id)
    }
    fn svc_needs_service(&self, id: Handle) -> u64 {
        (self.leaves.needs_service)(id)
    }
    fn svc_no_90(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.no_90)(id, a as u8, b as u8)
    }
    fn svc_oneway_blocking(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.oneway_blocking)(id, a as u32, b as u8)
    }
    fn svc_order_depot_service(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.order_depot_service)(id, a as u16);
        0
    }
    fn svc_order_dummy(&self, id: Handle) -> u64 {
        (self.leaves.order_dummy)(id);
        0
    }
    fn svc_order_free(&self, id: Handle) -> u64 {
        (self.leaves.order_free)(id);
        0
    }
    fn svc_order_max_speed(&self, id: Handle) -> u64 {
        (self.leaves.order_max_speed)(id)
    }
    fn svc_order_stop(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.order_stop)(id, a as u16)
    }
    fn svc_other_end(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.other_end)(id, a as u32)
    }
    fn svc_pay_running(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.pay_running)(id, a as i64);
        0
    }
    fn svc_pbs_signal_type(&self, id: Handle) -> u64 {
        (self.leaves.pbs_signal_type)(id)
    }
    fn svc_platform_ahead(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.platform_ahead)(id, a as u16, b as u32)
    }
    fn svc_platform_length(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.platform_length)(id, a as u16, b as u32)
    }
    fn svc_position(&self, id: Handle) -> u64 {
        (self.leaves.position)(id);
        0
    }
    fn svc_pow_wag_power(&self, id: Handle) -> u64 {
        (self.leaves.pow_wag_power)(id)
    }
    fn svc_price(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.price)(id, a as u32)
    }
    fn svc_process_orders(&self, id: Handle) -> u64 {
        (self.leaves.process_orders)(id)
    }
    fn svc_profile(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.profile)(id, a);
        0
    }
    fn svc_property(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.property)(id, a as u8, b as u32)
    }
    fn svc_railveh_wagon(&self, id: Handle) -> u64 {
        (self.leaves.railveh_wagon)(id)
    }
    fn svc_rail_tilt(&self, id: Handle) -> u64 {
        (self.leaves.rail_tilt)(id)
    }
    fn svc_rail_type(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.rail_type)(id, a as u32)
    }
    fn svc_rail_types(&self, id: Handle) -> u64 {
        (self.leaves.rail_types)(id)
    }
    fn svc_reserve_paths(&self, id: Handle) -> u64 {
        (self.leaves.reserve_paths)(id)
    }
    fn svc_reserve_under(&self, id: Handle) -> u64 {
        (self.leaves.reserve_under)(id);
        0
    }
    fn svc_reset_unbunch(&self, id: Handle) -> u64 {
        (self.leaves.reset_unbunch)(id);
        0
    }
    fn svc_reverse_at_signals(&self, id: Handle) -> u64 {
        (self.leaves.reverse_at_signals)(id)
    }
    fn svc_reverse_single_blocked(&self, id: Handle) -> u64 {
        (self.leaves.reverse_single_blocked)(id)
    }
    fn svc_reverse_windows(&self, id: Handle) -> u64 {
        (self.leaves.reverse_windows)(id);
        0
    }
    fn svc_running_windows(&self, id: Handle) -> u64 {
        (self.leaves.running_windows)(id);
        0
    }
    fn svc_service(&self, id: Handle) -> u64 {
        (self.leaves.service)(id);
        0
    }
    fn svc_servint(&self, id: Handle) -> u64 {
        (self.leaves.servint)(id)
    }
    fn svc_set_depot_res(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.set_depot_res)(id, a as u32, b as u8);
        0
    }
    fn svc_set_next(&self, id: Handle, a: Handle) -> u64 {
        (self.leaves.set_next)(id, a);
        0
    }
    fn svc_set_platform_res(&self, id: Handle, a: u64, b: u64, c: u64) -> u64 {
        (self.leaves.set_platform_res)(id, a as u32, b as u8, c as u8);
        0
    }
    fn svc_set_signal_state(&self, id: Handle, a: u64, b: u64, c: u64) -> u64 {
        (self.leaves.set_signal_state)(id, a as u32, b as u8, c as u8);
        0
    }
    fn svc_set_tunnel_res(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.set_tunnel_res)(id, a as u32, b as u8);
        0
    }
    fn svc_show_effect(&self, id: Handle) -> u64 {
        (self.leaves.show_effect)(id);
        0
    }
    fn svc_show_reservation(&self, id: Handle) -> u64 {
        (self.leaves.show_reservation)(id)
    }
    fn svc_signals_both(&self, id: Handle, a: u64, b: u64, c: u64) -> u64 {
        (self.leaves.signals_both)(id, a as u32, b as u8, c as u8);
        0
    }
    fn svc_signals_update(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.signals_update)(id, a as u32, b as u8)
    }
    fn svc_signals_update_owner(&self, id: Handle, a: u64, b: u64, c: u64) -> u64 {
        (self.leaves.signals_update_owner)(id, a as u32, b as u8, c as u8)
    }
    fn svc_signal_has_pbs(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.signal_has_pbs)(id, a as u32, b as u8)
    }
    fn svc_signal_pbs(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.signal_pbs)(id, a as u8)
    }
    fn svc_signal_type(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.signal_type)(id, a as u32, b as u8)
    }
    fn svc_sigseg_full(&self, id: Handle) -> u64 {
        (self.leaves.sigseg_full)(id)
    }
    fn svc_sigseg_pbs(&self, id: Handle) -> u64 {
        (self.leaves.sigseg_pbs)(id)
    }
    fn svc_small_explosion(&self, id: Handle, a: u64, b: u64, c: u64) -> u64 {
        (self.leaves.small_explosion)(id, a as i32, b as i32, c as i32);
        0
    }
    fn svc_speed_default(&self, id: Handle) -> u64 {
        (self.leaves.speed_default)(id)
    }
    fn svc_start_stop_dirty(&self, id: Handle) -> u64 {
        (self.leaves.start_stop_dirty)(id);
        0
    }
    fn svc_station(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.station)(id, a as u32)
    }
    fn svc_station_axis(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.station_axis)(id, a as u32)
    }
    fn svc_station_compatible(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.station_compatible)(id, a as u32, b as u32)
    }
    fn svc_station_dest(&self, id: Handle) -> u64 {
        (self.leaves.station_dest)(id)
    }
    fn svc_stopped_in_depot(&self, id: Handle) -> u64 {
        (self.leaves.stopped_in_depot)(id)
    }
    fn svc_stop_location(&self, id: Handle) -> u64 {
        (self.leaves.stop_location)(id)
    }
    fn svc_stuck_news(&self, id: Handle) -> u64 {
        (self.leaves.stuck_news)(id);
        0
    }
    fn svc_suppress_implicit(&self, id: Handle) -> u64 {
        (self.leaves.suppress_implicit)(id);
        0
    }
    fn svc_ticks_leave_depot(&self, id: Handle) -> u64 {
        (self.leaves.ticks_leave_depot)(id)
    }
    fn svc_tile_add_diag(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.tile_add_diag)(id, a as u32, b as u8)
    }
    fn svc_tile_offset_axis(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.tile_offset_axis)(id, a as u8)
    }
    fn svc_tile_offset_diag(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.tile_offset_diag)(id, a as u8)
    }
    fn svc_tile_owner(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.tile_owner)(id, a as u32)
    }
    fn svc_tile_rail_type(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.tile_rail_type)(id, a as u32)
    }
    fn svc_tile_virt(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.tile_virt)(id, a as i32, b as i32)
    }
    fn svc_trackdir_exit(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.trackdir_exit)(id, a as u8)
    }
    fn svc_trackdir_reaches(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.trackdir_reaches)(id, a as u8)
    }
    fn svc_track_bits(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.track_bits)(id, a as u32)
    }
    fn svc_track_crosses(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.track_crosses)(id, a as u8)
    }
    fn svc_track_direction(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.track_direction)(id, a as u8, b as u8)
    }
    fn svc_track_status(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.track_status)(id, a as u32, b as u8)
    }
    fn svc_train_list(&self, id: Handle) -> u64 {
        (self.leaves.train_list)(id);
        0
    }
    fn svc_train_visit(&self, id: Handle) -> u64 {
        (self.leaves.train_visit)(id)
    }
    fn svc_truncate_cargo(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.truncate_cargo)(id, a as u32);
        0
    }
    fn svc_try_path(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.try_path)(id, a as u8, b as u8)
    }
    fn svc_try_reserve(&self, id: Handle, a: u64, b: u64, c: u64) -> u64 {
        (self.leaves.try_reserve)(id, a as u32, b as u8, c as u8)
    }
    fn svc_tunnel_dir(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.tunnel_dir)(id, a as u32)
    }
    fn svc_unreserve(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.unreserve)(id, a as u32, b as u8);
        0
    }
    fn svc_update_delta(&self, id: Handle) -> u64 {
        (self.leaves.update_delta)(id);
        0
    }
    fn svc_update_speed(&self, id: Handle, a: u64, b: u64, c: u64) -> u64 {
        (self.leaves.update_speed)(id, a as u32, b as i32, c as i32)
    }
    fn svc_user_default(&self, id: Handle) -> u64 {
        (self.leaves.user_default)(id)
    }
    fn svc_veh_exit_dir(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.veh_exit_dir)(id, a as u8, b as u8)
    }
    fn svc_viewport(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.viewport)(id, a as u8, b as u8);
        0
    }
    fn svc_view_window(&self, id: Handle) -> u64 {
        (self.leaves.view_window)(id);
        0
    }
    fn svc_visit_type(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.visit_type)(id, a as u16)
    }
    fn svc_vis_effect(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.vis_effect)(id, a as u8);
        0
    }
    fn svc_wagon_override(&self, id: Handle) -> u64 {
        (self.leaves.wagon_override)(id)
    }
    fn svc_wagon_speed_limits(&self, id: Handle) -> u64 {
        (self.leaves.wagon_speed_limits)(id)
    }
    fn svc_wait_oneway(&self, id: Handle) -> u64 {
        (self.leaves.wait_oneway)(id)
    }
    fn svc_wait_pbs(&self, id: Handle) -> u64 {
        (self.leaves.wait_pbs)(id)
    }
    fn svc_wait_twoway(&self, id: Handle) -> u64 {
        (self.leaves.wait_twoway)(id)
    }
    fn svc_wait_unbunch(&self, id: Handle) -> u64 {
        (self.leaves.wait_unbunch)(id)
    }
    fn svc_write_crossing_bar(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.write_crossing_bar)(id, a as u32, b as u8);
        0
    }
    fn svc_write_crossing_res(&self, id: Handle, a: u64, b: u64) -> u64 {
        (self.leaves.write_crossing_res)(id, a as u32, b as u8);
        0
    }
    fn svc_write_visit_type(&self, id: Handle, a: u64) -> u64 {
        (self.leaves.write_visit_type)(id, a as u16);
        0
    }
}
const PROP_TRAIN_USER_DATA: u32 = 37;
const PROP_TRAIN_SPEED: u32 = 9;
const PROP_TRAIN_CARGO_AGE_PERIOD: u32 = 43;
const PROP_TRAIN_SHORTEN_FACTOR: u32 = 33;
const PROP_TRAIN_RUNNING_COST_FACTOR: u32 = 13;
impl Game<'_> {
    fn property(&self, id: Handle, prop: u32, default: u64) -> u64 {
        self.svc_property(id, u64::from(prop), default)
    }
    fn consist_changed(&self, id: Handle, allowed: u8) {
        let mut max_speed = u16::MAX;
        let head = self.read_consist_changed(id);
        let mut first_engine = if head.front != 0 {
            head.engine
        } else {
            u16::MAX
        };
        (self.leaves.write_total_length)(id, 0_u16);
        id.set_compatible_railtypes(0);
        let mut tilt = true;
        let mut min_curve_mod = i16::MAX;
        let mut u = id;
        while u != Handle::NONE {
            let part = self.read_consist_changed_1(u);
            (self.leaves.write_first_engine)(
                u,
                (u64::from(if u == id { u16::MAX } else { first_engine })) as u16,
            );
            u.set_railtypes(self.svc_rail_types(u));
            if part.engine_part != 0 {
                first_engine = part.engine;
            }
            u.set_user_def_data(self.svc_user_default(u));
            self.svc_invalidate_grf(id);
            self.svc_invalidate_grf(u);
            u = self.read_next(u);
        }
        u = id;
        while u != Handle::NONE {
            u.set_user_def_data(self.property(u, PROP_TRAIN_USER_DATA, u.get_user_def_data()));
            self.svc_invalidate_grf(id);
            self.svc_invalidate_grf(u);
            u = self.read_next(u);
        }
        u = id;
        while u != Handle::NONE {
            if self.svc_rail_tilt(u) == 0 {
                tilt = false;
            }
            min_curve_mod = min_curve_mod.min(self.svc_curve_mod(u) as i16);
            self.svc_cache_override(u);
            (self.leaves.write_colourmap)(u, 0);
            self.svc_vis_effect(u, 1);
            let powered = self.svc_pow_wag_power(id) != 0
                && self.svc_railveh_wagon(u) != 0
                && self.svc_wagon_override(u) != 0
                && self.read_vis_effect(u) & 0x80 == 0;
            u.set_flag(3, powered);
            if self.read_articulated(u) == 0 {
                if self.svc_engine_power(u) > 0 {
                    id.set_compatible_railtypes(
                        id.get_compatible_railtypes() | self.svc_all_powered(u, u.get_railtypes()),
                    );
                }
                if u.flag(6) {
                    u.set_railtypes(u.get_railtypes() | 1);
                    u.set_compatible_railtypes(u.get_compatible_railtypes() | 1);
                }
                if (self.svc_railveh_wagon(u) == 0 || self.svc_wagon_speed_limits(u) != 0)
                    && self.svc_wagon_override(u) == 0
                {
                    let speed =
                        self.property(u, PROP_TRAIN_SPEED, self.svc_speed_default(u)) as u16;
                    if speed != 0 {
                        max_speed = max_speed.min(speed);
                    }
                }
            }
            let cap = self.svc_capacity(u) as u16;
            let part = self.read_consist_changed_2(u);
            if allowed & 2 != 0 {
                if part.cargo_cap > cap {
                    self.svc_truncate_cargo(u, u64::from(cap));
                }
                (self.leaves.write_refit_cap)(
                    u,
                    (u64::from(cap.min(self.read_refit_cap(u)))) as u16,
                );
                (self.leaves.write_cargo_cap)(u, (u64::from(cap)) as u16);
            } else if cap != part.cargo_cap {
                self.svc_capacity_error(u);
            }
            (self.leaves.write_cargo_age)(
                u,
                (self.property(
                    u,
                    PROP_TRAIN_CARGO_AGE_PERIOD,
                    self.svc_cargo_age_default(u),
                )) as u16,
            );
            let mut length = u16::MAX;
            if self.svc_grf_version(u) >= 8 {
                length = self.property(u, PROP_TRAIN_SHORTEN_FACTOR, u64::from(u16::MAX)) as u16;
                if length != u16::MAX && length >= 8 {
                    self.svc_length_error(u, u64::from(length));
                }
            } else if self.svc_length_callback(u) != 0 {
                length = self.svc_callback_length(u) as u16;
            }
            if length == u16::MAX {
                length = self.svc_length_default(u) as u16;
            }
            length = 8 - length.min(7);
            if allowed & 1 != 0 {
                (self.leaves.write_length)(u, (u64::from(length)) as u8);
            } else if length != self.read_length(u) {
                self.svc_length_changed(u);
            }
            (self.leaves.write_total_length)(
                id,
                (u64::from(self.read_total_length(id).wrapping_add(self.read_length(u)))) as u16,
            );
            self.svc_invalidate_grf(id);
            self.svc_invalidate_grf(u);
            u = self.read_next(u);
        }
        (self.leaves.write_max_speed)(id, (u64::from(max_speed)) as u16);
        id.set_cached_tilt(u64::from(tilt));
        id.set_cached_curve_speed_mod(u64::from(min_curve_mod as u16));
        id.set_cached_max_curve_speed(u64::from(self.curve_limit(id)));
        self.svc_cargo_changed(id);
        if self.read_front(id) != 0 {
            self.update_acceleration(id);
            self.svc_consist_windows(id);
        }
    }
    fn curve_limit(&self, id: Handle) -> u16 {
        let mut max_speed = i32::from(u16::MAX);
        if self.svc_acc_model(id) == 0 {
            return max_speed as u16;
        }
        let mut curves = [0_i32; 2];
        let (mut numcurve, mut sum, mut pos, mut lastpos) = (0_i32, 0_i32, 0_i32, -1_i32);
        let mut u = id;
        loop {
            let part = self.read_curve_limit(u);
            if part.next == Handle::NONE {
                break;
            }
            let diff = part.direction.wrapping_sub(self.read_direction(part.next)) & 7;
            if diff != 0 {
                if diff == 7 {
                    curves[0] = curves[0].wrapping_add(1);
                }
                if diff == 1 {
                    curves[1] = curves[1].wrapping_add(1);
                }
                if diff == 7 || diff == 1 {
                    if lastpos != -1 {
                        numcurve = numcurve.wrapping_add(1);
                        sum = sum.wrapping_add(pos.wrapping_sub(lastpos));
                        if pos.wrapping_sub(lastpos) <= 8 && max_speed > 88 {
                            max_speed = 88;
                        }
                    }
                    lastpos = pos;
                }
                if diff == 6 || diff == 2 {
                    max_speed = 61;
                }
            }
            u = part.next;
            pos = pos.wrapping_add(i32::from(self.read_length(u)));
        }
        if numcurve > 0 && max_speed > 88 {
            if curves == [1, 1] {
                max_speed = i32::from(u16::MAX);
            } else {
                sum = sum.wrapping_add(7) / 8;
                sum /= numcurve;
                let d = 13 - sum.clamp(1, 12);
                max_speed = 232 - d * d;
            }
        }
        if max_speed != i32::from(u16::MAX) {
            max_speed = max_speed
                .wrapping_add((max_speed / 2).wrapping_mul(self.svc_curve_advantage(id) as i32));
            if id.get_cached_tilt() != 0 {
                max_speed = max_speed.wrapping_add(max_speed / 5);
            }
            max_speed = max_speed.wrapping_add(
                max_speed.wrapping_mul(i32::from(id.get_cached_curve_speed_mod() as i16)) / 256,
            );
            max_speed = max_speed.clamp(2, i32::from(u16::MAX));
        }
        max_speed as u16
    }
    fn stop_location(&self, id: Handle, station: u16, tile: u32) -> (i32, i32, i32) {
        let ahead = (self.svc_platform_ahead(id, u64::from(station), u64::from(tile)) as i32)
            .wrapping_mul(16);
        let length = (self.svc_platform_length(id, u64::from(station), u64::from(tile)) as i32)
            .wrapping_mul(16);
        let v = self.read_stop_location(id);
        let mut loc = 1;
        if i32::from(v.total_length) >= length {
            loc = 2;
        } else if v.order == 1 && v.order_destination == station {
            loc = self.svc_stop_location(id);
        }
        let stop = match loc {
            0 => i32::from(v.total_length),
            1 => length.wrapping_sub(length.wrapping_sub(i32::from(v.total_length)) / 2),
            _ => length,
        };
        (
            stop.wrapping_sub((i32::from(v.length) + 1) / 2),
            ahead,
            length,
        )
    }
    fn current_max_speed(&self, id: Handle) -> i32 {
        let v = self.read_current_max_speed(id);
        let model = self.svc_acc_model(id);
        let mut max_speed = if model == 0 {
            i32::from(v.max_track_speed)
        } else {
            id.get_cached_max_curve_speed() as i32
        };
        if model == 1 && self.svc_is_station(id, u64::from(v.tile)) != 0 {
            let station = self.svc_station(id, u64::from(v.tile)) as u16;
            if self.svc_order_stop(id, u64::from(station)) != 0 {
                let (stop, ahead, length) = self.stop_location(id, station, v.tile);
                let distance = ahead / 16 - length.wrapping_sub(stop) / 16;
                if distance > 0 {
                    let mut limit = 120;
                    let delta = i32::from(v.speed) / distance.wrapping_add(1);
                    if max_speed > i32::from(v.speed) - delta {
                        limit = i32::from(v.speed) - delta / 10;
                    }
                    limit = limit.max(25_i32.wrapping_mul(distance));
                    max_speed = max_speed.min(limit);
                }
            }
        }
        let mut u = id;
        while u != Handle::NONE {
            let part = self.read_current_max_speed_6(u);
            if model == 1 && u.get_track() == u64::from(DEPOT) {
                max_speed = max_speed.min(61);
                break;
            }
            if u.get_track() == u64::from(WORMHOLE) && part.status & 1 == 0 {
                max_speed = max_speed.min(self.svc_bridge_speed(u, u64::from(part.tile)) as i32);
            }
            u = part.next;
        }
        max_speed = max_speed.min(self.svc_order_max_speed(id) as i32);
        max_speed.min(i32::from(self.read_max_track_speed(id)))
    }
    fn update_acceleration(&self, id: Handle) {
        let v = self.read_update_acceleration(id);
        // Clamp(int, int, int): the uint product converts to int first.
        let accel = ((v.power / v.weight).wrapping_mul(4) as i32).clamp(1, 255);
        (self.leaves.write_acceleration)(id, accel as u8);
    }
    fn update_speed(&self, id: Handle) -> i32 {
        let v = self.read_update_speed(id);
        let braking = v.status & 2 != 0 || id.flag(0) || id.flag(8);
        let max = self.current_max_speed(id);
        if self.svc_acc_model(id) == 0 {
            self.svc_update_speed(
                id,
                i64::from(i32::from(v.acceleration) * if braking { -4 } else { 2 }) as u64,
                0,
                max as u64,
            ) as i32
        } else {
            self.svc_update_speed(
                id,
                self.svc_acceleration(id),
                u64::from(!braking) * 2,
                max as u64,
            ) as i32
        }
    }
    fn mark_dirty(&self, id: Handle) {
        let mut u = id;
        loop {
            (self.leaves.write_colourmap)(u, 0);
            self.svc_viewport(u, 1, 0);
            u = self.read_next(u);
            if u == Handle::NONE {
                break;
            }
        }
        self.svc_cargo_changed(id);
        self.update_acceleration(id);
    }
    fn trackdir(&self, id: Handle) -> u8 {
        let v = self.read_trackdir(id);
        if v.status & 128 != 0 {
            return u8::MAX;
        }
        let track = id.get_track() as u8;
        if track == DEPOT {
            return self.svc_diag_trackdir(id, self.svc_depot_dir(id, u64::from(v.tile))) as u8;
        }
        if track == WORMHOLE {
            return self.svc_diag_trackdir(id, self.svc_dir_diag(id, u64::from(v.direction))) as u8;
        }
        self.svc_track_direction(
            id,
            self.svc_first_track(id, u64::from(track)),
            u64::from(v.direction),
        ) as u8
    }
    fn running_cost(&self, id: Handle) -> i64 {
        let mut cost = 0_i64;
        let mut u = id;
        loop {
            if self.svc_cost_class(u) != self.svc_invalid_price(u) {
                let mut factor =
                    self.property(u, PROP_TRAIN_RUNNING_COST_FACTOR, self.svc_cost_default(u))
                        as u32;
                if factor != 0 {
                    if self.read_multiheaded(u) != 0 {
                        factor /= 2;
                    }
                    cost = cost.saturating_add(self.svc_price(u, u64::from(factor)) as i64);
                }
            }
            u = self.read_next_unit(u);
            if u == Handle::NONE {
                break;
            }
        }
        cost
    }
}
const BREAKDOWN_SPEEDS: [u16; 16] = [
    225, 210, 195, 180, 165, 150, 135, 120, 105, 90, 75, 60, 45, 30, 15, 15,
];
impl Game<'_> {
    fn count(&self, index: u64) {
        self.svc_profile(Handle::NONE, index);
    }

    fn write_status(&self, id: Handle, mask: u8, value: bool) {
        let old = self.read_status(id);
        (self.leaves.write_status)(
            id,
            (u64::from(if value { old | mask } else { old & !mask })) as u8,
        );
    }

    fn tile_diag(&self, tile: u32, dir: u8) -> u32 {
        self.svc_tile_add_diag(Handle::NONE, u64::from(tile), u64::from(dir)) as u32
    }
    fn exitdir(&self, id: Handle) -> u8 {
        self.svc_veh_exit_dir(
            id,
            u64::from(self.read_direction(id)),
            u64::from(id.track()),
        ) as u8
    }
    fn compatible(&self, id: Handle, tile: u32) -> bool {
        self.svc_compatible_rail_owner(id, u64::from(tile)) != 0
            && (self.read_front(id) == 0
                || id.get_compatible_railtypes()
                    & (1 << self.svc_rail_type(Handle::NONE, u64::from(tile)))
                    != 0)
    }
    fn can_leave(&self, id: Handle) -> bool {
        let v = self.read_can_leave(id);
        let track = id.track();
        if track == WORMHOLE || track == DEPOT {
            return false;
        }
        if self.svc_is_tunnelbridge(Handle::NONE, u64::from(v.tile)) != 0
            && self.svc_tunnel_dir(Handle::NONE, u64::from(v.tile)) as u8 * 2 + 1 == v.direction
        {
            return false;
        }
        if self.svc_is_depot(Handle::NONE, u64::from(v.tile)) != 0
            && (self.svc_depot_dir(Handle::NONE, u64::from(v.tile)) as u8 ^ 2) * 2 + 1
                == v.direction
        {
            return false;
        }
        true
    }
    fn approaching_crossing(&self, id: Handle) -> u32 {
        if !self.can_leave(id) {
            return INVALID;
        }
        let base = self.read_tile(id);
        let dir = self.exitdir(id);
        let tile = base.wrapping_add(self.svc_tile_offset_diag(id, u64::from(dir)) as u32);
        if self.svc_is_crossing(Handle::NONE, u64::from(tile)) == 0
            || self.svc_diag_axis(id, u64::from(dir))
                == self.svc_crossing_road_axis(Handle::NONE, u64::from(tile))
            || !self.compatible(id, tile)
        {
            return INVALID;
        }
        tile
    }
    fn train_on_tile(&self, tile: u32) -> bool {
        self.visit_tile(tile, |_| true)
    }
    fn crossing_approach(&self, tile: u32) -> bool {
        let dir = self.svc_axis_diag(
            Handle::NONE,
            self.svc_crossing_rail_axis(Handle::NONE, u64::from(tile)),
        ) as u8;
        for d in [dir, dir ^ 2] {
            let from =
                tile.wrapping_add(self.svc_tile_offset_diag(Handle::NONE, u64::from(d)) as u32);
            if self.visit_tile(from, |id| {
                let v = self.read_crossing_approach(id);
                if v.status & 128 == 0 && v.front != 0 && self.approaching_crossing(id) == tile {
                    return true;
                }

                false
            }) {
                return true;
            }
        }
        false
    }
    fn check_crossing(&self, tile: u32) -> bool {
        self.svc_crossing_reserved(Handle::NONE, u64::from(tile)) != 0
            || self.train_on_tile(tile)
            || self.crossing_approach(tile)
    }
    fn update_crossing_tile(&self, tile: u32, sound: bool, force: bool) {
        let barred = force || self.check_crossing(tile);
        if barred != (self.svc_crossing_barred(Handle::NONE, u64::from(tile)) != 0) {
            self.count(if barred { 13 } else { 14 });
            if barred && sound && self.svc_ambient_sound(Handle::NONE) != 0 {
                self.svc_crossing_sound(Handle::NONE, u64::from(tile));
            }
            self.svc_write_crossing_bar(Handle::NONE, u64::from(tile), u64::from(barred));
            self.svc_dirty_tile(Handle::NONE, u64::from(tile));
        }
    }
    fn update_crossing(&self, tile: u32, sound: bool, force: bool) {
        if self.svc_is_crossing(Handle::NONE, u64::from(tile)) == 0 {
            return;
        }
        let mut state = force;
        let axis = self.svc_crossing_road_axis(Handle::NONE, u64::from(tile));
        let dir = self.svc_axis_diag(Handle::NONE, axis) as u8;
        for d in [dir, dir ^ 2] {
            let mut t = tile;
            while !state
                && u64::from(t) < self.svc_map_size(Handle::NONE)
                && self.svc_is_crossing(Handle::NONE, u64::from(t)) != 0
                && self.svc_crossing_road_axis(Handle::NONE, u64::from(t)) == axis
            {
                state |= self.check_crossing(t);
                t = self.tile_diag(t, d);
            }
        }
        self.update_crossing_tile(tile, sound, state);
        for d in [dir, dir ^ 2] {
            let mut t = self.tile_diag(tile, d);
            while u64::from(t) < self.svc_map_size(Handle::NONE)
                && self.svc_is_crossing(Handle::NONE, u64::from(t)) != 0
                && self.svc_crossing_road_axis(Handle::NONE, u64::from(t)) == axis
            {
                self.update_crossing_tile(t, sound, state);
                t = self.tile_diag(t, d);
            }
        }
    }
    fn adjacent_crossing_dirty(&self, tile: u32, axis: u64) {
        let dir = self.svc_axis_diag(Handle::NONE, axis) as u8;
        for d in [dir, dir ^ 2] {
            let t = self.tile_diag(tile, d);
            if u64::from(t) < self.svc_map_size(Handle::NONE)
                && self.svc_is_crossing(Handle::NONE, u64::from(t)) != 0
                && self.svc_crossing_road_axis(Handle::NONE, u64::from(t)) == axis
            {
                self.svc_dirty_tile(Handle::NONE, u64::from(t));
            }
        }
    }
    fn crossing_removed(&self, tile: u32, axis: u64) {
        let dir = self.svc_axis_diag(Handle::NONE, axis) as u8;
        for d in [dir, dir ^ 2] {
            let diff = self.svc_tile_offset_diag(Handle::NONE, u64::from(d)) as u32;
            let mut occupied = false;
            let mut t = tile.wrapping_add(diff);
            while u64::from(t) < self.svc_map_size(Handle::NONE)
                && self.svc_is_crossing(Handle::NONE, u64::from(t)) != 0
                && self.svc_crossing_road_axis(Handle::NONE, u64::from(t)) == axis
            {
                occupied |= self.check_crossing(t);
                t = t.wrapping_add(diff);
            }
            if occupied {
                t = tile.wrapping_add(diff);
                if u64::from(t) < self.svc_map_size(Handle::NONE)
                    && self.svc_is_crossing(Handle::NONE, u64::from(t)) != 0
                    && self.svc_crossing_road_axis(Handle::NONE, u64::from(t)) == axis
                {
                    self.svc_dirty_tile(Handle::NONE, u64::from(t));
                }
            } else {
                t = tile.wrapping_add(diff);
                while u64::from(t) < self.svc_map_size(Handle::NONE)
                    && self.svc_is_crossing(Handle::NONE, u64::from(t)) != 0
                    && self.svc_crossing_road_axis(Handle::NONE, u64::from(t)) == axis
                {
                    if self.svc_crossing_barred(Handle::NONE, u64::from(t)) != 0 {
                        self.svc_write_crossing_bar(Handle::NONE, u64::from(t), 0);
                        self.svc_dirty_tile(Handle::NONE, u64::from(t));
                    } else {
                        self.svc_dirty_tile(Handle::NONE, u64::from(t));
                        break;
                    }
                    t = t.wrapping_add(diff);
                }
            }
        }
    }
    fn bar_crossing(&self, tile: u32) {
        if self.svc_crossing_barred(Handle::NONE, u64::from(tile)) == 0 {
            self.svc_write_crossing_res(Handle::NONE, u64::from(tile), 1);
            self.update_crossing(tile, true, false);
        }
    }
    fn mark_stuck(&self, id: Handle) {
        if !id.flag(8) {
            id.set_flag(8, true);
            id.set_wait_counter(0);
            (self.leaves.write_speed)(id, 0_u16);
            (self.leaves.write_subspeed)(id, 0_u8);
            self.svc_last_speed(id);
            self.svc_start_stop_dirty(id);
        }
    }
    fn whole_in_depot(&self, id: Handle) -> bool {
        let tile = self.read_tile(id);
        let mut u = id;
        while u != Handle::NONE {
            if u.track() != DEPOT || self.read_tile(u) != tile {
                return false;
            }
            u = self.read_next(u);
        }
        true
    }
    fn next_offset(&self, id: Handle) -> i32 {
        let v = self.read_next_offset(id);
        i32::from(v.length) / 2
            + if v.next == Handle::NONE {
                0
            } else {
                (i32::from(self.read_length(v.next)) + 1) / 2
            }
    }
    fn after_swap(&self, id: Handle) {
        if id.track() == WORMHOLE {
            self.count(3);
        }
        if id.track() != DEPOT {
            (self.leaves.write_direction)(id, (u64::from(self.read_direction(id) ^ 4)) as u8);
        }
        let v = self.read_after_swap(id);
        if id.track() != WORMHOLE {
            self.svc_enter_tile(id, u64::from(v.tile), v.x as u64, v.y as u64);
        } else {
            let vt = self.svc_tile_virt(id, v.x as u64, v.y as u64) as u32;
            if self.svc_is_tunnelbridge(Handle::NONE, u64::from(vt)) != 0 {
                self.svc_enter_tile(id, u64::from(vt), v.x as u64, v.y as u64);
                if id.track() != WORMHOLE
                    && self.svc_is_bridge(Handle::NONE, u64::from(self.read_tile(id))) != 0
                {
                    self.svc_position(id);
                    self.svc_inclination(id, 1, 1);
                    return;
                }
            }
        }
        self.svc_position(id);
        self.svc_viewport(id, 1, 1);
    }
    fn swap_ground_flags(&self, a: Handle, b: Handle) {
        let fa = self.read_gv_flags(a);
        let fb = self.read_gv_flags(b);
        (self.leaves.write_gv_flags)(a, (u64::from(fa & !3)) as u16);
        (self.leaves.write_gv_flags)(b, (u64::from(self.read_gv_flags(b) & !3)) as u16);
        if fa & 1 != 0 {
            (self.leaves.write_gv_flags)(b, (u64::from(self.read_gv_flags(b) | 2)) as u16);
        } else if fa & 2 != 0 {
            (self.leaves.write_gv_flags)(b, (u64::from(self.read_gv_flags(b) | 1)) as u16);
        }
        if fb & 1 != 0 {
            (self.leaves.write_gv_flags)(a, (u64::from(self.read_gv_flags(a) | 2)) as u16);
        } else if fb & 2 != 0 {
            (self.leaves.write_gv_flags)(a, (u64::from(self.read_gv_flags(a) | 1)) as u16);
        }
    }
    fn reverse_swap(&self, id: Handle, l: i32, r: i32) {
        let (mut a, mut b) = (id, id);
        for _ in 0..l {
            a = self.read_next(a);
        }
        for _ in 0..r {
            b = self.read_next(b);
        }
        if a != b {
            let va = self.read_reverse_swap(a);
            let vb = self.read_reverse_swap(b);
            self.write_status(b, 1, va.status & 1 != 0);
            self.write_status(a, 1, vb.status & 1 != 0);
            let ta = a.track();
            a.set_track(u64::from(b.track()));
            b.set_track(u64::from(ta));
            (self.leaves.write_direction)(a, (u64::from(vb.direction)) as u8);
            (self.leaves.write_direction)(b, (u64::from(va.direction)) as u8);
            (self.leaves.write_x)(a, (vb.x as u64) as i32);
            (self.leaves.write_x)(b, (va.x as u64) as i32);
            (self.leaves.write_y)(a, (vb.y as u64) as i32);
            (self.leaves.write_y)(b, (va.y as u64) as i32);
            (self.leaves.write_tile)(a, (u64::from(vb.tile)) as u32);
            (self.leaves.write_tile)(b, (u64::from(va.tile)) as u32);
            (self.leaves.write_z)(a, (vb.z as u64) as i32);
            (self.leaves.write_z)(b, (va.z as u64) as i32);
            self.swap_ground_flags(a, b);
            self.after_swap(a);
            self.after_swap(b);
        } else {
            self.swap_ground_flags(a, a);
            self.after_swap(a);
        }
    }
    fn advance_before_swap(&self, id: Handle) {
        let (mut base, mut first, mut last) = (id, id, self.read_last(id));
        let mut length = self.svc_count_chain(id) as u32;
        while length > 2 {
            last = self.read_previous(last);
            first = self.read_next(first);
            let diff = self.next_offset(base) - self.next_offset(last);
            for _ in 0..diff {
                self.count(4);
                self.controller(first, self.read_next(last), true);
            }
            base = first;
            length -= 2;
        }
    }
    fn advance_after_swap(&self, id: Handle) {
        let mut dep = id;
        while self.read_next(dep) != Handle::NONE
            && (dep.track() == DEPOT || self.read_next(dep).track() != DEPOT)
        {
            dep = self.read_next(dep);
        }
        let leave = self.read_next(dep);
        if leave != Handle::NONE {
            let d = self.svc_ticks_leave_depot(dep) as i32;
            if d <= 0 {
                self.write_status(leave, 1, false);
                leave.set_track(
                    1 << self.svc_depot_track(Handle::NONE, u64::from(self.read_tile(leave))),
                );
                for _ in d..=0 {
                    self.controller(leave, Handle::NONE, true);
                }
            }
        } else {
            dep = Handle::NONE;
        }
        let (mut base, mut first, mut last) = (id, id, self.read_last(id));
        let mut length = self.svc_count_chain(id) as u32;
        let mut nomove = dep == Handle::NONE;
        while length > 2 {
            if base == dep {
                break;
            }
            if last == dep {
                nomove = true;
            }
            last = self.read_previous(last);
            first = self.read_next(first);
            let diff = self.next_offset(last) - self.next_offset(base);
            for _ in 0..diff {
                self.count(5);
                self.controller(
                    first,
                    if nomove {
                        self.read_next(last)
                    } else {
                        Handle::NONE
                    },
                    true,
                );
            }
            base = first;
            length -= 2;
        }
    }
    fn reverse(&self, id: Handle) {
        self.count(2);
        if self.svc_is_depot(Handle::NONE, u64::from(self.read_tile(id))) != 0 {
            if self.whole_in_depot(id) {
                return;
            }
            self.svc_depot_dirty(id);
        }
        if !id.flag(8) {
            self.svc_free_reservation(id);
        }
        let crossing = self.approaching_crossing(id);
        let mut r = self.svc_count_chain(id) as i32 - 1;
        self.advance_before_swap(id);
        let mut l = 0;
        loop {
            self.reverse_swap(id, l, r);
            l += 1;
            r -= 1;
            if l > r {
                break;
            }
        }
        self.advance_after_swap(id);
        if self.svc_is_depot(Handle::NONE, u64::from(self.read_tile(id))) != 0 {
            self.svc_depot_dirty(id);
        }
        id.set_flag(7, !id.flag(7));
        id.set_flag(0, false);
        self.consist_changed(id, 0);
        let mut u = id;
        while u != Handle::NONE {
            self.svc_viewport(u, 0, 0);
            u = self.read_next(u);
        }
        if crossing != INVALID {
            self.update_crossing(crossing, true, false);
        }
        let crossing = self.approaching_crossing(id);
        if crossing != INVALID {
            self.bar_crossing(crossing);
        }
        if id.track() == DEPOT {
            if id.flag(8) {
                self.svc_start_stop_dirty(id);
            }
            id.set_flag(8, false);
            return;
        }
        let tile = self.read_tile(id);
        let mut dir = self.exitdir(id);
        if self.svc_is_depot(Handle::NONE, u64::from(tile)) != 0
            || self.svc_is_tunnelbridge(Handle::NONE, u64::from(tile)) != 0
        {
            dir = u8::MAX;
        }
        if self.svc_signals_update(id, u64::from(tile), u64::from(dir)) == self.svc_sigseg_pbs(id)
            || self.svc_reserve_paths(id) != 0
        {
            let td = self.trackdir(id);
            let mut okay = !(self.svc_is_railway(Handle::NONE, u64::from(tile)) != 0
                && self.svc_has_signal_td(id, u64::from(tile), u64::from(td)) != 0
                && self.svc_signal_pbs(
                    id,
                    self.svc_signal_type(
                        id,
                        u64::from(tile),
                        u64::from(id.track().trailing_zeros()),
                    ),
                ) == 0);
            if self.svc_is_depot(Handle::NONE, u64::from(tile)) != 0
                && self.svc_trackdir_exit(id, u64::from(td))
                    == self.svc_depot_dir(Handle::NONE, u64::from(tile))
            {
                okay = false;
            }
            if self.svc_is_station(Handle::NONE, u64::from(tile)) != 0 {
                self.svc_set_platform_res(
                    id,
                    u64::from(tile),
                    self.svc_trackdir_exit(id, u64::from(td)),
                    1,
                );
            }
            if self.svc_try_path(id, 0, u64::from(okay)) != 0 {
                self.svc_check_next(id);
            } else if self.read_order(id) != 3 {
                self.mark_stuck(id);
            }
        } else if id.flag(8) {
            id.set_flag(8, false);
            id.set_wait_counter(0);
        }
    }
    fn approaching_end(&self, id: Handle, signal: bool, reverse: bool) -> bool {
        let v = self.read_approaching_end(id);
        let mut x = v.x as u32 & 15;
        let y = v.y as u32 & 15;
        match v.direction {
            0 => x = (!x).wrapping_add(!y).wrapping_add(25),
            7 => x = (!y).wrapping_add(16),
            1 => x = (!x).wrapping_add(16),
            2 => x = (!x).wrapping_add(y).wrapping_add(9),
            3 => x = y,
            4 => x = x.wrapping_add(y).wrapping_sub(7),
            6 => x = (!y).wrapping_add(x).wrapping_add(9),
            _ => {}
        }
        if !signal
            && x.wrapping_add(
                u32::from(v.length + 1) / 2 * if v.direction & 1 != 0 { 1 } else { 2 },
            ) >= 16
        {
            (self.leaves.write_speed)(id, 0_u16);
            if reverse {
                self.reverse(id);
            }
            return false;
        }
        self.write_status(id, 16, true);
        let speed = BREAKDOWN_SPEEDS[(x & 15) as usize];
        if speed < v.speed {
            (self.leaves.write_speed)(id, (u64::from(speed)) as u16);
        }
        true
    }
    fn line_ends(&self, id: Handle, reverse: bool) -> bool {
        let v = self.read_line_ends(id);
        let t = v.breakdown;
        if t > 1 {
            self.write_status(id, 16, true);
            let speed = BREAKDOWN_SPEEDS[usize::from((!t >> 4) & 15)];
            if speed < v.speed {
                (self.leaves.write_speed)(id, (u64::from(speed)) as u16);
            }
        } else {
            self.write_status(id, 16, false);
        }
        if !self.can_leave(id) {
            return true;
        }
        let dir = self.exitdir(id);
        let tile = v
            .tile
            .wrapping_add(self.svc_tile_offset_diag(id, u64::from(dir)) as u32);
        let ts = self.svc_track_status(id, u64::from(tile), u64::from(dir ^ 2)) as u32;
        let reaches = self.svc_trackdir_reaches(id, u64::from(dir)) as u16;
        let tds = ts as u16 & reaches;
        let reds = (ts >> 16) as u16 & reaches;
        let mut bits = ((tds | tds >> 8) & 63) as u8;
        if self.svc_no_90(
            id,
            self.svc_tile_rail_type(Handle::NONE, u64::from(v.tile)),
            self.svc_tile_rail_type(Handle::NONE, u64::from(tile)),
        ) != 0
        {
            bits &= !(self.svc_track_crosses(id, u64::from(id.track().trailing_zeros())) as u8);
        }
        if bits == 0 || !self.compatible(id, tile) {
            return self.approaching_end(id, false, reverse);
        }
        if tds & reds != 0 {
            return self.approaching_end(id, true, reverse);
        }
        if self.svc_is_crossing(Handle::NONE, u64::from(tile)) != 0 {
            self.bar_crossing(tile);
        }
        true
    }
}
const INITIAL_SUBCOORD: [[[u8; 3]; 4]; 6] = [
    [[15, 8, 1], [0, 0, 0], [0, 8, 5], [0, 0, 0]],
    [[0, 0, 0], [8, 0, 3], [0, 0, 0], [8, 15, 7]],
    [[0, 0, 0], [7, 0, 2], [0, 7, 6], [0, 0, 0]],
    [[15, 8, 2], [0, 0, 0], [0, 0, 0], [8, 15, 6]],
    [[15, 7, 0], [8, 0, 4], [0, 0, 0], [0, 0, 0]],
    [[0, 0, 0], [0, 0, 0], [0, 8, 4], [7, 15, 0]],
];
impl Game<'_> {
    fn enter_station(&self, id: Handle, station: u16) {
        (self.leaves.write_last_station)(id, (u64::from(station)) as u16);
        if self.svc_visit_type(id, u64::from(station)) & self.svc_train_visit(id) == 0 {
            self.svc_write_visit_type(id, u64::from(station));
            self.svc_arrival_news(id, u64::from(station));
        }
        id.set_force_proceed(0);
        self.svc_view_window(id);
        self.svc_begin_loading(id);
        self.svc_arrival_triggers(id, u64::from(station));
    }
    fn speed_z(&self, id: Handle, old: i32) {
        let v = self.read_speed_z(id);
        if old == v.z || self.svc_acc_model(id) != 0 {
            return;
        }
        if old < v.z {
            (self.leaves.write_speed)(
                id,
                (u64::from(v.speed - ((u32::from(v.speed) * 64) >> 8) as u16)) as u16,
            );
        } else {
            let speed = v.speed.wrapping_add(2);
            if speed <= v.max_track_speed {
                (self.leaves.write_speed)(id, (u64::from(speed)) as u16);
            }
        }
    }
    fn moved_signals(&self, id: Handle, tile: u32, dir: u8) -> bool {
        if self.svc_is_railway(Handle::NONE, u64::from(tile)) != 0
            && self.svc_has_signals(Handle::NONE, u64::from(tile)) != 0
        {
            let tracks = self.svc_track_bits(Handle::NONE, u64::from(tile)) as u16;
            let tds = (tracks | tracks << 8) & self.svc_trackdir_reaches(id, u64::from(dir)) as u16;
            let td = tds.trailing_zeros() as u8;
            let exit = self.svc_trackdir_exit(id, u64::from(td));
            // Tile owner is used here, rather than the moving train's owner.
            if self.svc_signals_update_owner(
                id,
                u64::from(tile),
                exit,
                self.svc_tile_owner(Handle::NONE, u64::from(tile)),
            ) == self.svc_sigseg_pbs(id)
                && self.svc_has_signal_td(id, u64::from(tile), u64::from(td)) != 0
            {
                if self.svc_signal_pbs(
                    id,
                    self.svc_signal_type(id, u64::from(tile), u64::from(td & 7)),
                ) == 0
                {
                    return true;
                }
            }
        }
        false
    }
    // Result: 0 moved, 1 no move/early stop, 2 invalid rail, 3 requested reversal.
    fn move_vehicle(&self, id: Handle, prev: Handle, reverse: bool) -> (u8, bool) {
        let v = self.read_move_vehicle(id);
        if v.articulated != 0 {
            self.count(18);
        }
        let delta = [
            (-1, -1),
            (-1, 0),
            (-1, 1),
            (0, 1),
            (1, 1),
            (1, 0),
            (1, -1),
            (0, -1),
        ];
        let (dx, dy) = delta[usize::from(v.direction)];
        let (mut x, mut y) = (v.x.wrapping_add(dx), v.y.wrapping_add(dy));
        let mut old = v.tile;
        let new = self.svc_tile_virt(id, x as u64, y as u64) as u32;
        let mut entered = 0_u8;
        let mut signals = false;
        let mut changed = false;
        if id.track() != WORMHOLE {
            if old == new {
                if id.track() == DEPOT {
                    x = v.x;
                    y = v.y;
                } else {
                    if v.front != 0 && !self.line_ends(id, reverse) {
                        return (1, false);
                    }
                    let vets = self.svc_enter_tile(id, u64::from(new), x as u64, y as u64) as u8;
                    if vets & 4 != 0 {
                        return (2, false);
                    }
                    if vets & 1 != 0 {
                        self.enter_station(
                            id,
                            self.svc_station(Handle::NONE, u64::from(new)) as u16,
                        );
                    }
                }
            } else {
                entered = self.svc_diag_between(id, u64::from(old), u64::from(new)) as u8;
                let ts = self.svc_track_status(id, u64::from(new), u64::from(entered ^ 2)) as u32;
                let reaches = self.svc_trackdir_reaches(id, u64::from(entered)) as u16;
                let tds = ts as u16 & reaches;
                let reds = (((ts >> 16) as u16 & reaches) | (((ts >> 16) as u16 & reaches) >> 8))
                    as u8
                    & 63;
                let mut bits = ((tds | tds >> 8) & 63) as u8;
                if self.svc_no_90(
                    id,
                    self.svc_tile_rail_type(Handle::NONE, u64::from(old)),
                    self.svc_tile_rail_type(Handle::NONE, u64::from(new)),
                ) != 0
                    && prev == Handle::NONE
                {
                    bits &=
                        !(self.svc_track_crosses(id, u64::from(id.track().trailing_zeros())) as u8);
                }
                if bits == 0 || !self.compatible(id, new) {
                    return (2, false);
                }
                let mut chosen;
                if prev == Handle::NONE {
                    chosen = 1_u8
                        << self.svc_choose_track(
                            id,
                            u64::from(new),
                            u64::from(entered),
                            u64::from(bits),
                        );
                    if id.get_force_proceed() != 0
                        && self.svc_is_plain_rail(Handle::NONE, u64::from(new)) != 0
                        && self.svc_has_signals(Handle::NONE, u64::from(new)) != 0
                    {
                        let td = tds.trailing_zeros() as u8;
                        if self.svc_has_signal_td(id, u64::from(new), u64::from(td)) != 0
                            || (self.svc_has_signal_td(id, u64::from(new), u64::from(td ^ 8)) != 0
                                && self.svc_signal_type(id, u64::from(new), u64::from(td & 7))
                                    != self.svc_pbs_signal_type(id))
                        {
                            self.count(10);
                            id.set_force_proceed(u64::from(id.get_force_proceed() == 2));
                            self.svc_view_window(id);
                        }
                    }
                    if reds & chosen != 0 && id.get_force_proceed() == 0 {
                        let td = tds.trailing_zeros() as u8;
                        if id.flag(8) {
                            return (1, false);
                        }
                        if self.svc_has_signal_td(id, u64::from(new), u64::from(td ^ 8)) == 0 {
                            (self.leaves.write_speed)(id, 0_u16);
                            (self.leaves.write_subspeed)(id, 0_u8);
                            (self.leaves.write_progress)(id, 255_u8);
                            self.count(8);
                            if self.svc_reverse_at_signals(id) == 0
                                || u64::from(id.wait_inc())
                                    < self.svc_wait_oneway(id) * self.svc_day_ticks(id) * 2
                            {
                                return (1, false);
                            }
                        } else if self.svc_has_signal_td(id, u64::from(new), u64::from(td)) != 0 {
                            (self.leaves.write_speed)(id, 0_u16);
                            (self.leaves.write_subspeed)(id, 0_u8);
                            (self.leaves.write_progress)(id, 255_u8);
                            self.count(9);
                            if self.svc_reverse_at_signals(id) == 0
                                || u64::from(id.wait_inc())
                                    < self.svc_wait_twoway(id) * self.svc_day_ticks(id) * 2
                            {
                                let dir = self.svc_trackdir_exit(id, u64::from(td)) as u8;
                                let tile = self.tile_diag(new, dir);
                                let mut waiting = false;
                                self.visit_tile(tile, |u| {
                                    let other = self.read_move_vehicle_20(u);
                                    if other.status & 128 == 0
                                        && other.front != 0
                                        && u.track() & 63 != 0
                                        && other.speed <= 5
                                        && self.exitdir(u) == dir ^ 2
                                    {
                                        waiting = true;
                                        return true;
                                    }

                                    false
                                });
                                if !waiting {
                                    return (1, false);
                                }
                            }
                        }
                        if self.svc_reverse_at_signals(id) == 0
                            && self.svc_oneway_blocking(id, u64::from(new), u64::from(td)) == 0
                            && self.svc_signals_update(
                                id,
                                u64::from(self.read_tile(id)),
                                u64::from(entered),
                            ) == self.svc_sigseg_pbs(id)
                        {
                            id.set_wait_counter(0);
                            return (1, false);
                        }
                        return (3, false);
                    }
                    self.svc_try_reserve(id, u64::from(new), u64::from(chosen.trailing_zeros()), 0);
                } else {
                    let before = self.read_tile(prev);
                    if before == new {
                        chosen = if prev.track() == WORMHOLE {
                            bits
                        } else {
                            prev.track()
                        };
                    } else {
                        let exit =
                            self.svc_diag_between(id, u64::from(new), u64::from(before)) as usize;
                        let connecting =
                            [[1, 8, 0, 16], [4, 2, 16, 0], [0, 32, 1, 4], [32, 0, 8, 2]];
                        chosen = connecting[usize::from(entered)][exit];
                    }
                    chosen &= bits;
                }
                let sub = INITIAL_SUBCOORD[chosen.trailing_zeros() as usize][usize::from(entered)];
                x = (x & !15) | i32::from(sub[0]);
                y = (y & !15) | i32::from(sub[1]);
                let dir = sub[2];
                let vets = self.svc_enter_tile(id, u64::from(new), x as u64, y as u64) as u8;
                if vets & 4 != 0 {
                    return (2, false);
                }
                if vets & 2 == 0 {
                    if self.read_front(id) != 0 {
                        self.count(19);
                    }
                    let td = self.svc_track_direction(
                        id,
                        u64::from(chosen.trailing_zeros()),
                        u64::from(dir),
                    ) as u8;
                    if self.read_front(id) != 0
                        && self.svc_signal_has_pbs(id, u64::from(new), u64::from(td)) != 0
                    {
                        self.svc_set_signal_state(id, u64::from(new), u64::from(td), 0);
                        self.svc_dirty_tile(Handle::NONE, u64::from(new));
                    }
                    if self.read_next(id) == Handle::NONE {
                        self.svc_clear_reservation(
                            id,
                            u64::from(self.read_tile(id)),
                            u64::from(self.trackdir(id)),
                        );
                    }
                    (self.leaves.write_tile)(id, (u64::from(new)) as u32);
                    if self.svc_tile_rail_type(Handle::NONE, u64::from(new))
                        != self.svc_tile_rail_type(Handle::NONE, u64::from(old))
                    {
                        self.consist_changed(self.read_first(id), 0);
                    }
                    id.set_track(u64::from(chosen));
                }
                signals = true;
                if dir != self.read_direction(id) {
                    if prev == Handle::NONE && self.svc_acc_model(id) == 0 {
                        let diff = self.read_direction(id).wrapping_sub(dir) & 7;
                        let small = if self.svc_acc_type(id) == 2 { 0 } else { 64 };
                        let fraction = if diff == 1 || diff == 7 { small } else { 128 };
                        let speed = self.read_speed(id);
                        (self.leaves.write_speed)(
                            id,
                            (u64::from(speed - ((u32::from(speed) * fraction) >> 8) as u16)) as u16,
                        );
                    }
                    changed = true;
                    (self.leaves.write_direction)(id, (u64::from(dir)) as u8);
                }
                if self.read_front(id) != 0 {
                    id.set_wait_counter(0);
                    let crossing = self.approaching_crossing(id);
                    if crossing != INVALID
                        && self.svc_crossing_reserved(Handle::NONE, u64::from(crossing)) != 0
                        && self.svc_ambient_sound(id) != 0
                    {
                        self.svc_crossing_sound(Handle::NONE, u64::from(crossing));
                    }
                    self.svc_check_next(id);
                }
                if vets & 1 != 0 {
                    self.enter_station(id, self.svc_station(Handle::NONE, u64::from(new)) as u16);
                }
            }
        } else {
            if self.svc_is_tunnelbridge(Handle::NONE, u64::from(new)) != 0
                && self.svc_enter_tile(id, u64::from(new), x as u64, y as u64) & 2 != 0
            {
                if self.read_front(id) != 0 {
                    self.count(20);
                    self.reserve_track(
                        id,
                        new,
                        self.svc_tunnel_dir(Handle::NONE, u64::from(new)) & 1,
                    );
                    self.svc_check_next(id);
                }
                if old == new {
                    old = self.svc_other_end(Handle::NONE, u64::from(old)) as u32;
                }
            } else {
                (self.leaves.write_x)(id, (x as u64) as i32);
                (self.leaves.write_y)(id, (y as u64) as i32);
                self.svc_position(id);
                if self.read_status(id) & 1 == 0 {
                    self.svc_base_viewport(id, 1);
                }
                return (0, false);
            }
        }
        self.svc_update_delta(id);
        (self.leaves.write_x)(id, (x as u64) as i32);
        (self.leaves.write_y)(id, (y as u64) as i32);
        self.svc_position(id);
        let old_z = self.svc_inclination(id, u64::from(new != old), 0) as i32;
        if prev == Handle::NONE {
            self.speed_z(id, old_z);
        }
        if signals {
            if self.read_front(id) != 0 && self.moved_signals(id, new, entered) {
                if (self.svc_has_reserved(id, u64::from(new), u64::from(id.track())) == 0
                    && self.reserve_track(id, new, u64::from(id.track().trailing_zeros())) == 0)
                    || self.svc_try_path(id, 0, 0) == 0
                {
                    self.mark_stuck(id);
                }
            }
            if self.read_next(id) == Handle::NONE {
                self.moved_signals(id, old, entered ^ 2);
                if self.svc_is_crossing(Handle::NONE, u64::from(old)) != 0 {
                    self.update_crossing(old, true, false);
                }
            }
        }
        if self.read_front(id) != 0 && u64::from(self.read_tick(id)) % self.svc_backoff(id) == 0 {
            self.svc_check_next(id);
        }
        (0, changed)
    }
    fn controller(&self, mut id: Handle, nomove: Handle, reverse: bool) -> bool {
        let first = self.read_first(id);
        let mut prev = self.read_previous(id);
        let mut changed = false;
        while id != nomove {
            let (outcome, direction) = self.move_vehicle(id, prev, reverse);
            changed |= direction;
            if outcome == 1 {
                return false;
            }
            if outcome == 2 && prev != Handle::NONE {
                self.svc_disconnect(id);
            }
            if outcome >= 2 {
                if reverse {
                    id.set_wait_counter(0);
                    (self.leaves.write_speed)(id, 0_u16);
                    (self.leaves.write_subspeed)(id, 0_u8);
                    self.reverse(id);
                }
                return false;
            }
            prev = id;
            id = self.read_next(id);
        }
        if changed {
            first.set_cached_max_curve_speed(u64::from(self.curve_limit(first)));
        }
        true
    }
}

impl Game<'_> {
    /// `TryReserveRailTrack(tile, track)` with its default station triggers.
    fn reserve_track(&self, id: Handle, tile: u32, track: u64) -> u64 {
        self.svc_try_reserve(id, u64::from(tile), track, 1)
    }
    fn crash(&self, id: Handle, flooded: bool) -> u32 {
        let mut victims = 0_u32;
        if self.read_front(id) != 0 {
            victims += 2;
            if !id.flag(8) {
                self.svc_free_reservation(id);
            }
            let mut u = id;
            while u != Handle::NONE {
                let tile = self.read_tile(u);
                self.svc_clear_reservation(u, u64::from(tile), u64::from(self.trackdir(u)));
                if self.svc_is_tunnelbridge(Handle::NONE, u64::from(tile)) != 0 {
                    self.svc_set_tunnel_res(
                        u,
                        self.svc_other_end(Handle::NONE, u64::from(tile)),
                        0,
                    );
                }
                u = self.read_next(u);
            }
            let crossing = self.approaching_crossing(id);
            if crossing != INVALID {
                self.update_crossing(crossing, true, false);
            }
            self.svc_hide_fill(id);
        }
        victims = victims.wrapping_add(self.svc_crash_ground(id, u64::from(flooded)) as u32);
        id.set_crash_anim_pos(if flooded { 4000 } else { 1 });
        victims
    }
    fn crashed(&self, id: Handle) -> u32 {
        let mut victims = 0;
        if self.read_status(id) & 128 == 0 {
            victims = self.crash(id, false);
            self.svc_crash_event(id, u64::from(victims));
        }
        self.svc_reserve_under(id);
        victims
    }
    fn collision_one(&self, other: Handle, id: Handle) -> u32 {
        if other.track() == DEPOT {
            return 0;
        }
        let v = self.read_collision_one(other);
        let t = self.read_collision_one_22(id);
        if v.owner != t.owner || v.first == id {
            return 0;
        }
        let x = v.x.wrapping_sub(t.x);
        let y = v.y.wrapping_sub(t.y);
        if ((y.wrapping_add(7) | x.wrapping_add(7)) as u32) & !15 != 0 {
            return 0;
        }
        let min = (i32::from(v.length) + 1) / 2 + (i32::from(t.length) + 1) / 2 - 1;
        if x.wrapping_mul(x).wrapping_add(y.wrapping_mul(y)) > min.wrapping_mul(min)
            || v.z.wrapping_sub(t.z).wrapping_abs() > 5
        {
            return 0;
        }
        let victims = self.crashed(id);
        victims.wrapping_add(self.crashed(v.first))
    }
    fn collision(&self, id: Handle) -> bool {
        if id.track() == DEPOT {
            return false;
        }
        let v = self.read_after_swap(id);
        let mut victims = 0_u32;
        if id.track() == WORMHOLE {
            self.visit_tile(v.tile, |u| {
                victims = victims.wrapping_add(self.collision_one(u, id));

                false
            });
            self.visit_tile(
                self.svc_other_end(Handle::NONE, u64::from(v.tile)) as u32,
                |u| {
                    victims = victims.wrapping_add(self.collision_one(u, id));

                    false
                },
            );
        } else {
            self.visit_near(v.x, v.y, |u| {
                victims = victims.wrapping_add(self.collision_one(u, id));

                false
            });
        }
        if victims == 0 {
            return false;
        }
        self.count(15);
        self.svc_crash_news(id, u64::from(victims));
        self.svc_crash_rating(id);
        if self.svc_disaster_sound(id) != 0 {
            self.svc_crash_sound(id);
        }
        true
    }
    fn platform_occupied(&self, tile: u32) -> bool {
        let delta = self.svc_tile_offset_axis(
            Handle::NONE,
            self.svc_station_axis(Handle::NONE, u64::from(tile)),
        ) as u32;
        let mut t = tile;
        while self.svc_station_compatible(Handle::NONE, u64::from(t), u64::from(tile)) != 0 {
            if self.train_on_tile(t) {
                return true;
            }
            t = t.wrapping_sub(delta);
        }
        t = tile.wrapping_add(delta);
        while self.svc_station_compatible(Handle::NONE, u64::from(t), u64::from(tile)) != 0 {
            if self.train_on_tile(t) {
                return true;
            }
            t = t.wrapping_add(delta);
        }
        false
    }
    fn delete_last(&self, mut id: Handle) {
        self.count(16);
        let first = self.read_first(id);
        let mut last = id;
        while self.read_next(id) != Handle::NONE {
            last = id;
            id = self.read_next(id);
        }
        self.svc_set_next(last, Handle::NONE);
        if first != id {
            self.consist_changed(first, 3);
            if first.track() == DEPOT {
                self.svc_depot_window(first, u64::from(self.read_tile(first)));
            }
            (self.leaves.write_last_station)(id, (u64::from(self.read_last_station(first))) as u16);
        }
        let mut tracks = id.track();
        let v = self.read_delete_last(id);
        let tile = v.tile;
        let owner = v.owner;
        self.svc_delete_vehicle(id);
        if tracks == WORMHOLE {
            tracks = 1 << (self.svc_tunnel_dir(Handle::NONE, u64::from(tile)) & 1);
        }
        let track = tracks.trailing_zeros() as u8;
        if self.svc_has_reserved(Handle::NONE, u64::from(tile), u64::from(tracks)) != 0 {
            self.svc_unreserve(Handle::NONE, u64::from(tile), u64::from(track));
            let mut remaining = 0_u8;
            self.visit_tile(tile, |u| {
                if self.read_status(u) & 128 == 0 {
                    return false;
                }
                let t = u.track();
                if t == WORMHOLE {
                    remaining |=
                        1 << (self.svc_tunnel_dir(Handle::NONE, u64::from(self.read_tile(u))) & 1);
                } else if t != DEPOT {
                    remaining |= t;
                }

                false
            });
            for t in 0..6 {
                if remaining & (1 << t) != 0 {
                    self.reserve_track(Handle::NONE, tile, t);
                }
            }
        }
        if self.svc_is_crossing(Handle::NONE, u64::from(tile)) != 0 {
            self.update_crossing(tile, true, false);
        }
        if self.svc_is_station(Handle::NONE, u64::from(tile)) != 0 {
            let occupied = self.platform_occupied(tile);
            let dir = self.svc_axis_diag(
                Handle::NONE,
                self.svc_station_axis(Handle::NONE, u64::from(tile)),
            ) as u8;
            self.svc_set_platform_res(
                Handle::NONE,
                u64::from(tile),
                u64::from(dir),
                u64::from(occupied),
            );
            self.svc_set_platform_res(
                Handle::NONE,
                u64::from(tile),
                u64::from(dir ^ 2),
                u64::from(occupied),
            );
        }
        if self.svc_is_tunnelbridge(Handle::NONE, u64::from(tile)) != 0
            || self.svc_is_depot(Handle::NONE, u64::from(tile)) != 0
        {
            self.svc_signals_update_owner(
                Handle::NONE,
                u64::from(tile),
                u64::from(u8::MAX),
                u64::from(owner),
            );
        } else {
            self.svc_signals_both(
                Handle::NONE,
                u64::from(tile),
                u64::from(track),
                u64::from(owner),
            );
        }
    }
    fn change_dir_randomly(&self, mut id: Handle) {
        loop {
            if self.read_status(id) & 1 == 0 {
                let delta = [7, 0, 0, 1][(self.services.random() & 3) as usize];
                (self.leaves.write_direction)(
                    id,
                    (u64::from(self.read_direction(id).wrapping_add(delta) & 7)) as u8,
                );
                if id.track() != WORMHOLE {
                    self.svc_position(id);
                    self.svc_inclination(id, 0, 1);
                } else {
                    self.svc_viewport(id, 0, 1);
                }
            }
            id = self.read_next(id);
            if id == Handle::NONE {
                break;
            }
        }
    }
    fn handle_crashed(&self, id: Handle) -> bool {
        let state = (id.get_crash_anim_pos() as u16).wrapping_add(1);
        id.set_crash_anim_pos(u64::from(state));
        if state == 4 && self.read_status(id) & 1 == 0 {
            self.svc_large_explosion(id);
        }
        if state <= 200 {
            let mut r = self.services.random();
            if crate::services::chance16_i(1, 7, r) {
                let mut index = (r.wrapping_mul(10) >> 16) as i32;
                let mut u = id;
                loop {
                    index -= 1;
                    if index < 0 {
                        r = self.services.random();
                        self.svc_small_explosion(
                            u,
                            u64::from(((r >> 8) & 7) + 2),
                            u64::from(((r >> 16) & 7) + 2),
                            u64::from((r & 7) + 5),
                        );
                        break;
                    }
                    u = self.read_next(u);
                    if u == Handle::NONE {
                        break;
                    }
                }
            }
        }
        let tick = self.read_tick(id);
        if state <= 240 && tick & 3 == 0 {
            self.change_dir_randomly(id);
        }
        if state >= 4440 && tick & 31 == 0 {
            let remains = self.read_next(id) != Handle::NONE;
            self.delete_last(id);
            return remains;
        }
        true
    }
    fn stay_depot(&self, id: Handle) -> bool {
        if !self.whole_in_depot(id) {
            return false;
        }
        let v = self.read_stay_depot(id);
        if v.power == 0 {
            self.write_status(id, 2, true);
            self.svc_depot_window(id, u64::from(v.tile));
            return true;
        }
        if self.svc_wait_unbunch(id) != 0 {
            return true;
        }
        if id.get_force_proceed() == 0 {
            if id.wait_inc() < 37 {
                self.svc_train_list(id);
                return true;
            }
            id.set_wait_counter(0);
        }
        let segment = if self.svc_reserve_paths(id) != 0 {
            self.svc_sigseg_pbs(id)
        } else {
            self.svc_signals_update(id, u64::from(v.tile), u64::from(u8::MAX))
        };
        if id.get_force_proceed() == 0
            && (segment == self.svc_sigseg_full(id)
                || self.svc_has_depot_res(Handle::NONE, u64::from(v.tile)) != 0)
        {
            self.svc_train_list(id);
            return true;
        }
        if self.read_order(id) == 2 && self.read_tile(id) == self.read_dest(id) {
            if self.svc_has_depot_res(Handle::NONE, u64::from(self.read_tile(id))) == 0 {
                self.count(21);
                self.svc_enter_depot(id);
            }
            return true;
        }
        if segment == self.svc_sigseg_pbs(id)
            && self.svc_try_path(id, 0, 0) == 0
            && id.get_force_proceed() == 0
        {
            self.svc_train_list(id);
            self.mark_stuck(id);
            return true;
        }
        self.svc_set_depot_res(id, u64::from(self.read_tile(id)), 1);
        if self.svc_show_reservation(id) != 0 {
            self.svc_dirty_tile(Handle::NONE, u64::from(self.read_tile(id)));
        }
        self.count(6);
        self.svc_service(id);
        self.svc_leave_unbunch(id);
        self.svc_leave_sound(id);
        self.svc_train_list(id);
        id.set_track(if self.read_direction(id) & 2 != 0 {
            2
        } else {
            1
        });
        self.write_status(id, 1, false);
        (self.leaves.write_speed)(id, 0_u16);
        self.svc_viewport(id, 1, 1);
        self.svc_position(id);
        self.svc_signals_update(id, u64::from(self.read_tile(id)), u64::from(u8::MAX));
        self.update_acceleration(id);
        self.svc_depot_dirty(id);
        false
    }
    fn loco(&self, id: Handle, mode: bool) -> bool {
        self.count(u64::from(mode));
        if self.read_status(id) & 128 != 0 {
            return if mode { true } else { self.handle_crashed(id) };
        }
        if id.get_force_proceed() != 0 {
            id.set_flag(8, false);
            self.svc_start_stop_dirty(id);
        }
        if self.svc_handle_breakdown(id) != 0 {
            return true;
        }
        if id.flag(0) && self.read_speed(id) == 0 {
            self.reverse(id);
        }
        let v = self.read_loco(id);
        if v.status & 2 != 0 && v.speed == 0 {
            return true;
        }
        let valid_order = v.order != 0 && v.order != 7;
        if self.svc_process_orders(id) != 0 && self.svc_check_reverse(id) != 0 {
            id.set_wait_counter(0);
            (self.leaves.write_speed)(id, 0_u16);
            (self.leaves.write_subspeed)(id, 0_u8);
            id.set_flag(9, false);
            self.reverse(id);
            return true;
        } else if id.flag(9) {
            let tile = self.read_tile(id);
            let mut dir = self.exitdir(id);
            if self.svc_is_depot(Handle::NONE, u64::from(tile)) != 0
                || self.svc_is_tunnelbridge(Handle::NONE, u64::from(tile)) != 0
            {
                dir = u8::MAX;
            }
            if self.svc_signals_update(id, u64::from(tile), u64::from(dir))
                == self.svc_sigseg_pbs(id)
                || self.svc_reserve_paths(id) != 0
            {
                self.svc_try_path(id, 1, 1);
            }
            id.set_flag(9, false);
        }
        self.svc_loading(id, u64::from(mode));
        if self.read_order(id) == 3 || self.stay_depot(id) {
            return true;
        }
        if !mode {
            self.svc_show_effect(id);
        }
        if !valid_order && self.read_order(id) != 0 {
            self.svc_check_next(id);
        }
        if !mode && id.flag(8) {
            self.count(11);
            let wait = u64::from(id.wait_inc());
            let turn = wait % (self.svc_wait_pbs(id) * self.svc_day_ticks(id)) == 0
                && self.svc_reverse_at_signals(id) != 0;
            if !turn && wait % self.svc_backoff(id) != 0 && id.get_force_proceed() == 0 {
                return true;
            }
            if self.svc_try_path(id, 0, 0) == 0 {
                if turn {
                    self.count(12);
                    self.reverse(id);
                }
                if id.flag(8)
                    && id.get_wait_counter() > 2 * self.svc_wait_pbs(id) * self.svc_day_ticks(id)
                {
                    if self.svc_lost_warn(id) != 0
                        && u64::from(self.read_owner(id)) == self.svc_local_company(id)
                    {
                        self.svc_stuck_news(id);
                    }
                    id.set_wait_counter(0);
                }
                if id.get_force_proceed() == 0 {
                    return true;
                }
                id.set_flag(8, false);
                id.set_wait_counter(0);
                self.svc_start_stop_dirty(id);
            }
        }
        if self.read_order(id) == 4 {
            self.svc_order_free(id);
            self.svc_start_stop_dirty(id);
            return true;
        }
        let mut distance = self.update_speed(id);
        if self.read_speed(id) == 0 && self.read_status(id) & 2 != 0 {
            id.set_force_proceed(0);
            self.svc_view_window(id);
        }
        let mut advance = self.svc_advance_distance(id) as i32;
        if distance < advance {
            if self.read_speed(id) == 0 {
                self.svc_last_speed(id);
            }
        } else {
            self.line_ends(id, true);
            loop {
                distance = distance.wrapping_sub(advance);
                self.controller(id, Handle::NONE, true);
                if self.collision(id) {
                    break;
                }
                advance = self.svc_advance_distance(id) as i32;
                if distance < advance || self.read_speed(id) == 0 {
                    break;
                }
                let v = self.read_loco_26(id);
                if (v.order == 6 || v.order == 1)
                    && v.nonstop & 2 != 0
                    && self.svc_is_station_any(Handle::NONE, u64::from(v.tile)) != 0
                    && u64::from(v.order_destination)
                        == self.svc_station(Handle::NONE, u64::from(v.tile))
                {
                    self.svc_process_orders(id);
                }
            }
            self.svc_last_speed(id);
        }
        let mut u = id;
        while u != Handle::NONE {
            if self.read_status(u) & 1 == 0 {
                self.svc_viewport(u, 0, 0);
            }
            u = self.read_next(u);
        }
        if self.read_progress(id) == 0 {
            (self.leaves.write_progress)(id, (distance as u64) as u8);
        }
        true
    }
    fn tick(&self, id: Handle) -> bool {
        (self.leaves.write_tick)(id, (u64::from(self.read_tick(id).wrapping_add(1))) as u8);
        let v = self.read_tick_state(id);
        if v.front != 0 {
            if v.status & 2 == 0 || v.speed > 0 {
                (self.leaves.write_running)(id, (u64::from(v.running.wrapping_add(1))) as u8);
            }
            (self.leaves.write_order_time)(
                id,
                (self.read_order_time(id).wrapping_add(1) as u64) as i32,
            );
            if !self.loco(id, false) {
                return false;
            }
            return self.loco(id, true);
        } else if v.free_wagon != 0 && v.status & 128 != 0 {
            let state = (id.get_crash_anim_pos() as u16).wrapping_add(1);
            id.set_crash_anim_pos(u64::from(state));
            if state >= 4400 {
                self.count(17);
                self.svc_delete_vehicle(id);
                return false;
            }
        }
        true
    }
    fn needs_service(&self, id: Handle) {
        if self.svc_servint(id) == 0 || self.svc_needs_service(id) == 0 {
            return;
        }
        if self.svc_chain_depot(id) != 0 {
            self.count(7);
            self.svc_service(id);
            return;
        }
        let max = self.svc_max_depot_penalty(id) as u32;
        let depot = self.svc_find_depot(id, u64::from(max));
        let length = depot.length;
        if length == u32::MAX || length > max {
            if self.read_order(id) == 2 {
                self.svc_order_dummy(id);
                self.svc_start_stop_dirty(id);
            }
            return;
        }
        let tile = depot.tile;
        let depot_id = self.svc_depot_index(Handle::NONE, u64::from(tile)) as u16;
        let v = self.read_needs_service(id);
        if v.order == 2
            && v.order_destination != depot_id
            && !crate::services::chance16_i(3, 16, self.services.random())
        {
            return;
        }
        self.svc_suppress_implicit(id);
        self.svc_order_depot_service(id, u64::from(depot_id));
        (self.leaves.write_dest)(id, (u64::from(tile)) as u32);
        self.svc_start_stop_dirty(id);
    }
    fn economy_day(&self, id: Handle) {
        self.svc_economy_age(id);
        let day = self.read_day(id).wrapping_add(1);
        (self.leaves.write_day)(id, (u64::from(day)) as u8);
        if day & 7 == 0 {
            self.svc_decrease_value(id);
        }
        if self.read_front(id) != 0 {
            self.svc_check_breakdown(id);
            self.needs_service(id);
            self.svc_check_orders(id);
            if self.read_order(id) == 1 {
                let tile = self.svc_station_dest(id) as u32;
                if tile != INVALID {
                    (self.leaves.write_dest)(id, (u64::from(tile)) as u32);
                }
            }
            let running = self.read_running(id);
            if running != 0 {
                let cost = self.running_cost(id).saturating_mul(i64::from(running))
                    / (self.svc_cost_divisor(id) as i64);
                self.svc_pay_running(id, cost as u64);
                self.svc_running_windows(id);
            }
        }
    }
}
impl Game<'_> {
    fn next_force(&self, id: Handle) -> u8 {
        let v = self.read_next_force(id);
        if v.status & 128 != 0 || id.get_force_proceed() == 2 {
            return 0;
        }
        if !id.flag(8) {
            return if self.svc_chain_depot(id) != 0 { 1 } else { 2 };
        }
        let dir = self.svc_trackdir_exit(id, u64::from(self.trackdir(id))) as u8;
        let tile = self.tile_diag(v.tile, dir);
        if tile == INVALID
            || self.svc_is_railway(Handle::NONE, u64::from(tile)) == 0
            || self.svc_has_signals(Handle::NONE, u64::from(tile)) == 0
        {
            return 1;
        }
        let tracks = self.svc_diag_reaches_tracks(id, u64::from(dir)) as u8
            & self.svc_track_bits(Handle::NONE, u64::from(tile)) as u8;
        if tracks != 0
            && self.svc_has_signal(id, u64::from(tile), u64::from(tracks.trailing_zeros())) != 0
        {
            2
        } else {
            1
        }
    }
    fn reverse_command(&self, id: Handle, execute: bool, single: bool) -> u8 {
        if single {
            if self.read_multiheaded(id) != 0 || self.svc_reverse_single_blocked(id) != 0 {
                return 1;
            }
            let first = self.read_first(id);
            if self.svc_stopped_in_depot(first) == 0 {
                return 2;
            }
            if execute {
                id.set_flag(4, !id.flag(4));
                self.consist_changed(first, 3);
                self.svc_reverse_windows(first);
            }
        } else {
            let v = self.read_reverse_command(id);
            if v.front == 0 || v.status & 128 != 0 || v.breakdown != 0 {
                return 3;
            }
            if execute {
                if self.read_order(id) == 3 {
                    let last = self.read_tile(self.read_last(id));
                    if self.svc_is_station_any(Handle::NONE, u64::from(last)) == 0
                        || self.svc_station(Handle::NONE, u64::from(last))
                            != self.svc_station(Handle::NONE, u64::from(self.read_tile(id)))
                    {
                        self.svc_leave_station(id);
                    }
                }
                id.set_force_proceed(0);
                self.svc_view_window(id);
                if self.svc_acc_model(id) != 0 && self.read_speed(id) != 0 {
                    id.set_flag(0, !id.flag(0));
                } else {
                    (self.leaves.write_speed)(id, 0_u16);
                    self.svc_last_speed(id);
                    self.svc_hide_fill(id);
                    self.reverse(id);
                }
                self.svc_reset_unbunch(id);
            }
        }
        0
    }
    fn force_command(&self, id: Handle, execute: bool) -> bool {
        if self.read_front(id) == 0 {
            return false;
        }
        if execute {
            id.set_force_proceed(u64::from(self.next_force(id)));
            self.svc_view_window(id);
            self.svc_reset_unbunch(id);
        }
        true
    }
}

/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_consist_changed(
    id: Handle,
    allowed: u8,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.consist_changed(id, allowed);
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_curve_limit(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) -> u16 {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.curve_limit(id)
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_current_max_speed(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) -> i32 {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.current_max_speed(id)
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_update_acceleration(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.update_acceleration(id);
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_update_speed(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) -> i32 {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.update_speed(id)
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_mark_dirty(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.mark_dirty(id);
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_tick(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) -> u8 {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    u8::from(g.tick(id))
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_calendar_day(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.svc_age(id);
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_economy_day(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.economy_day(id);
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_running_cost(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) -> i64 {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.running_cost(id)
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_trackdir(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) -> u8 {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.trackdir(id)
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_mark_stuck(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.mark_stuck(id);
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_approaching_crossing(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) -> u32 {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.approaching_crossing(id)
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_reverse_swap(
    id: Handle,
    left: i32,
    right: i32,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.reverse_swap(id, left, right);
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_train_on_tile(
    tile: u32,
    leaves: *const Leaves,
    services: *const Services,
) -> u8 {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    u8::from(g.train_on_tile(tile))
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_update_crossing(
    tile: u32,
    sound: u8,
    force_bar: u8,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.update_crossing(tile, sound != 0, force_bar != 0);
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_adjacent_crossing_dirty(
    tile: u32,
    axis: u8,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.adjacent_crossing_dirty(tile, u64::from(axis));
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_crossing_removed(
    tile: u32,
    axis: u8,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.crossing_removed(tile, u64::from(axis));
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_reverse(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.reverse(id);
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_controller(
    id: Handle,
    nomove: Handle,
    reverse: u8,
    leaves: *const Leaves,
    services: *const Services,
) -> u8 {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    u8::from(g.controller(id, nomove, reverse != 0))
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_crash(
    id: Handle,
    flooded: u8,
    leaves: *const Leaves,
    services: *const Services,
) -> u32 {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.crash(id, flooded != 0)
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_stay_depot(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) -> u8 {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    u8::from(g.stay_depot(id))
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_needs_service(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.needs_service(id);
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_next_force(
    id: Handle,
    leaves: *const Leaves,
    services: *const Services,
) -> u8 {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.next_force(id)
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_reverse_command(
    id: Handle,
    execute: u8,
    single: u8,
    leaves: *const Leaves,
    services: *const Services,
) -> u8 {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    g.reverse_command(id, execute != 0, single != 0)
}
/// Invoke synchronous train policy with live serialized services.
/// # Safety
/// The descriptors and shell must remain live until the call returns; callbacks may reenter.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_force_command(
    id: Handle,
    execute: u8,
    leaves: *const Leaves,
    services: *const Services,
) -> u8 {
    // SAFETY: Immutable descriptors are live for this call; unaligned supports MSVC x86.
    let g = Game {
        leaves: unsafe { &*leaves },
        services: unsafe { &*services },
    };
    u8::from(g.force_command(id, execute != 0))
}
