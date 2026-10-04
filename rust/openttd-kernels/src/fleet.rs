/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! WIP canonical fleet state. Selected control-flow ownership remains pending.
#![allow(
    unsafe_code,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::too_many_arguments,
    clippy::too_many_lines,
    clippy::many_single_char_names,
    clippy::struct_field_names
)]
use std::collections::{BTreeMap, BTreeSet};
const NONE: u32 = u32::MAX;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct GroupFields {
    pub owner: u8,
    pub vehicle_type: u8,
    pub flags: u8,
    pub livery: [u8; 3],
    pub parent: u16,
    pub number: u16,
}
#[repr(C)]
struct Group {
    fields: GroupFields,
    name: Vec<u8>,
    children: BTreeSet<u16>,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct StatsFields {
    pub profit: i64,
    pub profit_min_age: i64,
    pub vehicles: u16,
    pub vehicles_min_age: u16,
    pub defined: bool,
    pub finished: bool,
}
#[repr(C)]
struct Stats {
    fields: StatsFields,
    engines: BTreeMap<u16, u16>,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Renew {
    pub from: u16,
    pub to: u16,
    pub next: *mut (),
    pub group: u16,
    pub when_old: bool,
}
/// Create sole state allocation; C++ starts scalar object lifetimes in its prefix.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_fleet_state_create(kind: u32) -> *mut () {
    match kind {
        0 => Box::into_raw(Box::new(Group {
            fields: GroupFields {
                owner: 255,
                vehicle_type: 255,
                flags: 0,
                livery: [0; 3],
                parent: 65535,
                number: 0,
            },
            name: Vec::new(),
            children: BTreeSet::new(),
        }))
        .cast(),
        1 => Box::into_raw(Box::new(Stats {
            fields: StatsFields::default(),
            engines: BTreeMap::new(),
        }))
        .cast(),
        2 => Box::into_raw(Box::new(Renew {
            from: 65535,
            to: 65535,
            next: std::ptr::null_mut(),
            group: 65535,
            when_old: false,
        }))
        .cast(),
        3 => Box::into_raw(Box::new(std::ptr::null_mut::<()>())).cast(),
        4 => Box::into_raw(Box::new(65535_u16)).cast(),
        _ => unreachable!(),
    }
}
/// Release owner after all shell/scalar accesses end.
/// # Safety
/// Kind matches its live unique create result, destroyed once on the game thread.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_fleet_state_destroy(kind: u32, p: *mut ()) {
    unsafe {
        match kind {
            0 => drop(Box::from_raw(p.cast::<Group>())),
            1 => drop(Box::from_raw(p.cast::<Stats>())),
            2 => drop(Box::from_raw(p.cast::<Renew>())),
            3 => drop(Box::from_raw(p.cast::<*mut ()>())),
            4 => drop(Box::from_raw(p.cast::<u16>())),
            _ => unreachable!(),
        }
    }
}
/// Copy detached property/statistics allocations without sharing their lifetimes.
/// # Safety
/// Live allocations of matching kind; serial, no active borrows.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_fleet_state_copy(kind: u32, to: *mut (), from: *const ()) {
    if std::ptr::eq(to, from.cast_mut()) {
        return;
    }
    unsafe {
        match kind {
            1 => {
                let a = &*from.cast::<Stats>();
                let b = &mut *to.cast::<Stats>();
                b.fields = a.fields;
                b.engines.clone_from(&a.engines);
            }
            3 => to.cast::<*mut ()>().write(from.cast::<*mut ()>().read()),
            _ => unreachable!(),
        }
    }
}
/// Copy the owned byte name for GUI/save adapters.
/// # Safety
/// Live group and writable output for length bytes, or null output for size query.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_fleet_name(
    p: *const (),
    out: *mut u8,
    length: usize,
) -> usize {
    let g = unsafe { &*p.cast::<Group>() };
    if !out.is_null() {
        unsafe { std::ptr::copy_nonoverlapping(g.name.as_ptr(), out, length) }
    }
    g.name.len()
}
/// Replace name at the original mutation point.
/// # Safety
/// Live exclusive group; readable bytes of length, no reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_fleet_set_name(p: *mut (), bytes: *const u8, length: usize) {
    let g = unsafe { &mut *p.cast::<Group>() };
    g.name = unsafe { std::slice::from_raw_parts(bytes, length) }.to_vec();
}
/// First ordered child at or above a typed pool ID, without retaining a borrow.
/// # Safety
/// Live group, serial access.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_fleet_child(p: *const (), n: u32) -> u32 {
    unsafe { &*p.cast::<Group>() }
        .children
        .range(n as u16..)
        .next()
        .map_or(NONE, |&v| u32::from(v))
}
/// Original sorted `FlatSet` insertion/removal.
/// # Safety
/// Live group, exclusive container access.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_fleet_child_change(p: *mut (), id: u32, insert: bool) {
    let g = unsafe { &mut *p.cast::<Group>() };
    if insert {
        g.children.insert(id as u16);
    } else {
        g.children.remove(&(id as u16));
    }
}
/// Cached engine count, including retained zero entries.
/// # Safety
/// Live statistics, serial access.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_fleet_engine_count(p: *const (), engine: u16) -> u16 {
    unsafe { &*p.cast::<Stats>() }
        .engines
        .get(&engine)
        .copied()
        .unwrap_or(0)
}
/// Clear one cache class at the original call point.
/// # Safety
/// Live statistics, no borrows retained by the caller.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_fleet_stats_clear(p: *mut (), mode: u32) {
    let s = unsafe { &mut *p.cast::<Stats>() };
    match mode {
        0 => {
            s.fields.profit = 0;
            s.fields.profit_min_age = 0;
            s.fields.vehicles = 0;
            s.fields.vehicles_min_age = 0;
            s.engines.clear();
        }
        1 => {
            s.fields.profit = 0;
            s.fields.profit_min_age = 0;
            s.fields.vehicles_min_age = 0;
        }
        2 => {
            s.fields.defined = false;
            s.fields.finished = false;
        }
        _ => unreachable!(),
    }
}
/// Adjust a retained engine-map entry with the original uint16 truncation.
/// # Safety
/// Live exclusive statistics; game-thread access without callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_fleet_engine_change(p: *mut (), engine: u16, delta: i32) {
    let s = unsafe { &mut *p.cast::<Stats>() };
    let value = s.engines.entry(engine).or_default();
    *value = (i32::from(*value).wrapping_add(delta)) as u16;
}
