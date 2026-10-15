/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Ground evolution for clear, railway and tunnel/bridge tiles. Native storage is
//! accessed through consumed-field records; no map or owner borrow spans calls.
//! Entries are synchronous and serialized by the game-state mutex. Native leaves
//! are noexcept and all Rust panics abort, including impossible original states.
#![allow(
    unsafe_code,
    missing_docs,
    clippy::too_many_lines,
    clippy::cast_possible_truncation,
    clippy::cast_sign_loss,
    clippy::if_not_else
)]
use crate::services::Services;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ClearGround {
    pub ground: u8,
    pub density: u8,
    pub counter: u8,
    pub snow: bool,
}
#[repr(C)]
pub struct FieldGround {
    pub industry: u16,
    pub field_type: u8,
}
#[repr(C)]
pub struct ClearServices {
    pub observe: extern "C" fn(u32) -> ClearGround,
    pub set_ground: extern "C" fn(u32, u8, u32),
    pub set_density: extern "C" fn(u32, u32),
    pub set_counter: extern "C" fn(u32, u32),
    pub clear_snow: extern "C" fn(u32),
    pub make_snow: extern "C" fn(u32, u32),
    pub make_clear: extern "C" fn(u32, u8, u32),
    pub fence: extern "C" fn(u32, u8) -> u32,
    pub set_fence: extern "C" fn(u32, u8, u32),
    pub field: extern "C" fn(u32) -> FieldGround,
    pub set_field: extern "C" fn(u32, u32),
    pub has_water_class: extern "C" fn(u32) -> bool,
    pub water_class: extern "C" fn(u32) -> u8,
    pub ambient: extern "C" fn(u32) -> bool,
}
#[repr(C)]
pub struct RailSnowGround {
    pub z: i32,
    pub slope: u8,
    pub tracks: u8,
    pub plain: bool,
}
#[repr(C)]
pub struct RailFenceGround {
    pub tracks: u8,
    pub owner: u8,
    pub plain: bool,
}
#[repr(C)]
pub struct RailServices {
    pub ground: extern "C" fn(u32) -> u8,
    pub set_ground: extern "C" fn(u32, u8),
    pub snow: extern "C" fn(u32) -> RailSnowGround,
    pub fences: extern "C" fn(u32) -> RailFenceGround,
    pub foundation: extern "C" fn(u8, u8) -> u8,
    pub owned_land: extern "C" fn(u32) -> bool,
    pub water_loop: extern "C" fn(u32),
}
#[repr(C)]
pub struct TunnelGround {
    pub z: i32,
    pub snow: bool,
}
#[repr(C)]
pub struct TunnelServices {
    pub snow: extern "C" fn(u32) -> TunnelGround,
    pub snow_or_desert: extern "C" fn(u32) -> bool,
    pub set_snow_or_desert: extern "C" fn(u32, bool),
}
fn diagonal(tile: u32, direction: u8, size_x: u32) -> u32 {
    match direction {
        0 => tile.wrapping_sub(1),
        1 => tile.wrapping_add(size_x),
        2 => tile.wrapping_add(1),
        _ => tile.wrapping_sub(size_x),
    }
}
fn three_corners(slope: u8) -> bool {
    matches!(slope, 7 | 11 | 13 | 14)
}
fn steep(slope: u8) -> bool {
    slope & 16 != 0
}

/// # Safety
/// Tables are live for this serialized synchronous entry; callbacks do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_clear_tile_loop(
    tile: u32,
    climate: u8,
    editor: bool,
    size_x: u32,
    clear: *const ClearServices,
    shared: *const Services,
) {
    // SAFETY: Caller supplies immutable live tables. No state borrow crosses a call.
    let (c, s) = unsafe { (&*clear, &*shared) };
    (c.ambient)(tile);
    let mut ground = (c.observe)(tile);
    if climate == 1 {
        let k = (s.tile_z)(tile) - i32::from((s.snow_line)()) + 1;
        let mut changed = false;
        if !ground.snow {
            if k >= 0 {
                (c.make_snow)(tile, 0);
                changed = true;
            }
        } else {
            let required = k.clamp(0, 3) as u8;
            if ground.density == required {
                if k < 0 {
                    (c.clear_snow)(tile);
                    changed = true;
                }
            } else {
                (c.set_density)(
                    tile,
                    u32::from(if ground.density < required {
                        ground.density + 1
                    } else {
                        ground.density - 1
                    }),
                );
                changed = true;
            }
        }
        if changed {
            (s.mark_dirty)(tile);
            ground = (c.observe)(tile);
        }
    } else if climate == 2 {
        let current = if ground.ground == 5 {
            ground.density
        } else {
            0
        };
        let mut expected = 0;
        if (s.tropic_zone)(tile) == 1 {
            let normal = (0..4).any(|direction| {
                let t = diagonal(tile, direction, size_x);
                (s.tile_valid)(t)
                    && ((s.tropic_zone)(t) != 1
                        || ((c.has_water_class)(t) && (c.water_class)(t) == 0))
            });
            expected = if normal { 1 } else { 3 };
        }
        if current != expected {
            (c.set_ground)(
                tile,
                if expected == 0 { 0 } else { 5 },
                if expected == 0 {
                    3
                } else {
                    u32::from(expected)
                },
            );
            (s.mark_dirty)(tile);
            ground = (c.observe)(tile);
        }
    }
    if ground.snow {
        return;
    }
    match ground.ground {
        0 => {
            if ground.density == 3 {
                return;
            }
            if editor {
                (c.set_ground)(tile, u8::from((s.random)(s.context) & 255 <= 21), 3);
            } else if ground.counter < 7 {
                (c.set_counter)(tile, u32::from(ground.counter) + 1);
                return;
            } else {
                (c.set_counter)(tile, 0);
                (c.set_density)(tile, u32::from(ground.density) + 1);
            }
        }
        3 => {
            let mut dirty = false;
            for direction in 0..4 {
                if (c.fence)(tile, direction) != 0 {
                    continue;
                }
                let neighbour = diagonal(tile, direction, size_x);
                if (s.tile_type)(neighbour) == 0 && (c.observe)(neighbour).ground == 3 {
                    continue;
                }
                (c.set_fence)(tile, direction, 3);
                dirty = true;
            }
            if dirty {
                (s.mark_dirty)(tile);
            }
            if editor {
                return;
            }
            if ground.counter < 7 {
                (c.set_counter)(tile, u32::from(ground.counter) + 1);
                return;
            }
            (c.set_counter)(tile, 0);
            let field = (c.field)(tile);
            if field.industry == u16::MAX && field.field_type >= 7 {
                (c.make_clear)(tile, 0, 2);
            } else {
                (c.set_field)(
                    tile,
                    if field.field_type < 8 {
                        u32::from(field.field_type) + 1
                    } else {
                        0
                    },
                );
            }
        }
        _ => return,
    }
    (s.mark_dirty)(tile);
}

/// # Safety
/// Tables are live for this serialized synchronous entry; callbacks do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_rail_tile_loop(
    tile: u32,
    climate: u8,
    size_x: u32,
    rail: *const RailServices,
    shared: *const Services,
) {
    // SAFETY: Caller supplies immutable live tables; observations are copied.
    let (r, s) = unsafe { (&*rail, &*shared) };
    let old = (r.ground)(tile);
    if old == 13 {
        (r.water_loop)(tile);
        return;
    }
    let mut new = None;
    if climate == 1 {
        let mut ground = (r.snow)(tile);
        let mut half = false;
        if ground.plain {
            let foundation = (r.foundation)(ground.slope, ground.tracks);
            match foundation {
                0 => {
                    if three_corners(ground.slope) {
                        ground.z += 1;
                    }
                }
                2 | 3 => {
                    if steep(ground.slope) {
                        ground.z += 1;
                    }
                }
                4 => ground.z += 1,
                _ => {
                    if steep(ground.slope) {
                        ground.z += 1;
                    }
                    ground.z += 1;
                }
            }
            half = (5..10).contains(&foundation);
        } else if ground.slope != 0 {
            ground.z += 1;
        }
        let snow = i32::from((s.snow_line)());
        if ground.z > snow {
            new = Some(if half && ground.z - snow == 1 { 14 } else { 12 });
        }
    } else if climate == 2 && (s.tropic_zone)(tile) == 1 {
        new = Some(12);
    }
    let new = new.unwrap_or_else(|| {
        if old == 0 {
            return 1;
        }
        let ground = (r.fences)(tile);
        if !ground.plain {
            return 1;
        }
        let mut fences = 0_u8;
        for (direction, tracks) in [37, 42, 25, 22].into_iter().enumerate() {
            if ground.tracks & tracks != 0 {
                continue;
            }
            let neighbour = diagonal(tile, direction as u8, size_x);
            let fence = if (s.tile_valid)(neighbour) {
                let kind = (s.tile_type)(neighbour);
                matches!(kind, 2 | 3 | 8 | 9)
                    || (kind == 10 && !(r.owned_land)(neighbour))
                    || (s.tile_owner)(neighbour) != ground.owner
            } else {
                true
            };
            if fence {
                fences |= 1 << direction;
            }
        }
        match fences {
            0 => 1,
            1 => 5,
            2 => 3,
            4 => 6,
            8 => 2,
            5 => 7,
            10 => 4,
            3 => 8,
            9 => 11,
            6 => 10,
            12 => 9,
            _ => unreachable!("original rail fence mask"),
        }
    });
    if old != new {
        (r.set_ground)(tile, new);
        (s.mark_dirty)(tile);
    }
}

/// # Safety
/// Tables are live for this serialized synchronous entry; callbacks do not unwind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_tunnel_tile_loop(
    tile: u32,
    climate: u8,
    tunnel: *const TunnelServices,
    shared: *const Services,
) {
    // SAFETY: Caller supplies immutable live tables; no mutable borrow retained.
    let (t, s) = unsafe { (&*tunnel, &*shared) };
    if climate == 1 {
        let ground = (t.snow)(tile);
        let snow = ground.z > i32::from((s.snow_line)());
        if ground.snow != snow {
            (t.set_snow_or_desert)(tile, !ground.snow);
            (s.mark_dirty)(tile);
        }
    } else if climate == 2 && (s.tropic_zone)(tile) == 1 && !(t.snow_or_desert)(tile) {
        (t.set_snow_or_desert)(tile, true);
        (s.mark_dirty)(tile);
    }
}
