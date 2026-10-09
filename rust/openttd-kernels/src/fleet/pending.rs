/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Ascending `VehicleID` tick-end autoreplacement with original overwrite semantics.
use std::cell::UnsafeCell;
use std::collections::BTreeMap;
#[repr(C)]
pub struct Services {
    current: unsafe extern "C" fn() -> u8,
    set_current: unsafe extern "C" fn(u8),
    vehicle: unsafe extern "C" fn(u32) -> *mut (),
    owner: unsafe extern "C" fn(*mut ()) -> u8,
    restart: unsafe extern "C" fn(*mut ()),
    x: unsafe extern "C" fn(*mut ()) -> i32,
    y: unsafe extern "C" fn(*mut ()) -> i32,
    z: unsafe extern "C" fn(*mut ()) -> i32,
    reserve: unsafe extern "C" fn(u8) -> u32,
    subtract: unsafe extern "C" fn(i64),
    command: unsafe extern "C" fn(*mut (), u32),
    local: unsafe extern "C" fn() -> bool,
    success: unsafe extern "C" fn(*mut ()) -> bool,
    money: unsafe extern "C" fn(*mut ()) -> i64,
    error: unsafe extern "C" fn(*mut ()) -> u32,
    animation: unsafe extern "C" fn(i32, i32, i32, i64),
    length_news: unsafe extern "C" fn(u32),
    failed_news: unsafe extern "C" fn(u32, u32),
    nothing: u32,
    cash: u32,
    limit: u32,
    length: u32,
}

struct Pending(UnsafeCell<BTreeMap<u32, bool>>);
// SAFETY: the game thread alone mutates/drains this process-lifetime map. No
// reference is retained across a callback that may insert/overwrite an entry.
unsafe impl Sync for Pending {}
static PENDING: Pending = Pending(UnsafeCell::new(BTreeMap::new()));
/// Clear pending replacements at the original initialization/tick-start point.
/// # Safety
/// Serial game-thread access; no map reference is active during this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_fleet_pending_clear() {
    unsafe { (&mut *PENDING.0.get()).clear() };
}
/// Record or overwrite the original `VehicleID` key's leave-depot flag.
/// # Safety
/// Serial game thread; map accesses end before returning/reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_fleet_pending_add(id: u32, leave: bool) {
    unsafe { (&mut *PENDING.0.get()).insert(id, leave) };
}
/// Drain in ascending IDs; newly inserted later keys remain visible this tick.
/// # Safety
/// Static noexcept services, serial game thread and a native caller-owned cost.
/// The map is accessed only to copy one current item before calling any service.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_fleet_pending_drain(
    services: *const Services,
    cost: *mut (),
) {
    let w = unsafe { &*services };
    let previous = unsafe { (w.current)() };
    let mut from = 0;
    loop {
        let item = unsafe {
            (&*PENDING.0.get())
                .range(from..)
                .next()
                .map(|(&id, &leave)| (id, leave))
        };
        let Some((id, leave)) = item else {
            break;
        };
        from = id + 1;
        let v = unsafe { (w.vehicle)(id) };
        let owner = unsafe { (w.owner)(v) };
        unsafe { (w.set_current)(owner) };
        if leave {
            unsafe { (w.restart)(v) };
        }
        let x = unsafe { (w.x)(v) };
        let y = unsafe { (w.y)(v) };
        let z = unsafe { (w.z)(v) };
        let reserve = i64::from(unsafe { (w.reserve)(owner) });
        unsafe {
            (w.subtract)(reserve);
            (w.command)(cost, id);
            (w.subtract)(reserve.wrapping_neg());
        }
        if !unsafe { (w.local)() } {
            continue;
        }
        if unsafe { (w.success)(cost) } {
            let money = unsafe { (w.money)(cost) };
            unsafe { (w.animation)(x, y, z, money) };
            continue;
        }
        let mut error = unsafe { (w.error)(cost) };
        if error == w.nothing || error == 65535 {
            continue;
        }
        if error == w.cash {
            error = w.limit;
        }
        if error == w.length {
            unsafe { (w.length_news)(id) };
        } else {
            unsafe { (w.failed_news)(id, error) };
        }
    }
    unsafe { (w.set_current)(previous) };
}
