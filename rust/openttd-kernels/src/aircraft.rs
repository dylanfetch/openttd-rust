/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */
//! Complete aircraft controllers and canonical private/airport-block ownership.
#![allow(
    unsafe_code,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::too_many_arguments,
    clippy::too_many_lines,
    clippy::many_single_char_names,
    clippy::struct_field_names,
    clippy::verbose_bit_mask,
    clippy::if_not_else, // Preserve the reference controller branch order.
    clippy::manual_midpoint // Source int bounds are finite map heights.
)]
#[repr(C)]
pub struct State {
    pub cached_max_range_sqr: u32,
    pub cached_max_range: u16,
    pub cache_padding: u16,
    pub crashed_counter: u16,
    pub targetairport: u16,
    pub pos: u8,
    pub previous_pos: u8,
    pub state: u8,
    pub last_direction: u8,
    pub number_consecutive_turns: u8,
    pub turn_counter: u8,
    pub flags: u8,
}
use crate::services::chance16_i;
use std::ffi::c_void;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Vehicle {
    pub handle: *mut c_void,
    pub state: *mut State,
    pub id: u32,
}
impl Vehicle {
    fn invalid() -> Self {
        Self {
            handle: std::ptr::null_mut(),
            state: std::ptr::null_mut(),
            id: 1_048_575,
        }
    }
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Node {
    pub next: *const c_void,
    pub blocks: u64,
    pub position: u8,
    pub next_position: u8,
    pub heading: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Moving {
    pub x: i16,
    pub y: i16,
    pub flags: u16,
    pub direction: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Position {
    pub x: i32,
    pub y: i32,
    pub tile: u32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct BlockNode {
    pub blocks: u64,
    pub position: u8,
    pub next_position: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct RouteNode {
    pub next: *const c_void,
    pub next_position: u8,
    pub heading: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct BlockChoice {
    pub next: *const c_void,
    pub blocks: u64,
    pub heading: u8,
}
struct Movement {
    x: i64,
    y: i64,
    flags: i64,
    direction: i64,
}
struct NewPosition {
    x: i64,
    y: i64,
    tile: u32,
}
#[repr(C)]
pub struct Leaves {
    pub subtype: unsafe extern "C" fn(Vehicle) -> u8,
    pub x: unsafe extern "C" fn(Vehicle) -> i32,
    pub set_x: unsafe extern "C" fn(Vehicle, i32),
    pub y: unsafe extern "C" fn(Vehicle) -> i32,
    pub set_y: unsafe extern "C" fn(Vehicle, i32),
    pub z: unsafe extern "C" fn(Vehicle) -> i32,
    pub set_z: unsafe extern "C" fn(Vehicle, i32),
    pub tile: unsafe extern "C" fn(Vehicle) -> u32,
    pub set_tile: unsafe extern "C" fn(Vehicle, u32),
    pub direction: unsafe extern "C" fn(Vehicle) -> u8,
    pub set_direction: unsafe extern "C" fn(Vehicle, u8),
    pub tick_counter: unsafe extern "C" fn(Vehicle) -> u8,
    pub set_tick_counter: unsafe extern "C" fn(Vehicle, u8),
    pub owner: unsafe extern "C" fn(Vehicle) -> u8,
    pub vehicle_status: unsafe extern "C" fn(Vehicle) -> u8,
    pub set_vehicle_status: unsafe extern "C" fn(Vehicle, u8),
    pub current_speed: unsafe extern "C" fn(Vehicle) -> u16,
    pub set_current_speed: unsafe extern "C" fn(Vehicle, u16),
    pub subspeed: unsafe extern "C" fn(Vehicle) -> u8,
    pub set_subspeed: unsafe extern "C" fn(Vehicle, u8),
    pub progress: unsafe extern "C" fn(Vehicle) -> u8,
    pub set_progress: unsafe extern "C" fn(Vehicle, u8),
    pub acceleration: unsafe extern "C" fn(Vehicle) -> u8,
    pub maximum_speed: unsafe extern "C" fn(Vehicle) -> u16,
    pub set_maximum_speed: unsafe extern "C" fn(Vehicle, u16),
    pub set_breakdown_counter: unsafe extern "C" fn(Vehicle, u8),
    pub order_type: unsafe extern "C" fn(Vehicle) -> u8,
    pub order_destination: unsafe extern "C" fn(Vehicle) -> u16,
    pub running_ticks: unsafe extern "C" fn(Vehicle) -> u8,
    pub set_running_ticks: unsafe extern "C" fn(Vehicle, u8),
    pub order_time: unsafe extern "C" fn(Vehicle) -> i32,
    pub set_order_time: unsafe extern "C" fn(Vehicle, i32),
    pub day_counter: unsafe extern "C" fn(Vehicle) -> u8,
    pub set_day_counter: unsafe extern "C" fn(Vehicle, u8),
    pub profit: unsafe extern "C" fn(Vehicle) -> i64,
    pub set_profit: unsafe extern "C" fn(Vehicle, i64),
    pub last_station: unsafe extern "C" fn(Vehicle) -> u16,
    pub set_last_station: unsafe extern "C" fn(Vehicle, u16),
    pub set_economy_service: unsafe extern "C" fn(Vehicle, i32),
    pub set_calendar_service: unsafe extern "C" fn(Vehicle, i32),
    pub set_breakdowns: unsafe extern "C" fn(Vehicle, u8),
    pub set_reliability: unsafe extern "C" fn(Vehicle, u16),
    pub set_cargo_age: unsafe extern "C" fn(Vehicle, u16),
    pub next: unsafe extern "C" fn(Vehicle) -> Vehicle,
    pub map_size_x: unsafe extern "C" fn() -> u32,
    pub map_max_x: unsafe extern "C" fn() -> u32,
    pub map_max_y: unsafe extern "C" fn() -> u32,
    pub plane_speed: unsafe extern "C" fn() -> u8,
    pub no_jetcrash: unsafe extern "C" fn() -> bool,
    pub plane_crashes: unsafe extern "C" fn() -> u8,
    pub service_at_helipad: unsafe extern "C" fn() -> bool,
    pub disaster_sound: unsafe extern "C" fn() -> bool,
    pub economy_date: unsafe extern "C" fn() -> i32,
    pub calendar_date: unsafe extern "C" fn() -> i32,
    pub station: unsafe extern "C" fn(u16) -> *const c_void,
    pub airport_tile: unsafe extern "C" fn(*const c_void) -> u32,
    pub station_tile: unsafe extern "C" fn(*const c_void) -> u32,
    pub rotation: unsafe extern "C" fn(*const c_void) -> u8,
    pub airport_width: unsafe extern "C" fn(*const c_void) -> u16,
    pub airport_height: unsafe extern "C" fn(*const c_void) -> u16,
    pub airport_type: unsafe extern "C" fn(*const c_void) -> u8,
    pub station_owner: unsafe extern "C" fn(*const c_void) -> u8,
    pub has_hangar: unsafe extern "C" fn(*const c_void) -> bool,
    pub has_airport: unsafe extern "C" fn(*const c_void) -> bool,
    pub airport_fta: unsafe extern "C" fn(*const c_void) -> *const c_void,
    pub airport_blocks: unsafe extern "C" fn(*const c_void) -> *mut u64,
    pub had_vehicle: unsafe extern "C" fn(*const c_void) -> u8,
    pub dummy_airport: unsafe extern "C" fn() -> *const c_void,
    pub airport_elements: unsafe extern "C" fn(*const c_void) -> u8,
    pub helipads: unsafe extern "C" fn(*const c_void) -> u8,
    pub airport_flags: unsafe extern "C" fn(*const c_void) -> u8,
    pub airport_delta_z: unsafe extern "C" fn(*const c_void) -> u8,
    pub node: unsafe extern "C" fn(*const c_void, u8) -> *const c_void,
    pub fta: unsafe extern "C" fn(*const c_void) -> Node,
    pub moving: unsafe extern "C" fn(*const c_void, u8) -> Moving,
    pub engine_speed: unsafe extern "C" fn(Vehicle) -> u16,
    pub engine_subtype: unsafe extern "C" fn(Vehicle) -> u8,
    pub engine_sound: unsafe extern "C" fn(Vehicle) -> u16,
    pub engine_reliability: unsafe extern "C" fn(Vehicle) -> u16,
    pub vehicle_type: unsafe extern "C" fn(Vehicle) -> u8,
    pub slope: unsafe extern "C" fn(i32, i32) -> i32,
    pub tile_height: unsafe extern "C" fn(u32) -> i32,
    pub airport_entry: unsafe extern "C" fn(*const c_void, u8) -> u8,
    pub direction_towards: unsafe extern "C" fn(Vehicle, i32, i32) -> u8,
    pub new_position: unsafe extern "C" fn(Vehicle) -> Position,
    pub hangar_height: unsafe extern "C" fn(u16) -> i32,
    pub terminal_count: unsafe extern "C" fn(*const c_void, u32) -> u8,
    pub hangar_exit: unsafe extern "C" fn(Vehicle) -> u8,
    pub can_use_station: unsafe extern "C" fn(Vehicle, u16) -> bool,
    pub service_interval: unsafe extern "C" fn(Vehicle) -> u16,
    pub needs_service: unsafe extern "C" fn(Vehicle) -> bool,
    pub chain_in_depot: unsafe extern "C" fn(Vehicle) -> bool,
    pub waiting_unbunching: unsafe extern "C" fn(Vehicle) -> bool,
    pub nearest_depot_order: unsafe extern "C" fn(Vehicle) -> bool,
    pub part_of_orders: unsafe extern "C" fn(Vehicle) -> bool,
    pub next_station: unsafe extern "C" fn(u16) -> u16,
    pub next_aircraft: unsafe extern "C" fn(u32) -> Vehicle,
    pub random: unsafe extern "C" fn() -> u32,
    pub update_position: unsafe extern "C" fn(Vehicle),
    pub rotor_image: unsafe extern "C" fn(Vehicle),
    pub copy_sprite: unsafe extern "C" fn(Vehicle, Vehicle),
    pub position_viewport: unsafe extern "C" fn(Vehicle),
    pub create_effect: unsafe extern "C" fn(Vehicle, u8, i32, i32, i32),
    pub dirty_start_stop: unsafe extern "C" fn(Vehicle),
    pub play_sound: unsafe extern "C" fn(Vehicle, u16),
    pub truncate_cargo: unsafe extern "C" fn(Vehicle),
    pub crash_news: unsafe extern "C" fn(Vehicle, u16, u32),
    pub station_rating: unsafe extern "C" fn(u32, u8, i32, u32),
    pub landing_rating: unsafe extern "C" fn(u16, u8),
    pub free_order: unsafe extern "C" fn(Vehicle),
    pub service_in_depot: unsafe extern "C" fn(Vehicle),
    pub leave_unbunching: unsafe extern "C" fn(Vehicle),
    pub dirty_depot: unsafe extern "C" fn(Vehicle),
    pub first_arrival: unsafe extern "C" fn(Vehicle, u16),
    pub begin_loading: unsafe extern "C" fn(Vehicle),
    pub dirty_details: unsafe extern "C" fn(Vehicle),
    pub update_delta: unsafe extern "C" fn(Vehicle),
    pub touchdown_animation: unsafe extern "C" fn(Vehicle, u16),
    pub destination_too_far: unsafe extern "C" fn(Vehicle),
    pub delete_range_news: unsafe extern "C" fn(Vehicle),
    pub handle_breakdown: unsafe extern "C" fn(Vehicle),
    pub handle_loading: unsafe extern "C" fn(Vehicle, bool),
    pub service_order: unsafe extern "C" fn(Vehicle, u16),
    pub dummy_order: unsafe extern "C" fn(Vehicle),
    pub age_vehicle: unsafe extern "C" fn(Vehicle),
    pub economy_age: unsafe extern "C" fn(Vehicle),
    pub decrease_value: unsafe extern "C" fn(Vehicle),
    pub check_orders: unsafe extern "C" fn(Vehicle),
    pub check_breakdown: unsafe extern "C" fn(Vehicle),
    pub running_cost: unsafe extern "C" fn(Vehicle) -> i64,
    pub subtract_cost: unsafe extern "C" fn(Vehicle, i64),
    pub dirty_lists: unsafe extern "C" fn(Vehicle),
    pub next_stopping_station: unsafe extern "C" fn(Vehicle) -> u16,
    pub remove_depot_orders: unsafe extern "C" fn(u16),
    pub assert_flying: unsafe extern "C" fn(Vehicle),
    pub invalid_movement: unsafe extern "C" fn(Vehicle),
    pub invalid_position: unsafe extern "C" fn(Vehicle, *const c_void),
    pub invalid_scheme: unsafe extern "C" fn(Vehicle),
    pub unreachable: unsafe extern "C" fn(Vehicle),
    pub speed_property: unsafe extern "C" fn(Vehicle) -> u32,
    pub cargo_age_property: unsafe extern "C" fn(Vehicle) -> u32,
    pub range_property: unsafe extern "C" fn(Vehicle) -> u32,
    pub start_sound: unsafe extern "C" fn(Vehicle) -> bool,
    pub touchdown_sound: unsafe extern "C" fn(Vehicle) -> bool,
    pub rotor_image_if_changed: unsafe extern "C" fn(Vehicle) -> bool,
    pub update_rotor_image: unsafe extern "C" fn(Vehicle),
    pub process_orders: unsafe extern "C" fn(Vehicle),
    pub enter_depot: unsafe extern "C" fn(Vehicle),
    pub vehicle_crash: unsafe extern "C" fn(Vehicle, bool) -> u32,
    pub delete_aircraft: unsafe extern "C" fn(Vehicle),
    pub send_to_depot: unsafe extern "C" fn(Vehicle, bool) -> bool,
    pub sample_count: unsafe extern "C" fn() -> u32,
    pub helicopter_sound: unsafe extern "C" fn() -> u32,
    pub explosion_sound: unsafe extern "C" fn() -> u32,
    pub skid_sound: unsafe extern "C" fn() -> u32,
    pub ticks_per_year: unsafe extern "C" fn() -> u32,
    pub fta_blocks: unsafe extern "C" fn(*const c_void) -> u64,
    pub fta_heading: unsafe extern "C" fn(*const c_void) -> u8,
    pub fta_next_position: unsafe extern "C" fn(*const c_void) -> u8,
    pub fta_next: unsafe extern "C" fn(*const c_void) -> *const c_void,
    pub block_node: unsafe extern "C" fn(*const c_void) -> BlockNode,
    pub route_node: unsafe extern "C" fn(*const c_void) -> RouteNode,
    pub block_choice: unsafe extern "C" fn(*const c_void) -> BlockChoice,
}
struct World<'a> {
    leaves: &'a Leaves,
}
const INVALID_STATION: u32 = 65535;
const OWNER_NONE: i64 = 16;
const INVALID_TILE: u32 = u32::MAX;
const HIDDEN: i64 = 1;
const STOPPED: i64 = 2;
const BROKEN: i64 = 64;
const CRASHED: i64 = 128;
const HANGAR: u8 = 1;
const TAKEOFF: u8 = 10;
const STARTTAKEOFF: u8 = 11;
const ENDTAKEOFF: u8 = 12;
const HELITAKEOFF: u8 = 13;
const FLYING: u8 = 14;
const LANDING: u8 = 15;
const ENDLANDING: u8 = 16;
const HELILANDING: u8 = 17;
const HELIENDLANDING: u8 = 18;
const NOTHING: u64 = 1 << 30;
const ZEPPELIN: u64 = 1 << 62;
const CLOSED: u64 = 1 << 63;
const NO_CLAMP: i64 = 1;
const TAKING_OFF: i64 = 2;
const SLOW_TURN: i64 = 4;
const LAND: i64 = 8;
const EXACT: i64 = 16;
const BRAKE: i64 = 32;
const HELI_RAISE: i64 = 64;
const HELI_LOWER: i64 = 128;
const HOLD: i64 = 256;
macro_rules! private_fields {
    ($(($get:ident,$set:ident,$field:ident,$type:ty)),* $(,)?)=>{$(
        fn $get(&self,id:Vehicle)->$type {
            let ptr=id.state;
            // SAFETY: Live shell owns a fixed allocation; only this scalar is read.
            unsafe { (&raw const (*ptr).$field).read() }
        }
        fn $set(&self,id:Vehicle,value:$type) {
            let ptr=id.state;
            // SAFETY: Serial game-thread scalar access; no reference spans callbacks.
            unsafe { (&raw mut (*ptr).$field).write(value); }
        }
    )*};
}
/* These macros emit one named synchronous function for each typed callback.
 * Widening preserves C++ integer promotions; narrowing occurs at field writes.
 * Neither macro selects an operation at runtime or copies the callback table. */
macro_rules! scalar_services {
    ($($name:ident($receiver:ident; $($arg:ident : $ty:ty),*) => $call:expr;)*) => {$(
        fn $name(&$receiver, $($arg:$ty),*) -> i64 {
            // SAFETY: No canonical owner borrow survives this noexcept callback.
            i64::from(unsafe { $call })
        }
    )*};
}
macro_rules! direct_services {
    ($($name:ident($receiver:ident; $($arg:ident : $ty:ty),*) -> $ret:ty => $call:expr;)*) => {$(
        fn $name(&$receiver, $($arg:$ty),*) -> $ret {
            // SAFETY: No canonical owner borrow survives this noexcept callback.
            unsafe { $call }
        }
    )*};
}
impl World<'_> {
    scalar_services! {
        subtype(self; v: Vehicle) => (self.leaves.subtype)(v);
        x(self; v: Vehicle) => (self.leaves.x)(v);
        y(self; v: Vehicle) => (self.leaves.y)(v);
        z(self; v: Vehicle) => (self.leaves.z)(v);
        tile(self; v: Vehicle) => (self.leaves.tile)(v);
        direction(self; v: Vehicle) => (self.leaves.direction)(v);
        tick_counter(self; v: Vehicle) => (self.leaves.tick_counter)(v);
        owner(self; v: Vehicle) => (self.leaves.owner)(v);
        vehicle_status(self; v: Vehicle) => (self.leaves.vehicle_status)(v);
        current_speed(self; v: Vehicle) => (self.leaves.current_speed)(v);
        subspeed(self; v: Vehicle) => (self.leaves.subspeed)(v);
        progress(self; v: Vehicle) => (self.leaves.progress)(v);
        acceleration(self; v: Vehicle) => (self.leaves.acceleration)(v);
        maximum_speed(self; v: Vehicle) => (self.leaves.maximum_speed)(v);
        order_type(self; v: Vehicle) => (self.leaves.order_type)(v);
        order_destination(self; v: Vehicle) => (self.leaves.order_destination)(v);
        running_ticks(self; v: Vehicle) => (self.leaves.running_ticks)(v);
        order_time(self; v: Vehicle) => (self.leaves.order_time)(v);
        day_counter(self; v: Vehicle) => (self.leaves.day_counter)(v);
        last_station(self; v: Vehicle) => (self.leaves.last_station)(v);
        map_size_x(self; ) => (self.leaves.map_size_x)();
        map_max_x(self; ) => (self.leaves.map_max_x)();
        map_max_y(self; ) => (self.leaves.map_max_y)();
        plane_speed(self; ) => (self.leaves.plane_speed)();
        no_jetcrash(self; ) => (self.leaves.no_jetcrash)();
        plane_crashes(self; ) => (self.leaves.plane_crashes)();
        service_at_helipad(self; ) => (self.leaves.service_at_helipad)();
        disaster_sound(self; ) => (self.leaves.disaster_sound)();
        economy_date(self; ) => (self.leaves.economy_date)();
        calendar_date(self; ) => (self.leaves.calendar_date)();
        airport_tile(self; s: *const c_void) => (self.leaves.airport_tile)(s);
        station_tile(self; s: *const c_void) => (self.leaves.station_tile)(s);
        rotation(self; s: *const c_void) => (self.leaves.rotation)(s);
        airport_width(self; s: *const c_void) => (self.leaves.airport_width)(s);
        airport_height(self; s: *const c_void) => (self.leaves.airport_height)(s);
        airport_type(self; s: *const c_void) => (self.leaves.airport_type)(s);
        station_owner(self; s: *const c_void) => (self.leaves.station_owner)(s);
        has_hangar(self; s: *const c_void) => (self.leaves.has_hangar)(s);
        has_airport(self; s: *const c_void) => (self.leaves.has_airport)(s);
        had_vehicle(self; s: *const c_void) => (self.leaves.had_vehicle)(s);
        airport_elements(self; airport: *const c_void) => (self.leaves.airport_elements)(airport);
        helipads(self; airport: *const c_void) => (self.leaves.helipads)(airport);
        airport_flags(self; airport: *const c_void) => (self.leaves.airport_flags)(airport);
        airport_delta_z(self; airport: *const c_void) => (self.leaves.airport_delta_z)(airport);
        engine_speed(self; v: Vehicle) => (self.leaves.engine_speed)(v);
        engine_subtype(self; v: Vehicle) => (self.leaves.engine_subtype)(v);
        engine_sound(self; v: Vehicle) => (self.leaves.engine_sound)(v);
        engine_reliability(self; v: Vehicle) => (self.leaves.engine_reliability)(v);
        vehicle_type(self; v: Vehicle) => (self.leaves.vehicle_type)(v);
        slope(self; x: i64, y: i64) => (self.leaves.slope)(x as i32, y as i32);
        tile_height(self; tile: i64) => (self.leaves.tile_height)(tile as u32);
        airport_entry(self; ap: *const c_void, direction: i64) => (self.leaves.airport_entry)(ap, direction as u8);
        direction_towards(self; v: Vehicle, x: i64, y: i64) => (self.leaves.direction_towards)(v, x as i32, y as i32);
        hangar_height(self; station: i64) => (self.leaves.hangar_height)(station as u16);
        terminal_count(self; ap: *const c_void, index: i64) => (self.leaves.terminal_count)(ap, index as u32);
        hangar_exit(self; v: Vehicle) => (self.leaves.hangar_exit)(v);
        can_use_station(self; v: Vehicle, station: i64) => (self.leaves.can_use_station)(v, station as u16);
        service_interval(self; v: Vehicle) => (self.leaves.service_interval)(v);
        needs_service(self; v: Vehicle) => (self.leaves.needs_service)(v);
        chain_in_depot(self; v: Vehicle) => (self.leaves.chain_in_depot)(v);
        waiting_unbunching(self; v: Vehicle) => (self.leaves.waiting_unbunching)(v);
        nearest_depot_order(self; v: Vehicle) => (self.leaves.nearest_depot_order)(v);
        part_of_orders(self; v: Vehicle) => (self.leaves.part_of_orders)(v);
        next_station(self; last: i64) => (self.leaves.next_station)(last as u16);
        next_stopping_station(self; vehicle: Vehicle) => (self.leaves.next_stopping_station)(vehicle);
        speed_property(self; v: Vehicle) => (self.leaves.speed_property)(v);
        cargo_age_property(self; v: Vehicle) => (self.leaves.cargo_age_property)(v);
        range_property(self; v: Vehicle) => (self.leaves.range_property)(v);
        start_sound(self; v: Vehicle) => (self.leaves.start_sound)(v);
        touchdown_sound(self; v: Vehicle) => (self.leaves.touchdown_sound)(v);
        rotor_image_if_changed(self; v: Vehicle) => (self.leaves.rotor_image_if_changed)(v);
        vehicle_crash(self; v: Vehicle, flooded: i64) => (self.leaves.vehicle_crash)(v, flooded != 0);
        send_to_depot(self; v: Vehicle, service: i64) => (self.leaves.send_to_depot)(v, service != 0);
        sample_count(self; ) => (self.leaves.sample_count)();
        helicopter_sound(self; ) => (self.leaves.helicopter_sound)();
        explosion_sound(self; ) => (self.leaves.explosion_sound)();
        skid_sound(self; ) => (self.leaves.skid_sound)();
        ticks_per_year(self; ) => (self.leaves.ticks_per_year)();
    }
    direct_services! {
        set_x(self; v: Vehicle, value: i64) -> () => (self.leaves.set_x)(v, value as i32);
        set_y(self; v: Vehicle, value: i64) -> () => (self.leaves.set_y)(v, value as i32);
        set_z(self; v: Vehicle, value: i64) -> () => (self.leaves.set_z)(v, value as i32);
        set_tile(self; v: Vehicle, value: i64) -> () => (self.leaves.set_tile)(v, value as u32);
        set_direction(self; v: Vehicle, value: i64) -> () => (self.leaves.set_direction)(v, value as u8);
        set_tick_counter(self; v: Vehicle, value: i64) -> () => (self.leaves.set_tick_counter)(v, value as u8);
        set_vehicle_status(self; v: Vehicle, value: i64) -> () => (self.leaves.set_vehicle_status)(v, value as u8);
        set_current_speed(self; v: Vehicle, value: i64) -> () => (self.leaves.set_current_speed)(v, value as u16);
        set_subspeed(self; v: Vehicle, value: i64) -> () => (self.leaves.set_subspeed)(v, value as u8);
        set_progress(self; v: Vehicle, value: i64) -> () => (self.leaves.set_progress)(v, value as u8);
        set_maximum_speed(self; v: Vehicle, value: i64) -> () => (self.leaves.set_maximum_speed)(v, value as u16);
        set_breakdown_counter(self; v: Vehicle, value: i64) -> () => (self.leaves.set_breakdown_counter)(v, value as u8);
        set_running_ticks(self; v: Vehicle, value: i64) -> () => (self.leaves.set_running_ticks)(v, value as u8);
        set_order_time(self; v: Vehicle, value: i64) -> () => (self.leaves.set_order_time)(v, value as i32);
        set_day_counter(self; v: Vehicle, value: i64) -> () => (self.leaves.set_day_counter)(v, value as u8);
        profit(self; v: Vehicle) -> i64 => (self.leaves.profit)(v);
        set_profit(self; v: Vehicle, value: i64) -> () => (self.leaves.set_profit)(v, value as i64);
        set_last_station(self; v: Vehicle, value: i64) -> () => (self.leaves.set_last_station)(v, value as u16);
        set_economy_service(self; v: Vehicle, value: i64) -> () => (self.leaves.set_economy_service)(v, value as i32);
        set_calendar_service(self; v: Vehicle, value: i64) -> () => (self.leaves.set_calendar_service)(v, value as i32);
        set_breakdowns(self; v: Vehicle, value: i64) -> () => (self.leaves.set_breakdowns)(v, value as u8);
        set_reliability(self; v: Vehicle, value: i64) -> () => (self.leaves.set_reliability)(v, value as u16);
        set_cargo_age(self; v: Vehicle, value: i64) -> () => (self.leaves.set_cargo_age)(v, value as u16);
        airport_fta(self; s: *const c_void) -> *const c_void => (self.leaves.airport_fta)(s);
        airport_blocks(self; s: *const c_void) -> *mut u64 => (self.leaves.airport_blocks)(s);
        dummy_airport(self; ) -> *const c_void => (self.leaves.dummy_airport)();
        node(self; airport: *const c_void, pos: u8) -> *const c_void => (self.leaves.node)(airport, pos);
        fta(self; node: *const c_void) -> Node => (self.leaves.fta)(node);
        block_choice(self; node: *const c_void) -> BlockChoice => (self.leaves.block_choice)(node);
        route_node(self; node: *const c_void) -> RouteNode => (self.leaves.route_node)(node);
        block_node(self; node: *const c_void) -> BlockNode => (self.leaves.block_node)(node);
        fta_blocks(self; node: *const c_void) -> u64 => (self.leaves.fta_blocks)(node);
        fta_heading(self; node: *const c_void) -> u8 => (self.leaves.fta_heading)(node);
        fta_next_position(self; node: *const c_void) -> u8 => (self.leaves.fta_next_position)(node);
        fta_next(self; node: *const c_void) -> *const c_void => (self.leaves.fta_next)(node);
        random(self; ) -> u32 => (self.leaves.random)();
        update_position(self; vehicle: Vehicle) -> () => (self.leaves.update_position)(vehicle);
        rotor_image(self; vehicle: Vehicle) -> () => (self.leaves.rotor_image)(vehicle);
        copy_sprite(self; vehicle: Vehicle, source: Vehicle) -> () => (self.leaves.copy_sprite)(vehicle, source);
        position_viewport(self; vehicle: Vehicle) -> () => (self.leaves.position_viewport)(vehicle);
        create_effect(self; vehicle: Vehicle, kind: i64, x: i64, y: i64, z: i64) -> () => (self.leaves.create_effect)(vehicle, kind as u8, x as i32, y as i32, z as i32);
        dirty_start_stop(self; vehicle: Vehicle) -> () => (self.leaves.dirty_start_stop)(vehicle);
        play_sound(self; vehicle: Vehicle, sound: i64) -> () => (self.leaves.play_sound)(vehicle, sound as u16);
        truncate_cargo(self; vehicle: Vehicle) -> () => (self.leaves.truncate_cargo)(vehicle);
        crash_news(self; vehicle: Vehicle, station: i64, victims: i64) -> () => (self.leaves.crash_news)(vehicle, station as u16, victims as u32);
        landing_rating(self; station_id: i64, cargo: i64) -> () => (self.leaves.landing_rating)(station_id as u16, cargo as u8);
        free_order(self; vehicle: Vehicle) -> () => (self.leaves.free_order)(vehicle);
        service_in_depot(self; vehicle: Vehicle) -> () => (self.leaves.service_in_depot)(vehicle);
        leave_unbunching(self; vehicle: Vehicle) -> () => (self.leaves.leave_unbunching)(vehicle);
        dirty_depot(self; vehicle: Vehicle) -> () => (self.leaves.dirty_depot)(vehicle);
        first_arrival(self; vehicle: Vehicle, station: i64) -> () => (self.leaves.first_arrival)(vehicle, station as u16);
        begin_loading(self; vehicle: Vehicle) -> () => (self.leaves.begin_loading)(vehicle);
        dirty_details(self; vehicle: Vehicle) -> () => (self.leaves.dirty_details)(vehicle);
        update_delta(self; vehicle: Vehicle) -> () => (self.leaves.update_delta)(vehicle);
        touchdown_animation(self; vehicle: Vehicle, station: i64) -> () => (self.leaves.touchdown_animation)(vehicle, station as u16);
        destination_too_far(self; vehicle: Vehicle) -> () => (self.leaves.destination_too_far)(vehicle);
        delete_range_news(self; vehicle: Vehicle) -> () => (self.leaves.delete_range_news)(vehicle);
        handle_breakdown(self; vehicle: Vehicle) -> () => (self.leaves.handle_breakdown)(vehicle);
        handle_loading(self; vehicle: Vehicle, second: i64) -> () => (self.leaves.handle_loading)(vehicle, second != 0);
        service_order(self; vehicle: Vehicle, station: i64) -> () => (self.leaves.service_order)(vehicle, station as u16);
        dummy_order(self; vehicle: Vehicle) -> () => (self.leaves.dummy_order)(vehicle);
        age_vehicle(self; vehicle: Vehicle) -> () => (self.leaves.age_vehicle)(vehicle);
        economy_age(self; vehicle: Vehicle) -> () => (self.leaves.economy_age)(vehicle);
        decrease_value(self; vehicle: Vehicle) -> () => (self.leaves.decrease_value)(vehicle);
        check_orders(self; vehicle: Vehicle) -> () => (self.leaves.check_orders)(vehicle);
        check_breakdown(self; vehicle: Vehicle) -> () => (self.leaves.check_breakdown)(vehicle);
        running_cost(self; vehicle: Vehicle) -> i64 => (self.leaves.running_cost)(vehicle);
        subtract_cost(self; vehicle: Vehicle, cost: i64) -> () => (self.leaves.subtract_cost)(vehicle, cost);
        dirty_lists(self; vehicle: Vehicle) -> () => (self.leaves.dirty_lists)(vehicle);
        remove_depot_orders(self; station_id: i64) -> () => (self.leaves.remove_depot_orders)(station_id as u16);
        assert_flying(self; vehicle: Vehicle) -> () => (self.leaves.assert_flying)(vehicle);
        invalid_movement(self; vehicle: Vehicle) -> () => (self.leaves.invalid_movement)(vehicle);
        invalid_position(self; vehicle: Vehicle, ap: *const c_void) -> () => (self.leaves.invalid_position)(vehicle, ap);
        invalid_scheme(self; vehicle: Vehicle) -> () => (self.leaves.invalid_scheme)(vehicle);
        unreachable(self; vehicle: Vehicle) -> () => (self.leaves.unreachable)(vehicle);
        update_rotor_image(self; v: Vehicle) -> () => (self.leaves.update_rotor_image)(v);
        process_orders(self; v: Vehicle) -> () => (self.leaves.process_orders)(v);
        enter_depot(self; v: Vehicle) -> () => (self.leaves.enter_depot)(v);
        delete_aircraft(self; v: Vehicle) -> () => (self.leaves.delete_aircraft)(v);
    }

    fn next(&self, v: Vehicle) -> Vehicle {
        unsafe { (self.leaves.next)(v) }
    }
    fn station(&self, id: u32) -> *const c_void {
        unsafe { (self.leaves.station)(id as u16) }
    }
    fn moving(&self, airport: *const c_void, pos: i64) -> Movement {
        // SAFETY: Synchronous noexcept service; no owner borrow spans this call.
        {
            let m = unsafe { (self.leaves.moving)(airport, pos as u8) };
            Movement {
                x: i64::from(m.x),
                y: i64::from(m.y),
                flags: i64::from(m.flags),
                direction: i64::from(m.direction),
            }
        }
    }
    fn new_position(&self, v: Vehicle) -> NewPosition {
        // SAFETY: Synchronous noexcept service; no owner borrow spans this call.
        {
            let p = unsafe { (self.leaves.new_position)(v) };
            NewPosition {
                x: i64::from(p.x),
                y: i64::from(p.y),
                tile: p.tile,
            }
        }
    }
    fn next_aircraft(&self, last: u32) -> Vehicle {
        unsafe { (self.leaves.next_aircraft)(last) }
    }
    fn station_rating(&self, tile: i64, owner: i64, amount: i64, radius: i64) {
        // SAFETY: Synchronous noexcept service; no owner borrow spans this call.
        unsafe {
            (self.leaves.station_rating)(tile as u32, owner as u8, amount as i32, radius as u32);
        }
    }

    fn status(&self, id: Vehicle, mask: i64, on: bool) {
        let old = self.vehicle_status(id);
        self.set_vehicle_status(id, if on { old | mask } else { old & !mask });
    }
    private_fields!(
        (pos, set_pos, pos, u8),
        (previous, set_previous, previous_pos, u8),
        (state, set_state, state, u8),
        (target, set_target, targetairport, u16),
        (lastdir, set_lastdir, last_direction, u8),
        (turns, set_turns, number_consecutive_turns, u8),
        (turn, set_turn, turn_counter, u8),
        (flags, set_flags, flags, u8),
        (crashed, set_crashed, crashed_counter, u16),
        (range, set_range, cached_max_range, u16),
        (range_sqr, set_range_sqr, cached_max_range_sqr, u32)
    );
    fn flag(&self, id: Vehicle, bit: u8, on: bool) {
        let f = self.flags(id);
        self.set_flags(id, if on { f | (1 << bit) } else { f & !(1 << bit) });
    }
    fn target_station(&self, id: Vehicle) -> *const c_void {
        self.station(u32::from(self.target(id)))
    }
    fn valid_airport(&self, id: Vehicle) -> bool {
        let s = self.target_station(id);
        !s.is_null() && self.airport_tile(s) as u32 != INVALID_TILE
    }
    fn airport(&self, id: Vehicle) -> *const c_void {
        let s = self.target_station(id);
        if s.is_null() {
            self.dummy_airport()
        } else {
            self.airport_fta(s)
        }
    }
    fn blocks(&self, station: u32) -> u64 {
        let ptr = self.airport_blocks(self.station(station));
        unsafe { ptr.read() }
    }
    fn set_blocks(&self, station: u32, value: u64) {
        let ptr = self.airport_blocks(self.station(station));
        unsafe {
            ptr.write(value);
        }
    }
    fn reserve(&self, station: u32, bits: u64) {
        self.set_blocks(station, self.blocks(station) | bits);
    }
    fn release(&self, station: u32, bits: u64) {
        self.set_blocks(station, self.blocks(station) & !bits);
    }
    fn tile_x(&self, t: u32) -> i64 {
        i64::from(t & (self.map_size_x() as u32 - 1))
    }
    fn tile_y(&self, t: u32) -> i64 {
        i64::from(t >> (self.map_size_x() as u32).trailing_zeros())
    }
    fn tile_virt(&self, x: i64, y: i64) -> u32 {
        ((y as i32 as u32) >> 4)
            .wrapping_mul(self.map_size_x() as u32)
            .wrapping_add((x as i32 as u32) >> 4)
    }
    fn height(&self, id: Vehicle) -> i64 {
        let x = self.x(id).clamp(0, self.map_max_x() * 16);
        let y = self.y(id).clamp(0, self.map_max_y() * 16);
        self.tile_height(i64::from(self.tile_virt(x, y)))
    }
    fn bounds(&self, id: Vehicle) -> (i64, i64) {
        let mut base = self.height(id);
        if self.subtype(id) == 0 && self.vehicle_type(id) == 3 {
            base += 34;
        }
        if self.direction(id) <= 3 {
            base += 10;
        }
        // C++ cached_max_speed is uint16; arithmetic promotes to signed int.
        base += (20 * (self.maximum_speed(id) / 200) - 90).min(0);
        (base + 120, base + 360)
    }
    fn flight_flags(&self, id: Vehicle, ptr: *mut u8, takeoff: bool) -> i64 {
        let (min, max) = self.bounds(id);
        let middle = (min + max) / 2;
        let mut z = self.z(id);
        // SAFETY: Scalar pointer belongs to the live aircraft/disaster owner for this synchronous access.
        let mut flags = unsafe { ptr.read() };
        if z < min || (flags & 4 != 0 && z < middle) {
            flags |= 4;
            z += if takeoff { 2 } else { 1 };
        } else if !takeoff && (z > max || (flags & 2 != 0 && z > middle)) {
            flags |= 2;
            z -= 1;
        } else if flags & 4 != 0 && z >= middle {
            flags &= !4;
        } else if flags & 2 != 0 && z <= middle {
            flags &= !2;
        }
        // SAFETY: All shared callbacks above completed before this raw scalar write.
        unsafe {
            ptr.write(flags);
        }
        z
    }
    fn flight(&self, id: Vehicle, takeoff: bool) -> i64 {
        let ptr = id.state;
        // SAFETY: Computing a raw field address creates no reference or access scope.
        self.flight_flags(id, unsafe { &raw mut (*ptr).flags }, takeoff)
    }
    fn effect(&self, id: Vehicle, x: i64, y: i64, z: i64, kind: u32) {
        self.create_effect(id, i64::from(kind), x, y, z);
    }
    fn position(&self, id: Vehicle, x: i64, y: i64, z: i64) {
        self.set_x(id, x);
        self.set_y(id, y);
        self.set_z(id, z);
        self.update_position(id);
        if self.subtype(id) == 0 {
            self.rotor_image(id);
        }
        let shadow = self.next(id);
        let safe_x = x.clamp(0, self.map_max_x() * 16);
        let mut safe_y = (y - 1).clamp(0, self.map_max_y() * 16);
        self.set_x(shadow, x);
        self.set_y(shadow, y - ((self.z(id) - self.slope(safe_x, safe_y)) >> 3));
        safe_y = self.y(shadow).clamp(0, self.map_max_y() * 16);
        self.set_z(shadow, self.slope(safe_x, safe_y));
        self.copy_sprite(shadow, id);
        self.position_viewport(shadow);
        let rotor = self.next(shadow);
        if !rotor.handle.is_null() {
            self.set_x(rotor, x);
            self.set_y(rotor, y);
            self.set_z(rotor, z + 5);
            self.position_viewport(rotor);
        }
    }
    fn position_current(&self, id: Vehicle) {
        self.position(id, self.x(id), self.y(id), self.z(id));
    }
    fn enter_hangar(&self, id: Vehicle) {
        self.set_subspeed(id, 0);
        self.set_progress(id, 0);
        let shadow = self.next(id);
        self.status(shadow, HIDDEN, true);
        let rotor = self.next(shadow);
        if !rotor.handle.is_null() {
            self.status(rotor, HIDDEN, true);
            self.set_current_speed(rotor, 0);
        }
        self.position_current(id);
    }
    fn cache(&self, id: Vehicle, update_range: bool) {
        let speed = self.speed_property(id) as u32;
        self.set_maximum_speed(
            id,
            if speed != 0 {
                i64::from(speed.wrapping_mul(128) / 10)
            } else {
                self.engine_speed(id)
            },
        );
        self.set_cargo_age(id, self.cargo_age_property(id));
        let shadow = self.next(id);
        self.set_cargo_age(shadow, self.cargo_age_property(shadow));
        if update_range {
            let range = self.range_property(id) as u16;
            self.set_range(id, range);
            self.set_range_sqr(
                id,
                u32::from(self.range(id)).wrapping_mul(u32::from(self.range(id))),
            );
        }
    }
    fn helicopter_tick(&self, id: Vehicle) {
        let rotor = self.next(self.next(id));
        if self.vehicle_status(rotor) & HIDDEN != 0 {
            return;
        }
        if self.order_type(id) == 3 || self.vehicle_status(id) & STOPPED != 0 {
            if self.current_speed(rotor) != 0 {
                self.set_current_speed(
                    rotor,
                    i64::from((self.current_speed(rotor) as u16).wrapping_add(1)),
                );
                if self.current_speed(rotor) >= 128 && self.state(rotor) == 3 {
                    self.set_current_speed(rotor, 0);
                }
            }
        } else {
            if self.current_speed(rotor) == 0 {
                self.set_current_speed(rotor, 112);
            }
            if self.current_speed(rotor) >= 80 {
                self.set_current_speed(rotor, self.current_speed(rotor) - 1);
            }
        }
        let tick = (self.tick_counter(rotor) as u8).wrapping_add(1);
        self.set_tick_counter(rotor, i64::from(tick));
        let speed = self.current_speed(rotor) >> 4;
        if speed == 0 {
            self.set_state(rotor, 0);
            if self.rotor_image_if_changed(id) != 0 {
                return;
            }
        } else if i64::from(tick) >= speed {
            self.set_tick_counter(rotor, 0);
            let state = self.state(rotor).wrapping_add(1);
            self.set_state(rotor, if state > 3 { 1 } else { state });
            self.update_rotor_image(id);
        } else {
            return;
        }
        self.position_viewport(rotor);
    }
    fn speed(&self, id: Vehicle, mut limit: u32, mut hard: bool) -> u32 {
        let mut spd = (self.acceleration(id) as u32).wrapping_mul(77);
        let factor = self.plane_speed() as u32;
        limit = limit.wrapping_mul(factor);
        if self.vehicle_status(id) & BROKEN != 0 {
            if 320 < limit {
                hard = false;
            }
            limit = limit.min(320);
        }
        if (self.maximum_speed(id) as u32) < limit {
            if (self.current_speed(id) as u32) < limit {
                hard = false;
            }
            limit = self.maximum_speed(id) as u32;
        }
        let old_sub = self.subspeed(id) as u8;
        let sub = old_sub.wrapping_add(spd as u8);
        self.set_subspeed(id, i64::from(sub));
        let cur = self.current_speed(id) as u32;
        if !hard && cur > limit {
            let reduction = ((cur as i32).wrapping_mul(cur as i32) / 16384) / factor as i32;
            limit = cur.wrapping_sub(reduction.max(1) as u32);
        }
        spd = (cur + (spd >> 8) + u32::from(sub < old_sub)).min(limit);
        if spd != cur {
            self.set_current_speed(id, i64::from(spd));
            self.dirty_start_stop(id);
        }
        if factor > 1 {
            spd /= factor;
        }
        if self.direction(id) & 1 == 0 {
            spd = spd.wrapping_mul(3) / 4;
        }
        spd = spd.wrapping_add(self.progress(id) as u32);
        self.set_progress(id, i64::from(spd as u8));
        spd >> 8
    }
    fn entry_point(&self, id: Vehicle, ap: *const c_void, rotation: i64) -> u8 {
        let s = self.target_station(id);
        let tile = if s.is_null() {
            0
        } else if self.airport_tile(s) as u32 != INVALID_TILE {
            self.airport_tile(s) as u32
        } else {
            self.station_tile(s) as u32
        };
        let dx = self.x(id) - self.tile_x(tile) * 16;
        let dy = self.y(id) - self.tile_y(tile) * 16;
        let dir = if dy.abs() < dx.abs() {
            if dx < 0 { 0 } else { 2 }
        } else if dy < 0 {
            3
        } else {
            1
        };
        let rotated = (dir + (0_u8.wrapping_sub(rotation as u8 >> 1) & 3)) & 3;
        self.airport_entry(ap, i64::from(rotated)) as u8
    }
    fn next_airport(&self, id: Vehicle) {
        if matches!(self.order_type(id), 1 | 2) {
            self.set_target(id, self.order_destination(id) as u16);
        }
        let s = self.target_station(id);
        let valid = !s.is_null() && self.airport_tile(s) as u32 != INVALID_TILE;
        let ap = if valid {
            self.airport_fta(s)
        } else {
            self.dummy_airport()
        };
        let rotation = if valid { self.rotation(s) } else { 0 };
        let pos = self.entry_point(id, ap, rotation);
        self.set_pos(id, pos);
        self.set_previous(id, pos);
    }
    fn controller(&self, id: Vehicle) -> bool {
        let s = self.target_station(id);
        let valid = !s.is_null();
        let mut tile = INVALID_TILE;
        let mut rotation = 0;
        let mut sx = 1;
        let mut sy = 1;
        if valid {
            if self.airport_tile(s) as u32 != INVALID_TILE {
                tile = self.airport_tile(s) as u32;
                rotation = self.rotation(s);
                sx = self.airport_width(s);
                sy = self.airport_height(s);
            } else {
                tile = self.station_tile(s) as u32;
            }
        }
        let ap = if tile == INVALID_TILE {
            self.dummy_airport()
        } else {
            self.airport_fta(s)
        };
        if !valid || self.airport_tile(s) as u32 == INVALID_TILE {
            if i64::from(self.pos(id)) >= self.airport_elements(ap) {
                let pos = self.entry_point(id, ap, 0);
                self.set_pos(id, pos);
                self.set_previous(id, pos);
            } else if i64::from(self.target(id)) != self.order_destination(id) {
                self.set_state(id, FLYING);
                self.cache(id, false);
                self.next_airport(id);
                self.position(id, self.x(id), self.y(id), self.flight(id, false));
                return false;
            }
        }
        let mut amd = self.moving(ap, i64::from(self.pos(id)));
        let ox = amd.x;
        let oy = amd.y;
        amd.direction = (amd.direction + rotation) & 7;
        match rotation {
            0 => {}
            2 => {
                amd.x = oy;
                amd.y = i64::from((sy * 16 - ox - 1) as i16);
            }
            4 => {
                amd.x = i64::from((sx * 16 - ox - 1) as i16);
                amd.y = i64::from((sy * 16 - oy - 1) as i16);
            }
            6 => {
                amd.x = i64::from((sx * 16 - oy - 1) as i16);
                amd.y = ox;
            }
            _ => {
                self.unreachable(id);
            }
        }
        let flags = amd.flags;
        let mut x = self.tile_x(tile) * 16;
        let mut y = self.tile_y(tile) * 16;
        if flags & HELI_RAISE != 0 {
            let rotor = self.next(self.next(id));
            if self.current_speed(rotor) > 32 {
                self.set_current_speed(id, 0);
                self.set_current_speed(rotor, self.current_speed(rotor) - 1);
                if self.current_speed(rotor) == 32 && self.start_sound(id) == 0 {
                    let sound = self.engine_sound(id);
                    self.play_sound(
                        id,
                        if sound < self.sample_count() {
                            self.helicopter_sound()
                        } else {
                            sound
                        },
                    );
                }
            } else {
                self.set_current_speed(rotor, 32);
                let count = self.speed(id, 65535, true);
                if count > 0 {
                    self.set_tile(id, 0);
                    let dest = self.bounds(id).0;
                    if self.z(id) >= dest {
                        self.set_current_speed(id, 0);
                        return true;
                    }
                    self.position(
                        id,
                        self.x(id),
                        self.y(id),
                        (self.z(id) + i64::from(count)).min(dest),
                    );
                }
            }
            return false;
        }
        if flags & HELI_LOWER != 0 {
            self.flag(id, 3, true);
            if !valid {
                self.set_state(id, FLYING);
                self.cache(id, false);
                self.next_airport(id);
                return false;
            }
            if self.airport_type(s) != 9 {
                x = self.x(id);
                y = self.y(id);
                tile = self.tile_virt(x, y);
            }
            self.set_tile(id, i64::from(tile));
            let z = self.slope(x, y) + 1 + self.airport_delta_z(ap);
            if z == self.z(id) {
                let rotor = self.next(self.next(id));
                if self.current_speed(rotor) >= 80 {
                    self.flag(id, 3, false);
                    return true;
                }
                self.set_current_speed(rotor, self.current_speed(rotor) + 4);
            } else {
                let count = self.speed(id, 65535, true);
                if count > 0 {
                    let current = self.z(id);
                    let dest = if current > z {
                        (current - i64::from(count)).max(z)
                    } else {
                        (current + i64::from(count)).min(z)
                    };
                    self.position(id, self.x(id), self.y(id), dest);
                }
            }
            return false;
        }
        let dist = ((x + amd.x - self.x(id)).abs() + (y + amd.y - self.y(id)).abs()) as u32;
        if flags & EXACT == 0 && dist <= if flags & SLOW_TURN != 0 { 8 } else { 4 } {
            return true;
        }
        if dist == 0 {
            let difference = (amd.direction - self.direction(id)) & 7;
            if difference == 0 {
                self.set_current_speed(id, 0);
                return true;
            }
            if self.speed(id, 50, true) == 0 {
                return false;
            }
            self.set_direction(
                id,
                (self.direction(id) + if difference > 4 { 7 } else { 1 }) & 7,
            );
            self.set_current_speed(id, self.current_speed(id) >> 1);
            self.position_current(id);
            return false;
        }
        if flags & BRAKE != 0 && self.current_speed(id) > 50 * self.plane_speed() {
            self.maybe_crash(id);
            if self.vehicle_status(id) & CRASHED != 0 {
                return false;
            }
        }
        let mut limit = 50;
        let mut hard = true;
        if flags & NO_CLAMP != 0 {
            limit = 65535;
        }
        if flags & HOLD != 0 {
            limit = 425;
            hard = false;
        }
        if flags & LAND != 0 {
            limit = 230;
            hard = false;
        }
        if flags & BRAKE != 0 {
            limit = 50;
            hard = false;
        }
        let mut count = self.speed(id, limit, hard);
        if count == 0 {
            return false;
        }
        let nudge = count + 3 > dist;
        if self.turn(id) != 0 {
            self.set_turn(id, self.turn(id) - 1);
        }
        loop {
            let px;
            let py;
            let next_tile;
            if nudge || flags & LAND != 0 {
                let vx = self.x(id);
                let vy = self.y(id);
                px = vx + (x + amd.x - vx).signum();
                py = vy + (y + amd.y - vy).signum();
                next_tile = if self.airport_type(s) == 9 {
                    self.airport_tile(s) as u32
                } else {
                    self.tile_virt(px, py)
                };
            } else {
                let newdir = self.direction_towards(id, x + amd.x, y + amd.y);
                if newdir != self.direction(id) {
                    if flags & SLOW_TURN != 0 && self.turns(id) < 8 && self.subtype(id) == 2 {
                        if self.turn(id) == 0 || newdir == i64::from(self.lastdir(id)) {
                            if newdir == i64::from(self.lastdir(id)) {
                                self.set_turns(id, 0);
                            } else {
                                self.set_turns(id, self.turns(id).wrapping_add(1));
                            }
                            self.set_turn(id, (2 * self.plane_speed()) as u8);
                            self.set_lastdir(id, self.direction(id) as u8);
                            self.set_direction(id, newdir);
                        }
                        let gp = self.new_position(id);
                        px = gp.x;
                        py = gp.y;
                        next_tile = gp.tile;
                    } else {
                        self.set_current_speed(id, self.current_speed(id) >> 1);
                        self.set_direction(id, newdir);
                        px = self.x(id);
                        py = self.y(id);
                        next_tile = self.tile(id) as u32;
                    }
                } else {
                    self.set_turns(id, 0);
                    let gp = self.new_position(id);
                    px = gp.x;
                    py = gp.y;
                    next_tile = gp.tile;
                }
            }
            self.set_tile(id, i64::from(next_tile));
            if flags & (TAKING_OFF | SLOW_TURN | LAND) != 0 {
                self.set_tile(id, 0);
            }
            let mut z = self.z(id);
            if flags & TAKING_OFF != 0 {
                z = self.flight(id, true);
            } else if flags & HOLD != 0 {
                if z > self.height(id) + if self.subtype(id) == 0 { 184 } else { 150 } {
                    z -= 1;
                }
            } else if flags & (SLOW_TURN | NO_CLAMP) == SLOW_TURN | NO_CLAMP {
                z = self.flight(id, false);
            }
            let mut airport_z = self.z(id);
            if flags & (LAND | BRAKE) != 0 && valid {
                airport_z = self.hangar_height(i64::from(self.target(id))) + 1;
            }
            if flags & LAND != 0 {
                if self.blocks(u32::from(self.target(id))) & ZEPPELIN != 0 {
                    self.set_state(id, FLYING);
                    self.cache(id, false);
                    self.position(id, px, py, self.flight(id, false));
                    self.set_pos(id, self.previous(id));
                    count -= 1;
                    if count == 0 {
                        break;
                    }
                    continue;
                }
                if self.airport_tile(s) as u32 == INVALID_TILE {
                    self.set_state(id, FLYING);
                    self.cache(id, false);
                    self.next_airport(id);
                    self.position(id, px, py, self.flight(id, false));
                    count -= 1;
                    if count == 0 {
                        break;
                    }
                    continue;
                }
                let t = i64::from(dist.wrapping_sub(4).max(1));
                let delta = z - airport_z;
                if delta >= t {
                    z -= (z - airport_z + t - 1) / t;
                }
                if z < airport_z {
                    z = airport_z;
                }
            }
            if flags & BRAKE != 0 {
                z += (airport_z - z).signum();
            }
            self.position(id, px, py, z);
            count -= 1;
            if count == 0 {
                break;
            }
        }
        false
    }
    fn crashed_tick(&self, id: Vehicle) -> bool {
        self.set_crashed(id, self.crashed(id).wrapping_add(3));
        let valid = self.valid_airport(id);
        if self.crashed(id) < 500 && !valid && self.crashed(id).is_multiple_of(3) {
            let z = self.slope(
                self.x(id).clamp(0, self.map_max_x() * 16),
                self.y(id).clamp(0, self.map_max_y() * 16),
            );
            self.set_z(id, self.z(id) - 1);
            if self.z(id) <= z {
                self.set_crashed(id, 500);
                self.set_z(id, z + 1);
            } else {
                self.set_crashed(id, 0);
            }
            self.position_current(id);
        }
        if self.crashed(id) < 650 {
            let r = self.random();
            if chance16_i(1, 32, r) {
                let delta = [7, 0, 0, 1][((r >> 16) & 3) as usize];
                self.set_direction(id, (self.direction(id) + delta) & 7);
                self.position_current(id);
                let r = self.random();
                self.effect(
                    id,
                    i64::from((r & 15).wrapping_sub(4) as i32),
                    i64::from(((r >> 4) & 15).wrapping_sub(4) as i32),
                    i64::from((r >> 8) & 15),
                    7,
                );
            }
        } else if self.crashed(id) >= 10000 {
            if valid {
                self.release(u32::from(self.target(id)), (1 << 8) | (1 << 29));
            }
            self.delete_aircraft(id);
            return false;
        }
        true
    }
    fn smoke(&self, id: Vehicle, mode: bool) {
        if self.vehicle_status(id) & BROKEN == 0 {
            return;
        }
        if self.current_speed(id) < 10 {
            self.status(id, BROKEN, false);
            self.set_breakdown_counter(id, 0);
            return;
        }
        if !mode && self.tick_counter(id) & 15 == 0 {
            let (x, y) = [
                (5, 5),
                (6, 0),
                (5, -5),
                (0, -6),
                (-5, -5),
                (-6, 0),
                (-5, 5),
                (0, 6),
            ][self.direction(id) as usize];
            self.effect(id, x, y, 2, 10);
        }
    }
    fn crash(&self, id: Vehicle, flooded: bool) -> u32 {
        let victims = (self.vehicle_crash(id, i64::from(flooded)) as u32).wrapping_add(2);
        self.set_crashed(id, if flooded { 9000 } else { 0 });
        victims
    }
    fn crash_airplane(&self, id: Vehicle) {
        self.effect(id, 4, 4, 8, 5);
        let victims = self.crash(id, false);
        self.truncate_cargo(id);
        self.truncate_cargo(self.next(id));
        let station = if self.valid_airport(id) {
            u32::from(self.target(id))
        } else {
            INVALID_STATION
        };
        self.crash_news(id, i64::from(station), i64::from(victims));
        let tile = self.tile_virt(
            self.x(id).clamp(0, self.map_max_x() * 16),
            self.y(id).clamp(0, self.map_max_y() * 16),
        );
        self.station_rating(i64::from(tile), self.owner(id), -160, 30);
        if self.disaster_sound() != 0 {
            self.play_sound(id, self.explosion_sound());
        }
    }
    fn maybe_crash(&self, id: Vehicle) {
        let ap = self.airport(id);
        let prob = if self.airport_flags(ap) & 4 != 0
            && self.engine_subtype(id) & 2 != 0
            && self.no_jetcrash() == 0
        {
            3276
        } else {
            let setting = self.plane_crashes();
            if setting == 0 {
                return;
            }
            (0x4000_u32.wrapping_shl(setting as u32)) / 1500
        };
        if self.random() & ((1 << 22) - 1) > prob {
            return;
        }
        for cargo in 0..64 {
            self.landing_rating(i64::from(self.target(id)), cargo);
        }
        self.crash_airplane(id);
    }
    fn missing_orders(&self, id: Vehicle) {
        if !self.valid_airport(id) {
            if self.send_to_depot(id, 0) != 0 {
                self.crash_airplane(id);
            }
        } else if self.order_type(id) != 2 {
            self.free_order(id);
        }
    }
    fn leave_hangar(&self, id: Vehicle, direction: i64) {
        self.set_current_speed(id, 0);
        self.set_subspeed(id, 0);
        self.set_progress(id, 0);
        self.set_direction(id, direction);
        self.status(id, HIDDEN, false);
        let shadow = self.next(id);
        self.status(shadow, HIDDEN, false);
        let rotor = self.next(shadow);
        if !rotor.handle.is_null() {
            self.status(rotor, HIDDEN, false);
            self.set_current_speed(rotor, 80);
        }
        self.service_in_depot(id);
        self.leave_unbunching(id);
        self.position_current(id);
        self.dirty_depot(id);
    }
    fn terminal(&self, id: Vehicle) {
        if self.order_type(id) == 2 {
            return;
        }
        let station = u32::from(self.target(id));
        self.set_last_station(id, i64::from(station));
        if self.had_vehicle(self.station(station)) & 16 == 0 {
            self.first_arrival(id, i64::from(station));
        }
        self.begin_loading(id);
    }
    fn has_block(&self, id: Vehicle, current: *const c_void, ap: *const c_void) -> bool {
        let reference = self.node(ap, self.pos(id));
        let c = self.block_node(current);
        let next = self.fta_blocks(self.node(ap, c.next_position));
        if self.fta_blocks(self.node(ap, c.position)) != next {
            let mut blocks = next;
            if current != reference && c.blocks != NOTHING {
                blocks |= c.blocks;
            }
            if self.blocks(u32::from(self.target(id))) & blocks != 0 {
                self.set_current_speed(id, 0);
                self.set_subspeed(id, 0);
                return true;
            }
        }
        false
    }
    fn set_block(&self, id: Vehicle, current: *const c_void, ap: *const c_void) -> bool {
        let c = self.fta(current);
        let next = self.fta_blocks(self.node(ap, c.next_position));
        let reference = self.node(ap, self.pos(id));
        if (self.fta_blocks(self.node(ap, c.position)) & next) != next {
            let mut blocks = next;
            let mut cursor = if current == reference {
                c.next
            } else {
                current
            };
            while !cursor.is_null() {
                let node = self.block_choice(cursor);
                if node.heading == c.heading && node.blocks != 0 {
                    blocks |= node.blocks;
                    break;
                }
                cursor = node.next;
            }
            if c.blocks == next {
                blocks ^= next;
            }
            let station = u32::from(self.target(id));
            if self.blocks(station) & blocks != 0 {
                self.set_current_speed(id, 0);
                self.set_subspeed(id, 0);
                return false;
            }
            if next != NOTHING {
                self.reserve(station, blocks);
            }
        }
        true
    }
    fn free_terminal(&self, id: Vehicle, start: u8, end: u8) -> bool {
        let states = [2, 3, 4, 5, 6, 7, 19, 20, 8, 9, 21];
        let bits = [0, 1, 2, 3, 4, 5, 22, 23, 6, 7, 24];
        let station = u32::from(self.target(id));
        for i in start..end {
            let block = 1 << bits[usize::from(i)];
            if self.blocks(station) & block == 0 {
                self.set_state(id, states[usize::from(i)]);
                self.reserve(station, block);
                return true;
            }
        }
        false
    }
    fn find_terminal(&self, id: Vehicle, ap: *const c_void) -> bool {
        let groups = self.terminal_count(ap, 0);
        if groups > 1 {
            let station = u32::from(self.target(id));
            let mut cursor = self.fta_next(self.node(ap, self.pos(id)));
            while !cursor.is_null() {
                let node = self.block_choice(cursor);
                if node.heading != 255 {
                    return false;
                }
                if self.blocks(station) & node.blocks == 0 {
                    let group = u32::from(self.fta_next_position(cursor)) + 1;
                    let mut start = 0;
                    for i in 1..group {
                        start += self.terminal_count(ap, i64::from(i));
                    }
                    let end = start + self.terminal_count(ap, i64::from(group));
                    if self.free_terminal(id, start as u8, end as u8) {
                        return true;
                    }
                }
                cursor = node.next;
            }
        }
        let mut total = 0;
        for i in (1..=groups).rev() {
            total += self.terminal_count(ap, i);
        }
        self.free_terminal(id, 0, total as u8)
    }
    fn find_helipad(&self, id: Vehicle, ap: *const c_void) -> bool {
        let pads = self.helipads(ap) as u8;
        if pads == 0 {
            self.find_terminal(id, ap)
        } else {
            self.free_terminal(id, 8, pads.wrapping_add(8))
        }
    }
    fn state_handler(&self, id: Vehicle, ap: *const c_void) {
        match self.state(id) {
            HANGAR => {
                if self.previous(id) != self.pos(id) {
                    self.enter_depot(id);
                    self.set_state(id, self.fta_heading(self.node(ap, self.pos(id))));
                    return;
                }
                if self.order_type(id) == 2 && self.vehicle_status(id) & STOPPED != 0 {
                    self.free_order(id);
                    return;
                }
                if self.waiting_unbunching(id) != 0 {
                    return;
                }
                if !matches!(self.order_type(id), 1 | 2) {
                    return;
                }
                if self.order_type(id) == 2
                    && self.order_destination(id) == i64::from(self.target(id))
                {
                    self.enter_depot(id);
                    return;
                }
                if self.has_block(id, self.node(ap, self.pos(id)), ap) {
                    return;
                }
                if self.order_destination(id) == i64::from(self.target(id)) {
                    if self.subtype(id) == 0 {
                        if !self.find_helipad(id, ap) {
                            return;
                        }
                    } else if !self.find_terminal(id, ap) {
                        return;
                    }
                } else {
                    self.set_state(
                        id,
                        if self.subtype(id) == 0 {
                            HELITAKEOFF
                        } else {
                            TAKEOFF
                        },
                    );
                }
                let dir = self.hangar_exit(id);
                self.leave_hangar(id, dir);
                self.airport_move(id, ap);
            }
            2..=9 | 19..=21 => {
                if self.previous(id) != self.pos(id) {
                    self.terminal(id);
                    self.set_state(id, self.fta_heading(self.node(ap, self.pos(id))));
                    if self.service_at_helipad() != 0
                        && self.subtype(id) == 0
                        && self.helipads(ap) > 0
                    {
                        self.set_economy_service(id, self.economy_date());
                        self.set_calendar_service(id, self.calendar_date());
                        self.set_breakdowns(id, 0);
                        self.set_reliability(id, self.engine_reliability(id));
                        self.dirty_details(id);
                    }
                    return;
                }
                if self.order_type(id) == 0 {
                    return;
                }
                if self.has_block(id, self.node(ap, self.pos(id)), ap) {
                    return;
                }
                let hangar = match self.order_type(id) {
                    1 => false,
                    2 => self.order_destination(id) == i64::from(self.target(id)),
                    7 => return,
                    _ => {
                        self.free_order(id);
                        true
                    }
                };
                self.set_state(
                    id,
                    if hangar && self.has_hangar(self.target_station(id)) != 0 {
                        HANGAR
                    } else if self.subtype(id) == 0 {
                        HELITAKEOFF
                    } else {
                        TAKEOFF
                    },
                );
                self.airport_move(id, ap);
            }
            TAKEOFF => {
                if self.start_sound(id) == 0 {
                    self.play_sound(id, self.engine_sound(id));
                }
                self.set_state(id, STARTTAKEOFF);
            }
            STARTTAKEOFF => {
                self.set_state(id, ENDTAKEOFF);
                self.update_delta(id);
            }
            ENDTAKEOFF => {
                self.set_state(id, FLYING);
                self.next_airport(id);
            }
            HELITAKEOFF => {
                self.set_state(id, FLYING);
                self.update_delta(id);
                self.next_airport(id);
                if self.needs_service(id) != 0 {
                    self.send_to_depot(id, 1);
                }
            }
            FLYING => {
                let station = u32::from(self.target(id));
                let s = self.station(station);
                if self.can_use_station(id, i64::from(station)) != 0
                    && (self.station_owner(s) == OWNER_NONE
                        || self.station_owner(s) == self.owner(id))
                    && self.blocks(station) & CLOSED == 0
                {
                    let landing = if self.subtype(id) == 0 {
                        HELILANDING
                    } else {
                        LANDING
                    };
                    let mut cursor = self.fta_next(self.node(ap, self.pos(id)));
                    while !cursor.is_null() {
                        let node = self.route_node(cursor);
                        if node.heading == landing {
                            let speed = self.current_speed(id);
                            let sub = self.subspeed(id);
                            if !self.has_block(id, cursor, ap) {
                                self.set_state(id, landing);
                                if landing == HELILANDING {
                                    self.flag(id, 3, true);
                                }
                                self.set_pos(id, node.next_position);
                                self.reserve(station, self.fta_blocks(self.node(ap, self.pos(id))));
                                return;
                            }
                            self.set_current_speed(id, speed);
                            self.set_subspeed(id, sub);
                        }
                        cursor = node.next;
                    }
                }
                self.set_state(id, FLYING);
                self.set_pos(id, self.fta_next_position(self.node(ap, self.pos(id))));
            }
            LANDING => {
                self.set_state(id, ENDLANDING);
                self.update_delta(id);
                self.touchdown_animation(id, i64::from(self.target(id)));
                if self.touchdown_sound(id) == 0 {
                    self.play_sound(id, self.skid_sound());
                }
                if self.needs_service(id) != 0 {
                    self.send_to_depot(id, 1);
                }
            }
            HELILANDING => {
                self.set_state(id, HELIENDLANDING);
                self.update_delta(id);
            }
            ENDLANDING => {
                if self.has_block(id, self.node(ap, self.pos(id)), ap) {
                    return;
                }
                if self.order_type(id) == 1 && self.find_terminal(id, ap) {
                    return;
                }
                self.set_state(id, HANGAR);
            }
            HELIENDLANDING => {
                if self.has_block(id, self.node(ap, self.pos(id)), ap) {
                    return;
                }
                if self.order_type(id) == 1 && self.find_helipad(id, ap) {
                    return;
                }
                self.set_state(
                    id,
                    if self.has_hangar(self.target_station(id)) != 0 {
                        HANGAR
                    } else {
                        HELITAKEOFF
                    },
                );
            }
            _ => {
                self.invalid_scheme(id);
            }
        }
    }
    fn airport_move(&self, id: Vehicle, ap: *const c_void) -> bool {
        if i64::from(self.pos(id)) >= self.airport_elements(ap) {
            self.invalid_position(id, ap);
        }
        let mut cursor = self.node(ap, self.pos(id));
        let current = self.route_node(cursor);
        if current.heading == self.state(id) {
            let previous = self.pos(id);
            let state = self.state(id);
            self.state_handler(id, ap);
            if self.state(id) != FLYING {
                self.set_previous(id, previous);
            }
            if self.state(id) != state || self.pos(id) != previous {
                self.cache(id, false);
            }
            return true;
        }
        self.set_previous(id, self.pos(id));
        if current.next.is_null() {
            if self.set_block(id, cursor, ap) {
                self.set_pos(id, current.next_position);
                self.cache(id, false);
            }
            return false;
        }
        loop {
            let node = self.route_node(cursor);
            if node.heading == self.state(id) || node.heading == 0 {
                if self.set_block(id, cursor, ap) {
                    self.set_pos(id, node.next_position);
                    self.cache(id, false);
                }
                return false;
            }
            cursor = node.next;
            if cursor.is_null() {
                break;
            }
        }
        self.invalid_movement(id);
        false
    }
    fn airport_next(&self, id: Vehicle) {
        if !self.controller(id) {
            return;
        }
        let ap = self.airport(id);
        let previous = self.fta_blocks(self.node(ap, self.previous(id)));
        let current = self.fta_blocks(self.node(ap, self.pos(id)));
        if previous != current {
            let station = u32::from(self.target(id));
            if !(self.blocks(station) & ZEPPELIN != 0 && previous == 1 << 8) {
                self.release(station, previous);
            }
        }
        self.airport_move(id, ap);
    }
    fn too_far(&self, id: Vehicle, too_far: bool) {
        let has = self.flags(id) & 1 != 0;
        if too_far {
            if !has {
                self.flag(id, 0, true);
                self.dirty_start_stop(id);
                self.destination_too_far(id);
            }
            return;
        }
        if has {
            self.flag(id, 0, false);
            self.dirty_start_stop(id);
            self.delete_range_news(id);
        }
    }
    fn distance(&self, a: u32, b: u32) -> u32 {
        let dx = (self.tile_x(a) - self.tile_x(b)) as i32;
        let dy = (self.tile_y(a) - self.tile_y(b)) as i32;
        dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) as u32
    }
    fn event(&self, id: Vehicle, pass: u32) -> bool {
        if self.vehicle_status(id) & CRASHED != 0 {
            return self.crashed_tick(id);
        }
        if self.vehicle_status(id) & STOPPED != 0 {
            return true;
        }
        self.handle_breakdown(id);
        self.smoke(id, pass != 0);
        self.process_orders(id);
        self.handle_loading(id, i64::from(pass));
        if matches!(self.order_type(id), 3 | 4) {
            return true;
        }
        if (ENDTAKEOFF..=HELIENDLANDING).contains(&self.state(id)) {
            self.too_far(id, false);
        } else if self.range_sqr(id) != 0 {
            let current = self.target_station(id);
            let next = if matches!(self.order_type(id), 1 | 2) {
                self.station(self.order_destination(id) as u32)
            } else {
                std::ptr::null()
            };
            if !current.is_null()
                && self.airport_tile(current) as u32 != INVALID_TILE
                && !next.is_null()
                && self.airport_tile(next) as u32 != INVALID_TILE
            {
                self.too_far(
                    id,
                    self.distance(
                        self.airport_tile(current) as u32,
                        self.airport_tile(next) as u32,
                    ) > self.range_sqr(id),
                );
            }
        }
        if self.flags(id) & 1 == 0 {
            self.airport_next(id);
        }
        true
    }
    fn tick(&self, id: Vehicle) -> bool {
        if self.subtype(id) > 2 {
            return true;
        }
        self.set_tick_counter(id, i64::from((self.tick_counter(id) as u8).wrapping_add(1)));
        if self.vehicle_status(id) & STOPPED == 0 {
            self.set_running_ticks(
                id,
                i64::from((self.running_ticks(id) as u8).wrapping_add(1)),
            );
        }
        if self.subtype(id) == 0 {
            self.helicopter_tick(id);
        }
        self.set_order_time(id, i64::from((self.order_time(id) as i32).wrapping_add(1)));
        for pass in 0..2 {
            if !self.event(id, pass) {
                return false;
            }
        }
        true
    }
    fn service_needed(&self, id: Vehicle) {
        if self.service_interval(id) == 0 || self.needs_service(id) == 0 {
            return;
        }
        if self.chain_in_depot(id) != 0 {
            self.service_in_depot(id);
            return;
        }
        if !matches!(self.order_type(id), 1 | 2) {
            return;
        }
        let station = self.order_destination(id) as u32;
        let st = self.station(station);
        if self.has_hangar(st) != 0 && self.can_use_station(id, i64::from(station)) != 0 {
            self.service_order(id, i64::from(station));
            self.dirty_start_stop(id);
        } else if self.order_type(id) == 2 {
            self.dummy_order(id);
            self.dirty_start_stop(id);
        }
    }
    fn day(&self, id: Vehicle, calendar: bool) {
        if self.subtype(id) > 2 {
            return;
        }
        if calendar {
            self.age_vehicle(id);
            return;
        }
        self.economy_age(id);
        let day = (self.day_counter(id) as u8).wrapping_add(1);
        self.set_day_counter(id, i64::from(day));
        if day & 7 == 0 {
            self.decrease_value(id);
        }
        self.check_orders(id);
        self.check_breakdown(id);
        self.service_needed(id);
        if self.running_ticks(id) == 0 {
            return;
        }
        let cost = self.running_cost(id);
        let ticks = self.running_ticks(id);
        let divisor = self.ticks_per_year();
        // Money's saturating multiplication is provided by the shared Money service.
        let cost = cost.saturating_mul(ticks) / divisor;
        self.set_profit(id, self.profit(id).saturating_sub(cost));
        self.set_running_ticks(id, 0);
        self.subtract_cost(id, cost);
        self.dirty_lists(id);
    }
    fn nearest_hangar(&self, id: Vehicle) -> u32 {
        let vtile = self.tile_virt(
            self.x(id).clamp(0, self.map_max_x() * 16),
            self.y(id).clamp(0, self.map_max_y() * 16),
        );
        let mut best = 0;
        let mut result = INVALID_STATION;
        let range = self.range_sqr(id);
        let mut last = INVALID_STATION;
        let mut next = INVALID_STATION;
        if range != 0 {
            if self.order_type(id) == 1
                || (self.order_type(id) == 2 && self.nearest_depot_order(id) == 0)
            {
                last = self.last_station(id) as u32;
                next = self.order_destination(id) as u32;
            } else {
                if self.valid_airport(id) {
                    last = u32::from(self.target(id));
                }
                next = self.next_stopping_station(id) as u32;
            }
        }
        let last = self.station(last);
        let next = self.station(next);
        let engine_subtype = self.engine_subtype(id);
        let mut cursor = INVALID_STATION;
        loop {
            cursor = self.next_station(i64::from(cursor)) as u32;
            if cursor == INVALID_STATION {
                break;
            }
            let st = self.station(cursor);
            if self.station_owner(st) != self.owner(id)
                || self.has_airport(st) == 0
                || self.has_hangar(st) == 0
            {
                continue;
            }
            let ap = self.airport_fta(st);
            if self.airport_flags(ap) & 4 != 0 && engine_subtype & 2 != 0 && self.no_jetcrash() == 0
            {
                continue;
            }
            if self.airport_flags(ap) & 1 == 0 && engine_subtype & 1 != 0 {
                continue;
            }
            if range != 0 {
                let ld = if !last.is_null() && self.airport_tile(last) as u32 != INVALID_TILE {
                    self.distance(self.airport_tile(st) as u32, self.airport_tile(last) as u32)
                } else {
                    0
                };
                let nd = if !next.is_null() && self.airport_tile(next) as u32 != INVALID_TILE {
                    self.distance(self.airport_tile(st) as u32, self.airport_tile(next) as u32)
                } else {
                    0
                };
                if ld > range || nd > range {
                    continue;
                }
            }
            let distance = self.distance(vtile, self.airport_tile(st) as u32);
            if distance < best || result == INVALID_STATION {
                best = distance;
                result = cursor;
            }
        }
        result
    }
    fn replacement(&self, station: u32) {
        let st = self.station(station);
        let ap = self.airport_fta(st);
        let rotation = if self.airport_tile(st) as u32 == INVALID_TILE {
            0
        } else {
            self.rotation(st)
        };
        let mut cursor = Vehicle::invalid();
        loop {
            cursor = self.next_aircraft(cursor.id);
            if cursor.handle.is_null() {
                break;
            }
            if self.subtype(cursor) > 2 || u32::from(self.target(cursor)) != station {
                continue;
            }
            self.assert_flying(cursor);
            if self.order_type(cursor) == 2
                && self.part_of_orders(cursor) == 0
                && self.order_destination(cursor) == i64::from(station)
                && (self.has_hangar(st) == 0
                    || self.can_use_station(cursor, i64::from(station)) == 0)
            {
                self.dummy_order(cursor);
                self.dirty_start_stop(cursor);
            }
            let pos = self.entry_point(cursor, ap, rotation);
            self.set_pos(cursor, pos);
            self.set_previous(cursor, pos);
            self.cache(cursor, false);
        }
        if self.has_hangar(st) == 0 {
            self.remove_depot_orders(i64::from(station));
        }
    }
}
/// Allocate the canonical private state once for every ordinary/indexed aircraft shell.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_aircraft_state_new() -> *mut State {
    Box::into_raw(Box::new(State {
        cached_max_range_sqr: 0,
        cached_max_range: 0,
        cache_padding: 0,
        crashed_counter: 0,
        targetairport: 65535,
        pos: 0,
        previous_pos: 0,
        state: 0,
        last_direction: 255,
        number_consecutive_turns: 0,
        turn_counter: 0,
        flags: 0,
    }))
}
/// Destroy the sole owner after the C++ shell's `PreDestructor` and scalar aliases end.
/// # Safety
/// The allocation is live, uniquely owned, and no scalar access remains active.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_state_destroy(state: *mut State) {
    unsafe {
        drop(Box::from_raw(state));
    }
}
/// Allocate the station airport's canonical mask, independently of service ownership.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_airport_blocks_new() -> *mut u64 {
    Box::into_raw(Box::new(0))
}
/// Release exactly once after the enclosing station stops using the scalar.
/// # Safety
/// Pointer is the live sole airport allocation with no active scalar access.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_airport_blocks_destroy(blocks: *mut u64) {
    unsafe {
        drop(Box::from_raw(blocks));
    }
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_tick(leaves: *const Leaves, id: Vehicle) -> bool {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    w.tick(id)
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_calendar_day(leaves: *const Leaves, id: Vehicle) {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    w.day(id, true);
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_economy_day(leaves: *const Leaves, id: Vehicle) {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    w.day(id, false);
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_position(
    leaves: *const Leaves,
    id: Vehicle,
    x: i32,
    y: i32,
    z: i32,
) {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    w.position(id, i64::from(x), i64::from(y), i64::from(z));
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_enter_hangar(leaves: *const Leaves, id: Vehicle) {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    w.enter_hangar(id);
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_cache(
    leaves: *const Leaves,
    id: Vehicle,
    range: bool,
) {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    w.cache(id, range);
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_flight_bounds(
    leaves: *const Leaves,
    id: Vehicle,
    min: *mut i32,
    max: *mut i32,
) {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    {
        let (lo, hi) = w.bounds(id);
        if !min.is_null() {
            unsafe {
                min.write(lo as i32);
            }
        }
        if !max.is_null() {
            unsafe {
                max.write(hi as i32);
            }
        }
    }
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_flight_level(
    leaves: *const Leaves,
    id: Vehicle,
    flags: *mut u8,
    takeoff: bool,
) -> i32 {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    w.flight_flags(id, flags, takeoff) as i32
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_height(leaves: *const Leaves, id: Vehicle) -> i32 {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    w.height(id) as i32
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_hold_altitude(
    leaves: *const Leaves,
    id: Vehicle,
) -> i32 {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    (w.height(id) + if w.subtype(id) == 0 { 184 } else { 150 }) as i32
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_missing_orders(leaves: *const Leaves, id: Vehicle) {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    w.missing_orders(id);
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_crash(
    leaves: *const Leaves,
    id: Vehicle,
    flooded: bool,
) -> u32 {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    w.crash(id, flooded)
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_next_airport(leaves: *const Leaves, id: Vehicle) {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    w.next_airport(id);
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_leave_hangar(
    leaves: *const Leaves,
    id: Vehicle,
    direction: u8,
) {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    w.leave_hangar(id, i64::from(direction));
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_order_location(leaves: *const Leaves, id: Vehicle) {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    if w.state(id) == FLYING {
        w.next_airport(id);
    }
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_replacement(leaves: *const Leaves, station: u16) {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    w.replacement(u32::from(station));
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_nearest_hangar(
    leaves: *const Leaves,
    id: Vehicle,
) -> u16 {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    w.nearest_hangar(id) as u16
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_closest_depot(
    leaves: *const Leaves,
    id: Vehicle,
) -> u16 {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    {
        let st = w.target_station(id);
        if !st.is_null()
            && w.airport_tile(st) as u32 != INVALID_TILE
            && w.can_use_station(id, i64::from(w.target(id))) != 0
            && w.has_hangar(st) != 0
        {
            w.target(id)
        } else {
            w.nearest_hangar(id) as u16
        }
    }
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_event(
    leaves: *const Leaves,
    id: Vehicle,
    pass: bool,
) -> bool {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    w.event(id, u32::from(pass))
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_release_blocks(leaves: *const Leaves, id: Vehicle) {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    if w.valid_airport(id) {
        let ap = w.airport(id);
        let bits = w.fta_blocks(w.node(ap, w.previous(id))) | w.fta_blocks(w.node(ap, w.pos(id)));
        w.release(u32::from(w.target(id)), bits);
    }
}
/// Run the original controller synchronously over live handles.
/// # Safety
/// Handles/state and immutable services satisfy the source lifetime preconditions.
/// Raw scalar accesses end before noexcept callbacks; no borrow survives reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_invalidate_target(
    leaves: *const Leaves,
    station: u16,
) {
    let w = World {
        leaves: unsafe { &*leaves },
    };
    {
        let mut cursor = Vehicle::invalid();
        loop {
            cursor = w.next_aircraft(cursor.id);
            if cursor.handle.is_null() {
                break;
            }
            if w.subtype(cursor) <= 2 && w.target(cursor) == station {
                w.set_target(cursor, 65535);
            }
        }
    }
}
