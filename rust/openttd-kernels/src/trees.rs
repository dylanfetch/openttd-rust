/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Trees own policy, iteration, grove geometry, commands and the persisted tick byte.
//! Canonical tiles remain in C++; observations are copied and writes are leaf operations.
//! Potentially throwing/reentrant services execute only between `advance` calls.
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
type Observe = extern "C" fn(*mut c_void, u32, *mut u32);
type Write = extern "C" fn(*mut c_void, u32, u32, u32, u32, u32, u32) -> u64;
type Trig = extern "C" fn(u32, f32) -> f32;

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

// Stack entries are resumable calls, never pointers/references to canonical state.
enum Task {
    Generate,
    Runs(u32),
    Groves(u32),
    GroveAttempts(u32, u32, Box<[Point; 16]>),
    GroveAfterProgress(u32, u32, Box<[Point; 16]>),
    ScatterInit,
    ScatterExtra(u32),
    Scatter(u32, bool, u32),
    ScatterAfterProgress(u32, u32, bool, u32),
    ScatterAfterPlace(u32, u32),
    HeightAttempts(u32, i32, u32),
    Place(u32, u32, bool),
    AfterPlace(u32, u32, bool),
    Plant(u32, u32, u32, u32),
    MakeTree(u32, u32, u32, u32, u32, u32),
    Dirty(u32),
    Loop(u32),
    Ambient(u32),
    Growth(u32),
    Tick,
    RainforestTicks(u32),
    CounterTick,
    RandomTree(bool),
    CommandInit(u32, u32, u32, bool),
    CommandBegin,
    CommandNext(bool),
    CommandAfterClear(u32, u32),
    CommandRating(u32, u32),
    CommandPlant(u32, u32),
    CommandFinishTile(u32, u32),
    Clear(u32),
    ClearCount(u32),
    ClearResult(u32),
}

struct Engine {
    context: *mut c_void,
    current_settings: Settings,
    settings: ReadSettings,
    observe: Observe,
    write: Write,
    random: extern "C" fn() -> u32,
    trig: Trig,
    tasks: Vec<Task>,
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
        let mut values = [0; 10];
        (self.observe)(self.context, tile, values.as_mut_ptr());
        Tile(values)
    }
    fn write(&self, op: u32, tile: u32, a: u32, b: u32, c: u32, d: u32) -> u64 {
        (self.write)(self.context, op, tile, a, b, c, d)
    }
    fn dirty(&self, tile: u32) {
        self.write(5, tile, 0, 0, 0, 0);
    }
    fn ground(&self, tile: u32, ground: u32, density: u32) {
        self.write(1, tile, ground, density, 0, 0);
    }
    fn shape(&self) -> [Point; 16] {
        let divisor = (f64::from(i32::MAX) / std::f64::consts::PI * 2.0) as f32;
        let phases = std::array::from_fn::<_, 4, _>(|_| (self.random)() as f32 / divisor);
        let mut shape = [Point::default(); 16];
        let step = (std::f64::consts::PI * 2.0 / 16.0) as f32;
        let mut theta = 0.0_f32;
        for vertex in &mut shape {
            let mut deviation = 0.0_f32;
            for (i, phase) in phases.iter().enumerate() {
                deviation += (self.trig)(0, (theta + phase) * (i + 1) as f32) * (8 >> i) as f32;
            }
            let radius = 8.0_f32 + deviation / 2.0;
            vertex.x = ((self.trig)(1, theta) * radius) as i32;
            vertex.y = ((self.trig)(0, theta) * radius) as i32;
            theta += step;
        }
        shape
    }
    fn advance(&mut self, response: u64, nested_cost: i64) -> Action {
        // Leaf callbacks cannot change settings or reenter; copy once per advance.
        // External actions have returned before the next advance refreshes them.
        self.refresh_settings();
        while let Some(task) = self.tasks.pop() {
            match task {
                Task::Generate => {
                    let s = self.settings();
                    if s.placer() == 0 {
                        continue;
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
                        s.scale(((self.random)() & 31) + 25)
                    };
                    total = total.wrapping_add(groups.wrapping_mul(1000));
                    self.tasks.push(Task::Runs(runs));
                    if groups != 0 {
                        self.tasks.push(Task::Groves(groups));
                    }
                    return Action::new(2, 0, total, 0);
                }
                Task::Runs(n) => {
                    if n != 0 {
                        self.tasks.push(Task::Runs(n - 1));
                        self.tasks.push(Task::ScatterInit);
                    }
                }
                Task::Groves(n) => {
                    let s = self.settings();
                    let tile = s.random_tile((self.random)());
                    let shape = Box::new(self.shape());
                    if n != 1 {
                        self.tasks.push(Task::Groves(n.wrapping_sub(1)));
                    }
                    self.tasks.push(Task::GroveAttempts(1000, tile, shape));
                }
                Task::GroveAttempts(n, center, shape) => {
                    if n == 0 {
                        continue;
                    }
                    // Progress precedes the draw; preserve shape without recomputation.
                    self.tasks.push(Task::GroveAfterProgress(center, n, shape));
                    return Action::new(1, 0, 0, 0);
                }
                Task::GroveAfterProgress(center, remaining, shape) => {
                    let r = (self.random)();
                    let x = (r & 31) as i32 - 16;
                    let y = ((r >> 8) & 31) as i32 - 16;
                    let tile = self.settings().offset(center, x, y);
                    let valid =
                        tile != INVALID && self.tile(tile).suitable(true) && in_grove(x, y, &shape);
                    self.tasks
                        .push(Task::GroveAttempts(remaining - 1, center, shape));
                    if valid {
                        self.tasks.push(Task::Place(tile, r, false));
                    }
                }
                Task::ScatterInit => {
                    let s = self.settings();
                    let n = s.scale(1000) / if s.editor() { 5 } else { 1 };
                    self.tasks.push(Task::ScatterExtra(s.height()));
                    self.tasks.push(Task::Scatter(n, false, s.height()));
                }
                Task::ScatterExtra(height) => {
                    let s = self.settings();
                    if s.climate() == 2 {
                        let n = s.scale(15000) / if s.editor() { 5 } else { 1 };
                        self.tasks.push(Task::Scatter(n, true, height));
                    }
                }
                Task::Scatter(n, rainforest, height) => {
                    if n == 0 {
                        continue;
                    }
                    let r = (self.random)();
                    let tile = self.settings().random_tile(r);
                    self.tasks.push(Task::Scatter(n - 1, rainforest, height));
                    self.tasks
                        .push(Task::ScatterAfterProgress(tile, r, rainforest, height));
                    return Action::new(1, 0, 0, 0);
                }
                Task::ScatterAfterProgress(tile, r, rainforest, height) => {
                    let observation = self.tile(tile);
                    if rainforest {
                        if observation.zone() == 2 && observation.suitable(false) {
                            self.tasks.push(Task::Place(tile, r, false));
                        }
                    } else if observation.suitable(true) {
                        self.tasks.push(Task::ScatterAfterPlace(tile, height));
                        self.tasks.push(Task::Place(tile, r, false));
                    }
                }
                Task::ScatterAfterPlace(tile, height) => {
                    let s = self.settings();
                    if s.placer() != 2 {
                        continue;
                    }
                    let ht = self.tile(tile).height();
                    let mut n = self.tile(tile).height() * 2;
                    if s.climate() == 1 && ht > s.snowline() {
                        n *= 3;
                    }
                    if height > 15 {
                        n = n * 15 / height as i32;
                    }
                    self.tasks.push(Task::HeightAttempts(tile, ht, n as u32));
                }
                Task::HeightAttempts(tile, height, n) => {
                    if n == 0 {
                        continue;
                    }
                    self.tasks.push(Task::HeightAttempts(tile, height, n - 1));
                    for _ in 0..1000 {
                        let r = (self.random)();
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
                        self.tasks.push(Task::Place(current, r, false));
                        break;
                    }
                }
                Task::Place(tile, r, keep) => {
                    let species = self
                        .tile(tile)
                        .random_type(self.settings(), (r >> 24) & 255);
                    if species == INVALID_TREE {
                        continue;
                    }
                    self.tasks.push(Task::AfterPlace(tile, r, keep));
                    self.tasks.push(Task::Plant(
                        tile,
                        species,
                        (r >> 22) & 3,
                        ((r >> 16) & 7).min(6),
                    ));
                }
                Task::AfterPlace(tile, r, keep) => {
                    self.dirty(tile);
                    if !keep && !matches!(self.tile(tile).ground(), 2..=4) {
                        self.ground(tile, (r >> 28) & 1, 3);
                    }
                }
                Task::Plant(tile, species, count, growth) => {
                    let t = self.tile(tile);
                    let mut density = 3;
                    let ground = if t.kind() == WATER {
                        self.tasks
                            .push(Task::MakeTree(tile, species, count, growth, 3, density));
                        return Action::new(3, tile, 0, 0);
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
                Task::MakeTree(tile, species, count, growth, ground, density) => {
                    self.write(0, tile, species, count, growth, ground | (density << 8));
                }
                Task::Dirty(tile) => self.dirty(tile),
                Task::Loop(tile) => {
                    self.tasks.push(Task::Ambient(tile));
                    let t = self.tile(tile);
                    if t.ground() == 3 {
                        return Action::new(4, tile, 0, 0);
                    }
                    let s = self.settings();
                    if s.climate() == 2 {
                        match t.zone() {
                            1 => {
                                if t.ground() != 2 {
                                    self.ground(tile, 2, 3);
                                    self.dirty(tile);
                                }
                            }
                            2 => {
                                let r = (self.random)();
                                if (((r & 65535) * 200 + 100) >> 16) < 1 && self.settings().flag(1)
                                {
                                    return Action::new(6, tile, (r >> 16) & 3, 0);
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
                                _ => continue,
                            }
                        } else {
                            let density = (k as u32).min(3);
                            if t.ground() != 2 && t.ground() != 4 {
                                self.ground(tile, if t.ground() == 1 { 4 } else { 2 }, density);
                            } else if t.density() != density {
                                self.ground(tile, t.ground(), density);
                            } else {
                                if density == 3 {
                                    let r = (self.random)();
                                    if (((r & 65535) * 200 + 100) >> 16) < 1
                                        && self.settings().flag(1)
                                    {
                                        return Action::new(6, tile, 4 + (r >> 31), 0);
                                    }
                                }
                                continue;
                            }
                        }
                        self.dirty(tile);
                    }
                }
                Task::Ambient(tile) => {
                    self.tasks.push(Task::Growth(tile));
                    return Action::new(5, tile, 0, 0);
                }
                Task::Growth(mut tile) => {
                    let s = self.settings();
                    let cycle = (11 * (tile % s.sx()) + 9 * (tile / s.sx()))
                        .wrapping_add((s.ticks() >> 8) as u32);
                    let t = self.tile(tile);
                    if cycle & 7 == 7 && t.ground() == 0 && t.density() < 3 {
                        self.ground(tile, 0, t.density() + 1);
                        self.dirty(tile);
                    }
                    if self.settings().extra() == 3 || cycle % 16 != 15 {
                        continue;
                    }
                    let t = self.tile(tile);
                    match t.growth() {
                        3 => {
                            if self.settings().climate() == 2
                                && t.species() != CACTUS
                                && t.zone() == 1
                            {
                                self.write(3, tile, 1, 0, 0, 0);
                            } else {
                                let r = (self.random)() & 7;
                                match r {
                                    0 => {
                                        self.write(3, tile, 1, 0, 0, 0);
                                    }
                                    1 if self.tile(tile).count() < 4
                                        && self.tile(tile).spread(self.settings()) =>
                                    {
                                        self.write(2, tile, 1, 0, 0, 0);
                                        self.write(4, tile, 0, 0, 0, 0);
                                    }
                                    1 | 2 => {
                                        if self.tile(tile).spread(self.settings()) {
                                            let species = self.tile(tile).species();
                                            let direction = ((self.random)() % 8) as usize;
                                            let (x, y) = [
                                                (-1, -1),
                                                (-1, 0),
                                                (-1, 1),
                                                (0, 1),
                                                (1, 1),
                                                (1, 0),
                                                (1, -1),
                                                (0, -1),
                                            ][direction];
                                            tile = tile.wrapping_add(
                                                (x + y * self.settings().sx() as i32) as u32,
                                            );
                                            let neighbor = self.tile(tile);
                                            if !neighbor.suitable(false)
                                                || (neighbor.kind() == CLEAR
                                                    && neighbor.ground() == 0
                                                    && !neighbor.snow()
                                                    && neighbor.density() != 3)
                                            {
                                                continue;
                                            }
                                            self.tasks.push(Task::Dirty(tile));
                                            self.tasks.push(Task::Plant(tile, species, 0, 0));
                                            continue;
                                        }
                                    }
                                    _ => continue,
                                }
                            }
                        }
                        6 => {
                            if !t.spread(self.settings()) {
                                self.write(4, tile, 0, 0, 0, 0);
                            } else if self.tile(tile).count() > 1 {
                                self.write(2, tile, u32::MAX, 0, 0, 0);
                                self.write(4, tile, 3, 0, 0, 0);
                            } else {
                                let t = self.tile(tile);
                                match t.ground() {
                                    3 => {
                                        self.write(7, tile, 0, 0, 0, 0);
                                    }
                                    0 => {
                                        self.write(6, tile, 0, t.density(), 0, 0);
                                    }
                                    1 => {
                                        self.write(6, tile, 1, 3, 0, 0);
                                    }
                                    4 => {
                                        self.write(6, tile, 1, 3, 0, 0);
                                        self.write(8, tile, t.density(), 0, 0, 0);
                                    }
                                    _ if self.settings().climate() == 2 => {
                                        self.write(6, tile, 5, t.density(), 0, 0);
                                    }
                                    _ => {
                                        self.write(6, tile, 0, 3, 0, 0);
                                        self.write(8, tile, t.density(), 0, 0, 0);
                                    }
                                }
                            }
                        }
                        _ => {
                            self.write(3, tile, 1, 0, 0, 0);
                        }
                    }
                    self.dirty(tile);
                }
                Task::Tick => {
                    let s = self.settings();
                    if s.extra() == 0 || s.extra() == 3 {
                        continue;
                    }
                    let skip = s.scale(16);
                    if skip < 16 && (s.ticks() & u64::from(16 / skip - 1)) != 0 {
                        continue;
                    }
                    self.tasks.push(Task::CounterTick);
                    if s.climate() == 2 {
                        self.tasks.push(Task::RainforestTicks(s.scale(1)));
                    }
                }
                Task::RainforestTicks(n) => {
                    if n != 0 {
                        self.tasks.push(Task::RainforestTicks(n - 1));
                        self.tasks.push(Task::RandomTree(true));
                    }
                }
                Task::CounterTick => {
                    let s = self.settings();
                    // SAFETY: Called on the game thread; no reference/raw-byte access survives this block.
                    let wrapped = unsafe {
                        let (next, wrapped) = decrement(TREE_COUNTER, s.scale(1));
                        TREE_COUNTER = next;
                        wrapped
                    };
                    if wrapped && self.settings().extra() != 1 {
                        self.tasks.push(Task::RandomTree(false));
                    }
                }
                Task::RandomTree(rainforest) => {
                    let r = (self.random)();
                    let s = self.settings();
                    let tile = s.random_tile(r);
                    let t = self.tile(tile);
                    if (rainforest && t.zone() != 2) || !t.suitable(false) {
                        continue;
                    }
                    let species = t.random_type(self.settings(), r >> 24);
                    if species != INVALID_TREE {
                        self.tasks.push(Task::Plant(tile, species, 0, 0));
                    }
                }
                Task::CommandInit(end, start, tree, diagonal) => {
                    let s = self.settings();
                    let climate = s.climate() as usize;
                    if start >= s.size()
                        || (tree != INVALID_TREE
                            && tree.wrapping_sub([0, 12, 20, 32][climate])
                                >= [12, 8, 12, 9][climate])
                    {
                        self.result = 1;
                        continue;
                    }
                    self.tree = tree;
                    self.tasks.push(Task::CommandBegin);
                    return Action::new(9, end, start, u32::from(diagonal));
                }
                Task::CommandBegin => {
                    self.limit = response as i32;
                    self.tasks.push(Task::CommandNext(false));
                }
                Task::CommandNext(advance) => {
                    let tile = self.write(11, 0, u32::from(advance), 0, 0, 0) as u32;
                    if tile == INVALID {
                        continue;
                    }
                    let t = self.tile(tile);
                    if t.kind() == TREES {
                        if t.count() == 4 {
                            self.message = 1;
                            self.tasks.push(Task::CommandNext(true));
                            continue;
                        }
                        self.limit -= 1;
                        if self.limit < 1 {
                            self.message = 2;
                            if self.limit >= 0 {
                                self.tasks.push(Task::CommandNext(true));
                            }
                            continue;
                        }
                        if self.settings().execute() {
                            self.write(2, tile, 1, 0, 0, 0);
                            self.dirty(tile);
                            self.write(10, tile, 0, 0, 0, 0);
                        }
                        self.cost = self
                            .cost
                            .saturating_add(self.settings().build().saturating_mul(2));
                        self.tasks.push(Task::CommandNext(true));
                    } else if t.kind() == WATER || t.kind() == CLEAR {
                        if t.kind() == WATER && (!t.coast() || t.corner()) {
                            self.message = 3;
                            self.tasks.push(Task::CommandNext(true));
                            continue;
                        }
                        if t.bridge() {
                            self.message = 4;
                            self.tasks.push(Task::CommandNext(true));
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
                            self.tasks.push(Task::CommandNext(true));
                            continue;
                        }
                        self.limit -= 1;
                        if self.limit < 1 {
                            self.message = 2;
                            if self.limit >= 0 {
                                self.tasks.push(Task::CommandNext(true));
                            }
                            continue;
                        }
                        if t.kind() == CLEAR && matches!(t.ground(), 2 | 3) {
                            self.tasks.push(Task::CommandAfterClear(tile, tree));
                            return Action::new(10, tile, 0, 0);
                        }
                        self.tasks.push(Task::CommandRating(tile, tree));
                    } else {
                        self.message = 4;
                        self.tasks.push(Task::CommandNext(true));
                    }
                }
                Task::CommandAfterClear(tile, tree) => {
                    if response != 0 {
                        self.result = 3;
                        self.tasks.clear();
                    } else {
                        self.cost = self.cost.saturating_add(nested_cost);
                        self.tasks.push(Task::CommandRating(tile, tree));
                    }
                }
                Task::CommandRating(tile, tree) => {
                    self.tasks.push(Task::CommandPlant(tile, tree));
                    let s = self.settings();
                    if !s.editor() && s.company() {
                        return Action::new(8, tile, 1, 0);
                    }
                }
                Task::CommandPlant(tile, mut tree) => {
                    if self.settings().execute() {
                        if tree == INVALID_TREE {
                            let r = (self.random)();
                            tree = self.tile(tile).random_type(self.settings(), r >> 24);
                            if tree == INVALID_TREE {
                                tree = CACTUS;
                            }
                        }
                        self.tasks.push(Task::CommandFinishTile(tile, tree));
                        self.tasks.push(Task::Plant(
                            tile,
                            tree,
                            0,
                            if self.settings().editor() { 3 } else { 0 },
                        ));
                    } else {
                        self.cost = self.cost.saturating_add(self.settings().build());
                        self.tasks.push(Task::CommandNext(true));
                    }
                }
                Task::CommandFinishTile(tile, tree) => {
                    self.dirty(tile);
                    self.write(10, tile, 0, 0, 0, 0);
                    if self.settings().editor() && (20..27).contains(&tree) {
                        self.write(9, tile, 2, 0, 0, 0);
                    }
                    self.cost = self.cost.saturating_add(self.settings().build());
                    self.tasks.push(Task::CommandNext(true));
                }
                Task::Clear(tile) => {
                    self.tasks.push(Task::ClearCount(tile));
                    if self.settings().company() {
                        return Action::new(8, tile, 0, 0);
                    }
                }
                Task::ClearCount(tile) => {
                    let t = self.tile(tile);
                    let num = t.count()
                        * if (20..27).contains(&t.species()) {
                            4
                        } else {
                            1
                        };
                    self.tasks.push(Task::ClearResult(num));
                    if self.settings().execute() {
                        return Action::new(7, tile, 0, 0);
                    }
                }
                Task::ClearResult(num) => {
                    self.cost = self.settings().clear().saturating_mul(i64::from(num));
                }
            }
        }
        Action {
            a: self.result,
            b: self.message,
            cost: self.cost,
            ..Action::default()
        }
    }
}

/// Create a private per-invocation tree continuation. Scalar kind/arguments follow
/// `trees_ffi.h`; the only retained pointers are the leaf callback context and functions.
///
/// # Safety
/// Context and callbacks remain valid until destroy, are exclusively accessed by
/// this game-thread invocation, cannot throw/reenter Rust, and write exactly the
/// documented output array lengths. Separate continuations may use separate contexts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_trees_create(
    kind: u32,
    tile: u32,
    a: u32,
    b: u32,
    c: u32,
    context: *mut c_void,
    settings: ReadSettings,
    observe: Observe,
    write: Write,
    random: extern "C" fn() -> u32,
    trig: Trig,
) -> *mut c_void {
    let task = match kind {
        0 => Task::Generate,
        1 => Task::ScatterInit,
        2 => Task::Place(tile, a, b != 0),
        3 => Task::Plant(tile, a, b, c),
        5 => Task::Loop(tile),
        6 => Task::Tick,
        8 => Task::CommandInit(tile, a, b, c != 0),
        9 => Task::Clear(tile),
        _ => Task::Dirty(tile),
    };
    Box::into_raw(Box::new(Engine {
        context,
        current_settings: Settings([0; 12]),
        settings,
        observe,
        write,
        random,
        trig,
        tasks: vec![task],
        cost: 0,
        message: 0,
        limit: 0,
        tree: INVALID_TREE,
        result: 0,
    }))
    .cast()
}

/// Advance until an external action or completion, retaining no world borrow.
///
/// # Safety
/// Owner is live and exclusive, returned by create, with no active advance call.
/// Responses follow the header protocol. All callbacks obey create's contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_trees_advance(
    owner: *mut c_void,
    response: u64,
    cost: i64,
) -> Action {
    // SAFETY: Caller owns the live engine exclusively until this call returns.
    unsafe { &mut *owner.cast::<Engine>() }.advance(response, cost)
}

/// Release continuation storage without accessing callbacks or canonical state.
///
/// # Safety
/// Owner is a live create result, destroyed exactly once with no active access.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_trees_destroy(owner: *mut c_void) {
    // SAFETY: Return allocation ownership to Rust exactly once.
    drop(unsafe { Box::from_raw(owner.cast::<Engine>()) });
}

/// Suitability policy for the explicitly deferred editor brush facade.
///
/// # Safety
/// Values points to ten copied observation words readable for this call only.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_trees_suitable(values: *const u32, desert: u8) -> u8 {
    let mut tile = [0; 10];
    // SAFETY: The caller provides the documented readable array.
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
    }
    impl Default for World {
        fn default() -> Self {
            Self {
                settings: [0, 15, 35, 4096, 64, 64, 10, 0, 0, 2, 15, 8],
                tile: [CLEAR, 0, 0, 0, 0, 3, 0, 0, 0, 0],
                writes: Vec::new(),
                iterator: 0,
                length: 1,
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
    extern "C" fn write(
        context: *mut c_void,
        op: u32,
        _: u32,
        a: u32,
        b: u32,
        c: u32,
        d: u32,
    ) -> u64 {
        // SAFETY: No context reference is retained across this callback.
        let w = unsafe { &mut *context.cast::<World>() };
        if op == 11 {
            w.iterator += a;
            return u64::from(if w.iterator < w.length { 1 } else { INVALID });
        }
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
        0
    }
    extern "C" fn random() -> u32 {
        0
    }
    extern "C" fn trig(_: u32, value: f32) -> f32 {
        value
    }
    fn engine(w: &mut World, task: Task) -> Engine {
        Engine {
            context: std::ptr::from_mut(w).cast(),
            current_settings: Settings([0; 12]),
            settings,
            observe,
            write,
            random,
            trig,
            tasks: vec![task],
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
    #[test]
    fn explicit_editor_planting_and_diagonal_iterator_request() {
        let mut w = World::default();
        w.settings[7] = 2;
        w.settings[11] = 9; // Editor, execute.
        let mut e = engine(&mut w, Task::CommandInit(2, 1, 20, true));
        let action = e.advance(0, 0);
        assert_eq!((action.kind, action.tile, action.a, action.b), (9, 2, 1, 1));
        let result = e.advance(i32::MAX as u64, 0);
        assert_eq!((result.kind, result.a, result.cost), (0, 0, 15));
        assert_eq!((w.tile[6], w.tile[8], w.tile[2]), (20, 3, 2));
        assert_eq!(
            w.writes.iter().map(|x| x.0).collect::<Vec<_>>(),
            [0, 5, 10, 9]
        );
    }
    #[test]
    fn explicit_tree_type_checks_and_wrong_tropical_zone() {
        let mut w = World::default();
        let mut e = engine(&mut w, Task::CommandInit(2, 1, 20, false));
        assert_eq!(e.advance(0, 0).a, 1); // Rainforest is outside temperate tree range.
        w.settings[7] = 2;
        let mut e = engine(&mut w, Task::CommandInit(2, 1, 20, false));
        assert_eq!(e.advance(0, 0).kind, 9);
        let result = e.advance(i32::MAX as u64, 0);
        assert_eq!((result.cost, result.b), (0, 5));
        assert_eq!(w.writes, [] as [(u32, u32, u32); 0]);
    }
    #[test]
    fn exhausted_limit_checks_zero_and_negative_separately() {
        let mut w = World {
            length: 5,
            ..World::default()
        };
        let mut e = engine(&mut w, Task::CommandInit(2, 1, 0, false));
        assert_eq!(e.advance(0, 0).kind, 9);
        let result = e.advance(1, 0);
        assert_eq!((result.cost, result.b, e.limit, w.iterator), (0, 2, -1, 1));
        assert_eq!(w.writes, [] as [(u32, u32, u32); 0]);
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
            let mut e = engine(&mut w, Task::CommandInit(2, 1, 0, false));
            assert_eq!(e.advance(0, 0).kind, 9);
            assert_eq!(e.advance(i32::MAX as u64, 0).cost, price);
            w.settings[2] = price as u64;
            w.tile[6] = 20;
            w.tile[7] = 4;
            let mut e = engine(&mut w, Task::Clear(1));
            assert_eq!(e.advance(0, 0).cost, price);
        }
    }
    #[test]
    fn ambient_reentry_uses_fresh_growth_without_normalizing_bitpattern_seven() {
        let mut w = World::default();
        w.settings[0] = 1024; // Tile1 cycle15.
        w.settings[9] = 0;
        w.tile = [TREES, 0, 0, 0, 0, 3, 0, 1, 7, 0];
        let mut e = engine(&mut w, Task::Loop(1));
        assert_eq!(e.advance(0, 0).kind, 5);
        assert_eq!(e.advance(0, 0).kind, 0);
        assert_eq!(w.writes.iter().map(|x| x.0).collect::<Vec<_>>(), [3, 5]);
        w.writes.clear();
        let mut e = engine(&mut w, Task::Loop(1));
        assert_eq!(e.advance(0, 0).kind, 5);
        w.tile[8] = 6; // Ambient/NewGRF changed the canonical map after Rust returned.
        assert_eq!(e.advance(0, 0).kind, 0);
        assert_eq!(w.writes.iter().map(|x| x.0).collect::<Vec<_>>(), [4, 5]);
        assert_eq!(w.tile[8], 0);
    }
}
