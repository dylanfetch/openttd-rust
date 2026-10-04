/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Complete town growth policy; canonical map, pools and shared caches remain C++.
//! Commands and `NewGRF` callbacks execute after the future yields to its C++ facade.
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
use std::{
    cell::Cell,
    ffi::c_void,
    future::Future,
    pin::Pin,
    rc::Rc,
    task::{Context, Poll, Waker},
};

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
#[derive(Default, Clone, Copy)]
pub struct Action {
    pub kind: u32,
    pub town: u32,
    pub tile: u32,
    pub a: u32,
    pub b: u32,
    pub c: u32,
    pub d: u32,
    pub cost: i64,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Leaves {
    pub observe: extern "C" fn(u32, u32, *mut u32),
    pub leaf: extern "C" fn(u32, u32, u32, u32, u32) -> u64,
    pub state: extern "C" fn(u32) -> *mut State,
    pub stations: extern "C" fn(u32, u32, *mut c_void, extern "C" fn(*mut c_void, u32, u32, u32)),
}
#[derive(Default)]
struct Exchange {
    action: Cell<Action>,
    result: Cell<(u64, i64)>,
}
struct Request {
    exchange: Rc<Exchange>,
    action: Action,
    yielded: bool,
}
impl Future for Request {
    type Output = (u64, i64);
    fn poll(mut self: Pin<&mut Self>, _: &mut Context<'_>) -> Poll<Self::Output> {
        if self.yielded {
            Poll::Ready(self.exchange.result.get())
        } else {
            self.exchange.action.set(self.action);
            self.yielded = true;
            Poll::Pending
        }
    }
}
#[derive(Clone)]
struct Engine {
    leaves: Leaves,
    services: Services,
    exchange: Rc<Exchange>,
    settings: Rc<Cell<[u32; 32]>>,
}
impl Engine {
    fn observe(&self, kind: u32, id: u32) -> [u32; 32] {
        let mut record = [0; 32];
        (self.leaves.observe)(kind, id, record.as_mut_ptr());
        record
    }
    fn tile(&self, tile: u32) -> [u32; 32] {
        self.observe(2, tile)
    }
    fn town(&self, town: u32) -> [u32; 32] {
        self.observe(1, town)
    }
    fn house(&self, house: u32) -> [u32; 32] {
        self.observe(3, house)
    }
    fn leaf(&self, op: u32, id: u32, a: u32, b: u32, c: u32) -> u64 {
        (self.leaves.leaf)(op, id, a, b, c)
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
    async fn action(&self, kind: u32, town: u32, tile: u32, args: [u32; 4]) -> (u64, i64) {
        let result = Request {
            exchange: self.exchange.clone(),
            action: Action {
                kind,
                town,
                tile,
                a: args[0],
                b: args[1],
                c: args[2],
                d: args[3],
                cost: 0,
            },
            yielded: false,
        }
        .await;
        self.settings.set(self.observe(0, 0));
        result
    }
    async fn road(&self, town: u32, tile: u32, bits: u32, flags: u32) -> bool {
        let rt = self.road_type();
        self.action(1, town, tile, [bits, rt, flags, 0]).await.0 != 0
    }
    async fn clear(&self, tile: u32, execute: bool) -> bool {
        self.action(2, 0, tile, [u32::from(execute), 0, 0, 0])
            .await
            .0
            != 0
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
        self.leaf(1, town, 0, 0, 0);
    }
    fn update_growth(&self, town: u32) {
        self.update_rate(town);
        self.flags(town, 1, 0);
        self.leaf(1, town, 0, 0, 0);
        let funding = self.get(town, 2) != 0;
        if self.settings.get()[3] == 0 && !funding {
            return;
        }
        let t = self.town(town);
        if !funding {
            for i in 0..6 {
                match t[11 + i] {
                    0xffff_fffe => {
                        if self.leaf(3, t[0], 0, 0, 0) as u32 >= self.settings.get()[5]
                            && t[17 + i] == 0
                            && t[3] > 90
                        {
                            return;
                        }
                    }
                    u32::MAX => {
                        if self.leaf(2, t[0], 0, 0, 0) == 1 && t[17 + i] == 0 && t[3] > 60 {
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
            self.leaf(1, town, 0, 0, 0);
            return;
        }
        if !funding && self.count_stations(town) == 0 && !self.chance(1, 12) {
            return;
        }
        self.flags(town, 0, 1);
        self.leaf(1, town, 0, 0, 0);
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

pub struct Task {
    exchange: Rc<Exchange>,
    future: Pin<Box<dyn Future<Output = u32>>>,
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_begin(
    leaves: *const Leaves,
    services: *const Services,
    operation: u32,
    town: u32,
    tile: u32,
    a: u32,
    b: u32,
    c: u32,
    d: u32,
) -> *mut Task {
    // SAFETY: Both immutable tables remain live while copied; no pointers retained.
    let leaves = unsafe { *leaves };
    let services = unsafe { *services };
    let exchange = Rc::new(Exchange::default());
    let mut settings = [0; 32];
    (leaves.observe)(0, 0, settings.as_mut_ptr());
    let engine = Engine {
        leaves,
        services,
        exchange: exchange.clone(),
        settings: Rc::new(Cell::new(settings)),
    };
    let future = Box::pin(async move { engine.run(operation, town, tile, a, b, c, d).await });
    Box::into_raw(Box::new(Task { exchange, future }))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_step(
    task: *mut Task,
    result: u64,
    cost: i64,
    action: *mut Action,
) -> u32 {
    // SAFETY: Facade exclusively owns this task between calls; all state access is raw.
    let task = unsafe { &mut *task };
    task.exchange.result.set((result, cost));
    let mut context = Context::from_waker(Waker::noop());
    match task.future.as_mut().poll(&mut context) {
        Poll::Pending => {
            unsafe {
                *action = task.exchange.action.get();
            }
            0
        }
        Poll::Ready(value) => {
            unsafe {
                *action = Action {
                    kind: 0,
                    a: value,
                    ..Action::default()
                };
            }
            1
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_task_destroy(task: *mut Task) {
    // SAFETY: Facade RAII destroys once, including exceptional command return.
    unsafe {
        drop(Box::from_raw(task));
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
        let mut mask = self.leaf(21, 0, 0, 0, 0);
        let mut best = 0;
        let mut best_speed = None;
        while mask != 0 {
            let rt = mask.trailing_zeros();
            mask &= mask - 1;
            let rti = self.observe(4, rt);
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
    async fn road_allowed(&self, town: u32, tile: u32, dir: u32) -> bool {
        let t = self.tile(tile);
        if t[5] == 0 || (t[6] != u32::MAX && t[6] == (dir & 1)) {
            return false;
        }
        if self.rb(tile) == 0
            && !self
                .road(town, tile, if dir & 1 != 0 { 5 } else { 10 }, 0)
                .await
            && !self.clear(tile, false).await
        {
            return false;
        }
        let slope = if self.settings.get()[2] != 0 {
            self.leaf(4, tile, 0, 0, 0) as u32
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
                    success = self.action(3, 0, tile, [edges, 0, 1, 0]).await.0 != 0;
                }
                if !success && self.chance(1, 3) {
                    return ret;
                }
            }
            return false;
        }
        ret
    }
    async fn terraform(&self, tile: u32, edges: u32, up: bool) -> bool {
        let (success, cost) = self.action(3, 0, tile, [edges, u32::from(up), 0, 0]).await;
        if success == 0 || cost >= ((self.leaf(5, 0, 0, 0, 0) as i64) + 2) * 8 {
            return false;
        }
        self.action(3, 0, tile, [edges, u32::from(up), 1, 0]).await;
        true
    }
    async fn level(&self, tile: u32) {
        if self.tile(tile)[0] == 3 {
            return;
        }
        let slope = self.tile(tile)[1];
        if slope == 0 {
            return;
        }
        if !self.terraform(tile, (!slope) & 15, true).await {
            self.terraform(tile, slope & 15, false).await;
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
    async fn extra_house(&self, town: u32, tile: u32, modes: u32) -> bool {
        if self.tile(tile)[5] == 0 {
            return false;
        }
        let mut count = 0;
        for dir in 0..4 {
            if matches!(self.tile(self.step(tile, dir))[0], 3 | 7) {
                count += 1;
            }
            if count >= 3 {
                return self.try_house(town, tile, modes).await;
            }
        }
        false
    }
    async fn road_continue(&self, town: u32, tile: u32, dir: u32) -> bool {
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
        self.action(1, town, next, [bit(reverse(dir)), rt, 0, 0])
            .await
            .0
            != 0
    }
    async fn bridge(&self, town: u32, tile: u32, dir: u32) -> bool {
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
        if length == 1 || !self.road_continue(town, end, dir).await {
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
            if self.action(4, 0, tile, [end, bridge, rt, 0]).await.0 != 0 {
                self.action(4, 0, tile, [end, bridge, rt, 1]).await;
                return true;
            }
        }
        false
    }
    async fn tunnel(&self, town: u32, tile: u32, dir: u32) -> bool {
        if self.tile(tile)[1] != incline(dir)
            || self.rb(self.step(tile, reverse(dir))) & bit(dir) == 0
        {
            return false;
        }
        let max = if self.road_continue(town, tile, dir).await {
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
        if length == 1 || !self.road_continue(town, end, dir).await {
            return false;
        }
        let rt = self.road_type();
        if self.action(5, 0, tile, [rt, 0, 0, 0]).await.0 != 0 {
            self.action(5, 0, tile, [rt, 1, 0, 0]).await;
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
    async fn in_tile(&self, town: u32, tile: &mut u32, cur: u32, mut dir: u32, modes: u32) -> u32 {
        let at = *tile;
        let layout = self.town(town)[4];
        let mut bits;
        if cur == 0 {
            if modes & 2 == 0 || (self.settings.get()[8] == 0 && self.tile(at)[0] == 1) {
                return 1;
            }
            if self.settings.get()[2] == 0 || self.chance(1, 6) {
                self.level(at).await;
            }
            if layout >= 2 {
                bits = self.grid(town, at, dir);
                if bits == 0 {
                    return 1;
                }
            } else {
                if !self.road_allowed(town, at, dir).await {
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
                if !self.road_allowed(town, self.step(at, dir), dir).await {
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
                    if self
                        .extra_house(town, self.step(house_tile, dir), modes)
                        .await
                    {
                        result = 0;
                    }
                }
                if layout >= 2 {
                    bits = self.grid(town, at, dir);
                    allow = bits & target == 0;
                } else {
                    bits = target;
                    allow = !self.road_allowed(town, house_tile, dir).await || self.chance(6, 10);
                }
            }
            allow &= self.roads_allow_house(house_tile);
            if allow {
                if self.tile(house_tile)[0] != 3 {
                    if self.chance(1, 6) {
                        self.level(house_tile).await;
                    }
                    if self.try_house(town, house_tile, modes).await {
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
        bits = self.leaf(6, at, bits, 0, 0) as u32;
        if bits == 0 {
            return 1;
        }
        if self.bridge(town, at, dir).await
            || self.tunnel(town, at, dir).await
            || self.road(town, at, bits, 1).await
        {
            return 0;
        }
        1
    }
    async fn at_road(&self, town: u32, mut tile: u32, modes: u32) -> bool {
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
            match self.in_tile(town, &mut tile, cur, dir, modes).await {
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
                    self.leaf(7, tile, town, 0, 0);
                }
            }
            iterations -= 1;
            if iterations < 0 {
                break;
            }
        }
        false
    }
    async fn grow(&self, town: u32, modes: u32) -> bool {
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
        let company = self.leaf(8, 0, self.settings.get()[11], 0, 0) as u32;
        let mut tile = self.town(town)[0];
        for (x, y) in OFFSETS {
            if self.rb(tile) != 0 {
                let result = self.at_road(town, tile, modes).await;
                self.leaf(8, 0, company, 0, 0);
                return result;
            }
            tile = self.offset(tile, x, y);
        }
        if modes & 2 != 0 {
            tile = self.town(town)[0];
            for (x, y) in OFFSETS {
                let t = self.tile(tile);
                if t[0] != 3 && t[1] == 0 && self.clear(tile, false).await {
                    let rt = self.road_type();
                    let r = self.services.random();
                    let a = r & 3;
                    let mut b = (r >> 8) & 3;
                    if a == b {
                        b ^= 2;
                    }
                    self.action(1, town, tile, [(1 << a) + (1 << b), rt, 2, 0])
                        .await;
                    self.leaf(8, 0, company, 0, 0);
                    return true;
                }
                tile = self.offset(tile, x, y);
            }
        }
        self.leaf(8, 0, company, 0, 0);
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
    async fn can_house(&self, tile: u32, noslope: bool) -> bool {
        let slope = self.tile(tile)[1];
        if (noslope && slope != 0) || slope & 16 != 0 {
            return false;
        }
        if !self.roads_allow_house(tile) || self.tile(tile)[6] != u32::MAX {
            return false;
        }
        self.clear(tile, false).await
    }
    async fn same_z(&self, tile: u32, z: u32, noslope: bool) -> bool {
        self.can_house(tile, noslope).await && self.tile(tile)[2] == z
    }
    async fn free_square(&self, mut tile: u32, z: u32, noslope: bool) -> bool {
        if !self.same_z(tile, z, noslope).await {
            return false;
        }
        for dir in 1..4 {
            tile = self.step(tile, dir);
            if !self.same_z(tile, z, noslope).await {
                return false;
            }
        }
        true
    }
    async fn check_house(
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
                if self.layout_house(town, test, modes, true)
                    && self.free_square(test, z, noslope).await
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
            if self.layout_house(town, test, modes, false) && self.same_z(test, z, noslope).await {
                return true;
            }
            test = self.step(*tile, reverse(dir));
            if self.layout_house(town, test, modes, false) && self.same_z(test, z, noslope).await {
                *tile = test;
                return true;
            }
            false
        } else {
            true
        }
    }
    async fn make_house_tile(
        &self,
        town: u32,
        tile: u32,
        counter: u32,
        stage: u32,
        house: u32,
        random: u32,
        protected: bool,
    ) {
        self.action(6, town, tile, [0, 0, 0, 0]).await;
        self.leaf(9, town, house, 0, 0);
        self.leaf(
            10,
            tile,
            town,
            house,
            counter | (stage << 8) | (random << 16) | (u32::from(protected) << 24),
        );
        if self.house(house)[2] & 32 != 0 {
            self.leaf(11, tile, 0, 0, 0);
        }
        self.leaf(12, tile, 0, 0, 0);
    }
    async fn build_house(
        &self,
        town: u32,
        tile: u32,
        house: u32,
        random: u32,
        complete: bool,
        protected: bool,
    ) {
        self.leaf(13, town, 1, 0, 0);
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
                self.leaf(14, town, h[9], 0, 0);
            } else {
                counter = (construction >> 2) & 3;
            }
        }
        let size = self.house(house)[2];
        let mut part = house;
        self.make_house_tile(town, tile, counter, stage, part, random, protected)
            .await;
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
            )
            .await;
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
            )
            .await;
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
            )
            .await;
        }
        self.leaf(15, town, tile, size, 0);
        self.leaf(16, town, 0, 0, 0);
        self.update_rate(town);
        let size = self.house(house)[2];
        self.action(8, town, tile, [0, 0, 0, 0]).await;
        if size & 24 != 0 {
            self.action(8, town, self.offset(tile, 0, 1), [0, 0, 0, 0])
                .await;
        }
        if size & 20 != 0 {
            self.action(8, town, self.offset(tile, 1, 0), [0, 0, 0, 0])
                .await;
        }
        if size & 16 != 0 {
            self.action(8, town, self.offset(tile, 1, 1), [0, 0, 0, 0])
                .await;
        }
    }
    async fn try_house(&self, town: u32, base: u32, modes: u32) -> bool {
        if !self.layout_house(town, base, modes, false) || !self.can_house(base, false).await {
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
        for house in 0..self.leaf(17, 0, 0, 0, 0) as u32 {
            let h = self.house(house);
            if h[0] & zones != zones || h[1] == 0 || h[4] != u16::MAX.into() {
                continue;
            }
            if self.leaf(18, town, house, h[8], 0) == u64::from(u16::MAX) {
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
            if !self
                .check_house(town, &mut tile, maxz, noslope, h[2], modes)
                .await
            {
                continue;
            }
            let random = u32::from(self.services.random() as u8);
            if h[10] != 0 && self.action(7, town, tile, [house, random, 0, 0]).await.0 == 0 {
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
            )
            .await;
            return true;
        }
        false
    }
    async fn tick(&self, town: u32) {
        if self.get(town, 4) & 1 == 0 {
            return;
        }
        let modes = 1 | if self.settings.get()[15] != 0 { 2 } else { 0 };
        let mut counter = i32::from(self.get(town, 0)) - 1;
        if counter < 0 {
            counter = i32::from(if self.grow(town, modes).await {
                self.get(town, 1)
            } else {
                self.get(town, 1).min((self.settings.get()[4] - 1) as u16)
            });
        }
        self.set(town, 0, counter as u16);
    }
    async fn expand(&self, town: u32, amount: u32, modes: u32) {
        if amount == 0 {
            let amount = self
                .services
                .random_range((self.town(town)[2] / 10).min(u32::from(u16::MAX)))
                + 3;
            self.leaf(13, town, amount, 0, 0);
            self.leaf(16, town, 0, 0, 0);
            for _ in 0..amount * 10 {
                self.grow(town, modes).await;
            }
            self.leaf(13, town, amount.wrapping_neg(), 0, 0);
        } else {
            for _ in 0..amount {
                for _ in 0..25 {
                    if self.grow(town, modes).await {
                        break;
                    }
                }
            }
        }
        self.leaf(16, town, 0, 0, 0);
        self.leaf(19, town, 0, 0, 0);
    }
    async fn run(&self, op: u32, town: u32, tile: u32, a: u32, b: u32, c: u32, d: u32) -> u32 {
        match op {
            0 => u32::from(self.grow(town, a).await),
            1 => u32::from(self.try_house(town, tile, a).await),
            2 => {
                self.build_house(town, tile, a, b, c != 0, d != 0).await;
                0
            }
            3 => {
                if self.settings.get()[6] == 0 {
                    let mut id = self.leaf(20, 0, 0, 0, 0) as u32;
                    while id != u32::MAX {
                        self.tick(id).await;
                        id = self.leaf(20, 0, id + 1, 0, 0) as u32;
                    }
                }
                0
            }
            4 => {
                self.update_rate(town);
                0
            }
            5 => {
                self.update_growth(town);
                0
            }
            6 => {
                self.expand(town, a, b).await;
                0
            }
            7 => {
                for field in [3, 2] {
                    let months = self.get(town, field);
                    if months != 0 {
                        self.set(town, field, months - 1);
                    }
                }
                0
            }
            8 => {
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
                0
            }
            9 => {
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
                0
            }
            10 => self.road_type(),
            11 => u32::from(self.tunnel(town, tile, a).await),
            _ => 0,
        }
    }
}
