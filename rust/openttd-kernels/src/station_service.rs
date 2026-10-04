/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Canonical station cargo-service state. Native scalar aliases and packet
//! services never overlap Rust references into this owner. Addresses are stable
//! for the shell lifetime, including indexed loading and retired-station reuse.
#![allow(
    unsafe_code,
    clippy::missing_safety_doc,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::too_many_arguments
)]
use std::ffi::c_void;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct CargoFields {
    pub max_waiting_cargo: u32,
    pub status: u8,
    pub time_since_pickup: u8,
    pub rating: u8,
    pub last_speed: u8,
    pub last_age: u8,
    pub amount_fract: u8,
}
impl Default for CargoFields {
    fn default() -> Self {
        Self {
            max_waiting_cargo: 0,
            status: 0,
            time_since_pickup: 255,
            rating: 175,
            last_speed: 0,
            last_age: 255,
            amount_fract: 0,
        }
    }
}
#[repr(C)]
pub struct Fields {
    pub always_accepted: u64,
    pub delete_ctr: u8,
    pub time_since_load: u8,
    pub time_since_unload: u8,
    pub last_vehicle_type: u8,
}
pub struct Service {
    fields: Fields,
    cargo: [CargoFields; 64],
    // Each entry is raw-owned: native iterators retain raw pointers, so even a
    // temporary Box/&mut reborrow on a surviving entry would invalidate them.
    queue: Vec<*mut QueueEntry>,
}
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_station_service_new() -> *mut Service {
    Box::into_raw(Box::new(Service {
        fields: Fields {
            always_accepted: 0,
            delete_ctr: 0,
            time_since_load: 255,
            time_since_unload: 255,
            last_vehicle_type: 255,
        },
        cargo: [CargoFields::default(); 64],
        queue: Vec::new(),
    }))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_service_destroy(state: *mut Service) {
    unsafe {
        drop(Box::from_raw(state));
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_service_fields(state: *mut Service) -> *mut Fields {
    unsafe { &raw mut (*state).fields }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_service_cargo(
    state: *mut Service,
    cargo: u8,
) -> *mut CargoFields {
    unsafe {
        (&raw mut (*state).cargo)
            .cast::<CargoFields>()
            .add(usize::from(cargo))
    }
}
pub struct QueueEntry {
    vehicle: *mut c_void,
    next: *mut QueueEntry,
}
impl Drop for Service {
    fn drop(&mut self) {
        for entry in self.queue.drain(..) {
            // SAFETY: The owner uniquely owns each live, individually allocated
            // entry. Native aliases have ended before shell/owner destruction.
            unsafe {
                drop(Box::from_raw(entry));
            }
        }
    }
}
unsafe fn queue_relink(state: *mut Service) {
    let mut next = std::ptr::null_mut();
    // Borrow only the private vector of handles, never a live exported node.
    for &entry in unsafe { &(*state).queue }.iter().rev() {
        unsafe {
            (&raw mut (*entry).next).write(next);
        }
        next = entry;
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_queue_size(state: *const Service) -> usize {
    unsafe { (*state).queue.len() }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_queue_get(
    state: *const Service,
    index: usize,
) -> *mut c_void {
    let entry = unsafe { (&(*state).queue)[index] };
    unsafe { (&raw const (*entry).vehicle).read() }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_queue_push(
    state: *mut Service,
    vehicle: *mut c_void,
) {
    let entry = Box::into_raw(Box::new(QueueEntry {
        vehicle,
        next: std::ptr::null_mut(),
    }));
    unsafe {
        if let Some(&last) = (*state).queue.last() {
            (&raw mut (*last).next).write(entry);
        }
        (*state).queue.push(entry);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_queue_erase(state: *mut Service, index: usize) {
    unsafe {
        let entry = (*state).queue.remove(index);
        drop(Box::from_raw(entry));
        queue_relink(state);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_queue_remove(
    state: *mut Service,
    vehicle: *mut c_void,
) {
    unsafe {
        (*state).queue.retain(|&entry| {
            if (&raw const (*entry).vehicle).read() == vehicle {
                drop(Box::from_raw(entry));
                false
            } else {
                true
            }
        });
        queue_relink(state);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_queue_clear(state: *mut Service) {
    for entry in unsafe { (*state).queue.drain(..) } {
        unsafe {
            drop(Box::from_raw(entry));
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_queue_first(state: *const Service) -> *mut c_void {
    unsafe {
        (*state)
            .queue
            .first()
            .copied()
            .unwrap_or(std::ptr::null_mut())
            .cast()
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_queue_next(
    _state: *const Service,
    entry: *const c_void,
) -> *mut c_void {
    unsafe {
        (&raw const (*entry.cast::<QueueEntry>()).next)
            .read()
            .cast()
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_queue_value(entry: *const c_void) -> *mut c_void {
    unsafe { (&raw const (*entry.cast::<QueueEntry>()).vehicle).read() }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_queue_erase_node(
    state: *mut Service,
    entry: *mut c_void,
) -> *mut c_void {
    let successor = unsafe {
        (&raw const (*entry.cast::<QueueEntry>()).next)
            .read()
            .cast()
    };
    // Original erase requires a live iterator from this owner; no new rejection
    // or fatal path is introduced for an input outside that precondition.
    let index = unsafe {
        (*state)
            .queue
            .iter()
            .position(|&node| node.cast::<c_void>() == entry)
            .unwrap_unchecked()
    };
    unsafe {
        openttd_rust_station_queue_erase(state, index);
    }
    successor
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct World {
    pub(crate) get: extern "C" fn(u32) -> *mut c_void,
    pub(crate) owner: extern "C" fn(*mut c_void) -> *mut Service,
    pub(crate) read: extern "C" fn(*mut c_void, u8, u32) -> u32,
    pub(crate) effect: extern "C" fn(*mut c_void, u8, u32, u32, u32),
    pub(crate) rating_callback: extern "C" fn(*mut c_void, u8, u32, u32) -> u16,
    pub(crate) tiles: extern "C" fn(*mut c_void) -> *mut c_void,
    pub(crate) tile_next: extern "C" fn(*mut c_void, u8) -> u32,
    pub(crate) tile_destroy: extern "C" fn(*mut c_void),
    pub(crate) accept_tile: extern "C" fn(u32, *mut u32, *mut u64),
    pub(crate) truncate: extern "C" fn(*mut c_void, u8, u32) -> *mut c_void,
    pub(crate) truncate_next: extern "C" fn(*mut c_void, *mut u32) -> u32,
    pub(crate) truncate_destroy: extern "C" fn(*mut c_void),
    pub(crate) random: extern "C" fn() -> u32,
}
// Field-sized operations avoid owner borrows while C++ aliases are live. A copied
// observation is used only up to the next original write/observation point.
unsafe fn cp(state: *mut Service, cargo: u8) -> *mut CargoFields {
    unsafe {
        (&raw mut (*state).cargo)
            .cast::<CargoFields>()
            .add(usize::from(cargo))
    }
}
unsafe fn status(state: *mut Service, cargo: u8, bit: u8, value: bool) {
    let p = unsafe { &raw mut (*cp(state, cargo)).status };
    let old = unsafe { p.read() };
    unsafe {
        p.write(if value {
            old | (1 << bit)
        } else {
            old & !(1 << bit)
        });
    }
}
unsafe fn acceptance_mask(state: *mut Service) -> u64 {
    let mut mask = 0;
    for cargo in 0..64 {
        if unsafe { (*cp(state, cargo)).status } & 1 != 0 {
            mask |= 1 << cargo;
        }
    }
    mask
}
unsafe fn truncate(st: *mut c_void, cargo: u8, amount: u32, w: &World) {
    let map = (w.truncate)(st, cargo, amount);
    let mut amount = 0;
    loop {
        let source = (w.truncate_next)(map, &raw mut amount);
        if source == u32::MAX {
            break;
        }
        let source_st = (w.get)(source);
        if source_st.is_null() {
            continue;
        }
        let ge = unsafe { cp((w.owner)(source_st), cargo) };
        unsafe {
            (&raw mut (*ge).max_waiting_cargo).write((*ge).max_waiting_cargo.max(amount));
        }
    }
    (w.truncate_destroy)(map);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_acceptance(
    st: *mut c_void,
    world: *const World,
    show: u8,
) {
    let w = unsafe { world.read() };
    let state = (w.owner)(st);
    let old = unsafe { acceptance_mask(state) };
    let mut acceptance = [0_u32; 64];
    if (w.read)(st, 8, 0) == 0 {
        let always = unsafe { &raw mut (*state).fields.always_accepted };
        unsafe {
            always.write(0);
        }
        let tiles = (w.tiles)(st);
        loop {
            let tile = (w.tile_next)(tiles, 0);
            if tile == u32::MAX {
                break;
            }
            (w.accept_tile)(tile, acceptance.as_mut_ptr(), always);
        }
        (w.tile_destroy)(tiles);
    }
    let facilities = (w.read)(st, 3, 0);
    for cargo in 0..64 {
        let mut amt = acceptance[usize::from(cargo)];
        let passenger = (w.read)(st, 16, u32::from(cargo)) != 0;
        // Train=0,truck=1,bus=2,airport=3,dock=4 in StationFacility.
        if facilities & if passenger { 0b11101 } else { 0b11011 } == 0 {
            amt = 0;
        }
        unsafe {
            status(state, cargo, 0, amt >= 8);
        }
        (w.effect)(st, 1, u32::from(cargo), amt / 8, 0);
    }
    let new = unsafe { acceptance_mask(state) };
    if old == new {
        return;
    }
    if show != 0 && (w.read)(st, 7, 0) != 0 && (w.read)(st, 4, 0) != 0 {
        let accepts = new & !old;
        let rejects = !new & old;
        if accepts != 0 {
            (w.effect)(st, 2, accepts as u32, (accepts >> 32) as u32, 0);
        }
        if rejects != 0 {
            (w.effect)(st, 2, rejects as u32, (rejects >> 32) as u32, 1);
        }
    }
    (w.effect)(st, 3, 0, 0, 0);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_watched(st: *mut c_void, world: *const World) {
    let w = unsafe { world.read() };
    let state = (w.owner)(st);
    let mut cargoes = 0_u64;
    for cargo in 0..64 {
        if unsafe { (*cp(state, cargo)).status } & 32 != 0 {
            cargoes |= 1 << cargo;
        }
    }
    if cargoes == 0 {
        return;
    }
    let tiles = (w.tiles)(st);
    loop {
        let tile = (w.tile_next)(tiles, 1);
        if tile == u32::MAX {
            break;
        }
        if (w.read)(st, 20, tile) != 0 {
            (w.effect)(st, 6, tile, cargoes as u32, (cargoes >> 32) as u32);
        }
    }
    (w.tile_destroy)(tiles);
}
#[unsafe(no_mangle)]
// Preserve the complete source rating loop and its immediate mutation order.
#[allow(clippy::too_many_lines)]
pub unsafe extern "C" fn openttd_rust_station_rating(st: *mut c_void, world: *const World) {
    let w = unsafe { world.read() };
    let state = (w.owner)(st);
    let mut waiting_changed = false;
    unsafe {
        let p = &raw mut (*state).fields.time_since_load;
        p.write(p.read().saturating_add(1));
        let p = &raw mut (*state).fields.time_since_unload;
        p.write(p.read().saturating_add(1));
    }
    for cargo in 0..64_u8 {
        if (w.read)(st, 14, u32::from(cargo)) == 0 {
            continue;
        }
        let ge = unsafe { cp(state, cargo) };
        if unsafe { (*ge).status } & 2 == 0 {
            unsafe {
                if (*ge).rating < 175 {
                    (&raw mut (*ge).rating).write((*ge).rating + 1);
                }
            }
            continue;
        }
        unsafe {
            (&raw mut (*ge).time_since_pickup).write((*ge).time_since_pickup.saturating_add(1));
        }
        if unsafe { (*ge).time_since_pickup } == 255 && (w.read)(st, 10, 0) != 0 {
            unsafe {
                status(state, cargo, 1, false);
                (&raw mut (*ge).last_speed).write(0);
                truncate(st, cargo, u32::MAX, &w);
            }
            waiting_changed = true;
            continue;
        }
        let mut skip = false;
        let mut rating = 0_i32;
        let mut waiting = (w.read)(st, 12, u32::from(cargo));
        let num_dests = (w.read)(st, 13, u32::from(cargo));
        let waiting_avg = waiting / num_dests.wrapping_add(1);
        if (w.read)(st, 11, 0) != 0 {
            unsafe {
                (&raw mut (*ge).rating).write(255);
            }
            rating = 255;
            skip = true;
        } else if (w.read)(st, 15, u32::from(cargo)) != 0 {
            let speed = unsafe { (*ge).last_speed };
            let var18 = unsafe {
                u32::from((*ge).time_since_pickup)
                    | ((*ge).max_waiting_cargo.min(65535) << 8)
                    | (u32::from(if speed == 0 { 255 } else { speed }) << 24)
            };
            let kind = unsafe { (*state).fields.last_vehicle_type };
            let var10 = if kind == 255 {
                0
            } else {
                u32::from(kind) + 0x10
            };
            let cb = (w.rating_callback)(st, cargo, var10, var18);
            if cb != 0xffff {
                skip = true;
                rating = i32::from(cb & 0x3fff);
                if cb & 0x4000 != 0 {
                    rating -= 0x4000;
                }
            }
        }
        if !skip {
            let b = i32::from(unsafe { (*ge).last_speed }) - 85;
            if b >= 0 {
                rating += b >> 2;
            }
            let mut waittime = unsafe { (*ge).time_since_pickup };
            if unsafe { (*state).fields.last_vehicle_type } == 2 {
                waittime >>= 2;
            }
            if waittime <= 21 {
                rating += 25;
            }
            if waittime <= 12 {
                rating += 25;
            }
            if waittime <= 6 {
                rating += 45;
            }
            if waittime <= 3 {
                rating += 35;
            }
            rating -= 90;
            let max = unsafe { (*ge).max_waiting_cargo };
            if max <= 1500 {
                rating += 55;
            }
            if max <= 1000 {
                rating += 35;
            }
            if max <= 600 {
                rating += 10;
            }
            if max <= 300 {
                rating += 20;
            }
            if max <= 100 {
                rating += 10;
            }
        }
        if (w.read)(st, 9, 0) != 0 {
            rating += 26;
        }
        let age = unsafe { (*ge).last_age };
        if age < 3 {
            rating += 10;
        }
        if age < 2 {
            rating += 10;
        }
        if age < 1 {
            rating += 13;
        }
        let old_rating = i32::from(unsafe { (*ge).rating });
        rating = (old_rating + (rating - old_rating).clamp(-2, 2)).clamp(0, 255);
        unsafe {
            (&raw mut (*ge).rating).write(rating as u8);
        }
        if rating <= 64 && waiting_avg >= 100 {
            let mut dec = (w.random)() & 31;
            if waiting_avg < 200 {
                dec &= 7;
            }
            waiting = waiting.wrapping_sub((dec + 1).wrapping_mul(num_dests));
            waiting_changed = true;
        }
        if rating <= 127 && waiting != 0 {
            let random = (w.random)();
            if rating <= (random & 127) as i32 {
                waiting = (waiting as i32)
                    .wrapping_sub((((random >> 8) & 3) + 1).wrapping_mul(num_dests) as i32)
                    .max(0) as u32;
                waiting_changed = true;
            }
        }
        if waiting > 4096 {
            waiting -= (waiting - 4096) / 64;
            waiting = waiting.min(32768);
            waiting_changed = true;
        }
        let available = (w.read)(st, 12, u32::from(cargo));
        if waiting_changed && waiting < available {
            unsafe {
                (&raw mut (*ge).max_waiting_cargo).write(0);
                truncate(st, cargo, available - waiting, &w);
            }
        } else {
            unsafe {
                (&raw mut (*ge).max_waiting_cargo).write(waiting_avg);
            }
        }
    }
    (w.effect)(st, 0, u32::from(waiting_changed), 0, 0);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_monthly(world: *const World) {
    let w = unsafe { world.read() };
    for id in 0..(w.read)(std::ptr::null_mut(), 0, 0) {
        let st = (w.get)(id);
        if st.is_null() || (w.read)(st, 5, 0) == 0 {
            continue;
        }
        let state = (w.owner)(st);
        for cargo in 0..64 {
            let current = unsafe { (*cp(state, cargo)).status } & 16 != 0;
            unsafe {
                status(state, cargo, 3, current);
                status(state, cargo, 4, false);
            }
        }
    }
}

unsafe fn admit(st: *mut c_void, cargo: u8, mut amount: u32, source: u32, w: &World) -> u32 {
    if (w.read)(st, 19, 0) == 0 {
        return 0;
    }
    let state = (w.owner)(st);
    let ge = unsafe { cp(state, cargo) };
    amount = amount.wrapping_add(u32::from(unsafe { (*ge).amount_fract }));
    unsafe {
        (&raw mut (*ge).amount_fract).write(amount as u8);
    }
    amount >>= 8;
    if amount == 0 {
        return 0;
    }
    (w.effect)(st, 7, u32::from(cargo), amount, source);
    (w.effect)(st, 8, u32::from(cargo), amount, 0);
    if unsafe { (*ge).status } & 2 == 0 {
        (w.effect)(st, 9, 0, 0, 0);
        unsafe {
            status(state, cargo, 1, true);
        }
    }
    (w.effect)(st, 10, u32::from(cargo), 0, 0);
    (w.effect)(st, 11, 0, 0, 0);
    amount
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_distribute(
    cargo: u8,
    mut amount: u32,
    source: u32,
    stations: *const *mut c_void,
    count: usize,
    exclusivity: u8,
    world: *const World,
) -> u32 {
    if count == 0 || amount == 0 {
        return 0;
    }
    let w = unsafe { world.read() };
    let mut used = Vec::new();
    for index in 0..count {
        let st = unsafe { stations.add(index).read() };
        let owner = (w.read)(st, 6, 0);
        if exclusivity != 255 && u32::from(exclusivity) != owner {
            continue;
        }
        if owner != 16 && (w.read)(st, 17, 0) > 0 && (w.read)(st, 18, 0) != owner {
            continue;
        }
        let ge = unsafe { cp((w.owner)(st), cargo) };
        let rating = unsafe { (*ge).rating };
        if rating == 0 {
            continue;
        }
        if (w.read)(st, 10, 0) != 0 && unsafe { (*ge).last_speed } == 0 {
            continue;
        }
        let passenger = (w.read)(st, 16, u32::from(cargo)) != 0;
        if (w.read)(st, 3, 0) == if passenger { 2 } else { 4 } {
            continue;
        }
        used.push((st, owner as usize, u32::from(rating), 0_u32));
    }
    if used.is_empty() {
        return 0;
    }
    if used.len() == 1 {
        let (st, _, rating, _) = used[0];
        return unsafe { admit(st, cargo, amount.wrapping_mul(rating + 1), source, &w) };
    }
    let mut company_best = [0_u32; 19];
    let mut company_sum = [0_u32; 19];
    let mut best_rating = 0;
    let mut best_sum = 0_u32;
    for &(_, owner, rating, _) in &used {
        if rating > company_best[owner] {
            best_sum = best_sum.wrapping_add(rating - company_best[owner]);
            company_best[owner] = rating;
            best_rating = best_rating.max(rating);
        }
        company_sum[owner] = company_sum[owner].wrapping_add(rating);
    }
    amount = amount.wrapping_mul(best_rating + 1);
    let mut moving = 0_u32;
    for &mut (_, owner, rating, ref mut share) in &mut used {
        *share = amount
            .wrapping_mul(company_best[owner])
            .wrapping_mul(rating)
            / best_sum
            / company_sum[owner];
        moving = moving.wrapping_add(*share);
    }
    if amount > moving {
        used.sort_by_key(|entry| std::cmp::Reverse(entry.2));
        for index in 0..amount - moving {
            used[index as usize].3 = used[index as usize].3.wrapping_add(1);
        }
    }
    let mut moved = 0_u32;
    for (st, _, _, share) in used {
        moved = moved.wrapping_add(unsafe { admit(st, cargo, share, source, &w) });
    }
    moved
}

#[repr(C)]
#[derive(Default)]
pub struct Edge {
    pub(crate) destination: *mut c_void,
    pub(crate) last_update: i32,
    pub(crate) unrestricted: i32,
    pub(crate) restricted: i32,
    pub(crate) distance: u32,
    pub(crate) node: u16,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Links {
    pub(crate) graph: extern "C" fn(*mut c_void, u8) -> *mut c_void,
    pub(crate) read: extern "C" fn(*mut c_void, u8, u32, u32) -> u32,
    pub(crate) edge: extern "C" fn(*mut c_void, u16, u32, *mut Edge),
    pub(crate) effect: extern "C" fn(*mut c_void, u8, u16, u16),
    pub(crate) order_list: extern "C" fn(u32) -> *mut c_void,
    pub(crate) order_read: extern "C" fn(*mut c_void, u8, u32) -> u32,
    pub(crate) order_vehicle: extern "C" fn(*mut c_void) -> *mut c_void,
    pub(crate) next_vehicle: extern "C" fn(*mut c_void, u8) -> *mut c_void,
    pub(crate) vehicle_read: extern "C" fn(*mut c_void, u8) -> u32,
    pub(crate) refresh: extern "C" fn(*mut c_void),
    pub(crate) reroute: extern "C" fn(*mut c_void, *mut c_void, u8, u16, u16),
}
unsafe fn reroute(st: *mut c_void, cargo: u8, avoid: u16, avoid2: u16, w: &World, l: &Links) {
    (l.reroute)(st, std::ptr::null_mut(), cargo, avoid, avoid2);
    let state = (w.owner)(st);
    let mut node = unsafe { openttd_rust_station_queue_first(state) };
    while !node.is_null() {
        let mut vehicle = unsafe { openttd_rust_station_queue_value(node) };
        while !vehicle.is_null() {
            if (l.vehicle_read)(vehicle, 2) == u32::from(cargo) {
                (l.reroute)(st, vehicle, cargo, avoid, avoid2);
            }
            vehicle = (l.next_vehicle)(vehicle, 0);
        }
        node = unsafe { openttd_rust_station_queue_next(state, node) };
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_reroute(
    st: *mut c_void,
    cargo: u8,
    avoid: u16,
    avoid2: u16,
    world: *const World,
    links: *const Links,
) {
    let w = unsafe { world.read() };
    let l = unsafe { links.read() };
    unsafe {
        reroute(st, cargo, avoid, avoid2, &w, &l);
    }
}
unsafe fn stale(from: *mut c_void, w: &World, l: &Links) {
    for cargo in 0..64 {
        let auto = (l.read)(from, 0, u32::from(cargo), 0) != 0;
        let graph = (l.graph)(from, cargo);
        if graph.is_null() {
            continue;
        }
        let node = (l.read)(from, 1, u32::from(cargo), 0) as u16;
        let mut remove = Vec::new();
        let count = (l.read)(graph, 2, u32::from(node), 0);
        for index in 0..count {
            let mut edge = Edge::default();
            (l.edge)(graph, node, index, &raw mut edge);
            let date = (l.read)(std::ptr::null_mut(), 3, 0, 0) as i32;
            let timeout = 32_u32.wrapping_add(edge.distance >> 3) as i32;
            if date.wrapping_sub(edge.last_update) > timeout {
                let mut updated = false;
                if auto {
                    let mut vehicles = Vec::new();
                    let from_id = (l.read)(from, 4, 0, 0);
                    let to_id = (l.read)(edge.destination, 4, 0, 0);
                    let order_count = (l.order_read)(std::ptr::null_mut(), 0, 0);
                    for id in 0..order_count {
                        let list = (l.order_list)(id);
                        if list.is_null() {
                            continue;
                        }
                        let mut found_from = false;
                        let mut found_to = false;
                        for order in 0..(l.order_read)(list, 1, 0) {
                            let dest = (l.order_read)(list, 2, order);
                            if dest == u32::MAX {
                                continue;
                            }
                            if dest == from_id {
                                found_from = true;
                                if found_to {
                                    break;
                                }
                            } else if dest == to_id {
                                found_to = true;
                                if found_from {
                                    break;
                                }
                            }
                        }
                        if found_from && found_to {
                            vehicles.push((l.order_vehicle)(list));
                        }
                    }
                    let mut iterator = 0;
                    while iterator < vehicles.len() {
                        let vehicle = vehicles[iterator];
                        if (l.vehicle_read)(vehicle, 0) == 0
                            || date.wrapping_sub((l.vehicle_read)(vehicle, 1) as i32) <= 1024
                        {
                            (l.refresh)(vehicle);
                        }
                        if (l.read)(graph, 5, u32::from(node), index) as i32 == date {
                            updated = true;
                            break;
                        }
                        let next_shared = (l.next_vehicle)(vehicle, 1);
                        if next_shared.is_null() {
                            vehicles.remove(iterator);
                        } else {
                            vehicles[iterator] = next_shared;
                            iterator += 1;
                        }
                        if iterator == vehicles.len() {
                            iterator = 0;
                        }
                    }
                }
                if !updated {
                    remove.push(edge.node);
                    let to_id = (l.read)(edge.destination, 4, 0, 0) as u16;
                    (l.effect)(from, 0, u16::from(cargo), to_id);
                    unsafe {
                        reroute(from, cargo, to_id, (l.read)(from, 4, 0, 0) as u16, w, l);
                    }
                }
            } else if edge.unrestricted != -1 && date.wrapping_sub(edge.unrestricted) > timeout {
                (l.effect)(graph, 1, node, edge.node);
                let to_id = (l.read)(edge.destination, 4, 0, 0) as u16;
                (l.effect)(from, 2, u16::from(cargo), to_id);
                unsafe {
                    reroute(from, cargo, to_id, (l.read)(from, 4, 0, 0) as u16, w, l);
                }
            } else if edge.restricted != -1 && date.wrapping_sub(edge.restricted) > timeout {
                (l.effect)(graph, 3, node, edge.node);
            }
        }
        for dest in remove {
            (l.effect)(graph, 4, node, dest);
        }
        let date = (l.read)(std::ptr::null_mut(), 3, 0, 0) as i32;
        if date.wrapping_sub((l.read)(graph, 6, 0, 0) as i32) > 256 {
            (l.effect)(graph, 5, 0, 0);
        }
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_stale(
    st: *mut c_void,
    world: *const World,
    links: *const Links,
) {
    let w = unsafe { world.read() };
    let l = unsafe { links.read() };
    unsafe {
        stale(st, &w, &l);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_tick(world: *const World, links: *const Links) {
    let w = unsafe { world.read() };
    let l = unsafe { links.read() };
    if (w.read)(std::ptr::null_mut(), 2, 0) != 0 {
        return;
    }
    let mut id = 0_u32;
    while id < (w.read)(std::ptr::null_mut(), 0, 0) {
        let st = (w.get)(id);
        if !st.is_null() {
            let state = (w.owner)(st);
            if (w.read)(st, 3, 0) & 128 == 0 && (w.read)(st, 4, 0) != 0 {
                let mut b = unsafe { (*state).fields.delete_ctr }.wrapping_add(1);
                if b >= 185 {
                    b = 0;
                }
                unsafe {
                    (&raw mut (*state).fields.delete_ctr).write(b);
                }
                if b == 0 {
                    unsafe {
                        openttd_rust_station_rating(st, world);
                    }
                }
            }
            let tick = u64::from((w.read)(std::ptr::null_mut(), 1, 0))
                | (u64::from((w.read)(std::ptr::null_mut(), 21, 0)) << 32);
            let phase = tick.wrapping_add(u64::from(id));
            if (w.read)(st, 5, 0) != 0 && phase % 504 == 0 {
                unsafe {
                    stale(st, &w, &l);
                }
            }
            if phase % 250 == 0 {
                if (w.read)(st, 4, 0) == 0 {
                    let b = unsafe { (*state).fields.delete_ctr }.wrapping_add(1);
                    unsafe {
                        (&raw mut (*state).fields.delete_ctr).write(b);
                    }
                    if b >= 8 {
                        (w.effect)(st, 4, 0, 0, 0);
                    }
                    id += 1;
                    continue;
                }
                if (w.read)(st, 5, 0) != 0 {
                    unsafe {
                        openttd_rust_station_watched(st, world);
                    }
                    for cargo in 0..64 {
                        unsafe {
                            status(state, cargo, 5, false);
                        }
                    }
                }
                if (w.read)(st, 3, 0) & 128 == 0 {
                    unsafe {
                        openttd_rust_station_acceptance(st, world, 1);
                    }
                }
            }
            if phase % 250 == 0 {
                (w.effect)(st, 5, 0, 0, 0);
            }
        }
        id += 1;
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Loading {
    pub(crate) read: extern "C" fn(*mut c_void, u8) -> i64,
    pub(crate) write: extern "C" fn(*mut c_void, u8, i64),
    pub(crate) next: extern "C" fn(*mut c_void, u8) -> *mut c_void,
    pub(crate) next_stations: extern "C" fn(*mut c_void) -> *mut c_void,
    pub(crate) next_stations_destroy: extern "C" fn(*mut c_void),
    pub(crate) cargo:
        extern "C" fn(*mut c_void, *mut c_void, u8, u32, *mut c_void, *mut c_void) -> u32,
    pub(crate) load_callback: extern "C" fn(*mut c_void, u8) -> u16,
    pub(crate) payment: extern "C" fn(*mut c_void, u8) -> *mut c_void,
    pub(crate) effect: extern "C" fn(*mut c_void, *mut c_void, u8, u8),
    pub(crate) refit: extern "C" fn(*mut c_void, u8, u8, *mut i64) -> u64,
}
macro_rules! loading_readers {
    ($($name:ident = $field:literal),+ $(,)?) => {
        impl Loading {
            $(fn $name(&self, vehicle: *mut c_void) -> u32 {
                (self.read)(vehicle, $field) as u32
            })+
        }
    };
}
loading_readers! {
    vehicle_type = 0,
    cargo_type = 1,
    cargo_cap = 2,
    stored_count = 4,
    remaining_count = 5,
    reserved_count = 6,
    unload_count = 7,
    deliver_count = 8,
    transfer_count = 9,
    load_count = 10,
    cargo_unloading = 11,
    unload_type = 12,
    load_type = 13,
    is_refit = 14,
    refit_cargo = 15,
    has_orders = 17,
    load_unload_ticks = 18,
    stopped_or_crashed = 19,
    has_articulated_part = 20,
    is_articulated_part = 21,
    is_rear_dualheaded = 22,
    is_normal_aircraft = 23,
    is_multiheaded = 24,
    cached_max_speed = 25,
    engine_load_amount = 28,
    grf_version = 29,
    has_load_callback = 30,
    no_default_cargo_multiplier = 31,
    cargo_multiplier = 32,
    cached_total_length = 34,
    full_load_order = 39,
    vehicle_owner = 40,
    aircraft_old_speed = 42,
    passenger_cargo = 44,
    gradual_loading = 46,
    improved_load = 47,
    station_length_penalty = 48,
    stop_loading = 54,
    current_company = 56,
    last_station_visited = 57,
    total_count = 58,
}

fn parts(l: &Loading, v: *mut c_void, mut action: impl FnMut(*mut c_void) -> bool) -> bool {
    let mut part = v;
    while !part.is_null() {
        if !action(part) {
            return false;
        }
        if l.vehicle_type(part) == 0 && l.is_multiheaded(part) != 0 && !action((l.next)(part, 3)) {
            return false;
        }
        part = if l.has_articulated_part(part) != 0 {
            (l.next)(part, 2)
        } else {
            std::ptr::null_mut()
        };
    }
    if l.vehicle_type(v) == 3 && l.is_normal_aircraft(v) != 0 {
        return action((l.next)(v, 0));
    }
    true
}
fn may_load(st: *mut c_void, v: *mut c_void, w: &World, l: &Loading) -> bool {
    (w.read)(st, 6, 0) != 16
        || (w.read)(st, 17, 0) == 0
        || (w.read)(st, 18, 0) == l.vehicle_owner(v)
}
fn reserve_consist(
    st: *mut c_void,
    front: *mut c_void,
    mut capleft: Option<&mut [u32; 64]>,
    next: *mut c_void,
    w: &World,
    l: &Loading,
) {
    let must_reserve = l.is_refit(front) == 0 || (l.payment)(front, 0).is_null();
    let mut v = front;
    while !v.is_null() {
        if l.is_articulated_part(v) == 0
            && (l.vehicle_type(v) != 0 || l.is_rear_dualheaded(v) == 0)
            && (l.vehicle_type(v) != 3 || l.is_normal_aircraft(v) != 0)
            && (must_reserve || l.refit_cargo(front) == l.cargo_type(v))
        {
            parts(l, v, |part| {
                let cap = l.cargo_cap(part);
                let remaining = l.remaining_count(part);
                if cap > remaining && may_load(st, part, w, l) {
                    (l.cargo)(st, part, 4, cap - remaining, next, std::ptr::null_mut());
                }
                true
            });
        }
        if let Some(ref mut capleft) = capleft
            && l.cargo_cap(v) != 0
        {
            let cargo = l.cargo_type(v) as usize;
            capleft[cargo] =
                capleft[cargo].wrapping_add(l.cargo_cap(v).wrapping_sub(l.remaining_count(v)));
        }
        v = (l.next)(v, 0);
    }
}
fn load_amount(v: *mut c_void, l: &Loading) -> u32 {
    let mut amount = l.engine_load_amount(v);
    let mail = l.vehicle_type(v) == 3 && l.is_normal_aircraft(v) == 0;
    if mail {
        amount = amount.wrapping_add(3) / 4;
    }
    if l.gradual_loading(v) != 0 {
        let version = l.grf_version(v);
        let mut cb = 0xffff;
        if version >= 8 {
            cb = (l.load_callback)(v, 0);
        } else if l.has_load_callback(v) != 0 {
            cb = (l.load_callback)(v, 1);
        }
        if cb != 0xffff {
            if version < 8 {
                cb &= 255;
            }
            if cb >= 256 {
                (l.write)(v, 7, i64::from(cb));
            } else if cb != 0 {
                amount = u32::from(cb);
            }
        }
    }
    if l.no_default_cargo_multiplier(v) != 0 && !mail {
        amount = amount.wrapping_mul(l.cargo_multiplier(v)).wrapping_add(255) / 256;
    }
    amount.max(1)
}
fn refit_vehicle(
    v: *mut c_void,
    capleft: &mut [u32; 64],
    st: *mut c_void,
    next: *mut c_void,
    mut new_cargo: u8,
    w: &World,
    l: &Loading,
) {
    let start = (l.next)(v, 4);
    if !parts(l, start, |part| l.stored_count(part) == 0) {
        return;
    }
    let old_company = l.current_company(std::ptr::null_mut());
    (l.write)(std::ptr::null_mut(), 11, i64::from(l.vehicle_owner(v)));
    let mut mask = (l.read)(v, 51) as u64;
    parts(l, start, |part| {
        let cargo = l.cargo_type(part) as usize;
        capleft[cargo] =
            capleft[cargo].wrapping_sub(l.cargo_cap(part).wrapping_sub(l.reserved_count(part)));
        mask |= (l.read)(part, 51) as u64;
        true
    });
    let auto = new_cargo == 253;
    if auto {
        new_cargo = l.cargo_type(start) as u8;
        for cargo in 0..64_u8 {
            if mask & (1_u64 << cargo) == 0
                || (l.cargo)(
                    st,
                    std::ptr::null_mut(),
                    7,
                    u32::from(cargo),
                    next,
                    std::ptr::null_mut(),
                ) == 0
            {
                continue;
            }
            let mut cost = 0;
            let capacity = (l.refit)(start, cargo, 0, &raw mut cost);
            if capacity > 0
                && (capleft[usize::from(cargo)] < capleft[usize::from(new_cargo)]
                    || (capleft[usize::from(cargo)] == capleft[usize::from(new_cargo)]
                        && (w.read)(st, 12, u32::from(cargo))
                            > (w.read)(st, 12, u32::from(new_cargo))))
            {
                new_cargo = cargo;
            }
        }
    }
    if new_cargo < 64 && u32::from(new_cargo) != l.cargo_type(start) {
        parts(l, start, |part| {
            (l.cargo)(
                st,
                part,
                1,
                u32::MAX,
                std::ptr::null_mut(),
                std::ptr::null_mut(),
            );
            true
        });
        let mut cost = 0;
        let succeeded = (l.refit)(start, new_cargo, 1, &raw mut cost);
        if succeeded != 0 {
            (l.write)((l.next)(v, 5), 6, cost.wrapping_shl(8));
        }
    }
    let do_reserve = auto || l.full_load_order((l.next)(v, 5)) != 0;
    parts(l, start, |part| {
        let remaining = l.cargo_cap(part).wrapping_sub(l.remaining_count(part));
        if do_reserve {
            (l.cargo)(st, part, 4, remaining, next, std::ptr::null_mut());
        }
        let cargo = l.cargo_type(part) as usize;
        capleft[cargo] =
            capleft[cargo].wrapping_add(l.cargo_cap(part).wrapping_sub(l.remaining_count(part)));
        true
    });
    (l.write)(std::ptr::null_mut(), 11, i64::from(old_company));
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_prepare(
    front: *mut c_void,
    world: *const World,
    loading: *const Loading,
) {
    let w = unsafe { world.read() };
    let l = unsafe { loading.read() };
    let st = (w.get)(l.last_station_visited(front));
    unsafe {
        openttd_rust_station_queue_push((w.owner)(st), front);
    }
    (l.write)(front, 1, 0);
    (l.write)(front, 3, 1);
    let payment = (l.payment)(front, 1);
    let next = (l.next_stations)(front);
    if l.has_orders(front) == 0 || l.unload_type(front) != 4 {
        let mut v = front;
        while !v.is_null() {
            if l.cargo_cap(v) > 0 && l.total_count(v) > 0 {
                (l.cargo)(st, v, 0, l.unload_type(front), next, payment);
                if l.unload_count(v) > 0 {
                    (l.write)(v, 0, 1);
                }
            }
            v = (l.next)(v, 0);
        }
    }
    (l.next_stations_destroy)(next);
}
fn update_ticks(front: *mut c_void, st: *mut c_void, mut ticks: i32, l: &Loading) {
    if l.vehicle_type(front) == 0 && l.station_length_penalty(front) != 0 {
        // Platform length is a world-geometry query on the native station shell.
        let overhang = l.cached_total_length(front).wrapping_sub(
            (l.cargo)(st, front, 8, 0, std::ptr::null_mut(), std::ptr::null_mut()).wrapping_mul(16),
        ) as i32;
        if overhang > 0 {
            ticks = ticks.wrapping_shl(1);
            ticks = ticks.wrapping_add(overhang.wrapping_mul(ticks) / 8);
        }
    }
    (l.write)(front, 3, i64::from(ticks.max(1)));
}
// Preserve complete loading control at the source observation points.
#[allow(clippy::too_many_lines)]
unsafe fn load_vehicle(front: *mut c_void, st: *mut c_void, w: &World, l: &Loading) {
    let next = (l.next_stations)(front);
    let auto = l.is_refit(front) != 0 && l.refit_cargo(front) == 253;
    let mut capleft = [0_u32; 64];
    let reserve = if l.improved_load(front) != 0 && auto {
        (l.payment)(front, 0).is_null()
    } else {
        l.full_load_order(front) != 0
    };
    if reserve {
        reserve_consist(
            st,
            front,
            if auto && l.load_unload_ticks(front) != 0 {
                Some(&mut capleft)
            } else {
                None
            },
            next,
            w,
            l,
        );
    }
    if l.load_unload_ticks(front) != 0 {
        (l.next_stations_destroy)(next);
        return;
    }
    if l.vehicle_type(front) == 0
        && (l.cargo)(st, front, 9, 0, std::ptr::null_mut(), std::ptr::null_mut()) == 0
    {
        (l.write)(front, 1, 1);
        (l.write)(front, 3, 1);
        (l.next_stations_destroy)(next);
        return;
    }
    let state = (w.owner)(st);
    let mut ticks = 0_i32;
    let mut dirty_vehicle = false;
    let mut dirty_station = false;
    let mut emptied = true;
    let mut unloaded = false;
    let mut loaded_any = false;
    let mut full_load = 0_u64;
    let mut cargo_not_full = 0_u64;
    let mut cargo_full = 0_u64;
    let mut reservation_left = 0_u64;
    (l.write)(front, 4, 0);
    let payment = (l.payment)(front, 0);
    let mut artic_part = 0_u32;
    let mut v = front;
    while !v.is_null() {
        if v == front || l.has_articulated_part((l.next)(v, 6)) == 0 {
            artic_part = 0;
        }
        if l.cargo_cap(v) == 0 {
            v = (l.next)(v, 0);
            continue;
        }
        artic_part = artic_part.wrapping_add(1);
        let mut cargo = l.cargo_type(v) as u8;
        let mut ge = unsafe { cp(state, cargo) };
        if l.cargo_unloading(v) != 0 && l.unload_type(front) != 4 {
            let count = l.unload_count(v);
            let mut amount = if l.gradual_loading(v) != 0 {
                count.min(load_amount(v, l))
            } else {
                count
            };
            if unsafe { (*ge).status } & 1 == 0 && l.deliver_count(v) > 0 {
                let unload_type = l.unload_type(front);
                if unload_type == 2 || unload_type == 1 {
                    (l.cargo)(st, v, 5, l.deliver_count(v), next, payment);
                } else {
                    let remaining = l.remaining_count(v).wrapping_add(l.deliver_count(v));
                    if l.cargo_cap(v) < remaining {
                        (l.cargo)(
                            st,
                            v,
                            1,
                            remaining - l.cargo_cap(v),
                            std::ptr::null_mut(),
                            std::ptr::null_mut(),
                        );
                    }
                    (l.cargo)(st, v, 6, l.deliver_count(v), next, payment);
                    unloaded = true;
                }
            }
            if l.transfer_count(v) > 0 {
                dirty_station = true;
                if unsafe { (*ge).status } & 2 == 0 {
                    unsafe {
                        (&raw mut (*ge).time_since_pickup).write(0);
                        status(state, cargo, 1, true);
                    }
                }
            }
            amount = (l.cargo)(st, v, 2, amount, next, payment);
            let remaining = l.unload_count(v) > 0;
            if amount > 0 {
                dirty_vehicle = true;
                unloaded = true;
                ticks = ticks.wrapping_add(amount as i32);
                unsafe {
                    (&raw mut (*state).fields.time_since_unload).write(0);
                }
            }
            if l.gradual_loading(v) != 0 && remaining {
                emptied = false;
            } else {
                (l.write)(v, 0, 0);
            }
            v = (l.next)(v, 0);
            continue;
        }
        if l.load_type(front) == 4 || l.stop_loading(front) != 0 {
            v = (l.next)(v, 0);
            continue;
        }
        if l.is_refit(front) != 0 && artic_part == 1 {
            refit_vehicle(v, &mut capleft, st, next, l.refit_cargo(front) as u8, w, l);
            cargo = l.cargo_type(v) as u8;
            ge = unsafe { cp(state, cargo) };
        }
        (l.write)(v, 5, i64::from(l.cargo_cap(v)));
        let kind = l.vehicle_type(front);
        let speed = match kind {
            0 | 2 => l.cached_max_speed(front),
            1 => l.cached_max_speed(front) / 2,
            3 => l.aircraft_old_speed(front),
            _ => unreachable!(),
        };
        unsafe {
            (&raw mut (*ge).last_speed).write(speed.min(255) as u8);
            (&raw mut (*ge).last_age)
                .write(((l.read)(front, 27) - (l.read)(front, 26)).clamp(0, 255) as u8);
        }
        let mut cap = l.cargo_cap(v).wrapping_sub(l.stored_count(v));
        if cap > 0 {
            unsafe {
                (&raw mut (*ge).time_since_pickup).write(0);
            }
            if (l.load_count(v) > 0 || (w.read)(st, 12, u32::from(cargo)) > 0)
                && may_load(st, v, w, l)
            {
                if l.stored_count(v) == 0 {
                    (l.effect)(st, v, 0, cargo);
                }
                if l.gradual_loading(v) != 0 {
                    cap = cap.min(load_amount(v, l));
                }
                let loaded = (l.cargo)(st, v, 3, cap, next, std::ptr::null_mut());
                if l.load_count(v) > 0 {
                    reservation_left |= 1 << cargo;
                }
                if loaded == cap {
                    full_load |= 1 << cargo;
                } else {
                    full_load &= !(1 << cargo);
                }
                if loaded > 0 {
                    emptied = false;
                    loaded_any = true;
                    unsafe {
                        (&raw mut (*state).fields.time_since_load).write(0);
                        (&raw mut (*state).fields.last_vehicle_type).write(l.vehicle_type(v) as u8);
                    }
                    if (l.cargo)(
                        st,
                        std::ptr::null_mut(),
                        10,
                        u32::from(cargo),
                        std::ptr::null_mut(),
                        std::ptr::null_mut(),
                    ) == 0
                    {
                        (l.effect)(st, v, 1, cargo);
                    }
                    ticks = ticks.wrapping_add(loaded as i32);
                    dirty_vehicle = true;
                    dirty_station = true;
                }
            }
        }
        if l.stored_count(v) >= l.cargo_cap(v) {
            cargo_full |= 1 << cargo;
        } else {
            cargo_not_full |= 1 << cargo;
        }
        v = (l.next)(v, 0);
    }
    if loaded_any || unloaded {
        if l.vehicle_type(front) == 0 {
            (l.effect)(st, front, 2, 0);
        } else if l.vehicle_type(front) == 1 {
            (l.effect)(st, front, 3, 0);
        }
    }
    emptied &= unloaded;
    if !unloaded {
        (l.payment)(payment, 2);
    }
    (l.write)(front, 2, 0);
    if loaded_any || unloaded {
        if l.gradual_loading(front) != 0 {
            ticks = [40, 20, 10, 20][l.vehicle_type(front) as usize];
        }
        let wait = ((l.read)(front, 36) - (l.read)(front, 38)).max(0);
        if !unloaded
            && full_load == 0
            && reservation_left == 0
            && l.full_load_order(front) == 0
            && (l.read)(front, 37) >= wait
        {
            (l.write)(front, 2, 1);
        }
        update_ticks(front, st, ticks, l);
    } else {
        update_ticks(front, st, 20, l);
        let mut finished = true;
        if l.full_load_order(front) != 0 {
            if l.load_type(front) == 3 {
                if (l.vehicle_type(front) == 3
                    && l.passenger_cargo(front) != 0
                    && l.cargo_cap(front) > l.stored_count(front))
                    || (cargo_not_full != 0 && (cargo_full & !cargo_not_full) == 0)
                {
                    finished = false;
                }
            } else if cargo_not_full != 0 {
                finished = false;
            }
            if !finished {
                (l.write)(front, 8, 0);
            }
        }
        (l.write)(front, 1, i64::from(finished));
    }
    (l.write)(front, 10, 0);
    if emptied {
        dirty_vehicle = true;
        (l.effect)(st, front, 4, 0);
    }
    if dirty_vehicle {
        (l.write)(front, 9, 0);
    }
    if dirty_station {
        (l.effect)(st, front, 5, 0);
    }
    (l.next_stations_destroy)(next);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_load(
    st: *mut c_void,
    world: *const World,
    loading: *const Loading,
) {
    let w = unsafe { world.read() };
    let l = unsafe { loading.read() };
    let state = (w.owner)(st);
    let mut node = unsafe { openttd_rust_station_queue_first(state) };
    if node.is_null() {
        return;
    }
    let mut last = std::ptr::null_mut();
    while !node.is_null() {
        let v = unsafe { openttd_rust_station_queue_value(node) };
        if l.stopped_or_crashed(v) == 0 {
            let ticks = l.load_unload_ticks(v).wrapping_sub(1);
            (l.write)(v, 3, i64::from(ticks));
            if ticks == 0 {
                last = v;
            }
        }
        node = unsafe { openttd_rust_station_queue_next(state, node) };
    }
    if last.is_null() {
        return;
    }
    node = unsafe { openttd_rust_station_queue_first(state) };
    while !node.is_null() {
        let v = unsafe { openttd_rust_station_queue_value(node) };
        if l.stopped_or_crashed(v) == 0 {
            unsafe {
                load_vehicle(v, st, &w, &l);
            }
        }
        if v == last {
            break;
        }
        node = unsafe { openttd_rust_station_queue_next(state, node) };
    }
    (l.payment)(std::ptr::null_mut(), 3);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_station_modify_rating(state: *mut Service, amount: i32) {
    for cargo in 0..64 {
        let ge = unsafe { cp(state, cargo) };
        if unsafe { (*ge).status } != 0 {
            unsafe {
                (&raw mut (*ge).rating)
                    .write(i32::from((*ge).rating).wrapping_add(amount).clamp(0, 255) as u8);
            }
        }
    }
}
