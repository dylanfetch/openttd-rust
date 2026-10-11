/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Industry tile construction, builtin animation and tile-loop control.
//! Map writes and resolver callbacks stay synchronous and can reenter. Owner
//! fields are read through short raw accesses, never borrowed across callbacks.
// Retain the original tick masks for source comparison.
#![allow(clippy::verbose_bit_mask)]
use super::{ConstructionServices, GenerationServices, Industry, LifecycleServices, Services};
use std::{ffi::c_void, ptr};

#[repr(C)]
pub struct EffectHandle {
    _private: [u8; 0],
}
#[repr(C)]
pub struct ShellHandle {
    _private: [u8; 0],
}
#[repr(C)]
pub struct TileIndustry {
    pub shell: *mut ShellHandle,
    pub owner: *mut Industry,
}
#[repr(C)]
pub struct TileServices {
    pub gfx: unsafe extern "C" fn(u32) -> u16,
    pub set_gfx: unsafe extern "C" fn(u32, u16),
    pub frame: unsafe extern "C" fn(u32) -> u8,
    pub set_frame: unsafe extern "C" fn(u32, u8),
    pub animation_loop: unsafe extern "C" fn(u32) -> u8,
    pub set_animation_loop: unsafe extern "C" fn(u32, u8),
    pub delete_animated: unsafe extern "C" fn(u32),
    pub tick_counter: unsafe extern "C" fn() -> u64,
    pub ambient: unsafe extern "C" fn() -> bool,
    pub animate_new: unsafe extern "C" fn(u32),
    pub construction_counter: unsafe extern "C" fn(u32) -> u8,
    pub construction_stage: unsafe extern "C" fn(u32) -> u8,
    pub completed: unsafe extern "C" fn(u32) -> bool,
    pub set_completed: unsafe extern "C" fn(u32),
    pub reset_construction: unsafe extern "C" fn(u32),
    pub industry: unsafe extern "C" fn(u32) -> TileIndustry,
    pub industry_index: unsafe extern "C" fn(u32) -> u16,
    pub toffee_offset: unsafe extern "C" fn(u8) -> u8,
    pub on_water: unsafe extern "C" fn(u32) -> bool,
    pub water_loop: unsafe extern "C" fn(u32),
    pub is_industry: unsafe extern "C" fn(u32) -> bool,
    pub transport: unsafe extern "C" fn(u32) -> bool,
    pub anim_production: unsafe extern "C" fn(u16) -> u16,
    pub anim_next: unsafe extern "C" fn(u16) -> u16,
    pub build_oilrig: unsafe extern "C" fn(u32),
    pub pixel_max_z: unsafe extern "C" fn(u32) -> i32,
    pub create_effect: unsafe extern "C" fn(i32, i32, i32, u8) -> *mut EffectHandle,
    pub create_effect_above: unsafe extern "C" fn(i32, i32, i32, u8) -> *mut EffectHandle,
    pub set_effect_substate: unsafe extern "C" fn(*mut EffectHandle, u8),
}
fn chance(s: &Services, a: u32, b: u32) -> bool {
    crate::services::chance16_i(a, b, unsafe { (s.random)() })
}
fn sound(tile: u32, sound: u16, s: &Services, t: &TileServices) {
    if unsafe { (t.ambient)() } {
        unsafe { (s.play_sound)(sound, tile) };
    }
}
fn dirty(tile: u32, l: &LifecycleServices) {
    unsafe { (l.mark_dirty)(tile) };
}
fn chimney(tile: u32, c: &ConstructionServices, t: &TileServices) {
    let map = unsafe { (c.map)() };
    let x = (tile % map.size_x).wrapping_mul(16);
    let y = (tile / map.size_x).wrapping_mul(16);
    let z = unsafe { (t.pixel_max_z)(tile) };
    unsafe {
        (t.create_effect)(
            x.wrapping_add(15) as i32,
            y.wrapping_add(14) as i32,
            z.wrapping_add(59),
            0,
        )
    };
}
fn bigger(
    tile: u32,
    s: &Services,
    c: &ConstructionServices,
    l: &LifecycleServices,
    t: &TileServices,
) {
    let counter = unsafe { (t.construction_counter)(tile) }.wrapping_add(1);
    if counter != 4 {
        unsafe { (c.set_construction_counter)(tile, counter) };
        return;
    }
    let stage = unsafe { (t.construction_stage)(tile) }.wrapping_add(1);
    unsafe {
        (c.set_construction_counter)(tile, 0);
        (c.set_construction_stage)(tile, stage);
        super::openttd_rust_industry_animate_tile(tile, 0, 0, s, l);
        if stage == 3 {
            (t.set_completed)(tile);
        }
    }
    dirty(tile, l);
    if !unsafe { (t.completed)(tile) } {
        return;
    }
    let gfx = unsafe { (t.gfx)(tile) };
    if gfx >= 175 {
        return;
    }
    match gfx {
        8 => chimney(tile, c, t),
        24 => {
            let other = tile.wrapping_add(unsafe { (c.map)() }.size_x);
            if unsafe { (t.is_industry)(other) } && unsafe { (t.gfx)(other) } == 24 {
                let own = unsafe { (t.industry_index)(tile) };
                let other = unsafe { (t.industry_index)(other) };
                if own == other {
                    unsafe { (t.build_oilrig)(tile) };
                }
            }
        }
        143 | 162 | 165 => unsafe {
            (t.set_frame)(tile, 0);
            (t.set_animation_loop)(tile, 0);
        },
        148..=155 => unsafe { (c.add_animated)(tile, false) },
        _ => {}
    }
}
fn mine(tile: u32, s: &Services, l: &LifecycleServices, t: &TileServices) {
    let state = (unsafe { (t.tick_counter)() } & 0x7ff) as i32 - 0x400;
    if state < 0 {
        return;
    }
    if state < 0x1a0 {
        if !(0x20..0x180).contains(&state) {
            let frame = unsafe { (t.frame)(tile) };
            if frame & 0x40 == 0 {
                unsafe { (t.set_frame)(tile, frame | 0x40) };
                sound(tile, 9, s, t);
            }
            if state & 7 != 0 {
                return;
            }
        } else if state & 3 != 0 {
            return;
        }
        let mut frame = unsafe { (t.frame)(tile) }.wrapping_add(1) | 0x40;
        if frame > 0xc2 {
            frame = 0xc0;
        }
        unsafe { (t.set_frame)(tile, frame) };
        dirty(tile, l);
    } else if (0x200..0x3a0).contains(&state) {
        let mask = if !(0x220..0x380).contains(&state) {
            7
        } else {
            3
        };
        if state & mask != 0 {
            return;
        }
        let mut frame = (unsafe { (t.frame)(tile) } & 0xbf).wrapping_sub(1);
        if frame < 0x80 {
            frame = 0x82;
        }
        unsafe { (t.set_frame)(tile, frame) };
        dirty(tile, l);
    }
}
/// # Safety
/// Industry tile; immutable service tables outlive the synchronous invocation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_tile_animate(
    tile: u32,
    s: *const Services,
    c: *const ConstructionServices,
    l: *const LifecycleServices,
    t: *const TileServices,
) {
    let (s, c, l, t) = unsafe { (&*s, &*c, &*l, &*t) };
    let gfx = unsafe { (t.gfx)(tile) };
    if unsafe { (c.animation_enabled)(gfx) } {
        unsafe { (t.animate_new)(tile) };
        return;
    }
    match gfx {
        174 if unsafe { (t.tick_counter)() } & 1 == 0 => {
            let mut frame = unsafe { (t.frame)(tile) }.wrapping_add(1);
            if unsafe { (t.ambient)() } {
                match frame & 7 {
                    2 => unsafe { (s.play_sound)(45, tile) },
                    6 => unsafe { (s.play_sound)(41, tile) },
                    _ => {}
                }
            }
            if frame >= 96 {
                frame = 0;
                unsafe { (t.delete_animated)(tile) };
            }
            unsafe { (t.set_frame)(tile, frame) };
            dirty(tile, l);
        }
        165 if unsafe { (t.tick_counter)() } & 3 == 0 => {
            let mut frame = unsafe { (t.frame)(tile) };
            if unsafe { (t.toffee_offset)(frame) } == 0xff {
                sound(tile, 48, s, t);
            }
            frame = frame.wrapping_add(1);
            if frame >= 70 {
                frame = 0;
                unsafe { (t.delete_animated)(tile) };
            }
            unsafe { (t.set_frame)(tile, frame) };
            dirty(tile, l);
        }
        162 if unsafe { (t.tick_counter)() } & 1 == 0 => {
            let mut frame = unsafe { (t.frame)(tile) }.wrapping_add(1);
            if frame >= 40 {
                frame = 0;
                unsafe { (t.delete_animated)(tile) };
            }
            unsafe { (t.set_frame)(tile, frame) };
            dirty(tile, l);
        }
        10 if unsafe { (t.tick_counter)() } & 3 == 0 => {
            let frame = unsafe { (t.frame)(tile) };
            if frame == 6 {
                unsafe {
                    (t.set_frame)(tile, 0);
                    (t.delete_animated)(tile);
                }
            } else {
                unsafe { (t.set_frame)(tile, frame.wrapping_add(1)) };
            }
            dirty(tile, l);
        }
        143 if unsafe { (t.tick_counter)() } & 1 == 0 => {
            let mut frame = unsafe { (t.frame)(tile) }.wrapping_add(1);
            match frame {
                1 => sound(tile, 44, s, t),
                23 => sound(tile, 43, s, t),
                28 => sound(tile, 42, s, t),
                _ => {
                    if frame >= 50 {
                        let mut count =
                            u32::from(unsafe { (t.animation_loop)(tile) }).wrapping_add(1);
                        frame = 0;
                        if count >= 8 {
                            count = 0;
                            unsafe { (t.delete_animated)(tile) };
                        }
                        unsafe { (t.set_animation_loop)(tile, count as u8) };
                    }
                }
            }
            unsafe { (t.set_frame)(tile, frame) };
            dirty(tile, l);
        }
        148..=155 if unsafe { (t.tick_counter)() } & 3 == 0 => {
            unsafe { (t.set_gfx)(tile, if gfx < 155 { gfx.wrapping_add(1) } else { 148 }) };
            dirty(tile, l);
        }
        30..=32 if unsafe { (t.tick_counter)() } & 7 == 0 => {
            let stop = chance(s, 1, 7);
            let mut frame = unsafe { (t.frame)(tile) }.wrapping_add(1);
            let mut gfx = gfx;
            let last = if frame == 4 {
                frame = 0;
                gfx = gfx.wrapping_add(1);
                if gfx == 33 {
                    gfx = 30;
                    true
                } else {
                    false
                }
            } else {
                false
            };
            if last && stop {
                unsafe {
                    (t.set_gfx)(tile, 29);
                    (c.set_construction_stage)(tile, 3);
                    (t.delete_animated)(tile);
                }
            } else {
                unsafe {
                    (t.set_frame)(tile, frame);
                    (t.set_gfx)(tile, gfx);
                }
            }
            dirty(tile, l);
        }
        1 | 48 | 88 => mine(tile, s, l, t),
        _ => {}
    }
}
fn bubble(tile: u32, s: &Services, c: &ConstructionServices, t: &TileServices) {
    sound(tile, 46, s, t);
    let dir = (unsafe { (s.random)() } & 3) as usize;
    let map = unsafe { (c.map)() };
    let x = (tile % map.size_x)
        .wrapping_mul(16)
        .wrapping_add([11_i32, 0, -4, -14][dir] as u32) as i32;
    let y = (tile / map.size_x)
        .wrapping_mul(16)
        .wrapping_add([-4_i32, -10, -4, 1][dir] as u32) as i32;
    let vehicle = unsafe { (t.create_effect_above)(x, y, [49, 59, 60, 65][dir], 9) };
    if !vehicle.is_null() {
        unsafe { (t.set_effect_substate)(vehicle, dir as u8) };
    }
}
/// # Safety
/// Live map tile. Water can destroy its industry, so no owner is resolved until
/// the source's post-water type recheck. Later callbacks retain no Rust borrow.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_tile_loop(
    tile: u32,
    s: *const Services,
    c: *const ConstructionServices,
    l: *const LifecycleServices,
    t: *const TileServices,
    g: *const GenerationServices,
) {
    let (s, c, l, t) = unsafe { (&*s, &*c, &*l, &*t) };
    if unsafe { (t.on_water)(tile) } {
        unsafe { (t.water_loop)(tile) };
    }
    if !unsafe { (t.is_industry)(tile) } {
        return;
    }
    let industry = unsafe { (t.industry)(tile) };
    unsafe {
        super::openttd_rust_industry_randomise_tile(
            tile,
            0,
            industry.shell.cast::<c_void>(),
            industry.owner,
            s,
            l,
        );
    };
    if !unsafe { (t.completed)(tile) } {
        bigger(tile, s, c, l, t);
        return;
    }
    if unsafe { ((*g).editor)() } {
        return;
    }
    if unsafe { (t.transport)(tile) } {
        let industry = unsafe { (t.industry)(tile) };
        if !unsafe {
            super::openttd_rust_industry_animate(
                industry.owner,
                (t.industry_index)(tile),
                4,
                (c.map)().size_x,
                s,
                l,
            )
        } {
            let gfx = unsafe { (t.gfx)(tile) };
            let next = unsafe { (t.anim_production)(gfx) };
            if next != 255 {
                unsafe {
                    (t.reset_construction)(tile);
                    (t.set_completed)(tile);
                    (t.set_gfx)(tile, next);
                }
                dirty(tile, l);
                return;
            }
        }
    }
    if unsafe { super::openttd_rust_industry_animate_tile(tile, 1, 0, s, l) } {
        return;
    }
    let gfx = unsafe { (t.gfx)(tile) };
    let next = unsafe { (t.anim_next)(gfx) };
    if next != 255 {
        unsafe {
            (t.reset_construction)(tile);
            (t.set_gfx)(tile, next);
        };
        dirty(tile, l);
        return;
    }
    let mut gfx = unsafe { (t.gfx)(tile) };
    match gfx {
        0 | 47 | 79 => {
            if unsafe { (t.tick_counter)() } & 0x400 == 0 && chance(s, 1, 2) {
                gfx = match gfx {
                    0 => 1,
                    47 => 48,
                    _ => 88,
                };
                unsafe {
                    (t.set_gfx)(tile, gfx);
                    (t.set_frame)(tile, 0x80);
                    (c.add_animated)(tile, true);
                }
            }
        }
        29 => {
            if chance(s, 1, 6) {
                unsafe {
                    (t.set_gfx)(tile, 30);
                    (t.set_frame)(tile, 0);
                    (c.add_animated)(tile, true);
                }
            }
        }
        1 | 48 | 88 => {
            if unsafe { (t.tick_counter)() } & 0x400 == 0 {
                gfx = match gfx {
                    1 => 0,
                    48 => 47,
                    _ => 79,
                };
                unsafe {
                    (t.set_gfx)(tile, gfx);
                    (t.set_completed)(tile);
                    (c.set_construction_stage)(tile, 3);
                    (t.delete_animated)(tile);
                }
                dirty(tile, l);
            }
        }
        10 => {
            if chance(s, 1, 3) {
                sound(tile, 10, s, t);
                unsafe { (c.add_animated)(tile, true) };
            }
        }
        49 => {
            let map = unsafe { (c.map)() };
            unsafe {
                (t.create_effect_above)(
                    (tile % map.size_x).wrapping_mul(16).wrapping_add(6) as i32,
                    (tile / map.size_x).wrapping_mul(16).wrapping_add(6) as i32,
                    43,
                    11,
                )
            };
        }
        143 => {
            let industry = unsafe { (t.industry)(tile) };
            let delivered =
                unsafe { ptr::read(ptr::addr_of!((*industry.owner).fields.was_cargo_delivered)) };
            if delivered != 0 {
                unsafe {
                    (*industry.owner).fields.was_cargo_delivered = 0;
                    (t.set_animation_loop)(tile, 0);
                    (c.add_animated)(tile, true);
                }
            }
        }
        161 => bubble(tile, s, c, t),
        165 => unsafe { (c.add_animated)(tile, true) },
        174 if chance(s, 1, 3) => {
            unsafe { (c.add_animated)(tile, true) };
        }
        _ => {}
    }
}
