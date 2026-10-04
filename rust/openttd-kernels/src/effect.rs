/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Owns every effect controller and the private animation bytes.
#![allow(
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::verbose_bit_mask
)]

#[derive(Default)]
pub struct State {
    animation: u16,
    substate: u8,
}
#[repr(C)]
pub struct View {
    pub x: i32,
    pub y: i32,
    pub z: i32,
    pub sprite: u32,
    pub progress: u8,
    pub spritenum: u8,
    pub subtype: u8,
    pub ambient: u8,
    pub sprite_write: u32,
}
#[repr(C)]
#[derive(Default)]
pub struct Cursor {
    pub phase: u32,
    pub animation: u32,
    pub tile: u32,
    pub random: u32,
}
#[repr(C)]
pub struct Leaves {
    pub industry: extern "C" fn(i32, i32, *mut u32) -> u32,
}
const INITIAL: [u32; 12] = [
    3701, 3079, 3073, 3084, 2040, 3709, 3737, 3725, 1416, 4751, 2040, 2040,
];
const LAST: [u32; 12] = [
    3708, 3083, 3078, 3089, 2044, 3724, 3740, 3736, 0, 0, 2044, 2044,
];
const BULLDOZER: [(usize, u32, u8); 20] = [
    (0, 0, 4),
    (3, 3, 4),
    (2, 2, 7),
    (0, 2, 7),
    (1, 1, 3),
    (2, 2, 7),
    (0, 2, 7),
    (1, 1, 3),
    (2, 2, 7),
    (0, 2, 7),
    (3, 3, 6),
    (2, 2, 6),
    (1, 1, 7),
    (3, 1, 7),
    (0, 0, 3),
    (1, 1, 7),
    (3, 1, 7),
    (0, 0, 3),
    (1, 1, 7),
    (3, 1, 7),
];
const INC: [(i32, i32); 4] = [(-1, 0), (0, 1), (1, 0), (0, -1)];
const BUBBLE_FLOAT_SW: &[(i32, i32, i32, u32)] = &[
    (0, 0, 1, 0),
    (1, 0, 1, 1),
    (0, 0, 1, 0),
    (1, 0, 1, 2),
    (1, 4, 0, 0),
];
const BUBBLE_FLOAT_NE: &[(i32, i32, i32, u32)] = &[
    (0, 0, 1, 0),
    (-1, 0, 1, 1),
    (0, 0, 1, 0),
    (-1, 0, 1, 2),
    (1, 4, 0, 0),
];
const BUBBLE_FLOAT_SE: &[(i32, i32, i32, u32)] = &[
    (0, 0, 1, 0),
    (0, 1, 1, 1),
    (0, 0, 1, 0),
    (0, 1, 1, 2),
    (1, 4, 0, 0),
];
const BUBBLE_FLOAT_NW: &[(i32, i32, i32, u32)] = &[
    (0, 0, 1, 0),
    (0, -1, 1, 1),
    (0, 0, 1, 0),
    (0, -1, 1, 2),
    (1, 4, 0, 0),
];
const BUBBLE_BURST: &[(i32, i32, i32, u32)] = &[
    (0, 0, 1, 2),
    (0, 0, 1, 7),
    (0, 0, 1, 8),
    (0, 0, 1, 9),
    (0, 4, 0, 0),
];
const BUBBLE_ABSORB: &[(i32, i32, i32, u32)] = &[
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (0, 0, 1, 0),
    (0, 0, 1, 2),
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (0, 0, 1, 0),
    (0, 0, 1, 2),
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (0, 0, 1, 0),
    (0, 0, 1, 2),
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (0, 0, 1, 0),
    (0, 0, 1, 2),
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (0, 0, 1, 0),
    (0, 0, 1, 2),
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (0, 0, 1, 0),
    (0, 0, 1, 2),
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (0, 0, 1, 0),
    (0, 0, 1, 2),
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (0, 0, 1, 0),
    (0, 0, 1, 2),
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (0, 0, 1, 0),
    (0, 0, 1, 2),
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (0, 0, 1, 0),
    (0, 0, 1, 2),
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (0, 0, 1, 0),
    (0, 0, 1, 2),
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (0, 0, 1, 0),
    (0, 0, 1, 2),
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (0, 0, 1, 0),
    (0, 0, 1, 2),
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (0, 0, 1, 0),
    (0, 0, 1, 2),
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (0, 0, 1, 0),
    (0, 0, 1, 2),
    (0, 0, 1, 0),
    (0, 0, 1, 1),
    (2, 1, 3, 0),
    (1, 1, 3, 1),
    (2, 1, 3, 0),
    (1, 1, 3, 2),
    (2, 1, 3, 0),
    (1, 1, 3, 1),
    (2, 1, 3, 0),
    (1, 0, 1, 2),
    (0, 0, 1, 0),
    (1, 0, 1, 1),
    (0, 0, 1, 0),
    (1, 0, 1, 2),
    (0, 0, 1, 0),
    (1, 0, 1, 1),
    (0, 0, 1, 0),
    (1, 0, 1, 2),
    (2, 4, 0, 0),
    (0, 0, 0, 10),
    (0, 0, 0, 11),
    (0, 0, 0, 12),
    (0, 0, 0, 13),
    (0, 0, 0, 14),
    (0, 4, 0, 0),
];
const BUBBLES: [&[(i32, i32, i32, u32)]; 6] = [
    BUBBLE_FLOAT_SW,
    BUBBLE_FLOAT_NE,
    BUBBLE_FLOAT_SE,
    BUBBLE_FLOAT_NW,
    BUBBLE_BURST,
    BUBBLE_ABSORB,
];

fn set_sprite(v: &mut View, sprite: u32) {
    v.sprite = sprite;
    v.sprite_write = 2;
}
fn increment(v: &mut View) -> bool {
    if v.sprite == LAST[usize::from(v.subtype)] {
        return false;
    }
    v.sprite = v.sprite.wrapping_add(1);
    v.sprite_write = 1;
    true
}
fn init(s: &mut State, v: &mut View, c: &mut Cursor) -> u8 {
    if v.subtype == 0 && c.phase == 0 {
        c.phase = 10;
        return 7;
    }
    set_sprite(v, INITIAL[usize::from(v.subtype)]);
    v.progress = match v.subtype {
        1 | 4 | 10 | 11 => 12,
        3 => 1,
        _ => 0,
    };
    if v.subtype == 0 {
        set_sprite(v, INITIAL[0] + (c.random & 7));
        v.progress = ((c.random >> 16) & 7) as u8;
    }
    if v.subtype == 8 {
        s.animation = 0;
        s.substate = 0;
    }
    if v.subtype == 9 {
        v.spritenum = 0;
    }
    0
}
// Outcomes: done, viewport+done, viewport+resume, burst sound, success sound,
// animated tile+resume, delete, RNG+resume. All external services run after Rust returns.
fn countdown(s: &mut State) -> u8 {
    s.animation = s.animation.wrapping_sub(1);
    if s.animation == 0 { 6 } else { 0 }
}
fn bubble_finish(s: &mut State, v: &mut View, c: &Cursor) -> u8 {
    s.animation = c.animation as u16;
    let b = BUBBLES[usize::from(v.spritenum) - 1][c.animation as usize];
    v.x = v.x.wrapping_add(b.0);
    v.y = v.y.wrapping_add(b.1);
    v.z = v.z.wrapping_add(b.2);
    set_sprite(v, 4748 + b.3);
    1
}
fn bubble_absorb(s: &mut State, v: &mut View, c: &mut Cursor, leaves: &Leaves) -> u8 {
    if (leaves.industry)(v.x, v.y, &raw mut c.tile) == 2 {
        c.phase = 4;
        return 5;
    }
    bubble_finish(s, v, c)
}
fn bubble_burst(s: &mut State, v: &mut View, c: &mut Cursor, burst: bool) -> u8 {
    if burst {
        v.spritenum = 5;
        if v.ambient != 0 {
            c.animation = 0;
            c.phase = 2;
            return 3;
        }
    }
    c.animation = 0;
    bubble_finish(s, v, c)
}
fn bubble_continue(s: &mut State, v: &mut View, c: &mut Cursor, leaves: &Leaves) -> u8 {
    let b = BUBBLES[usize::from(v.spritenum) - 1][c.animation as usize];
    if b.1 == 4 && b.0 == 0 {
        return 6;
    }
    if b.1 == 4 && b.0 == 1 {
        if v.z <= 180 {
            c.phase = 6;
            return 7;
        }
        return bubble_burst(s, v, c, true);
    }
    if b.1 == 4 && b.0 == 2 {
        c.animation += 1;
        if v.ambient != 0 {
            c.phase = 3;
            return 4;
        }
        return bubble_absorb(s, v, c, leaves);
    }
    bubble_finish(s, v, c)
}
fn bubble(s: &mut State, v: &mut View, c: &mut Cursor, leaves: &Leaves) -> u8 {
    if v.progress & 3 != 0 {
        return 0;
    }
    if v.spritenum == 0 {
        v.sprite = v.sprite.wrapping_add(1);
        v.sprite_write = 1;
        if v.sprite < 4754 {
            return 1;
        }
        if s.substate != 0 {
            c.phase = 5;
            return 7;
        }
        v.spritenum = 6;
        c.animation = 0;
    } else {
        c.animation = u32::from(s.animation) + 1;
    }
    bubble_continue(s, v, c, leaves)
}
#[allow(clippy::too_many_lines)]
fn step(s: &mut State, v: &mut View, c: &mut Cursor, leaves: &Leaves) -> u8 {
    match c.phase {
        1 => return countdown(s),
        2 | 4 => return bubble_finish(s, v, c),
        3 => return bubble_absorb(s, v, c, leaves),
        5 => {
            v.spritenum = (c.random & 3) as u8 + 1;
            c.animation = 0;
            return bubble_continue(s, v, c, leaves);
        }
        6 => return bubble_burst(s, v, c, ((u32::from(c.random as u16) * 96 + 48) >> 16) < 1),
        _ => {}
    }
    // Match the source's promotion/narrowing of each byte/word separately.
    match v.subtype {
        0 => {
            if v.progress > 0 {
                v.progress -= 1;
                return 0;
            }
            if (leaves.industry)(v.x, v.y, &raw mut c.tile) == 0 {
                return 6;
            }
            if !increment(v) {
                set_sprite(v, 3701);
            }
            v.progress = 7;
            1
        }
        3 => {
            if v.progress < 2 {
                v.progress += 1;
                return 0;
            }
            v.progress = 0;
            if increment(v) { 1 } else { 6 }
        }
        _ => {
            v.progress = v.progress.wrapping_add(1);
            match v.subtype {
                1 | 4 | 10 | 11 => {
                    let mut moved = false;
                    if v.progress & (if v.subtype == 1 { 7 } else { 3 }) == 0 {
                        v.z = v.z.wrapping_add(1);
                        moved = true;
                    }
                    if v.progress & 15 == 4 {
                        if !increment(v) {
                            return 6;
                        }
                        moved = true;
                    }
                    u8::from(moved)
                }
                2 => {
                    if v.progress & 3 == 0 {
                        v.z = v.z.wrapping_add(1);
                        return 1;
                    }
                    if v.progress & 7 == 1 {
                        return if increment(v) { 1 } else { 6 };
                    }
                    0
                }
                5 | 7 => {
                    if v.progress & 3 == 0 {
                        if increment(v) { 1 } else { 6 }
                    } else {
                        0
                    }
                }
                6 => {
                    if v.progress & 7 == 0 {
                        if !increment(v) {
                            set_sprite(v, 3737);
                        }
                        c.phase = 1;
                        return 2;
                    }
                    countdown(s)
                }
                8 => {
                    if v.progress & 7 != 0 {
                        return 0;
                    }
                    let b = BULLDOZER[usize::from(s.animation)];
                    set_sprite(v, 1416 + b.1);
                    v.x = v.x.wrapping_add(INC[b.0].0);
                    v.y = v.y.wrapping_add(INC[b.0].1);
                    s.substate = s.substate.wrapping_add(1);
                    if s.substate >= b.2 {
                        s.substate = 0;
                        s.animation = s.animation.wrapping_add(1);
                        if usize::from(s.animation) == BULLDOZER.len() {
                            return 6;
                        }
                    }
                    1
                }
                9 => bubble(s, v, c, leaves),
                _ => {
                    let _ = INITIAL[usize::from(v.subtype)];
                    0
                }
            }
        }
    }
}

/// Allocates zeroed private state for both ordinary construction and indexed loading.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_effect_new() -> *mut State {
    Box::into_raw(Box::default())
}
/// # Safety
/// Destroy exactly once after the shell's final use. No borrow survives return.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_effect_destroy(owner: *mut State) {
    drop(unsafe { Box::from_raw(owner) });
}
/// # Safety
/// Owner is live/aligned; field is 0 animation or 1 substate. Synchronous access
/// is serialized and no callback is active. Original narrowing is preserved.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_effect_set(owner: *mut State, field: u8, value: u16) {
    let s = unsafe { &mut *owner };
    if field == 0 {
        s.animation = value;
    } else {
        s.substate = value as u8;
    }
}
/// # Safety
/// Same live/serialized owner contract as set; no pointer/reference is retained.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_effect_get(owner: *const State, field: u8) -> u16 {
    let s = unsafe { &*owner };
    if field == 0 {
        s.animation
    } else {
        u16::from(s.substate)
    }
}
/// # Safety
/// Owner and disjoint view/cursor/table are live, aligned, initialized and exclusive
/// for this call. Table leaves cannot throw, reenter, allocate or retain pointers.
/// Only private bytes persist; view/cursor are per-invocation copied observations.
/// Returned external actions run after this call; reacquire current shared fields
/// before resuming. No reference crosses an external action. Original subtype,
/// animation-table and position preconditions apply. Panics/OOM abort.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_effect_step(
    owner: *mut State,
    view: *mut View,
    cursor: *mut Cursor,
    leaves: *const Leaves,
    initialize: u8,
) -> u8 {
    let (s, v, c, l) = unsafe { (&mut *owner, &mut *view, &mut *cursor, &*leaves) };
    if initialize != 0 {
        init(s, v, c)
    } else {
        step(s, v, c, l)
    }
}
