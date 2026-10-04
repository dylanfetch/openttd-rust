/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Controller path extension, rollback, temporary-order lookahead, and reservations.
#![allow(
    unsafe_code,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::too_many_lines,
    clippy::too_many_arguments,
    clippy::similar_names,
    clippy::many_single_char_names,
    clippy::items_after_statements
)]
use crate::train_state::State;
use std::{
    cell::RefCell,
    future::Future,
    pin::Pin,
    rc::Rc,
    task::{Context, Poll, Waker},
};
const INVALID: u32 = u32::MAX;
const STATION_RAIL: u32 = 73;
const CHECK_REVERSE: u32 = 74;
const CONDITIONAL: u32 = 75;
const SERVICE: u32 = 76;
const DEPOT: u8 = 0x80;
const WORMHOLE: u8 = 0x40;
const BACKOFF: u32 = 0;
const RESERVE_PATHS: u32 = 1;
const LINE_REVERSE: u32 = 3;
const SHOW_RES: u32 = 4;
const IS_STATION: u32 = 5;
const IS_WAYPOINT: u32 = 6;
const IS_RAILWAY: u32 = 7;
const IS_TUNNEL: u32 = 8;
const IS_DEPOT: u32 = 9;
const IS_PLAIN: u32 = 10;
const STATION: u32 = 11;
const DEPOT_DIR: u32 = 12;
const TUNNEL_DIR: u32 = 13;
const OTHER_END: u32 = 14;
const IS_BRIDGE: u32 = 15;
const TUNNEL_FREE: u32 = 16;
const SET_TUNNEL: u32 = 17;
const MARK_BRIDGE: u32 = 18;
const MARK_TILE: u32 = 19;
const COMPAT_STATION: u32 = 20;
const SET_PLATFORM: u32 = 21;
const UNRESERVE: u32 = 22;
const RESERVED: u32 = 23;
const OVERLAP: u32 = 24;
const HAS_RESERVED: u32 = 25;
const HAS_SIGNAL: u32 = 26;
const HAS_PBS: u32 = 27;
const IS_PBS: u32 = 28;
const GREEN: u32 = 29;
const SET_SIGNAL: u32 = 30;
const ONEWAY: u32 = 31;
const BLOCKING: u32 = 32;
const SIGNAL_BUFFER: u32 = 33;
const UPDATE_BUFFER: u32 = 34;
const RAIL90: u32 = 35;
const EXIT_DIR: u32 = 36;
const REACH_TRACKS: u32 = 37;
const REACH_DIRS: u32 = 38;
const CROSS_TRACKS: u32 = 39;
const CROSS_DIRS: u32 = 40;
const ENTER_TD: u32 = 41;
const TILE_ADD: u32 = 42;
const TILE_OFFSET: u32 = 43;
const SAFE: u32 = 44;
const FREE: u32 = 45;
const TRY_TRACK: u32 = 46;
const DEPOT_RESERVED: u32 = 47;
const SET_DEPOT: u32 = 48;
const TRACK_STATUS: u32 = 49;
const DIAG_REACH_DIRS: u32 = 50;
const TRACKDIR: u32 = 51;
const ALL_COMPAT: u32 = 52;
const ORDER_STOP: u32 = 53;
const STUCK: u32 = 54;
const START_STOP: u32 = 55;
const PATH_RESULT: u32 = 56;
const SAVE_ORDER: u32 = 57;
const RESTORE_ORDER: u32 = 58;
const WRITE_DEST: u32 = 59;
const WRITE_LAST: u32 = 60;
const WRITE_SUPPRESS: u32 = 61;
const ORDER_TYPE: u32 = 62;
const ORDER_SERVICE: u32 = 63;
const NEEDS_SERVICE: u32 = 64;
const COPY_ORDER: u32 = 65;
const SET_DEPOT_DEST: u32 = 66;
const STATION_TRAIN: u32 = 67;
const STATION_XY: u32 = 68;
const INCREMENT_ORDER: u32 = 69;
const DIAG_TRACK: u32 = 70;
const BITS_TRACK: u32 = 71;
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct View {
    pub tile: u32,
    pub dest: u32,
    pub next: u32,
    pub destination: u16,
    pub last_station: u16,
    pub direction: u8,
    pub order: u8,
    pub num_orders: u8,
    pub order_index: u8,
    pub suppress: u8,
    pub nearest: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Follow {
    pub old_tile: u32,
    pub new_tile: u32,
    pub skipped: i32,
    pub dirs: u16,
    pub old_td: u8,
    pub exitdir: u8,
    pub tunnel: u8,
    pub bridge: u8,
    pub station: u8,
    pub error: u8,
}
impl Default for Follow {
    fn default() -> Self {
        Self {
            old_tile: INVALID,
            new_tile: INVALID,
            skipped: 0,
            dirs: 0,
            old_td: 0xff,
            exitdir: 0xff,
            tunnel: 0,
            bridge: 0,
            station: 0,
            error: 0,
        }
    }
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Pbs {
    pub tile: u32,
    pub other: u32,
    pub td: u8,
    pub okay: u8,
}
impl Default for Pbs {
    fn default() -> Self {
        Self {
            tile: INVALID,
            other: INVALID,
            td: 0xff,
            okay: 0,
        }
    }
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Step {
    pub value: u64,
    pub action: u32,
    pub id: u32,
    pub tile: u32,
    pub final_dest: u32,
    pub td: u8,
    pub dir: u8,
    pub tracks: u8,
    pub reserve: u8,
    pub found: u8,
    pub got: u8,
    pub okay: u8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Leaves {
    pub observe: extern "C" fn(u32, *mut View),
    pub leaf: extern "C" fn(*mut std::ffi::c_void, u32, u32, u64, u64, u64) -> u64,
    pub owner: extern "C" fn(u32) -> *mut State,
    pub follow: extern "C" fn(u32, u64, *mut Follow) -> u8,
    pub origin: extern "C" fn(u32, u8) -> Pbs,
}
#[derive(Default)]
struct Mailbox {
    action: Option<Step>,
    result: Option<Step>,
}
#[derive(Clone)]
struct Game {
    leaves: Leaves,
    context: *mut std::ffi::c_void,
    mailbox: Rc<RefCell<Mailbox>>,
}
struct Call {
    mailbox: Rc<RefCell<Mailbox>>,
    action: Option<Step>,
}
impl Future for Call {
    type Output = Step;
    fn poll(mut self: Pin<&mut Self>, _: &mut Context<'_>) -> Poll<Step> {
        if let Some(action) = self.action.take() {
            self.mailbox.borrow_mut().action = Some(action);
            return Poll::Pending;
        }
        Poll::Ready(
            self.mailbox
                .borrow_mut()
                .result
                .take()
                .expect("C++ resumes one named service"),
        )
    }
}
impl Game {
    fn read(&self, id: u32) -> View {
        let mut v = View::default();
        (self.leaves.observe)(id, &raw mut v);
        v
    }
    fn leaf(&self, op: u32, id: u32, a: u64, b: u64, c: u64) -> u64 {
        (self.leaves.leaf)(self.context, op, id, a, b, c)
    }
    fn op(&self, op: u32, id: u32) -> u64 {
        self.leaf(op, id, 0, 0, 0)
    }
    fn val(&self, op: u32, id: u32, a: u64) -> u64 {
        self.leaf(op, id, a, 0, 0)
    }
    fn pair(&self, op: u32, id: u32, a: u64, b: u64) -> u64 {
        self.leaf(op, id, a, b, 0)
    }
    fn test(&self, op: u32, id: u32, t: u32) -> bool {
        self.val(op, id, u64::from(t)) != 0
    }
    fn has(&self, op: u32, id: u32, t: u32, td: u8) -> bool {
        self.pair(op, id, u64::from(t), u64::from(td)) != 0
    }
    fn get(&self, id: u32, field: u8) -> u64 {
        // SAFETY: Live canonical owner on the game thread; scalar borrow ends here.
        unsafe { crate::train_state::get((self.leaves.owner)(id), field) }
    }
    fn set(&self, id: u32, field: u8, value: u64) {
        // SAFETY: Exclusive serialized access; no borrowed state crosses a callback.
        unsafe {
            crate::train_state::set((self.leaves.owner)(id), field, value);
        }
    }
    fn follow(&self, id: u32, types: u64, ft: &mut Follow, tile: u32, td: u8) -> bool {
        ft.old_tile = tile;
        ft.old_td = td;
        (self.leaves.follow)(id, types, ft) != 0
    }
    fn origin(&self, id: u32, other: bool) -> Pbs {
        (self.leaves.origin)(id, u8::from(other))
    }
    fn dir(&self, id: u32, td: u8) -> u8 {
        self.val(EXIT_DIR, id, u64::from(td)) as u8
    }
    fn add(&self, id: u32, tile: u32, dir: u8) -> u32 {
        self.pair(TILE_ADD, id, u64::from(tile), u64::from(dir)) as u32
    }
    fn signal(&self, id: u32, tile: u32, td: u8, green: bool) {
        self.leaf(
            SET_SIGNAL,
            id,
            u64::from(tile),
            u64::from(td),
            u64::from(green),
        );
    }
    fn reserve_bounded(&self, id: u32, tile: u32, td: u8) -> bool {
        self.has(TRY_TRACK, id, tile, td & 7)
    }
    async fn reserve_track(&self, id: u32, tile: u32, track: u8) -> bool {
        if self.test(STATION_RAIL, id, tile) {
            // Original TryReserveRailTrack runs station randomisation then animation.
            self.call(Step {
                action: 8,
                id,
                tile,
                td: track,
                ..Step::default()
            })
            .await
            .value
                != 0
        } else {
            self.has(TRY_TRACK, id, tile, track)
        }
    }
    async fn reserve(&self, id: u32, tile: u32, td: u8) -> bool {
        self.reserve_track(id, tile, td & 7).await
    }
    fn unreserve(&self, id: u32, tile: u32, td: u8) {
        self.pair(UNRESERVE, id, u64::from(tile), u64::from(td & 7));
    }
    fn clear_stuck(&self, id: u32) {
        self.set(id, 0, self.get(id, 0) & !(1 << 8));
    }
    fn stuck(&self, id: u32) -> bool {
        self.get(id, 0) & (1 << 8) != 0
    }
    fn td(&self, id: u32) -> u8 {
        self.op(TRACKDIR, id) as u8
    }
    async fn call(&self, action: Step) -> Step {
        Call {
            mailbox: self.mailbox.clone(),
            action: Some(action),
        }
        .await
    }
    async fn service(&self, action: u32, id: u32) -> Step {
        self.call(Step {
            action,
            id,
            ..Step::default()
        })
        .await
    }
    async fn pathfind(
        &self,
        id: u32,
        tile: u32,
        dir: u8,
        tracks: u8,
        reserve: bool,
        want_final: bool,
    ) -> Step {
        // The named YAPF service may run station animation/NewGRF callbacks.
        self.call(Step {
            action: 1,
            id,
            tile,
            dir,
            tracks,
            reserve: u8::from(reserve),
            got: u8::from(want_final),
            final_dest: INVALID,
            found: 1,
            ..Step::default()
        })
        .await
    }
    async fn safe_track(&self, id: u32, tile: u32, td: u8, override_types: bool) -> bool {
        self.call(Step {
            action: 2,
            id,
            tile,
            td,
            reserve: u8::from(override_types),
            ..Step::default()
        })
        .await
        .value
            != 0
    }
}
fn first(bits: u16) -> u8 {
    if bits == 0 {
        0xff
    } else {
        bits.trailing_zeros() as u8
    }
}
fn single(bits: u16) -> bool {
    bits & bits.wrapping_sub(1) == 0
}
fn tracks(bits: u16) -> u8 {
    (bits | (bits >> 8)) as u8
}
fn dirs(bits: u8) -> u16 {
    u16::from(bits) | (u16::from(bits) << 8)
}
fn pbs(step: Step) -> Pbs {
    Pbs {
        tile: step.tile,
        td: step.td,
        okay: step.okay,
        other: INVALID,
    }
}
fn clear(g: &Game, id: u32, tile: u32, td: u8) {
    let dir = g.dir(id, td);
    if g.test(IS_TUNNEL, id, tile) {
        if g.val(TUNNEL_DIR, id, u64::from(tile)) as u8 == dir ^ 2 {
            let end = g.val(OTHER_END, id, u64::from(tile)) as u32;
            if g.pair(TUNNEL_FREE, id, u64::from(tile), u64::from(end)) != 0 {
                g.pair(SET_TUNNEL, id, u64::from(tile), 0);
                g.pair(SET_TUNNEL, id, u64::from(end), 0);
                if g.op(SHOW_RES, id) != 0 {
                    if g.test(IS_BRIDGE, id, tile) {
                        g.val(MARK_BRIDGE, id, u64::from(tile));
                    } else {
                        g.val(MARK_TILE, id, u64::from(tile));
                        g.val(MARK_TILE, id, u64::from(end));
                    }
                }
            }
        }
    } else if g.test(IS_STATION, id, tile) {
        let next = g.add(id, tile, dir);
        if g.pair(COMPAT_STATION, id, u64::from(next), u64::from(tile)) == 0 {
            g.leaf(SET_PLATFORM, id, u64::from(tile), u64::from(dir ^ 2), 0);
        }
    } else {
        g.unreserve(id, tile, td);
    }
}
fn free(g: &Game, id: u32) {
    let v = g.read(id);
    let mut tile = v.tile;
    let mut td = g.td(id);
    let mut free_tile = !(g.test(IS_STATION, id, tile) || g.test(IS_TUNNEL, id, tile));
    let station = if g.test(IS_STATION, id, tile) {
        g.val(STATION, id, u64::from(tile)) as u16
    } else {
        u16::MAX
    };
    if g.test(IS_DEPOT, id, tile) && g.dir(id, td) != g.val(DEPOT_DIR, id, u64::from(tile)) as u8 {
        return;
    }
    if g.get(id, 5) == u64::from(DEPOT) {
        let mut u = id;
        while u != INVALID {
            let part = g.read(u);
            if g.get(u, 5) != u64::from(DEPOT) || part.tile != v.tile {
                return;
            }
            u = part.next;
        }
    }
    let reserved = g.val(RESERVED, id, u64::from(tile)) as u8;
    if g.val(OVERLAP, id, u64::from(reserved | (1 << (td & 7)))) != 0 {
        return;
    }
    let types = g.op(ALL_COMPAT, id);
    let mut ft = Follow::default();
    while g.follow(id, types, &mut ft, tile, td) {
        tile = ft.new_tile;
        let bits = ft.dirs & dirs(g.val(RESERVED, id, u64::from(tile)) as u8);
        td = first(bits);
        if td == 0xff {
            break;
        }
        if g.test(IS_RAILWAY, id, tile) {
            if g.has(HAS_SIGNAL, id, tile, td) && !g.has(IS_PBS, id, tile, td & 7) {
                g.unreserve(id, tile, td);
                break;
            }
            if g.has(HAS_PBS, id, tile, td) {
                if !g.has(GREEN, id, tile, td) {
                    break;
                }
                g.signal(id, tile, td, false);
                g.val(MARK_TILE, id, u64::from(tile));
            } else if g.has(HAS_PBS, id, tile, td ^ 8) {
                g.pair(
                    SIGNAL_BUFFER,
                    id,
                    u64::from(tile),
                    u64::from(g.dir(id, td ^ 8)),
                );
            } else if g.has(HAS_SIGNAL, id, tile, td ^ 8) && g.has(ONEWAY, id, tile, td & 7) {
                break;
            }
        }
        if free_tile
            || (!(ft.station != 0 && g.val(STATION, id, u64::from(ft.new_tile)) as u16 == station)
                && ft.tunnel == 0
                && ft.bridge == 0)
        {
            clear(g, id, tile, td);
        }
        free_tile = true;
    }
    g.op(UPDATE_BUFFER, id);
}
fn extend(g: &Game, id: u32, new_tracks: &mut u8, enterdir: &mut u8) -> Pbs {
    let origin = g.origin(id, false);
    let types = g.get(id, 3);
    let mut ft = Follow::default();
    let mut red = Vec::new();
    let mut tile = origin.tile;
    let mut td = origin.td;
    while g.follow(id, types, &mut ft, tile, td) {
        if single(ft.dirs) && g.has(BLOCKING, id, ft.new_tile, first(ft.dirs)) {
            break;
        }
        if g.pair(RAIL90, id, u64::from(ft.old_tile), u64::from(ft.new_tile)) != 0 {
            ft.dirs &= !(g.val(CROSS_DIRS, id, u64::from(ft.old_td)) as u16);
            if ft.dirs == 0 {
                break;
            }
        }
        let target = ft.station != 0
            || (g.test(IS_RAILWAY, id, ft.new_tile) && !g.test(IS_PLAIN, id, ft.new_tile));
        if target || !single(ft.dirs) {
            if g.pair(
                HAS_RESERVED,
                id,
                u64::from(ft.new_tile),
                u64::from(tracks(g.val(REACH_DIRS, id, u64::from(ft.old_td)) as u16)),
            ) != 0
            {
                break;
            }
            if ft.skipped != 0 {
                let off = g.val(TILE_OFFSET, id, u64::from(ft.exitdir)) as i32;
                ft.new_tile = ft
                    .new_tile
                    .wrapping_sub(off.wrapping_mul(ft.skipped) as u32);
            }
            *new_tracks = tracks(ft.dirs);
            *enterdir = ft.exitdir;
            return Pbs {
                tile: ft.new_tile,
                td: ft.old_td,
                okay: 0,
                other: INVALID,
            };
        }
        tile = ft.new_tile;
        td = first(ft.dirs);
        let rev = td ^ 8;
        if g.has(SAFE, id, tile, td) {
            if !(g.has(FREE, id, tile, td) && g.reserve_bounded(id, tile, td)) {
                break;
            }
            if g.has(HAS_PBS, id, tile, rev) && g.has(GREEN, id, tile, rev) {
                red.push((tile, rev));
                g.signal(id, tile, rev, false);
                g.val(MARK_TILE, id, u64::from(tile));
            }
            return Pbs {
                tile,
                td,
                okay: 1,
                other: INVALID,
            };
        }
        if !g.reserve_bounded(id, tile, td) {
            break;
        }
        if g.has(HAS_PBS, id, tile, rev) && g.has(GREEN, id, tile, rev) {
            red.push((tile, rev));
            g.signal(id, tile, rev, false);
            g.val(MARK_TILE, id, u64::from(tile));
        }
    }
    if ft.error == 1 || ft.error == 4 {
        return Pbs {
            tile: ft.old_tile,
            td: ft.old_td,
            okay: 1,
            other: INVALID,
        };
    }
    tile = origin.tile;
    td = origin.td;
    let stopped = ft.old_tile;
    let stopped_td = ft.old_td;
    while tile != stopped || td != stopped_td {
        if !g.follow(id, types, &mut ft, tile, td) {
            break;
        }
        if g.pair(RAIL90, id, u64::from(ft.old_tile), u64::from(ft.new_tile)) != 0 {
            ft.dirs &= !(g.val(CROSS_DIRS, id, u64::from(ft.old_td)) as u16);
        }
        tile = ft.new_tile;
        td = first(ft.dirs);
        g.unreserve(id, tile, td);
    }
    for (tile, td) in red {
        g.signal(id, tile, td, true);
    }
    Pbs::default()
}
struct Orders {
    g: Game,
    id: u32,
    dest: u32,
    last: u16,
    suppress: u8,
    index: u8,
    restored: bool,
}
impl Orders {
    fn new(g: &Game, id: u32) -> Self {
        let v = g.read(id);
        g.op(SAVE_ORDER, id);
        Self {
            g: g.clone(),
            id,
            dest: v.dest,
            last: v.last_station,
            suppress: v.suppress,
            index: v.order_index,
            restored: false,
        }
    }
    fn restore(&mut self) {
        self.g.op(RESTORE_ORDER, self.id);
        self.g.val(WRITE_DEST, self.id, u64::from(self.dest));
        self.g.val(WRITE_LAST, self.id, u64::from(self.last));
        self.g
            .val(WRITE_SUPPRESS, self.id, u64::from(self.suppress));
        self.restored = true;
    }
    async fn next(&mut self, skip: bool) -> bool {
        let g = self.g.clone();
        let id = self.id;
        if g.read(id).num_orders == 0 {
            return false;
        }
        if skip {
            self.index = self.index.wrapping_add(1);
        }
        let mut depth = 0;
        loop {
            if self.index >= g.read(id).num_orders {
                self.index = 0;
            }
            let kind = g.val(ORDER_TYPE, id, u64::from(self.index)) as u8;
            match kind {
                1 | 2 | 6 => {
                    if kind != 2
                        || g.val(ORDER_SERVICE, id, u64::from(self.index)) == 0
                        || g.op(NEEDS_SERVICE, id) != 0
                    {
                        g.val(COPY_ORDER, id, u64::from(self.index));
                        return g
                            .call(Step {
                                action: 6,
                                id,
                                value: u64::from(self.index),
                                ..Step::default()
                            })
                            .await
                            .value
                            != 0;
                    }
                }
                7 => {
                    let next = g.val(CONDITIONAL, id, u64::from(self.index)) as u8;
                    if next != 0xff {
                        depth += 1;
                        self.index = next;
                        if self.index != g.read(id).order_index
                            && depth < i32::from(g.read(id).num_orders)
                        {
                            continue;
                        }
                        return false;
                    }
                }
                _ => {}
            }
            self.index = self.index.wrapping_add(1);
            depth += 1;
            if self.index == g.read(id).order_index || depth >= i32::from(g.read(id).num_orders) {
                return false;
            }
        }
    }
}
impl Drop for Orders {
    fn drop(&mut self) {
        if !self.restored {
            self.restore();
        }
    }
}
async fn choose(
    g: &Game,
    id: u32,
    tile: u32,
    dir: u8,
    mut available: u8,
    force: bool,
    mark: bool,
) -> (u8, bool) {
    let mut best = 0xff;
    let mut reserve = g.op(RESERVE_PATHS, id) != 0 || force;
    let mut changed = false;
    let mut final_dest = INVALID;
    let mut got = false;
    let res =
        g.val(RESERVED, id, u64::from(tile)) as u8 & g.val(REACH_TRACKS, id, u64::from(dir)) as u8;
    if res != 0 {
        return (first(u16::from(res)), false);
    }
    if single(u16::from(available)) {
        let track = first(u16::from(available));
        if track != 0xff
            && g.has(
                HAS_PBS,
                id,
                tile,
                g.pair(ENTER_TD, id, u64::from(track), u64::from(dir)) as u8,
            )
        {
            reserve = true;
            changed = true;
            g.signal(
                id,
                tile,
                g.pair(ENTER_TD, id, u64::from(track), u64::from(dir)) as u8,
                true,
            );
        } else if !reserve {
            return (track, false);
        }
        best = track;
    }
    let mut dest = Pbs {
        tile,
        td: 0xff,
        okay: 0,
        other: INVALID,
    };
    let mut dest_dir = dir;
    if reserve {
        dest = extend(g, id, &mut available, &mut dest_dir);
        if dest.tile == INVALID {
            if mark {
                g.op(STUCK, id);
            }
            if changed {
                g.signal(
                    id,
                    tile,
                    g.pair(ENTER_TD, id, u64::from(best), u64::from(dir)) as u8,
                    false,
                );
            }
            return (first(u16::from(available)), false);
        }
        if dest.okay != 0 {
            if changed {
                g.val(MARK_TILE, id, u64::from(tile));
            }
            g.reserve(id, g.read(id).tile, g.td(id)).await;
            return (best, true);
        }
        g.op(SERVICE, id);
        if matches!(g.read(id).order, 2 | 5 | 7) {
            g.service(4, id).await;
        }
    }
    let mut orders = Orders::new(g, id);
    let v = g.read(id);
    if v.order == 4 {
        orders.next(false).await;
    } else if v.order == 3
        || (v.order != 2
            && if v.order == 1 {
                g.test(IS_STATION, id, v.tile)
                    && u64::from(v.destination) == g.val(STATION, id, u64::from(v.tile))
            } else {
                v.tile == v.dest
            })
    {
        orders.next(true).await;
    }
    if dest.tile != INVALID && dest.okay == 0 {
        let new_tile = dest.tile;
        let result = g
            .pathfind(id, new_tile, dest_dir, available, reserve, true)
            .await;
        dest = pbs(result);
        final_dest = result.final_dest;
        if new_tile == tile {
            best = result.value as u8;
        }
        g.val(PATH_RESULT, id, u64::from(result.found));
    }
    if !reserve {
        return (best, false);
    }
    if dest.tile != INVALID && dest.okay == 0 {
        if mark {
            g.op(STUCK, id);
        }
        free(g, id);
        return (best, false);
    }
    if dest.tile == INVALID {
        let origin = g.origin(id, false);
        if g.safe_track(id, origin.tile, origin.td, false).await {
            let res = g.val(RESERVED, id, u64::from(tile)) as u8
                & g.val(REACH_TRACKS, id, u64::from(dir)) as u8;
            best = first(u16::from(res));
            g.reserve(id, g.read(id).tile, g.td(id)).await;
            got = true;
            if changed {
                g.val(MARK_TILE, id, u64::from(tile));
            }
        } else {
            free(g, id);
            if mark {
                g.op(STUCK, id);
            }
        }
        return (best, got);
    }
    got = true;
    while !g.has(SAFE, id, dest.tile, dest.td) {
        let exit = g.dir(id, dest.td);
        let next = g.add(id, dest.tile, exit);
        let mut reachable = tracks(g.val(TRACK_STATUS, id, u64::from(next)) as u16)
            & g.val(REACH_TRACKS, id, u64::from(exit)) as u8;
        if g.pair(RAIL90, id, u64::from(dest.tile), u64::from(next)) != 0 {
            reachable &= !(g.val(CROSS_TRACKS, id, u64::from(dest.td & 7)) as u8);
        }
        if orders.next(true).await {
            let current = pbs(g.pathfind(id, next, exit, reachable, true, false).await);
            if current.tile != INVALID {
                dest = current;
                if dest.okay != 0 {
                    continue;
                }
                free(g, id);
                if mark {
                    g.op(STUCK, id);
                }
                got = false;
                changed = false;
                break;
            }
        }
        if !g.safe_track(id, dest.tile, dest.td, true).await {
            free(g, id);
            if mark {
                g.op(STUCK, id);
            }
            got = false;
            changed = false;
        }
        break;
    }
    g.reserve(id, g.read(id).tile, g.td(id)).await;
    if changed {
        g.val(MARK_TILE, id, u64::from(tile));
    }
    orders.restore();
    let v = g.read(id);
    if v.order == 2 && v.nearest != 0 && final_dest != INVALID && g.test(IS_DEPOT, id, final_dest) {
        g.val(SET_DEPOT_DEST, id, u64::from(final_dest));
        g.val(WRITE_DEST, id, u64::from(final_dest));
        g.op(START_STOP, id);
    }
    (best, got)
}
async fn try_path(g: &Game, id: u32, mark: bool, first_okay: bool) -> bool {
    let v = g.read(id);
    if g.get(id, 5) == u64::from(DEPOT) {
        if g.test(DEPOT_RESERVED, id, v.tile) {
            if mark {
                g.op(STUCK, id);
            }
            return false;
        }
        let dir = g.val(DEPOT_DIR, id, u64::from(v.tile)) as u8;
        let next = g.add(id, v.tile, dir);
        if g.pair(
            HAS_RESERVED,
            id,
            u64::from(next),
            g.val(REACH_TRACKS, id, u64::from(dir)),
        ) != 0
        {
            return false;
        }
    }
    let origin = g.origin(id, true);
    if origin.other != INVALID && origin.other != id {
        if mark {
            g.op(STUCK, id);
        }
        return false;
    }
    if origin.okay != 0 && (v.tile != origin.tile || first_okay) {
        if g.stuck(id) {
            g.op(START_STOP, id);
        }
        g.clear_stuck(id);
        return true;
    }
    if g.get(id, 5) == u64::from(DEPOT) {
        g.pair(SET_DEPOT, id, u64::from(v.tile), 1);
        if g.op(SHOW_RES, id) != 0 {
            g.val(MARK_TILE, id, u64::from(v.tile));
        }
    }
    let exit = g.dir(id, origin.td);
    let new_tile = g.add(id, origin.tile, exit);
    let mut reachable = tracks(
        g.val(TRACK_STATUS, id, u64::from(new_tile)) as u16
            & g.val(DIAG_REACH_DIRS, id, u64::from(exit)) as u16,
    );
    if g.pair(RAIL90, id, u64::from(origin.tile), u64::from(new_tile)) != 0 {
        reachable &= !(g.val(CROSS_TRACKS, id, u64::from(origin.td & 7)) as u8);
    }
    let (_, made) = choose(g, id, new_tile, exit, reachable, true, mark).await;
    if !made {
        if g.get(id, 5) == u64::from(DEPOT) {
            g.pair(SET_DEPOT, id, u64::from(g.read(id).tile), 0);
        }
        return false;
    }
    if g.stuck(id) {
        g.set(id, 2, 0);
        g.op(START_STOP, id);
    }
    g.clear_stuck(id);
    true
}
async fn check_next(g: &Game, id: u32) {
    if g.op(BACKOFF, id) == 255 || g.get(id, 5) == u64::from(DEPOT) {
        return;
    }
    let v = g.read(id);
    match v.order {
        2 => {
            if v.tile == v.dest {
                return;
            }
        }
        6 => {
            if g.test(IS_WAYPOINT, id, v.tile)
                && g.val(STATION, id, u64::from(v.tile)) == u64::from(v.destination)
            {
                g.service(4, id).await;
            }
        }
        0 | 3 | 4 if v.num_orders > 0 => return,
        _ => {}
    }
    let tile = g.read(id).tile;
    if g.test(IS_STATION, id, tile)
        && g.val(ORDER_STOP, id, g.val(STATION, id, u64::from(tile))) != 0
    {
        return;
    }
    let td = g.td(id);
    if g.test(IS_RAILWAY, id, tile)
        && g.has(HAS_SIGNAL, id, tile, td)
        && !g.has(IS_PBS, id, tile, td & 7)
        && !g.has(GREEN, id, tile, td)
    {
        return;
    }
    let mut ft = Follow::default();
    if !g.follow(id, g.get(id, 3), &mut ft, tile, td) {
        return;
    }
    if g.pair(
        HAS_RESERVED,
        id,
        u64::from(ft.new_tile),
        u64::from(tracks(ft.dirs)),
    ) == 0
        && single(ft.dirs)
        && g.has(HAS_PBS, id, ft.new_tile, first(ft.dirs))
    {
        let mut available = tracks(ft.dirs);
        if g.pair(RAIL90, id, u64::from(ft.old_tile), u64::from(ft.new_tile)) != 0 {
            available &= !(g.val(CROSS_TRACKS, id, u64::from(ft.old_td & 7)) as u8);
        }
        choose(g, id, ft.new_tile, ft.exitdir, available, false, false).await;
    }
}
async fn run(g: Game, kind: u32, id: u32, a: u64, b: u64, c: u64) -> Step {
    let mut result = Step::default();
    match kind {
        0 => check_next(&g, id).await,
        1 => clear(&g, id, a as u32, b as u8),
        2 => free(&g, id),
        3 => {
            let (track, got) = choose(
                &g,
                id,
                a as u32,
                b as u8,
                (b >> 8) as u8,
                c & 1 != 0,
                c & 2 != 0,
            )
            .await;
            result.value = u64::from(track);
            result.got = u8::from(got);
        }
        4 => result.value = u64::from(try_path(&g, id, a != 0, b != 0).await),
        5 => {
            if g.op(LINE_REVERSE, id) == 0
                && g.get(id, 5) != u64::from(DEPOT)
                && g.get(id, 5) != u64::from(WORMHOLE)
                && g.read(id).direction & 1 != 0
            {
                result.value = g.op(CHECK_REVERSE, id);
            }
        }
        6 => {
            if a as u16 == g.read(id).last_station {
                g.val(WRITE_LAST, id, u64::from(u16::MAX));
            }
            if g.val(STATION_TRAIN, id, a) == 0 {
                g.op(INCREMENT_ORDER, id);
                result.value = 0;
            } else {
                result.value = g.val(STATION_XY, id, a);
            }
        }
        7 => {
            let mut part = id;
            while part != INVALID {
                let v = g.read(part);
                let track = g.get(part, 5) as u8;
                match track {
                    WORMHOLE => {
                        g.reserve_track(
                            part,
                            v.tile,
                            g.val(DIAG_TRACK, part, g.val(TUNNEL_DIR, part, u64::from(v.tile)))
                                as u8,
                        )
                        .await;
                    }
                    DEPOT => {}
                    _ => {
                        g.reserve_track(
                            part,
                            v.tile,
                            g.val(BITS_TRACK, part, u64::from(track)) as u8,
                        )
                        .await;
                    }
                }
                part = v.next;
            }
        }
        _ => unreachable!(),
    }
    result
}
/// A stack-lifetime controller invocation; the future owns all temporary policy state.
pub struct Task {
    future: Pin<Box<dyn Future<Output = Step>>>,
    mailbox: Rc<RefCell<Mailbox>>,
}
/// Create a controller invocation with live synchronous C++ services.
///
/// # Safety
/// The leaves/context and referenced vehicle must remain live until destruction.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_reservation_new(
    kind: u32,
    id: u32,
    a: u64,
    b: u64,
    c: u64,
    leaves: *const Leaves,
    context: *mut std::ffi::c_void,
) -> *mut Task {
    // SAFETY: Caller supplies a live callback table; copy retains no table borrow.
    let leaves = unsafe { *leaves };
    let mailbox = Rc::new(RefCell::new(Mailbox::default()));
    let g = Game {
        leaves,
        context,
        mailbox: mailbox.clone(),
    };
    Box::into_raw(Box::new(Task {
        future: Box::pin(run(g, kind, id, a, b, c)),
        mailbox,
    }))
}
/// Resume after exactly one named C++ service and return the next boundary/result.
///
/// # Safety
/// `task` is a live invocation with exclusive game-thread access.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_reservation_step(
    task: *mut Task,
    result: Step,
) -> Step {
    // SAFETY: Invocation is exclusively accessible for this poll, ending on return.
    let task = unsafe { &mut *task };
    task.mailbox.borrow_mut().result = Some(result);
    let mut cx = Context::from_waker(Waker::noop());
    match task.future.as_mut().poll(&mut cx) {
        Poll::Ready(done) => done,
        Poll::Pending => task
            .mailbox
            .borrow_mut()
            .action
            .take()
            .expect("one named service per suspension"),
    }
}
/// Destroy once; dropping the order saver restores a still-active temporary order.
///
/// # Safety
/// `task` is a live allocation created here, and its leaves/context remain live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_train_reservation_destroy(task: *mut Task) {
    // SAFETY: The C++ stack owner transfers its invocation exactly once.
    unsafe {
        drop(Box::from_raw(task));
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::Cell;
    thread_local! {static ACTIVE:Cell<*mut Mock>=const {Cell::new(std::ptr::null_mut())};}
    struct Mock {
        state: State,
        stop: u8,
        log: Vec<(u32, u32, u8)>,
        view: View,
    }
    impl Mock {
        fn new(stop: u8) -> Self {
            Self {
                state: State::default(),
                stop,
                log: Vec::new(),
                view: View {
                    tile: 0,
                    dest: 9,
                    next: INVALID,
                    last_station: 7,
                    order_index: 254,
                    suppress: 1,
                    ..View::default()
                },
            }
        }
    }
    extern "C" fn observe(_: u32, out: *mut View) {
        ACTIVE.with(|active| {
            // SAFETY: The test installs a live mock for this synchronous invocation.
            unsafe {
                *out = (*active.get()).view;
            }
        });
    }
    extern "C" fn owner(_: u32) -> *mut State {
        ACTIVE.with(|active| {
            // SAFETY: Mock remains exclusively on this test thread, accessor borrow ends here.
            unsafe { &raw mut (*active.get()).state }
        })
    }
    extern "C" fn leaf(
        context: *mut std::ffi::c_void,
        op: u32,
        _: u32,
        a: u64,
        b: u64,
        c: u64,
    ) -> u64 {
        // SAFETY: Test game provides its exclusive live mock; this borrow ends on return.
        let mock = unsafe { &mut *context.cast::<Mock>() };
        match op {
            BLOCKING | RAIL90 | IS_RAILWAY | STATION_RAIL => 0,
            SAFE => u64::from(mock.stop == 2 && a == 2),
            FREE | HAS_PBS | GREEN => 1,
            TRY_TRACK => {
                mock.log.push((op, a as u32, b as u8));
                u64::from(a != 3 || mock.stop == 1)
            }
            SET_SIGNAL => {
                mock.log.push((op, a as u32, c as u8));
                0
            }
            UNRESERVE | MARK_TILE | SAVE_ORDER | RESTORE_ORDER => {
                mock.log.push((op, a as u32, b as u8));
                0
            }
            WRITE_DEST => {
                mock.view.dest = a as u32;
                0
            }
            WRITE_LAST => {
                mock.view.last_station = a as u16;
                0
            }
            WRITE_SUPPRESS => {
                mock.view.suppress = a as u8;
                0
            }
            _ => panic!("unexpected test leaf {op}"),
        }
    }
    extern "C" fn follow(_: u32, _: u64, out: *mut Follow) -> u8 {
        ACTIVE.with(|active| {
            // SAFETY: Installed mock and caller output are live for this synchronous call.
            unsafe {
                let mock = &mut *active.get();
                let ft = &mut *out;
                if ft.old_tile == 3 && mock.stop == 1 {
                    ft.error = 4;
                    return 0;
                }
                ft.new_tile = ft.old_tile + 1;
                ft.dirs = 1;
                ft.error = 0;
                1
            }
        })
    }
    extern "C" fn origin(_: u32, _: u8) -> Pbs {
        Pbs {
            tile: 0,
            other: INVALID,
            td: 0,
            okay: 0,
        }
    }
    fn game(mock: &mut Mock) -> Game {
        ACTIVE.with(|active| active.set(mock));
        Game {
            leaves: Leaves {
                observe,
                leaf,
                owner,
                follow,
                origin,
            },
            context: std::ptr::from_mut(mock).cast(),
            mailbox: Rc::default(),
        }
    }
    #[test]
    fn controller_extension_rolls_back_only_new_tracks_and_restores_signal_order() {
        let mut mock = Mock::new(0);
        let g = game(&mut mock);
        let mut available = 0;
        let mut dir = 0;
        assert_eq!(extend(&g, 0, &mut available, &mut dir).tile, INVALID);
        assert_eq!(
            mock.log,
            vec![
                (TRY_TRACK, 1, 0),
                (SET_SIGNAL, 1, 0),
                (MARK_TILE, 1, 0),
                (TRY_TRACK, 2, 0),
                (SET_SIGNAL, 2, 0),
                (MARK_TILE, 2, 0),
                (TRY_TRACK, 3, 0),
                (UNRESERVE, 1, 0),
                (UNRESERVE, 2, 0),
                (SET_SIGNAL, 1, 1),
                (SET_SIGNAL, 2, 1),
            ]
        );
    }
    #[test]
    fn controller_extension_accepts_safe_position_or_end_of_line_without_rollback() {
        for stop in [1, 2] {
            let mut mock = Mock::new(stop);
            let g = game(&mut mock);
            let mut available = 0;
            let mut dir = 0;
            let result = extend(&g, 0, &mut available, &mut dir);
            assert_eq!(
                (result.tile, result.td, result.okay),
                (if stop == 1 { 3 } else { 2 }, 0, 1)
            );
            assert!(
                !mock
                    .log
                    .iter()
                    .any(|&(op, _, value)| op == UNRESERVE || (op == SET_SIGNAL && value == 1))
            );
        }
    }
    #[test]
    fn dropping_or_explicitly_restoring_order_saver_restores_once() {
        for explicit in [false, true] {
            let mut mock = Mock::new(0);
            let g = game(&mut mock);
            {
                let mut orders = Orders::new(&g, 0);
                mock.view.dest = 42;
                mock.view.last_station = 8;
                mock.view.suppress = 0;
                assert_eq!(orders.index, 254);
                if explicit {
                    orders.restore();
                }
            }
            assert_eq!(
                (mock.view.dest, mock.view.last_station, mock.view.suppress),
                (9, 7, 1)
            );
            assert_eq!(
                mock.log
                    .iter()
                    .filter(|&&(op, _, _)| op == RESTORE_ORDER)
                    .count(),
                1
            );
        }
    }
    #[test]
    fn station_reservation_returns_before_callback_and_resumes_its_result() {
        let mut mock = Mock::new(0);
        let mut g = game(&mut mock);
        extern "C" fn station_leaf(
            context: *mut std::ffi::c_void,
            op: u32,
            id: u32,
            a: u64,
            b: u64,
            c: u64,
        ) -> u64 {
            if op == STATION_RAIL {
                1
            } else {
                leaf(context, op, id, a, b, c)
            }
        }
        g.leaves.leaf = station_leaf;
        let mailbox = g.mailbox.clone();
        let mut future = Box::pin(g.reserve_track(0, 7, 2));
        let mut cx = Context::from_waker(Waker::noop());
        assert!(future.as_mut().poll(&mut cx).is_pending());
        let action = mailbox.borrow_mut().action.take().unwrap();
        assert_eq!(
            (action.action, action.id, action.tile, action.td),
            (8, 0, 7, 2)
        );
        assert_eq!(mock.log, Vec::new());
        mailbox.borrow_mut().result = Some(Step {
            value: 1,
            ..Step::default()
        });
        assert_eq!(future.as_mut().poll(&mut cx), Poll::Ready(true));
    }
}
