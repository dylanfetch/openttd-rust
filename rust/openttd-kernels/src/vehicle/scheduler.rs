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
pub struct CargoAddresses {
    pub front: VehicleRef,
    pub period: *const u16,
    pub counter: *mut u16,
}
#[repr(C)]
pub struct MotionAddresses {
    pub motion: *mut u32,
    pub speed: *const u16,
    pub front_type: *const VehicleType,
    pub subtype: *const u8,
    pub tick: *const u8,
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
    pub cargo_addresses: extern "C" fn(VehicleRef) -> CargoAddresses,
    pub cargo_owner: extern "C" fn(VehicleRef) -> *mut cargo_storage::List,
    pub motion_addresses: extern "C" fn(VehicleRef, VehicleRef) -> MotionAddresses,
    pub running_sound: extern "C" fn(VehicleRef),
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

// Call-local projection of existing typed service slots, constructed once at
// entry. This is Rust-only; it adds no ABI, callback or world state.
struct PostTickServices {
    cargo_addresses: extern "C" fn(VehicleRef) -> CargoAddresses,
    cargo_owner: extern "C" fn(VehicleRef) -> *mut cargo_storage::List,
    motion_addresses: extern "C" fn(VehicleRef, VehicleRef) -> MotionAddresses,
    running_sound: extern "C" fn(VehicleRef),
    periodic_sound: extern "C" fn(VehicleRef, bool),
    status: extern "C" fn(VehicleRef) -> u8,
    cargo: *const cargo_storage::Services,
}

/// A field-sized raw access ends before the next service. These functions never
/// form references into the native shell. COUNTED=false removes accounting.
unsafe fn scalar_read<T: Copy, const COUNTED: bool>(address: *const T) -> T {
    if COUNTED {
        crate::witness::add(
            crate::witness::VEHICLE_SCALAR_READ_BYTES,
            std::mem::size_of::<T>() as u64,
        );
    }
    unsafe { address.read() }
}
unsafe fn scalar_write<T, const COUNTED: bool>(address: *mut T, value: T) {
    if COUNTED {
        crate::witness::add(
            crate::witness::VEHICLE_SCALAR_WRITE_BYTES,
            std::mem::size_of::<T>() as u64,
        );
    }
    unsafe { address.write(value) };
}
macro_rules! post_svc {
    ($counted:ident, $slot:expr, $function:expr $(, $arg:expr)*) => {{
        if $counted { crate::witness::hit($slot); }
        svc!($counted, $function $(, $arg)*)
    }};
}

/// # Safety
/// Tick survived. Native shells are separately allocated: pool array growth
/// cannot move their scalar members. Cargo packet lookup and `NewGRF` sound
/// resolution may reenter, but cannot destroy these shells, reset the pool or
/// yield the game-state mutex. Only addresses survive those audited callbacks;
/// field loads/stores end before each callback and subsequent values are fresh.
/// Never retain a descriptor across Tick, traversal, loading, day or fleet work.
unsafe fn post_tick<const COUNTED: bool>(
    schedule: &PostTickServices,
    vehicle_type: VehicleType,
    vehicle: VehicleRef,
) {
    if vehicle_type > AIRCRAFT {
        return;
    }
    let cargo = post_svc!(
        COUNTED,
        crate::witness::VEHICLE_CARGO_ADDRESSES,
        schedule.cargo_addresses,
        vehicle
    );
    if COUNTED {
        crate::witness::add(
            crate::witness::VEHICLE_DESCRIPTOR_BYTES,
            std::mem::size_of::<CargoAddresses>() as u64,
        );
    }
    let period = unsafe { scalar_read::<_, COUNTED>(cargo.period) };
    if period != 0 {
        let counter = unsafe { scalar_read::<_, COUNTED>(cargo.counter) }.min(period);
        // Preserve the original clamp store, then unsigned predecrement store.
        unsafe { scalar_write::<_, COUNTED>(cargo.counter, counter) };
        let counter = counter.wrapping_sub(1);
        unsafe { scalar_write::<_, COUNTED>(cargo.counter, counter) };
        if counter == 0 {
            let owner = post_svc!(
                COUNTED,
                crate::witness::VEHICLE_CARGO_OWNER,
                schedule.cargo_owner,
                vehicle
            );
            if COUNTED {
                crate::witness::hit(crate::witness::VEHICLE_CARGO_AGES);
            }
            unsafe { cargo_storage::openttd_rust_cargo_list_age(owner, schedule.cargo) };
            let period = unsafe { scalar_read::<_, COUNTED>(cargo.period) };
            unsafe { scalar_write::<_, COUNTED>(cargo.counter, period) };
        }
    }
    let front_status = post_svc!(
        COUNTED,
        crate::witness::VEHICLE_STATUS,
        schedule.status,
        cargo.front
    );
    if front_status & VS_CRASHED != 0 {
        return;
    }
    if post_svc!(
        COUNTED,
        crate::witness::VEHICLE_STATUS,
        schedule.status,
        vehicle
    ) & VS_HIDDEN
        != 0
    {
        return;
    }
    let motion = post_svc!(
        COUNTED,
        crate::witness::VEHICLE_MOTION_ADDRESSES,
        schedule.motion_addresses,
        vehicle,
        cargo.front
    );
    if COUNTED {
        crate::witness::add(
            crate::witness::VEHICLE_DESCRIPTOR_BYTES,
            std::mem::size_of::<MotionAddresses>() as u64,
        );
    }
    if front_status & VS_STOPPED != 0
        && (unsafe { scalar_read::<_, COUNTED>(motion.front_type) } != TRAIN
            || unsafe { scalar_read::<_, COUNTED>(motion.speed) } == 0)
    {
        return;
    }
    if COUNTED {
        crate::witness::hit(crate::witness::VEHICLE_MOTION);
    }
    let speed = unsafe { scalar_read::<_, COUNTED>(motion.speed) };
    let counter =
        unsafe { scalar_read::<_, COUNTED>(motion.motion) }.wrapping_add(u32::from(speed));
    unsafe { scalar_write::<_, COUNTED>(motion.motion, counter) };
    // Ship has no subtype gate; do not read its subtype.
    match vehicle_type {
        TRAIN if unsafe { scalar_read::<_, COUNTED>(motion.subtype) } & ENGINE == 0 => return,
        ROAD if unsafe { scalar_read::<_, COUNTED>(motion.subtype) } & FRONT == 0 => return,
        AIRCRAFT if unsafe { scalar_read::<_, COUNTED>(motion.subtype) } > NORMAL_AIRCRAFT => {
            return;
        }
        _ => {}
    }
    if unsafe { scalar_read::<_, COUNTED>(motion.motion) } & 0xff
        < u32::from(unsafe { scalar_read::<_, COUNTED>(motion.speed) })
    {
        post_svc!(
            COUNTED,
            crate::witness::VEHICLE_RUNNING_SOUND,
            schedule.running_sound,
            vehicle
        );
    }
    // Running sound can change the following observations; load after it returns.
    if unsafe { scalar_read::<_, COUNTED>(motion.tick) } & 15 == 0 {
        let running = unsafe { scalar_read::<_, COUNTED>(motion.speed) } > 0
            && post_svc!(
                COUNTED,
                crate::witness::VEHICLE_STATUS,
                schedule.status,
                cargo.front
            ) & (VS_STOPPED | TRAIN_SLOWING)
                == 0;
        post_svc!(
            COUNTED,
            crate::witness::VEHICLE_PERIODIC_SOUND,
            schedule.periodic_sound,
            vehicle,
            running
        );
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
    let post = PostTickServices {
        cargo_addresses: schedule.cargo_addresses,
        cargo_owner: schedule.cargo_owner,
        motion_addresses: schedule.motion_addresses,
        running_sound: schedule.running_sound,
        periodic_sound: schedule.periodic_sound,
        status: unsafe { (*(*tables.road).vehicle).status },
        cargo: tables.cargo,
    };
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
        unsafe { post_tick::<COUNTED>(&post, dispatch.vehicle_type, vehicle) };
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

#[cfg(test)]
mod tests {
    use super::*;
    use std::ptr::{NonNull, null, null_mut};

    // Only test-owned scalar cells, never a replica of the native Vehicle layout.
    #[allow(clippy::struct_excessive_bools)] // Independent inaccessible-field probes.
    struct Cells {
        period: u16,
        counter: u16,
        motion: u32,
        speed: u16,
        kind: VehicleType,
        subtype: u8,
        tick: u8,
        front_status: u8,
        status: u8,
        status_calls: u8,
        hide_counter: bool,
        hide_motion: bool,
        hide_speed: bool,
        hide_subtype: bool,
        hide_tick: bool,
        hide_kind: bool,
        sound_changes: u8,
        packet_reloads: bool,
        events: Vec<&'static str>,
        running: Option<bool>,
        cargo: *mut cargo_storage::List,
        packet: cargo_storage::Packet,
    }
    impl Cells {
        fn new() -> Self {
            Self {
                period: 0,
                counter: 9,
                motion: 0,
                speed: 0,
                kind: ROAD,
                subtype: FRONT,
                tick: 1,
                front_status: 0,
                status: 0,
                status_calls: 0,
                hide_counter: false,
                hide_motion: false,
                hide_speed: false,
                hide_subtype: false,
                hide_tick: false,
                hide_kind: false,
                sound_changes: 0,
                packet_reloads: false,
                events: Vec::new(),
                running: None,
                cargo: null_mut(),
                packet: cargo_storage::Packet {
                    feeder_share: 0,
                    source_xy: 0,
                    count: 1,
                    periods_in_transit: 0,
                    first_station: 0,
                    next_hop: 0,
                    source_id: 0,
                    travelled_x: 0,
                    travelled_y: 0,
                    source_type: 0,
                    in_vehicle: 1,
                },
            }
        }
        fn handle(&mut self) -> VehicleRef {
            VehicleRef(NonNull::from(self).cast())
        }
        fn run(&mut self, kind: VehicleType) {
            let handle = self.handle();
            let services = PostTickServices {
                cargo_addresses,
                cargo_owner,
                motion_addresses,
                running_sound,
                periodic_sound,
                status,
                cargo: &raw const CARGO_SERVICES,
            };
            unsafe { post_tick::<false>(&services, kind, handle) };
        }
    }
    unsafe fn cells(v: VehicleRef) -> *mut Cells {
        v.0.as_ptr().cast()
    }
    extern "C" fn cargo_addresses(v: VehicleRef) -> CargoAddresses {
        let c = unsafe { &mut *cells(v) };
        c.events.push("cargo");
        CargoAddresses {
            front: v,
            period: &raw const c.period,
            counter: if c.hide_counter {
                null_mut()
            } else {
                &raw mut c.counter
            },
        }
    }
    extern "C" fn cargo_owner(v: VehicleRef) -> *mut cargo_storage::List {
        let c = unsafe { &mut *cells(v) };
        c.events.push("age-owner");
        c.cargo
    }
    extern "C" fn status(v: VehicleRef) -> u8 {
        let c = unsafe { &mut *cells(v) };
        c.status_calls += 1;
        c.events.push("status");
        if c.status_calls == 2 {
            c.status
        } else {
            c.front_status
        }
    }
    extern "C" fn motion_addresses(v: VehicleRef, _: VehicleRef) -> MotionAddresses {
        let c = unsafe { &mut *cells(v) };
        c.events.push("motion");
        MotionAddresses {
            motion: if c.hide_motion {
                null_mut()
            } else {
                &raw mut c.motion
            },
            speed: if c.hide_speed {
                null()
            } else {
                &raw const c.speed
            },
            front_type: if c.hide_kind {
                null()
            } else {
                &raw const c.kind
            },
            subtype: if c.hide_subtype {
                null()
            } else {
                &raw const c.subtype
            },
            tick: if c.hide_tick {
                null()
            } else {
                &raw const c.tick
            },
        }
    }
    extern "C" fn running_sound(v: VehicleRef) {
        let c = unsafe { &mut *cells(v) };
        c.events.push("running-sound");
        match c.sound_changes {
            1 => {
                c.speed = 0;
                c.tick = 0;
                c.front_status = VS_STOPPED;
            }
            2 => {
                c.speed = 3;
                c.tick = 0;
                c.front_status = VS_STOPPED;
            }
            3 => {
                c.tick = 1;
            }
            _ => {}
        }
    }
    extern "C" fn periodic_sound(v: VehicleRef, running: bool) {
        let c = unsafe { &mut *cells(v) };
        c.events.push("periodic-sound");
        c.running = Some(running);
    }
    extern "C" fn packet(shell: *mut c_void) -> *mut cargo_storage::Packet {
        let c = unsafe { &mut *shell.cast::<Cells>() };
        if c.packet_reloads {
            c.period = 17;
            c.events.push("age-packet");
        }
        &raw mut c.packet
    }
    extern "C" fn destroy(_: *mut c_void) {}
    extern "C" fn can_allocate() -> u8 {
        1
    }
    extern "C" fn create(_: *const cargo_storage::Packet) -> *mut c_void {
        null_mut()
    }
    extern "C" fn random(_: u32) -> u32 {
        0
    }
    extern "C" fn coordinate(_: u32, _: u8) -> u32 {
        0
    }
    extern "C" fn flow(
        _: *const c_void,
        _: u8,
        _: u16,
        _: u16,
        _: u16,
        _: *const u16,
        _: usize,
        _: *mut u8,
    ) -> u16 {
        0
    }
    extern "C" fn pay(_: *mut c_void, _: u8, _: u8, _: *const c_void, _: u32, _: u32) -> i64 {
        0
    }
    extern "C" fn origin(_: *mut c_void, _: u16, _: u32, _: u8) {}
    extern "C" fn flow_owner(_: *const c_void, _: u16) -> *mut crate::cargo_flow::Flow {
        null_mut()
    }
    extern "C" fn random_draw() -> u32 {
        0
    }
    extern "C" fn packet_next(_: *mut c_void) -> *mut c_void {
        null_mut()
    }
    static CARGO_SERVICES: cargo_storage::Services = cargo_storage::Services {
        can_allocate,
        create,
        packet,
        destroy,
        random,
        coordinate,
        flow,
        pay,
        origin,
        flow_owner,
        random_draw,
        packet_next,
    };

    #[test]
    fn cargo_short_circuit_wrap_clamp_and_period_reload() {
        let mut c = Cells::new();
        c.front_status = VS_CRASHED;
        c.hide_counter = true; // Source never accesses the counter when period=0.
        c.run(ROAD);
        assert_eq!(c.counter, 9);
        assert_eq!(c.events, ["cargo", "status"]);
        for (counter, expected) in [(0, u16::MAX), (9, 3)] {
            let mut c = Cells::new();
            c.period = 4;
            c.counter = counter;
            c.front_status = VS_CRASHED;
            c.run(ROAD);
            assert_eq!(c.counter, expected);
            assert_eq!(c.events, ["cargo", "status"]);
        }
        let mut c = Cells::new();
        c.period = 4;
        c.counter = 1;
        c.front_status = VS_CRASHED;
        c.cargo = cargo_storage::openttd_rust_cargo_list_new(1);
        let shell = c.handle().0.as_ptr().cast();
        unsafe {
            cargo_storage::openttd_rust_cargo_list_append(
                c.cargo,
                &raw const CARGO_SERVICES,
                shell,
                2, // Cargo keep action, as in the existing cargo-storage fixture.
            );
        };
        c.packet_reloads = true;
        c.run(ROAD);
        assert_eq!(c.counter, 17); // Packet callback changes the later period read.
        assert_eq!(c.packet.periods_in_transit, 1);
        assert_eq!(c.events, ["cargo", "age-owner", "age-packet", "status"]);
        unsafe {
            cargo_storage::openttd_rust_cargo_list_destroy(c.cargo, &raw const CARGO_SERVICES);
        };
    }

    #[test]
    fn hidden_stopped_and_type_gates_skip_inaccessible_members() {
        let mut c = Cells::new();
        c.status = VS_HIDDEN;
        c.hide_motion = true;
        c.run(ROAD);
        assert_eq!(c.events, ["cargo", "status", "status"]);
        for (kind, speed) in [(ROAD, 23), (TRAIN, 0)] {
            let mut c = Cells::new();
            c.kind = kind;
            c.speed = speed;
            c.front_status = VS_STOPPED;
            c.hide_motion = true;
            c.hide_subtype = true;
            c.hide_tick = true;
            c.hide_speed = kind == ROAD; // Native || skips speed on a stopped road.
            c.run(kind);
            assert_eq!(c.events, ["cargo", "status", "status", "motion"]);
        }
        for (kind, subtype) in [(ROAD, 0), (TRAIN, 0), (AIRCRAFT, 3)] {
            let mut c = Cells::new();
            c.speed = 5;
            c.motion = u32::MAX;
            c.subtype = subtype;
            c.hide_kind = true;
            c.hide_tick = true;
            c.run(kind);
            assert_eq!(c.motion, 4); // Original unsigned motion wraps.
            assert_eq!(c.events, ["cargo", "status", "status", "motion"]);
        }
    }

    #[test]
    fn fresh_sound_observations_and_ship_subtype_short_circuit() {
        let mut c = Cells::new();
        c.speed = 8;
        c.motion = 252;
        c.sound_changes = 1;
        c.run(ROAD);
        assert_eq!(c.motion, 260);
        assert_eq!(c.running, Some(false));
        assert_eq!(c.status_calls, 2); // Fresh speed=0 skips periodic status.
        assert_eq!(
            c.events,
            [
                "cargo",
                "status",
                "status",
                "motion",
                "running-sound",
                "periodic-sound"
            ]
        );
        for change in [2, 3] {
            let mut c = Cells::new();
            c.speed = 8;
            c.motion = 252;
            c.tick = 0;
            c.sound_changes = change;
            c.run(ROAD);
            if change == 2 {
                assert_eq!(c.running, Some(false));
                assert_eq!(c.status_calls, 3); // Fresh positive speed reads changed status.
            } else {
                assert_eq!(c.running, None); // Changed tick skips periodic observations.
                assert_eq!(c.status_calls, 2);
            }
        }
        let mut c = Cells::new();
        c.speed = 5;
        c.motion = 17;
        c.tick = 0;
        c.hide_subtype = true;
        c.hide_kind = true;
        c.run(SHIP);
        assert_eq!(c.running, Some(true));
        assert_eq!(c.status_calls, 3);
        let mut c = Cells::new();
        c.front_status = VS_STOPPED;
        c.kind = TRAIN;
        c.speed = 5;
        c.subtype = ENGINE;
        c.tick = 0;
        c.motion = 17;
        c.run(TRAIN); // A coasting stopped train still increments its motion.
        assert_eq!(c.motion, 22);
        assert_eq!(c.running, Some(false));
    }
}
