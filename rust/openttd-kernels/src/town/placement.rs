/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Town-site eligibility, grid alignment and coastal search control.
use super::{Engine, Leaves, engine, position};
use crate::services::{CommandCost, CommandCostServices, Services, TownHandle, scale_by_size};

#[repr(C)]
#[derive(Clone, Copy)]
pub struct World {
    pub spatial_empty: extern "C" fn() -> bool,
    pub nearest_town: extern "C" fn(u32) -> *const TownHandle,
    pub clear_ground: extern "C" fn(u32) -> u8,
    pub tree_ground: extern "C" fn(u32) -> u8,
    pub water_distance: extern "C" fn(u32, bool) -> u32,
    pub number_towns: extern "C" fn() -> u8,
    pub custom_towns: extern "C" fn() -> u16,
    pub edge_error: u32,
    pub town_error: u32,
    pub site_error: u32,
}
fn close_to_town(e: &Engine, w: &World, tile: u32, distance: u32) -> bool {
    if (w.spatial_empty)() {
        return false;
    }
    let native = (w.nearest_town)(tile);
    let owner = (e.services.town_owner)(native);
    // SAFETY: Native index returns the same live owner as source Town::Get.
    // The copied position access ends before subsequent map or pool calls.
    let xy = unsafe { position(owner) };
    let width = e.size_x();
    (tile & (width - 1))
        .abs_diff(xy & (width - 1))
        .wrapping_add((tile / width).abs_diff(xy / width))
        < distance
}
pub(super) fn site_error(e: &Engine, w: &World, tile: u32, surrounding: bool) -> Option<u32> {
    if (e.services.distance_from_edge)(tile) < 12 {
        return Some(w.edge_error);
    }
    if close_to_town(e, w, tile, 20) {
        return Some(w.town_error);
    }
    let kind = (e.services.tile_type)(tile);
    if (kind != 0 && kind != 4) || (e.services.tile_slope)(tile) != 0 {
        return Some(w.site_error);
    }
    if surrounding {
        let mut counter = 0;
        let height = (e.services.tile_z)(tile);
        let width = e.size_x();
        let size_y = (e.leaves.map_size_y)();
        let mut sequence =
            crate::spiral::square(tile & (width - 1), tile / width, 5, width, size_y);
        while !sequence.is_end() {
            if counter == 12 {
                break;
            }
            let at = sequence.y.wrapping_mul(width).wrapping_add(sequence.x);
            match (e.services.tile_type)(at) {
                0 => {
                    if (w.clear_ground)(at) == 1 {
                        sequence = sequence.advance(width, size_y);
                        continue;
                    }
                }
                4 => {
                    if (w.tree_ground)(at) == 1 {
                        sequence = sequence.advance(width, size_y);
                        continue;
                    }
                }
                _ => {
                    sequence = sequence.advance(width, size_y);
                    continue;
                }
            }
            if (e.services.tile_max_z)(at) <= height.wrapping_add(1)
                && (e.services.tile_z)(at) >= height.wrapping_sub(1)
            {
                counter += 1;
            }
            sequence = sequence.advance(width, size_y);
        }
        if counter < 12 {
            return Some(w.site_error);
        }
    }
    None
}
pub(super) fn align(e: &Engine, tile: u32, layout: u8) -> u32 {
    if layout != 2 && layout != 3 {
        return tile;
    }
    let width = e.size_x();
    let (mut x, mut y) = (tile & (width - 1), tile / width);
    if layout == 2 {
        x -= x % 3;
        y -= y % 3;
    } else {
        x &= !3;
        y &= !3;
    }
    y.wrapping_mul(width).wrapping_add(x)
}
fn aligned(e: &Engine, tile: u32, layout: u8) -> bool {
    let divisor = match layout {
        2 => 3,
        3 => 4,
        _ => return true,
    };
    let width = e.size_x();
    (tile & (width - 1)).is_multiple_of(divisor) && (tile / width).is_multiple_of(divisor)
}
pub(super) fn coastal(e: &Engine, w: &World, tile: u32, layout: u8) -> u32 {
    let width = e.size_x();
    let size_y = (e.leaves.map_size_y)();
    let mut coast = crate::spiral::square(tile & (width - 1), tile / width, 40, width, size_y);
    while !coast.is_end() {
        let at = coast.y.wrapping_mul(width).wrapping_add(coast.x);
        if (e.services.tile_type)(at) != 0 {
            coast = coast.advance(width, size_y);
            continue;
        }
        let mut furthest = u32::MAX;
        let mut max_distance = 0;
        let mut test = crate::spiral::square(coast.x, coast.y, 10, width, size_y);
        while !test.is_end() {
            let at = test.y.wrapping_mul(width).wrapping_add(test.x);
            if (e.services.tile_type)(at) == 0
                && (e.services.tile_slope)(at) == 0
                && aligned(e, at, layout)
                && site_error(e, w, at, true).is_none()
            {
                let distance = (w.water_distance)(at, true);
                if distance > max_distance {
                    furthest = at;
                    max_distance = distance;
                }
            }
            test = test.advance(width, size_y);
        }
        return furthest;
    }
    u32::MAX
}
pub(super) fn default_towns(e: &Engine, w: &World) -> u32 {
    let number = (w.number_towns)();
    if number == 4 {
        return u32::from((w.custom_towns)());
    }
    let size = e.size_x().wrapping_mul((e.leaves.map_size_y)());
    scale_by_size(size, [5, 11, 23, 46][usize::from(number)])
}
/// Original native stack result; the site loop retains no owner/map borrow.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_site(
    leaves: *const Leaves,
    services: *const Services,
    world: *const World,
    costs: *const CommandCostServices,
    result: *mut CommandCost,
    tile: u32,
    surrounding: bool,
) {
    let e = engine(leaves, services);
    // SAFETY: Immutable native tables and the stack result remain live.
    let (w, c) = unsafe { (*world, *costs) };
    if let Some(error) = site_error(&e, &w, tile, surrounding) {
        (c.error)(result, error);
    } else {
        (c.expense_money)(result, 12, 0);
    }
}
/// Pure map arithmetic; all dimensions stay valid throughout this entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_align(
    leaves: *const Leaves,
    services: *const Services,
    tile: u32,
    layout: u8,
) -> u32 {
    align(&engine(leaves, services), tile, layout)
}
/// Complete nested source spiral search, including the first-coast early return.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_coastal(
    leaves: *const Leaves,
    services: *const Services,
    world: *const World,
    tile: u32,
    layout: u8,
) -> u32 {
    // SAFETY: Immutable native table outlives this synchronous call.
    coastal(&engine(leaves, services), &unsafe { *world }, tile, layout)
}
/// Settings are observed only along the selected original branch.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_town_default_number(
    leaves: *const Leaves,
    services: *const Services,
    world: *const World,
) -> u32 {
    // SAFETY: Immutable native table outlives this synchronous call.
    default_towns(&engine(leaves, services), &unsafe { *world })
}
