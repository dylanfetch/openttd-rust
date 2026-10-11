/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Complete town growth policy; canonical map, pools and shared caches remain C++.
//! Commands and `NewGRF` callbacks run directly after all owner access ends.
//! No private-state reference survives a shared service or a yielded action.
#![allow(
    unsafe_code,
    missing_docs,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::too_many_arguments,
    clippy::too_many_lines,
    clippy::similar_names,
    clippy::many_single_char_names,
    clippy::collapsible_if,
    clippy::collapsible_match,
    clippy::decimal_bitwise_operands
)]
use crate::services::{Services, chance16_i};
use std::{cell::Cell, ffi::c_void};

#[derive(Default)]
pub struct State {
    counter: u16,
    rate: u16,
    funding: u8,
    road: u8,
    flags: u8,
}
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_town_new() -> *mut State {
    Box::into_raw(Box::default())
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_destroy(state: *mut State) {
    // SAFETY: The shell owns this allocation, destroying it exactly once.
    unsafe {
        drop(Box::from_raw(state));
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_get(state: *const State, field: u8) -> u16 {
    // SAFETY: Serialized live owner; this borrow ends before returning to C++.
    let state = unsafe { &*state };
    match field {
        0 => state.counter,
        1 => state.rate,
        2 => u16::from(state.funding),
        3 => u16::from(state.road),
        _ => u16::from(state.flags),
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_set(state: *mut State, field: u8, value: u16) {
    // SAFETY: Serialized live owner; no reference survives this immediate write.
    let state = unsafe { &mut *state };
    match field {
        0 => state.counter = value,
        1 => state.rate = value,
        2 => state.funding = value as u8,
        3 => state.road = value as u8,
        _ => state.flags = value as u8,
    }
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Leaves {
    pub settings: extern "C" fn(*mut u32),
    pub town: extern "C" fn(u32, *mut u32),
    pub tile: extern "C" fn(u32, *mut u32),
    pub house: extern "C" fn(u32, *mut u32),
    pub road_info: extern "C" fn(u32, *mut u32),
    pub state: extern "C" fn(u32) -> *mut State,
    pub stations: extern "C" fn(u32, u32, *mut c_void, extern "C" fn(*mut c_void, u32, u32, u32)),
    pub dirty_town: extern "C" fn(u32),
    pub tropic_zone: extern "C" fn(u32) -> u32,
    pub tile_height: extern "C" fn(u32) -> u32,
    pub foundation_slope: extern "C" fn(u32) -> u32,
    pub terraform_price: extern "C" fn() -> i64,
    pub clean_road_bits: extern "C" fn(u32, u32) -> u32,
    pub set_road_town: extern "C" fn(u32, u32),
    pub exchange_company: extern "C" fn(u32) -> u32,
    pub increase_building_count: extern "C" fn(u32, u32),
    pub make_house: extern "C" fn(u32, u32, u32, u32),
    pub add_animation: extern "C" fn(u32),
    pub dirty_tile: extern "C" fn(u32),
    pub change_house_count: extern "C" fn(u32, u32),
    pub change_population: extern "C" fn(u32, u32),
    pub nearby_stations: extern "C" fn(u32, u32, u32),
    pub update_radius: extern "C" fn(u32),
    pub house_count: extern "C" fn() -> u32,
    pub building_count: extern "C" fn(u32, u32, u32) -> u16,
    pub update_max_pass: extern "C" fn(u32),
    pub next_town: extern "C" fn(u32) -> u32,
    pub road_mask: extern "C" fn() -> u64,
    pub build_road: extern "C" fn(u32, u32, u32, u32, u32) -> bool,
    pub clear_land: extern "C" fn(u32, u32) -> bool,
    pub terraform_land: extern "C" fn(u32, u32, u32, u32) -> CommandResult,
    pub build_bridge: extern "C" fn(u32, u32, u32, u32, u32) -> bool,
    pub build_tunnel: extern "C" fn(u32, u32, u32) -> bool,
    pub clear_house_land: extern "C" fn(u32),
    pub allow_construction: extern "C" fn(u32, u32, u32, u32) -> bool,
    pub construction_animation: extern "C" fn(u32),
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct CommandResult {
    pub cost: i64,
    pub success: bool,
}
#[derive(Clone)]
struct Engine {
    leaves: Leaves,
    services: Services,
    settings: Cell<[u32; 32]>,
}
impl Engine {
    fn tile(&self, id: u32) -> [u32; 32] {
        let mut record = [0; 32];
        (self.leaves.tile)(id, record.as_mut_ptr());
        record
    }
    fn town(&self, id: u32) -> [u32; 32] {
        let mut record = [0; 32];
        (self.leaves.town)(id, record.as_mut_ptr());
        record
    }
    fn house(&self, id: u32) -> [u32; 32] {
        let mut record = [0; 32];
        (self.leaves.house)(id, record.as_mut_ptr());
        record
    }
    fn road_info(&self, id: u32) -> [u32; 32] {
        let mut record = [0; 32];
        (self.leaves.road_info)(id, record.as_mut_ptr());
        record
    }
    fn get(&self, town: u32, field: u8) -> u16 {
        // SAFETY: The pool lookup yields the source's live town owner.
        unsafe { openttd_rust_town_get((self.leaves.state)(town), field) }
    }
    fn set(&self, town: u32, field: u8, value: u16) {
        // SAFETY: Every access is immediate; no owner reference crosses callbacks.
        unsafe {
            openttd_rust_town_set((self.leaves.state)(town), field, value);
        }
    }
    fn flags(&self, town: u32, clear: u16, set: u16) {
        self.set(town, 4, (self.get(town, 4) & !clear) | set);
    }
    fn chance(&self, a: u32, b: u32) -> bool {
        chance16_i(a, b, self.services.random())
    }
    fn dir(&self) -> u32 {
        self.services.random_range(4)
    }
    fn offset(&self, tile: u32, x: i32, y: i32) -> u32 {
        tile.wrapping_add((x + y * (self.settings.get()[0] as i32)) as u32)
    }
    fn step(&self, tile: u32, dir: u32) -> u32 {
        let (x, y) = [(-1, 0), (0, 1), (1, 0), (0, -1)][dir as usize];
        self.offset(tile, x, y)
    }
    fn valid(&self, tile: u32) -> bool {
        self.tile(tile)[4] != 0
    }
    fn rb(&self, tile: u32) -> u32 {
        self.tile(tile)[7]
    }
    fn gridpos(&self, town: u32, tile: u32) -> (i32, i32) {
        let center = self.town(town)[0];
        let sx = self.settings.get()[0];
        (
            (center % sx) as i32 - (tile % sx) as i32,
            (center / sx) as i32 - (tile / sx) as i32,
        )
    }
    fn distance(&self, a: u32, b: u32) -> u32 {
        let sx = self.settings.get()[0];
        let x = (a % sx) as i32 - (b % sx) as i32;
        let y = (a / sx) as i32 - (b / sx) as i32;
        (x * x + y * y) as u32
    }
    fn road(&self, town: u32, tile: u32, bits: u32, flags: u32) -> bool {
        let rt = self.road_type();
        (self.leaves.build_road)(town, tile, bits, rt, flags)
    }
    fn clear(&self, tile: u32, execute: bool) -> bool {
        (self.leaves.clear_land)(tile, u32::from(execute))
    }
    fn count_stations(&self, town: u32) -> u32 {
        struct StationCount {
            engine: Engine,
            center: u32,
            radius: u32,
            count: u32,
        }
        extern "C" fn visit(ctx: *mut c_void, xy: u32, load: u32, unload: u32) {
            // SAFETY: Synchronous non-reentrant visitor; context lives on this stack.
            let count = unsafe { &mut *ctx.cast::<StationCount>() };
            if count.engine.distance(xy, count.center) <= count.radius
                && (load <= 20 || unload <= 20)
            {
                count.count = count.count.wrapping_add(1);
            }
        }
        let record = self.town(town);
        let mut count = StationCount {
            engine: self.clone(),
            center: record[0],
            radius: record[6],
            count: 0,
        };
        (self.leaves.stations)(record[0], record[6] / 2, (&raw mut count).cast(), visit);
        count.count
    }
    fn update_rate(&self, town: u32) {
        if self.get(town, 4) & 8 != 0 {
            return;
        }
        let old = self.get(town, 1);
        let record = self.town(town);
        let rates = [
            [120_u32, 120, 120, 100, 80, 60],
            [320, 420, 300, 220, 160, 100],
        ];
        let funding = self.get(town, 2) != 0;
        let mut m = rates[usize::from(!funding)][self.count_stations(town).min(5) as usize];
        m >>= if self.settings.get()[3] == 0 {
            1
        } else {
            self.settings.get()[3] - 1
        };
        if record[5] != 0 {
            m /= 2;
        }
        let rate = ((m / (record[2] / 50 + 1)).min(930) + 1)
            .wrapping_mul(self.settings.get()[4])
            .wrapping_sub(1) as u16;
        self.set(town, 1, rate);
        if rate != u16::MAX {
            let counter = self.get(town, 0);
            self.set(
                town,
                0,
                if old == u16::MAX {
                    rate.min(counter)
                } else {
                    let numerator = u32::from(counter) * (u32::from(rate) + 1);
                    round_div(numerator as i32, u32::from(old) + 1) as u16
                },
            );
        }
        (self.leaves.dirty_town)(town);
    }
    fn update_growth(&self, town: u32) {
        self.update_rate(town);
        self.flags(town, 1, 0);
        (self.leaves.dirty_town)(town);
        let funding = self.get(town, 2) != 0;
        if self.settings.get()[3] == 0 && !funding {
            return;
        }
        let t = self.town(town);
        if !funding {
            for i in 0..6 {
                match t[11 + i] {
                    0xffff_fffe => {
                        if (self.leaves.tile_height)(t[0]) >= self.settings.get()[5]
                            && t[17 + i] == 0
                            && t[3] > 90
                        {
                            return;
                        }
                    }
                    u32::MAX => {
                        if (self.leaves.tropic_zone)(t[0]) == 1 && t[17 + i] == 0 && t[3] > 60 {
                            return;
                        }
                    }
                    goal => {
                        if goal > t[17 + i] {
                            return;
                        }
                    }
                }
            }
        }
        if self.get(town, 4) & 8 != 0 {
            if self.get(town, 1) != u16::MAX {
                self.flags(town, 0, 1);
            }
            (self.leaves.dirty_town)(town);
            return;
        }
        if !funding && self.count_stations(town) == 0 && !self.chance(1, 12) {
            return;
        }
        self.flags(town, 0, 1);
        (self.leaves.dirty_town)(town);
    }
}
fn reverse(dir: u32) -> u32 {
    (dir + 2) & 3
}
fn bit(dir: u32) -> u32 {
    1 << (3 - dir)
}
fn incline(dir: u32) -> u32 {
    [12, 6, 3, 9][dir as usize]
}

fn engine(leaves: *const Leaves, services: *const Services) -> Engine {
    // SAFETY: Immutable game-thread service tables remain live for each synchronous entry.
    let leaves = unsafe { *leaves };
    let services = unsafe { *services };
    let mut settings = [0; 32];
    (leaves.settings)(settings.as_mut_ptr());
    Engine {
        leaves,
        services,
        settings: Cell::new(settings),
    }
}
fn round_div(a: i32, b: u32) -> i32 {
    if a > 0 {
        (a + (b as i32) / 2) / (b as i32)
    } else {
        (a - ((b as i32) - 1) / 2) / (b as i32)
    }
}

impl Engine {
    fn road_type(&self) -> u32 {
        let mut mask = (self.leaves.road_mask)();
        let mut best = 0;
        let mut best_speed = None;
        while mask != 0 {
            let rt = mask.trailing_zeros();
            mask &= mask - 1;
            let rti = self.road_info(rt);
            if rti[0] == 0 {
                continue;
            }
            let date = rti[1] as i32;
            if date >= 0
                && date < self.settings.get()[17] as i32
                && date > self.settings.get()[16] as i32
            {
                continue;
            }
            let speed = if rti[2] == 0 { 50 } else { rti[2] };
            if best_speed.is_some_and(|previous| speed < previous) {
                continue;
            }
            best = rt;
            best_speed = Some(speed);
        }
        best
    }
    fn neighbour_road(&self, tile: u32, dir: u32, distance: u32) -> bool {
        if !self.valid(tile) {
            return false;
        }
        let left = self.step(tile, (dir + 1) & 3).wrapping_sub(tile);
        let right = self.step(tile, (dir + 3) & 3).wrapping_sub(tile);
        let back = self.step(tile, reverse(dir)).wrapping_sub(tile);
        for pos in 4..(distance + 1) * 4 {
            let mut diff = if pos & 1 != 0 { left } else { right }.wrapping_mul(pos / 4);
            if pos & 2 != 0 {
                diff = diff.wrapping_add(back);
            }
            let test = tile.wrapping_add(diff);
            if self.valid(test)
                && self.rb(test) & bit(if pos & 2 != 0 { dir } else { reverse(dir) }) != 0
            {
                return true;
            }
        }
        false
    }
    fn road_allowed(&self, town: u32, tile: u32, dir: u32) -> bool {
        let t = self.tile(tile);
        if t[5] == 0 || (t[6] != u32::MAX && t[6] == (dir & 1)) {
            return false;
        }
        if self.rb(tile) == 0
            && !self.road(town, tile, if dir & 1 != 0 { 5 } else { 10 }, 0)
            && !self.clear(tile, false)
        {
            return false;
        }
        let slope = if self.settings.get()[2] != 0 {
            (self.leaves.foundation_slope)(tile)
        } else {
            self.tile(tile)[1]
        };
        let ret = !self.neighbour_road(tile, dir, if self.town(town)[4] == 0 { 1 } else { 2 });
        if slope == 0 {
            return ret;
        }
        let desired = if dir & 1 != 0 { 9 } else { 12 };
        if desired != slope && (desired ^ 15) != slope {
            if self.chance(1, 8) {
                let mut success = false;
                if self.settings.get()[7] == 0 && self.chance(1, 10) {
                    let edges = if self.chance(1, 16) {
                        slope
                    } else {
                        slope ^ 15
                    };
                    success = (self.leaves.terraform_land)(tile, edges, 0, 1).success;
                }
                if !success && self.chance(1, 3) {
                    return ret;
                }
            }
            return false;
        }
        ret
    }
    fn terraform(&self, tile: u32, edges: u32, up: bool) -> bool {
        let CommandResult { success, cost } =
            (self.leaves.terraform_land)(tile, edges, u32::from(up), 0);
        if !success || cost >= ((self.leaves.terraform_price)() + 2) * 8 {
            return false;
        }
        (self.leaves.terraform_land)(tile, edges, u32::from(up), 1);
        true
    }
    fn level(&self, tile: u32) {
        if self.tile(tile)[0] == 3 {
            return;
        }
        let slope = self.tile(tile)[1];
        if slope == 0 {
            return;
        }
        if !self.terraform(tile, (!slope) & 15, true) {
            self.terraform(tile, slope & 15, false);
        }
    }
    fn grid(&self, town: u32, tile: u32, dir: u32) -> u32 {
        let (x, y) = self.gridpos(town, tile);
        let spacing = if self.town(town)[4] == 2 { 3 } else { 4 };
        let bits = if x % spacing == 0 { 5 } else { 0 } | if y % spacing == 0 { 10 } else { 0 };
        if bits != 15 {
            return bits;
        }
        let template = match self.tile(tile)[1] {
            1 => 3,
            3 => 7,
            2 => 6,
            6 => 14,
            4 => 12,
            12 => 13,
            8 => 9,
            9 => 11,
            23 | 27 | 29 | 30 => 0,
            _ => 15,
        };
        if bit(reverse(dir)) & template != 0 {
            template
        } else {
            bit(dir) | bit(reverse(dir))
        }
    }
    fn extra_house(&self, town: u32, tile: u32, modes: u32) -> bool {
        if self.tile(tile)[5] == 0 {
            return false;
        }
        let mut count = 0;
        for dir in 0..4 {
            if matches!(self.tile(self.step(tile, dir))[0], 3 | 7) {
                count += 1;
            }
            if count >= 3 {
                return self.try_house(town, tile, modes);
            }
        }
        false
    }
    fn road_continue(&self, town: u32, tile: u32, dir: u32) -> bool {
        let next = self.step(tile, dir);
        let t = self.tile(next);
        let rt = self.road_type();
        if t[4] == 0 {
            return false;
        }
        match t[0] {
            9 => return t[15] == 1 && t[16] == dir,
            5 => {
                return match t[13] {
                    1 => t[14] == dir & 1,
                    2 => t[14] == reverse(dir),
                    _ => false,
                };
            }
            2 => return t[12] != u32::MAX && t[12] == reverse(dir),
            1 => {
                if self.settings.get()[8] == 0 {
                    return false;
                }
            }
            _ => {}
        }
        (
            (self.leaves.build_road)(town, next, bit(reverse(dir)), rt, 0),
            0_i64,
        )
            .0
    }
    fn bridge(&self, town: u32, tile: u32, dir: u32) -> bool {
        let slope = self.tile(tile)[1];
        if slope != 0 && slope & incline(dir) != 0 {
            return false;
        }
        if self.rb(self.step(tile, reverse(dir))) & bit(dir) == 0 {
            return false;
        }
        let max = (self.town(town)[3] / 1000 + 5).min(11);
        let mut length = 0;
        let mut end = tile;
        loop {
            let old = length;
            length += 1;
            if old >= if slope == 0 { 5 } else { max } {
                return false;
            }
            end = self.step(end, dir);
            let t = self.tile(end);
            if t[4] == 0
                || !((t[24] != 0 && (slope != 0 || t[19] == 0))
                    || t[20] != 0
                    || (t[21] != 0 && t[22] != 0))
            {
                break;
            }
        }
        if length == 1 || !self.road_continue(town, end, dir) {
            return false;
        }
        if slope != 0 {
            let mut spiral = crate::spiral::hole(
                tile % self.settings.get()[0],
                tile / self.settings.get()[0],
                length,
                0,
                0,
                self.settings.get()[0],
                self.settings.get()[1],
            );
            while !spiral.is_end() {
                let t = self.tile(spiral.x + spiral.y * self.settings.get()[0]);
                if t[25] != 0 && t[15] == 1 && t[1] & incline(reverse(dir)) != 0 {
                    return false;
                }
                spiral = spiral.advance(self.settings.get()[0], self.settings.get()[1]);
            }
        }
        for _ in 0..=22 {
            let bridge = self.services.random_range(self.settings.get()[9] - 1);
            let rt = self.road_type();
            if (self.leaves.build_bridge)(tile, end, bridge, rt, 0) {
                (self.leaves.build_bridge)(tile, end, bridge, rt, 1);
                return true;
            }
        }
        false
    }
    fn tunnel(&self, town: u32, tile: u32, dir: u32) -> bool {
        if self.tile(tile)[1] != incline(dir)
            || self.rb(self.step(tile, reverse(dir))) & bit(dir) == 0
        {
            return false;
        }
        let max = if self.road_continue(town, tile, dir) {
            let mut test = tile;
            for _ in 0..4 {
                let t = self.tile(test);
                if t[4] == 0 {
                    return false;
                }
                let slope = t[1];
                if slope != incline(dir) && slope & 16 == 0 && !matches!(slope, 1 | 2 | 4 | 8) {
                    return false;
                }
                test = self.step(test, dir);
            }
            self.town(town)[3] / 1000 + 7
        } else {
            5
        };
        let mut length = 0_u8;
        let mut end = tile;
        loop {
            let old = length;
            length = length.wrapping_add(1);
            if u32::from(old) >= max {
                return false;
            }
            end = self.step(end, dir);
            if !self.valid(end) || self.tile(tile)[3] == self.tile(end)[3] {
                break;
            }
        }
        if length == 1 || !self.road_continue(town, end, dir) {
            return false;
        }
        let rt = self.road_type();
        if (self.leaves.build_tunnel)(tile, rt, 0) {
            (self.leaves.build_tunnel)(tile, rt, 1);
            return true;
        }
        false
    }
    fn roads_allow_house(&self, tile: u32) -> bool {
        let mut found = false;
        for (x, y) in [
            (-1, -1),
            (-1, 0),
            (-1, 1),
            (0, -1),
            (0, 1),
            (1, -1),
            (1, 0),
            (1, 1),
        ] {
            let t = self.tile(self.offset(tile, x, y));
            if t[4] == 0 || !(t[0] == 2 || t[13] != 0) {
                continue;
            }
            found = true;
            if t[8] != 63 && t[9] & 1 == 0 {
                return true;
            }
        }
        !found
    }
    fn grow_road(&self, tile: u32) -> bool {
        let t = self.tile(tile);
        t[0] != 2 || t[9] & 2 != 0 || t[8] == self.road_type()
    }
    fn can_follow(&self, tile: u32, dir: u32, modes: u32) -> bool {
        let next = self.step(tile, dir);
        let t = self.tile(next);
        if t[4] == 0 || t[18] != 0 {
            return false;
        }
        let bits = self.rb(next);
        if modes & 2 != 0 {
            match t[0] {
                2 => bits != 0,
                5 => t[13] == 1,
                9 => t[15] == 1,
                3 | 8 | 10 => false,
                _ => true,
            }
        } else {
            let back = bit(reverse(dir));
            bits & back != 0 && bits & !back != 0
        }
    }
}

impl Engine {
    fn in_tile(&self, town: u32, tile: &mut u32, cur: u32, mut dir: u32, modes: u32) -> u32 {
        let at = *tile;
        let layout = self.town(town)[4];
        let mut bits;
        if cur == 0 {
            if modes & 2 == 0 || (self.settings.get()[8] == 0 && self.tile(at)[0] == 1) {
                return 1;
            }
            if self.settings.get()[2] == 0 || self.chance(1, 6) {
                self.level(at);
            }
            if layout >= 2 {
                bits = self.grid(town, at, dir);
                if bits == 0 {
                    return 1;
                }
            } else {
                if !self.road_allowed(town, at, dir) {
                    return 1;
                }
                let source = reverse(dir);
                if self.chance(1, 4) {
                    loop {
                        dir = self.dir();
                        if dir != source {
                            break;
                        }
                    }
                }
                if !self.road_allowed(town, self.step(at, dir), dir) {
                    if dir != reverse(source) {
                        return 1;
                    }
                    if self.tile(self.step(at, (dir + 1) & 3))[0] != 3
                        && self.tile(self.step(at, (dir + 3) & 3))[0] != 3
                    {
                        return 1;
                    }
                }
                bits = bit(dir) | bit(source);
            }
        } else if dir < 4 && cur & bit(reverse(dir)) == 0 {
            if !self.grow_road(at) {
                return 2;
            }
            if modes & 2 == 0 {
                return 1;
            }
            bits = if layout >= 2 {
                self.grid(town, at, dir)
            } else {
                bit(reverse(dir))
            };
        } else {
            let mut allow = true;
            let t = self.tile(at);
            if t[0] == 9 {
                if t[15] == 1 && (dir != 4 || self.chance(1, 2)) {
                    *tile = t[17];
                }
                return 2;
            }
            dir = self.dir();
            let target = bit(dir);
            let house_tile = if cur & target != 0 {
                if cur & 10 != target {
                    return 2;
                }
                let house = match cur {
                    9 => self.offset(at, 1, 1),
                    6 => self.offset(at, -1, -1),
                    12 => self.offset(at, 1, -1),
                    3 => self.offset(at, -1, 1),
                    _ => return 2,
                };
                dir = 4;
                house
            } else {
                self.step(at, dir)
            };
            if self.tile(house_tile)[18] != 0 {
                return 2;
            }
            if !self.valid(house_tile) {
                return 2;
            }
            let mut result = 2;
            bits = 0;
            if dir != 4 && modes & 2 != 0 {
                if layout == 3 || layout == 1 {
                    if self.extra_house(town, self.step(house_tile, dir), modes) {
                        result = 0;
                    }
                }
                if layout >= 2 {
                    bits = self.grid(town, at, dir);
                    allow = bits & target == 0;
                } else {
                    bits = target;
                    allow = !self.road_allowed(town, house_tile, dir) || self.chance(6, 10);
                }
            }
            allow &= self.roads_allow_house(house_tile);
            if allow {
                if self.tile(house_tile)[0] != 3 {
                    if self.chance(1, 6) {
                        self.level(house_tile);
                    }
                    if self.try_house(town, house_tile, modes) {
                        result = 0;
                    }
                }
                return result;
            }
            if !self.grow_road(at) {
                return result;
            }
        }
        if self.tile(at)[18] != 0 {
            return 1;
        }
        bits = (self.leaves.clean_road_bits)(at, bits);
        if bits == 0 {
            return 1;
        }
        if self.bridge(town, at, dir) || self.tunnel(town, at, dir) || self.road(town, at, bits, 1)
        {
            return 0;
        }
        1
    }
    fn at_road(&self, town: u32, mut tile: u32, modes: u32) -> bool {
        let t = self.town(town);
        let factor = match t[4] {
            1 => 2,
            2 | 3 => 1,
            _ => 4,
        };
        let mut iterations = (10 + t[2].wrapping_mul(factor) / 9) as i32;
        let mut dir = 4;
        loop {
            let mut cur = self.rb(tile);
            match self.in_tile(town, &mut tile, cur, dir, modes) {
                0 => return true,
                1 => iterations = 0,
                _ => {}
            }
            if dir < 4 {
                cur &= !bit(reverse(dir));
            }
            if cur == 0 {
                return false;
            }
            let t = self.tile(tile);
            if t[0] == 9 {
                dir = reverse(t[16]);
            } else {
                loop {
                    if cur == 0 {
                        return false;
                    }
                    let target = loop {
                        dir = self.dir();
                        let bits = bit(dir);
                        if cur & bits != 0 {
                            break bits;
                        }
                    };
                    cur &= !target;
                    if self.can_follow(tile, dir, modes) {
                        break;
                    }
                }
            }
            tile = self.step(tile, dir);
            let t = self.tile(tile);
            if t[0] == 2 && t[12] == u32::MAX && t[8] != 63 {
                if t[10] == self.settings.get()[11] && t[11] != town {
                    return false;
                }
                if t[10] == self.settings.get()[12] && self.settings.get()[6] != 0 {
                    (self.leaves.set_road_town)(tile, town);
                }
            }
            iterations -= 1;
            if iterations < 0 {
                break;
            }
        }
        false
    }
    fn grow(&self, town: u32, modes: u32) -> bool {
        const OFFSETS: [(i32, i32); 13] = [
            (-1, 0),
            (1, 1),
            (1, -1),
            (-1, -1),
            (-1, 0),
            (0, 2),
            (2, 0),
            (0, -2),
            (-1, -1),
            (-2, 2),
            (2, 2),
            (2, -2),
            (0, 0),
        ];
        // Company replacement is a shared scalar leaf; the C++ invocation facade
        // additionally restores it on command exceptions while dropping the task.
        let company = (self.leaves.exchange_company)(self.settings.get()[11]);
        let mut tile = self.town(town)[0];
        for (x, y) in OFFSETS {
            if self.rb(tile) != 0 {
                let result = self.at_road(town, tile, modes);
                (self.leaves.exchange_company)(company);
                return result;
            }
            tile = self.offset(tile, x, y);
        }
        if modes & 2 != 0 {
            tile = self.town(town)[0];
            for (x, y) in OFFSETS {
                let t = self.tile(tile);
                if t[0] != 3 && t[1] == 0 && self.clear(tile, false) {
                    let rt = self.road_type();
                    let r = self.services.random();
                    let a = r & 3;
                    let mut b = (r >> 8) & 3;
                    if a == b {
                        b ^= 2;
                    }
                    (self.leaves.build_road)(town, tile, (1 << a) + (1 << b), rt, 2);
                    (self.leaves.exchange_company)(company);
                    return true;
                }
                tile = self.offset(tile, x, y);
            }
        }
        (self.leaves.exchange_company)(company);
        false
    }
    fn layout_house(&self, town: u32, tile: u32, modes: u32, square: bool) -> bool {
        if modes & 1 == 0 {
            return false;
        }
        if modes & 2 == 0 {
            return true;
        }
        let (x, y) = self.gridpos(town, tile);
        match self.town(town)[4] {
            2 => {
                if square {
                    matches!(x % 3, 2 | -1) && matches!(y % 3, 2 | -1)
                } else {
                    x % 3 != 0 && y % 3 != 0
                }
            }
            3 => {
                if square {
                    x & 3 >= 2 && y & 3 >= 2
                } else {
                    x % 4 != 0 && y % 4 != 0
                }
            }
            _ => true,
        }
    }
    fn can_house(&self, tile: u32, noslope: bool) -> bool {
        let slope = self.tile(tile)[1];
        if (noslope && slope != 0) || slope & 16 != 0 {
            return false;
        }
        if !self.roads_allow_house(tile) || self.tile(tile)[6] != u32::MAX {
            return false;
        }
        self.clear(tile, false)
    }
    fn same_z(&self, tile: u32, z: u32, noslope: bool) -> bool {
        self.can_house(tile, noslope) && self.tile(tile)[2] == z
    }
    fn free_square(&self, mut tile: u32, z: u32, noslope: bool) -> bool {
        if !self.same_z(tile, z, noslope) {
            return false;
        }
        for dir in 1..4 {
            tile = self.step(tile, dir);
            if !self.same_z(tile, z, noslope) {
                return false;
            }
        }
        true
    }
    fn check_house(
        &self,
        town: u32,
        tile: &mut u32,
        z: u32,
        noslope: bool,
        size: u32,
        modes: u32,
    ) -> bool {
        if size & 16 != 0 {
            let mut test = *tile;
            for dir in 1..=4 {
                if self.layout_house(town, test, modes, true) && self.free_square(test, z, noslope)
                {
                    *tile = test;
                    return true;
                }
                if dir == 4 {
                    break;
                }
                test = self.step(test, reverse(dir));
            }
            false
        } else if size & 12 != 0 {
            let dir = if size & 4 != 0 { 2 } else { 1 };
            let mut test = self.step(*tile, dir);
            if self.layout_house(town, test, modes, false) && self.same_z(test, z, noslope) {
                return true;
            }
            test = self.step(*tile, reverse(dir));
            if self.layout_house(town, test, modes, false) && self.same_z(test, z, noslope) {
                *tile = test;
                return true;
            }
            false
        } else {
            true
        }
    }
    fn make_house_tile(
        &self,
        town: u32,
        tile: u32,
        counter: u32,
        stage: u32,
        house: u32,
        random: u32,
        protected: bool,
    ) {
        (self.leaves.clear_house_land)(tile);
        (self.leaves.increase_building_count)(town, house);
        (self.leaves.make_house)(
            tile,
            town,
            house,
            counter | (stage << 8) | (random << 16) | (u32::from(protected) << 24),
        );
        if self.house(house)[2] & 32 != 0 {
            (self.leaves.add_animation)(tile);
        }
        (self.leaves.dirty_tile)(tile);
    }
    fn build_house(
        &self,
        town: u32,
        tile: u32,
        house: u32,
        random: u32,
        complete: bool,
        protected: bool,
    ) {
        (self.leaves.change_house_count)(town, 1);
        let h = self.house(house);
        let mut counter = 0;
        let mut stage = 0;
        if self.settings.get()[7] != 0 || self.settings.get()[6] != 0 || complete {
            let construction = self.services.random();
            stage = 3;
            if self.settings.get()[7] != 0 && h[3] & 1 == 0 && self.chance(1, 7) {
                stage = construction & 3;
            }
            if stage == 3 {
                (self.leaves.change_population)(town, h[9]);
            } else {
                counter = (construction >> 2) & 3;
            }
        }
        let size = self.house(house)[2];
        let mut part = house;
        self.make_house_tile(town, tile, counter, stage, part, random, protected);
        if size & 24 != 0 {
            part += 1;
            self.make_house_tile(
                town,
                self.offset(tile, 0, 1),
                counter,
                stage,
                part,
                random,
                protected,
            );
        }
        if size & 20 != 0 {
            part += 1;
            self.make_house_tile(
                town,
                self.offset(tile, 1, 0),
                counter,
                stage,
                part,
                random,
                protected,
            );
        }
        if size & 16 != 0 {
            part += 1;
            self.make_house_tile(
                town,
                self.offset(tile, 1, 1),
                counter,
                stage,
                part,
                random,
                protected,
            );
        }
        (self.leaves.nearby_stations)(town, tile, size);
        (self.leaves.update_radius)(town);
        self.update_rate(town);
        let size = self.house(house)[2];
        (self.leaves.construction_animation)(tile);
        if size & 24 != 0 {
            (self.leaves.construction_animation)(self.offset(tile, 0, 1));
        }
        if size & 20 != 0 {
            (self.leaves.construction_animation)(self.offset(tile, 1, 0));
        }
        if size & 16 != 0 {
            (self.leaves.construction_animation)(self.offset(tile, 1, 1));
        }
    }
    fn try_house(&self, town: u32, base: u32, modes: u32) -> bool {
        if !self.layout_house(town, base, modes, false) || !self.can_house(base, false) {
            return false;
        }
        let t = self.tile(base);
        let slope = t[1];
        let maxz = t[2];
        let center = self.town(town);
        let distance = self.distance(center[0], base);
        let mut zone = 0;
        if self.get(town, 2) != 0 && distance <= 25 {
            zone = 4;
        } else {
            for i in 0..5 {
                if distance < center[6 + i] {
                    zone = i as u32;
                }
            }
        }
        let climate = match self.settings.get()[10] {
            0 => 12,
            1 => {
                if maxz > self.settings.get()[13] {
                    11
                } else {
                    13
                }
            }
            2 => 14,
            _ => 15,
        };
        let zones = (1 << zone) | (1 << climate);
        let mut probs = Vec::new();
        let mut max = 0_u32;
        for house in 0..(self.leaves.house_count)() {
            let h = self.house(house);
            if h[0] & zones != zones || h[1] == 0 || h[4] != u16::MAX.into() {
                continue;
            }
            if (self.leaves.building_count)(town, house, h[8]) == u16::MAX {
                continue;
            }
            max = max.wrapping_add(h[5]);
            probs.push((house, h[5]));
        }
        while max > 0 {
            let mut tile = base;
            let mut r = self.services.random_range(max);
            let index = probs
                .iter()
                .position(|(_, weight)| {
                    if *weight > r {
                        true
                    } else {
                        r -= *weight;
                        false
                    }
                })
                .unwrap();
            let (house, weight) = probs.swap_remove(index);
            max -= weight;
            let h = self.house(house);
            if self.settings.get()[7] == 0 && self.settings.get()[6] == 0 && h[3] & 1 != 0 {
                continue;
            }
            let year = self.settings.get()[14] as i32;
            if year < (h[6] as i32) || year > (h[7] as i32) {
                continue;
            }
            let oneof = if h[2] & 64 != 0 {
                2
            } else if h[2] & 128 != 0 {
                4
            } else {
                0
            };
            if self.get(town, 4) & oneof != 0 {
                continue;
            }
            let noslope = h[2] & 2 != 0;
            if noslope && slope != 0 {
                continue;
            }
            if !self.check_house(town, &mut tile, maxz, noslope, h[2], modes) {
                continue;
            }
            let random = u32::from(self.services.random() as u8);
            if self.house(house)[10] != 0
                && !(
                    (self.leaves.allow_construction)(town, tile, house, random),
                    0_i64,
                )
                    .0
            {
                continue;
            }
            self.flags(town, 0, oneof);
            self.build_house(
                town,
                tile,
                house,
                random,
                false,
                self.house(house)[3] & 2 != 0,
            );
            return true;
        }
        false
    }
    fn tick(&self, town: u32) {
        if self.get(town, 4) & 1 == 0 {
            return;
        }
        let modes = 1 | if self.settings.get()[15] != 0 { 2 } else { 0 };
        let mut counter = i32::from(self.get(town, 0)) - 1;
        if counter < 0 {
            counter = i32::from(if self.grow(town, modes) {
                self.get(town, 1)
            } else {
                self.get(town, 1).min((self.settings.get()[4] - 1) as u16)
            });
        }
        self.set(town, 0, counter as u16);
    }
    fn expand(&self, town: u32, amount: u32, modes: u32) {
        if amount == 0 {
            let amount = self
                .services
                .random_range((self.town(town)[2] / 10).min(u32::from(u16::MAX)))
                + 3;
            (self.leaves.change_house_count)(town, amount);
            (self.leaves.update_radius)(town);
            for _ in 0..amount * 10 {
                self.grow(town, modes);
            }
            (self.leaves.change_house_count)(town, amount.wrapping_neg());
        } else {
            for _ in 0..amount {
                for _ in 0..25 {
                    if self.grow(town, modes) {
                        break;
                    }
                }
            }
        }
        (self.leaves.update_radius)(town);
        (self.leaves.update_max_pass)(town);
    }
    fn tick_all(&self) {
        if self.settings.get()[6] == 0 {
            let mut id = (self.leaves.next_town)(0);
            while id != u32::MAX {
                self.tick(id);
                id = (self.leaves.next_town)(id + 1);
            }
        }
    }
    fn monthly_counters(&self, town: u32) {
        for field in [3, 2] {
            let months = self.get(town, field);
            if months != 0 {
                self.set(town, field, months - 1);
            }
        }
    }
    fn custom_rate(&self, town: u32, a: u32) {
        if a == 0 {
            self.flags(town, 8, 0);
        } else {
            let old = u32::from(self.get(town, 1));
            let counter = u32::from(self.get(town, 0));
            self.set(
                town,
                0,
                if counter >= old {
                    a as u16
                } else {
                    (counter * a / old) as u16
                },
            );
            self.set(town, 1, a as u16);
            self.flags(town, 0, 8);
        }
        self.update_growth(town);
    }
    fn fund(&self, town: u32) {
        self.set(town, 2, 3);
        self.update_growth(town);
        let counter = i32::from(self.get(town, 0));
        let rate = i32::from(self.get(town, 1));
        let ticks = self.settings.get()[4] as i32;
        self.set(
            town,
            0,
            (counter.min(2 * ticks - (rate - counter) % ticks)) as u16,
        );
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_grow(
    leaves: *const Leaves,
    services: *const Services,
    town: u32,
    modes: u32,
) -> bool {
    engine(leaves, services).grow(town, modes)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_try_house(
    leaves: *const Leaves,
    services: *const Services,
    town: u32,
    tile: u32,
    modes: u32,
) -> bool {
    engine(leaves, services).try_house(town, tile, modes)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_build_house(
    leaves: *const Leaves,
    services: *const Services,
    town: u32,
    tile: u32,
    house: u32,
    random: u32,
    complete: bool,
    protected: bool,
) {
    engine(leaves, services).build_house(town, tile, house, random, complete, protected);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_tick_all(
    leaves: *const Leaves,
    services: *const Services,
) {
    engine(leaves, services).tick_all();
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_update_rate(
    leaves: *const Leaves,
    services: *const Services,
    town: u32,
) {
    engine(leaves, services).update_rate(town);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_update_growth(
    leaves: *const Leaves,
    services: *const Services,
    town: u32,
) {
    engine(leaves, services).update_growth(town);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_expand(
    leaves: *const Leaves,
    services: *const Services,
    town: u32,
    amount: u32,
    modes: u32,
) {
    engine(leaves, services).expand(town, amount, modes);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_monthly_counters(
    leaves: *const Leaves,
    services: *const Services,
    town: u32,
) {
    engine(leaves, services).monthly_counters(town);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_custom_rate(
    leaves: *const Leaves,
    services: *const Services,
    town: u32,
    rate: u32,
) {
    engine(leaves, services).custom_rate(town, rate);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_fund(
    leaves: *const Leaves,
    services: *const Services,
    town: u32,
) {
    engine(leaves, services).fund(town);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_road_type(
    leaves: *const Leaves,
    services: *const Services,
) -> u32 {
    engine(leaves, services).road_type()
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_tunnel(
    leaves: *const Leaves,
    services: *const Services,
    town: u32,
    tile: u32,
    dir: u32,
) -> bool {
    engine(leaves, services).tunnel(town, tile, dir)
}
