/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Road vehicles own their movement, consist traversal and private counters/cache.
//! Each entry receives a live vehicle handle and its Rust owner; consist links
//! carry both. Owner scalars are read and written in place through the raw owner
//! pointer, so no reference into an owner survives a service call. Observations
//! are copies taken at the original read points and refreshed after every service
//! that can change them.
#![allow(
    unsafe_code,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::too_many_lines,
    clippy::too_many_arguments,
    clippy::struct_excessive_bools,
    clippy::many_single_char_names,
    clippy::similar_names,
    clippy::verbose_bit_mask,
    clippy::items_after_statements,
    clippy::if_same_then_else,
    clippy::needless_late_init,
    clippy::manual_is_power_of_two,
    clippy::collapsible_if,
    clippy::if_not_else
)]
use crate::services::Services;
use crate::vehicle::{
    AM_REALISTIC, CALLBACK_FAILED, GroundServices, HVOT_BUS, HVOT_TRUCK, INVALID_ENGINE,
    INVALID_PRICE, INVALID_TRACKDIR, MP_ROAD, MP_STATION, MP_TUNNELBRIDGE, Map, MapServices,
    OT_GOTO_DEPOT, OT_GOTO_STATION, OT_LEAVESTATION, OT_LOADING, SND_DEPARTURE_OLD_RV_1,
    SND_DEPARTURE_OLD_RV_2, VEHICLE_LENGTH, VETS_CANNOT_ENTER, VETS_ENTERED_WORMHOLE, VS_CRASHED,
    VS_HIDDEN, VS_STOPPED, VehicleRef, VehicleServices, YEAR_TICKS, advance_distance,
};
use std::ffi::c_void;
use std::ptr::NonNull;

const DEPOT: u8 = 254;
const WORMHOLE: u8 = 255;
const STOP: u8 = 32;
const DT_STOP: u8 = 64;
const REVERSE: [u8; 4] = [6, 7, 14, 15];
const DIAG_TRACK: [u8; 4] = [0, 1, 8, 9];
/// Owner scalar selectors, shared with `openttd_rust_road_get/set`.
const STATE: usize = 0;
const FRAME: usize = 1;
const BLOCKED: usize = 2;
const OVERTAKING: usize = 3;
const OVERTAKING_CTR: usize = 4;
const CRASHED: usize = 5;
const REVERSE_CTR: usize = 6;

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
        (*state).scalars[field as usize] = narrow(field as usize, value);
    }
}
/// Fields 2 and 5 are `uint16_t`; the others are `uint8_t`.
fn narrow(field: usize, value: u16) -> u16 {
    if field == BLOCKED || field == CRASHED {
        value
    } else {
        u16::from(value as u8)
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

/// Settings and map size, read by the C++ facade once per entry.
#[repr(C)]
pub struct Entry {
    pub max_penalty: u32,
    pub acceleration_model: u8,
    pub road_side: u8,
    pub queue: bool,
    pub map_log_x: u8,
}

/// `RoadVehicle::Tick` inputs read by the facade before any service runs.
#[repr(C)]
pub struct TickRead {
    pub next: Option<VehicleRef>,
    pub next_state: *mut State,
    pub tile: u32,
    pub z: i32,
    pub order_time: i32,
    pub tick: u8,
    pub running: u8,
    pub status: u8,
    pub front: bool,
    pub crossing: bool,
}

/// `GetCurrentMaxSpeed` inputs of the front part.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct MaxSpeedRead {
    pub next: Option<VehicleRef>,
    pub next_state: *mut State,
    pub tile: u32,
    pub max_track_speed: u16,
    pub order_max_speed: u16,
    pub direction: u8,
    pub status: u8,
}

/// `ShowVisualEffect()`, then the speed and first-step inputs it leaves.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct SpeedRead {
    pub max: MaxSpeedRead,
    pub x: i32,
    pub y: i32,
    pub z: i32,
    pub acceleration: i32,
    pub length: u8,
    pub tram: bool,
}

/// One further consist part for speed, viewport and slope walks.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct SpeedPart {
    pub next: Option<VehicleRef>,
    pub next_state: *mut State,
    pub tile: u32,
    pub direction: u8,
    pub status: u8,
}

/// Movement inputs of one part at the start of `IndividualRoadVehicleController`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Step {
    pub next: Option<VehicleRef>,
    pub next_state: *mut State,
    pub tile: u32,
    pub x: i32,
    pub y: i32,
    pub z: i32,
    pub cur_speed: u16,
    pub direction: u8,
    pub length: u8,
    pub front: bool,
    pub tram: bool,
}

/// One part for `RoadVehCheckTrainCrash`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct CrashPart {
    pub next: Option<VehicleRef>,
    pub next_state: *mut State,
    pub tile: u32,
    pub z: i32,
    pub crossing: bool,
}

/// `VehicleEnterTile` result, then the fields it may have changed.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Entered {
    pub tile: u32,
    pub cur_speed: u16,
    pub flags: u8,
    pub direction: u8,
    pub order_type: u8,
}

/// One road vehicle near the probe, other than the probing consist.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Candidate {
    pub vehicle: VehicleRef,
    pub state: *mut State,
    pub first: VehicleRef,
    pub first_state: *mut State,
    pub index: u32,
    pub x: i32,
    pub y: i32,
    pub z: i32,
    pub first_speed: u16,
    pub direction: u8,
}

/// Road vehicle on a tile checked for overtaking.
#[repr(C)]
pub struct TileVehicle {
    pub vehicle: VehicleRef,
    pub first: VehicleRef,
}

/// Generic part read: link, tile, height and road/tram type.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct PartRead {
    pub next: Option<VehicleRef>,
    pub next_state: *mut State,
    pub tile: u32,
    pub z: i32,
    pub tram: bool,
}

/// `RoadFindPathToDest` vehicle inputs.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct PathRead {
    pub dest: u32,
    pub owner: u8,
    pub articulated: bool,
    pub bus: bool,
}

/// Road stop arrival and departure inputs.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct StopRead {
    pub order_destination: u16,
    pub order_type: u8,
    pub owner: u8,
    pub bus: bool,
}

/// `RoadVehCheckOvertake` inputs of either vehicle.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OvertakeRead {
    pub tile: u32,
    pub cur_speed: u16,
    pub direction: u8,
    pub status: u8,
    pub articulated: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RoadTypes {
    pub before: u8,
    pub after: u8,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct PreviousTile {
    pub tile: u32,
    pub exists: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrackChoice {
    pub trackdir: u8,
    pub found: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct DepotResult {
    pub tile: u32,
    pub length: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct DepotOrder {
    pub dest: u32,
    pub order_type: u8,
    pub nonstop: u8,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ArrivalRead {
    pub had_vehicle_of_type: u8,
    pub bus: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SoundRead {
    pub sound: u16,
    pub tick: u8,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CachePart {
    pub next: Option<VehicleRef>,
    pub engine: u16,
    pub max_speed: u16,
    pub grf_version: u8,
    pub shorten: u8,
    pub length: u8,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CostRead {
    pub cost_class: u8,
    pub cost_factor: u8,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ServiceRead {
    pub tile: u32,
    pub servint: u16,
    pub cur_speed: u16,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct TurnRead {
    pub tile: u32,
    pub breakdown_ctr: u8,
    pub direction: u8,
    pub order_type: u8,
    pub status: u8,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct TrackdirRead {
    pub tile: u32,
    pub direction: u8,
    pub status: u8,
}

pub type CloseVisitor = extern "C" fn(*mut c_void, *const Candidate) -> bool;
pub type TrainVisitor = extern "C" fn(*mut c_void, *const i32) -> bool;
pub type TileVisitor = extern "C" fn(*mut c_void, *const TileVehicle) -> bool;

/// Road-specific services; the shared tables are borrowed through pointers.
#[repr(C)]
pub struct Leaves {
    pub vehicle: *const VehicleServices,
    pub ground: *const GroundServices,
    pub map: *const MapServices,
    pub shared: *const Services,
    pub visual_then_speed: extern "C" fn(VehicleRef, bool) -> SpeedRead,
    pub max_speed_read: extern "C" fn(VehicleRef) -> MaxSpeedRead,
    pub speed_part: extern "C" fn(VehicleRef) -> SpeedPart,
    pub step: extern "C" fn(VehicleRef) -> Step,
    pub crash_part: extern "C" fn(VehicleRef) -> CrashPart,
    pub part: extern "C" fn(VehicleRef) -> PartRead,
    pub enter_tile: extern "C" fn(VehicleRef, u32, i32, i32) -> Entered,
    pub visit_close: extern "C" fn(VehicleRef, i32, i32, bool, CloseVisitor, *mut c_void),
    pub visit_trains: extern "C" fn(VehicleRef, TrainVisitor, *mut c_void),
    pub visit_tile: extern "C" fn(u32, TileVisitor, *mut c_void),
    pub has_road: extern "C" fn(VehicleRef, u32) -> bool,
    pub track_status: extern "C" fn(VehicleRef, u32) -> u32,
    pub any_road_bits: extern "C" fn(VehicleRef, u32, bool) -> u8,
    pub tram_bits: extern "C" fn(u32) -> u8,
    pub is_road_depot_tile: extern "C" fn(u32) -> bool,
    pub depot_direction: extern "C" fn(u32) -> u8,
    pub is_normal_road: extern "C" fn(u32) -> bool,
    pub has_road_works: extern "C" fn(u32) -> bool,
    pub disallowed_directions: extern "C" fn(u32) -> u8,
    pub is_bay_stop: extern "C" fn(u32) -> bool,
    pub bay_direction: extern "C" fn(u32) -> u8,
    pub is_drive_through: extern "C" fn(u32) -> bool,
    pub is_station_road_stop: extern "C" fn(u32) -> bool,
    pub stop_type: extern "C" fn(u32) -> u8,
    pub has_free_bay: extern "C" fn(u32) -> bool,
    pub continuation: extern "C" fn(u32, u32) -> bool,
    pub move_tile: extern "C" fn(VehicleRef, u32) -> RoadTypes,
    pub path_read: extern "C" fn(VehicleRef) -> PathRead,
    pub choose_track: extern "C" fn(VehicleRef, u32, u8, u16) -> TrackChoice,
    pub find_depot: extern "C" fn(VehicleRef, i32) -> DepotResult,
    pub can_build_tram: extern "C" fn(VehicleRef, u32, u8) -> bool,
    pub previous_tile: extern "C" fn(VehicleRef) -> PreviousTile,
    pub disconnect: extern "C" fn(),
    pub stop_read: extern "C" fn(VehicleRef) -> StopRead,
    pub should_stop: extern "C" fn(VehicleRef, u32) -> bool,
    pub stop_leave: extern "C" fn(VehicleRef),
    pub stop_entrance: extern "C" fn(VehicleRef, bool),
    pub stop_entrance_busy: extern "C" fn(VehicleRef) -> bool,
    pub arrival_read: extern "C" fn(VehicleRef, u16) -> ArrivalRead,
    pub arrival_news: extern "C" fn(VehicleRef, u16, u8, u8),
    pub begin_loading_at: extern "C" fn(VehicleRef, u16),
    pub overtake_read: extern "C" fn(VehicleRef) -> OvertakeRead,
    pub sound_read: extern "C" fn(VehicleRef) -> SoundRead,
    pub depot_order: extern "C" fn(VehicleRef) -> DepotOrder,
    pub depot_exit: extern "C" fn(VehicleRef, i32, i32),
    pub crash_news: extern "C" fn(VehicleRef, u32),
    pub detach_last: extern "C" fn(VehicleRef, VehicleRef, VehicleRef),
    pub cache_begin: extern "C" fn(VehicleRef),
    pub cache_part: extern "C" fn(VehicleRef) -> CachePart,
    pub update_cargo_age: extern "C" fn(VehicleRef),
    pub write_max_speed: extern "C" fn(VehicleRef, u16),
    pub cost_read: extern "C" fn(VehicleRef) -> CostRead,
    pub price: extern "C" fn(VehicleRef, u32) -> i64,
    pub pay_running: extern "C" fn(VehicleRef, i64),
    pub service_read: extern "C" fn(VehicleRef) -> ServiceRead,
    pub turn_read: extern "C" fn(VehicleRef) -> TurnRead,
    pub trackdir_read: extern "C" fn(VehicleRef) -> TrackdirRead,
}

/// A consist part: live vehicle handle and its live Rust owner.
#[derive(Clone, Copy, PartialEq, Eq)]
struct Part {
    v: VehicleRef,
    s: NonNull<State>,
}
impl Part {
    fn new(v: VehicleRef, s: *mut State) -> Self {
        // SAFETY: Every road vehicle shell owns a non-null state.
        Self {
            v,
            s: unsafe { NonNull::new_unchecked(s) },
        }
    }
    fn link(v: Option<VehicleRef>, s: *mut State) -> Option<Self> {
        v.map(|v| Self::new(v, s))
    }
    fn get(self, field: usize) -> u16 {
        // SAFETY: Live owner; the place read ends before any service call.
        unsafe { (*self.s.as_ptr()).scalars[field] }
    }
    fn set(self, field: usize, value: u16) {
        // SAFETY: Live owner, game thread; no reference outlives this store.
        unsafe { (*self.s.as_ptr()).scalars[field] = narrow(field, value) }
    }
    fn state(self) -> u8 {
        self.get(STATE) as u8
    }
    fn frame(self) -> u8 {
        self.get(FRAME) as u8
    }
    fn overtaking(self) -> u8 {
        self.get(OVERTAKING) as u8
    }
    fn path_len(self) -> usize {
        // SAFETY: Live owner, short accessor only.
        unsafe { (*self.s.as_ptr()).path.len() }
    }
    fn path_back(self) -> PathElement {
        let last = self.path_len() - 1;
        // SAFETY: Caller establishes a nonempty path; no service in this scope.
        unsafe { (&(*self.s.as_ptr()).path)[last] }
    }
    fn path_clear(self) {
        // SAFETY: Live owner, game thread.
        unsafe { (*self.s.as_ptr()).path.clear() }
    }
    fn path_pop(self) {
        // SAFETY: Live owner, game thread.
        unsafe {
            (*self.s.as_ptr()).path.pop();
        }
    }
    fn path_push(self, trackdir: u8, tile: u32) {
        // SAFETY: Live owner, game thread.
        unsafe { (*self.s.as_ptr()).path.push(PathElement { trackdir, tile }) }
    }
}
/// Read the state scalar of a part known only by its owner pointer.
fn state_of(s: *mut State) -> u8 {
    // SAFETY: Live owner supplied by a native traversal for this dispatch only.
    unsafe { (*s).scalars[STATE] as u8 }
}

/// Synchronously visit native candidates with a Rust closure.
fn visit<T, F: FnMut(&T) -> bool>(
    call: impl FnOnce(extern "C" fn(*mut c_void, *const T) -> bool, *mut c_void),
    mut visitor: F,
) {
    extern "C" fn dispatch<T, F: FnMut(&T) -> bool>(context: *mut c_void, item: *const T) -> bool {
        // SAFETY: Native traversal calls synchronously and once at a time; item is
        // borrowed for this dispatch only.
        unsafe { (*context.cast::<F>())(&*item) }
    }
    call(dispatch::<T, F>, (&raw mut visitor).cast());
}

struct Game<'a> {
    l: &'a Leaves,
    vs: &'a VehicleServices,
    gs: &'a GroundServices,
    ms: &'a MapServices,
    ss: &'a Services,
    acceleration_model: u8,
    road_side: u8,
    queue: bool,
    max_penalty: u32,
    map: Map,
}

fn in_range(value: u8, start: u8, end: u8) -> bool {
    value >= start && value < end
}
fn reversing(dir: u8) -> bool {
    dir & 7 >= 6
}
fn straight(dir: u8) -> bool {
    dir & 6 == 0
}
fn diag(dir: u8) -> u8 {
    (dir >> 1) & 3
}

impl Game<'_> {
    /// # Safety
    /// Tables outlive the synchronous entry and every nested entry.
    unsafe fn new<'a>(leaves: *const Leaves, entry: Option<&Entry>) -> Game<'a> {
        // SAFETY: Immutable static tables supplied by the C++ facade.
        let l = unsafe { &*leaves };
        let (acceleration_model, road_side, queue, max_penalty, log_x) =
            entry.map_or((0, 0, false, 0, 0), |e| {
                (
                    e.acceleration_model,
                    e.road_side,
                    e.queue,
                    e.max_penalty,
                    e.map_log_x,
                )
            });
        // SAFETY: As above; the shared tables are static.
        unsafe {
            Game {
                l,
                vs: &*l.vehicle,
                gs: &*l.ground,
                ms: &*l.map,
                ss: &*l.shared,
                acceleration_model,
                road_side,
                queue,
                max_penalty,
                map: Map { log_x },
            }
        }
    }
    fn realistic(&self) -> bool {
        self.acceleration_model == AM_REALISTIC
    }
    /// `== AM_ORIGINAL`; the original tests either constant at each site.
    fn original(&self) -> bool {
        self.acceleration_model == 0
    }
    fn side(&self) -> usize {
        usize::from(self.road_side) << 4
    }
    fn random(&self) -> u32 {
        (self.ss.random)(self.ss.context)
    }
    fn tile_xy(&self, tile: u32, pos: (u8, u8)) -> (i32, i32) {
        (
            (self.map.tile_x(tile) * 16 + u32::from(pos.0)) as i32,
            (self.map.tile_y(tile) * 16 + u32::from(pos.1)) as i32,
        )
    }
    fn set_speed(&self, v: Part, s: &mut Step, speed: u16) {
        (self.vs.write_speed)(v.v, speed);
        s.cur_speed = speed;
    }
    fn observe(s: &mut Step, e: Entered) {
        s.tile = e.tile;
        s.cur_speed = e.cur_speed;
        s.direction = e.direction;
    }

    /// `RoadVehicle::GetCurrentMaxSpeed`.
    fn max_speed(&self, front: Part, r: MaxSpeedRead) -> i32 {
        let mut speed = i32::from(r.max_track_speed);
        let mut u = SpeedPart {
            next: r.next,
            next_state: r.next_state,
            tile: r.tile,
            direction: r.direction,
            status: r.status,
        };
        let mut part = front;
        loop {
            if self.realistic() {
                let state = front.state();
                if state <= 15 && reversing(state) {
                    speed = i32::from(r.max_track_speed) / 2;
                    break;
                } else if u.direction & 1 == 0 {
                    speed = i32::from(r.max_track_speed) * 3 / 4;
                }
            }
            if part.state() == WORMHOLE && u.status & VS_HIDDEN == 0 {
                speed = speed.min(i32::from((self.ms.bridge_speed)(u.tile)) * 2);
            }
            let Some(next) = Part::link(u.next, u.next_state) else {
                break;
            };
            part = next;
            u = (self.l.speed_part)(next.v);
        }
        speed.min(i32::from(r.order_max_speed) * 2)
    }
    /// `RoadVehicle::UpdateSpeed`, returning the distance and stored speed.
    fn update_speed(&self, front: Part, r: &SpeedRead) -> (i32, u16) {
        let over = front.overtaking() != 0;
        let (accel, min) = if self.realistic() {
            (
                r.acceleration.wrapping_add(if over { 256 } else { 0 }) as u32,
                if r.max.status & VS_STOPPED != 0 { 0 } else { 4 },
            )
        } else {
            (if over { 512 } else { 256 }, 0)
        };
        let speed = (self.gs.do_update_speed)(front.v, accel, min, self.max_speed(front, r.max));
        (speed.distance, speed.cur_speed)
    }
    /// `RoadVehFindCloseTo`; `v` is always the front part.
    fn close(&self, v: Part, z: i32, x: i32, y: i32, dir: u8, update: bool) -> Option<Candidate> {
        if v.get(REVERSE_CTR) != 0 {
            return None;
        }
        let mut best: Option<Candidate> = None;
        let mut best_diff = u32::MAX;
        let wormhole = v.state() == WORMHOLE;
        visit(
            |dispatch, context| (self.l.visit_close)(v.v, x, y, wormhole, dispatch, context),
            |u: &Candidate| {
                // FindClosestBlockingRoadVeh; C++ already skipped other types and our consist.
                if state_of(u.state) == DEPOT {
                    return true;
                }
                if u.z.wrapping_sub(z).wrapping_abs() >= 6 || u.direction != dir {
                    return true;
                }
                let x_diff = u.x.wrapping_sub(x);
                let y_diff = u.y.wrapping_sub(y);
                let diff = x_diff.wrapping_abs().wrapping_add(y_diff.wrapping_abs()) as u32;
                if diff > best_diff
                    || (diff == best_diff && best.as_ref().is_some_and(|b| u.index > b.index))
                {
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
                let d = usize::from(u.direction);
                if axis(DX[d], x_diff) && axis(DY[d], y_diff) {
                    best = Some(*u);
                    best_diff = diff;
                }
                true
            },
        );
        if best_diff == u32::MAX {
            v.set(BLOCKED, 0);
            return None;
        }
        if update {
            let counter = v.get(BLOCKED).wrapping_add(1);
            v.set(BLOCKED, counter);
            if counter > 1480 {
                return None;
            }
        }
        best
    }
    /// `CheckRoadBlockedForOvertaking`.
    fn overtake_blocked(&self, v: Part, u: VehicleRef, tile: u32, trackdir: u8) -> bool {
        if !(self.l.has_road)(v.v, tile) {
            return true;
        }
        let ts = (self.l.track_status)(v.v, tile);
        let trackdirs = ts & 0x3f3f;
        let red = (ts >> 16) & 0x3f3f;
        let trackbits = (trackdirs | (trackdirs >> 8)) & 0x3f;
        if trackdirs & (1 << trackdir) == 0 || trackbits & !3 != 0 || red != 0 {
            return true;
        }
        let mut blocked = false;
        visit(
            |dispatch, context| (self.l.visit_tile)(tile, dispatch, context),
            |w: &TileVehicle| {
                blocked = w.first == w.vehicle && w.vehicle != u && w.vehicle != v.v;
                !blocked
            },
        );
        blocked
    }
    /// `RoadVehCheckOvertake(v, u)`; pure predicates are evaluated cheapest first.
    fn overtake(&self, v: Part, s: &Step, u: Part) {
        if s.tram {
            return;
        }
        let ur = (self.l.overtake_read)(u.v);
        if s.direction != ur.direction || s.direction & 1 == 0 {
            return;
        }
        let state = v.state();
        if state >= STOP || !straight(state & 15) {
            return;
        }
        if (self.ms.tile_type)(s.tile) == MP_STATION || (self.ms.tile_type)(ur.tile) == MP_STATION {
            return;
        }
        if s.next.is_some() && (self.l.overtake_read)(v.v).articulated {
            return;
        }
        let u_speed = if self.original() || (self.gs.acceleration)(u.v) > 0 {
            self.max_speed(u, (self.l.max_speed_read)(u.v))
        } else {
            i32::from(ur.cur_speed)
        };
        if u_speed >= self.max_speed(v, (self.l.max_speed_read)(v.v))
            && ur.status & VS_STOPPED == 0
            && ur.cur_speed != 0
        {
            return;
        }
        let trackdir = DIAG_TRACK[usize::from(diag(s.direction))];
        if self.overtake_blocked(v, u.v, s.tile, trackdir) {
            return;
        }
        let tile = self.map.add_diag(s.tile, diag(s.direction));
        if self.overtake_blocked(v, u.v, tile, trackdir) {
            return;
        }
        v.set(
            OVERTAKING_CTR,
            if ur.cur_speed == 0 || ur.status & VS_STOPPED != 0 {
                17
            } else {
                0
            },
        );
        v.set(OVERTAKING, 16);
    }
    /// `RoadVehGetSlidingDirection`.
    fn sliding(s: &Step, x: i32, y: i32) -> u8 {
        const NEW_DIR: [u8; 11] = [0, 7, 6, 255, 1, 0, 5, 255, 2, 3, 4];
        let xd = x.wrapping_sub(s.x).wrapping_add(1);
        let yd = y.wrapping_sub(s.y).wrapping_add(1);
        let new = if xd as u32 > 2 || yd as u32 > 2 {
            s.direction
        } else {
            NEW_DIR[(yd * 4 + xd) as usize]
        };
        if new == s.direction {
            return s.direction;
        }
        let delta = if new.wrapping_sub(s.direction) & 7 > 4 {
            7
        } else {
            1
        };
        (s.direction + delta) & 7
    }
    /// Direction store with the original-acceleration curve slowdown.
    fn set_direction(&self, v: Part, s: &mut Step, dir: u8) {
        (self.vs.write_direction)(v.v, dir);
        s.direction = dir;
        if self.original() {
            self.set_speed(v, s, s.cur_speed - (s.cur_speed >> 2));
        }
    }
    /// x/y store, `UpdatePosition`, `UpdateInclination` and `RoadZPosAffectSpeed`.
    fn move_to(&self, v: Part, s: &mut Step, x: i32, y: i32, new_tile: bool, delta: bool) {
        let m = (self.gs.move_incline)(v.v, x, y, new_tile, delta);
        s.x = x;
        s.y = y;
        s.z = m.z;
        s.cur_speed = m.cur_speed;
        if m.old_z == m.z || !self.original() {
            return;
        }
        if m.old_z < m.z {
            self.set_speed(v, s, (u32::from(m.cur_speed) * 232 / 256) as u16);
        } else {
            let speed = m.cur_speed.wrapping_add(2);
            if speed <= m.max_track_speed {
                self.set_speed(v, s, speed);
            }
        }
    }
    /// `StartRoadVehSound`.
    fn start_sound(&self, v: Part) {
        if !(self.vs.play_start_sound)(v.v) {
            let r = (self.l.sound_read)(v.v);
            let mut sound = r.sound;
            if sound == SND_DEPARTURE_OLD_RV_1 && r.tick & 3 == 0 {
                sound = SND_DEPARTURE_OLD_RV_2;
            }
            (self.vs.play_sound)(v.v, sound);
        }
    }
    /// `RoadVehicle::Crash`.
    fn crash(&self, v: Part, flooded: bool) -> u32 {
        let c = (self.gs.crash)(v.v, flooded);
        let mut victims = c.victims;
        if c.front {
            victims = victims.wrapping_add(1);
            if in_range(v.state(), DT_STOP, DT_STOP + 16) {
                (self.l.stop_leave)(v.v);
            }
        }
        v.set(CRASHED, if flooded { 2000 } else { 1 });
        victims
    }
    /// `RoadVehCheckTrainCrash`, starting with the front part's observation.
    fn train_crash(&self, v: Part, first: CrashPart) -> bool {
        let mut part = v;
        let mut u = first;
        loop {
            if part.state() != WORMHOLE && u.crossing {
                let z = u.z;
                let mut hit = false;
                visit(
                    |dispatch, context| (self.l.visit_trains)(v.v, dispatch, context),
                    |tz: &i32| {
                        hit = tz.wrapping_sub(z).wrapping_abs() <= 6;
                        !hit
                    },
                );
                if hit {
                    let victims = self.crash(v, false);
                    (self.l.crash_news)(v.v, victims);
                    return true;
                }
            }
            let Some(next) = Part::link(u.next, u.next_state) else {
                return false;
            };
            part = next;
            u = (self.l.crash_part)(next.v);
        }
    }
    /// `RoadVehIsCrashed` with this tick's counter.
    fn crashed(&self, v: Part, tick: u8) -> bool {
        let counter = v.get(CRASHED).wrapping_add(1);
        v.set(CRASHED, counter);
        if counter == 2 {
            (self.vs.large_explosion)(v.v);
        } else if counter <= 45 {
            if tick & 7 == 0 {
                const DELTA: [u8; 4] = [7, 0, 0, 1];
                let mut u = Some(v.v);
                while let Some(part) = u {
                    let random = self.random();
                    let r = (self.l.speed_part)(part);
                    (self.gs.turn)(part, (r.direction + DELTA[(random & 3) as usize]) & 7);
                    u = r.next;
                }
            }
        } else if counter >= 2220 && tick & 31 == 0 {
            let first = (self.l.part)(v.v);
            let alive = first.next.is_some();
            let mut previous = v;
            let mut last = v;
            let mut next = Part::link(first.next, first.next_state);
            while let Some(part) = next {
                previous = last;
                last = part;
                let r = (self.l.part)(part.v);
                next = Part::link(r.next, r.next_state);
            }
            (self.l.detach_last)(previous.v, last.v, v.v);
            if in_range(last.state(), STOP, STOP + 16) {
                (self.l.stop_leave)(last.v);
            }
            (self.vs.destroy)(last.v);
            return alive;
        }
        true
    }
    /// `RoadVehUpdateCache`.
    fn update_cache(&self, v: Part, same_length: bool) {
        (self.l.cache_begin)(v.v);
        let first = (self.l.cache_part)(v.v);
        let mut total: u16 = 0;
        let mut u = v.v;
        let mut r = first;
        loop {
            (self.gs.write_first_engine)(
                u,
                if u == v.v {
                    INVALID_ENGINE
                } else {
                    first.engine
                },
            );
            let mut length = VEHICLE_LENGTH;
            let mut factor = if r.grf_version >= 8 {
                let value = (self.vs.property)(u, 0x23, i32::from(CALLBACK_FAILED)) as u16;
                if value != CALLBACK_FAILED && value >= u16::from(VEHICLE_LENGTH) {
                    (self.vs.unknown_length_result)(u, value);
                }
                value
            } else {
                (self.vs.length_callback)(u)
            };
            if factor == CALLBACK_FAILED {
                factor = u16::from(r.shorten);
            }
            if factor != 0 {
                length -= factor.min(u16::from(VEHICLE_LENGTH - 1)) as u8;
            }
            if same_length && length != r.length {
                (self.vs.length_changed)(u);
            }
            total = total.wrapping_add(u16::from(length));
            (self.gs.write_length)(u, length, v.v, total);
            (self.l.update_cargo_age)(u);
            let Some(next) = r.next else {
                break;
            };
            u = next;
            r = (self.l.cache_part)(u);
        }
        let speed = (self.vs.property)(v.v, 0x15, 0) as u32;
        (self.l.write_max_speed)(
            v.v,
            if speed != 0 {
                speed.wrapping_mul(4)
            } else {
                u32::from(first.max_speed)
            } as u16,
        );
    }
    /// `RoadFindPathToDest`.
    fn path(&self, v: Part, s: &Step, tile: u32, entry: u8) -> u8 {
        let ts = (self.l.track_status)(v.v, tile);
        let red = (ts >> 16) & 0x3f3f;
        let mut trackdirs = ts & 0x3f3f;
        let p = (self.l.path_read)(v.v);
        let kind = (self.ms.tile_type)(tile);
        if kind == MP_ROAD {
            if (self.l.is_road_depot_tile)(tile)
                && ((self.ms.tile_owner)(tile) != p.owner
                    || (self.l.depot_direction)(tile) == entry)
            {
                trackdirs = 0;
            }
        } else if kind == MP_STATION && (self.l.is_bay_stop)(tile) {
            if (self.ms.tile_owner)(tile) != p.owner
                || (self.l.bay_direction)(tile) == entry
                || p.articulated
            {
                trackdirs = 0;
            } else if (self.l.stop_type)(tile) != u8::from(!p.bus)
                || (!self.queue && !(self.l.has_free_bay)(tile))
            {
                trackdirs = 0;
            }
        }
        trackdirs &= [0x1009, 0x0016, 0x0520, 0x2a00][usize::from(entry)];
        let best = if trackdirs == 0 {
            if v.path_len() != 0 {
                v.path_clear();
            }
            REVERSE[usize::from(entry)]
        } else {
            let mut forced = None;
            if v.get(REVERSE_CTR) != 0 {
                let mut reverse = true;
                if s.tram {
                    let bits = (self.l.any_road_bits)(v.v, tile, false);
                    let straight = if entry & 1 == 0 { 10 } else { 5 };
                    reverse = (bits & straight) == straight || bits == (8 >> entry);
                }
                if reverse {
                    v.set(REVERSE_CTR, 0);
                    if s.tile != tile {
                        forced = Some(REVERSE[usize::from(entry)]);
                    }
                }
            }
            if let Some(dir) = forced {
                dir
            } else if p.dest == 0 {
                let count = trackdirs.count_ones();
                let mut n = ((u64::from(self.random()) * u64::from(count)) >> 32) as u32;
                let mut bits = trackdirs;
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
            } else if trackdirs & (trackdirs - 1) == 0 {
                if v.path_len() != 0 && v.path_back().tile == tile {
                    v.path_clear();
                }
                trackdirs.trailing_zeros() as u8
            } else {
                let mut cached = None;
                if v.path_len() != 0 {
                    let item = v.path_back();
                    if item.tile != tile {
                        v.path_clear();
                    } else if trackdirs & (1 << item.trackdir) != 0 {
                        v.path_pop();
                        cached = Some(item.trackdir);
                    } else {
                        v.path_clear();
                    }
                }
                if let Some(dir) = cached {
                    dir
                } else {
                    let r = (self.l.choose_track)(v.v, tile, entry, trackdirs as u16);
                    (self.vs.pathfinding_result)(v.v, r.found);
                    r.trackdir
                }
            }
        };
        if red & (1 << best) != 0 {
            INVALID_TRACKDIR
        } else {
            best
        }
    }
    /// `RoadVehLeaveDepot`.
    fn leave_depot(&self, v: Part, first: bool) -> bool {
        if v.state() != DEPOT {
            return false;
        }
        let d = (self.l.part)(v.v);
        let mut next = Part::link(d.next, d.next_state);
        while let Some(u) = next {
            if u.state() != DEPOT {
                return false;
            }
            let r = (self.l.part)(u.v);
            if r.tile != d.tile {
                return false;
            }
            next = Part::link(r.next, r.next_state);
        }
        let dir = (self.l.depot_direction)(d.tile);
        let direction = dir * 2 + 1;
        (self.vs.write_direction)(v.v, direction);
        let tdir = DIAG_TRACK[usize::from(dir)];
        let rd = crate::road_data::entry(d.tram, self.side() + usize::from(tdir), 6);
        let (x, y) = self.tile_xy(d.tile, (rd.0 & 15, rd.1 & 15));
        if first {
            let o = (self.l.depot_order)(v.v);
            if o.order_type == OT_GOTO_DEPOT && d.tile == o.dest {
                (self.vs.enter_depot)(v.v);
                return true;
            }
            if self.close(v, d.z, x, y, direction, false).is_some() {
                return true;
            }
            (self.vs.service_in_depot)(v.v);
            (self.vs.leave_unbunching_depot)(v.v);
            self.start_sound(v);
            (self.vs.write_speed)(v.v, 0);
        }
        v.set(STATE, u16::from(tdir));
        v.set(FRAME, 6);
        (self.l.depot_exit)(v.v, x, y);
        true
    }
    /// `FollowPreviousRoadVehicle`.
    fn follow(&self, v: Part, s: &Step, prev: Part, tile: u32, entry: u8, reversed: bool) -> u8 {
        let prev_tile = (self.l.part)(prev.v).tile;
        if prev_tile == s.tile && !reversed {
            return REVERSE[usize::from(entry)];
        }
        let state = prev.state();
        let dir;
        if state == WORMHOLE || state == DEPOT {
            let diagonal = if (self.ms.tile_type)(tile) == MP_TUNNELBRIDGE {
                (self.ms.tunnel_bridge_direction)(tile)
            } else if (self.l.is_road_depot_tile)(tile) {
                (self.l.depot_direction)(tile) ^ 2
            } else {
                return INVALID_TRACKDIR;
            };
            dir = DIAG_TRACK[usize::from(diagonal)];
        } else if reversed && (prev_tile != tile || (state < 16 && reversing(state))) {
            let north = if prev_tile != tile {
                prev_tile < tile
            } else {
                state == 15 || state == 6
            };
            dir = [[10, 13, 12, 2], [5, 11, 3, 4]][usize::from(!north)][usize::from(entry ^ 2)];
        } else if state & 64 != 0 {
            dir = state & 9;
        } else if state < 16 {
            dir = state;
        } else {
            return INVALID_TRACKDIR;
        }
        const REQUIRED: [u8; 8] = [10, 5, 9, 6, 3, 12, 10, 5];
        let bits = (self.l.any_road_bits)(v.v, tile, true);
        if REQUIRED[usize::from(dir & 7)] & bits == 0 {
            INVALID_TRACKDIR
        } else {
            dir
        }
    }
    /// `RoadVehArrivesAt`, `BeginLoading` and the road stop arrival triggers.
    fn arrive_load(&self, v: Part, s: &Step, station: u16) {
        let r = (self.l.arrival_read)(v.v, station);
        let bit = if r.bus { HVOT_BUS } else { HVOT_TRUCK };
        if r.had_vehicle_of_type & bit == 0 {
            let headline = if r.bus { 0 } else { 2 } + u8::from(s.tram);
            (self.l.arrival_news)(v.v, station, bit, headline);
        }
        (self.l.begin_loading_at)(v.v, station);
    }
    /// Drive-through stop test shared by the blocked and stop-frame paths.
    fn stops_here(&self, v: Part, tile: u32) -> bool {
        let r = (self.l.stop_read)(v.v);
        (self.l.should_stop)(v.v, tile)
            && r.owner == (self.ms.tile_owner)(tile)
            && (self.l.stop_type)(tile) == u8::from(!r.bus)
    }
    /// `IndividualRoadVehicleController`; `s` is a fresh observation of `v`.
    fn individual(&self, v: Part, prev: Option<Part>, s: &mut Step) -> bool {
        if v.overtaking() != 0 {
            if (self.ms.tile_type)(s.tile) == MP_STATION {
                v.set(OVERTAKING, 0);
            } else {
                let counter = v.get(OVERTAKING_CTR).wrapping_add(1) as u8;
                v.set(OVERTAKING_CTR, u16::from(counter));
                if counter >= 35 && v.state() < STOP && straight(v.state()) {
                    v.set(OVERTAKING, 0);
                }
            }
        }
        if v.state() == DEPOT {
            return true;
        }
        if v.state() == WORMHOLE {
            let gp = (self.vs.new_position)(v.v);
            if s.front {
                if let Some(u) = self.close(v, s.z, gp.x, gp.y, s.direction, true) {
                    self.set_speed(v, s, u.first_speed);
                    return false;
                }
            }
            if (self.ms.tile_type)(gp.tile) == MP_TUNNELBRIDGE {
                let e = (self.l.enter_tile)(v.v, gp.tile, gp.x, gp.y);
                Self::observe(s, e);
                if e.flags & VETS_ENTERED_WORMHOLE != 0 {
                    self.move_without_speed(v, s, gp.x, gp.y);
                    return true;
                }
            }
            let status = (self.vs.move_then_status)(v.v, gp.x, gp.y);
            s.x = gp.x;
            s.y = gp.y;
            if status & VS_HIDDEN == 0 {
                (self.vs.base_viewport)(v.v);
            }
            return true;
        }
        let state = v.state();
        let movement = if state & 64 != 0 { state & 9 } else { state };
        let rd = crate::road_data::entry(
            s.tram,
            (usize::from(movement) + self.side()) ^ usize::from(v.overtaking()),
            usize::from(v.frame()) + 1,
        );
        let entry = rd.0 & 3;
        if rd.0 & 128 != 0 {
            let mut tile = self.map.add_diag(s.tile, entry);
            let mut dir = if s.front {
                if (self.l.has_road)(v.v, tile) {
                    self.path(v, s, tile, entry)
                } else {
                    REVERSE[usize::from(entry)]
                }
            } else {
                // Non-front parts are only driven by the consist loop, which supplies prev.
                prev.map_or(INVALID_TRACKDIR, |p| {
                    self.follow(v, s, p, tile, entry, false)
                })
            };
            if dir == INVALID_TRACKDIR {
                if !s.front {
                    (self.l.disconnect)();
                }
                self.set_speed(v, s, 0);
                return false;
            }
            loop {
                let mut start_frame = 0;
                if reversing(dir) {
                    v.set(OVERTAKING, 0);
                    if s.tram {
                        let needed: u8 = [2, 1, 8, 4][usize::from(((dir >> 2) & 2) | (dir & 1))];
                        let previous = (self.l.previous_tile)(v.v);
                        let big = (previous.exists && previous.tile == tile)
                            || (s.front
                                && (self.l.is_normal_road)(tile)
                                && !(self.l.has_road_works)(tile)
                                && (self.l.has_road)(v.v, tile)
                                && needed & (self.l.tram_bits)(tile) != 0);
                        if !big {
                            if !s.front
                                || !(self.l.can_build_tram)(v.v, tile, needed)
                                || (!needed & (self.l.any_road_bits)(v.v, s.tile, false)) == 0
                            {
                                tile = s.tile;
                                start_frame = 16;
                            } else {
                                self.set_speed(v, s, 0);
                                return false;
                            }
                        }
                    } else if (self.l.is_normal_road)(s.tile)
                        && (self.l.disallowed_directions)(s.tile) != 0
                    {
                        self.set_speed(v, s, 0);
                        return false;
                    } else {
                        tile = s.tile;
                    }
                }
                let pos = crate::road_data::entry(
                    s.tram,
                    (usize::from(dir) + self.side()) ^ usize::from(v.overtaking()),
                    start_frame,
                );
                let (x, y) = self.tile_xy(tile, pos);
                let new_dir = Self::sliding(s, x, y);
                if s.front {
                    if let Some(u) = self.close(v, s.z, x, y, new_dir, true) {
                        self.set_speed(v, s, u.first_speed);
                        v.path_push(dir, tile);
                        return false;
                    }
                }
                let e = (self.l.enter_tile)(v.v, tile, x, y);
                Self::observe(s, e);
                if e.flags & VETS_CANNOT_ENTER != 0 {
                    if (self.ms.tile_type)(tile) != MP_TUNNELBRIDGE {
                        self.set_speed(v, s, 0);
                        return false;
                    }
                    dir = REVERSE[usize::from(entry)];
                    continue;
                }
                let state = v.state();
                if in_range(state, STOP, DT_STOP + 16) && (self.ms.tile_type)(s.tile) == MP_STATION
                {
                    if reversing(dir) && in_range(state, STOP, STOP + 16) {
                        self.set_speed(v, s, 0);
                        return false;
                    }
                    if (self.l.is_drive_through)(s.tile)
                        && (self.l.continuation)(s.tile, tile)
                        && s.tile != tile
                    {
                        dir = v.state();
                    } else if (self.l.is_station_road_stop)(s.tile) {
                        (self.l.stop_leave)(v.v);
                    }
                }
                if e.flags & VETS_ENTERED_WORMHOLE == 0 {
                    let types = (self.l.move_tile)(v.v, tile);
                    s.tile = tile;
                    v.set(STATE, u16::from(dir));
                    v.set(FRAME, start_frame as u16);
                    if types.before != types.after {
                        if s.front {
                            self.update_cache(v, false);
                        }
                        (self.gs.first_cargo_changed)(v.v);
                    }
                }
                if new_dir != s.direction {
                    self.set_direction(v, s, new_dir);
                }
                self.move_to(v, s, x, y, true, true);
                return true;
            }
        }
        if rd.0 & 64 != 0 {
            let mut start_frame = 1;
            let dir = if s.tram && !(self.l.is_road_depot_tile)(s.tile) && {
                let bits = (self.l.any_road_bits)(v.v, s.tile, true);
                bits != 0 && bits & (bits - 1) == 0
            } {
                start_frame = 21;
                [14, 15, 6, 7][usize::from(entry)]
            } else if s.front {
                self.path(v, s, s.tile, entry)
            } else {
                prev.map_or(INVALID_TRACKDIR, |p| {
                    self.follow(v, s, p, s.tile, entry, true)
                })
            };
            if dir == INVALID_TRACKDIR {
                self.set_speed(v, s, 0);
                return false;
            }
            let pos = crate::road_data::entry(s.tram, self.side() + usize::from(dir), start_frame);
            let (x, y) = self.tile_xy(s.tile, pos);
            let new_dir = Self::sliding(s, x, y);
            if s.front {
                if let Some(u) = self.close(v, s.z, x, y, new_dir, true) {
                    self.set_speed(v, s, u.first_speed);
                    v.path_push(dir, s.tile);
                    return false;
                }
            }
            let e = (self.l.enter_tile)(v.v, s.tile, x, y);
            Self::observe(s, e);
            if e.flags & VETS_CANNOT_ENTER != 0 {
                self.set_speed(v, s, 0);
                return false;
            }
            v.set(STATE, u16::from(dir));
            v.set(FRAME, start_frame as u16);
            if new_dir != s.direction {
                self.set_direction(v, s, new_dir);
            }
            self.move_to(v, s, x, y, true, true);
            return true;
        }
        if let Some(next) = Part::link(s.next, s.next_state) {
            if (self.l.is_road_depot_tile)(s.tile)
                && u16::from(v.frame()) == u16::from(s.length) + 6
            {
                self.leave_depot(next, false);
            }
        }
        let x = (s.x & !15) + i32::from(rd.0 & 15);
        let y = (s.y & !15) + i32::from(rd.1 & 15);
        let new_dir = Self::sliding(s, x, y);
        if s.front && !in_range(v.state(), STOP, STOP + 16) {
            if let Some(c) = self.close(v, s.z, x, y, new_dir, true) {
                let u = Part::new(c.first, c.first_state);
                if v.overtaking() == 0 {
                    self.overtake(v, s, u);
                }
                if v.overtaking() == 0 {
                    self.set_speed(v, s, c.first_speed);
                }
                if s.cur_speed == 0
                    && in_range(v.state(), DT_STOP, DT_STOP + 16)
                    && (self.l.should_stop)(v.v, s.tile)
                {
                    let r = (self.l.stop_read)(v.v);
                    if r.owner == (self.ms.tile_owner)(s.tile)
                        && r.order_type != OT_LEAVESTATION
                        && (self.l.stop_type)(s.tile) == u8::from(!r.bus)
                    {
                        let station = (self.ms.station_index)(s.tile);
                        (self.vs.write_last_station)(v.v, station);
                        self.arrive_load(v, s, station);
                    }
                }
                return false;
            }
        }
        if new_dir != s.direction {
            if self.original() {
                self.set_speed(v, s, s.cur_speed - (s.cur_speed >> 2));
            }
            (self.gs.turn)(v.v, new_dir);
            s.direction = new_dir;
            return true;
        }
        let state = v.state();
        let frame = v.frame();
        // The frame test is pure and decides most drive-through passes without a read.
        if s.front
            && ((in_range(state, STOP, STOP + 16)
                && crate::road_data::_ROAD_STOP_STOP_FRAME
                    [usize::from(state - STOP) + self.side()]
                    == frame)
                || (in_range(state, DT_STOP, DT_STOP + 16)
                    && frame == 11
                    && self.stops_here(v, s.tile)))
        {
            let station = (self.ms.station_index)(s.tile);
            if state & 4 == 0 {
                if (self.l.is_drive_through)(s.tile) {
                    let next = self.map.add_dir(s.tile, s.direction);
                    if (self.l.continuation)(s.tile, next) && (self.l.has_road)(v.v, next) {
                        v.set(FRAME, u16::from(frame) + 1);
                        self.move_to(v, s, x, y, true, false);
                        return true;
                    }
                }
                (self.l.stop_entrance)(v.v, false);
                v.set(STATE, u16::from(state | 4));
                (self.vs.write_last_station)(v.v, station);
                if (self.l.is_drive_through)(s.tile) || {
                    let r = (self.l.stop_read)(v.v);
                    r.order_type == OT_GOTO_STATION && r.order_destination == station
                } {
                    self.arrive_load(v, s, station);
                    return false;
                }
            } else {
                if (self.l.stop_entrance_busy)(v.v) {
                    self.set_speed(v, s, 0);
                    return false;
                }
                if (self.l.stop_read)(v.v).order_type == OT_LEAVESTATION {
                    (self.vs.order_free)(v.v);
                }
            }
            if (self.l.is_bay_stop)(s.tile) {
                (self.l.stop_entrance)(v.v, true);
            }
            self.start_sound(v);
            (self.vs.start_stop_dirty)(v.v);
        }
        let e = (self.l.enter_tile)(v.v, s.tile, x, y);
        Self::observe(s, e);
        if e.flags & VETS_CANNOT_ENTER != 0 {
            self.set_speed(v, s, 0);
            return false;
        }
        if e.order_type == OT_LEAVESTATION && (self.l.is_drive_through)(s.tile) {
            (self.vs.order_free)(v.v);
        }
        if e.flags & VETS_ENTERED_WORMHOLE == 0 {
            v.set(FRAME, u16::from(v.frame()) + 1);
        }
        self.move_to(v, s, x, y, false, true);
        true
    }
    /// Wormhole entry: x/y, `UpdatePosition`, `UpdateInclination(true, true)`.
    fn move_without_speed(&self, v: Part, s: &mut Step, x: i32, y: i32) {
        let m = (self.gs.move_incline)(v.v, x, y, true, true);
        s.x = x;
        s.y = y;
        s.z = m.z;
        s.cur_speed = m.cur_speed;
    }
    /// One `IndividualRoadVehicleController` pass over the consist.
    fn consist_step(&self, v: Part, s: &mut Step) -> bool {
        if !self.individual(v, None, s) {
            return false;
        }
        let mut prev = v;
        let mut next = Part::link(s.next, s.next_state);
        while let Some(u) = next {
            let mut us = (self.l.step)(u.v);
            if !self.individual(u, Some(prev), &mut us) {
                return false;
            }
            prev = u;
            next = Part::link(us.next, us.next_state);
        }
        true
    }
    /// `RoadVehController`.
    fn controller(&self, v: Part, t: &TickRead, tick: u8) -> bool {
        let reverse = v.get(REVERSE_CTR);
        if reverse != 0 {
            v.set(REVERSE_CTR, reverse - 1);
        }
        let first = CrashPart {
            next: t.next,
            next_state: t.next_state,
            tile: t.tile,
            z: t.z,
            crossing: t.crossing,
        };
        if t.status & VS_CRASHED != 0 || self.train_crash(v, first) {
            return self.crashed(v, tick);
        }
        let b = (self.vs.handle_breakdown)(v.v);
        if b.broken {
            return true;
        }
        if b.status & VS_STOPPED != 0 {
            (self.gs.set_last_speed)(v.v);
            return true;
        }
        if (self.vs.process_orders_then_loading)(v.v) == OT_LOADING {
            return true;
        }
        if v.state() == DEPOT {
            if (self.vs.waiting_for_unbunching)(v.v) {
                return true;
            }
            if self.leave_depot(v, true) {
                return true;
            }
        }
        let r = (self.l.visual_then_speed)(v.v, self.realistic());
        let mut s = Step {
            next: r.max.next,
            next_state: r.max.next_state,
            tile: r.max.tile,
            x: r.x,
            y: r.y,
            z: r.z,
            cur_speed: 0,
            direction: r.max.direction,
            length: r.length,
            front: true,
            tram: r.tram,
        };
        let (mut j, speed) = self.update_speed(v, &r);
        s.cur_speed = speed;
        let mut adv = advance_distance(s.direction);
        let mut blocked = false;
        while j >= adv {
            j -= adv;
            if !self.consist_step(v, &mut s) {
                blocked = true;
                break;
            }
            if s.next.is_some() {
                // Later parts may have changed the front; observe it again.
                s = (self.l.step)(v.v);
            }
            adv = advance_distance(s.direction);
            if j >= adv {
                let crossing = v.state() != WORMHOLE && (self.ms.is_level_crossing)(s.tile);
                let first = CrashPart {
                    next: s.next,
                    next_state: s.next_state,
                    tile: s.tile,
                    z: s.z,
                    crossing,
                };
                if self.train_crash(v, first) {
                    break;
                }
            }
        }
        let last = (self.gs.set_last_speed)(v.v);
        if last.status & VS_HIDDEN == 0 {
            (self.gs.update_viewport)(v.v, false, false);
        }
        let mut next = s.next;
        while let Some(u) = next {
            let p = (self.l.speed_part)(u);
            if p.status & VS_HIDDEN == 0 {
                (self.gs.update_viewport)(u, false, false);
            }
            next = p.next;
        }
        if last.progress == 0 {
            (self.vs.write_progress)(v.v, if blocked { adv - 1 } else { j } as u8);
        }
        true
    }
    /// `RoadVehicle::Tick`.
    fn tick(&self, v: Part, t: &TickRead) -> bool {
        let tick = t.tick.wrapping_add(1);
        if !t.front {
            (self.vs.write_counters)(v.v, tick, t.running, t.order_time);
            return true;
        }
        let running = if t.status & VS_STOPPED == 0 {
            t.running.wrapping_add(1)
        } else {
            t.running
        };
        (self.vs.write_counters)(v.v, tick, running, t.order_time.wrapping_add(1));
        self.controller(v, t, tick)
    }
    /// `RoadVehicle::SetDestTile`.
    fn set_dest(&self, v: Part, tile: u32, current: u32) {
        if tile == current {
            return;
        }
        v.path_clear();
        (self.vs.write_dest)(v.v, tile);
    }
    /// `RoadVehicle::GetRunningCost`.
    fn running_cost(&self, v: Part) -> i64 {
        let r = (self.l.cost_read)(v.v);
        if r.cost_class == INVALID_PRICE {
            return 0;
        }
        let factor = (self.vs.property)(v.v, 9, i32::from(r.cost_factor)) as u32;
        if factor == 0 {
            return 0;
        }
        (self.l.price)(v.v, factor)
    }
    /// `CheckIfRoadVehNeedsService`; `IsChainInDepot` and `FindClosestRoadDepot` inline.
    fn service(&self, v: Part) {
        let r = (self.l.service_read)(v.v);
        if r.servint == 0 || !(self.vs.needs_automatic_servicing)(v.v) {
            return;
        }
        let depot_tile = (self.l.is_road_depot_tile)(r.tile);
        let mut all_in_depot = depot_tile && r.cur_speed == 0;
        if all_in_depot && v.state() != DEPOT {
            all_in_depot = false;
        }
        if all_in_depot {
            let mut next = {
                let p = (self.l.part)(v.v);
                Part::link(p.next, p.next_state)
            };
            while let Some(u) = next {
                let p = (self.l.part)(u.v);
                if u.state() != DEPOT || p.tile != r.tile {
                    all_in_depot = false;
                    break;
                }
                next = Part::link(p.next, p.next_state);
            }
        }
        if all_in_depot {
            (self.vs.service_in_depot)(v.v);
            return;
        }
        let penalty = self.max_penalty;
        let depot = if depot_tile {
            DepotResult {
                tile: r.tile,
                length: 0,
            }
        } else {
            (self.l.find_depot)(v.v, penalty as i32)
        };
        if depot.length == u32::MAX || depot.length > penalty {
            if (self.l.depot_order)(v.v).order_type == OT_GOTO_DEPOT {
                (self.vs.order_dummy)(v.v);
            }
            return;
        }
        let depot_id = (self.ms.depot_index)(depot.tile);
        let o = (self.l.depot_order)(v.v);
        if o.order_type == OT_GOTO_DEPOT
            && o.nonstop & 1 != 0
            && !crate::services::chance16_i(1, 20, self.random())
        {
            return;
        }
        (self.gs.go_to_depot_service)(v.v, depot_id);
        self.set_dest(v, depot.tile, (self.l.path_read)(v.v).dest);
        (self.vs.start_stop_dirty)(v.v);
    }
    /// `RoadVehicle::OnNewEconomyDay`.
    fn economy_day(&self, v: Part) {
        let day = (self.vs.economy_age)(v.v).wrapping_add(1);
        (self.vs.write_day)(v.v, day);
        if day & 7 == 0 {
            (self.vs.decrease_value)(v.v);
        }
        if v.get(BLOCKED) == 0 {
            (self.vs.check_breakdown)(v.v);
        }
        self.service(v);
        let running = (self.vs.check_orders)(v.v);
        if running == 0 {
            return;
        }
        let cost = self.running_cost(v).saturating_mul(i64::from(running)) / YEAR_TICKS;
        (self.l.pay_running)(v.v, cost);
    }
    /// `CmdTurnRoadVeh` after the ownership checks.
    fn turn(&self, v: Part, execute: bool) -> bool {
        let r = (self.l.turn_read)(v.v);
        if r.status & (VS_STOPPED | VS_CRASHED) != 0
            || r.breakdown_ctr != 0
            || v.overtaking() != 0
            || v.state() == WORMHOLE
            || v.state() == DEPOT
            || r.order_type == OT_LOADING
        {
            return false;
        }
        if (self.l.is_normal_road)(r.tile) && (self.l.disallowed_directions)(r.tile) != 0 {
            return false;
        }
        if (self.ms.tile_type)(r.tile) == MP_TUNNELBRIDGE
            && diag(r.direction) == (self.ms.tunnel_bridge_direction)(r.tile)
        {
            return false;
        }
        if execute {
            v.set(REVERSE_CTR, 180);
            (self.vs.reset_depot_unbunching)(v.v);
        }
        true
    }
    /// `RoadVehicle::GetVehicleTrackdir`.
    fn trackdir(&self, v: Part) -> u8 {
        let r = (self.l.trackdir_read)(v.v);
        if r.status & VS_CRASHED != 0 {
            return INVALID_TRACKDIR;
        }
        if v.state() == DEPOT {
            return DIAG_TRACK[usize::from((self.l.depot_direction)(r.tile))];
        }
        if (self.l.is_bay_stop)(r.tile) {
            return DIAG_TRACK[usize::from((self.l.bay_direction)(r.tile))];
        }
        let state = v.state();
        if state > 15 {
            return DIAG_TRACK[usize::from(diag(r.direction))];
        }
        if reversing(state) { state - 6 } else { state }
    }
    /// `RoadVehicle::HasToUseGetSlopePixelZ`.
    fn slope_pixel(&self, v: VehicleRef, first: Part, direction: u8) -> bool {
        let state = first.state();
        if state <= 15 && reversing(state) {
            return true;
        }
        let mut u = first.v;
        while u != v {
            let p = (self.l.speed_part)(u);
            if direction != p.direction {
                return true;
            }
            // The walk reaches `v` before the chain ends, as in the original.
            let Some(next) = p.next else {
                return false;
            };
            u = next;
        }
        false
    }
}

macro_rules! entry {
    ($leaves:expr, $entry:expr) => {
        // SAFETY: Immutable tables outlive this synchronous call and nested entries;
        // owner places are accessed only through raw pointers between services.
        unsafe { Game::new($leaves, $entry) }
    };
}

/// # Safety
/// Live handle/owner pair, entry record and tables for this synchronous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_tick(
    v: VehicleRef,
    state: *mut State,
    entry: *const Entry,
    read: *const TickRead,
    leaves: *const Leaves,
) -> bool {
    // SAFETY: Records are borrowed for this call only.
    let (e, t) = unsafe { (&*entry, &*read) };
    entry!(leaves, Some(e)).tick(Part::new(v, state), t)
}
/// # Safety
/// As `openttd_rust_road_tick`; `prev` is null or a live pair.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_individual(
    v: VehicleRef,
    state: *mut State,
    prev: Option<VehicleRef>,
    prev_state: *mut State,
    entry: *const Entry,
    leaves: *const Leaves,
) -> bool {
    // SAFETY: Entry record borrowed for this call only.
    let e = unsafe { &*entry };
    let g = entry!(leaves, Some(e));
    let mut s = (g.l.step)(v);
    g.individual(Part::new(v, state), Part::link(prev, prev_state), &mut s)
}
/// # Safety
/// As `openttd_rust_road_tick`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_leave_depot(
    v: VehicleRef,
    state: *mut State,
    first: bool,
    entry: *const Entry,
    leaves: *const Leaves,
) -> bool {
    // SAFETY: Entry record borrowed for this call only.
    let e = unsafe { &*entry };
    entry!(leaves, Some(e)).leave_depot(Part::new(v, state), first)
}
/// # Safety
/// As `openttd_rust_road_tick`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_crash(
    v: VehicleRef,
    state: *mut State,
    flooded: bool,
    leaves: *const Leaves,
) -> u32 {
    entry!(leaves, None).crash(Part::new(v, state), flooded)
}
/// # Safety
/// As `openttd_rust_road_tick`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_update_cache(
    v: VehicleRef,
    state: *mut State,
    same_length: bool,
    leaves: *const Leaves,
) {
    entry!(leaves, None).update_cache(Part::new(v, state), same_length);
}
/// # Safety
/// As `openttd_rust_road_tick`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_calendar_day(
    v: VehicleRef,
    front: bool,
    leaves: *const Leaves,
) {
    if front {
        // SAFETY: Static tables.
        unsafe { ((*(*leaves).vehicle).age)(v) };
    }
}
/// # Safety
/// As `openttd_rust_road_tick`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_economy_day(
    v: VehicleRef,
    state: *mut State,
    front: bool,
    entry: *const Entry,
    leaves: *const Leaves,
) {
    if front {
        // SAFETY: Entry record borrowed for this call only.
        let e = unsafe { &*entry };
        entry!(leaves, Some(e)).economy_day(Part::new(v, state));
    }
}
/// # Safety
/// As `openttd_rust_road_tick`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_running_cost(
    v: VehicleRef,
    state: *mut State,
    leaves: *const Leaves,
) -> i64 {
    entry!(leaves, None).running_cost(Part::new(v, state))
}
/// # Safety
/// As `openttd_rust_road_tick`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_max_speed(
    v: VehicleRef,
    state: *mut State,
    entry: *const Entry,
    read: *const MaxSpeedRead,
    leaves: *const Leaves,
) -> i32 {
    // SAFETY: Records borrowed for this call only.
    let (e, r) = unsafe { (&*entry, *read) };
    entry!(leaves, Some(e)).max_speed(Part::new(v, state), r)
}
/// # Safety
/// As `openttd_rust_road_tick`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_set_dest(
    v: VehicleRef,
    state: *mut State,
    tile: u32,
    current: u32,
    leaves: *const Leaves,
) {
    entry!(leaves, None).set_dest(Part::new(v, state), tile, current);
}
/// # Safety
/// As `openttd_rust_road_tick`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_turn(
    v: VehicleRef,
    state: *mut State,
    execute: bool,
    leaves: *const Leaves,
) -> bool {
    entry!(leaves, None).turn(Part::new(v, state), execute)
}
/// # Safety
/// As `openttd_rust_road_tick`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_trackdir(
    v: VehicleRef,
    state: *mut State,
    leaves: *const Leaves,
) -> u8 {
    entry!(leaves, None).trackdir(Part::new(v, state))
}
/// # Safety
/// As `openttd_rust_road_tick`; `first` is the live first part of `v`'s consist.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_slope_pixel(
    v: VehicleRef,
    first: VehicleRef,
    first_state: *mut State,
    direction: u8,
    leaves: *const Leaves,
) -> bool {
    entry!(leaves, None).slope_pixel(v, Part::new(first, first_state), direction)
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
