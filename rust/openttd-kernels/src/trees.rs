/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Trees own policy, iteration, grove geometry, commands and the persisted tick byte.
//! Canonical tiles remain in C++; observations are copied and writes are leaf operations.
//! Shared leaves are called directly; water, `NewGRF` ambient and nested commands
//! execute only after returning to C++ because they can reenter or run arbitrary code.
#![allow(
    unsafe_code,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::cast_precision_loss,
    clippy::too_many_arguments,
    clippy::too_many_lines,
    clippy::similar_names,
    clippy::many_single_char_names
)]
use crate::services::{Services, chance16_i};
use std::ffi::c_void;

const INVALID: u32 = u32::MAX;
const CLEAR: u32 = 0;
const TREES: u32 = 4;
const WATER: u32 = 6;
const CACTUS: u32 = 27;
const INVALID_TREE: u32 = 255;

// Process-lifetime allocation, initialized without runtime construction. Access is
// serialized by the game thread; save/load dereferences its raw address only when
// no Rust call is active. In particular no Rust reference to it is ever formed.
static mut TREE_COUNTER: u8 = 0;

/// Stable address for unchanged DATE and legacy TTD/TTO serialization descriptors.
/// The game thread exclusively serializes access; callers never keep references
/// across a Rust invocation. `LoadCheck` deliberately does not access this byte.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_tree_counter() -> *mut u8 {
    &raw mut TREE_COUNTER
}

/// Reset the private persisted byte when initializing a game.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_trees_initialize() {
    // SAFETY: Game initialization is serialized with ticks and save/load.
    unsafe {
        TREE_COUNTER = 0;
    }
}

fn decrement(counter: u8, scale: u32) -> (u8, bool) {
    let next = u32::from(counter).wrapping_sub(scale) as u8;
    (next, counter <= next)
}

/// Scalar event; C++ dispatches shared services only after Rust returns.
#[repr(C)]
#[derive(Default, Debug, Clone, Copy)]
pub struct Action {
    pub kind: u32,
    pub tile: u32,
    pub a: u32,
    pub b: u32,
    pub cost: i64,
}
impl Action {
    fn new(kind: u32, tile: u32, a: u32, b: u32) -> Self {
        Self {
            kind,
            tile,
            a,
            b,
            cost: 0,
        }
    }
}

#[derive(Clone, Copy)]
struct Settings([u64; 12]);
impl Settings {
    fn ticks(self) -> u64 {
        self.0[0]
    }
    fn build(self) -> i64 {
        self.0[1] as i64
    }
    fn clear(self) -> i64 {
        self.0[2] as i64
    }
    fn size(self) -> u32 {
        self.0[3] as u32
    }
    fn sx(self) -> u32 {
        self.0[4] as u32
    }
    fn sy(self) -> u32 {
        self.0[5] as u32
    }
    fn snowline(self) -> i32 {
        self.0[6] as i32
    }
    fn climate(self) -> u32 {
        self.0[7] as u32
    }
    fn placer(self) -> u32 {
        self.0[8] as u32
    }
    fn extra(self) -> u32 {
        self.0[9] as u32
    }
    fn height(self) -> u32 {
        self.0[10] as u32
    }
    fn flag(self, bit: u32) -> bool {
        self.0[11] & (1 << bit) != 0
    }
    fn editor(self) -> bool {
        self.flag(0)
    }
    fn execute(self) -> bool {
        self.flag(3)
    }
    fn company(self) -> bool {
        self.flag(4)
    }
    fn scale(self, n: u32) -> u32 {
        n.wrapping_shl(self.size().ilog2() - 12).div_ceil(16)
    }
    fn random_tile(self, random: u32) -> u32 {
        random & (self.size() - 1)
    }
    fn offset(self, tile: u32, x: i32, y: i32) -> u32 {
        let nx = (tile % self.sx()).wrapping_add(x as u32);
        let ny = (tile / self.sx()).wrapping_add(y as u32);
        if (self.flag(2) && (nx == 0 || ny == 0)) || nx >= self.sx() - 1 || ny >= self.sy() - 1 {
            INVALID
        } else {
            nx + ny * self.sx()
        }
    }
}

#[derive(Clone, Copy)]
struct Tile([u32; 10]);
impl Tile {
    fn kind(self) -> u32 {
        self.0[0]
    }
    fn bridge(self) -> bool {
        self.0[1] != 0
    }
    fn zone(self) -> u32 {
        self.0[2]
    }
    fn height(self) -> i32 {
        self.0[3] as i32
    }
    fn ground(self) -> u32 {
        self.0[4]
    }
    fn density(self) -> u32 {
        self.0[5]
    }
    fn species(self) -> u32 {
        self.0[6]
    }
    fn count(self) -> u32 {
        self.0[7]
    }
    fn growth(self) -> u32 {
        self.0[8]
    }
    fn snow(self) -> bool {
        self.0[9] & 1 != 0
    }
    fn coast(self) -> bool {
        self.0[9] & 2 != 0
    }
    fn corner(self) -> bool {
        self.0[9] & 4 != 0
    }
    fn suitable(self, desert: bool) -> bool {
        match self.kind() {
            WATER => !self.bridge() && self.coast() && !self.corner(),
            CLEAR => {
                !self.bridge()
                    && self.ground() != 2
                    && self.ground() != 3
                    && (desert || self.ground() != 5)
            }
            _ => false,
        }
    }
    fn spread(self, s: Settings) -> bool {
        if s.climate() == 2 {
            match self.zone() {
                1 => false,
                2 => s.extra() == 1 || s.extra() == 2,
                _ => s.extra() == 2,
            }
        } else {
            s.extra() == 2
        }
    }
    fn random_type(self, s: Settings, seed: u32) -> u32 {
        let (base, count) = match s.climate() {
            0 => (0, 12),
            1 => (12, 8),
            2 => match self.zone() {
                0 => (28, 4),
                1 => return if seed > 12 { INVALID_TREE } else { CACTUS },
                _ => (20, 7),
            },
            _ => (32, 9),
        };
        u32::from((seed.wrapping_mul(count) / 256 + base) as u8)
    }
}

type ReadSettings = extern "C" fn(*mut c_void, *mut u64);
type Leaf = extern "C" fn(*mut c_void, u32, u32, u32, u32) -> u64;

#[derive(Clone, Copy, Default)]
struct Point {
    x: i32,
    y: i32,
}
fn triangle(x: i32, y: i32, a: Point, b: Point) -> bool {
    let s = a.x * y - a.y * x;
    let t = (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
    if (s < 0) != (t < 0) && s != 0 && t != 0 {
        return false;
    }
    let d = -b.x * (y - b.y) + b.y * (x - b.x);
    (d < 0) == (s + t <= 0)
}
fn in_grove(x: i32, y: i32, shape: &[Point; 16]) -> bool {
    (0..16).any(|i| triangle(x, y, shape[i], shape[(i + 1) % 16]))
}

// Only ordinary-play reentry boundaries survive. No RNG or loop continuation.
#[derive(Clone, Copy)]
enum Pending {
    Start(u32, u32, u32, u32, u32),
    Water(u32),
    Ambient(u32),
    LandscapeClear(u32),
    Done,
}
struct Engine {
    context: *mut c_void,
    current_settings: Settings,
    settings: ReadSettings,
    services: Services,
    leaf: Leaf,
    pending: Pending,
    cost: i64,
    message: u32,
    limit: i32,
    tree: u32,
    result: u32,
}
impl Engine {
    fn settings(&self) -> Settings {
        self.current_settings
    }
    fn refresh_settings(&mut self) {
        let mut values = [0; 12];
        (self.settings)(self.context, values.as_mut_ptr());
        self.current_settings = Settings(values);
    }
    fn tile(&self, tile: u32) -> Tile {
        Tile(self.services.tile(tile))
    }
    fn write(&self, op: u32, tile: u32, a: u32, b: u32, c: u32, d: u32) {
        self.services.write(op, tile, [a, b, c, d]);
    }
    fn leaf(&self, op: u32, tile: u32, a: u32, b: u32) -> u64 {
        (self.leaf)(self.context, op, tile, a, b)
    }
    fn dirty(&self, tile: u32) {
        self.write(5, tile, 0, 0, 0, 0);
    }
    fn ground(&self, tile: u32, ground: u32, density: u32) {
        self.write(1, tile, ground, density, 0, 0);
    }
    fn shape(&self) -> [Point; 16] {
        let divisor = (f64::from(i32::MAX) / std::f64::consts::PI * 2.0) as f32;
        let phases = std::array::from_fn::<_, 4, _>(|_| self.services.random() as f32 / divisor);
        let mut shape = [Point::default(); 16];
        let step = (std::f64::consts::PI * 2.0 / 16.0) as f32;
        let mut theta = 0.0_f32;
        for vertex in &mut shape {
            let mut deviation = 0.0_f32;
            for (i, phase) in phases.iter().enumerate() {
                deviation +=
                    (self.services.trig)(0, (theta + phase) * (i + 1) as f32) * (8 >> i) as f32;
            }
            let radius = 8.0_f32 + deviation / 2.0;
            vertex.x = ((self.services.trig)(1, theta) * radius) as i32;
            vertex.y = ((self.services.trig)(0, theta) * radius) as i32;
            theta += step;
        }
        shape
    }
    fn plant(&self, tile: u32, species: u32, count: u32, growth: u32) {
        let t = self.tile(tile);
        let mut density = 3;
        let ground = if t.kind() == WATER {
            self.leaf(3, tile, 0, 0);
            3
        } else {
            if t.ground() != 1 {
                density = t.density();
            }
            if t.snow() {
                if t.ground() == 1 { 4 } else { 2 }
            } else {
                match t.ground() {
                    0 => 0,
                    1 => 1,
                    _ => 2,
                }
            }
        };
        self.write(0, tile, species, count, growth, ground | (density << 8));
    }
    fn place(&self, tile: u32, r: u32, keep: bool) {
        let tree = self.tile(tile).random_type(self.settings(), r >> 24);
        if tree == INVALID_TREE {
            return;
        }
        self.plant(tile, tree, (r >> 22) & 3, ((r >> 16) & 7).min(6));
        self.dirty(tile);
        if !keep && !matches!(self.tile(tile).ground(), 2..=4) {
            self.ground(tile, (r >> 28) & 1, 3);
        }
    }
    // A cancellation status immediately propagates to C++; loops are not resumed.
    fn progress(&self) -> Result<(), ()> {
        if self.leaf(1, 0, 0, 0) == 0 {
            Ok(())
        } else {
            Err(())
        }
    }
    fn groves(&self, groups: u32) -> Result<(), ()> {
        for _ in 0..groups {
            let center = self.settings().random_tile(self.services.random());
            let shape = self.shape();
            for _ in 0..1000 {
                self.progress()?;
                let r = self.services.random();
                let x = (r & 31) as i32 - 16;
                let y = ((r >> 8) & 31) as i32 - 16;
                let tile = self.settings().offset(center, x, y);
                if tile != INVALID && self.tile(tile).suitable(true) && in_grove(x, y, &shape) {
                    self.place(tile, r, false);
                }
            }
        }
        Ok(())
    }
    fn same_height(&self, tile: u32, height: i32) {
        for _ in 0..1000 {
            let r = self.services.random();
            let x = (r & 31) as i32 - 16;
            let y = ((r >> 8) & 31) as i32 - 16;
            let current = self.settings().offset(tile, x, y);
            if current == INVALID || x.abs() + y.abs() > 16 {
                continue;
            }
            let t = self.tile(current);
            if !t.suitable(true) || (t.height() - height).abs() > 2 {
                continue;
            }
            self.place(current, r, false);
            break;
        }
    }
    fn scatter(&self) -> Result<(), ()> {
        let s = self.settings();
        let divisor = if s.editor() { 5 } else { 1 };
        for _ in 0..s.scale(1000) / divisor {
            let r = self.services.random();
            let tile = s.random_tile(r);
            self.progress()?;
            if !self.tile(tile).suitable(true) {
                continue;
            }
            self.place(tile, r, false);
            if s.placer() != 2 {
                continue;
            }
            let height = self.tile(tile).height();
            let mut n = self.tile(tile).height() * 2;
            if s.climate() == 1 && height > s.snowline() {
                n *= 3;
            }
            if s.height() > 15 {
                n = n * 15 / s.height() as i32;
            }
            for _ in 0..n {
                self.same_height(tile, height);
            }
        }
        if s.climate() == 2 {
            for _ in 0..s.scale(15000) / divisor {
                let r = self.services.random();
                let tile = s.random_tile(r);
                self.progress()?;
                if self.tile(tile).zone() == 2 && self.tile(tile).suitable(false) {
                    self.place(tile, r, false);
                }
            }
        }
        Ok(())
    }
    fn generate(&self) -> Result<(), ()> {
        let s = self.settings();
        if s.placer() == 0 {
            return Ok(());
        }
        let runs = if s.placer() == 1 {
            if s.climate() == 1 { 15 } else { 6 }
        } else if s.climate() == 1 {
            4
        } else {
            2
        };
        let mut total = s.scale(1000);
        if s.climate() == 2 {
            total = total.wrapping_add(s.scale(15000));
        }
        total = total.wrapping_mul(runs);
        let groups = if s.climate() == 3 {
            0
        } else {
            s.scale((self.services.random() & 31) + 25)
        };
        if self.leaf(2, 0, total.wrapping_add(groups.wrapping_mul(1000)), 0) != 0 {
            return Err(());
        }
        if groups != 0 {
            self.groves(groups)?;
        }
        for _ in 0..runs {
            self.scatter()?;
        }
        Ok(())
    }
    fn climate(&self, tile: u32) {
        let t = self.tile(tile);
        let s = self.settings();
        if s.climate() == 2 {
            match t.zone() {
                1 if t.ground() != 2 => {
                    self.ground(tile, 2, 3);
                    self.dirty(tile);
                }
                2 => {
                    let r = self.services.random();
                    if chance16_i(1, 200, r) && s.flag(1) {
                        self.leaf(6, tile, (r >> 16) & 3, 0);
                    }
                }
                _ => (),
            }
        } else if s.climate() == 1 {
            let k = t.height() - s.snowline() + 1;
            if k < 0 {
                match t.ground() {
                    2 => self.ground(tile, 0, 3),
                    4 => self.ground(tile, 1, 3),
                    _ => return,
                }
            } else {
                let density = (k as u32).min(3);
                if t.ground() != 2 && t.ground() != 4 {
                    self.ground(tile, if t.ground() == 1 { 4 } else { 2 }, density);
                } else if t.density() != density {
                    self.ground(tile, t.ground(), density);
                } else {
                    if density == 3 {
                        let r = self.services.random();
                        if chance16_i(1, 200, r) && s.flag(1) {
                            self.leaf(6, tile, 4 + (r >> 31), 0);
                        }
                    }
                    return;
                }
            }
            self.dirty(tile);
        }
    }
    fn growth(&self, mut tile: u32) {
        let s = self.settings();
        let cycle =
            (11 * (tile % s.sx()) + 9 * (tile / s.sx())).wrapping_add((s.ticks() >> 8) as u32);
        let t = self.tile(tile);
        if cycle & 7 == 7 && t.ground() == 0 && t.density() < 3 {
            self.ground(tile, 0, t.density() + 1);
            self.dirty(tile);
        }
        if s.extra() == 3 || cycle % 16 != 15 {
            return;
        }
        let t = self.tile(tile);
        match t.growth() {
            3 => {
                if s.climate() == 2 && t.species() != CACTUS && t.zone() == 1 {
                    self.write(3, tile, 1, 0, 0, 0);
                } else {
                    match self.services.random() & 7 {
                        0 => self.write(3, tile, 1, 0, 0, 0),
                        1 if self.tile(tile).count() < 4 && self.tile(tile).spread(s) => {
                            self.write(2, tile, 1, 0, 0, 0);
                            self.write(4, tile, 0, 0, 0, 0);
                        }
                        1 | 2 => {
                            if self.tile(tile).spread(s) {
                                let tree = self.tile(tile).species();
                                let (x, y) = [
                                    (-1, -1),
                                    (-1, 0),
                                    (-1, 1),
                                    (0, 1),
                                    (1, 1),
                                    (1, 0),
                                    (1, -1),
                                    (0, -1),
                                ][(self.services.random() % 8) as usize];
                                tile = tile.wrapping_add((x + y * s.sx() as i32) as u32);
                                let neighbor = self.tile(tile);
                                if !neighbor.suitable(false)
                                    || (neighbor.kind() == CLEAR
                                        && neighbor.ground() == 0
                                        && !neighbor.snow()
                                        && neighbor.density() != 3)
                                {
                                    return;
                                }
                                self.plant(tile, tree, 0, 0);
                            }
                        }
                        _ => return,
                    }
                }
            }
            6 => {
                if !t.spread(s) {
                    self.write(4, tile, 0, 0, 0, 0);
                } else if t.count() > 1 {
                    self.write(2, tile, u32::MAX, 0, 0, 0);
                    self.write(4, tile, 3, 0, 0, 0);
                } else {
                    match t.ground() {
                        3 => self.write(7, tile, 0, 0, 0, 0),
                        0 => self.write(6, tile, 0, t.density(), 0, 0),
                        1 => self.write(6, tile, 1, 3, 0, 0),
                        4 => {
                            self.write(6, tile, 1, 3, 0, 0);
                            self.write(8, tile, t.density(), 0, 0, 0);
                        }
                        _ if s.climate() == 2 => self.write(6, tile, 5, t.density(), 0, 0),
                        _ => {
                            self.write(6, tile, 0, 3, 0, 0);
                            self.write(8, tile, t.density(), 0, 0, 0);
                        }
                    }
                }
            }
            _ => self.write(3, tile, 1, 0, 0, 0),
        }
        self.dirty(tile);
    }
    fn random_tree(&self, rainforest: bool) {
        let r = self.services.random();
        let s = self.settings();
        let tile = s.random_tile(r);
        let t = self.tile(tile);
        if (rainforest && t.zone() != 2) || !t.suitable(false) {
            return;
        }
        let tree = t.random_type(s, r >> 24);
        if tree != INVALID_TREE {
            self.plant(tile, tree, 0, 0);
        }
    }
    fn tick(&self) {
        let s = self.settings();
        if s.extra() == 0 || s.extra() == 3 {
            return;
        }
        let skip = s.scale(16);
        if skip < 16 && s.ticks() & u64::from(16 / skip - 1) != 0 {
            return;
        }
        if s.climate() == 2 {
            for _ in 0..s.scale(1) {
                self.random_tree(true);
            }
        }
        // SAFETY: Game thread only, no reference to this byte is formed.
        let wrapped = unsafe {
            let (next, wrapped) = decrement(TREE_COUNTER, s.scale(1));
            TREE_COUNTER = next;
            wrapped
        };
        if wrapped && s.extra() != 1 {
            self.random_tree(false);
        }
    }
    fn command_finish(&mut self, tile: u32) {
        let s = self.settings();
        if !s.editor() && s.company() {
            self.leaf(8, tile, 1, 0);
        }
        if s.execute() {
            let mut tree = self.tree;
            if tree == INVALID_TREE {
                tree = self.tile(tile).random_type(s, self.services.random() >> 24);
                if tree == INVALID_TREE {
                    tree = CACTUS;
                }
            }
            self.plant(tile, tree, 0, if s.editor() { 3 } else { 0 });
            self.dirty(tile);
            self.leaf(10, tile, 0, 0);
            if s.editor() && (20..27).contains(&tree) {
                self.write(9, tile, 2, 0, 0, 0);
            }
        }
        self.cost = self.cost.saturating_add(s.build());
    }
    fn command_next(&mut self, mut advance: bool) -> Option<Action> {
        loop {
            let tile = self.leaf(11, 0, u32::from(advance), 0) as u32;
            advance = true;
            if tile == INVALID {
                return None;
            }
            let t = self.tile(tile);
            if t.kind() == TREES {
                if t.count() == 4 {
                    self.message = 1;
                    continue;
                }
                self.limit -= 1;
                if self.limit < 1 {
                    self.message = 2;
                    if self.limit < 0 {
                        return None;
                    }
                    continue;
                }
                if self.settings().execute() {
                    self.write(2, tile, 1, 0, 0, 0);
                    self.dirty(tile);
                    self.leaf(10, tile, 0, 0);
                }
                self.cost = self
                    .cost
                    .saturating_add(self.settings().build().saturating_mul(2));
            } else if t.kind() == WATER || t.kind() == CLEAR {
                if t.kind() == WATER && (!t.coast() || t.corner()) {
                    self.message = 3;
                    continue;
                }
                if t.bridge() {
                    self.message = 4;
                    continue;
                }
                let s = self.settings();
                let tree = self.tree;
                if s.climate() == 2
                    && tree != INVALID_TREE
                    && ((tree == CACTUS && t.zone() != 1)
                        || ((20..27).contains(&tree) && t.zone() != 2 && !s.editor())
                        || ((28..32).contains(&tree) && t.zone() != 0))
                {
                    self.message = 5;
                    continue;
                }
                self.limit -= 1;
                if self.limit < 1 {
                    self.message = 2;
                    if self.limit < 0 {
                        return None;
                    }
                    continue;
                }
                if t.kind() == CLEAR && matches!(t.ground(), 2 | 3) {
                    self.pending = Pending::LandscapeClear(tile);
                    return Some(Action::new(10, tile, 0, 0));
                }
                self.command_finish(tile);
            } else {
                self.message = 4;
            }
        }
    }
    fn advance(&mut self, response: u64, nested_cost: i64) -> Action {
        self.refresh_settings();
        let pending = std::mem::replace(&mut self.pending, Pending::Done);
        match pending {
            Pending::Start(kind, tile, a, b, c) => match kind {
                0 | 1 => {
                    let result = if kind == 0 {
                        self.generate()
                    } else {
                        self.scatter()
                    };
                    if result.is_err() {
                        return Action::new(12, 0, 0, 0);
                    }
                }
                2 => self.place(tile, a, b != 0),
                3 => self.plant(tile, a, b, c),
                5 => {
                    if self.tile(tile).ground() == 3 {
                        self.pending = Pending::Water(tile);
                        return Action::new(4, tile, 0, 0);
                    }
                    self.climate(tile);
                    self.pending = Pending::Ambient(tile);
                    return Action::new(5, tile, 0, 0);
                }
                6 => self.tick(),
                8 => {
                    let s = self.settings();
                    let climate = s.climate() as usize;
                    if a >= s.size()
                        || (b != INVALID_TREE
                            && b.wrapping_sub([0, 12, 20, 32][climate]) >= [12, 8, 12, 9][climate])
                    {
                        self.result = 1;
                    } else {
                        self.tree = b;
                        self.limit = self.leaf(9, tile, a, c) as i32;
                        if let Some(action) = self.command_next(false) {
                            return action;
                        }
                    }
                }
                9 => {
                    if self.settings().company() {
                        self.leaf(8, tile, 0, 0);
                    }
                    let t = self.tile(tile);
                    let num = t.count()
                        * if (20..27).contains(&t.species()) {
                            4
                        } else {
                            1
                        };
                    if self.settings().execute() {
                        self.leaf(7, tile, 0, 0);
                    }
                    self.cost = self.settings().clear().saturating_mul(i64::from(num));
                }
                _ => self.dirty(tile),
            },
            Pending::Water(tile) => {
                self.pending = Pending::Ambient(tile);
                return Action::new(5, tile, 0, 0);
            }
            Pending::Ambient(tile) => self.growth(tile),
            Pending::LandscapeClear(tile) => {
                if response != 0 {
                    self.result = 3;
                } else {
                    self.cost = self.cost.saturating_add(nested_cost);
                    self.command_finish(tile);
                    if let Some(action) = self.command_next(true) {
                        return action;
                    }
                }
            }
            Pending::Done => (),
        }
        Action {
            a: self.result,
            b: self.message,
            cost: self.cost,
            ..Action::default()
        }
    }
}

/// Create an invocation. Shared and component leaves are synchronous/noexcept;
/// only returned water/NewGRF/landscape-clear/progress-abort actions may throw or reenter.
/// # Safety
/// Context/callbacks outlive destruction; table points to a readable Services.
/// Calls are game-thread serialized, output lengths follow the header. No C++
/// exception crosses a Rust call; panic/OOM abort. No world reference is retained.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_trees_create(
    kind: u32,
    tile: u32,
    a: u32,
    b: u32,
    c: u32,
    context: *mut c_void,
    settings: ReadSettings,
    services: *const Services,
    leaf: Leaf,
) -> *mut c_void {
    // SAFETY: Caller supplies the immutable readable table for this call.
    let services = unsafe { *services };
    Box::into_raw(Box::new(Engine {
        context,
        current_settings: Settings([0; 12]),
        settings,
        services,
        leaf,
        pending: Pending::Start(kind, tile, a, b, c),
        cost: 0,
        message: 0,
        limit: 0,
        tree: INVALID_TREE,
        result: 0,
    }))
    .cast()
}
/// Run straight-line work until an ordinary-play reentry boundary.
/// # Safety
/// Owner is live and exclusive; response/cost match the returned action protocol.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_trees_advance(
    owner: *mut c_void,
    response: u64,
    cost: i64,
) -> Action {
    // SAFETY: The caller owns this live invocation exclusively until return.
    unsafe { &mut *owner.cast::<Engine>() }.advance(response, cost)
}
/// Destroy once, including on C++ exceptions; no world/context access.
/// # Safety
/// Owner is live, created here, and no advance call is active.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_trees_destroy(owner: *mut c_void) {
    // SAFETY: Caller returns exclusive allocation ownership exactly once.
    drop(unsafe { Box::from_raw(owner.cast::<Engine>()) });
}
/// Suitability policy used by the editor brush.
/// # Safety
/// Values supplies ten copied observation words readable for this call only.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_trees_suitable(values: *const u32, desert: u8) -> u8 {
    let mut tile = [0; 10];
    // SAFETY: Caller supplies the documented readable array.
    unsafe {
        std::ptr::copy_nonoverlapping(values, tile.as_mut_ptr(), 10);
    }
    u8::from(Tile(tile).suitable(desert != 0))
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn counter_full_byte_and_scale_domain() {
        for scale in 1..=256 {
            for old in 0..=u8::MAX {
                let (next, wrapped) = decrement(old, scale);
                assert_eq!(next, (u32::from(old).wrapping_sub(scale) & 255) as u8);
                assert_eq!(wrapped, u32::from(old) < scale);
            }
        }
    }
    struct World {
        settings: [u64; 12],
        tile: [u32; 10],
        writes: Vec<(u32, u32, u32)>,
        iterator: u32,
        length: u32,
        limit: i32,
        cancel: bool,
        draws: u32,
        progress: u32,
    }
    impl Default for World {
        fn default() -> Self {
            Self {
                settings: [0, 15, 35, 4096, 64, 64, 10, 0, 0, 2, 15, 8],
                tile: [CLEAR, 0, 0, 0, 0, 3, 0, 0, 0, 0],
                writes: Vec::new(),
                iterator: 0,
                length: 1,
                limit: i32::MAX,
                cancel: false,
                draws: 0,
                progress: 0,
            }
        }
    }
    extern "C" fn settings(context: *mut c_void, output: *mut u64) {
        // SAFETY: Tests own the exclusive live context and output lengths follow the ABI.
        unsafe {
            let w = &*context.cast::<World>();
            std::ptr::copy_nonoverlapping(w.settings.as_ptr(), output, 12);
        }
    }
    extern "C" fn observe(context: *mut c_void, _: u32, output: *mut u32) {
        // SAFETY: Same callback contract as settings.
        unsafe {
            let w = &*context.cast::<World>();
            std::ptr::copy_nonoverlapping(w.tile.as_ptr(), output, 10);
        }
    }
    extern "C" fn write(context: *mut c_void, op: u32, _: u32, a: u32, b: u32, c: u32, d: u32) {
        // SAFETY: No context reference is retained across this callback.
        let w = unsafe { &mut *context.cast::<World>() };
        w.writes.push((op, a, b));
        match op {
            0 => {
                w.tile[0] = TREES;
                w.tile[4] = d & 255;
                w.tile[5] = d >> 8;
                w.tile[6] = a;
                w.tile[7] = b + 1;
                w.tile[8] = c;
            }
            1 => {
                w.tile[4] = a;
                w.tile[5] = b;
            }
            4 => w.tile[8] = a,
            9 => w.tile[2] = a,
            _ => (),
        }
    }
    extern "C" fn trig(_: u32, value: f32) -> f32 {
        value
    }
    extern "C" fn random(context: *mut c_void) -> u32 {
        // SAFETY: Fixture context remains live and exclusively accessed per call.
        unsafe {
            (*context.cast::<World>()).draws += 1;
        }
        0
    }
    extern "C" fn leaf(context: *mut c_void, op: u32, tile: u32, a: u32, b: u32) -> u64 {
        // SAFETY: Test owns the exclusive live context for each callback.
        let w = unsafe { &mut *context.cast::<World>() };
        match op {
            1 | 2 => {
                w.progress += 1;
                u64::from(w.cancel)
            }
            9 => {
                w.writes.push((op, tile, a));
                w.writes.push((99, b, 0));
                w.limit as u64
            }
            11 => {
                w.iterator += a;
                u64::from(if w.iterator < w.length { 1 } else { INVALID })
            }
            10 => {
                w.writes.push((op, a, b));
                0
            }
            _ => 0,
        }
    }
    extern "C" fn industry(_: i32, _: i32, _: *mut u32) -> u32 {
        0
    }
    fn engine(w: &mut World, kind: u32, a: u32, b: u32, c: u32) -> Engine {
        let context = std::ptr::from_mut(w).cast();
        Engine {
            context,
            current_settings: Settings([0; 12]),
            settings,
            services: Services {
                context,
                random,
                observe_tile: observe,
                write_tile: write,
                trig,
                industry,
            },
            leaf,
            pending: Pending::Start(kind, 2, a, b, c),
            cost: 0,
            message: 0,
            limit: 0,
            tree: INVALID_TREE,
            result: 0,
        }
    }
    // Script APIs cannot request explicit tree types, diagonal rectangles, or editor
    // planting. These protocol tests cover those source branches and full Money bounds;
    // end-to-end simulation/command evidence remains the unchanged reference harness.
    // Headless semantic scenarios cannot click Abort during map generation.
    #[test]
    fn cancellation_preserves_draw_order_and_stops_before_map_effects() {
        let mut w = World {
            cancel: true,
            ..World::default()
        };
        let mut e = engine(&mut w, 1, 0, 0, 0);
        assert_eq!(e.advance(0, 0).kind, 12);
        assert_eq!((w.draws, w.progress), (1, 1)); // Scatter draws before progress.
        assert_eq!(w.writes, [] as [(u32, u32, u32); 0]);
        w.draws = 0;
        w.progress = 0;
        let mut e = engine(&mut w, 0, 0, 0, 0);
        e.refresh_settings();
        assert!(e.groves(1).is_err());
        assert_eq!((w.draws, w.progress), (5, 1)); // Center/phases precede progress, attempt does not.
        assert_eq!(w.writes, [] as [(u32, u32, u32); 0]);
        w.draws = 0;
        w.progress = 0;
        w.settings[8] = 1;
        let mut e = engine(&mut w, 0, 0, 0, 0);
        assert_eq!(e.advance(0, 0).kind, 12);
        assert_eq!((w.draws, w.progress), (1, 1)); // Group count precedes total-progress cancellation.
        assert_eq!(w.writes, [] as [(u32, u32, u32); 0]);
    }
    #[test]
    fn explicit_editor_planting_and_diagonal_iterator_request() {
        let mut w = World::default();
        w.settings[7] = 2;
        w.settings[11] = 9; // Editor, execute.
        let mut e = engine(&mut w, 8, 1, 20, 1);
        let result = e.advance(0, 0);
        assert_eq!(w.writes[..2], [(9, 2, 1), (99, 1, 0)]);
        assert_eq!((result.kind, result.a, result.cost), (0, 0, 15));
        assert_eq!((w.tile[6], w.tile[8], w.tile[2]), (20, 3, 2));
        assert_eq!(
            w.writes.iter().map(|x| x.0).collect::<Vec<_>>(),
            [9, 99, 0, 5, 10, 9]
        );
    }
    #[test]
    fn explicit_tree_type_checks_and_wrong_tropical_zone() {
        let mut w = World::default();
        let mut e = engine(&mut w, 8, 1, 20, 0);
        assert_eq!(e.advance(0, 0).a, 1); // Rainforest is outside temperate tree range.
        w.settings[7] = 2;
        let mut e = engine(&mut w, 8, 1, 20, 0);
        let result = e.advance(0, 0);
        assert_eq!((result.cost, result.b), (0, 5));
        assert_eq!(w.writes.len(), 2);
    }
    #[test]
    fn exhausted_limit_checks_zero_and_negative_separately() {
        let mut w = World {
            length: 5,
            limit: 1,
            ..World::default()
        };
        let mut e = engine(&mut w, 8, 1, 0, 0);
        let result = e.advance(0, 0);
        assert_eq!((result.cost, result.b, e.limit, w.iterator), (0, 2, -1, 1));
        assert_eq!(w.writes.len(), 2); // Iterator initialization only.
    }
    #[test]
    fn money_extremes_keep_original_saturation_order() {
        for price in [i64::MIN, i64::MAX] {
            let mut w = World {
                length: 3,
                ..World::default()
            };
            w.settings[1] = price as u64;
            w.settings[11] = 0; // Test mode; no tile changes.
            w.tile = [TREES, 0, 0, 0, 0, 3, 0, 1, 0, 0];
            let mut e = engine(&mut w, 8, 1, 0, 0);
            assert_eq!(e.advance(0, 0).cost, price);
            w.settings[2] = price as u64;
            w.tile[6] = 20;
            w.tile[7] = 4;
            let mut e = engine(&mut w, 9, 0, 0, 0);
            assert_eq!(e.advance(0, 0).cost, price);
        }
    }
    #[test]
    fn ambient_reentry_uses_fresh_growth_without_normalizing_bitpattern_seven() {
        let mut w = World::default();
        w.settings[0] = 2304; // Tile1 cycle15.
        w.settings[9] = 0;
        w.tile = [TREES, 0, 0, 0, 0, 3, 0, 1, 7, 0];
        let mut e = engine(&mut w, 5, 0, 0, 0);
        assert_eq!(e.advance(0, 0).kind, 5);
        assert_eq!(e.advance(0, 0).kind, 0);
        assert_eq!(w.writes.iter().map(|x| x.0).collect::<Vec<_>>(), [3, 5]);
        w.writes.clear();
        let mut e = engine(&mut w, 5, 0, 0, 0);
        assert_eq!(e.advance(0, 0).kind, 5);
        w.tile[8] = 6; // Ambient/NewGRF changed the canonical map after Rust returned.
        assert_eq!(e.advance(0, 0).kind, 0);
        assert_eq!(w.writes.iter().map(|x| x.0).collect::<Vec<_>>(), [4, 5]);
        assert_eq!(w.tile[8], 0);
    }
}
