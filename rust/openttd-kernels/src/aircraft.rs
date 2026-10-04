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
use crate::services::{Services, chance16_i};
use std::cell::Cell;
use std::ffi::c_void;
use std::future::Future;
use std::pin::Pin;
use std::rc::Rc;
use std::task::{Context as PollContext, Poll, Waker};
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
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Action {
    pub kind: u32,
    pub id: u32,
    pub other: u32,
    pub a: i64,
    pub b: i64,
    pub c: i64,
    pub d: i64,
}
struct Channel {
    action: Cell<Action>,
    response: Cell<i64>,
}
struct Request {
    channel: Rc<Channel>,
    action: Option<Action>,
}
impl Future for Request {
    type Output = i64;
    fn poll(mut self: Pin<&mut Self>, _: &mut PollContext<'_>) -> Poll<i64> {
        if let Some(action) = self.action.take() {
            self.channel.action.set(action);
            Poll::Pending
        } else {
            Poll::Ready(self.channel.response.get())
        }
    }
}
#[derive(Clone)]
struct World {
    read: unsafe extern "C" fn(*mut c_void, u32, u32, i64, i64, *mut i64),
    write: unsafe extern "C" fn(*mut c_void, u32, u32, i64),
    context: *mut c_void,
    service: unsafe extern "C" fn(*mut c_void, *const Action) -> i64,
    channel: Rc<Channel>,
    services: Services,
}
const SUBTYPE: usize = 0;
const X: usize = 1;
const Y: usize = 2;
const Z: usize = 3;
const TILE: usize = 4;
const DIR: usize = 5;
const TICK: usize = 6;
const OWNER: usize = 7;
const STATUS: usize = 8;
const NEXT: usize = 9;
const SPEED: usize = 10;
const SUBSPEED: usize = 11;
const PROGRESS: usize = 12;
const ACCEL: usize = 13;
const MAX_SPEED: usize = 14;
const BREAKDOWN: usize = 15;
const ORDER: usize = 16;
const DEST: usize = 17;
const RUNNING: usize = 18;
const ORDER_TIME: usize = 19;
const DAY: usize = 20;
const PROFIT: usize = 21;
const LAST_STATION: usize = 22;
const ECONOMY_SERVICE: usize = 23;
const CALENDAR_SERVICE: usize = 24;
const BREAKDOWNS: usize = 25;
const RELIABILITY: usize = 26;
const CARGO_AGE: usize = 27;
const INVALID: u32 = 1_048_575;
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
        fn $get(&self,id:u32)->$type {
            let ptr=self.private(id);
            // SAFETY: Live shell owns a fixed allocation; only this scalar is read.
            unsafe { (&raw const (*ptr).$field).read() }
        }
        fn $set(&self,id:u32,value:$type) {
            let ptr=self.private(id);
            // SAFETY: Serial game-thread scalar access; no reference spans callbacks.
            unsafe { (&raw mut (*ptr).$field).write(value); }
        }
    )*};
}
impl World {
    fn read(&self, kind: u32, id: u32, a: i64, b: i64) -> [i64; 40] {
        let mut out = [0; 40];
        // SAFETY: noexcept callback copies at most 40 scalars; original preconditions apply.
        unsafe {
            (self.read)(self.context, kind, id, a, b, out.as_mut_ptr());
        }
        out
    }
    fn world(&self) -> [i64; 40] {
        self.read(0, 0, 0, 0)
    }
    fn v(&self, id: u32, field: usize) -> i64 {
        self.read(1, id, 0, 0)[field]
    }
    fn set(&self, id: u32, field: usize, value: i64) {
        // SAFETY: Source-valid live vehicle; callback writes exactly one canonical scalar.
        unsafe {
            (self.write)(self.context, field as u32, id, value);
        }
    }
    fn status(&self, id: u32, mask: i64, on: bool) {
        let old = self.v(id, STATUS);
        self.set(id, STATUS, if on { old | mask } else { old & !mask });
    }
    fn private(&self, id: u32) -> *mut State {
        self.read(8, id, 0, 0)[0] as usize as *mut State
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
    fn flag(&self, id: u32, bit: u8, on: bool) {
        let f = self.flags(id);
        self.set_flags(id, if on { f | (1 << bit) } else { f & !(1 << bit) });
    }
    fn station(&self, id: u32) -> [i64; 40] {
        self.read(2, id, 0, 0)
    }
    fn target_station(&self, id: u32) -> [i64; 40] {
        self.station(u32::from(self.target(id)))
    }
    fn valid_airport(&self, id: u32) -> bool {
        let s = self.target_station(id);
        s[0] != 0 && s[1] as u32 != INVALID_TILE
    }
    fn airport(&self, id: u32) -> i64 {
        let s = self.target_station(id);
        if s[0] == 0 {
            self.read(3, 0, 0, 0)[0]
        } else {
            s[10]
        }
    }
    fn class(&self, ap: i64) -> [i64; 40] {
        self.read(3, 0, ap, 0)
    }
    fn node(&self, ap: i64, pos: u8) -> i64 {
        self.read(4, 0, ap, i64::from(pos))[0]
    }
    fn fta(&self, node: i64) -> [i64; 40] {
        self.read(5, 0, node, 0)
    }
    fn blocks(&self, id: u32) -> u64 {
        let ptr = self.station(id)[11] as usize as *const u64;
        // SAFETY: Live station's separately owned airport scalar, read without a reference.
        unsafe { ptr.read() }
    }
    fn set_blocks(&self, id: u32, value: u64) {
        let ptr = self.station(id)[11] as usize as *mut u64;
        // SAFETY: Fixed live airport allocation; all aliases are serial game-thread scalar accesses.
        unsafe {
            ptr.write(value);
        }
    }
    fn reserve(&self, id: u32, bits: u64) {
        self.set_blocks(id, self.blocks(id) | bits);
    }
    fn release(&self, id: u32, bits: u64) {
        self.set_blocks(id, self.blocks(id) & !bits);
    }
    fn service(&self, kind: u32, id: u32, other: u32, a: i64, b: i64, c: i64, d: i64) -> i64 {
        let action = Action {
            kind,
            id,
            other,
            a,
            b,
            c,
            d,
        };
        // SAFETY: No owner/world references exist. Bounded leaves cannot throw/reenter this controller.
        unsafe { (self.service)(self.context, &raw const action) }
    }
    fn leaf(&self, kind: u32, id: u32) -> i64 {
        self.service(kind, id, 0, 0, 0, 0, 0)
    }
    async fn action(&self, kind: u32, id: u32, other: u32, a: i64) -> i64 {
        Request {
            channel: self.channel.clone(),
            action: Some(Action {
                kind,
                id,
                other,
                a,
                ..Action::default()
            }),
        }
        .await
    }
    fn random(&self) -> u32 {
        self.services.random()
    }
    fn tile_x(&self, t: u32) -> i64 {
        i64::from(t & (self.world()[0] as u32 - 1))
    }
    fn tile_y(&self, t: u32) -> i64 {
        i64::from(t >> (self.world()[0] as u32).trailing_zeros())
    }
    fn tile_virt(&self, x: i64, y: i64) -> u32 {
        ((y as i32 as u32) >> 4)
            .wrapping_mul(self.world()[0] as u32)
            .wrapping_add((x as i32 as u32) >> 4)
    }
    fn slope(&self, x: i64, y: i64) -> i64 {
        self.read(9, 0, x, y)[0]
    }
    fn height(&self, id: u32) -> i64 {
        let w = self.world();
        let x = self.v(id, X).clamp(0, w[2] * 16);
        let y = self.v(id, Y).clamp(0, w[3] * 16);
        self.read(10, self.tile_virt(x, y), 0, 0)[0]
    }
    fn bounds(&self, id: u32) -> (i64, i64) {
        let mut base = self.height(id);
        if self.v(id, SUBTYPE) == 0 && self.read(1, id, 0, 0)[34] == 3 {
            base += 34;
        }
        if self.v(id, DIR) <= 3 {
            base += 10;
        }
        // C++ cached_max_speed is uint16; arithmetic promotes to signed int.
        base += (20 * (self.v(id, MAX_SPEED) / 200) - 90).min(0);
        (base + 120, base + 360)
    }
    fn flight_flags(&self, id: u32, ptr: *mut u8, takeoff: bool) -> i64 {
        let (min, max) = self.bounds(id);
        let middle = (min + max) / 2;
        let mut z = self.v(id, Z);
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
    fn flight(&self, id: u32, takeoff: bool) -> i64 {
        let ptr = self.private(id);
        // SAFETY: Computing a raw field address creates no reference or access scope.
        self.flight_flags(id, unsafe { &raw mut (*ptr).flags }, takeoff)
    }
    fn effect(&self, id: u32, x: i64, y: i64, z: i64, kind: u32) {
        self.service(7, id, kind, x, y, z, 0);
    }
    fn position(&self, id: u32, x: i64, y: i64, z: i64) {
        self.set(id, X, x);
        self.set(id, Y, y);
        self.set(id, Z, z);
        self.leaf(1, id);
        if self.v(id, SUBTYPE) == 0 {
            self.leaf(2, id);
        }
        let shadow = self.v(id, NEXT) as u32;
        let w = self.world();
        let safe_x = x.clamp(0, w[2] * 16);
        let mut safe_y = (y - 1).clamp(0, w[3] * 16);
        self.set(shadow, X, x);
        self.set(
            shadow,
            Y,
            y - ((self.v(id, Z) - self.slope(safe_x, safe_y)) >> 3),
        );
        safe_y = self.v(shadow, Y).clamp(0, w[3] * 16);
        self.set(shadow, Z, self.slope(safe_x, safe_y));
        self.service(3, shadow, id, 0, 0, 0, 0);
        self.leaf(4, shadow);
        let rotor = self.v(shadow, NEXT) as u32;
        if rotor != INVALID {
            self.set(rotor, X, x);
            self.set(rotor, Y, y);
            self.set(rotor, Z, z + 5);
            self.leaf(4, rotor);
        }
    }
    fn position_current(&self, id: u32) {
        self.position(id, self.v(id, X), self.v(id, Y), self.v(id, Z));
    }
    fn enter_hangar(&self, id: u32) {
        self.set(id, SUBSPEED, 0);
        self.set(id, PROGRESS, 0);
        let shadow = self.v(id, NEXT) as u32;
        self.status(shadow, HIDDEN, true);
        let rotor = self.v(shadow, NEXT) as u32;
        if rotor != INVALID {
            self.status(rotor, HIDDEN, true);
            self.set(rotor, SPEED, 0);
        }
        self.position_current(id);
    }
    fn cache(&self, id: u32, update_range: bool) {
        let speed = self.service(5, id, 0, 0, 0, 0, 0) as u32;
        self.set(
            id,
            MAX_SPEED,
            if speed != 0 {
                i64::from(speed.wrapping_mul(128) / 10)
            } else {
                self.read(11, id, 0, 0)[0]
            },
        );
        self.set(id, CARGO_AGE, self.service(5, id, 1, 0, 0, 0, 0));
        let shadow = self.v(id, NEXT) as u32;
        self.set(shadow, CARGO_AGE, self.service(5, shadow, 1, 0, 0, 0, 0));
        if update_range {
            let range = self.service(5, id, 2, 0, 0, 0, 0) as u16;
            self.set_range(id, range);
            self.set_range_sqr(
                id,
                u32::from(self.range(id)).wrapping_mul(u32::from(self.range(id))),
            );
        }
    }
    fn helicopter_tick(&self, id: u32) {
        let rotor = self.v(self.v(id, NEXT) as u32, NEXT) as u32;
        if self.v(rotor, STATUS) & HIDDEN != 0 {
            return;
        }
        if self.v(id, ORDER) == 3 || self.v(id, STATUS) & STOPPED != 0 {
            if self.v(rotor, SPEED) != 0 {
                self.set(
                    rotor,
                    SPEED,
                    i64::from((self.v(rotor, SPEED) as u16).wrapping_add(1)),
                );
                if self.v(rotor, SPEED) >= 128 && self.state(rotor) == 3 {
                    self.set(rotor, SPEED, 0);
                }
            }
        } else {
            if self.v(rotor, SPEED) == 0 {
                self.set(rotor, SPEED, 112);
            }
            if self.v(rotor, SPEED) >= 80 {
                self.set(rotor, SPEED, self.v(rotor, SPEED) - 1);
            }
        }
        let tick = (self.v(rotor, TICK) as u8).wrapping_add(1);
        self.set(rotor, TICK, i64::from(tick));
        let speed = self.v(rotor, SPEED) >> 4;
        if speed == 0 {
            self.set_state(rotor, 0);
            if self.service(6, id, 0, 0, 0, 0, 0) != 0 {
                return;
            }
        } else if i64::from(tick) >= speed {
            self.set(rotor, TICK, 0);
            let state = self.state(rotor).wrapping_add(1);
            self.set_state(rotor, if state > 3 { 1 } else { state });
            self.service(6, id, 1, 0, 0, 0, 0);
        } else {
            return;
        }
        self.leaf(4, rotor);
    }
    fn speed(&self, id: u32, mut limit: u32, mut hard: bool) -> u32 {
        let mut spd = (self.v(id, ACCEL) as u32).wrapping_mul(77);
        let factor = self.world()[4] as u32;
        limit = limit.wrapping_mul(factor);
        if self.v(id, STATUS) & BROKEN != 0 {
            if 320 < limit {
                hard = false;
            }
            limit = limit.min(320);
        }
        if (self.v(id, MAX_SPEED) as u32) < limit {
            if (self.v(id, SPEED) as u32) < limit {
                hard = false;
            }
            limit = self.v(id, MAX_SPEED) as u32;
        }
        let old_sub = self.v(id, SUBSPEED) as u8;
        let sub = old_sub.wrapping_add(spd as u8);
        self.set(id, SUBSPEED, i64::from(sub));
        let cur = self.v(id, SPEED) as u32;
        if !hard && cur > limit {
            let reduction = ((cur as i32).wrapping_mul(cur as i32) / 16384) / factor as i32;
            limit = cur.wrapping_sub(reduction.max(1) as u32);
        }
        spd = (cur + (spd >> 8) + u32::from(sub < old_sub)).min(limit);
        if spd != cur {
            self.set(id, SPEED, i64::from(spd));
            self.leaf(8, id);
        }
        if factor > 1 {
            spd /= factor;
        }
        if self.v(id, DIR) & 1 == 0 {
            spd = spd.wrapping_mul(3) / 4;
        }
        spd = spd.wrapping_add(self.v(id, PROGRESS) as u32);
        self.set(id, PROGRESS, i64::from(spd as u8));
        spd >> 8
    }
    fn entry_point(&self, id: u32, ap: i64, rotation: i64) -> u8 {
        let s = self.target_station(id);
        let tile = if s[0] == 0 {
            0
        } else if s[1] as u32 != INVALID_TILE {
            s[1] as u32
        } else {
            s[2] as u32
        };
        let dx = self.v(id, X) - self.tile_x(tile) * 16;
        let dy = self.v(id, Y) - self.tile_y(tile) * 16;
        let dir = if dy.abs() < dx.abs() {
            if dx < 0 { 0 } else { 2 }
        } else if dy < 0 {
            3
        } else {
            1
        };
        let rotated = (dir + (0_u8.wrapping_sub(rotation as u8 >> 1) & 3)) & 3;
        self.read(12, 0, ap, i64::from(rotated))[0] as u8
    }
    fn next_airport(&self, id: u32) {
        if matches!(self.v(id, ORDER), 1 | 2) {
            self.set_target(id, self.v(id, DEST) as u16);
        }
        let s = self.target_station(id);
        let valid = s[0] != 0 && s[1] as u32 != INVALID_TILE;
        let ap = if valid { s[10] } else { self.class(0)[0] };
        let rotation = if valid { s[3] } else { 0 };
        let pos = self.entry_point(id, ap, rotation);
        self.set_pos(id, pos);
        self.set_previous(id, pos);
    }
    async fn controller(&self, id: u32) -> bool {
        let s = self.target_station(id);
        let valid = s[0] != 0;
        let mut tile = INVALID_TILE;
        let mut rotation = 0;
        let mut sx = 1;
        let mut sy = 1;
        if valid {
            if s[1] as u32 != INVALID_TILE {
                tile = s[1] as u32;
                rotation = s[3];
                sx = s[4];
                sy = s[5];
            } else {
                tile = s[2] as u32;
            }
        }
        let ap = if tile == INVALID_TILE {
            self.class(0)[0]
        } else {
            s[10]
        };
        if !valid || s[1] as u32 == INVALID_TILE {
            if i64::from(self.pos(id)) >= self.class(ap)[1] {
                let pos = self.entry_point(id, ap, 0);
                self.set_pos(id, pos);
                self.set_previous(id, pos);
            } else if i64::from(self.target(id)) != self.v(id, DEST) {
                self.set_state(id, FLYING);
                self.cache(id, false);
                self.next_airport(id);
                self.position(id, self.v(id, X), self.v(id, Y), self.flight(id, false));
                return false;
            }
        }
        let mut amd = self.read(6, 0, ap, i64::from(self.pos(id)));
        let ox = amd[0];
        let oy = amd[1];
        amd[3] = (amd[3] + rotation) & 7;
        match rotation {
            0 => {}
            2 => {
                amd[0] = oy;
                amd[1] = i64::from((sy * 16 - ox - 1) as i16);
            }
            4 => {
                amd[0] = i64::from((sx * 16 - ox - 1) as i16);
                amd[1] = i64::from((sy * 16 - oy - 1) as i16);
            }
            6 => {
                amd[0] = i64::from((sx * 16 - oy - 1) as i16);
                amd[1] = ox;
            }
            _ => {
                self.leaf(48, id);
            }
        }
        let flags = amd[2];
        let mut x = self.tile_x(tile) * 16;
        let mut y = self.tile_y(tile) * 16;
        if flags & HELI_RAISE != 0 {
            let rotor = self.v(self.v(id, NEXT) as u32, NEXT) as u32;
            if self.v(rotor, SPEED) > 32 {
                self.set(id, SPEED, 0);
                self.set(rotor, SPEED, self.v(rotor, SPEED) - 1);
                if self.v(rotor, SPEED) == 32 && self.service(9, id, 0, 0, 0, 0, 0) == 0 {
                    let sound = self.read(11, id, 0, 0)[3];
                    self.service(
                        10,
                        id,
                        0,
                        if sound < self.world()[10] {
                            self.world()[11]
                        } else {
                            sound
                        },
                        0,
                        0,
                        0,
                    );
                }
            } else {
                self.set(rotor, SPEED, 32);
                let count = self.speed(id, 65535, true);
                if count > 0 {
                    self.set(id, TILE, 0);
                    let dest = self.bounds(id).0;
                    if self.v(id, Z) >= dest {
                        self.set(id, SPEED, 0);
                        return true;
                    }
                    self.position(
                        id,
                        self.v(id, X),
                        self.v(id, Y),
                        (self.v(id, Z) + i64::from(count)).min(dest),
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
            if s[6] != 9 {
                x = self.v(id, X);
                y = self.v(id, Y);
                tile = self.tile_virt(x, y);
            }
            self.set(id, TILE, i64::from(tile));
            let z = self.slope(x, y) + 1 + self.class(ap)[4];
            if z == self.v(id, Z) {
                let rotor = self.v(self.v(id, NEXT) as u32, NEXT) as u32;
                if self.v(rotor, SPEED) >= 80 {
                    self.flag(id, 3, false);
                    return true;
                }
                self.set(rotor, SPEED, self.v(rotor, SPEED) + 4);
            } else {
                let count = self.speed(id, 65535, true);
                if count > 0 {
                    let current = self.v(id, Z);
                    let dest = if current > z {
                        (current - i64::from(count)).max(z)
                    } else {
                        (current + i64::from(count)).min(z)
                    };
                    self.position(id, self.v(id, X), self.v(id, Y), dest);
                }
            }
            return false;
        }
        let dist = ((x + amd[0] - self.v(id, X)).abs() + (y + amd[1] - self.v(id, Y)).abs()) as u32;
        if flags & EXACT == 0 && dist <= if flags & SLOW_TURN != 0 { 8 } else { 4 } {
            return true;
        }
        if dist == 0 {
            let difference = (amd[3] - self.v(id, DIR)) & 7;
            if difference == 0 {
                self.set(id, SPEED, 0);
                return true;
            }
            if self.speed(id, 50, true) == 0 {
                return false;
            }
            self.set(
                id,
                DIR,
                (self.v(id, DIR) + if difference > 4 { 7 } else { 1 }) & 7,
            );
            self.set(id, SPEED, self.v(id, SPEED) >> 1);
            self.position_current(id);
            return false;
        }
        if flags & BRAKE != 0 && self.v(id, SPEED) > 50 * self.world()[4] {
            self.maybe_crash(id).await;
            if self.v(id, STATUS) & CRASHED != 0 {
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
                let vx = self.v(id, X);
                let vy = self.v(id, Y);
                px = vx + (x + amd[0] - vx).signum();
                py = vy + (y + amd[1] - vy).signum();
                next_tile = if s[6] == 9 {
                    s[1] as u32
                } else {
                    self.tile_virt(px, py)
                };
            } else {
                let newdir = self.read(13, id, x + amd[0], y + amd[1])[0];
                if newdir != self.v(id, DIR) {
                    if flags & SLOW_TURN != 0 && self.turns(id) < 8 && self.v(id, SUBTYPE) == 2 {
                        if self.turn(id) == 0 || newdir == i64::from(self.lastdir(id)) {
                            if newdir == i64::from(self.lastdir(id)) {
                                self.set_turns(id, 0);
                            } else {
                                self.set_turns(id, self.turns(id).wrapping_add(1));
                            }
                            self.set_turn(id, (2 * self.world()[4]) as u8);
                            self.set_lastdir(id, self.v(id, DIR) as u8);
                            self.set(id, DIR, newdir);
                        }
                        let gp = self.read(14, id, 0, 0);
                        px = gp[0];
                        py = gp[1];
                        next_tile = gp[2] as u32;
                    } else {
                        self.set(id, SPEED, self.v(id, SPEED) >> 1);
                        self.set(id, DIR, newdir);
                        px = self.v(id, X);
                        py = self.v(id, Y);
                        next_tile = self.v(id, TILE) as u32;
                    }
                } else {
                    self.set_turns(id, 0);
                    let gp = self.read(14, id, 0, 0);
                    px = gp[0];
                    py = gp[1];
                    next_tile = gp[2] as u32;
                }
            }
            self.set(id, TILE, i64::from(next_tile));
            if flags & (TAKING_OFF | SLOW_TURN | LAND) != 0 {
                self.set(id, TILE, 0);
            }
            let mut z = self.v(id, Z);
            if flags & TAKING_OFF != 0 {
                z = self.flight(id, true);
            } else if flags & HOLD != 0 {
                if z > self.height(id) + if self.v(id, SUBTYPE) == 0 { 184 } else { 150 } {
                    z -= 1;
                }
            } else if flags & (SLOW_TURN | NO_CLAMP) == SLOW_TURN | NO_CLAMP {
                z = self.flight(id, false);
            }
            let mut airport_z = self.v(id, Z);
            if flags & (LAND | BRAKE) != 0 && valid {
                airport_z = self.read(15, u32::from(self.target(id)), 0, 0)[0] + 1;
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
                if s[1] as u32 == INVALID_TILE {
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
    async fn crashed_tick(&self, id: u32) -> bool {
        self.set_crashed(id, self.crashed(id).wrapping_add(3));
        let valid = self.valid_airport(id);
        if self.crashed(id) < 500 && !valid && self.crashed(id).is_multiple_of(3) {
            let w = self.world();
            let z = self.slope(
                self.v(id, X).clamp(0, w[2] * 16),
                self.v(id, Y).clamp(0, w[3] * 16),
            );
            self.set(id, Z, self.v(id, Z) - 1);
            if self.v(id, Z) <= z {
                self.set_crashed(id, 500);
                self.set(id, Z, z + 1);
            } else {
                self.set_crashed(id, 0);
            }
            self.position_current(id);
        }
        if self.crashed(id) < 650 {
            let r = self.random();
            if chance16_i(1, 32, r) {
                let delta = [7, 0, 0, 1][((r >> 16) & 3) as usize];
                self.set(id, DIR, (self.v(id, DIR) + delta) & 7);
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
            self.action(104, id, 0, 0).await;
            return false;
        }
        true
    }
    fn smoke(&self, id: u32, mode: bool) {
        if self.v(id, STATUS) & BROKEN == 0 {
            return;
        }
        if self.v(id, SPEED) < 10 {
            self.status(id, BROKEN, false);
            self.set(id, BREAKDOWN, 0);
            return;
        }
        if !mode && self.v(id, TICK) & 15 == 0 {
            let (x, y) = [
                (5, 5),
                (6, 0),
                (5, -5),
                (0, -6),
                (-5, -5),
                (-6, 0),
                (-5, 5),
                (0, 6),
            ][self.v(id, DIR) as usize];
            self.effect(id, x, y, 2, 10);
        }
    }
    async fn crash(&self, id: u32, flooded: bool) -> u32 {
        let victims = (self.action(103, id, u32::from(flooded), 0).await as u32).wrapping_add(2);
        self.set_crashed(id, if flooded { 9000 } else { 0 });
        victims
    }
    async fn crash_airplane(&self, id: u32) {
        self.effect(id, 4, 4, 8, 5);
        let victims = self.crash(id, false).await;
        self.leaf(11, id);
        self.leaf(11, self.v(id, NEXT) as u32);
        let station = if self.valid_airport(id) {
            u32::from(self.target(id))
        } else {
            INVALID_STATION
        };
        self.service(12, id, station, i64::from(victims), 0, 0, 0);
        let w = self.world();
        let tile = self.tile_virt(
            self.v(id, X).clamp(0, w[2] * 16),
            self.v(id, Y).clamp(0, w[3] * 16),
        );
        self.service(13, tile, self.v(id, OWNER) as u32, -160, 30, 0, 0);
        if self.world()[8] != 0 {
            self.service(10, id, 0, self.world()[12], 0, 0, 0);
        }
    }
    async fn maybe_crash(&self, id: u32) {
        let ap = self.airport(id);
        let prob = if self.class(ap)[3] & 4 != 0
            && self.read(11, id, 0, 0)[1] & 2 != 0
            && self.world()[5] == 0
        {
            3276
        } else {
            let setting = self.world()[6];
            if setting == 0 {
                return;
            }
            (0x4000_u32.wrapping_shl(setting as u32)) / 1500
        };
        if self.random() & ((1 << 22) - 1) > prob {
            return;
        }
        for cargo in 0..64 {
            self.service(14, u32::from(self.target(id)), cargo, 0, 0, 0, 0);
        }
        self.crash_airplane(id).await;
    }
    async fn missing_orders(&self, id: u32) {
        if !self.valid_airport(id) {
            if self.action(105, id, 0, 0).await != 0 {
                self.crash_airplane(id).await;
            }
        } else if self.v(id, ORDER) != 2 {
            self.leaf(15, id);
        }
    }
    fn leave_hangar(&self, id: u32, direction: i64) {
        self.set(id, SPEED, 0);
        self.set(id, SUBSPEED, 0);
        self.set(id, PROGRESS, 0);
        self.set(id, DIR, direction);
        self.status(id, HIDDEN, false);
        let shadow = self.v(id, NEXT) as u32;
        self.status(shadow, HIDDEN, false);
        let rotor = self.v(shadow, NEXT) as u32;
        if rotor != INVALID {
            self.status(rotor, HIDDEN, false);
            self.set(rotor, SPEED, 80);
        }
        self.leaf(16, id);
        self.leaf(17, id);
        self.position_current(id);
        self.leaf(18, id);
    }
    fn terminal(&self, id: u32) {
        if self.v(id, ORDER) == 2 {
            return;
        }
        let station = u32::from(self.target(id));
        self.set(id, LAST_STATION, i64::from(station));
        if self.station(station)[12] & 16 == 0 {
            self.service(19, id, station, 0, 0, 0, 0);
        }
        self.leaf(20, id);
    }
    fn has_block(&self, id: u32, current: i64, ap: i64) -> bool {
        let reference = self.node(ap, self.pos(id));
        let c = self.fta(current);
        let next = self.fta(self.node(ap, c[2] as u8));
        if self.fta(self.node(ap, c[1] as u8))[3] as u64 != next[3] as u64 {
            let mut blocks = next[3] as u64;
            if current != reference && c[3] as u64 != NOTHING {
                blocks |= c[3] as u64;
            }
            if self.blocks(u32::from(self.target(id))) & blocks != 0 {
                self.set(id, SPEED, 0);
                self.set(id, SUBSPEED, 0);
                return true;
            }
        }
        false
    }
    fn set_block(&self, id: u32, current: i64, ap: i64) -> bool {
        let c = self.fta(current);
        let next = self.fta(self.node(ap, c[2] as u8));
        let reference = self.node(ap, self.pos(id));
        if (self.fta(self.node(ap, c[1] as u8))[3] as u64 & next[3] as u64) != next[3] as u64 {
            let mut blocks = next[3] as u64;
            let mut cursor = if current == reference { c[0] } else { current };
            while cursor != 0 {
                let node = self.fta(cursor);
                if node[4] == c[4] && node[3] != 0 {
                    blocks |= node[3] as u64;
                    break;
                }
                cursor = node[0];
            }
            if c[3] as u64 == next[3] as u64 {
                blocks ^= next[3] as u64;
            }
            let station = u32::from(self.target(id));
            if self.blocks(station) & blocks != 0 {
                self.set(id, SPEED, 0);
                self.set(id, SUBSPEED, 0);
                return false;
            }
            if next[3] as u64 != NOTHING {
                self.reserve(station, blocks);
            }
        }
        true
    }
    fn free_terminal(&self, id: u32, start: u8, end: u8) -> bool {
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
    fn terminal_count(&self, ap: i64, index: u32) -> u32 {
        self.read(16, index, ap, 0)[0] as u32
    }
    fn find_terminal(&self, id: u32, ap: i64) -> bool {
        let groups = self.terminal_count(ap, 0);
        if groups > 1 {
            let station = u32::from(self.target(id));
            let mut cursor = self.fta(self.node(ap, self.pos(id)))[0];
            while cursor != 0 {
                let node = self.fta(cursor);
                if node[4] != 255 {
                    return false;
                }
                if self.blocks(station) & node[3] as u64 == 0 {
                    let group = node[2] as u32 + 1;
                    let mut start = 0;
                    for i in 1..group {
                        start += self.terminal_count(ap, i);
                    }
                    let end = start + self.terminal_count(ap, group);
                    if self.free_terminal(id, start as u8, end as u8) {
                        return true;
                    }
                }
                cursor = node[0];
            }
        }
        let mut total = 0;
        for i in (1..=groups).rev() {
            total += self.terminal_count(ap, i);
        }
        self.free_terminal(id, 0, total as u8)
    }
    fn find_helipad(&self, id: u32, ap: i64) -> bool {
        let pads = self.class(ap)[2] as u8;
        if pads == 0 {
            self.find_terminal(id, ap)
        } else {
            self.free_terminal(id, 8, pads.wrapping_add(8))
        }
    }
    async fn state_handler(&self, id: u32, ap: i64) {
        match self.state(id) {
            HANGAR => {
                if self.previous(id) != self.pos(id) {
                    self.action(102, id, 0, 0).await;
                    self.set_state(id, self.fta(self.node(ap, self.pos(id)))[4] as u8);
                    return;
                }
                if self.v(id, ORDER) == 2 && self.v(id, STATUS) & STOPPED != 0 {
                    self.leaf(15, id);
                    return;
                }
                if self.read(23, id, 0, 0)[0] != 0 {
                    return;
                }
                if !matches!(self.v(id, ORDER), 1 | 2) {
                    return;
                }
                if self.v(id, ORDER) == 2 && self.v(id, DEST) == i64::from(self.target(id)) {
                    self.action(102, id, 0, 0).await;
                    return;
                }
                if self.has_block(id, self.node(ap, self.pos(id)), ap) {
                    return;
                }
                if self.v(id, DEST) == i64::from(self.target(id)) {
                    if self.v(id, SUBTYPE) == 0 {
                        if !self.find_helipad(id, ap) {
                            return;
                        }
                    } else if !self.find_terminal(id, ap) {
                        return;
                    }
                } else {
                    self.set_state(
                        id,
                        if self.v(id, SUBTYPE) == 0 {
                            HELITAKEOFF
                        } else {
                            TAKEOFF
                        },
                    );
                }
                let dir = self.read(17, id, 0, 0)[0];
                self.leave_hangar(id, dir);
                Box::pin(self.airport_move(id, ap)).await;
            }
            2..=9 | 19..=21 => {
                if self.previous(id) != self.pos(id) {
                    self.terminal(id);
                    self.set_state(id, self.fta(self.node(ap, self.pos(id)))[4] as u8);
                    if self.world()[7] != 0 && self.v(id, SUBTYPE) == 0 && self.class(ap)[2] > 0 {
                        let world = self.world();
                        self.set(id, ECONOMY_SERVICE, world[13]);
                        self.set(id, CALENDAR_SERVICE, world[14]);
                        self.set(id, BREAKDOWNS, 0);
                        self.set(id, RELIABILITY, self.read(11, id, 0, 0)[4]);
                        self.leaf(21, id);
                    }
                    return;
                }
                if self.v(id, ORDER) == 0 {
                    return;
                }
                if self.has_block(id, self.node(ap, self.pos(id)), ap) {
                    return;
                }
                let hangar = match self.v(id, ORDER) {
                    1 => false,
                    2 => self.v(id, DEST) == i64::from(self.target(id)),
                    7 => return,
                    _ => {
                        self.leaf(15, id);
                        true
                    }
                };
                self.set_state(
                    id,
                    if hangar && self.target_station(id)[8] != 0 {
                        HANGAR
                    } else if self.v(id, SUBTYPE) == 0 {
                        HELITAKEOFF
                    } else {
                        TAKEOFF
                    },
                );
                Box::pin(self.airport_move(id, ap)).await;
            }
            TAKEOFF => {
                if self.service(9, id, 0, 0, 0, 0, 0) == 0 {
                    self.service(10, id, 0, self.read(11, id, 0, 0)[3], 0, 0, 0);
                }
                self.set_state(id, STARTTAKEOFF);
            }
            STARTTAKEOFF => {
                self.set_state(id, ENDTAKEOFF);
                self.leaf(22, id);
            }
            ENDTAKEOFF => {
                self.set_state(id, FLYING);
                self.next_airport(id);
            }
            HELITAKEOFF => {
                self.set_state(id, FLYING);
                self.leaf(22, id);
                self.next_airport(id);
                if self.read(21, id, 0, 0)[0] != 0 {
                    self.action(105, id, 1, 0).await;
                }
            }
            FLYING => {
                let station = u32::from(self.target(id));
                let s = self.station(station);
                if self.read(18, id, i64::from(station), 0)[0] != 0
                    && (s[7] == OWNER_NONE || s[7] == self.v(id, OWNER))
                    && self.blocks(station) & CLOSED == 0
                {
                    let landing = if self.v(id, SUBTYPE) == 0 {
                        HELILANDING
                    } else {
                        LANDING
                    };
                    let mut cursor = self.fta(self.node(ap, self.pos(id)))[0];
                    while cursor != 0 {
                        let node = self.fta(cursor);
                        if node[4] == i64::from(landing) {
                            let speed = self.v(id, SPEED);
                            let sub = self.v(id, SUBSPEED);
                            if !self.has_block(id, cursor, ap) {
                                self.set_state(id, landing);
                                if landing == HELILANDING {
                                    self.flag(id, 3, true);
                                }
                                self.set_pos(id, node[2] as u8);
                                self.reserve(
                                    station,
                                    self.fta(self.node(ap, self.pos(id)))[3] as u64,
                                );
                                return;
                            }
                            self.set(id, SPEED, speed);
                            self.set(id, SUBSPEED, sub);
                        }
                        cursor = node[0];
                    }
                }
                self.set_state(id, FLYING);
                self.set_pos(id, self.fta(self.node(ap, self.pos(id)))[2] as u8);
            }
            LANDING => {
                self.set_state(id, ENDLANDING);
                self.leaf(22, id);
                self.service(23, id, u32::from(self.target(id)), 0, 0, 0, 0);
                if self.service(9, id, 1, 0, 0, 0, 0) == 0 {
                    self.service(10, id, 0, self.world()[15], 0, 0, 0);
                }
                if self.read(21, id, 0, 0)[0] != 0 {
                    self.action(105, id, 1, 0).await;
                }
            }
            HELILANDING => {
                self.set_state(id, HELIENDLANDING);
                self.leaf(22, id);
            }
            ENDLANDING => {
                if self.has_block(id, self.node(ap, self.pos(id)), ap) {
                    return;
                }
                if self.v(id, ORDER) == 1 && self.find_terminal(id, ap) {
                    return;
                }
                self.set_state(id, HANGAR);
            }
            HELIENDLANDING => {
                if self.has_block(id, self.node(ap, self.pos(id)), ap) {
                    return;
                }
                if self.v(id, ORDER) == 1 && self.find_helipad(id, ap) {
                    return;
                }
                self.set_state(
                    id,
                    if self.target_station(id)[8] != 0 {
                        HANGAR
                    } else {
                        HELITAKEOFF
                    },
                );
            }
            _ => {
                self.leaf(47, id);
            }
        }
    }
    async fn airport_move(&self, id: u32, ap: i64) -> bool {
        if i64::from(self.pos(id)) >= self.class(ap)[1] {
            self.service(46, id, 0, ap, 0, 0, 0);
        }
        let mut cursor = self.node(ap, self.pos(id));
        let current = self.fta(cursor);
        if current[4] == i64::from(self.state(id)) {
            let previous = self.pos(id);
            let state = self.state(id);
            self.state_handler(id, ap).await;
            if self.state(id) != FLYING {
                self.set_previous(id, previous);
            }
            if self.state(id) != state || self.pos(id) != previous {
                self.cache(id, false);
            }
            return true;
        }
        self.set_previous(id, self.pos(id));
        if current[0] == 0 {
            if self.set_block(id, cursor, ap) {
                self.set_pos(id, current[2] as u8);
                self.cache(id, false);
            }
            return false;
        }
        loop {
            let node = self.fta(cursor);
            if node[4] == i64::from(self.state(id)) || node[4] == 0 {
                if self.set_block(id, cursor, ap) {
                    self.set_pos(id, node[2] as u8);
                    self.cache(id, false);
                }
                return false;
            }
            cursor = node[0];
            if cursor == 0 {
                break;
            }
        }
        self.service(45, id, 0, 0, 0, 0, 0);
        false
    }
    async fn airport_next(&self, id: u32) {
        if !self.controller(id).await {
            return;
        }
        let ap = self.airport(id);
        let previous = self.fta(self.node(ap, self.previous(id)))[3] as u64;
        let current = self.fta(self.node(ap, self.pos(id)))[3] as u64;
        if previous != current {
            let station = u32::from(self.target(id));
            if !(self.blocks(station) & ZEPPELIN != 0 && previous == 1 << 8) {
                self.release(station, previous);
            }
        }
        self.airport_move(id, ap).await;
    }
    fn too_far(&self, id: u32, too_far: bool) {
        let has = self.flags(id) & 1 != 0;
        if too_far {
            if !has {
                self.flag(id, 0, true);
                self.leaf(8, id);
                self.leaf(24, id);
            }
            return;
        }
        if has {
            self.flag(id, 0, false);
            self.leaf(8, id);
            self.leaf(25, id);
        }
    }
    fn distance(&self, a: u32, b: u32) -> u32 {
        let dx = (self.tile_x(a) - self.tile_x(b)) as i32;
        let dy = (self.tile_y(a) - self.tile_y(b)) as i32;
        dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) as u32
    }
    async fn event(&self, id: u32, pass: u32) -> bool {
        if self.v(id, STATUS) & CRASHED != 0 {
            return self.crashed_tick(id).await;
        }
        if self.v(id, STATUS) & STOPPED != 0 {
            return true;
        }
        self.leaf(26, id);
        self.smoke(id, pass != 0);
        self.action(100, id, 0, 0).await;
        self.service(27, id, pass, 0, 0, 0, 0);
        if matches!(self.v(id, ORDER), 3 | 4) {
            return true;
        }
        if (ENDTAKEOFF..=HELIENDLANDING).contains(&self.state(id)) {
            self.too_far(id, false);
        } else if self.range_sqr(id) != 0 {
            let current = self.target_station(id);
            let next = if matches!(self.v(id, ORDER), 1 | 2) {
                self.station(self.v(id, DEST) as u32)
            } else {
                [0; 40]
            };
            if current[0] != 0
                && current[1] as u32 != INVALID_TILE
                && next[0] != 0
                && next[1] as u32 != INVALID_TILE
            {
                self.too_far(
                    id,
                    self.distance(current[1] as u32, next[1] as u32) > self.range_sqr(id),
                );
            }
        }
        if self.flags(id) & 1 == 0 {
            self.airport_next(id).await;
        }
        true
    }
    async fn tick(&self, id: u32) -> bool {
        if self.v(id, SUBTYPE) > 2 {
            return true;
        }
        self.set(
            id,
            TICK,
            i64::from((self.v(id, TICK) as u8).wrapping_add(1)),
        );
        if self.v(id, STATUS) & STOPPED == 0 {
            self.set(
                id,
                RUNNING,
                i64::from((self.v(id, RUNNING) as u8).wrapping_add(1)),
            );
        }
        if self.v(id, SUBTYPE) == 0 {
            self.helicopter_tick(id);
        }
        self.set(
            id,
            ORDER_TIME,
            i64::from((self.v(id, ORDER_TIME) as i32).wrapping_add(1)),
        );
        for pass in 0..2 {
            if !self.event(id, pass).await {
                return false;
            }
        }
        true
    }
    fn service_needed(&self, id: u32) {
        if self.read(19, id, 0, 0)[0] == 0 || self.read(21, id, 0, 0)[0] == 0 {
            return;
        }
        if self.read(22, id, 0, 0)[0] != 0 {
            self.leaf(16, id);
            return;
        }
        if !matches!(self.v(id, ORDER), 1 | 2) {
            return;
        }
        let station = self.v(id, DEST) as u32;
        let st = self.station(station);
        if st[8] != 0 && self.read(18, id, i64::from(station), 0)[0] != 0 {
            self.service(28, id, station, 0, 0, 0, 0);
            self.leaf(8, id);
        } else if self.v(id, ORDER) == 2 {
            self.leaf(29, id);
            self.leaf(8, id);
        }
    }
    fn day(&self, id: u32, calendar: bool) {
        if self.v(id, SUBTYPE) > 2 {
            return;
        }
        if calendar {
            self.leaf(30, id);
            return;
        }
        self.leaf(31, id);
        let day = (self.v(id, DAY) as u8).wrapping_add(1);
        self.set(id, DAY, i64::from(day));
        if day & 7 == 0 {
            self.leaf(32, id);
        }
        self.leaf(33, id);
        self.leaf(34, id);
        self.service_needed(id);
        if self.v(id, RUNNING) == 0 {
            return;
        }
        let cost = self.leaf(35, id);
        let ticks = self.v(id, RUNNING);
        let divisor = self.world()[16];
        // Money's saturating multiplication is provided by the shared Money service.
        let cost = cost.saturating_mul(ticks) / divisor;
        self.set(id, PROFIT, self.v(id, PROFIT).saturating_sub(cost));
        self.set(id, RUNNING, 0);
        self.service(38, id, 0, cost, 0, 0, 0);
        self.leaf(39, id);
    }
    fn nearest_hangar(&self, id: u32) -> u32 {
        let world = self.world();
        let vtile = self.tile_virt(
            self.v(id, X).clamp(0, world[2] * 16),
            self.v(id, Y).clamp(0, world[3] * 16),
        );
        let mut best = 0;
        let mut result = INVALID_STATION;
        let range = self.range_sqr(id);
        let mut last = INVALID_STATION;
        let mut next = INVALID_STATION;
        if range != 0 {
            if self.v(id, ORDER) == 1 || (self.v(id, ORDER) == 2 && self.read(24, id, 0, 0)[0] == 0)
            {
                last = self.v(id, LAST_STATION) as u32;
                next = self.v(id, DEST) as u32;
            } else {
                if self.valid_airport(id) {
                    last = u32::from(self.target(id));
                }
                next = self.leaf(40, id) as u32;
            }
        }
        let last = self.station(last);
        let next = self.station(next);
        let avi = self.read(11, id, 0, 0);
        let mut cursor = INVALID_STATION;
        loop {
            cursor = self.read(20, 0, i64::from(cursor), 0)[0] as u32;
            if cursor == INVALID_STATION {
                break;
            }
            let st = self.station(cursor);
            if st[7] != self.v(id, OWNER) || st[9] == 0 || st[8] == 0 {
                continue;
            }
            let ap = self.class(st[10]);
            if ap[3] & 4 != 0 && avi[1] & 2 != 0 && self.world()[5] == 0 {
                continue;
            }
            if ap[3] & 1 == 0 && avi[1] & 1 != 0 {
                continue;
            }
            if range != 0 {
                let ld = if last[0] != 0 && last[1] as u32 != INVALID_TILE {
                    self.distance(st[1] as u32, last[1] as u32)
                } else {
                    0
                };
                let nd = if next[0] != 0 && next[1] as u32 != INVALID_TILE {
                    self.distance(st[1] as u32, next[1] as u32)
                } else {
                    0
                };
                if ld > range || nd > range {
                    continue;
                }
            }
            let distance = self.distance(vtile, st[1] as u32);
            if distance < best || result == INVALID_STATION {
                best = distance;
                result = cursor;
            }
        }
        result
    }
    fn replacement(&self, station: u32) {
        let st = self.station(station);
        let ap = st[10];
        let rotation = if st[1] as u32 == INVALID_TILE {
            0
        } else {
            st[3]
        };
        let mut cursor = INVALID;
        loop {
            cursor = self.read(20, 1, i64::from(cursor), 0)[0] as u32;
            if cursor == INVALID {
                break;
            }
            if self.v(cursor, SUBTYPE) > 2 || u32::from(self.target(cursor)) != station {
                continue;
            }
            self.service(44, cursor, 0, 0, 0, 0, 0);
            if self.v(cursor, ORDER) == 2
                && self.read(25, cursor, 0, 0)[0] == 0
                && self.v(cursor, DEST) == i64::from(station)
                && (st[8] == 0 || self.read(18, cursor, i64::from(station), 0)[0] == 0)
            {
                self.leaf(29, cursor);
                self.leaf(8, cursor);
            }
            let pos = self.entry_point(cursor, ap, rotation);
            self.set_pos(cursor, pos);
            self.set_previous(cursor, pos);
            self.cache(cursor, false);
        }
        if st[8] == 0 {
            self.service(41, station, 0, 0, 0, 0, 0);
        }
    }
}
async fn run(w: World, operation: u32, id: u32, a: i64, b: i64, c: i64, _d: i64) -> i64 {
    match operation {
        0 => i64::from(w.tick(id).await),
        1 => {
            w.day(id, true);
            0
        }
        2 => {
            w.day(id, false);
            0
        }
        3 => {
            w.position(id, a, b, c);
            0
        }
        4 => {
            w.enter_hangar(id);
            0
        }
        5 => {
            w.cache(id, a != 0);
            0
        }
        6 => {
            let bounds = w.bounds(id);
            if a == 0 { bounds.0 } else { bounds.1 }
        }
        7 => w.flight_flags(id, b as usize as *mut u8, a != 0),
        8 => {
            w.missing_orders(id).await;
            0
        }
        9 => i64::from(w.crash(id, a != 0).await),
        10 => {
            w.next_airport(id);
            0
        }
        11 => {
            w.leave_hangar(id, a);
            0
        }
        12 => {
            if w.state(id) == FLYING {
                w.next_airport(id);
            }
            0
        }
        13 => {
            w.replacement(id);
            0
        }
        14 => i64::from(w.nearest_hangar(id)),
        15 => {
            let st = w.target_station(id);
            if st[0] != 0
                && st[1] as u32 != INVALID_TILE
                && w.read(18, id, i64::from(w.target(id)), 0)[0] != 0
                && st[8] != 0
            {
                i64::from(w.target(id))
            } else {
                i64::from(w.nearest_hangar(id))
            }
        }
        18 => i64::from(w.event(id, a as u32).await),
        19 => {
            if w.valid_airport(id) {
                let ap = w.airport(id);
                let bits = w.fta(w.node(ap, w.previous(id)))[3] as u64
                    | w.fta(w.node(ap, w.pos(id)))[3] as u64;
                w.release(u32::from(w.target(id)), bits);
            }
            0
        }
        20 => {
            let mut cursor = INVALID;
            loop {
                cursor = w.read(20, 1, i64::from(cursor), 0)[0] as u32;
                if cursor == INVALID {
                    break;
                }
                if w.v(cursor, SUBTYPE) <= 2 && u32::from(w.target(cursor)) == id {
                    w.set_target(cursor, INVALID_STATION as u16);
                }
            }
            0
        }
        16 => w.height(id),
        17 => w.height(id) + if w.v(id, SUBTYPE) == 0 { 184 } else { 150 },
        _ => 0,
    }
}
pub struct Run {
    channel: Rc<Channel>,
    future: Pin<Box<dyn Future<Output = i64>>>,
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
/// Create a call-scoped controller continuation over copied records.
/// # Safety
/// Callbacks/context live until destruction and fulfill the documented header contract.
/// Shared services are noexcept; returned reentry/destruction runs without Rust borrows.
/// All owner scalar access is on the serial game thread. Panics abort.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_create(
    operation: u32,
    id: u32,
    a: i64,
    b: i64,
    c: i64,
    d: i64,
    context: *mut c_void,
    read: unsafe extern "C" fn(*mut c_void, u32, u32, i64, i64, *mut i64),
    write: unsafe extern "C" fn(*mut c_void, u32, u32, i64),
    services: *const Services,
    service: unsafe extern "C" fn(*mut c_void, *const Action) -> i64,
) -> *mut Run {
    let channel = Rc::new(Channel {
        action: Cell::new(Action::default()),
        response: Cell::new(0),
    });
    // SAFETY: Immutable shared service table is copied for this invocation.
    let w = World {
        read,
        write,
        context,
        service,
        services: unsafe { *services },
        channel: channel.clone(),
    };
    Box::into_raw(Box::new(Run {
        channel,
        future: Box::pin(run(w, operation, id, a, b, c, d)),
    }))
}
/// Resume after a named C++ action; kind zero contains the result.
/// # Safety
/// Run is exclusively polled, all callbacks follow source lifetime preconditions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_advance(owner: *mut Run, response: i64) -> Action {
    // SAFETY: Only this invocation's continuation is borrowed, never world/private state.
    let run = unsafe { &mut *owner };
    run.channel.response.set(response);
    let mut context = PollContext::from_waker(Waker::noop());
    match run.future.as_mut().poll(&mut context) {
        Poll::Pending => run.channel.action.get(),
        Poll::Ready(a) => Action {
            a,
            ..Action::default()
        },
    }
}
/// Destroy/cancel the call-scoped continuation, including during C++ exception cleanup.
/// # Safety
/// Run is live/unique with no active poll. Cancellation invokes no shared service.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_aircraft_destroy(owner: *mut Run) {
    unsafe {
        drop(Box::from_raw(owner));
    }
}
