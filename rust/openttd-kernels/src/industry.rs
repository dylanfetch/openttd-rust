/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Canonical industry cargo/history and production/builder control.
//!
//! The C++ shell owns one Box handle. Its typed views alias only field-sized raw
//! accesses here. No reference into an owner survives a world service: construction,
//! destruction, cargo distribution and `NewGRF` resolution may revisit live state.
//! Slot growth invalidates views exactly as the original vector; optional history
//! allocations remain stable until slot removal. Panics abort; no unwind crosses FFI.
// Original fixed-width conversion and variable names are retained for audit.
#![allow(
    unsafe_code,
    clippy::cast_possible_truncation,
    clippy::cast_sign_loss,
    clippy::cast_possible_wrap,
    clippy::many_single_char_names,
    clippy::similar_names,
    clippy::struct_field_names,
    clippy::too_many_lines,
    clippy::if_not_else
)]
use std::ffi::c_void;
use std::ptr;
const RECORDS: usize = 61;
const TYPES: usize = 240;
#[repr(C)]
#[derive(Default, Clone, Copy)]
pub struct Pair {
    pub first: u16,
    pub second: u16,
}
#[repr(C)]
#[derive(Clone)]
pub struct Produced {
    pub cargo: u8,
    pub waiting: u16,
    pub rate: u8,
    pub history: [Pair; RECORDS],
}
impl Default for Produced {
    fn default() -> Self {
        Self {
            cargo: 0,
            waiting: 0,
            rate: 0,
            history: [Pair::default(); RECORDS],
        }
    }
}
#[repr(C)]
pub struct Accepted {
    pub cargo: u8,
    pub waiting: u16,
    pub accumulated_waiting: u32,
    pub last_accepted: i32,
    pub history: *mut [Pair; RECORDS],
}
impl Default for Accepted {
    fn default() -> Self {
        Self {
            cargo: 0,
            waiting: 0,
            accumulated_waiting: 0,
            last_accepted: 0,
            history: ptr::null_mut(),
        }
    }
}
impl Drop for Accepted {
    fn drop(&mut self) {
        if !self.history.is_null() {
            unsafe {
                drop(Box::from_raw(self.history));
            }
        }
    }
}
#[repr(C)]
#[derive(Default, Clone, Copy)]
pub struct Fields {
    pub valid_history: u64,
    pub last_prod_year: i32,
    pub counter: u16,
    pub prod_level: u8,
    pub was_cargo_delivered: u8,
    pub ctlflags: u8,
}
#[derive(Default)]
pub struct Industry {
    fields: Fields,
    produced: Vec<Produced>,
    accepted: Vec<Accepted>,
}
#[repr(C)]
#[derive(Default, Clone, Copy)]
pub struct BuildFields {
    pub probability: u32,
    pub min_number: u8,
    pub target_count: u16,
    pub max_wait: u16,
    pub wait_count: u16,
}
#[repr(C)]
pub struct BuilderFields {
    pub builddata: [BuildFields; TYPES],
    pub wanted_inds: u32,
    pub daily_counter: u32,
    pub daily_increment: u32,
    pub sound_tile: u32,
    pub sound_ctr: u8,
}
pub struct Builder {
    fields: BuilderFields,
}
#[repr(C)]
pub struct Slots {
    pub data: *mut c_void,
    pub size: usize,
}
/// Allocate a canonical owner. Allocation failure aborts.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_industry_new() -> *mut Industry {
    Box::into_raw(Box::default())
}
/// # Safety
/// Handle is live and uniquely owned; all C++ slot/history/scalar views have ended.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_destroy(owner: *mut Industry) {
    unsafe {
        drop(Box::from_raw(owner));
    }
}
/// # Safety
/// Live shell handle. Returned fields are nonowning; no concurrent accesses.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_fields(owner: *mut Industry) -> *mut Fields {
    unsafe { ptr::addr_of_mut!((*owner).fields) }
}
/// # Safety
/// Live handle; any vector mutation invalidates outstanding views. Only the
/// addressed vector is borrowed during its mutation; no callback runs here.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_slots(
    owner: *mut Industry,
    produced: u8,
    operation: u8,
    count: usize,
) -> Slots {
    fn edit<T: Default>(v: &mut Vec<T>, operation: u8, count: usize) -> Slots {
        match operation {
            0 => {}
            1 => {
                if count > v.len() {
                    v.reserve(count - v.len());
                }
            }
            2 => v.resize_with(count, T::default),
            3 => v.push(T::default()),
            4 => v.shrink_to_fit(),
            _ => unreachable!(),
        }
        Slots {
            data: v.as_mut_ptr().cast(),
            size: v.len(),
        }
    }
    if produced != 0 {
        unsafe { edit(&mut (*owner).produced, operation, count) }
    } else {
        unsafe { edit(&mut (*owner).accepted, operation, count) }
    }
}
/// # Safety
/// Live accepted slot with canonical layout. Existing history remains stable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_history(accepted: *mut Accepted) -> *mut c_void {
    unsafe {
        if (*accepted).history.is_null() {
            (*accepted).history = Box::into_raw(Box::new([Pair::default(); RECORDS]));
        }
        (*accepted).history.cast()
    }
}
/// Allocate the single builder owner for its C++ shell.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_industry_builder_new() -> *mut Builder {
    Box::into_raw(Box::new(Builder {
        fields: BuilderFields {
            builddata: [BuildFields::default(); TYPES],
            wanted_inds: 0,
            daily_counter: 0,
            daily_increment: 0,
            sound_tile: 0,
            sound_ctr: 0,
        },
    }))
}
/// # Safety
/// Unique live builder handle; all nonowning views have ended.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_builder_destroy(owner: *mut Builder) {
    unsafe {
        drop(Box::from_raw(owner));
    }
}
/// # Safety
/// Live builder shell, serial access only. No allocation or callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_builder_fields(
    owner: *mut Builder,
) -> *mut BuilderFields {
    unsafe { ptr::addr_of_mut!((*owner).fields) }
}

#[repr(C)]
#[derive(Default, Clone, Copy)]
pub struct Observation {
    pub owner: *mut Industry,
    pub tile: u32,
    pub behaviour: u32,
    pub id: u16,
    pub width: u16,
    pub height: u16,
    pub callbacks: u16,
    pub sound_count: u16,
    pub life: u8,
    pub original: u8,
    pub minimal_cargo: u8,
    pub kind: u8,
    pub up_text: u32,
    pub down_text: u32,
    pub closure_text: u32,
}
#[repr(C)]
pub struct Services {
    pub observe: unsafe extern "C" fn(*mut c_void, *mut Observation),
    pub next: unsafe extern "C" fn(u32) -> *mut c_void,
    pub setting: unsafe extern "C" fn(u8) -> u32,
    pub world: unsafe extern "C" fn(u8, *mut c_void, u32, u32, u32) -> u64,
}
// Named operations shared with industry_cmd.cpp. They are world leaves, not
// continuations: construction and commands return values at the source call site.
const RANDOM: u8 = 0;
const RANGE: u8 = 1;
const SCALE: u8 = 2;
const INVERSE: u8 = 3;
const SOUND: u8 = 4;
const SPECIAL: u8 = 5;
const TRIGGER: u8 = 6;
const PRODUCTION: u8 = 7;
const MOVE: u8 = 8;
const RATE: u8 = 9;
const TYPE_COUNT: u8 = 11;
const TOTAL: u8 = 12;
const MAP_SCALE: u8 = 13;
const CREATE: u8 = 14;
const ADVERTISE: u8 = 15;
const RANDOM_INDUSTRY: u8 = 16;
const DESTROY: u8 = 17;
const DIRTY: u8 = 18;
const DIRECTORY: u8 = 19;
const COMPANY: u8 = 20;
const CHANGE_CALLBACK: u8 = 21;
const CUSTOM_TEXT: u8 = 22;
const NEWS: u8 = 23;
const RATE_NEWS: u8 = 24;
const MAP_TILE: u8 = 25;
const FIELD_WRITE: u8 = 26;
const FENCE: u8 = 27;
const COMPLETED: u8 = 28;
const HARVEST: u8 = 29;
const CALLBACK_ERROR: u8 = 30;
const ENABLED: u8 = 31;
const PROB_CALLBACK: u8 = 32;
fn world(s: &Services, op: u8, i: *mut c_void, a: u32, b: u32, c: u32) -> u64 {
    unsafe { (s.world)(op, i, a, b, c) }
}
fn setting(s: &Services, n: u8) -> u32 {
    unsafe { (s.setting)(n) }
}
fn observe(s: &Services, i: *mut c_void) -> Observation {
    let mut o = Observation::default();
    unsafe {
        (s.observe)(i, &raw mut o);
    }
    o
}
fn rand(s: &Services) -> u32 {
    world(s, RANDOM, ptr::null_mut(), 0, 0, 0) as u32
}
fn range(s: &Services, n: u32) -> u32 {
    world(s, RANGE, ptr::null_mut(), n, 0, 0) as u32
}
fn chance_value(a: u32, b: u32, value: u32) -> bool {
    (((value & 0xffff).wrapping_mul(b).wrapping_add(b / 2)) >> 16) < a
}
fn chance(s: &Services, a: u32, b: u32) -> bool {
    chance_value(a, b, rand(s))
}
fn fields(o: Observation) -> *mut Fields {
    unsafe { ptr::addr_of_mut!((*o.owner).fields) }
}
fn produced(o: Observation, slot: usize) -> *mut Produced {
    unsafe { (*o.owner).produced.as_mut_ptr().add(slot) }
}
fn accepted(o: Observation, slot: usize) -> *mut Accepted {
    unsafe { (*o.owner).accepted.as_mut_ptr().add(slot) }
}
fn produced_count(o: Observation) -> usize {
    unsafe { (*o.owner).produced.len() }
}
fn accepted_count(o: Observation) -> usize {
    unsafe { (*o.owner).accepted.len() }
}
fn valid(cargo: u8) -> bool {
    cargo != 255
}
fn transported(p: Pair) -> u32 {
    if p.first == 0 {
        0
    } else {
        (u32::from(p.second) * 256 / u32::from(p.first)).min(255)
    }
}
fn helper(i: *mut c_void, s: &Services, scale: bool) {
    let o = observe(s, i);
    for n in 0..produced_count(o) {
        let p = produced(o, n);
        unsafe {
            if !valid((*p).cargo) {
                continue;
            }
        }
        let mut amount = unsafe { u32::from((*p).rate) };
        if scale {
            amount = u32::from(world(s, SCALE, i, amount, 0, 0) as u16);
        }
        unsafe {
            (*p).waiting = (u32::from((*p).waiting) + amount).min(65535) as u16;
        }
    }
}
fn produce(i: *mut c_void, s: &Services, builder: *mut Builder) {
    let o = observe(s, i);
    let f = fields(o);
    if unsafe { (*f).counter.is_multiple_of(64) } {
        let r = rand(s);
        if chance_value(1, 14, r)
            && o.sound_count != 0
            && setting(s, 0) != 0
            && (0..produced_count(o)).any(|n| unsafe { (*produced(o, n)).history[1].first > 0 })
        {
            world(
                s,
                SOUND,
                i,
                0,
                ((r >> 16) * u32::from(o.sound_count)) >> 16,
                o.tile,
            );
        }
    }
    unsafe {
        (*f).counter = (*f).counter.wrapping_sub(1);
    }
    if o.callbacks & 4 != 0
        && unsafe { u32::from((*f).counter) % world(s, INVERSE, i, 256, 0, 0) as u32 == 0 }
    {
        world(s, PRODUCTION, i, 1, 0, 0);
        helper(i, s, false);
    }
    if unsafe { !(*f).counter.is_multiple_of(256) } {
        return;
    }
    if o.callbacks & 4 == 0 {
        helper(i, s, true);
    }
    for (flag, parameter) in [(1, 0), (2, 1)] {
        if o.behaviour & flag == 0 {
            continue;
        }
        let cb = if o.callbacks & (1 << 9) != 0 {
            let random = rand(s);
            world(s, SPECIAL, i, random, parameter, 0) as u32
        } else {
            0xffff
        };
        let act = if cb != 0xffff {
            cb != 0
        } else if parameter == 0 {
            chance(s, 1, 8)
        } else {
            unsafe { (*f).counter.is_multiple_of(512) }
        };
        if act {
            if parameter == 0 {
                farm(i, s);
            } else {
                chop(i, s, builder);
            }
        }
    }
    world(s, TRIGGER, i, 0, 0, 0);
}
/// # Safety
/// Live builder and synchronous services; C++ supplies raw records, serial world
/// access, and terminating noexcept environmental failures. No borrowed owner
/// reference survives a service, including recursive production on construction.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_tick(
    builder: *mut Builder,
    services: *const Services,
) {
    let s = unsafe { &*services };
    let b = unsafe { ptr::addr_of_mut!((*builder).fields) };
    unsafe {
        if (*b).sound_ctr != 0 {
            (*b).sound_ctr = (*b).sound_ctr.wrapping_add(1);
            if (*b).sound_ctr == 75 {
                if setting(s, 0) != 0 {
                    world(s, SOUND, ptr::null_mut(), 1, 0, (*b).sound_tile);
                }
            } else if (*b).sound_ctr == 160 {
                (*b).sound_ctr = 0;
                if setting(s, 0) != 0 {
                    world(s, SOUND, ptr::null_mut(), 2, 0, (*b).sound_tile);
                }
            }
        }
    }
    if setting(s, 1) != 0 {
        return;
    }
    let mut from = 0;
    loop {
        let i = unsafe { (s.next)(from) };
        if i.is_null() {
            break;
        }
        let o = observe(s, i);
        from = u32::from(o.id) + 1;
        produce(i, s, builder);
        if world(s, 33, ptr::null_mut(), 0, 0, 0).wrapping_add(u64::from(o.id)) % 74 == 0 {
            for n in 0..accepted_count(o) {
                let a = accepted(o, n);
                unsafe {
                    (*a).accumulated_waiting = (*a)
                        .accumulated_waiting
                        .wrapping_add(u32::from((*a).waiting));
                }
            }
        }
    }
}
/// # Safety
/// Industry and services follow the tick contract; distribution can reenter
/// industry access, so history/waiting writes occur before/after with raw fields.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_transport(
    i: *mut c_void,
    services: *const Services,
) -> u8 {
    let s = unsafe { &*services };
    let o = observe(s, i);
    let mut moved = false;
    for n in 0..produced_count(o) {
        let p = produced(o, n);
        let mut cw = unsafe { u32::from((*p).waiting).min(255) };
        if cw <= u32::from(o.minimal_cargo) || unsafe { !valid((*p).cargo) } {
            continue;
        }
        unsafe {
            (*p).waiting = (*p).waiting.wrapping_sub(cw as u16);
        }
        if setting(s, 3) != 0 {
            cw = cw.div_ceil(2);
        }
        unsafe {
            (*p).history[0].first = (*p).history[0].first.wrapping_add(cw as u16);
        }
        let am = world(s, MOVE, i, n as u32, cw, 0) as u16;
        unsafe {
            (*p).history[0].second = (*p).history[0].second.wrapping_add(am);
        }
        moved |= am != 0;
    }
    u8::from(moved)
}
fn recompute(i: *mut c_void, s: &Services) {
    let o = observe(s, i);
    let f = fields(o);
    for n in 0..produced_count(o) {
        let rate = world(s, RATE, i, n as u32, 0, 0) as u32;
        let value = rate * unsafe { u32::from((*f).prod_level) };
        unsafe {
            (*produced(o, n)).rate = value.div_ceil(16).min(255) as u8;
        }
    }
}
/// # Safety
/// Live industry; the original `UsesOriginalEconomy` precondition applies.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_recompute(i: *mut c_void, s: *const Services) {
    recompute(i, unsafe { &*s });
}
fn update_history(mask: &mut u64, month: u32) {
    for (first, last, division, total) in [(1, 25, 1, 1), (25, 42, 3, 3), (42, 61, 4, 12)] {
        if *mask & (1 << (last - 1)) != 0
            || !month.is_multiple_of(total)
            || (division != 1 && *mask & (1 << (first - division)) == 0)
        {
            continue;
        }
        let bits = ((1u64 << (last - first)) - 1) << first;
        *mask = (*mask & !bits) | (((*mask & bits) << 1 | (1 << first)) & bits);
    }
}
fn rotate(history: &mut [Pair; RECORDS], mask: u64, month: u32) {
    for (first, last, division, total) in [(1, 25, 1, 1), (25, 42, 3, 3), (42, 61, 4, 12)] {
        if !month.is_multiple_of(total) {
            continue;
        }
        history.copy_within(first..last - 1, first + 1);
        if total == 1 {
            history[first] = history[0];
            history[0] = Pair::default();
        } else if mask & (1 << (first - division)) != 0 {
            let mut a = 0u32;
            let mut b = 0u32;
            for pair in &history[first - division..first] {
                a += u32::from(pair.first);
                b += u32::from(pair.second);
            }
            history[first] = Pair {
                first: (a / division as u32).min(65535) as u16,
                second: (b / division as u32).min(65535) as u16,
            };
        }
    }
}
fn statistics(i: *mut c_void, s: &Services) {
    let o = observe(s, i);
    let f = fields(o);
    let month = setting(s, 4);
    unsafe {
        update_history(&mut (*f).valid_history, month);
    }
    for n in 0..produced_count(o) {
        let p = produced(o, n);
        unsafe {
            if !valid((*p).cargo) {
                continue;
            }
            if (*p).history[0].first != 0 {
                (*f).last_prod_year = setting(s, 5) as i32;
            }
            rotate(&mut (*p).history, (*f).valid_history, month);
        }
    }
    for n in 0..accepted_count(o) {
        let a = accepted(o, n);
        unsafe {
            if !valid((*a).cargo) || (*a).history.is_null() {
                continue;
            }
            let history = (*a).history;
            (*history)[0].second =
                ((*a).accumulated_waiting / setting(s, 6).max(1)).min(65535) as u16;
            (*a).accumulated_waiting = 0;
            rotate(&mut *history, (*f).valid_history, month);
        }
    }
}
fn protected(o: Observation, s: &Services) -> bool {
    if o.behaviour & (1 << 7) != 0 && setting(s, 7) == 0 {
        return false;
    }
    o.behaviour & (1 << 17) == 0
        && world(s, TYPE_COUNT, ptr::null_mut(), u32::from(o.kind), 0, 0) <= 1
}
fn change(i: *mut c_void, s: &Services, monthly: bool) {
    let o = observe(s, i);
    let f = fields(o);
    let mut str = 0;
    let mut close = false;
    let mut standard = false;
    let mut suppress = false;
    let mut recalc = false;
    let original = o.original != 0;
    let mut div = 0u8;
    let mut mul = 0u8;
    let mut increment = 0i8;
    let callback = o.callbacks & (1 << if monthly { 5 } else { 4 }) != 0;
    if callback {
        let random = rand(s);
        let result = world(s, CHANGE_CALLBACK, i, u32::from(monthly), random, 0);
        let mut res = result as u16;
        if res != 0xffff {
            suppress = res & 128 != 0;
            if res & 256 != 0 {
                str = world(s, CUSTOM_TEXT, i, (result >> 16) as u32 & 0xffff, 0, 0) as u32;
            }
            res &= 15;
            match res {
                0 => {}
                1 => div = 1,
                2 => mul = 1,
                3 => close = true,
                4 => standard = true,
                5..=8 => div = (res - 3) as u8,
                9..=12 => mul = (res - 7) as u8,
                13 => increment = -1,
                14 => increment = 1,
                15 => {
                    unsafe {
                        (*f).prod_level = ((result >> 32) & 255).clamp(4, 128) as u8;
                    }
                    recalc = true;
                }
                _ => unreachable!(),
            }
        }
    } else if monthly == original || (!original && setting(s, 8) == 2) || o.life == 0 {
        return;
    }
    if standard || (!callback && o.life & 3 != 0) {
        let only_decrease = o.behaviour & (1 << 7) != 0 && setting(s, 7) == 0;
        if original {
            if only_decrease || chance(s, 1, 3) {
                let pct = if produced_count(o) > 0 {
                    unsafe { transported((*produced(o, 0)).history[1]) }
                } else {
                    0
                };
                if !only_decrease && (pct > 153) != chance(s, 1, 3) {
                    mul = 1;
                } else {
                    div = 1;
                }
            }
        } else if setting(s, 8) == 1 {
            close = unsafe { (*f).ctlflags & 5 == 0 };
            for n in 0..produced_count(o) {
                let p = produced(o, n);
                if unsafe { !valid((*p).cargo) } {
                    continue;
                }
                let random = rand(s);
                let old = unsafe { i32::from((*p).rate) };
                let pct = unsafe { transported((*p).history[1]) };
                let mut mult = if pct > 153 { 1 } else { -1 };
                if only_decrease {
                    mult = -1;
                } else if chance_value(1, if pct > 204 { 6 } else { 3 }, random) {
                    mult *= -1;
                }
                let mut new = old;
                if chance_value(1, 22, random >> 16) {
                    new = new.wrapping_add(
                        mult * (((range(s, 50) + 10) * old as u32) >> 8).max(1) as i32,
                    );
                }
                new = new.clamp(1, 255);
                if unsafe { (*p).cargo == setting(s, 9) as u8 } && o.behaviour & (1 << 19) == 0 {
                    new = new.clamp(0, 16);
                }
                if unsafe {
                    ((*f).ctlflags & 1 != 0 && new < old) || ((*f).ctlflags & 2 != 0 && new > old)
                } {
                    continue;
                }
                if new == old && old > 1 {
                    close = false;
                    continue;
                }
                let percent = if old == 0 { 100 } else { new * 100 / old - 100 };
                unsafe {
                    (*p).rate = new as u8;
                }
                if new > 1 {
                    close = false;
                }
                if percent.abs() >= 10 {
                    world(
                        s,
                        RATE_NEWS,
                        i,
                        unsafe { u32::from((*p).cargo) },
                        percent as u32,
                        0,
                    );
                }
            }
        }
    }
    if unsafe {
        ((*f).ctlflags & 1 != 0 && (div > 0 || increment < 0))
            || ((*f).ctlflags & 2 != 0 && (mul > 0 || increment > 0))
    } {
        return;
    }
    if unsafe { (*f).ctlflags & 8 != 0 } {
        div = 0;
        mul = 0;
        increment = 0;
    }
    if !callback
        && o.life & 4 != 0
        && unsafe { (setting(s, 5) as i32).wrapping_sub((*f).last_prod_year) >= 5 }
        && chance(s, 1, if original { 2 } else { 180 })
    {
        close = true;
    }
    while mul != 0 && unsafe { (*f).prod_level < 128 } {
        mul = mul.wrapping_sub(1);
        unsafe {
            (*f).prod_level = (u16::from((*f).prod_level) * 2).min(128) as u8;
        }
        recalc = true;
        if str == 0 {
            str = o.up_text;
        }
    }
    while div != 0 && !close {
        div = div.wrapping_sub(1);
        if unsafe { (*f).prod_level == 4 } {
            close = true;
            break;
        }
        unsafe {
            (*f).prod_level = ((*f).prod_level / 2).max(4);
        }
        recalc = true;
        if str == 0 {
            str = o.down_text;
        }
    }
    if increment != 0 {
        if increment < 0 && unsafe { (*f).prod_level == 4 } {
            close = true;
        } else {
            unsafe {
                (*f).prod_level = ((i32::from((*f).prod_level) + i32::from(increment)) as u32)
                    .clamp(4, 128) as u8;
            }
            recalc = true;
        }
    }
    if recalc {
        recompute(i, s);
    }
    if close && !protected(o, s) && unsafe { (*f).ctlflags & 4 == 0 } {
        unsafe {
            (*f).prod_level = 0;
        }
        world(s, DIRTY, i, 0, 0, 0);
        str = o.closure_text;
    }
    if !suppress && str != 0 {
        world(s, NEWS, i, str, u32::from(close), 0);
    }
}
/// # Safety
/// Live shell and synchronous services. Bounded `NewGRF` resolution cannot invoke
/// script VMs; news only queues events. No owner borrow crosses a service.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_change(
    i: *mut c_void,
    monthly: u8,
    s: *const Services,
) {
    change(i, unsafe { &*s }, monthly != 0);
}
fn type_data(f: *mut BuildFields, kind: u8, s: &Services) -> bool {
    let probability = game_probability(kind, s);
    let p = probability as u32;
    let min = (probability >> 32) as u8;
    unsafe {
        let changed = (*f).probability != p || (*f).min_number != min;
        (*f).probability = p;
        (*f).min_number = min;
        changed
    }
}
fn game_probability(kind: u8, s: &Services) -> u64 {
    if setting(s, 10) != 0 {
        return 0;
    }
    let info = world(s, ENABLED, ptr::null_mut(), u32::from(kind), 0, 0);
    let behavior = (info >> 32) as u32;
    let year = setting(s, 11) as i32;
    if info & 1 == 0
        || info & 2 == 0
        || (behavior & (1 << 8) != 0 && year > 1950)
        || (behavior & (1 << 9) != 0 && year < 1960)
    {
        return 0;
    }
    let probability = world(
        s,
        PROB_CALLBACK,
        ptr::null_mut(),
        u32::from(kind),
        ((info >> 8) & 255) as u32,
        0,
    ) as u8;
    if probability == 0 {
        return 0;
    }
    u64::from(probability) | ((u64::from(behavior & (1 << 17) != 0)) << 32)
}
/// # Safety
/// Live build record; no reference is retained across the resolver service.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_build_type(
    f: *mut BuildFields,
    kind: u8,
    s: *const Services,
) -> u8 {
    u8::from(type_data(f, kind, unsafe { &*s }))
}
fn build_fields(builder: *mut Builder) -> *mut BuilderFields {
    unsafe { ptr::addr_of_mut!((*builder).fields) }
}
fn build_type(b: *mut BuilderFields, n: usize) -> *mut BuildFields {
    unsafe { ptr::addr_of_mut!((*b).builddata[n]) }
}
fn build_reset(builder: *mut Builder, s: &Services) {
    let b = build_fields(builder);
    unsafe {
        (*b).wanted_inds = (world(s, TOTAL, ptr::null_mut(), 0, 0, 0) as u32).wrapping_shl(16);
        for n in 0..TYPES {
            *build_type(b, n) = BuildFields {
                max_wait: 1,
                ..BuildFields::default()
            };
        }
    }
}
/// # Safety
/// Live exclusive builder operation, with source total-count service.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_build_reset(b: *mut Builder, s: *const Services) {
    build_reset(b, unsafe { &*s });
}
fn build_monthly(builder: *mut Builder, s: &Services) {
    if setting(s, 10) != 0 {
        return;
    }
    let b = build_fields(builder);
    let max = 1 + (world(s, MAP_SCALE, ptr::null_mut(), 3, 0, 0) as u32).min(99);
    if (world(s, TOTAL, ptr::null_mut(), 0, 0, 0) as u32).wrapping_add(max)
        >= unsafe { (*b).wanted_inds >> 16 }
    {
        let increment = world(s, MAP_SCALE, ptr::null_mut(), 0x38000 / (10 * 12), 0, 0) as u32;
        unsafe {
            (*b).wanted_inds = (*b).wanted_inds.wrapping_add(increment);
        }
    }
}
/// # Safety
/// Live builder; direct total/map observations preserve the original order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_build_monthly(b: *mut Builder, s: *const Services) {
    build_monthly(b, unsafe { &*s });
}
fn targets(builder: *mut Builder, s: &Services) {
    let b = build_fields(builder);
    let mut changed = false;
    let mut planned = 0u32;
    for n in 0..TYPES {
        let f = build_type(b, n);
        changed |= type_data(f, n as u8, s);
        planned += unsafe { u32::from((*f).target_count) };
    }
    let mut amount = unsafe { (*b).wanted_inds >> 16 };
    changed |= planned != amount;
    if !changed {
        return;
    }
    let mut forced = 0;
    let mut probability = 0u32;
    for n in 0..TYPES {
        let f = build_type(b, n);
        unsafe {
            forced += u32::from((*f).min_number);
            (*f).target_count = u16::from((*f).min_number);
            probability = probability.wrapping_add((*f).probability);
        }
    }
    if probability == 0 {
        return;
    }
    amount = amount.saturating_sub(forced);
    while amount > 0 {
        let mut random = range(s, probability);
        let mut n = 0;
        while random >= unsafe { (*build_type(b, n)).probability } {
            random -= unsafe { (*build_type(b, n)).probability };
            n += 1;
        }
        unsafe {
            (*build_type(b, n)).target_count = (*build_type(b, n)).target_count.wrapping_add(1);
        }
        amount -= 1;
    }
}
/// # Safety
/// Live builder; slot fields are read/written only between source services.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_build_targets(b: *mut Builder, s: *const Services) {
    targets(b, unsafe { &*s });
}
fn count(s: &Services, n: usize) -> i32 {
    world(s, TYPE_COUNT, ptr::null_mut(), n as u32, 0, 0) as i32
}
fn place(kind: u8, creation: u8, hard: bool, s: &Services) -> *mut c_void {
    for _ in 0..if hard { 10000 } else { 2000 } {
        let i = world(
            s,
            CREATE,
            ptr::null_mut(),
            u32::from(kind),
            u32::from(creation),
            0,
        ) as usize as *mut c_void;
        if !i.is_null() {
            return i;
        }
    }
    ptr::null_mut()
}
/// # Safety
/// Construction service retains the original complete transaction, may reenter
/// production, and returns its live shell. Rust retains no owner references.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_place(
    kind: u8,
    creation: u8,
    hard: u8,
    s: *const Services,
) -> *mut c_void {
    place(kind, creation, hard != 0, unsafe { &*s })
}
fn build_try(builder: *mut Builder, s: &Services) {
    targets(builder, s);
    let b = build_fields(builder);
    let mut missing = 0i32;
    let mut eligible = 0;
    let mut probability = 0u32;
    let mut forced = TYPES;
    for n in 0..TYPES {
        let f = build_type(b, n);
        let difference = unsafe { i32::from((*f).target_count) } - count(s, n);
        missing = missing.wrapping_add(difference);
        if unsafe { (*f).wait_count > 0 } {
            continue;
        }
        if difference > 0 {
            if count(s, n) == 0
                && unsafe { (*f).min_number > 0 }
                && (forced == TYPES
                    || difference
                        > unsafe { i32::from((*build_type(b, forced)).target_count) }
                            - count(s, forced))
            {
                forced = n;
            }
            probability = probability.wrapping_add(difference as u32);
            eligible += 1;
        }
    }
    if setting(s, 3) != 0 || (forced == TYPES && (missing <= 0 || probability == 0)) {
        eligible = 0;
    }
    if eligible >= 1 {
        let n = if forced != TYPES {
            forced
        } else {
            let mut random = if eligible > 1 {
                range(s, probability)
            } else {
                0
            };
            let mut n = 0;
            loop {
                let f = build_type(b, n);
                if unsafe { (*f).wait_count == 0 } {
                    let difference = unsafe { i32::from((*f).target_count) } - count(s, n);
                    if difference > 0 {
                        if eligible == 1 || random < difference as u32 {
                            break n;
                        }
                        random -= difference as u32;
                    }
                }
                n += 1;
            }
        };
        let i = place(n as u8, 1, false, s);
        let f = build_type(b, n);
        if i.is_null() {
            unsafe {
                (*f).wait_count = (*f).max_wait.wrapping_add(1);
                (*f).max_wait = (u32::from((*f).max_wait) + 2).min(1000) as u16;
            }
        } else {
            world(s, ADVERTISE, i, 0, 0, 0);
            unsafe {
                (*f).max_wait = ((*f).max_wait / 2).max(1);
            }
        }
    }
    for n in 0..TYPES {
        let f = build_type(b, n);
        unsafe {
            if (*f).wait_count > 0 {
                (*f).wait_count -= 1;
            }
        }
    }
}
/// # Safety
/// Live builder. Construction and news services run with raw owner fields only;
/// recursive industry calls cannot alias a borrowed Rust owner or vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_build_try(b: *mut Builder, s: *const Services) {
    build_try(b, unsafe { &*s });
}
/// # Safety
/// Live builder and original ordered daily timer invocation. The company service
/// temporarily changes and restores the original current company.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_daily(
    builder: *mut Builder,
    services: *const Services,
) {
    let s = unsafe { &*services };
    let b = build_fields(builder);
    unsafe {
        (*b).daily_counter = (*b).daily_counter.wrapping_add((*b).daily_increment);
    }
    let loops = unsafe { ((*b).daily_counter >> 16) as u16 };
    unsafe {
        (*b).daily_counter &= 65535;
    }
    if loops == 0 {
        return;
    }
    let company = world(s, COMPANY, ptr::null_mut(), 0, 0, 0) as u32;
    let mut percentage = 3;
    let wanted = unsafe { (*b).wanted_inds >> 16 };
    if wanted > world(s, TOTAL, ptr::null_mut(), 0, 0, 0) as u32 {
        percentage =
            (percentage + wanted - world(s, TOTAL, ptr::null_mut(), 0, 0, 0) as u32).min(9);
    }
    for _ in 0..loops {
        if chance(s, percentage, 100) {
            build_try(builder, s);
        } else {
            let i = world(s, RANDOM_INDUSTRY, ptr::null_mut(), 0, 0, 0) as usize as *mut c_void;
            if !i.is_null() {
                change(i, s, false);
                world(s, DIRTY, i, 0, 0, 0);
            }
        }
    }
    world(s, COMPANY, ptr::null_mut(), 1, company, 0);
    world(s, DIRECTORY, ptr::null_mut(), 0, 0, 0);
}
/// # Safety
/// Live builder and original ordered monthly invocation. Destruction happens only
/// after statistics and with all Rust owner borrows ended; next ID matches pool order.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_monthly(
    builder: *mut Builder,
    services: *const Services,
) {
    let s = unsafe { &*services };
    let company = world(s, COMPANY, ptr::null_mut(), 0, 0, 0) as u32;
    build_monthly(builder, s);
    let mut from = 0;
    loop {
        let i = unsafe { (s.next)(from) };
        if i.is_null() {
            break;
        }
        let o = observe(s, i);
        from = u32::from(o.id) + 1;
        statistics(i, s);
        if unsafe { (*fields(o)).prod_level == 0 } {
            world(s, DESTROY, i, 0, 0, 0);
        } else {
            change(i, s, true);
            world(s, DIRTY, i, 0, 0, 0);
        }
    }
    world(s, COMPANY, ptr::null_mut(), 1, company, 0);
    world(s, DIRECTORY, ptr::null_mut(), 0, 0, 0);
}
/// # Safety
/// Live industry with no outstanding iterators. Rust owns removal/history freeing.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_trim(i: *mut Industry) {
    unsafe {
        let a = &mut (*i).accepted;
        let n = a.iter().rposition(|a| valid(a.cargo)).map_or(0, |n| n + 1);
        a.truncate(n);
        a.shrink_to_fit();
        let p = &mut (*i).produced;
        let n = p.iter().rposition(|p| valid(p.cargo)).map_or(0, |n| n + 1);
        p.truncate(n);
        p.shrink_to_fit();
    }
}
/// # Safety
/// Live industry and validated original-economy command arguments. The original
/// compares the requested level after assigning it, so its news branch is unreachable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_set_production(
    i: *mut c_void,
    level: u8,
    s: *const Services,
) {
    let s = unsafe { &*s };
    let o = observe(s, i);
    unsafe {
        (*fields(o)).ctlflags |= 8;
        (*fields(o)).prod_level = level;
    }
    recompute(i, s);
}

fn tile_xy(x: u32, y: u32, s: &Services) -> u32 {
    y.wrapping_mul(setting(s, 13)).wrapping_add(x)
}
fn suitable(tile: u32, fields: bool, rough: bool, s: &Services) -> bool {
    let info = world(s, MAP_TILE, ptr::null_mut(), tile, 0, 0) as u32;
    match info & 15 {
        0 => {
            if info & 16 != 0 {
                return false;
            }
            match (info >> 8) & 7 {
                5 => false,
                1 => rough,
                3 => fields,
                _ => true,
            }
        }
        4 => {
            let ground = (info >> 8) & 7;
            ground != 3 && (rough || ground != 1)
        }
        _ => false,
    }
}
fn fence(mut tile: u32, mut size: u32, kind: u32, side: u32, s: &Services) {
    let width = setting(s, 13);
    let stride = if side == 0 || side == 2 { width } else { 1 };
    loop {
        tile &= setting(s, 13).wrapping_mul(setting(s, 14)).wrapping_sub(1);
        let info = world(s, FENCE, ptr::null_mut(), tile, side, 0) as u32;
        if info != 0 {
            let mut kind = kind;
            if kind == 1 && chance(s, 1, 7) {
                kind = 2;
            }
            world(s, FENCE, ptr::null_mut(), tile, side, kind);
        }
        tile = tile.wrapping_add(stride);
        size -= 1;
        if size == 0 {
            break;
        }
    }
}
fn plant(tile: u32, industry: u16, s: &Services) {
    let climate = setting(s, 7);
    if climate == 1
        && ((world(s, MAP_TILE, ptr::null_mut(), tile, 0, 0) >> 16) as u32 + 2) >= setting(s, 15)
    {
        return;
    }
    let mut r = (rand(s) & 0x303) + 0x404;
    if climate == 1 {
        r += 0x404;
    }
    let width = setting(s, 13);
    let height = setting(s, 14);
    let sx = r & 255;
    let sy = (r >> 8) & 255;
    let x = tile % width;
    let y = tile / width;
    let x = x - x.min(sx / 2);
    let y = y - y.min(sy / 2);
    let sx = sx.min(width - x);
    let sy = sy.min(height - y);
    if sx == 0 || sy == 0 {
        return;
    }
    let mut count = 0;
    for yy in y..y + sy {
        for xx in x..x + sx {
            count += u32::from(suitable(tile_xy(xx, yy, s), false, false, s));
        }
    }
    if count * 2 < sx * sy {
        return;
    }
    r = rand(s);
    let counter = (r >> 5) & 7;
    let field = (((r >> 8) & 255) * 9) >> 8;
    for yy in y..y + sy {
        for xx in x..x + sx {
            let tile = tile_xy(xx, yy, s);
            if suitable(tile, true, true, s) {
                world(
                    s,
                    FIELD_WRITE,
                    ptr::null_mut(),
                    tile,
                    field | (counter << 8),
                    u32::from(industry),
                );
            }
        }
    }
    let mut kind = 3;
    if climate != 1 && climate != 2 {
        kind = [1, 1, 1, 1, 1, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6][(rand(s) & 15) as usize];
    }
    fence(tile_xy(x, y, s), sy, kind, 0, s);
    fence(tile_xy(x, y, s), sx, kind, 3, s);
    fence(tile_xy(x + sx - 1, y, s), sy, kind, 2, s);
    fence(tile_xy(x, y + sy - 1, s), sx, kind, 1, s);
}
fn farm(i: *mut c_void, s: &Services) {
    let o = observe(s, i);
    let x = (u32::from(o.width) / 2)
        .wrapping_add(rand(s) % 31)
        .wrapping_sub(16);
    let y = (u32::from(o.height) / 2)
        .wrapping_add(rand(s) % 31)
        .wrapping_sub(16);
    let tile = world(s, MAP_TILE, i, o.tile, x, y) as u32;
    if tile != u32::MAX {
        plant(tile, o.id, s);
    }
}
/// # Safety
/// Live industry, synchronous map/RNG services. No owner references cross calls.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_farm(i: *mut c_void, s: *const Services) {
    farm(i, unsafe { &*s });
}
fn chop(i: *mut c_void, s: &Services, builder: *mut Builder) {
    let o = observe(s, i);
    if produced_count(o) == 0 || unsafe { !valid((*produced(o, 0)).cargo) } {
        return;
    }
    let width = setting(s, 13);
    for y in o.tile / width..o.tile / width + u32::from(o.height) {
        for x in o.tile % width..o.tile % width + u32::from(o.width) {
            if world(s, COMPLETED, i, tile_xy(x, y, s), 0, 0) == 0 {
                return;
            }
        }
    }
    let mut spiral =
        crate::spiral::square(o.tile % width, o.tile / width, 40, width, setting(s, 14));
    while !spiral.is_end() {
        let tile = tile_xy(spiral.x, spiral.y, s);
        let info = world(s, MAP_TILE, ptr::null_mut(), tile, 0, 0) as u32;
        if info & 15 == 4 && info & 32 != 0 {
            let b = build_fields(builder);
            unsafe {
                (*b).sound_ctr = 1;
                (*b).sound_tile = tile;
            }
            world(s, HARVEST, i, tile, 0, 0);
            let amount = world(s, SCALE, i, 45, 0, 0) as u32;
            unsafe {
                let p = produced(o, 0);
                (*p).waiting = (u32::from((*p).waiting) + amount).min(65535) as u16;
            }
            break;
        }
        spiral = spiral.advance(width, setting(s, 14));
    }
}
#[repr(C)]
pub struct ProductionResult {
    pub subtract: [i32; 256],
    pub add: [i32; 256],
    pub again: i32,
    pub cargo_input: [u8; 256],
    pub cargo_output: [u8; 256],
    pub version: u8,
    pub num_input: u8,
    pub num_output: u8,
    pub present: u8,
}
pub type Resolve = unsafe extern "C" fn(*mut c_void, u32, u32, *mut ProductionResult);
/// # Safety
/// Live industry and local bounded C++ resolver context for this call. Resolution
/// observes live canonical fields but cannot execute scripts or commands. It
/// copies only one result; no group or register references survive the service.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_production_callback(
    i: *mut c_void,
    context: *mut c_void,
    behaviour: u32,
    reason: u8,
    resolve: Resolve,
    services: *const Services,
) {
    let s = unsafe { &*services };
    let o = observe(s, i);
    let random = if behaviour & (1 << 15) != 0 {
        rand(s)
    } else {
        0
    };
    let multiplier = if behaviour & (1 << 14) != 0 {
        unsafe { i32::from((*fields(o)).prod_level) }
    } else {
        1
    };
    let mut parameter = u32::from(reason);
    for n in 0..=65536 {
        if n == 65536 {
            world(s, CALLBACK_ERROR, i, 0, 0, 0);
            break;
        }
        parameter = (parameter & !0x00ff_ff00) | (n << 8);
        let mut result = ProductionResult {
            subtract: [0; 256],
            add: [0; 256],
            again: 0,
            cargo_input: [0; 256],
            cargo_output: [0; 256],
            version: 0,
            num_input: 0,
            num_output: 0,
            present: 0,
        };
        unsafe {
            resolve(context, random, parameter, &raw mut result);
        }
        if result.present == 0 {
            break;
        }
        if result.version == 255 {
            world(s, CALLBACK_ERROR, i, 1, 0, 0);
            break;
        }
        for n in 0..usize::from(result.num_input) {
            let slot = if result.version < 2 {
                if n >= accepted_count(o) {
                    break;
                }
                Some(n)
            } else {
                let cargo = result.cargo_input[n];
                if !valid(cargo) {
                    None
                } else {
                    (0..accepted_count(o)).find(|&n| unsafe { (*accepted(o, n)).cargo == cargo })
                }
            };
            if let Some(nslot) = slot {
                let a = accepted(o, nslot);
                unsafe {
                    if result.version < 2 && !valid((*a).cargo) {
                        continue;
                    }
                    (*a).waiting = i32::from((*a).waiting)
                        .wrapping_sub(result.subtract[n].wrapping_mul(multiplier))
                        .clamp(0, 65535) as u16;
                }
            }
        }
        for n in 0..usize::from(result.num_output) {
            let slot = if result.version < 2 {
                if n >= produced_count(o) {
                    break;
                }
                Some(n)
            } else {
                let cargo = result.cargo_output[n];
                if !valid(cargo) {
                    None
                } else {
                    (0..produced_count(o)).find(|&n| unsafe { (*produced(o, n)).cargo == cargo })
                }
            };
            if let Some(nslot) = slot {
                let p = produced(o, nslot);
                unsafe {
                    if result.version < 2 && !valid((*p).cargo) {
                        continue;
                    }
                    (*p).waiting = i32::from((*p).waiting)
                        .wrapping_add(result.add[n].max(0).wrapping_mul(multiplier))
                        .clamp(0, 65535) as u16;
                }
            }
        }
        if result.again == 0 {
            break;
        }
        parameter = (parameter & 0x00ff_ffff) | ((result.again as u32 & 255) << 24);
    }
    world(s, DIRTY, i, 0, 0, 0);
}

/// # Safety
/// Live builder record; resetting has no callback or allocation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_build_type_reset(fields: *mut BuildFields) {
    unsafe {
        *fields = BuildFields {
            max_wait: 1,
            ..BuildFields::default()
        };
    }
}
/// # Safety
/// Synchronous command services with the same live lookup and current-company
/// contract as the native command facade. Returns failure before any mutation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_command(
    id: u16,
    production: u8,
    execute: u8,
    value: u8,
    services: *const Services,
) -> u8 {
    let s = unsafe { &*services };
    if setting(s, 16) == 0 {
        return 0;
    }
    if production != 0 && !(4..=128).contains(&value) {
        return 0;
    }
    let i = world(s, 34, ptr::null_mut(), u32::from(id), 0, 0) as usize as *mut c_void;
    if i.is_null() || (production == 0 && value & !15 != 0) {
        return 0;
    }
    if execute != 0 {
        if production != 0 {
            unsafe {
                openttd_rust_industry_set_production(i, value, services);
            }
        } else {
            let o = observe(s, i);
            unsafe {
                (*fields(o)).ctlflags = value;
            }
        }
    }
    1
}

/// # Safety
/// Live builder; native map-size preconditions constrain the original shift.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_industry_daily_start(
    builder: *mut Builder,
    map_bits: u32,
    init_counter: u8,
) {
    let fields = build_fields(builder);
    unsafe {
        (*fields).daily_increment = (1u32 << map_bits) / 31;
        if init_counter != 0 {
            (*fields).daily_counter = 0;
        }
    }
}
