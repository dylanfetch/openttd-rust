/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Founding initialization and shared native shell/index presentation leaves.
use super::{Leaves, State, engine, update_radius};
use crate::services::{Services, TownHandle};

#[repr(C)]
#[derive(Clone, Copy)]
pub struct NameParams {
    pub grfid: u32,
    pub kind: u16,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct CountSizes {
    pub houses: usize,
    pub classes: usize,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct World {
    pub count_sizes: extern "C" fn() -> CountSizes,
    pub insert_spatial: extern "C" fn(*const TownHandle),
    pub acceptance_cargo: extern "C" fn(u8) -> bool,
    pub name_params: extern "C" fn() -> NameParams,
    pub initial_city_size: extern "C" fn() -> u8,
    pub show_zone: extern "C" fn(*const TownHandle, bool),
    pub virt_coord: extern "C" fn(*const TownHandle),
    pub dirty_directory: extern "C" fn(),
    pub update_noise: extern "C" fn(),
}

unsafe fn initialize_counts(owner: *mut State, sizes: CountSizes) {
    // SAFETY: Serialized original structural mutation. No view survives clearing
    // or allocating persistent component storage, and no callback runs here.
    unsafe {
        (*owner).id_counts.clear();
        (*owner).class_counts.clear();
        (*owner).id_counts.resize(sizes.houses, 0);
        (*owner).class_counts.resize(sizes.classes, 0);
    }
}
/// Native afterload/cache callers use the same persistent count initialization.
/// The owner is live and no alias survives clearing or resizing its vectors.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_initialize_counts(owner: *mut State, sizes: CountSizes) {
    // SAFETY: Native caller supplies the canonical live owner's count storage.
    unsafe {
        initialize_counts(owner, sizes);
    }
}

fn layout(e: &super::Engine, desired: u8) -> u8 {
    if desired != 4 {
        return desired;
    }
    let tile = e.position();
    let x = tile & (e.size_x() - 1);
    let y = tile / e.size_x();
    // Original TileHash uses unsigned native uint subtraction.
    (((x >> 4) ^ (x >> 6) ^ (y >> 4)).wrapping_sub(y >> 6) % 4) as u8
}

/// Single canonical layout policy for shell callers and founding initialization.
/// Tables and native shell remain live; no owner borrow spans the map-size leaf.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_initialize_layout(
    leaves: *const Leaves,
    services: *const Services,
    native: *const TownHandle,
    desired: u8,
) {
    let mut e = engine(leaves, services);
    e.owner = (e.services.town_owner)(native);
    let selected = layout(&e, desired);
    // SAFETY: Serialized immediate mutation of the resolved live owner.
    unsafe {
        (*e.owner).simulation.layout = selected;
    }
}

/// Native cleanup scope owns generation-flag restoration outside this entry.
/// Owner and immutable tables remain live; every field access ends before a leaf
/// or growth call, including commands and `NewGRF` that can reenter this owner.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_create(
    leaves: *const Leaves,
    services: *const Services,
    world: *const World,
    native: *const TownHandle,
    town: u16,
    tile: u32,
    name: u32,
    size: u8,
    city: bool,
    desired_layout: u8,
    manual: bool,
) {
    let mut e = engine(leaves, services);
    e.owner = (e.services.town_owner)(native);
    // SAFETY: Native table remains immutable for this synchronous entry.
    let w = unsafe { *world };
    let owner = e.owner;
    // SAFETY: Immediate original initialization, with no callbacks or borrows.
    unsafe {
        (*owner).simulation.xy = tile;
        (*owner).simulation.num_houses = 0;
        (*owner).simulation.time_until_rebuild = 10;
        update_radius(owner);
        (*owner).growth.flags = 0;
        (*owner).simulation.population = 0;
    }
    let sizes = (w.count_sizes)();
    // SAFETY: Live owner; persistent count storage has no outstanding views.
    unsafe {
        initialize_counts(owner, sizes);
        (*owner).growth.counter = town % 70;
        (*owner).growth.rate = (250 + 1) * 70 - 1;
    }
    (w.show_zone)(native, false);
    (w.insert_spatial)(native);
    match (e.leaves.climate)() {
        1 => {
            if (w.acceptance_cargo)(5) {
                // SAFETY: Immediate live-owner mutation ends before next leaf.
                unsafe {
                    (*owner).simulation.goal[5] = u32::MAX - 1;
                }
            }
        }
        2 => {
            if (w.acceptance_cargo)(5) {
                // SAFETY: Immediate live-owner mutation ends before next leaf.
                unsafe {
                    (*owner).simulation.goal[5] = u32::MAX;
                }
            }
            if (w.acceptance_cargo)(4) {
                // SAFETY: Immediate live-owner mutation ends before next leaf.
                unsafe {
                    (*owner).simulation.goal[4] = u32::MAX;
                }
            }
        }
        _ => {}
    }
    // SAFETY: Original scalar initialization; no callback during owner access.
    unsafe {
        (*owner).growth.funding = 0;
        (*owner).simulation.ratings = [500; 15];
        (*owner).simulation.have_ratings = 0;
        (*owner).simulation.exclusivity = u8::MAX;
        (*owner).simulation.exclusive_counter = 0;
        (*owner).simulation.statues = 0;
    }
    let params = (w.name_params)();
    // SAFETY: Immediate copies; no borrowed view of native name parameters.
    unsafe {
        (*owner).simulation.townnamegrfid = params.grfid;
        (*owner).simulation.townnametype = params.kind;
        (*owner).simulation.townnameparts = name;
    }
    let selected_layout = layout(&e, desired_layout);
    // SAFETY: No callback while the final initialization fields are written.
    unsafe {
        (*owner).simulation.layout = selected_layout;
        (*owner).simulation.larger_town = city;
    }
    let mut x = i32::from(size).wrapping_mul(16).wrapping_add(3);
    if size == 3 {
        x = (e.services.random() & 15) as i32 + 8;
    }
    if city && (!manual || (e.leaves.editor)()) {
        x = x.wrapping_mul(i32::from((w.initial_city_size)()));
    }
    e.change_house_count(x as u32);
    // SAFETY: Pure owner arithmetic, no service or reentry during access.
    unsafe {
        update_radius(owner);
    }
    let mut remaining = x.wrapping_mul(4);
    loop {
        e.grow(u32::from(town), 3);
        remaining = remaining.wrapping_sub(1);
        if remaining == 0 {
            break;
        }
    }
    (w.virt_coord)(native);
    (w.dirty_directory)();
    e.change_house_count(x.wrapping_neg() as u32);
    // SAFETY: Pure owner arithmetic ends before subsequent growth services.
    unsafe {
        update_radius(owner);
    }
    e.update_rate(u32::from(town));
    (e.leaves.update_max_pass)(u32::from(town));
    (w.update_noise)();
}
