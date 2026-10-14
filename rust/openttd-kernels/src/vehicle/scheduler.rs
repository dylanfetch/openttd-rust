/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Live base-vehicle scheduling prototype. Owners stay opaque and call-local;
//! no Rust world/owner reference survives a service that can mutate or reenter.
#![allow(unsafe_code, clippy::too_many_lines, clippy::verbose_bit_mask)]
use super::{CALLBACK_FAILED, VS_CRASHED, VS_HIDDEN, VS_STOPPED, VehicleRef};
use crate::{aircraft, cargo_storage, fleet, road, services, ship_control, station_service, train};
use std::ffi::c_void;

/// The native `VehicleType` representation, including its non-pool sentinels.
#[allow(dead_code)] // These variants are also constructed by the native pool leaf.
#[repr(u8)]
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum VehicleType {
    Train = 0,
    Road = 1,
    Ship = 2,
    Aircraft = 3,
    Effect = 4,
    Disaster = 5,
    End = 6,
    Invalid = 255,
}
const TRAIN: VehicleType = VehicleType::Train;
const ROAD: VehicleType = VehicleType::Road;
const SHIP: VehicleType = VehicleType::Ship;
const AIRCRAFT: VehicleType = VehicleType::Aircraft;
const FRONT: u8 = 1;
const ENGINE: u8 = 1 << 3;
const NORMAL_AIRCRAFT: u8 = 2;
const TRAIN_SLOWING: u8 = 1 << 4;
const DAY_TICKS: u32 = 74;

#[repr(C)]
#[derive(Clone, Copy)]
pub union TickOwner {
    pub road: *mut road::State,
    pub train: *mut crate::train_state::State,
    pub ship: *mut ship_control::State,
    pub aircraft: *mut aircraft::State,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Dispatch {
    pub vehicle: Option<VehicleRef>,
    pub owner: TickOwner,
    pub id: u32,
    pub vehicle_type: VehicleType,
    pub subtype: u8,
    pub road_tick: u8,
}
#[repr(C)]
pub struct DayLookup {
    pub vehicle: Option<VehicleRef>,
    pub end: bool,
}
#[repr(C)]
pub struct DayCallback {
    pub day: u8,
    pub has_engine: bool,
}
#[repr(C)]
pub struct CargoTick {
    pub front: VehicleRef,
    pub cargo: *mut cargo_storage::List,
    pub period: u16,
    pub counter: u16,
}
#[repr(C)]
pub struct MotionTick {
    pub motion: u32,
    pub speed: u16,
    pub front_status: u8,
    pub status: u8,
    pub front_type: VehicleType,
}
#[repr(C)]
pub struct SoundTick {
    pub motion: u32,
    pub speed: u16,
    pub subtype: u8,
}
#[repr(C)]
pub struct PeriodicSound {
    pub speed: u16,
    pub tick: u8,
    pub front_status: u8,
}
#[repr(C)]
pub struct StationLookup {
    pub station: *mut c_void,
    pub id: u32,
}
#[repr(C)]
pub struct SchedulerServices {
    pub next: extern "C" fn(u32) -> Dispatch,
    pub day_at: extern "C" fn(u32) -> DayLookup,
    pub dispatch: extern "C" fn(VehicleRef) -> Dispatch,
    pub day_callback_read: extern "C" fn(VehicleRef) -> DayCallback,
    pub day_callback: extern "C" fn(VehicleRef) -> u16,
    pub randomise_callback: extern "C" fn(VehicleRef),
    pub first_dirty: extern "C" fn(VehicleRef),
    pub unknown_day_callback: extern "C" fn(VehicleRef, u16),
    pub other_tick: extern "C" fn(VehicleRef) -> bool,
    pub assert_survival: Option<extern "C" fn(VehicleRef, u32, bool)>,
    pub cargo_read: extern "C" fn(VehicleRef) -> CargoTick,
    pub write_cargo_counter: extern "C" fn(VehicleRef, u16),
    pub cargo_period: extern "C" fn(VehicleRef) -> u16,
    pub motion_read: extern "C" fn(VehicleRef, VehicleRef) -> MotionTick,
    pub write_motion: extern "C" fn(VehicleRef, u32),
    pub sound_read: extern "C" fn(VehicleRef, VehicleRef) -> SoundTick,
    pub running_sound: extern "C" fn(VehicleRef),
    pub periodic_read: extern "C" fn(VehicleRef, VehicleRef) -> PeriodicSound,
    pub periodic_sound: extern "C" fn(VehicleRef, bool),
    pub next_station: extern "C" fn(u32) -> StationLookup,
    pub economy_begin: extern "C" fn() -> u64,
    pub economy_end: extern "C" fn(u64),
    pub reset_accumulators: extern "C" fn(),
    pub train_begin: extern "C" fn() -> u64,
    pub road_begin: extern "C" fn() -> u64,
    pub ship_begin: extern "C" fn() -> u64,
    pub aircraft_begin: extern "C" fn() -> u64,
    pub train_end: extern "C" fn(u64),
    pub road_end: extern "C" fn(u64),
    pub ship_end: extern "C" fn(u64),
    pub aircraft_end: extern "C" fn(u64),
}
#[repr(C)]
pub struct Tables {
    pub scheduler: *const SchedulerServices,
    pub road: *const road::Leaves,
    pub train: *const train::Leaves,
    pub ship: *const ship_control::Leaves,
    pub aircraft: *const aircraft::Leaves,
    pub shared: *const services::Services,
    pub cargo: *const cargo_storage::Services,
    pub station: *const station_service::World,
    pub loading: *const station_service::Loading,
    pub pending: *const fleet::pending::Services,
    pub transaction: *const fleet::transactions::Services,
    pub group: *const fleet::GroupServices,
}

// Select counted/plain implementations once at entry. In the plain monomorph,
// COUNTED=false erases every new per-visit witness branch/load/update.
macro_rules! svc {
    ($counted:ident, $function:expr $(, $arg:expr)*) => {{
        let result = ($function)($($arg),*);
        if $counted {
            crate::witness::hit(crate::witness::VEHICLE_CROSSINGS);
            crate::witness::add(crate::witness::VEHICLE_OBSERVED_BYTES, std::mem::size_of_val(&result) as u64);
        }
        result
    }};
}

/// Rust chooses the type gate and the native timing scope before direct dispatch.
/// # Safety
/// Live shell/owner, with immutable tables throughout this synchronous call.
unsafe fn tick<const COUNTED: bool>(
    tables: &Tables,
    schedule: &SchedulerServices,
    dispatch: Dispatch,
    vehicle: VehicleRef,
) -> bool {
    let measured = match dispatch.vehicle_type {
        TRAIN => dispatch.subtype & FRONT != 0,
        ROAD | SHIP => true,
        AIRCRAFT => dispatch.subtype <= NORMAL_AIRCRAFT,
        _ => false,
    };
    if dispatch.vehicle_type == AIRCRAFT && !measured {
        return true;
    }
    let (begin, end) = match dispatch.vehicle_type {
        TRAIN => (schedule.train_begin, schedule.train_end),
        ROAD => (schedule.road_begin, schedule.road_end),
        SHIP => (schedule.ship_begin, schedule.ship_end),
        AIRCRAFT => (schedule.aircraft_begin, schedule.aircraft_end),
        _ => {
            if COUNTED {
                crate::witness::hit(crate::witness::VEHICLE_TAIL_CROSSINGS);
            }
            return svc!(COUNTED, schedule.other_tick, vehicle);
        }
    };
    let start = if measured { svc!(COUNTED, begin) } else { 0 };
    // SAFETY: The type tag selects the live native owner; controller helpers
    // retain no owner borrow across their direct shared-service calls.
    let survives = unsafe {
        match dispatch.vehicle_type {
            TRAIN => {
                train::tick(
                    train::Handle {
                        shell: vehicle.0.as_ptr().cast(),
                        owner: dispatch.owner.train,
                    },
                    tables.train,
                    tables.shared,
                ) != 0
            }
            ROAD => road::tick(
                vehicle,
                dispatch.owner.road,
                &road::TickRead {
                    tick: dispatch.road_tick,
                    front: dispatch.subtype & FRONT != 0,
                },
                tables.road,
            ),
            SHIP => ship_control::tick_resolved(
                dispatch.id,
                dispatch.owner.ship,
                tables.ship,
                tables.shared,
            ),
            AIRCRAFT => aircraft::tick(
                tables.aircraft,
                aircraft::Vehicle {
                    handle: vehicle.0.as_ptr().cast(),
                    state: dispatch.owner.aircraft,
                    id: dispatch.id,
                },
            ),
            _ => svc!(COUNTED, schedule.other_tick, vehicle),
        }
    };
    if measured {
        svc!(COUNTED, end, start);
    }
    survives
}

/// # Safety
/// The live day lookup and callbacks preserve the original current-shell lifetime.
unsafe fn day<const COUNTED: bool>(
    tables: &Tables,
    schedule: &SchedulerServices,
    calendar: bool,
    mut id: u32,
) {
    loop {
        let lookup = svc!(COUNTED, schedule.day_at, id);
        if lookup.end {
            break;
        }
        id += DAY_TICKS;
        let Some(vehicle) = lookup.vehicle else {
            continue;
        };
        if COUNTED {
            crate::witness::hit(crate::witness::VEHICLE_DAYS);
        }
        if !calendar {
            let callback_state = svc!(COUNTED, schedule.day_callback_read, vehicle);
            if callback_state.day & 0x1f == 0 && callback_state.has_engine {
                if COUNTED {
                    crate::witness::hit(crate::witness::VEHICLE_CALLBACK32);
                }
                let callback = svc!(COUNTED, schedule.day_callback, vehicle);
                if callback != CALLBACK_FAILED {
                    if callback & 1 != 0 {
                        svc!(COUNTED, schedule.randomise_callback, vehicle);
                    }
                    if callback != 0 {
                        svc!(COUNTED, schedule.first_dirty, vehicle);
                    }
                    if callback & !3 != 0 {
                        svc!(COUNTED, schedule.unknown_day_callback, vehicle, callback);
                    }
                }
            }
        }
        // Resolve controller owner/gates after all 32-day callback effects.
        let dispatch = svc!(COUNTED, schedule.dispatch, vehicle);
        // SAFETY: The current type owns exactly the selected opaque owner.
        unsafe {
            match (dispatch.vehicle_type, calendar) {
                (TRAIN, true) => train::calendar_day(
                    train::Handle {
                        shell: vehicle.0.as_ptr().cast(),
                        owner: dispatch.owner.train,
                    },
                    tables.train,
                    tables.shared,
                ),
                (TRAIN, false) => train::economy_day(
                    train::Handle {
                        shell: vehicle.0.as_ptr().cast(),
                        owner: dispatch.owner.train,
                    },
                    tables.train,
                    tables.shared,
                ),
                (ROAD, true) => {
                    road::calendar_day(vehicle, dispatch.subtype & FRONT != 0, tables.road);
                }
                (ROAD, false) => road::economy_day(
                    vehicle,
                    dispatch.owner.road,
                    dispatch.subtype & FRONT != 0,
                    tables.road,
                ),
                (SHIP, true) => ship_control::calendar_day(dispatch.id, tables.ship, tables.shared),
                (SHIP, false) => {
                    ship_control::economy_day_resolved(
                        dispatch.id,
                        dispatch.owner.ship,
                        tables.ship,
                        tables.shared,
                    );
                }
                (AIRCRAFT, true) => aircraft::calendar_day(
                    tables.aircraft,
                    aircraft::Vehicle {
                        handle: vehicle.0.as_ptr().cast(),
                        state: dispatch.owner.aircraft,
                        id: dispatch.id,
                    },
                ),
                (AIRCRAFT, false) => aircraft::economy_day(
                    tables.aircraft,
                    aircraft::Vehicle {
                        handle: vehicle.0.as_ptr().cast(),
                        state: dispatch.owner.aircraft,
                        id: dispatch.id,
                    },
                ),
                _ => {} // Effect/disaster inherit Vehicle'schedule empty day methods.
            }
        }
    }
}

/// # Safety
/// The vehicle survived Tick; the saved First shell survives the original aging/sound services.
unsafe fn post_tick<const COUNTED: bool>(
    tables: &Tables,
    schedule: &SchedulerServices,
    dispatch: Dispatch,
    vehicle: VehicleRef,
) {
    if dispatch.vehicle_type > AIRCRAFT {
        return;
    }
    let cargo = svc!(COUNTED, schedule.cargo_read, vehicle);
    if cargo.period != 0 {
        // The original unsigned --counter underflows when the counter is zero.
        let counter = cargo.counter.min(cargo.period).wrapping_sub(1);
        svc!(COUNTED, schedule.write_cargo_counter, vehicle, counter);
        if counter == 0 {
            // SAFETY: Canonical list owner; no owner/world borrow spans packet services.
            if COUNTED {
                crate::witness::hit(crate::witness::VEHICLE_CARGO_AGES);
            }
            unsafe { cargo_storage::openttd_rust_cargo_list_age(cargo.cargo, tables.cargo) };
            svc!(
                COUNTED,
                schedule.write_cargo_counter,
                vehicle,
                svc!(COUNTED, schedule.cargo_period, vehicle)
            );
        }
    }
    let motion = svc!(COUNTED, schedule.motion_read, vehicle, cargo.front);
    if motion.front_status & VS_CRASHED != 0 || motion.status & VS_HIDDEN != 0 {
        return;
    }
    if motion.front_status & VS_STOPPED != 0 && (motion.front_type != TRAIN || motion.speed == 0) {
        return;
    }
    if COUNTED {
        crate::witness::hit(crate::witness::VEHICLE_MOTION);
    }
    svc!(
        COUNTED,
        schedule.write_motion,
        vehicle,
        motion.motion.wrapping_add(u32::from(motion.speed))
    );
    let sound = svc!(COUNTED, schedule.sound_read, vehicle, cargo.front);
    match dispatch.vehicle_type {
        TRAIN if sound.subtype & ENGINE == 0 => return,
        ROAD if sound.subtype & FRONT == 0 => return,
        AIRCRAFT if sound.subtype > NORMAL_AIRCRAFT => return,
        _ => {}
    }
    if sound.motion & 0xff < u32::from(sound.speed) {
        svc!(COUNTED, schedule.running_sound, vehicle);
    }
    // Running sound may reenter; the 16-tick decision observes fields afterward.
    let periodic = svc!(COUNTED, schedule.periodic_read, vehicle, cargo.front);
    if periodic.tick & 15 == 0 {
        let running =
            periodic.speed > 0 && periodic.front_status & (VS_STOPPED | TRAIN_SLOWING) == 0;
        svc!(COUNTED, schedule.periodic_sound, vehicle, running);
    }
}

/// # Safety
/// Immutable tables, serialized live pools and the native caller-owned `CommandCost`
/// remain valid until return. No borrowed world storage crosses a service/reentry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_vehicle_calendar_day(
    tables: *const Tables,
    normal: bool,
    date_fract: u32,
) {
    if !normal {
        return;
    }
    // SAFETY: The native facade supplies immutable live tables for this call.
    let tables = unsafe { &*tables };
    if crate::witness::enabled() {
        unsafe { day::<true>(tables, &*tables.scheduler, true, date_fract) };
    } else {
        unsafe { day::<false>(tables, &*tables.scheduler, true, date_fract) };
    }
}
/// # Safety
/// As the calendar entry; cost is native stack storage retained throughout fleet drain.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_vehicle_ticks(
    tables: *const Tables,
    normal: bool,
    date_fract: u32,
    cost: *mut (),
) {
    if crate::witness::enabled() {
        unsafe { run_ticks::<true>(tables, normal, date_fract, cost) };
    } else {
        unsafe { run_ticks::<false>(tables, normal, date_fract, cost) };
    }
}

unsafe fn run_ticks<const COUNTED: bool>(
    tables: *const Tables,
    normal: bool,
    date_fract: u32,
    cost: *mut (),
) {
    // SAFETY: Facade supplies immutable live tables and serialized pools.
    let tables = unsafe { &*tables };
    let schedule = unsafe { &*tables.scheduler };
    unsafe { fleet::pending::openttd_rust_fleet_pending_clear() };
    if normal {
        unsafe { day::<COUNTED>(tables, schedule, false, date_fract) };
    }
    let start = svc!(COUNTED, schedule.economy_begin);
    let mut station_from = 0;
    loop {
        let station = svc!(COUNTED, schedule.next_station, station_from);
        if station.station.is_null() {
            break;
        }
        station_from = station.id + 1;
        if COUNTED {
            crate::witness::hit(crate::witness::VEHICLE_STATIONS);
        }
        // SAFETY: Canonical live station, queue and immutable loading services.
        unsafe {
            station_service::openttd_rust_station_load(
                station.station,
                tables.station,
                tables.loading,
            );
        };
    }
    svc!(COUNTED, schedule.economy_end, start);
    svc!(COUNTED, schedule.reset_accumulators);
    let mut from = 0;
    loop {
        let dispatch = svc!(COUNTED, schedule.next, from);
        let Some(vehicle) = dispatch.vehicle else {
            break;
        };
        from = dispatch.id + 1;
        if COUNTED {
            crate::witness::hit(crate::witness::VEHICLE_VISITS + dispatch.vehicle_type as usize);
            let primary = match dispatch.vehicle_type {
                TRAIN | ROAD => dispatch.subtype & FRONT != 0,
                SHIP => true,
                AIRCRAFT => dispatch.subtype <= NORMAL_AIRCRAFT,
                _ => false,
            };
            if primary {
                crate::witness::hit(
                    crate::witness::VEHICLE_PRIMARY + dispatch.vehicle_type as usize,
                );
            }
        }
        // SAFETY: Pool leaf returned the live current shell/owner.
        let survives = unsafe { tick::<COUNTED>(tables, schedule, dispatch, vehicle) };
        // The native slot is absent exactly when the source asserts compile out.
        if let Some(assert_survival) = schedule.assert_survival {
            svc!(COUNTED, assert_survival, vehicle, dispatch.id, survives);
        }
        if !survives {
            if COUNTED {
                crate::witness::hit(crate::witness::VEHICLE_DELETED);
            }
            continue;
        }
        unsafe { post_tick::<COUNTED>(tables, schedule, dispatch, vehicle) };
    }
    // SAFETY: No owner/world borrow spans commands in the canonical fleet drain.
    unsafe {
        fleet::pending::openttd_rust_fleet_pending_drain(
            tables.pending,
            tables.transaction,
            tables.group,
            cost,
        );
    };
}
