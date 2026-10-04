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
use std::cell::Cell;
use std::ffi::c_void;
use std::future::Future;
use std::pin::Pin;
use std::rc::Rc;
use std::task::{Context, Poll, Waker};

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
/// Synchronous copied game services. Leaves cannot reenter or destroy owners.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Leaves {
    pub observe: extern "C" fn(u32, *mut View),
    pub write: extern "C" fn(u32, u32, u64),
    pub leaf: extern "C" fn(u32, u32, u64, u64, u64) -> u64,
    pub owner: extern "C" fn(u32) -> *mut State,
    pub nearby: extern "C" fn(u32, u32, i32, i32, *mut u32, usize) -> usize,
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
    fn nearby(&self, id: u32, kind: u32, x: i32, y: i32) -> Vec<u32> {
        let n = (self.leaves.nearby)(id, kind, x, y, std::ptr::null_mut(), 0);
        let mut result = vec![0; n];
        (self.leaves.nearby)(id, kind, x, y, result.as_mut_ptr(), n);
        result
    }
    fn random(&self) -> u32 {
        (self.services.random)(self.services.context)
    }
}
struct Task {
    future: Pin<Box<dyn Future<Output = u64>>>,
    mailbox: Rc<Mailbox>,
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_road_create(
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
pub unsafe extern "C" fn openttd_rust_road_advance(task: *mut c_void, response: u64) -> Action {
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
pub unsafe extern "C" fn openttd_rust_road_task_destroy(task: *mut c_void) {
    // SAFETY: Single RAII cleanup including C++ exception paths; drop touches no world.
    unsafe {
        drop(Box::from_raw(task.cast::<Task>()));
    }
}
/// Copied shared vehicle observation, refreshed at each original observation point.
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct View {
    pub r#type: u32,
    pub first: u32,
    pub next: u32,
    pub previous: u32,
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
    pub breakdown: u32,
    pub max_track_speed: u32,
    pub length: u32,
    pub total_length: u32,
    pub roadtype: u32,
    pub front: u32,
    pub articulated: u32,
    pub tram: u32,
    pub bus: u32,
    pub order_nonstop: u32,
}
const WRITE_TILE: u32 = 0;
const WRITE_X: u32 = 1;
const WRITE_Y: u32 = 2;
const WRITE_DIRECTION: u32 = 3;
const WRITE_SPEED: u32 = 4;
const WRITE_TICK: u32 = 5;
const WRITE_RUNNING: u32 = 6;
const WRITE_DAY: u32 = 7;
const WRITE_ORDER_TIME: u32 = 8;
const WRITE_PROGRESS: u32 = 9;
const WRITE_LAST_STATION: u32 = 10;
const WRITE_HIDDEN: u32 = 11;
const WRITE_FIRST_ENGINE: u32 = 12;
const WRITE_LENGTH: u32 = 13;
const WRITE_TOTAL_LENGTH: u32 = 14;
const WRITE_CARGO_AGE: u32 = 15;
const WRITE_MAX_SPEED: u32 = 16;
const WRITE_SUPPRESS_IMPLICIT: u32 = 17;

const ACC_MODEL: u32 = 1;
const ROAD_SIDE: u32 = 2;
const TILE_TYPE: u32 = 3;
const HAS_ROAD: u32 = 4;
const TRACK_STATUS: u32 = 5;
const TILE_OWNER: u32 = 6;
const DEPOT_DIR: u32 = 7;
const BAY_DIR: u32 = 8;
const IS_DEPOT: u32 = 9;
const NORMAL_ROAD: u32 = 10;
const ROAD_WORKS: u32 = 11;
const DISALLOWED: u32 = 12;
const BAY_STOP: u32 = 13;
const IS_DT_STOP: u32 = 14;
const STOP_TYPE: u32 = 15;
const FREE_BAY: u32 = 16;
const ANY_ROAD_BITS: u32 = 17;
const ROAD_BITS: u32 = 18;

const OFFSET: u32 = 20;
const TILE_X: u32 = 21;
const TILE_Y: u32 = 22;
const STATION: u32 = 23;
const CONTINUATION: u32 = 24;
const BRIDGE_SPEED: u32 = 25;
const MAX_PENALTY: u32 = 26;
const SERVINT: u32 = 27;
const NEEDS_SERVICE: u32 = 28;

const WAIT_UNBUNCH: u32 = 30;
const ORDER_STOP: u32 = 31;
const ROAD_TYPE: u32 = 32;
const QUEUE: u32 = 33;
const TUNNEL_DIR: u32 = 34;
const ACCELERATION: u32 = 35;
const UPDATE_SPEED: u32 = 36;
const ADVANCE: u32 = 37;
const INCLINATION: u32 = 151;
const POSITION: u32 = 39;
const VIEWPORT: u32 = 152;
const BASE_VIEWPORT: u32 = 41;
const LAST_SPEED: u32 = 42;
const ROADSTOP_LEAVE: u32 = 43;
const ENTRANCE_SET: u32 = 44;
const ENTRANCE_BUSY: u32 = 45;
const ORDER_FREE: u32 = 46;
const SET_NEXT: u32 = 47;
const START_STOP_DIRTY: u32 = 48;
const DEPOT_DIRTY: u32 = 49;
const DETAILS_DIRTY: u32 = 50;
const SERVICE: u32 = 51;
const LEAVE_UNBUNCH: u32 = 52;
const RESET_UNBUNCH: u32 = 53;
const PATH_RESULT: u32 = 54;
const ORDER_DUMMY: u32 = 55;
const ORDER_DEPOT: u32 = 56;
const DEPOT_INDEX: u32 = 57;
const DECREASE_VALUE: u32 = 58;
const AGE: u32 = 59;
const ECONOMY_AGE: u32 = 60;
const CHECK_BREAKDOWN: u32 = 61;
const CHECK_ORDERS: u32 = 62;
const PAY_RUNNING: u32 = 63;
const COST_CLASS: u32 = 64;
const COST_FACTOR: u32 = 65;
const GET_PRICE: u32 = 66;
const GRF_VERSION: u32 = 67;
const LENGTH_DEFAULT: u32 = 68;
const AGE_DEFAULT: u32 = 69;
const SPEED_DEFAULT: u32 = 70;
const LENGTH_ERROR: u32 = 71;
const DISCONNECT: u32 = 72;
const EXPLOSION: u32 = 73;
const SOUND_DEFAULT: u32 = 74;
const SOUND: u32 = 75;
const SOUND_OLD1: u32 = 76;
const SOUND_OLD2: u32 = 77;

const ENGINE_INVALID: u32 = 79;
const INVALID_PRICE: u32 = 80;
const COST_DIVISOR: u32 = 81;
const ENTER_TILE: u32 = 128;
const ENTER_DEPOT: u32 = 129;
const PROCESS_ORDERS: u32 = 130;
const LOADING: u32 = 131;
const BEGIN_LOADING: u32 = 132;
const TRAM_PROBE: u32 = 133;
const PROPERTY: u32 = 134;
const LENGTH_CALLBACK: u32 = 135;
const PLAY_SOUND: u32 = 136;
const VISUAL: u32 = 137;
const UPDATE_VISUAL: u32 = 138;
const CARGO_CHANGED: u32 = 139;
const CACHE_INVALIDATE: u32 = 140;
const LENGTH_CHANGED: u32 = 141;
const BREAKDOWN: u32 = 142;
const DELETE: u32 = 143;
const GROUND_CRASH: u32 = 144;
const ARRIVAL: u32 = 145;
const STOP_RANDOM: u32 = 146;
const STOP_ANIMATION: u32 = 147;
const CRASH_NEWS: u32 = 148;
const YAPF: u32 = 149;
const FIND_DEPOT: u32 = 150;
impl View {
    fn stopped(self) -> bool {
        self.status & 2 != 0
    }
    fn crashed(self) -> bool {
        self.status & 128 != 0
    }
    fn hidden(self) -> bool {
        self.status & 1 != 0
    }
    fn front(self) -> bool {
        self.front != 0
    }
    fn tram(self) -> bool {
        self.tram != 0
    }
}
impl Game {
    fn q(&self, op: u32, id: u32, a: u32, b: u32) -> u32 {
        self.leaf(op, id, u64::from(a), u64::from(b), 0) as u32
    }
    fn q0(&self, op: u32, id: u32) -> u32 {
        self.q(op, id, 0, 0)
    }
    fn tile(&self, op: u32, id: u32, tile: u32) -> u32 {
        self.q(op, id, tile, 0)
    }
    fn effect(&self, op: u32, id: u32) {
        self.q0(op, id);
    }
    fn realistic(&self) -> bool {
        self.q0(ACC_MODEL, INVALID) == 1
    }
    fn side(&self) -> usize {
        (self.q0(ROAD_SIDE, INVALID) as usize) << 4
    }
    fn max_speed(&self, id: u32) -> i32 {
        let v = self.read(id);
        let mut speed = v.max_track_speed as i32;
        let mut uid = id;
        while uid != INVALID {
            let u = self.read(uid);
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
                speed = speed.min((self.tile(BRIDGE_SPEED, uid, u.tile) * 2) as i32);
            }
            uid = u.next;
        }
        speed.min((v.order_max_speed * 2) as i32)
    }
    fn update_speed(&self, id: u32) -> i32 {
        let over = self.get(id, 3) != 0;
        let (accel, min) = if self.realistic() {
            (
                (self.q0(ACCELERATION, id) as i32).wrapping_add(if over { 256 } else { 0 }) as u32,
                if self.read(id).stopped() { 0 } else { 4 },
            )
        } else {
            (if over { 512 } else { 256 }, 0)
        };
        self.leaf(
            UPDATE_SPEED,
            id,
            u64::from(accel),
            min,
            self.max_speed(id) as u64,
        ) as i32
    }
    fn close(&self, id: u32, x: i32, y: i32, dir: u32, update: bool) -> u32 {
        let v = self.read(id);
        let front = v.first;
        if self.get(front, 6) != 0 {
            return INVALID;
        }
        let mut best = INVALID;
        let mut best_diff = u32::MAX;
        let kind = u32::from(self.get(front, 0) == WORMHOLE);
        for uid in self.nearby(id, kind, x, y) {
            let u = self.read(uid);
            let xd = u.x as i32 - x;
            let yd = u.y as i32 - y;
            if u.r#type != 1 || self.get(uid, 0) == DEPOT || v.first == u.first {
                continue;
            }
            if (u.z as i32 - v.z as i32).abs() >= 6 || u.direction != dir {
                continue;
            }
            let diff = xd.unsigned_abs() + yd.unsigned_abs();
            if diff > best_diff || (diff == best_diff && uid > best) {
                continue;
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
        }
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
        if self.tile(HAS_ROAD, id, tile) == 0 {
            return true;
        }
        let status = self.tile(TRACK_STATUS, id, tile);
        let tracks = status & 0x3f3f;
        let red = (status >> 16) & 0x3f3f;
        let bits = ((tracks | (tracks >> 8)) & 63) as u8;
        if tracks & (1 << dir) == 0 || bits & !3 != 0 || red != 0 {
            return true;
        }
        self.nearby(id, 2, tile as i32, 0).into_iter().any(|uid| {
            let u = self.read(uid);
            u.r#type == 1 && u.first == uid && uid != id && uid != other
        })
    }
    fn overtake(&self, id: u32, other: u32) {
        let v = self.read(id);
        let u = self.read(other);
        if v.tram()
            || self.tile(TILE_TYPE, id, v.tile) == 5
            || self.tile(TILE_TYPE, id, u.tile) == 5
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
        let uspeed = if !self.realistic() || (self.q0(ACCELERATION, other) as i32) > 0 {
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
            .wrapping_add(self.q(OFFSET, id, diag(v.direction), 0));
        if self.overtake_blocked(id, other, tile, dir) {
            return;
        }
        self.set(id, 4, if u.speed == 0 || u.stopped() { 17 } else { 0 });
        self.set(id, 3, 16);
    }
    fn sliding(&self, id: u32, x: i32, y: i32) -> u32 {
        let v = self.read(id);
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
        let v = self.read(id);
        if old == v.z as i32 || self.realistic() {
            return;
        }
        if old < (v.z as i32) {
            self.write(id, WRITE_SPEED, u64::from(v.speed * 232 / 256));
        } else {
            let speed = v.speed.wrapping_add(2) as u16;
            if u32::from(speed) <= v.max_track_speed {
                self.write(id, WRITE_SPEED, u64::from(speed));
            }
        }
    }
    async fn position(&self, id: u32, x: i32, y: i32, new_tile: bool, delta: bool) {
        self.write(id, WRITE_X, x as u64);
        self.write(id, WRITE_Y, y as u64);
        self.effect(POSITION, id);
        let old = self
            .action(INCLINATION, id, u64::from(new_tile), u64::from(delta), 0)
            .await as i32;
        self.height_speed(id, old);
    }
    fn direction(&self, id: u32, dir: u32) {
        if dir != self.read(id).direction {
            self.write(id, WRITE_DIRECTION, u64::from(dir));
            if !self.realistic() {
                let speed = self.read(id).speed;
                self.write(id, WRITE_SPEED, u64::from(speed - (speed >> 2)));
            }
        }
    }
    async fn sound(&self, id: u32) {
        if self.action(PLAY_SOUND, id, 0, 0, 0).await == 0 {
            let mut sound = self.q0(SOUND_DEFAULT, id);
            if sound == self.q0(SOUND_OLD1, id) && self.read(id).tick & 3 == 0 {
                sound = self.q0(SOUND_OLD2, id);
            }
            self.q(SOUND, id, sound, 0);
        }
    }
    async fn crash(&self, id: u32, flooded: bool) -> u32 {
        let mut victims = self
            .action(GROUND_CRASH, id, u64::from(flooded), 0, 0)
            .await as u32;
        if self.read(id).front() {
            victims = victims.wrapping_add(1);
            if in_range(self.get(id, 0), DT_STOP, DT_STOP + 16) {
                self.effect(ROADSTOP_LEAVE, id);
            }
        }
        self.set(id, 5, if flooded { 2000 } else { 1 });
        victims
    }
    async fn train_crash(&self, id: u32) -> bool {
        let mut uid = id;
        while uid != INVALID {
            let u = self.read(uid);
            if self.get(uid, 0) != WORMHOLE && self.tile(IS_CROSSING, uid, u.tile) != 0 {
                let v = self.read(id);
                if self
                    .nearby(id, 3, v.x as i32, v.y as i32)
                    .into_iter()
                    .any(|tid| {
                        let t = self.read(tid);
                        t.r#type == 0 && (t.z as i32 - u.z as i32).abs() <= 6
                    })
                {
                    let victims = self.crash(id, false).await;
                    self.q(CRASH_NEWS, id, victims, 0);
                    return true;
                }
            }
            uid = u.next;
        }
        false
    }
    async fn crashed(&self, id: u32) -> bool {
        let counter = self.get(id, 5).wrapping_add(1);
        self.set(id, 5, counter);
        if counter == 2 {
            self.effect(EXPLOSION, id);
        } else if counter <= 45 {
            if self.read(id).tick & 7 == 0 {
                let mut uid = id;
                while uid != INVALID {
                    let random = self.random();
                    let u = self.read(uid);
                    let delta = [7, 0, 0, 1][(random & 3) as usize];
                    self.write(uid, WRITE_DIRECTION, u64::from((u.direction + delta) & 7));
                    self.action(VIEWPORT, uid, 1, 1, 0).await;
                    uid = self.read(uid).next;
                }
            }
        } else if counter >= 2220 && self.read(id).tick & 31 == 0 {
            let front = self.read(id).first;
            let alive = self.read(id).next != INVALID;
            let mut previous = id;
            let mut last = id;
            while self.read(last).next != INVALID {
                previous = last;
                last = self.read(last).next;
            }
            self.q(SET_NEXT, previous, INVALID, 0);
            self.write(
                last,
                WRITE_LAST_STATION,
                u64::from(self.read(front).last_station),
            );
            if in_range(self.get(last, 0), STOP, STOP + 16) {
                self.effect(ROADSTOP_LEAVE, last);
            }
            self.action(DELETE, last, 0, 0, 0).await;
            return alive;
        }
        true
    }
    async fn update_cache(&self, id: u32, same: bool) {
        self.effect(CACHE_INVALIDATE, id);
        self.write(id, WRITE_TOTAL_LENGTH, 0);
        let mut uid = id;
        while uid != INVALID {
            let engine = if uid == id {
                self.q0(ENGINE_INVALID, id)
            } else {
                self.read(id).engine
            };
            self.write(uid, WRITE_FIRST_ENGINE, u64::from(engine));
            let mut length = 8;
            let factor = if self.q0(GRF_VERSION, uid) >= 8 {
                let value = u32::from(self.action(PROPERTY, uid, 0x23, 65535, 0).await as u16);
                if value != 65535 && value >= 8 {
                    self.q(LENGTH_ERROR, uid, value, 0);
                }
                value
            } else {
                u32::from(self.action(LENGTH_CALLBACK, uid, 0, 0, 0).await as u16)
            };
            let factor = if factor == 65535 {
                self.q0(LENGTH_DEFAULT, uid)
            } else {
                factor
            };
            if factor != 0 {
                length -= factor.min(7);
            }
            if same && length != self.read(uid).length {
                self.action(LENGTH_CHANGED, uid, 0, 0, 0).await;
            }
            self.write(uid, WRITE_LENGTH, u64::from(length));
            self.write(
                id,
                WRITE_TOTAL_LENGTH,
                u64::from(
                    self.read(id)
                        .total_length
                        .wrapping_add(self.read(uid).length),
                ),
            );
            self.action(UPDATE_VISUAL, uid, 0, 0, 0).await;
            let age = self
                .action(PROPERTY, uid, 0x22, u64::from(self.q0(AGE_DEFAULT, uid)), 0)
                .await;
            self.write(uid, WRITE_CARGO_AGE, age);
            uid = self.read(uid).next;
        }
        let speed = self.action(PROPERTY, id, 0x15, 0, 0).await as u32;
        self.write(
            id,
            WRITE_MAX_SPEED,
            u64::from(if speed != 0 {
                speed.wrapping_mul(4)
            } else {
                self.q0(SPEED_DEFAULT, id)
            }),
        );
    }
    async fn path(&self, id: u32, tile: u32, entry: u8) -> u8 {
        let status = self.tile(TRACK_STATUS, id, tile);
        let red = (status >> 16) & 0x3f3f;
        let mut tracks = status & 0x3f3f;
        let v = self.read(id);
        let kind = self.tile(TILE_TYPE, id, tile);
        if kind == 2 {
            if self.tile(IS_DEPOT, id, tile) != 0
                && (self.tile(TILE_OWNER, id, tile) != v.owner
                    || self.tile(DEPOT_DIR, id, tile) == u32::from(entry))
            {
                tracks = 0;
            }
        } else if kind == 5 && self.tile(BAY_STOP, id, tile) != 0 {
            if self.tile(TILE_OWNER, id, tile) != v.owner
                || self.tile(BAY_DIR, id, tile) == u32::from(entry)
                || v.articulated != 0
            {
                tracks = 0;
            } else if self.tile(STOP_TYPE, id, tile) != u32::from(v.bus == 0)
                || (self.q0(QUEUE, id) == 0 && self.tile(FREE_BAY, id, tile) == 0)
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
                    let bits = self.tile(ANY_ROAD_BITS, id, tile);
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
            } else if self.read(id).dest == 0 {
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
                    let result = self
                        .action(
                            YAPF,
                            id,
                            u64::from(tile),
                            u64::from(entry),
                            u64::from(tracks),
                        )
                        .await;
                    self.q(PATH_RESULT, id, (result >> 8) as u32, 0);
                    result as u8
                }
            };
        }
        if red & (1 << best) != 0 { 255 } else { best }
    }
    async fn leave_depot(&self, id: u32, first: bool) -> bool {
        let tile = self.read(id).tile;
        let mut uid = id;
        while uid != INVALID {
            let u = self.read(uid);
            if self.get(uid, 0) != DEPOT || u.tile != tile {
                return false;
            }
            uid = u.next;
        }
        let dir = self.tile(DEPOT_DIR, id, tile);
        self.write(id, WRITE_DIRECTION, u64::from(dir * 2 + 1));
        let tdir = DIAG_TRACK[dir as usize];
        let rd = crate::road_data::entry(self.read(id).tram(), self.side() + tdir as usize, 6);
        let x = (self.tile(TILE_X, id, tile) * 16 + u32::from(rd.0 & 15)) as i32;
        let y = (self.tile(TILE_Y, id, tile) * 16 + u32::from(rd.1 & 15)) as i32;
        if first {
            let v = self.read(id);
            if v.order_type == 2 && tile == v.dest {
                self.action(ENTER_DEPOT, id, 0, 0, 0).await;
                return true;
            }
            if self.close(id, x, y, self.read(id).direction, false) != INVALID {
                return true;
            }
            self.effect(SERVICE, id);
            self.effect(LEAVE_UNBUNCH, id);
            self.sound(id).await;
            self.write(id, WRITE_SPEED, 0);
        }
        self.write(id, WRITE_HIDDEN, 0);
        self.set(id, 0, u16::from(tdir));
        self.set(id, 1, 6);
        self.write(id, WRITE_X, x as u64);
        self.write(id, WRITE_Y, y as u64);
        self.effect(POSITION, id);
        self.action(INCLINATION, id, 1, 1, 0).await;
        self.effect(DEPOT_DIRTY, id);
        true
    }
    fn follow(&self, id: u32, previous: u32, tile: u32, entry: u8, reversed: bool) -> u8 {
        let v = self.read(id);
        let prev = self.read(previous);
        if prev.tile == v.tile && !reversed {
            return REVERSE[entry as usize];
        }
        let state = self.get(previous, 0);
        let dir;
        if state == WORMHOLE || state == DEPOT {
            let diagonal = if self.tile(TILE_TYPE, id, tile) == 9 {
                self.tile(TUNNEL_DIR, id, tile)
            } else if self.tile(IS_DEPOT, id, tile) != 0 {
                self.tile(DEPOT_DIR, id, tile) ^ 2
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
        if required & self.q(ANY_ROAD_BITS, id, tile, 1) == 0 {
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
impl Game {
    async fn arrive_load(&self, id: u32, station: u32) {
        let v = self.read(id);
        let bit = if v.bus != 0 { 4 } else { 8 };
        if self.q(STATION_VISITS, id, station, 0) & bit == 0 {
            self.q(STATION_VISIT_SET, id, station, bit);
            let headline = if v.bus != 0 { 0 } else { 2 } + u32::from(v.tram());
            let local = v.owner == self.q0(LOCAL_COMPANY, INVALID);
            self.leaf(
                ARRIVAL,
                id,
                u64::from(station),
                u64::from(headline),
                u64::from(local),
            );
        }
        self.action(BEGIN_LOADING, id, 0, 0, 0).await;
        self.action(STOP_RANDOM, id, u64::from(station), 0, 0).await;
        self.action(STOP_ANIMATION, id, u64::from(station), 0, 0)
            .await;
    }
    async fn individual(&self, id: u32, previous: u32) -> bool {
        if self.get(id, 3) != 0 {
            if self.tile(TILE_TYPE, id, self.read(id).tile) == 5 {
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
            let v = self.read(id);
            let packed = self.leaf(NEW_POSITION, id, 0, 0, 0);
            let x = packed as u32 as i32;
            let y = (packed >> 32) as u32 as i32;
            let new_tile = self.q(VIRT_TILE, id, x as u32, y as u32);
            if v.front() {
                let blocking = self.close(id, x, y, v.direction, true);
                if blocking != INVALID {
                    self.write(
                        id,
                        WRITE_SPEED,
                        u64::from(self.read(self.read(blocking).first).speed),
                    );
                    return false;
                }
            }
            if self.tile(TILE_TYPE, id, new_tile) == 9
                && self
                    .action(ENTER_TILE, id, u64::from(new_tile), x as u64, y as u64)
                    .await
                    & 2
                    != 0
            {
                self.write(id, WRITE_X, x as u64);
                self.write(id, WRITE_Y, y as u64);
                self.effect(POSITION, id);
                self.action(INCLINATION, id, 1, 1, 0).await;
                return true;
            }
            self.write(id, WRITE_X, x as u64);
            self.write(id, WRITE_Y, y as u64);
            self.effect(POSITION, id);
            if !self.read(id).hidden() {
                self.effect(BASE_VIEWPORT, id);
            }
            return true;
        }
        let v = self.read(id);
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
                .wrapping_add(self.q(OFFSET, id, u32::from(rd.0 & 3), 0));
            let mut dir = if v.front() {
                if self.tile(HAS_ROAD, id, tile) != 0 {
                    self.path(id, tile, rd.0 & 3).await
                } else {
                    REVERSE[(rd.0 & 3) as usize]
                }
            } else {
                self.follow(id, previous, tile, rd.0 & 3, false)
            };
            if dir == 255 {
                if !v.front() {
                    self.effect(DISCONNECT, id);
                }
                self.write(id, WRITE_SPEED, 0);
                return false;
            }
            loop {
                let mut frame = 0;
                if reversing(dir) {
                    self.set(id, 3, 0);
                    if self.read(id).tram() {
                        let needed = [2, 1, 8, 4][match dir {
                            6 => 0,
                            7 => 1,
                            14 => 2,
                            15 => 3,
                            _ => unreachable!(),
                        }];
                        let previous_id = self.read(id).previous;
                        let big = (previous_id != INVALID && self.read(previous_id).tile == tile)
                            || (self.read(id).front()
                                && self.tile(NORMAL_ROAD, id, tile) != 0
                                && self.tile(ROAD_WORKS, id, tile) == 0
                                && self.tile(HAS_ROAD, id, tile) != 0
                                && needed & self.tile(ROAD_BITS, id, tile) != 0);
                        if !big {
                            if !self.read(id).front()
                                || self
                                    .action(TRAM_PROBE, id, u64::from(tile), u64::from(needed), 0)
                                    .await
                                    == 0
                                || (!needed & self.tile(ANY_ROAD_BITS, id, self.read(id).tile)) == 0
                            {
                                tile = self.read(id).tile;
                                frame = 16;
                            } else {
                                self.write(id, WRITE_SPEED, 0);
                                return false;
                            }
                        }
                    } else if self.tile(NORMAL_ROAD, id, self.read(id).tile) != 0
                        && self.tile(DISALLOWED, id, self.read(id).tile) != 0
                    {
                        self.write(id, WRITE_SPEED, 0);
                        return false;
                    } else {
                        tile = self.read(id).tile;
                    }
                }
                let pos = crate::road_data::entry(
                    self.read(id).tram(),
                    (dir as usize + self.side()) ^ self.get(id, 3) as usize,
                    frame,
                );
                let x = (self.tile(TILE_X, id, tile) * 16 + u32::from(pos.0)) as i32;
                let y = (self.tile(TILE_Y, id, tile) * 16 + u32::from(pos.1)) as i32;
                let new_dir = self.sliding(id, x, y);
                if self.read(id).front() {
                    let blocking = self.close(id, x, y, new_dir, true);
                    if blocking != INVALID {
                        self.write(
                            id,
                            WRITE_SPEED,
                            u64::from(self.read(self.read(blocking).first).speed),
                        );
                        self.path_push(id, dir, tile);
                        return false;
                    }
                }
                let enter = self
                    .action(ENTER_TILE, id, u64::from(tile), x as u64, y as u64)
                    .await;
                if enter & 4 != 0 {
                    if self.tile(TILE_TYPE, id, tile) != 9 {
                        self.write(id, WRITE_SPEED, 0);
                        return false;
                    }
                    dir = REVERSE[(rd.0 & 3) as usize];
                    continue;
                }
                let v = self.read(id);
                if in_range(self.get(id, 0), STOP, DT_STOP + 16)
                    && self.tile(TILE_TYPE, id, v.tile) == 5
                {
                    if reversing(dir) && in_range(self.get(id, 0), STOP, STOP + 16) {
                        self.write(id, WRITE_SPEED, 0);
                        return false;
                    }
                    if self.tile(IS_DT_STOP, id, v.tile) != 0
                        && self.q(CONTINUATION, id, v.tile, tile) != 0
                        && v.tile != tile
                    {
                        dir = self.get(id, 0) as u8;
                    } else if self.tile(IS_ROAD_STOP, id, v.tile) != 0 {
                        self.effect(ROADSTOP_LEAVE, id);
                    }
                }
                if enter & 2 == 0 {
                    let old_tile = self.read(id).tile;
                    self.write(id, WRITE_TILE, u64::from(tile));
                    self.set(id, 0, u16::from(dir));
                    self.set(id, 1, frame as u16);
                    if self.tile(ROAD_TYPE, id, old_tile) != self.tile(ROAD_TYPE, id, tile) {
                        if self.read(id).front() {
                            self.update_cache(id, false).await;
                        }
                        self.action(CARGO_CHANGED, self.read(id).first, 0, 0, 0)
                            .await;
                    }
                }
                self.direction(id, new_dir);
                self.position(id, x, y, true, true).await;
                return true;
            }
        }
        if rd.0 & 64 != 0 {
            let mut frame = 1;
            let v = self.read(id);
            let bits = self.q(ANY_ROAD_BITS, id, v.tile, 1);
            let dir = if v.tram()
                && self.tile(IS_DEPOT, id, v.tile) == 0
                && bits != 0
                && bits & (bits - 1) == 0
            {
                frame = 21;
                [14, 15, 6, 7][(rd.0 & 3) as usize]
            } else if v.front() {
                self.path(id, v.tile, rd.0 & 3).await
            } else {
                self.follow(id, previous, v.tile, rd.0 & 3, true)
            };
            if dir == 255 {
                self.write(id, WRITE_SPEED, 0);
                return false;
            }
            let pos =
                crate::road_data::entry(self.read(id).tram(), self.side() + dir as usize, frame);
            let tile = self.read(id).tile;
            let x = (self.tile(TILE_X, id, tile) * 16 + u32::from(pos.0)) as i32;
            let y = (self.tile(TILE_Y, id, tile) * 16 + u32::from(pos.1)) as i32;
            let new_dir = self.sliding(id, x, y);
            if self.read(id).front() {
                let blocking = self.close(id, x, y, new_dir, true);
                if blocking != INVALID {
                    self.write(
                        id,
                        WRITE_SPEED,
                        u64::from(self.read(self.read(blocking).first).speed),
                    );
                    self.path_push(id, dir, tile);
                    return false;
                }
            }
            if self
                .action(ENTER_TILE, id, u64::from(tile), x as u64, y as u64)
                .await
                & 4
                != 0
            {
                self.write(id, WRITE_SPEED, 0);
                return false;
            }
            self.set(id, 0, u16::from(dir));
            self.set(id, 1, frame as u16);
            self.direction(id, new_dir);
            self.position(id, x, y, true, true).await;
            return true;
        }
        let v = self.read(id);
        if v.next != INVALID
            && self.tile(IS_DEPOT, id, v.tile) != 0
            && u32::from(self.get(id, 1)) == v.length + 6
        {
            self.leave_depot(v.next, false).await;
        }
        let v = self.read(id);
        let x = (v.x as i32 & !15) + i32::from(rd.0 & 15);
        let y = (v.y as i32 & !15) + i32::from(rd.1 & 15);
        let new_dir = self.sliding(id, x, y);
        if self.read(id).front() && !in_range(self.get(id, 0), STOP, STOP + 16) {
            let mut blocking = self.close(id, x, y, new_dir, true);
            if blocking != INVALID {
                blocking = self.read(blocking).first;
                if self.get(id, 3) == 0 {
                    self.overtake(id, blocking);
                }
                if self.get(id, 3) == 0 {
                    self.write(id, WRITE_SPEED, u64::from(self.read(blocking).speed));
                }
                let v = self.read(id);
                if v.speed == 0
                    && in_range(self.get(id, 0), DT_STOP, DT_STOP + 16)
                    && self.tile(ORDER_STOP, id, v.tile) != 0
                    && v.owner == self.tile(TILE_OWNER, id, v.tile)
                    && v.order_type != 4
                    && self.tile(STOP_TYPE, id, v.tile) == u32::from(v.bus == 0)
                {
                    let station = self.tile(STATION, id, v.tile);
                    self.write(id, WRITE_LAST_STATION, u64::from(station));
                    self.arrive_load(id, station).await;
                }
                return false;
            }
        }
        let old_dir = self.read(id).direction;
        if new_dir != old_dir {
            self.direction(id, new_dir);
            self.action(VIEWPORT, id, 1, 1, 0).await;
            return true;
        }
        let v = self.read(id);
        let state = self.get(id, 0);
        let frame = self.get(id, 1);
        if v.front()
            && ((in_range(state, STOP, STOP + 16)
                && crate::road_data::_ROAD_STOP_STOP_FRAME[state as usize - 32 + self.side()]
                    == frame as u8)
                || (in_range(state, DT_STOP, DT_STOP + 16)
                    && self.tile(ORDER_STOP, id, v.tile) != 0
                    && v.owner == self.tile(TILE_OWNER, id, v.tile)
                    && self.tile(STOP_TYPE, id, v.tile) == u32::from(v.bus == 0)
                    && frame == 11))
        {
            let station = self.tile(STATION, id, v.tile);
            if state & 4 == 0 {
                if self.tile(IS_DT_STOP, id, v.tile) != 0 {
                    let next = v
                        .tile
                        .wrapping_add(self.q(OFFSET, id, diag(v.direction), 0));
                    if self.q(CONTINUATION, id, v.tile, next) != 0
                        && self.tile(HAS_ROAD, id, next) != 0
                    {
                        self.set(id, 1, self.get(id, 1).wrapping_add(1));
                        self.position(id, x, y, true, false).await;
                        return true;
                    }
                }
                self.q(ENTRANCE_SET, id, 0, 0);
                self.set(id, 0, self.get(id, 0) | 4);
                self.write(id, WRITE_LAST_STATION, u64::from(station));
                let v = self.read(id);
                if self.tile(IS_DT_STOP, id, v.tile) != 0
                    || (v.order_type == 1 && v.order_destination == station)
                {
                    self.arrive_load(id, station).await;
                    return false;
                }
            } else {
                if self.q0(ENTRANCE_BUSY, id) != 0 {
                    self.write(id, WRITE_SPEED, 0);
                    return false;
                }
                if self.read(id).order_type == 4 {
                    self.effect(ORDER_FREE, id);
                }
            }
            if self.tile(BAY_STOP, id, self.read(id).tile) != 0 {
                self.q(ENTRANCE_SET, id, 1, 0);
            }
            self.sound(id).await;
            self.effect(START_STOP_DIRTY, id);
        }
        let tile = self.read(id).tile;
        let enter = self
            .action(ENTER_TILE, id, u64::from(tile), x as u64, y as u64)
            .await;
        if enter & 4 != 0 {
            self.write(id, WRITE_SPEED, 0);
            return false;
        }
        let v = self.read(id);
        if v.order_type == 4 && self.tile(IS_DT_STOP, id, v.tile) != 0 {
            self.effect(ORDER_FREE, id);
        }
        if enter & 2 == 0 {
            self.set(id, 1, self.get(id, 1).wrapping_add(1));
        }
        self.position(id, x, y, false, true).await;
        true
    }
    async fn controller(&self, id: u32) -> bool {
        let v = self.read(id);
        self.write(
            id,
            WRITE_ORDER_TIME,
            u64::from(v.order_time.wrapping_add(1)),
        );
        let reverse = self.get(id, 6);
        if reverse != 0 {
            self.set(id, 6, reverse - 1);
        }
        if self.read(id).crashed() || self.train_crash(id).await {
            return self.crashed(id).await;
        }
        if self.action(BREAKDOWN, id, 0, 0, 0).await != 0 {
            return true;
        }
        if self.read(id).stopped() {
            self.effect(LAST_SPEED, id);
            return true;
        }
        self.action(PROCESS_ORDERS, id, 0, 0, 0).await;
        self.action(LOADING, id, 0, 0, 0).await;
        if self.read(id).order_type == 3 {
            return true;
        }
        if self.get(id, 0) == DEPOT {
            if self.q0(WAIT_UNBUNCH, id) != 0 {
                return true;
            }
            if self.leave_depot(id, true).await {
                return true;
            }
        }
        self.action(VISUAL, id, 0, 0, 0).await;
        let mut distance = self.update_speed(id);
        let mut advance = self.q0(ADVANCE, id) as i32;
        let mut blocked = false;
        while distance >= advance {
            distance -= advance;
            let mut uid = id;
            let mut prev = INVALID;
            while uid != INVALID {
                if !self.individual(uid, prev).await {
                    blocked = true;
                    break;
                }
                prev = uid;
                uid = self.read(uid).next;
            }
            if blocked {
                break;
            }
            advance = self.q0(ADVANCE, id) as i32;
            if distance >= advance && self.train_crash(id).await {
                break;
            }
        }
        self.effect(LAST_SPEED, id);
        let mut uid = id;
        while uid != INVALID {
            let u = self.read(uid);
            if !u.hidden() {
                self.action(VIEWPORT, uid, 0, 0, 0).await;
            }
            uid = u.next;
        }
        if self.read(id).progress == 0 {
            self.write(
                id,
                WRITE_PROGRESS,
                if blocked { advance - 1 } else { distance } as u64,
            );
        }
        true
    }
    fn set_dest(&self, id: u32, tile: u32) {
        if tile == self.read(id).dest {
            return;
        }
        self.path_clear(id);
        self.q(SET_DEST, id, tile, 0);
    }
    async fn running_cost(&self, id: u32) -> i64 {
        if self.q0(COST_CLASS, id) == self.q0(INVALID_PRICE, id) {
            return 0;
        }
        let factor = self
            .action(PROPERTY, id, 9, u64::from(self.q0(COST_FACTOR, id)), 0)
            .await;
        if factor == 0 {
            return 0;
        }
        self.leaf(GET_PRICE, id, factor, 0, 0) as i64
    }
    async fn service(&self, id: u32) {
        if self.q0(SERVINT, id) == 0 || self.q0(NEEDS_SERVICE, id) == 0 {
            return;
        }
        let v = self.read(id);
        let mut all_in_depot = self.tile(IS_DEPOT, id, v.tile) != 0 && v.speed == 0;
        let mut uid = v.first;
        if all_in_depot {
            while uid != INVALID {
                let u = self.read(uid);
                if self.get(uid, 0) != DEPOT || u.tile != v.tile {
                    all_in_depot = false;
                    break;
                }
                uid = u.next;
            }
        }
        if all_in_depot {
            self.effect(SERVICE, id);
            return;
        }
        let penalty = self.q0(MAX_PENALTY, id);
        // FindClosestRoadDepot returns this tile at distance zero even while
        // moving or when only part of the consist has entered the depot.
        let depot = if self.tile(IS_DEPOT, id, v.tile) != 0 {
            u64::from(v.tile)
        } else {
            self.action(FIND_DEPOT, id, u64::from(penalty), 0, 0).await
        };
        let tile = depot as u32;
        let length = (depot >> 32) as u32;
        if length == u32::MAX || length > penalty {
            if self.read(id).order_type == 2 {
                self.effect(ORDER_DUMMY, id);
                self.effect(START_STOP_DIRTY, id);
            }
            return;
        }
        let depot = self.tile(DEPOT_INDEX, id, tile);
        let v = self.read(id);
        if v.order_type == 2
            && v.order_nonstop & 1 != 0
            && !crate::services::chance16_i(1, 20, self.random())
        {
            return;
        }
        self.write(id, WRITE_SUPPRESS_IMPLICIT, 0);
        self.q(ORDER_DEPOT, id, depot, 0);
        self.set_dest(id, tile);
        self.effect(START_STOP_DIRTY, id);
    }
    async fn day(&self, id: u32, calendar: bool) {
        if !self.read(id).front() {
            return;
        }
        if calendar {
            self.effect(AGE, id);
            return;
        }
        self.effect(ECONOMY_AGE, id);
        let day = self.read(id).day.wrapping_add(1) as u8;
        self.write(id, WRITE_DAY, u64::from(day));
        if day & 7 == 0 {
            self.effect(DECREASE_VALUE, id);
        }
        if self.get(id, 2) == 0 {
            self.effect(CHECK_BREAKDOWN, id);
        }
        self.service(id).await;
        self.effect(CHECK_ORDERS, id);
        if self.read(id).running == 0 {
            return;
        }
        let cost = self
            .running_cost(id)
            .await
            .saturating_mul(i64::from(self.read(id).running))
            / i64::from(self.q0(COST_DIVISOR, id));
        self.leaf(PAY_RUNNING, id, cost as u64, 0, 0);
        self.effect(DETAILS_DIRTY, id);
    }
    fn trackdir(&self, id: u32) -> u8 {
        let v = self.read(id);
        if v.crashed() {
            return 255;
        }
        if self.get(id, 0) == DEPOT {
            return DIAG_TRACK[self.tile(DEPOT_DIR, id, v.tile) as usize];
        }
        if self.tile(BAY_STOP, id, v.tile) != 0 {
            return DIAG_TRACK[self.tile(BAY_DIR, id, v.tile) as usize];
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
        let v = self.read(id);
        let mut uid = v.first;
        let state = self.get(uid, 0);
        if state <= 15 && reversing(state as u8) {
            return true;
        }
        while uid != id {
            let u = self.read(uid);
            if v.direction != u.direction {
                return true;
            }
            uid = u.next;
        }
        false
    }
    fn turn(&self, id: u32, execute: bool) -> bool {
        let v = self.read(id);
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
        if self.tile(NORMAL_ROAD, id, v.tile) != 0 && self.tile(DISALLOWED, id, v.tile) != 0 {
            return false;
        }
        if self.tile(TILE_TYPE, id, v.tile) == 9
            && diag(v.direction) == self.tile(TUNNEL_DIR, id, v.tile)
        {
            return false;
        }
        if execute {
            self.set(id, 6, 180);
            self.effect(RESET_UNBUNCH, id);
        }
        true
    }
}
async fn run(g: Game, kind: u32, id: u32, a: u64, _b: u64, _c: u64) -> u64 {
    match kind {
        0 => {
            let v = g.read(id);
            g.write(id, WRITE_TICK, u64::from(v.tick.wrapping_add(1)));
            if v.front() {
                if !g.read(id).stopped() {
                    g.write(
                        id,
                        WRITE_RUNNING,
                        u64::from(g.read(id).running.wrapping_add(1)),
                    );
                }
                u64::from(g.controller(id).await)
            } else {
                1
            }
        }
        1 => u64::from(g.individual(id, a as u32).await),
        2 => u64::from(g.leave_depot(id, a != 0).await),
        3 => u64::from(g.crash(id, a != 0).await),
        4 => {
            g.update_cache(id, a != 0).await;
            0
        }
        5 => {
            g.day(id, true).await;
            0
        }
        6 => {
            g.day(id, false).await;
            0
        }
        7 => g.running_cost(id).await as u64,
        8 => g.max_speed(id) as u64,
        9 => g.update_speed(id) as u64,
        10 => {
            g.set_dest(id, a as u32);
            0
        }
        11 => u64::from(g.turn(id, a != 0)),
        12 => u64::from(g.trackdir(id)),
        13 => u64::from(g.slope_pixel(id)),
        _ => unreachable!(),
    }
}
/// Shared modern/legacy afterload road-stop frame query, retaining original indexes.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_road_stop_frame(index: u32) -> u8 {
    crate::road_data::_ROAD_STOP_STOP_FRAME[index as usize]
}

const IS_CROSSING: u32 = 82;
const NEW_POSITION: u32 = 83;
const VIRT_TILE: u32 = 84;
const IS_ROAD_STOP: u32 = 85;
const SET_DEST: u32 = 86;

/// Narrow unchanged-reference table comparison for the corpus's tram-table gap.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_road_drive_entry(tram: u8, state: u8, frame: u8) -> u16 {
    let (x, y) = crate::road_data::entry(tram != 0, state as usize, frame as usize);
    u16::from(x) | (u16::from(y) << 8)
}

const STATION_VISITS: u32 = 87;
const STATION_VISIT_SET: u32 = 88;
const LOCAL_COMPANY: u32 = 89;
