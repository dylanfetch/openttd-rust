/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Complete tile scheduler. Native arrays stay canonical; existing Rust owners
//! dispatch directly without a native trampoline. All panics abort at the C ABI.
#![allow(unsafe_code)]

use crate::{industry, services, trees};

const FEEDBACKS: [u32; 13] = [
    0xD8F, 0x1296, 0x2496, 0x4357, 0x8679, 0x1030E, 0x206CD, 0x403FE, 0x807B8, 0x1004B2, 0x2006A8,
    0x4004B2, 0x800B87,
];
// Serialized by the game-state mutex on both GUI threads; raw save/load access
// occurs only at the original serial boundaries. Never form a Rust reference.
static mut CURSOR: u32 = 0;

/// Stable canonical storage for DATE, old-loader, reset and afterload repair.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_tile_loop_cursor() -> *mut u32 {
    &raw mut CURSOR
}

/// Existing industry tables consumed by this entry, borrowed for the batch.
#[repr(C)]
pub struct IndustryServices {
    pub production: *const industry::Services,
    pub construction: *const industry::ConstructionServices,
    pub lifecycle: *const industry::LifecycleServices,
    pub tiles: *const industry::TileServices,
    pub generation: *const industry::GenerationServices,
}

/// Each native handler remains separately typed; map reads use the shared table.
#[repr(C)]
pub struct Services {
    pub shared: *const services::Services,
    pub trees: *const trees::TreeServices,
    pub industry: IndustryServices,
    pub clear_loop: extern "C" fn(u32),
    pub rail_loop: extern "C" fn(u32),
    pub road_loop: extern "C" fn(u32),
    pub house_loop: extern "C" fn(u32),
    pub station_loop: extern "C" fn(u32),
    pub water_loop: extern "C" fn(u32),
    pub void_loop: extern "C" fn(u32),
    pub tunnel_bridge_loop: extern "C" fn(u32),
    pub object_loop: extern "C" fn(u32),
}

unsafe fn dispatch(tile: u32, log_x: u8, settings: trees::LoopSettings, s: &Services) {
    // SAFETY: All tables are immutable and live for the synchronous batch. Owner
    // entries end their accesses before callbacks; only scalar cursor is local.
    let tile_type = unsafe { ((*s.shared).tile_type)(tile) };
    // These diagnostic hits are absent from normal and timing builds.
    #[cfg(landscape_loop_count)]
    crate::witness::hit(crate::witness::LANDSCAPE + usize::from(tile_type));
    match tile_type {
        0 => (s.clear_loop)(tile),
        1 => (s.rail_loop)(tile),
        2 => (s.road_loop)(tile),
        3 => (s.house_loop)(tile),
        4 => unsafe { trees::openttd_rust_trees_tile_loop(tile, settings, s.trees, s.shared) },
        5 => (s.station_loop)(tile),
        6 => (s.water_loop)(tile),
        7 => (s.void_loop)(tile),
        8 => unsafe {
            industry::openttd_rust_industry_tile_loop(
                tile,
                log_x,
                s.industry.production,
                s.industry.construction,
                s.industry.lifecycle,
                s.industry.tiles,
                s.industry.generation,
            );
        },
        9 => (s.tunnel_bridge_loop)(tile),
        10 => (s.object_loop)(tile),
        // The source dispatch table has handlers only for original live tile types.
        _ => unsafe { std::hint::unreachable_unchecked() },
    }
}

fn sequence(mut tile: u32, log_sum: u8, zero_phase: bool, mut visit: impl FnMut(u32)) -> u32 {
    let feedback = FEEDBACKS[usize::from(log_sum - 12)];
    let mut count = 1_u32 << (log_sum - 8);
    debug_assert_ne!(tile, 0);
    if zero_phase {
        visit(0);
        count -= 1;
    }
    for _ in 0..count {
        visit(tile);
        tile = (tile >> 1) ^ (0_u32.wrapping_sub(tile & 1) & feedback);
    }
    tile
}

/// Own the source's complete batch, including tile-zero phase and final commit.
/// # Safety
/// Dimensions are legal original map dimensions; cursor is nonzero. All service
/// pointers/tables are immutable and live. Calls are serialized with reset and
/// save/load. Callbacks are noexcept; reentry sees the cursor before this batch.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_run_tile_loop(
    log_x: u8,
    log_y: u8,
    settings: trees::LoopSettings,
    services: *const Services,
) {
    // SAFETY: The caller supplies a live immutable table; no state borrow spans
    // dispatch. A nested call therefore preserves original commit-after-batch.
    let s = unsafe { &*services };
    let tile = unsafe { CURSOR };
    #[cfg(landscape_loop_count)]
    {
        crate::witness::hit(crate::witness::LANDSCAPE + 11);
        if settings.tick_counter % 256 == 0 {
            crate::witness::hit(crate::witness::LANDSCAPE + 12);
        }
    }
    let next = sequence(
        tile,
        log_x + log_y,
        settings.tick_counter % 256 == 0,
        |tile| {
            unsafe { dispatch(tile, log_x, settings, s) };
        },
    );
    unsafe { CURSOR = next };
}
