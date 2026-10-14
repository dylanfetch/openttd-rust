/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Canonical engine lifecycle state. Native shells own this allocation through
//! ordinary/indexed construction, temporary staging and vector relocation.
//! Native scalar aliases carry their real widths. No Rust reference is retained
//! across a native callback. Allocation failure and panics abort; no unwinding.
#![allow(
    unsafe_code,
    clippy::cast_ptr_alignment,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::too_many_lines,
    clippy::struct_field_names,
    clippy::struct_excessive_bools,
    clippy::fn_params_excessive_bools
)]
use crate::services::ByteSpan;
#[repr(C)]
#[derive(Default, Clone, Copy)]
pub struct Lifecycle {
    pub company_avail: u16,
    pub company_hidden: u16,
    pub preview_asked: u16,
    pub intro_date: i32,
    pub age: i32,
    pub reliability: u16,
    pub reliability_spd_dec: u16,
    pub reliability_start: u16,
    pub reliability_max: u16,
    pub reliability_final: u16,
    pub duration_phase_1: u16,
    pub duration_phase_2: u16,
    pub duration_phase_3: u16,
    pub flags: u8,
    pub preview_company: u8,
    pub preview_wait: u8,
}
#[repr(C)]
struct State {
    lifecycle: Lifecycle,
    name: Vec<u8>,
}
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_engine_new() -> *mut Lifecycle {
    Box::into_raw(Box::new(State {
        lifecycle: Lifecycle {
            preview_company: 255,
            ..Lifecycle::default()
        },
        name: Vec::new(),
    }))
    .cast()
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_destroy(state: *mut Lifecycle) {
    // SAFETY: The shell transfers its sole live allocation at destruction.
    drop(unsafe { Box::from_raw(state.cast::<State>()) });
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_name(state: *const Lifecycle) -> ByteSpan {
    // SAFETY: Caller keeps the live owner stable until the borrowed bytes expire.
    let name = unsafe { &(*state.cast::<State>()).name };
    ByteSpan {
        data: name.as_ptr(),
        size: name.len(),
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_set_name(state: *mut Lifecycle, text: ByteSpan) {
    // SAFETY: Owner is live and serialized; source may alias this owner's name,
    // so copy before replacing. Empty spans may have a null pointer.
    let name = if text.size == 0 {
        Vec::new()
    } else {
        unsafe { std::slice::from_raw_parts(text.data, text.size) }.to_vec()
    };
    unsafe {
        (*state.cast::<State>()).name = name;
    }
}

use crate::services::{RandomServices, Services as SharedServices};
use crate::vehicle::{OpenTTDEngine, OpenTTDVehicle};
use std::ffi::c_void;
use std::ptr::{addr_of, addr_of_mut};
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EngineRef {
    pub shell: *const OpenTTDEngine,
    pub state: *mut Lifecycle,
    pub id: u16,
    pub kind: u8,
}
#[repr(C)]
pub struct SyncVariant {
    pub parent: u16,
    pub sync: bool,
}
#[repr(C)]
pub struct DisplayVariant {
    pub parent: u16,
    pub display: u16,
}
#[repr(C)]
pub struct PreviewVariant {
    pub parent: u16,
    pub join: bool,
}
#[repr(C)]
pub struct Reliability {
    pub base_life: u8,
    pub retire_early: i8,
}
#[repr(C)]
pub struct Startup {
    pub base_intro: i32,
    pub grfid: u32,
    pub decay_speed: u8,
    pub base_life: u8,
    pub climate: bool,
}
#[repr(C)]
pub struct SeedConfig {
    pub base_intro: i32,
    pub lifelength: i32,
    pub decay_speed: u8,
    pub retire_early: i8,
}
#[repr(C)]
pub struct AgingYear {
    pub base_intro: i32,
    pub lifelength: i32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Ymd {
    pub year: i32,
    pub month: u8,
    pub day: u8,
}
#[repr(C)]
pub struct StartSettings {
    pub generation_seed: u32,
    pub starting_year: i32,
    pub date: i32,
}
#[repr(C)]
pub struct Company {
    pub shell: *const c_void,
    pub id: u8,
}
#[repr(C)]
pub struct CompanyPreview {
    pub blocked: u8,
    pub performance: i32,
}
#[repr(C)]
pub struct Vehicle {
    pub shell: *mut OpenTTDVehicle,
    pub id: u32,
    pub owner: u8,
    pub kind: u8,
}
#[repr(C)]
pub struct VehicleCargo {
    pub engine: *const OpenTTDEngine,
    pub cargo: u8,
}
#[repr(C)]
pub struct Services {
    pub shared: *const SharedServices,
    pub rng: *const RandomServices,
    pub next: unsafe extern "C" fn(u32) -> EngineRef,
    pub resolve: unsafe extern "C" fn(u16) -> EngineRef,
    pub sync_variant: unsafe extern "C" fn(*const OpenTTDEngine) -> SyncVariant,
    pub display_variant: unsafe extern "C" fn(*const OpenTTDEngine) -> DisplayVariant,
    pub preview_variant: unsafe extern "C" fn(*const OpenTTDEngine) -> PreviewVariant,
    pub extra_flags: unsafe extern "C" fn(*const OpenTTDEngine) -> u8,
    pub engine_id: unsafe extern "C" fn(*const OpenTTDEngine) -> u16,
    pub reliability: unsafe extern "C" fn(*const OpenTTDEngine) -> Reliability,
    pub startup: unsafe extern "C" fn(*const OpenTTDEngine) -> Startup,
    pub seed_config: unsafe extern "C" fn(*const OpenTTDEngine) -> SeedConfig,
    pub base_intro: unsafe extern "C" fn(*const OpenTTDEngine) -> i32,
    pub aging_year: unsafe extern "C" fn(*const OpenTTDEngine) -> AgingYear,
    pub calendar_year: unsafe extern "C" fn() -> i32,
    pub never_expire: unsafe extern "C" fn() -> bool,
    pub start_settings: unsafe extern "C" fn() -> StartSettings,
    pub train_limit: unsafe extern "C" fn() -> u16,
    pub road_limit: unsafe extern "C" fn() -> u16,
    pub ship_limit: unsafe extern "C" fn() -> u16,
    pub aircraft_limit: unsafe extern "C" fn() -> u16,
    pub ai_train_disabled: unsafe extern "C" fn() -> bool,
    pub ai_road_disabled: unsafe extern "C" fn() -> bool,
    pub ai_ship_disabled: unsafe extern "C" fn() -> bool,
    pub ai_aircraft_disabled: unsafe extern "C" fn() -> bool,
    pub date_to_ymd: unsafe extern "C" fn(i32) -> Ymd,
    pub ymd_to_date: unsafe extern "C" fn(i32, u8, u8) -> i32,
    pub enabled: unsafe extern "C" fn(*const OpenTTDEngine) -> bool,
    pub climate: unsafe extern "C" fn(*const OpenTTDEngine) -> bool,
    pub rail_kind: unsafe extern "C" fn(*const OpenTTDEngine) -> u8,
    pub clear_variant: unsafe extern "C" fn(*const OpenTTDEngine),
    pub refresh_type: unsafe extern "C" fn(u8),
    pub refresh_lists: unsafe extern "C" fn(bool),
    pub company_next: unsafe extern "C" fn(u32) -> Company,
    pub company_preview: unsafe extern "C" fn(*const c_void) -> CompanyPreview,
    pub vehicle_next: unsafe extern "C" fn(u32) -> Vehicle,
    pub vehicle_cargo: unsafe extern "C" fn(*mut OpenTTDVehicle) -> VehicleCargo,
    pub can_carry: unsafe extern "C" fn(*const OpenTTDEngine) -> bool,
    pub refit_mask: unsafe extern "C" fn(*const OpenTTDEngine) -> u64,
    pub update_railtypes: unsafe extern "C" fn(u8),
    pub update_roadtypes: unsafe extern "C" fn(u8),
    pub date_company_types: unsafe extern "C" fn(u8, i32),
    pub company_valid: unsafe extern "C" fn(u8) -> bool,
    pub compatible_railtypes: unsafe extern "C" fn(*const OpenTTDEngine) -> u64,
    pub powered_roadtypes: unsafe extern "C" fn(*const OpenTTDEngine) -> u64,
    pub company_railtypes: unsafe extern "C" fn(u8) -> u64,
    pub company_roadtypes: unsafe extern "C" fn(u8) -> u64,
    pub company_engine_count: unsafe extern "C" fn(u8, u16) -> u16,
    pub block_preview: unsafe extern "C" fn(u8),
    pub introduces_railtypes: unsafe extern "C" fn(*const OpenTTDEngine) -> u64,
    pub introduces_roadtypes: unsafe extern "C" fn(*const OpenTTDEngine) -> u64,
    pub set_railtypes: unsafe extern "C" fn(u8, u64, i32),
    pub set_roadtypes: unsafe extern "C" fn(u8, u64, i32),
    pub current_company: unsafe extern "C" fn() -> u8,
    pub local_company: unsafe extern "C" fn() -> u8,
    pub toolbar_enable: unsafe extern "C" fn(u8),
    pub toolbar_release: unsafe extern "C" fn(u8),
    pub close_preview: unsafe extern "C" fn(u16),
    pub invalidate_preview: unsafe extern "C" fn(u16),
    pub offer_preview: unsafe extern "C" fn(u16, u8, bool),
    pub available_event: unsafe extern "C" fn(u16),
    pub available_news: unsafe extern "C" fn(u16),
    pub no_engines: unsafe extern "C" fn(i32),
    pub screen_dirty: unsafe extern "C" fn(),
    pub utf8_length: unsafe extern "C" fn(ByteSpan) -> u32,
}

const AVAILABLE: u8 = 1;
const EXCLUSIVE: u8 = 2;
const INVALID: u16 = u16::MAX;
// Original static year is process-wide; all engine entries run serialized by
// the game-state mutex, including main-thread GUI commands and game timers.
static mut AGING_STOPS: i32 = 0;
macro_rules! read {
    ($e:expr, $field:ident) => {
        unsafe { addr_of!((*$e.state).$field).read() }
    };
}
macro_rules! write {
    ($e:expr, $field:ident, $value:expr) => {{
        let value = $value;
        unsafe { addr_of_mut!((*$e.state).$field).write(value) }
    }};
}
unsafe fn parent(s: &Services, mut e: EngineRef) -> EngineRef {
    loop {
        let v = unsafe { (s.sync_variant)(e.shell) };
        if v.parent == INVALID || !v.sync {
            return e;
        }
        e = unsafe { (s.resolve)(v.parent) };
    }
}
unsafe fn clear_variant(s: &Services, e: EngineRef) {
    let id = unsafe { (s.engine_id)(e.shell) };
    let mut from = 0;
    loop {
        let other = unsafe { (s.next)(from) };
        if other.shell.is_null() {
            break;
        }
        from = u32::from(other.id) + 1;
        if other.kind == e.kind && unsafe { (s.display_variant)(other.shell) }.display == id {
            unsafe {
                (s.clear_variant)(other.shell);
            }
        }
    }
}
unsafe fn reliability(s: &Services, e: EngineRef, month: bool) {
    let re = unsafe { parent(s, e) };
    let mut age = read!(re, age) as u32;
    if month
        && unsafe { (s.engine_id)(re.shell) > (s.engine_id)(e.shell) }
        && age != i32::MAX as u32
    {
        age = age.wrapping_add(1);
    }
    let cfg = unsafe { (s.reliability)(e.shell) };
    let phase1 = u32::from(read!(e, duration_phase_1));
    let phase2 = u32::from(read!(e, duration_phase_2));
    let phase3 = u32::from(read!(e, duration_phase_3));
    if read!(e, company_avail) != 0 && !unsafe { (s.never_expire)() } && cfg.base_life != 255 {
        let early = i32::from(cfg.retire_early);
        let max_age = ((phase1 + phase2) as i32 - early * 12).max(0) as u32;
        if early != 0 && age >= max_age {
            write!(e, company_avail, 0);
            unsafe {
                clear_variant(s, e);
                (s.refresh_type)(e.kind);
            }
        }
    }
    let value;
    if age < phase1 {
        let start = u32::from(read!(e, reliability_start));
        value = age.wrapping_mul(u32::from(read!(e, reliability_max)).wrapping_sub(start)) / phase1
            + start;
    } else {
        age = age.wrapping_sub(phase1);
        if age < phase2 || unsafe { (s.never_expire)() } || cfg.base_life == 255 {
            value = u32::from(read!(e, reliability_max));
        } else {
            age = age.wrapping_sub(phase2);
            if age < phase3 {
                let max = u32::from(read!(e, reliability_max));
                let delta = u32::from(read!(e, reliability_final)).wrapping_sub(max) as i32;
                value = ((age as i32).wrapping_mul(delta) / phase3 as i32) as u32;
                write!(e, reliability, value.wrapping_add(max) as u16);
                return;
            }
            write!(e, company_avail, 0);
            write!(e, reliability, read!(e, reliability_final));
            unsafe {
                clear_variant(s, e);
                (s.refresh_type)(e.kind);
            }
            return;
        }
    }
    write!(e, reliability, value as u16);
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_reliability(
    s: *const Services,
    e: EngineRef,
    month: bool,
) {
    // SAFETY: Entry borrows its immutable native table; state accesses are raw,
    // field-sized and end before every possibly reentrant callback.
    unsafe {
        reliability(&*s, e, month);
    }
}
unsafe fn start_one(s: &Services, e: EngineRef, aging: Ymd, seed: u32) {
    let cfg = unsafe { (s.startup)(e.shell) };
    let settings = unsafe { (s.start_settings)() };
    write!(e, age, 0);
    write!(e, flags, 0);
    write!(e, company_avail, 0);
    write!(e, company_hidden, 0);
    let rng = unsafe { &*s.rng };
    let shared = unsafe { &*s.shared };
    let saved = unsafe { (rng.save)() };
    unsafe {
        (rng.set_seed)(
            settings.generation_seed ^ seed ^ cfg.base_intro as u32 ^ u32::from(e.kind) ^ cfg.grfid,
        );
    }
    let r = (shared.random)(shared.context);
    let cutoff = unsafe { (s.ymd_to_date)(settings.starting_year.wrapping_add(2), 0, 1) };
    let intro = if cfg.base_intro <= cutoff {
        cfg.base_intro
    } else {
        cfg.base_intro.wrapping_add((r & 511) as i32)
    };
    write!(e, intro_date, intro);
    if intro <= settings.date {
        let ymd = unsafe { (s.date_to_ymd)(intro) };
        let aging_months = aging
            .year
            .wrapping_mul(12)
            .wrapping_add(i32::from(aging.month));
        let intro_months = ymd
            .year
            .wrapping_mul(12)
            .wrapping_add(i32::from(ymd.month))
            .wrapping_add(i32::from(ymd.day > 1));
        write!(e, age, aging_months.wrapping_sub(intro_months));
        write!(e, company_avail, u16::MAX);
        write!(e, flags, read!(e, flags) | AVAILABLE);
    }
    let re = unsafe { parent(s, e) };
    let re_cfg = unsafe { (s.seed_config)(re.shell) };
    unsafe {
        (rng.set_seed)(
            settings.generation_seed
                ^ seed
                ^ (u32::from((s.engine_id)(re.shell)) << 16)
                ^ (re_cfg.base_intro as u32).wrapping_shl(12)
                ^ (u32::from(re_cfg.decay_speed) << 8)
                ^ (re_cfg.lifelength as u32).wrapping_shl(4)
                ^ (i32::from(re_cfg.retire_early) as u32)
                ^ u32::from(e.kind)
                ^ cfg.grfid,
        );
    }
    let r = (shared.random)(shared.context);
    write!(e, reliability_start, ((r >> 16) & 16383) as u16 + 0x7ae0);
    write!(e, reliability_max, (r & 16383) as u16 + 0xbfff);
    let r = (shared.random)(shared.context);
    write!(e, reliability_final, ((r >> 16) & 16383) as u16 + 0x3fff);
    write!(e, duration_phase_1, (r & 31) as u16 + 7);
    write!(
        e,
        duration_phase_2,
        (((r >> 5) & 15) as i32 + i32::from(cfg.base_life) * 12 - 96).max(0) as u16
    );
    write!(e, duration_phase_3, ((r >> 9) & 127) as u16 + 120);
    unsafe {
        (rng.restore)(saved);
    }
    write!(e, reliability_spd_dec, u16::from(cfg.decay_speed) << 2);
    if !cfg.climate {
        write!(e, flags, read!(e, flags) | AVAILABLE);
        write!(e, company_avail, 0);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_start_one(
    s: *const Services,
    e: EngineRef,
    aging: Ymd,
    seed: u32,
) {
    unsafe {
        start_one(&*s, e, aging, seed);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_aging_stops(s: *const Services) {
    let s = unsafe { &*s };
    let mut year = 2050;
    let mut from = 0;
    loop {
        let e = unsafe { (s.next)(from) };
        if e.shell.is_null() {
            break;
        }
        from = u32::from(e.id) + 1;
        if !unsafe { (s.climate)(e.shell) } || unsafe { wagon(s, e) } {
            continue;
        }
        let cfg = unsafe { (s.aging_year)(e.shell) };
        let date = cfg
            .base_intro
            .wrapping_add(cfg.lifelength.wrapping_mul(366) / 2);
        year = year.max(unsafe { (s.date_to_ymd)(date) }.year);
    }
    unsafe {
        AGING_STOPS = year;
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_start_all(s: *const Services) {
    let s = unsafe { &*s };
    let date =
        unsafe { ((*s.shared).calendar_date)() }.min(unsafe { (s.ymd_to_date)(AGING_STOPS, 0, 1) });
    let aging = unsafe { (s.date_to_ymd)(date) };
    let shared = unsafe { &*s.shared };
    let seed = (shared.random)(shared.context);
    let mut from = 0;
    loop {
        let e = unsafe { (s.next)(from) };
        if e.shell.is_null() {
            break;
        }
        from = u32::from(e.id) + 1;
        unsafe {
            start_one(s, e, aging, seed);
        }
    }
    from = 0;
    loop {
        let e = unsafe { (s.next)(from) };
        if e.shell.is_null() {
            break;
        }
        from = u32::from(e.id) + 1;
        unsafe {
            reliability(s, e, false);
        }
    }
    from = 0;
    loop {
        let c = unsafe { (s.company_next)(from) };
        if c.id == 255 {
            break;
        }
        from = u32::from(c.id) + 1;
        unsafe {
            (s.update_railtypes)(c.id);
            (s.update_roadtypes)(c.id);
        }
    }
    unsafe {
        (s.refresh_lists)(true);
    }
}
unsafe fn wagon(s: &Services, e: EngineRef) -> bool {
    e.kind == 0 && unsafe { (s.rail_kind)(e.shell) } == 2
}
unsafe fn disabled(s: &Services, kind: u8, ai: bool) -> bool {
    match kind {
        0 => unsafe { (s.train_limit)() == 0 || (ai && (s.ai_train_disabled)()) },
        1 => unsafe { (s.road_limit)() == 0 || (ai && (s.ai_road_disabled)()) },
        2 => unsafe { (s.ship_limit)() == 0 || (ai && (s.ai_ship_disabled)()) },
        3 => unsafe { (s.aircraft_limit)() == 0 || (ai && (s.ai_aircraft_disabled)()) },
        _ => unreachable!(),
    }
}
unsafe fn enable(s: &Services, e: EngineRef, company: u8) {
    write!(
        e,
        company_avail,
        read!(e, company_avail) | (1u16 << company)
    );
    unsafe {
        if e.kind == 0 {
            (s.update_railtypes)(company);
        } else if e.kind == 1 {
            (s.update_roadtypes)(company);
        }
    }
    if company == unsafe { (s.local_company)() } {
        unsafe {
            (s.refresh_type)(e.kind);
            (s.toolbar_enable)(e.kind);
        }
    }
}
unsafe fn disable(s: &Services, e: EngineRef, company: u8) {
    write!(
        e,
        company_avail,
        read!(e, company_avail) & !(1u16 << company)
    );
    unsafe {
        if e.kind == 0 {
            (s.update_railtypes)(company);
        } else if e.kind == 1 {
            (s.update_roadtypes)(company);
        }
    }
    if company == unsafe { (s.local_company)() } {
        unsafe {
            clear_variant(s, e);
            (s.refresh_type)(e.kind);
        }
    }
}
unsafe fn accept(s: &Services, e: EngineRef, company: u8, depth: i32) {
    write!(e, preview_company, 255);
    write!(e, preview_asked, u16::MAX);
    unsafe {
        enable(s, e, company);
        (s.invalidate_preview)(e.id);
    }
    if depth >= 10 {
        return;
    }
    let mut from = 0;
    loop {
        let ve = unsafe { (s.next)(from) };
        if ve.shell.is_null() {
            break;
        }
        from = u32::from(ve.id) + 1;
        if ve.kind != e.kind {
            continue;
        }
        let v = unsafe { (s.preview_variant)(ve.shell) };
        if ve.id != e.id && v.parent == e.id && v.join {
            unsafe {
                accept(s, ve, company, depth + 1);
            }
        }
    }
}
unsafe fn preview_company(s: &Services, e: EngineRef) -> u8 {
    let mask = if e.kind == 0 {
        u64::MAX
    } else {
        unsafe { (s.refit_mask)(e.shell) }
    };
    let mut best = 255;
    let mut performance = -1;
    let mut from = 0;
    loop {
        let c = unsafe { (s.company_next)(from) };
        if c.id == 255 {
            break;
        }
        from = u32::from(c.id) + 1;
        let preview = unsafe { (s.company_preview)(c.shell) };
        if preview.blocked != 0
            || read!(e, preview_asked) & (1 << c.id) != 0
            || preview.performance <= performance
        {
            continue;
        }
        let mut veh_from = 0;
        loop {
            let v = unsafe { (s.vehicle_next)(veh_from) };
            if v.shell.is_null() {
                break;
            }
            veh_from = v.id + 1;
            if v.owner != c.id || v.kind != e.kind {
                continue;
            }
            let cargo = unsafe { (s.vehicle_cargo)(v.shell) };
            if !unsafe { (s.can_carry)(cargo.engine) }
                || mask & 1u64.wrapping_shl(u32::from(cargo.cargo)) == 0
            {
                continue;
            }
            performance = preview.performance;
            best = c.id;
            break;
        }
    }
    best
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_daily(s: *const Services) {
    let s = unsafe { &*s };
    let date = unsafe { ((*s.shared).calendar_date)() };
    let mut from = 0;
    loop {
        let c = unsafe { (s.company_next)(from) };
        if c.id == 255 {
            break;
        }
        from = u32::from(c.id) + 1;
        unsafe {
            (s.date_company_types)(c.id, date);
        }
    }
    if unsafe { (s.calendar_year)() } >= unsafe { AGING_STOPS } {
        return;
    }
    from = 0;
    loop {
        let e = unsafe { (s.next)(from) };
        if e.shell.is_null() {
            break;
        }
        from = u32::from(e.id) + 1;
        if read!(e, flags) & EXCLUSIVE == 0 {
            continue;
        }
        if read!(e, preview_company) != 255 {
            let wait = read!(e, preview_wait).wrapping_sub(1);
            write!(e, preview_wait, wait);
            if wait == 0 {
                unsafe {
                    (s.close_preview)(e.id);
                }
                write!(e, preview_company, 255);
            }
        } else if read!(e, preview_asked).count_ones() < 15 {
            let company = unsafe { preview_company(s, e) };
            write!(e, preview_company, company);
            if company == 255 {
                write!(e, preview_asked, u16::MAX);
                continue;
            }
            write!(e, preview_asked, read!(e, preview_asked) | (1 << company));
            write!(e, preview_wait, 20);
            let ai = !unsafe { disabled(s, e.kind, true) };
            unsafe {
                (s.offer_preview)(e.id, company, ai);
            }
        }
    }
}
unsafe fn available(s: &Services, e: EngineRef, date: i32) {
    if read!(e, flags) & EXCLUSIVE != 0 {
        let mut from = 0;
        loop {
            let c = unsafe { (s.company_next)(from) };
            if c.id == 255 {
                break;
            }
            from = u32::from(c.id) + 1;
            if read!(e, company_avail) & (1 << c.id) == 0 {
                continue;
            }
            if unsafe { (s.company_engine_count)(c.id, e.id) } == 0 {
                unsafe {
                    (s.block_preview)(c.id);
                }
            }
        }
    }
    write!(e, flags, (read!(e, flags) & !EXCLUSIVE) | AVAILABLE);
    unsafe {
        (s.refresh_type)(e.kind);
    }
    write!(e, company_avail, u16::MAX);
    if unsafe { wagon(s, e) } {
        return;
    }
    if e.kind == 0 {
        let introduced = unsafe { (s.introduces_railtypes)(e.shell) };
        let mut from = 0;
        loop {
            let c = unsafe { (s.company_next)(from) };
            if c.id == 255 {
                break;
            }
            from = u32::from(c.id) + 1;
            let types = unsafe { (s.company_railtypes)(c.id) } | introduced;
            unsafe {
                (s.set_railtypes)(c.id, types, date);
            }
        }
    } else if e.kind == 1 {
        let introduced = unsafe { (s.introduces_roadtypes)(e.shell) };
        let mut from = 0;
        loop {
            let c = unsafe { (s.company_next)(from) };
            if c.id == 255 {
                break;
            }
            from = u32::from(c.id) + 1;
            let types = unsafe { (s.company_roadtypes)(c.id) } | introduced;
            unsafe {
                (s.set_roadtypes)(c.id, types, date);
            }
        }
    }
    if !unsafe { disabled(s, e.kind, true) } {
        unsafe {
            (s.available_event)(e.id);
        }
    }
    if !unsafe { disabled(s, e.kind, false) } && unsafe { (s.extra_flags)(e.shell) } & 1 == 0 {
        unsafe {
            (s.available_news)(e.id);
        }
    }
    unsafe {
        (s.toolbar_release)(e.kind);
        (s.close_preview)(e.id);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_monthly(s: *const Services) {
    let s = unsafe { &*s };
    if unsafe { (s.calendar_year)() } >= unsafe { AGING_STOPS } {
        return;
    }
    let date = unsafe { ((*s.shared).calendar_date)() };
    let mut refresh = false;
    let mut from = 0;
    loop {
        let e = unsafe { (s.next)(from) };
        if e.shell.is_null() {
            break;
        }
        from = u32::from(e.id) + 1;
        if read!(e, flags) & AVAILABLE != 0 && read!(e, age) != i32::MAX {
            write!(e, age, read!(e, age).wrapping_add(1));
            unsafe {
                reliability(s, e, true);
            }
            refresh = true;
        }
        if !unsafe { (s.enabled)(e.shell) } {
            continue;
        }
        if read!(e, flags) & AVAILABLE == 0 && date >= read!(e, intro_date).wrapping_add(365) {
            unsafe {
                available(s, e, date);
            }
        } else if read!(e, flags) & (AVAILABLE | EXCLUSIVE) == 0 && date >= read!(e, intro_date) {
            if unsafe { disabled(s, e.kind, false) }
                || unsafe { wagon(s, e) }
                || unsafe { (s.extra_flags)(e.shell) } & 2 != 0
            {
                continue;
            }
            write!(e, flags, read!(e, flags) | EXCLUSIVE);
            write!(e, preview_company, 255);
            write!(e, preview_asked, 0);
        }
    }
    unsafe {
        (s.refresh_lists)(refresh);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_clear_hidden(s: *const Services, company: u8) {
    let s = unsafe { &*s };
    let mut from = 0;
    loop {
        let e = unsafe { (s.next)(from) };
        if e.shell.is_null() {
            break;
        }
        from = u32::from(e.id) + 1;
        write!(
            e,
            company_hidden,
            read!(e, company_hidden) & !(1 << company)
        );
    }
}
unsafe fn buildable(s: &Services, e: EngineRef, kind: u8, company: u8) -> bool {
    if e.shell.is_null() || e.kind != kind {
        return false;
    }
    if company == 18 {
        if read!(e, flags) & AVAILABLE == 0 || read!(e, company_avail) == 0 {
            return false;
        }
    } else if read!(e, company_avail) & 1u16.wrapping_shl(u32::from(company)) == 0 {
        return false;
    }
    if !unsafe { (s.enabled)(e.shell) } {
        return false;
    }
    if company != 18 {
        if kind == 0
            && unsafe { (s.compatible_railtypes)(e.shell) & (s.company_railtypes)(company) } == 0
        {
            return false;
        }
        if kind == 1
            && unsafe { (s.powered_roadtypes)(e.shell) & (s.company_roadtypes)(company) } == 0
        {
            return false;
        }
    }
    true
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_buildable(
    s: *const Services,
    e: EngineRef,
    kind: u8,
    company: u8,
) -> bool {
    unsafe { buildable(&*s, e, kind, company) }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_variant_hidden(
    s: *const Services,
    e: EngineRef,
    company: u8,
) -> bool {
    if company >= 15 {
        return false;
    }
    if read!(e, company_hidden) & (1 << company) != 0 {
        return true;
    }
    let s = unsafe { &*s };
    let mut re = e;
    loop {
        let variant = unsafe { (s.display_variant)(re.shell) };
        let ve = if variant.display == re.id || variant.display == INVALID {
            re
        } else {
            unsafe { (s.resolve)(variant.display) }
        };
        let hidden = read!(ve, company_hidden) & (1 << company) != 0;
        if hidden || variant.parent == INVALID {
            return hidden;
        }
        re = unsafe { (s.resolve)(variant.parent) };
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_visibility(
    s: *const Services,
    e: EngineRef,
    hide: bool,
    execute: bool,
) -> u8 {
    let s = unsafe { &*s };
    let company = unsafe { (s.current_company)() };
    if e.shell.is_null() || company >= 15 || !unsafe { buildable(s, e, e.kind, company) } {
        return 1;
    }
    if execute {
        let mask = 1 << company;
        write!(
            e,
            company_hidden,
            if hide {
                read!(e, company_hidden) | mask
            } else {
                read!(e, company_hidden) & !mask
            }
        );
        unsafe {
            (s.refresh_type)(e.kind);
        }
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_want_preview(
    s: *const Services,
    e: EngineRef,
    execute: bool,
) -> u8 {
    let s = unsafe { &*s };
    let company = unsafe { (s.current_company)() };
    if e.shell.is_null() || read!(e, flags) & EXCLUSIVE == 0 || read!(e, preview_company) != company
    {
        return 1;
    }
    if execute {
        unsafe {
            accept(s, e, company, 0);
        }
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_control(
    s: *const Services,
    e: EngineRef,
    company: u8,
    allow: bool,
    execute: bool,
) -> u8 {
    let s = unsafe { &*s };
    if unsafe { (s.current_company)() } != 18
        || e.shell.is_null()
        || !unsafe { (s.company_valid)(company) }
    {
        return 1;
    }
    if execute {
        if allow {
            unsafe {
                enable(s, e, company);
            }
        } else {
            unsafe {
                disable(s, e, company);
            }
        }
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_rename(
    s: *const Services,
    e: EngineRef,
    name: ByteSpan,
    execute: bool,
) -> u8 {
    if e.shell.is_null() {
        return 1;
    }
    let s = unsafe { &*s };
    if name.size != 0 {
        if unsafe {
            (s.utf8_length)(ByteSpan {
                data: name.data,
                size: name.size,
            })
        } >= 32
        {
            return 1;
        }
        let bytes = unsafe { std::slice::from_raw_parts(name.data, name.size) };
        let mut from = 0;
        loop {
            let other = unsafe { (s.next)(from) };
            if other.shell.is_null() {
                break;
            }
            from = u32::from(other.id) + 1;
            let old = unsafe { openttd_rust_engine_name(other.state) };
            if old.size != 0 && unsafe { std::slice::from_raw_parts(old.data, old.size) } == bytes {
                return 2;
            }
        }
    }
    if execute {
        unsafe {
            openttd_rust_engine_set_name(e.state, name);
            (s.screen_dirty)();
        }
    }
    0
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_engine_check(s: *const Services) {
    let s = unsafe { &*s };
    let mut min = i32::MAX;
    let mut from = 0;
    loop {
        let e = unsafe { (s.next)(from) };
        if e.shell.is_null() {
            break;
        }
        from = u32::from(e.id) + 1;
        if !unsafe { (s.enabled)(e.shell) } || unsafe { wagon(s, e) } {
            continue;
        }
        if read!(e, flags) & AVAILABLE != 0 && read!(e, company_avail) != 0 {
            return;
        }
        min = min.min(unsafe { (s.base_intro)(e.shell) });
    }
    unsafe {
        (s.no_engines)(min);
    }
}
