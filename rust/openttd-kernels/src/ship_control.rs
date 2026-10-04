/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Complete ship controller and canonical private scalars. Shared path stays in `ship_yapf`.
#![allow(
    unsafe_code,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::too_many_lines,
    clippy::too_many_arguments,
    clippy::similar_names,
    clippy::items_after_statements,
    clippy::verbose_bit_mask,
    clippy::needless_late_init,
    clippy::if_not_else
)]
use crate::services::Services;
use crate::water_regions::Patch;
use std::cell::Cell;
use std::collections::{HashSet, VecDeque};
use std::ffi::c_void;
use std::future::Future;
use std::pin::Pin;
use std::rc::Rc;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::task::{Context, Poll, Waker};
static PROFILE_ENABLED: AtomicBool = AtomicBool::new(false);
static PROFILE: [AtomicU64; 16] = [const { AtomicU64::new(0) }; 16];
fn witness(branch: usize) {
    if PROFILE_ENABLED.load(Ordering::Relaxed) {
        PROFILE[branch].fetch_add(1, Ordering::Relaxed);
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_ship_control_profile_enable() {
    PROFILE_ENABLED.store(true, Ordering::Relaxed);
}
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_ship_control_profile(index: u8) -> u64 {
    PROFILE[usize::from(index)].load(Ordering::Relaxed)
}
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_ship_control_reload_rotation() {
    witness(10);
}

const INVALID: u32 = u32::MAX;
const DEPOT: u16 = 128;
const WORMHOLE: u16 = 64;
const INVALID_DIR: u16 = 255;
/// One private allocation per shell, including indexed load construction.
pub struct State {
    scalars: [u16; 4],
}
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_ship_state_new() -> *mut State {
    Box::into_raw(Box::new(State {
        scalars: [0, INVALID_DIR, 0, 0],
    }))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_ship_state_destroy(state: *mut State) {
    // SAFETY: Shell transfers its live allocation exactly once after PreDestructor.
    unsafe {
        drop(Box::from_raw(state));
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_ship_state_get(state: *const State, field: u8) -> u16 {
    // SAFETY: Live game-thread owner; selector 0..3, no retained reference.
    unsafe { (*state).scalars[field as usize] }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_ship_state_set(state: *mut State, field: u8, value: u16) {
    // SAFETY: Exclusive serialized owner access, exact source width truncation.
    unsafe {
        (*state).scalars[field as usize] = if field < 2 {
            u16::from(value as u8)
        } else {
            value
        };
    }
}
/// Only actual reentry boundaries suspend execution; RNG and world leaves are direct.
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Action {
    pub op: u32,
    pub id: u32,
    pub a: u64,
    pub b: u64,
    pub c: u64,
}
#[derive(Default)]
struct Mailbox {
    action: Cell<Action>,
    response: Cell<u64>,
}
struct Reentry {
    mailbox: Rc<Mailbox>,
    action: Action,
    yielded: bool,
}
impl Future for Reentry {
    type Output = u64;
    fn poll(mut self: Pin<&mut Self>, _: &mut Context<'_>) -> Poll<u64> {
        if self.yielded {
            Poll::Ready(self.mailbox.response.get())
        } else {
            self.mailbox.action.set(self.action);
            self.yielded = true;
            Poll::Pending
        }
    }
}
/// Copied synchronous services; no reference into a C++ object enters Rust.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Leaves {
    pub observe: extern "C" fn(u32, *mut View),
    pub write: extern "C" fn(u32, u32, u64),
    pub leaf: extern "C" fn(u32, u32, u64, u64, u64) -> u64,
    pub owner: extern "C" fn(u32) -> *mut State,
    pub patch: extern "C" fn(u32) -> Patch,
    pub neighbours: extern "C" fn(Patch, *mut Patch) -> usize,
    pub depots: extern "C" fn(*mut Depot, usize) -> usize,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Depot {
    pub id: u32,
    pub tile: u32,
    pub owner: u32,
    pub ship: u32,
}
#[derive(Clone)]
struct Game {
    leaves: Leaves,
    services: Services,
    mailbox: Rc<Mailbox>,
}
impl Game {
    fn read(&self, id: u32) -> View {
        let mut v = View::default();
        (self.leaves.observe)(id, &raw mut v);
        v
    }
    fn write(&self, id: u32, field: u32, value: u64) {
        (self.leaves.write)(id, field, value);
    }
    fn leaf(&self, op: u32, id: u32, a: u64, b: u64, c: u64) -> u64 {
        (self.leaves.leaf)(op, id, a, b, c)
    }
    async fn action(&self, op: u32, id: u32, a: u64, b: u64, c: u64) -> u64 {
        Reentry {
            mailbox: self.mailbox.clone(),
            action: Action { op, id, a, b, c },
            yielded: false,
        }
        .await
    }
    fn owner(&self, id: u32) -> *mut State {
        (self.leaves.owner)(id)
    }
    fn get(&self, id: u32, field: u8) -> u16 {
        // SAFETY: Resolve the live shell each time; borrow ends within the accessor.
        unsafe { openttd_rust_ship_state_get(self.owner(id), field) }
    }
    fn set(&self, id: u32, field: u8, value: u16) {
        // SAFETY: Serialized access, no Rust borrow spans a callback.
        unsafe {
            openttd_rust_ship_state_set(self.owner(id), field, value);
        }
    }
}
struct Task {
    future: Pin<Box<dyn Future<Output = u64>>>,
    mailbox: Rc<Mailbox>,
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_ship_control_create(
    kind: u32,
    id: u32,
    a: u64,
    b: u64,
    c: u64,
    leaves: *const Leaves,
    services: *const Services,
) -> *mut c_void {
    // SAFETY: Caller supplies live immutable tables; copy them before first service.
    let mailbox = Rc::new(Mailbox::default());
    let game = unsafe {
        Game {
            leaves: leaves.read(),
            services: services.read(),
            mailbox: mailbox.clone(),
        }
    };
    Box::into_raw(Box::new(Task {
        future: Box::pin(run(game, kind, id, a, b, c)),
        mailbox,
    }))
    .cast()
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_ship_control_advance(
    task: *mut c_void,
    response: u64,
) -> Action {
    // SAFETY: C++ owns the live invocation; nested road calls use distinct tasks.
    let task = unsafe { &mut *task.cast::<Task>() };
    task.mailbox.response.set(response);
    let mut context = Context::from_waker(Waker::noop());
    match task.future.as_mut().poll(&mut context) {
        Poll::Ready(result) => Action {
            op: 0,
            a: result,
            ..Action::default()
        },
        Poll::Pending => task.mailbox.action.get(),
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_ship_control_destroy(task: *mut c_void) {
    // SAFETY: Single RAII cleanup including C++ exception paths; drop touches no world.
    unsafe {
        drop(Box::from_raw(task.cast::<Task>()));
    }
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct View {
    pub tile: u32,
    pub dest: u32,
    pub x: u32,
    pub y: u32,
    pub z: u32,
    pub direction: u32,
    pub speed: u32,
    pub tick: u32,
    pub running: u32,
    pub day: u32,
    pub order_time: u32,
    pub progress: u32,
    pub status: u32,
    pub owner: u32,
    pub engine: u32,
    pub last_station: u32,
    pub order_destination: u32,
    pub order_type: u32,
    pub order_max_speed: u32,
    pub acceleration: u32,
    pub max_speed: u32,
}
const WRITE_TILE: u32 = 0;
const WRITE_X: u32 = 1;
const WRITE_Y: u32 = 2;
const WRITE_Z: u32 = 3;
const WRITE_DIRECTION: u32 = 4;
const WRITE_SPEED: u32 = 5;
const WRITE_TICK: u32 = 6;
const WRITE_RUNNING: u32 = 7;
const WRITE_DAY: u32 = 8;
const WRITE_ORDER_TIME: u32 = 9;
const WRITE_PROGRESS: u32 = 10;
const WRITE_LAST_STATION: u32 = 11;
const WRITE_HIDDEN: u32 = 12;
const WRITE_MAX_SPEED: u32 = 13;
const WRITE_CARGO_AGE: u32 = 14;
const WRITE_DEST: u32 = 15;
const DEPOT_DIR: u32 = 1;
const DEPOT_AXIS: u32 = 2;
const IS_DEPOT: u32 = 3;
const DEPOT_INDEX: u32 = 4;
const WAIT_UNBUNCH: u32 = 5;
const CHAIN_DEPOT: u32 = 6;
const SERVINT: u32 = 7;
const NEEDS_SERVICE: u32 = 8;
const MAX_DISTANCE: u32 = 9;
const TILE_VALID: u32 = 10;
const TILE_TYPE: u32 = 11;
const WATER_CLASS: u32 = 12;
const LOCK_MIDDLE: u32 = 13;
const LOCK_DIR: u32 = 14;
const TILE_MIN_Z: u32 = 15;
const TILE_MAX_Z: u32 = 16;
const TRACK_STATUS: u32 = 17;
const OFFSET: u32 = 18;
const DIAG_BETWEEN: u32 = 19;
const DIST_SQUARE: u32 = 20;
const DIST_MANHATTAN: u32 = 21;
const DOCKING: u32 = 22;
const DOCK: u32 = 23;
const DOCK_WATER: u32 = 24;
const STATION: u32 = 25;
const INDUSTRY_STATION: u32 = 26;
const OILRIG: u32 = 27;
const STATION_USE: u32 = 28;
const STATION_XY: u32 = 29;
const STATION_CONTAINS: u32 = 30;
const STATION_DOCK: u32 = 31;
const STATION_VISITS: u32 = 32;
const VISIT_SET: u32 = 33;
const ARRIVAL: u32 = 34;
const SERVICE: u32 = 35;
const LEAVE_UNBUNCH: u32 = 36;
const PATH_RESULT: u32 = 37;
const ORDER_FREE: u32 = 38;
const ORDER_DUMMY: u32 = 39;
const ORDER_DEPOT: u32 = 40;
const ORDER_LEAVE: u32 = 41;
const ORDER_INCREMENT: u32 = 42;
const TIMETABLE: u32 = 43;
const POSITION: u32 = 44;
const START_DIRTY: u32 = 45;
const DEPOT_DIRTY: u32 = 46;
const DEPOT_INVALIDATE: u32 = 47;
const SHIPS_DIRTY: u32 = 48;
const DETAILS_DIRTY: u32 = 49;
const AGE: u32 = 50;
const ECONOMY_AGE: u32 = 51;
const DECREASE_VALUE: u32 = 52;
const CHECK_BREAKDOWN: u32 = 53;
const CHECK_ORDERS: u32 = 54;
const RUNNING_COST: u32 = 55;
const COST_DIVISOR: u32 = 56;
const PAY_RUNNING: u32 = 57;
const SPEED_DEFAULT: u32 = 58;
const AGE_DEFAULT: u32 = 59;
const SPEED_FRAC: u32 = 60;
const PROPERTY: u32 = 61;
const UPDATE_VISUAL: u32 = 62;
const CACHE_INVALIDATE: u32 = 63;
const CAPACITY: u32 = 64;
const BUILD_SHARED: u32 = 65;
const SPRITE_DIRECTION: u32 = 66;
const NEW_POSITION: u32 = 68;
const VIRT_TILE: u32 = 69;
const EXIT_DIR: u32 = 70;
const TRACK_DIRECTION: u32 = 71;
const TRACKS_REACH: u32 = 72;
const BUSY_TILE: u32 = 73;
const PATH_SIZE: u32 = 74;
const PATH_BACK: u32 = 75;
const PATH_POP: u32 = 76;
const PATH_CLEAR: u32 = 77;
const ENTER_TILE: u32 = 128;
const ENTER_DEPOT: u32 = 129;
const PROCESS_ORDERS: u32 = 130;
const LOADING: u32 = 131;
const BEGIN_LOADING: u32 = 132;
const BREAKDOWN: u32 = 133;
const VIEWPORT: u32 = 134;
const BASE_VIEWPORT: u32 = 135;
const VISUAL: u32 = 136;
const CACHE: u32 = 137;
const PLAY_SOUND: u32 = 138;
const YAPF_REVERSE: u32 = 139;
const YAPF_CHOOSE: u32 = 140;
const UPDATE_DELTA: u32 = 141;
const TILE_X: u32 = 90;
const TILE_Y: u32 = 91;
const BUILD_FLAG: u32 = 92;
const BUILD_RANDOM: u32 = 93;

impl Game {
    fn q(&self, op: u32, id: u32, a: u32) -> u32 {
        self.leaf(op, id, u64::from(a), 0, 0) as u32
    }
    fn do_leaf(&self, op: u32, id: u32) {
        self.leaf(op, id, 0, 0, 0);
    }
    async fn do_action(&self, op: u32, id: u32) {
        self.action(op, id, 0, 0, 0).await;
    }
    async fn viewport(&self, id: u32) {
        self.action(VIEWPORT, id, 1, 1, 0).await;
    }
    fn path_clear(&self, id: u32) {
        self.do_leaf(PATH_CLEAR, id);
    }
    fn destination(&self, id: u32, tile: u32) {
        if tile == self.read(id).dest {
            return;
        }
        self.path_clear(id);
        self.write(id, WRITE_DEST, u64::from(tile));
    }
    fn remember_rotation(&self, id: u32) {
        let v = self.read(id);
        self.set(id, 2, v.x as u16);
        self.set(id, 3, v.y as u16);
    }
    async fn reverse(&self, id: u32, trackdir: u32) {
        witness(2);
        if trackdir == INVALID {
            self.write(
                id,
                WRITE_DIRECTION,
                u64::from((self.read(id).direction + 4) & 7),
            );
        } else {
            const DIRS: [u32; 16] = [1, 3, 2, 2, 4, 4, 255, 255, 5, 7, 6, 6, 0, 0, 255, 255];
            self.write(id, WRITE_DIRECTION, u64::from(DIRS[trackdir as usize]));
            self.set(id, 0, 1 << (trackdir & 7));
        }
        self.remember_rotation(id);
        self.write(id, WRITE_SPEED, 0);
        self.path_clear(id);
        self.do_leaf(POSITION, id);
        self.viewport(id).await;
    }
    fn depot(&self, id: u32, max_distance: u32) -> Option<Depot> {
        witness(11);
        let v = self.read(id);
        let start = (self.leaves.patch)(v.tile);
        let mut visited = HashSet::new();
        let mut queue = VecDeque::new();
        let key = |p: Patch| (p.x, p.y, p.label);
        visited.insert(key(start));
        queue.push_back(start);
        let max_region = (max_distance / 16 + 1) as i32;
        while let Some(current) = queue.pop_front() {
            let mut neighbours = [Patch {
                x: 0,
                y: 0,
                label: 0,
            }; 320];
            let count = (self.leaves.neighbours)(current, neighbours.as_mut_ptr());
            for patch in &neighbours[..count] {
                if (patch.x - start.x).abs() > max_region || (patch.y - start.y).abs() > max_region
                {
                    continue;
                }
                if visited.insert(key(*patch)) {
                    queue.push_back(*patch);
                }
            }
        }
        let count = (self.leaves.depots)(std::ptr::null_mut(), 0);
        let mut depots = vec![Depot::default(); count];
        (self.leaves.depots)(depots.as_mut_ptr(), count);
        let mut best = None;
        let mut best_distance = u32::MAX;
        for depot in depots {
            if depot.ship == 0 || depot.owner != v.owner {
                continue;
            }
            let distance =
                self.leaf(DIST_SQUARE, id, u64::from(depot.tile), u64::from(v.tile), 0) as u32;
            if distance < best_distance
                && distance <= max_distance.wrapping_mul(max_distance)
                && visited.contains(&key((self.leaves.patch)(depot.tile)))
            {
                best_distance = distance;
                best = Some(depot);
            }
        }
        best
    }
    fn service(&self, id: u32) {
        if self.q(SERVINT, id, 0) == 0 || self.q(NEEDS_SERVICE, id, 0) == 0 {
            return;
        }
        if self.q(CHAIN_DEPOT, id, 0) != 0 {
            self.do_leaf(SERVICE, id);
            return;
        }
        let distance = self.q(MAX_DISTANCE, id, 0);
        if let Some(depot) = self.depot(id, distance) {
            witness(5);
            self.q(ORDER_DEPOT, id, depot.id);
            self.destination(id, depot.tile);
            self.do_leaf(START_DIRTY, id);
        } else if self.read(id).order_type == 2 {
            self.do_leaf(ORDER_DUMMY, id);
            self.do_leaf(START_DIRTY, id);
        }
    }
    fn cache(&self, id: u32) {
        let v = self.read(id);
        let sea = self.q(WATER_CLASS, id, v.tile) == 0;
        let speed = self.q(SPEED_DEFAULT, id, 0);
        let raw = self.leaf(PROPERTY, id, 0, u64::from(speed), 0) as u32;
        let frac = self.q(SPEED_FRAC, id, u32::from(sea));
        self.write(
            id,
            WRITE_MAX_SPEED,
            u64::from(raw.wrapping_mul(256 - frac) / 256),
        );
        let period = self.q(AGE_DEFAULT, id, 0);
        let age = self.leaf(PROPERTY, id, 1, u64::from(period), 0);
        self.write(id, WRITE_CARGO_AGE, age);
        self.do_leaf(UPDATE_VISUAL, id);
    }
    async fn stay_depot(&self, id: u32) -> bool {
        if self.q(CHAIN_DEPOT, id, 0) == 0 {
            return false;
        }
        if self.q(WAIT_UNBUNCH, id, 0) != 0 {
            return true;
        }
        let v = self.read(id);
        if v.order_type == 2
            && self.q(IS_DEPOT, id, v.tile) != 0
            && self.q(DEPOT_INDEX, id, v.tile) == v.order_destination
        {
            self.do_action(ENTER_DEPOT, id).await;
            return true;
        }
        if v.dest == 0 || self.q(BUSY_TILE, id, 0) != 0 {
            return true;
        }
        let mut direction = self.q(DEPOT_DIR, id, v.tile) * 2 + 1;
        self.write(id, WRITE_DIRECTION, u64::from(direction));
        if self.action(YAPF_REVERSE, id, 0, 0, 0).await & 1 != 0 {
            direction = (direction + 4) & 7;
            self.write(id, WRITE_DIRECTION, u64::from(direction));
        }
        witness(4);
        self.set(id, 0, 1 << self.q(DEPOT_AXIS, id, v.tile));
        self.set(id, 1, direction as u16);
        self.write(id, WRITE_HIDDEN, 0);
        self.write(id, WRITE_SPEED, 0);
        self.viewport(id).await;
        self.do_leaf(DEPOT_DIRTY, id);
        self.do_leaf(SERVICE, id);
        self.do_leaf(LEAVE_UNBUNCH, id);
        self.do_action(PLAY_SOUND, id).await;
        self.do_leaf(DEPOT_INVALIDATE, id);
        self.do_leaf(SHIPS_DIRTY, id);
        false
    }
    fn lock_delta(&self, id: u32) -> i32 {
        let v = self.read(id);
        if self.q(LOCK_MIDDLE, id, v.tile) == 0 || v.x & 15 != 8 || v.y & 15 != 8 {
            return 0;
        }
        let diag = self.q(LOCK_DIR, id, v.tile);
        if v.direction / 2 == diag {
            i32::from((v.z as i32) < self.q(TILE_MAX_Z, id, v.tile) as i32 * 8)
        } else {
            -i32::from((v.z as i32) > self.q(TILE_MIN_Z, id, v.tile) as i32 * 8)
        }
    }
    async fn move_lock(&self, id: u32) -> bool {
        let delta = self.lock_delta(id);
        if delta > 0 {
            witness(0);
        } else if delta < 0 {
            witness(1);
        }
        if delta == 0 {
            return false;
        }
        let v = self.read(id);
        if v.speed != 0 {
            self.write(id, WRITE_SPEED, 0);
            self.do_leaf(START_DIRTY, id);
        }
        if v.tick & 7 == 0 {
            self.write(id, WRITE_Z, u64::from(v.z.wrapping_add(delta as u32)));
            self.do_leaf(POSITION, id);
            self.viewport(id).await;
        }
        true
    }
    fn accelerate(&self, id: u32) -> u32 {
        let v = self.read(id);
        let speed = v
            .speed
            .wrapping_add(v.acceleration)
            .min(v.max_speed)
            .min(v.order_max_speed.wrapping_mul(2));
        if speed != v.speed {
            self.write(id, WRITE_SPEED, u64::from(speed));
            self.do_leaf(START_DIRTY, id);
        }
        let advance = speed.wrapping_mul(3) / 4;
        let distance = if v.direction & 1 != 0 { 192 } else { 256 };
        let progress = advance.wrapping_add(v.progress);
        self.write(id, WRITE_PROGRESS, u64::from(progress % distance));
        progress / distance
    }
    async fn choose(&self, id: u32, tile: u32, tracks: u32) -> u32 {
        let found;
        let track;
        if self.read(id).dest == 0 {
            let mut choice = u32::from(self.get(id, 0)).trailing_zeros();
            if choice >= 2 {
                choice ^= 1;
            }
            if tracks & (1 << choice) == 0 {
                choice = tracks.trailing_zeros();
            }
            track = choice;
            found = false;
        } else {
            if self.q(PATH_SIZE, id, 0) != 0 {
                witness(14);
                let cached = self.q(PATH_BACK, id, 0) & 7;
                if tracks & (1 << cached) != 0 {
                    self.do_leaf(PATH_POP, id);
                    return cached;
                }
                self.path_clear(id);
            }
            let result = self
                .action(YAPF_CHOOSE, id, u64::from(tile), u64::from(tracks), 0)
                .await;
            track = result as u32 & 255;
            found = result & 256 != 0;
        }
        self.q(PATH_RESULT, id, u32::from(found));
        track
    }
    fn destination_station(&self, id: u32, tile: u32, station: u32) -> bool {
        for dir in 0..4 {
            let adjacent = tile.wrapping_add(self.q(OFFSET, id, dir));
            if self.q(TILE_VALID, id, adjacent) == 0 {
                continue;
            }
            if self.q(DOCK, id, adjacent) != 0
                && self.q(STATION, id, adjacent) == station
                && self.q(DOCK_WATER, id, adjacent) != 0
            {
                return true;
            }
            if self.q(TILE_TYPE, id, adjacent) == 8
                && self.q(INDUSTRY_STATION, id, adjacent) == station
            {
                return true;
            }
            if self.q(OILRIG, id, adjacent) != 0 && self.q(STATION, id, adjacent) == station {
                return true;
            }
        }
        false
    }
    fn arrives(&self, id: u32, station: u32) {
        let visits = self.q(STATION_VISITS, id, station);
        if visits & 32 == 0 {
            self.leaf(VISIT_SET, id, u64::from(station), 32, 0);
            self.q(ARRIVAL, id, station);
        }
    }
    async fn controller(&self, id: u32) {
        let v = self.read(id);
        self.write(id, WRITE_TICK, u64::from(v.tick.wrapping_add(1)));
        self.write(
            id,
            WRITE_ORDER_TIME,
            u64::from(v.order_time.wrapping_add(1)),
        );
        if self.action(BREAKDOWN, id, 0, 0, 0).await != 0 {
            return;
        }
        if self.read(id).status & 2 != 0 {
            return;
        }
        if self.action(PROCESS_ORDERS, id, 0, 0, 0).await != 0
            && self.action(YAPF_REVERSE, id, 0, 0, 0).await & 1 != 0
        {
            self.reverse(id, INVALID).await;
            return;
        }
        self.do_action(LOADING, id).await;
        if self.read(id).order_type == 3 {
            return;
        }
        if self.stay_depot(id).await {
            return;
        }
        self.do_action(VISUAL, id).await;
        let v = self.read(id);
        let rotation = u32::from(self.get(id, 1));
        if v.direction != rotation {
            witness(3);
            if v.tick & 7 == 0 {
                let diff = v.direction.wrapping_sub(rotation) & 7;
                self.set(
                    id,
                    1,
                    ((rotation + if diff > 4 { 7 } else { 1 }) & 7) as u16,
                );
                self.do_leaf(SPRITE_DIRECTION, id);
                self.viewport(id).await;
            }
            return;
        }
        if self.move_lock(id).await {
            return;
        }
        let steps = self.accelerate(id);
        for _ in 0..steps {
            if self.move_lock(id).await {
                return;
            }
            let v = self.read(id);
            let position = self.leaf(NEW_POSITION, id, 0, 0, 0);
            let mut x = position as u32;
            let mut y = (position >> 32) as u32;
            let old_tile = v.tile;
            let new_tile = self.leaf(VIRT_TILE, id, u64::from(x), u64::from(y), 0) as u32;
            if self.get(id, 0) != WORMHOLE {
                if old_tile == new_tile {
                    if self.get(id, 0) == DEPOT {
                        x = v.x;
                        y = v.y;
                    } else {
                        if self
                            .action(
                                ENTER_TILE,
                                id,
                                u64::from(new_tile),
                                u64::from(x),
                                u64::from(y),
                            )
                            .await
                            & 4
                            != 0
                        {
                            self.reverse(id, INVALID).await;
                            return;
                        }
                        let v = self.read(id);
                        if v.order_type == 4 {
                            self.do_leaf(ORDER_FREE, id);
                            self.do_leaf(START_DIRTY, id);
                            let exit = self.leaf(
                                EXIT_DIR,
                                id,
                                u64::from(v.direction),
                                u64::from(self.get(id, 0)),
                                0,
                            ) as u32;
                            let tile = v.tile.wrapping_add(self.q(OFFSET, id, exit));
                            if self.leaf(TRACK_STATUS, id, u64::from(tile), u64::from(exit), 0) == 0
                            {
                                self.reverse(id, INVALID).await;
                                return;
                            }
                        } else if v.dest != 0 {
                            if v.order_type == 6
                                && self.leaf(
                                    DIST_MANHATTAN,
                                    id,
                                    u64::from(v.dest),
                                    u64::from(new_tile),
                                    0,
                                ) <= 3
                            {
                                witness(6);
                                self.do_leaf(TIMETABLE, id);
                                self.do_leaf(ORDER_INCREMENT, id);
                                self.do_leaf(ORDER_DUMMY, id);
                            } else if v.order_type == 2 && v.dest == new_tile {
                                if x & 15 == 8 && y & 15 == 8 {
                                    witness(15);
                                    self.do_action(ENTER_DEPOT, id).await;
                                    return;
                                }
                            } else if v.order_type == 1 && self.q(DOCKING, id, new_tile) != 0 {
                                let station = v.order_destination;
                                if self.leaf(
                                    STATION_CONTAINS,
                                    id,
                                    u64::from(station),
                                    u64::from(new_tile),
                                    0,
                                ) != 0
                                    && self.destination_station(id, new_tile, station)
                                {
                                    self.write(id, WRITE_LAST_STATION, u64::from(station));
                                    if self.q(STATION_DOCK, id, station) != 0 {
                                        witness(7);
                                        self.arrives(id, station);
                                        self.do_action(BEGIN_LOADING, id).await;
                                    } else {
                                        self.do_leaf(ORDER_LEAVE, id);
                                        self.do_leaf(ORDER_INCREMENT, id);
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if self.q(TILE_VALID, id, new_tile) == 0 {
                        self.reverse(id, INVALID).await;
                        return;
                    }
                    let diag = self.leaf(
                        DIAG_BETWEEN,
                        id,
                        u64::from(old_tile),
                        u64::from(new_tile),
                        0,
                    ) as u32;
                    let tracks = self.leaf(TRACK_STATUS, id, u64::from(new_tile), 255, 0) as u32
                        & self.q(TRACKS_REACH, id, diag);
                    if tracks == 0 {
                        let result = self.action(YAPF_REVERSE, id, 1, 0, 0).await;
                        let trackdir = (result >> 8) as u32;
                        self.reverse(id, if trackdir == 255 { INVALID } else { trackdir })
                            .await;
                        return;
                    }
                    let track = self.choose(id, new_tile, tracks).await;
                    if track == 255 {
                        self.reverse(id, INVALID).await;
                        return;
                    }
                    let (sx, sy, direction) = SUBCOORD[diag as usize][track as usize];
                    x = (x & !15) | u32::from(sx);
                    y = (y & !15) | u32::from(sy);
                    let entered = self
                        .action(
                            ENTER_TILE,
                            id,
                            u64::from(new_tile),
                            u64::from(x),
                            u64::from(y),
                        )
                        .await;
                    if entered & 4 != 0 {
                        self.reverse(id, INVALID).await;
                        return;
                    }
                    if entered & 2 == 0 {
                        self.write(id, WRITE_TILE, u64::from(new_tile));
                        self.set(id, 0, 1 << track);
                        if self.q(WATER_CLASS, id, old_tile) != self.q(WATER_CLASS, id, new_tile) {
                            witness(8);
                            self.do_action(CACHE, id).await;
                        }
                    }
                    let diff = u32::from(direction).wrapping_sub(self.read(id).direction) & 7;
                    if diff == 0 || diff == 1 || diff == 7 {
                        self.set(id, 1, u16::from(direction));
                        self.write(id, WRITE_DIRECTION, u64::from(direction));
                    } else {
                        self.write(id, WRITE_SPEED, 0);
                        self.write(id, WRITE_DIRECTION, u64::from(direction));
                        self.remember_rotation(id);
                    }
                }
            } else {
                witness(9);
                if self.q(TILE_TYPE, id, new_tile) != 9
                    || self
                        .action(
                            ENTER_TILE,
                            id,
                            u64::from(new_tile),
                            u64::from(x),
                            u64::from(y),
                        )
                        .await
                        & 2
                        == 0
                {
                    self.write(id, WRITE_X, u64::from(x));
                    self.write(id, WRITE_Y, u64::from(y));
                    self.do_leaf(POSITION, id);
                    if self.read(id).status & 1 == 0 {
                        self.do_action(BASE_VIEWPORT, id).await;
                    }
                    continue;
                }
                if self.q(PATH_SIZE, id, 0) != 0 {
                    self.do_leaf(PATH_POP, id);
                }
            }
            self.write(id, WRITE_X, u64::from(x));
            self.write(id, WRITE_Y, u64::from(y));
            self.do_leaf(POSITION, id);
            self.viewport(id).await;
        }
    }
}
const SUBCOORD: [[(u8, u8, u8); 6]; 4] = [
    [
        (15, 8, 1),
        (0, 0, 255),
        (0, 0, 255),
        (15, 8, 2),
        (15, 7, 0),
        (0, 0, 255),
    ],
    [
        (0, 0, 255),
        (8, 0, 3),
        (7, 0, 2),
        (0, 0, 255),
        (8, 0, 4),
        (0, 0, 255),
    ],
    [
        (0, 8, 5),
        (0, 0, 255),
        (0, 7, 6),
        (0, 0, 255),
        (0, 0, 255),
        (0, 8, 4),
    ],
    [
        (0, 0, 255),
        (8, 15, 7),
        (0, 0, 255),
        (8, 15, 6),
        (0, 0, 255),
        (7, 15, 0),
    ],
];
async fn run(game: Game, kind: u32, id: u32, a: u64, b: u64, _c: u64) -> u64 {
    match kind {
        0 => {
            let v = game.read(id);
            if v.status & 2 == 0 {
                game.write(id, WRITE_RUNNING, u64::from(v.running.wrapping_add(1)));
            }
            game.controller(id).await;
            1
        }
        1 => {
            game.do_leaf(AGE, id);
            0
        }
        2 => {
            witness(13);
            game.do_leaf(ECONOMY_AGE, id);
            let day = game.read(id).day.wrapping_add(1);
            game.write(id, WRITE_DAY, u64::from(day));
            if day & 7 == 0 {
                game.do_leaf(DECREASE_VALUE, id);
            }
            game.do_leaf(CHECK_BREAKDOWN, id);
            game.service(id);
            game.do_leaf(CHECK_ORDERS, id);
            let ticks = game.read(id).running;
            if ticks != 0 {
                let cost = game.leaf(RUNNING_COST, id, 0, 0, 0) as i64;
                let cost =
                    cost.saturating_mul(i64::from(ticks)) / i64::from(game.q(COST_DIVISOR, id, 0));
                game.leaf(PAY_RUNNING, id, cost as u64, 0, 0);
                game.do_leaf(DETAILS_DIRTY, id);
                game.do_leaf(SHIPS_DIRTY, id);
            }
            0
        }
        3 => {
            game.cache(id);
            0
        }
        4 => {
            game.destination(id, a as u32);
            0
        }
        5 => game
            .depot(id, a as u32)
            .map_or(u64::MAX, |d| u64::from(d.tile) | (u64::from(d.id) << 32)),
        6 => u64::from(game.destination_station(id, a as u32, b as u32)),
        7 => {
            let station = a as u32;
            if station == game.read(id).last_station {
                game.write(id, WRITE_LAST_STATION, 65535);
            }
            if game.q(STATION_USE, id, station) != 0 {
                u64::from(game.q(STATION_XY, id, station))
            } else {
                game.do_leaf(ORDER_INCREMENT, id);
                0
            }
        }
        8 => {
            let v = game.read(id);
            if v.status & 128 != 0 {
                255
            } else if game.get(id, 0) == DEPOT {
                u64::from([0_u32, 1, 8, 9][game.q(DEPOT_DIR, id, v.tile) as usize])
            } else if game.get(id, 0) == WORMHOLE {
                u64::from([0_u32, 1, 8, 9][(v.direction / 2) as usize])
            } else {
                game.leaf(
                    TRACK_DIRECTION,
                    id,
                    u64::from(u32::from(game.get(id, 0)).trailing_zeros()),
                    u64::from(v.direction),
                    0,
                )
            }
        }
        9 => {
            witness(12);
            let tile = game.read(id).tile;
            let x = game.q(TILE_X, id, tile).wrapping_mul(16) + 8;
            let y = game.q(TILE_Y, id, tile).wrapping_mul(16) + 8;
            game.leaf(BUILD_SHARED, id, 0, a, 0);
            game.write(id, WRITE_X, u64::from(x));
            game.write(id, WRITE_Y, u64::from(y));
            game.leaf(BUILD_SHARED, id, 1, a, 0);
            let direction = game.q(DEPOT_DIR, id, tile) * 2 + 1;
            game.write(id, WRITE_DIRECTION, u64::from(direction));
            game.set(id, 1, direction as u16);
            game.do_action(UPDATE_DELTA, id).await;
            game.leaf(BUILD_SHARED, id, 2, a, 0);
            game.set(id, 0, DEPOT);
            game.leaf(BUILD_SHARED, id, 3, a, 0);
            game.leaf(BUILD_RANDOM, id, u64::from(game.services.random()), 0, 0);
            game.leaf(BUILD_SHARED, id, 4, a, 0);
            game.do_action(CACHE, id).await;
            if game.q(BUILD_FLAG, id, a as u32) != 0 {
                game.leaf(BUILD_SHARED, id, 5, a, 0);
            }
            game.leaf(BUILD_SHARED, id, 6, a, 0);
            game.do_leaf(CACHE_INVALIDATE, id);
            let capacity = game.q(CAPACITY, id, 0);
            game.leaf(BUILD_SHARED, id, 7, a, u64::from(capacity));
            game.do_leaf(CACHE_INVALIDATE, id);
            game.do_leaf(POSITION, id);
            0
        }
        _ => unreachable!("C++ chooses a defined ship entry kind"),
    }
}
