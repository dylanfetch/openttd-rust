/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! House construction and aging policy. Native leaves own packed map storage;
//! Rust owns transitions, traversal and callback order. No borrow spans a leaf.
use super::{Engine, Leaves, change_population, engine};
use crate::services::Services;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct World {
    pub byte: extern "C" fn(u32) -> u8,
    pub completed: extern "C" fn(u32) -> bool,
    pub set_byte: extern "C" fn(u32, u8),
    pub set_completed: extern "C" fn(u32, bool),
    pub house: extern "C" fn(u32) -> u16,
    pub population: extern "C" fn(u16) -> u8,
    pub construction_animation: extern "C" fn(u32, bool),
}
fn advance_single(e: &Engine, w: &World, tile: u32) {
    // Original AB(m5,0,5,1) preserves the upper three bits and wraps the low five.
    let old = (w.byte)(tile);
    let next = (old & !31) | (old.wrapping_add(1) & 31);
    (w.set_byte)(tile, next);
    if (next >> 3) & 3 == 3 {
        (w.set_completed)(tile, true);
    }
    if !(w.completed)(tile) && next & 7 != 0 {
        return;
    }
    (w.construction_animation)(tile, false);
    // The animation can reenter; re-observe completion and resolve the owner here.
    if (w.completed)(tile) {
        let town = u32::from((e.services.town_index)(tile));
        let owner = (e.leaves.state)(town);
        let population = (w.population)((w.house)(tile));
        // SAFETY: The original resolves this live owner after animation. Its access
        // ends before presentation callbacks; no borrow was retained above.
        unsafe {
            change_population(&e.leaves, owner, town, i32::from(population));
        }
        (w.set_byte)(tile, 0);
    }
    (e.services.mark_dirty)(tile);
}
/// All native tables remain live for this synchronous invocation. Animation may
/// reenter owners; copied flags and map bytes follow the source observation points.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_advance_house(
    leaves: *const Leaves,
    services: *const Services,
    world: *const World,
    tile: u32,
) {
    let e = engine(leaves, services);
    // SAFETY: Native immutable house table outlives this call.
    let w = unsafe { *world };
    let flags = (e.leaves.house_flags)((w.house)(tile));
    if flags & 29 != 0 {
        advance_single(&e, &w, tile);
    }
    if flags & 24 != 0 {
        advance_single(&e, &w, e.offset(tile, 0, 1));
    }
    if flags & 20 != 0 {
        advance_single(&e, &w, e.offset(tile, 1, 0));
    }
    if flags & 16 != 0 {
        advance_single(&e, &w, e.offset(tile, 1, 1));
    }
}
/// Original `Map::Iterate` order and house-only age saturation. Map size is captured
/// once by the native timer entry; byte accesses end before every native mutation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_yearly(
    services: *const Services,
    world: *const World,
    size: u32,
) {
    // SAFETY: Immutable native tables remain live through the serial traversal.
    let (s, w) = unsafe { (*services, *world) };
    for tile in 0..size {
        if (s.tile_type)(tile) != 3 {
            continue;
        }
        if (w.completed)(tile) {
            let age = (w.byte)(tile);
            if age != u8::MAX {
                (w.set_byte)(tile, age + 1);
            }
        }
    }
}

/// Updated north-part ID and original signed map-relative tile difference.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct North {
    pub house: u16,
    pub diff: i32,
}
fn north(e: &Engine, mut house: u16) -> North {
    let (mut x, mut y) = (0, 0);
    if house >= 3 {
        let previous = (e.leaves.house_flags)(house - 1);
        if previous & 4 != 0 {
            house -= 1;
            x = -1;
        } else if previous & 24 != 0 {
            house -= 1;
            y = -1;
        } else if (e.leaves.house_flags)(house - 2) & 16 != 0 {
            house -= 2;
            x = -1;
        } else if (e.leaves.house_flags)(house - 3) & 16 != 0 {
            house -= 3;
            x = -1;
            y = -1;
        }
    }
    North {
        house,
        diff: e.offset(0, x, y) as i32,
    }
}
/// Original conditional spec observations and north-part normalization. Tables
/// remain immutable/live throughout this call; no native/Rust borrow is retained.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_house_north(
    leaves: *const Leaves,
    services: *const Services,
    house: u16,
) -> North {
    north(&engine(leaves, services), house)
}
