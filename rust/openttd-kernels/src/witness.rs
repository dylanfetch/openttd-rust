/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! One branch-witness facility for every port: named relaxed counters behind a
//! single enable flag. A hit is a Rust atomic load (and add when enabled); it
//! never crosses into C++. `src/rust/witness.cpp` enables it from
//! `OPENTTD_WITNESS` and writes `$HOME/branch-witnesses.json` at exit.
#![allow(unsafe_code)]
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};

/// First train controller/reservation counter; indices follow the train order.
pub const TRAIN: usize = 0;
/// First ship controller counter.
pub const SHIP: usize = 31;
/// First ship YAPF counter; 0..13 are the search statistics.
pub const SHIP_YAPF: usize = 47;
/// First rail YAPF counter.
pub const RAIL: usize = 64;
/// First water-region map-query counter.
pub const WATER: usize = 87;

/// Counter names, grouped by the prefix the harness selects.
pub const NAMES: [&str; 91] = [
    "train.tick_first",
    "train.tick_second",
    "train.reversal",
    "train.wormhole_swap",
    "train.unequal_before",
    "train.unequal_after",
    "train.depot_start",
    "train.depot_service",
    "train.red_oneway",
    "train.red_twoway",
    "train.force_signal",
    "train.stuck_retry",
    "train.stuck_reverse",
    "train.cross_bar",
    "train.cross_unbar",
    "train.collision",
    "train.crash_delete",
    "train.free_wagon_delete",
    "train.articulated_move",
    "train.first_tile_rail",
    "train.wormhole_exit",
    "train.depot_reentry",
    "train.controller_extension",
    "train.extension_fail",
    "train.extension_rollback",
    "train.extension_opposing_red",
    "train.extension_restore",
    "train.free_path",
    "train.choose_track",
    "train.order_lookahead",
    "train.order_restore",
    "ship.lock_up",
    "ship.lock_down",
    "ship.reverse",
    "ship.rotate",
    "ship.depot_leave",
    "ship.auto_service",
    "ship.buoy",
    "ship.loading",
    "ship.water_change",
    "ship.aqueduct",
    "ship.rotation_reload",
    "ship.depot_search",
    "ship.build",
    "ship.economy_day",
    "ship.path_cache",
    "ship.depot_enter",
    "ship_yapf.region_nodes",
    "ship_yapf.track_nodes",
    "ship_yapf.retries",
    "ship_yapf.track_limits",
    "ship_yapf.intermediate",
    "ship_yapf.cache_truncations",
    "ship_yapf.final_region_clears",
    "ship_yapf.lost",
    "ship_yapf.random_draws",
    "ship_yapf.reverse_origins",
    "ship_yapf.blocked_fallbacks",
    "ship_yapf.region_limits",
    "ship_yapf.alternate_docking",
    "ship_yapf.choose_calls",
    "ship_yapf.reverse_calls",
    "ship_yapf.blocked_calls",
    "ship_yapf.reverse_chosen",
    "rail.choose",
    "rail.reverse",
    "rail.depot",
    "rail.safe",
    "rail.cached",
    "rail.uncached",
    "rail.partial_paths",
    "rail.bounded_depot",
    "rail.found",
    "rail.allow90",
    "rail.forbid90",
    "rail.cache_flushes",
    "rail.reserve_attempts",
    "rail.target_busy",
    "rail.reserve_success",
    "rail.rollback",
    "rail.invalidations",
    "rail.reverse_chosen",
    "rail.depot_found",
    "rail.safe_found",
    "rail.limit_stops",
    "rail.signals_red",
    "rail.signals_restored",
    "water.tracks",
    "water.follows",
    "water.aqueducts",
    "water.rebuilds",
];

static ENABLED: AtomicBool = AtomicBool::new(false);
static COUNTS: [AtomicU64; NAMES.len()] = [const { AtomicU64::new(0) }; NAMES.len()];

/// Whether counters are on; lets a site skip work that only feeds counters.
#[inline]
pub fn enabled() -> bool {
    ENABLED.load(Ordering::Relaxed)
}

/// Add `amount` to counter `index` when witnesses are enabled.
#[inline]
pub fn add(index: usize, amount: u64) {
    if ENABLED.load(Ordering::Relaxed) {
        COUNTS[index].fetch_add(amount, Ordering::Relaxed);
    }
}

/// Count one hit of branch `index`.
#[inline]
pub fn hit(index: usize) {
    add(index, 1);
}

/// Turn every counter on; called once at static initialization.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_witness_enable() {
    ENABLED.store(true, Ordering::Relaxed);
}

/// Name of counter `index` (UTF-8, not NUL-terminated, static lifetime) and its
/// length; null past the last counter.
///
/// # Safety
/// `length` is a writable `usize` for this call only.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_witness_name(index: usize, length: *mut usize) -> *const u8 {
    NAMES.get(index).map_or(std::ptr::null(), |name| {
        // SAFETY: Caller supplies a writable output for this call.
        unsafe { length.write(name.len()) };
        name.as_ptr()
    })
}

/// Current count of counter `index`, which must be below the name count.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_witness_count(index: usize) -> u64 {
    COUNTS[index].load(Ordering::Relaxed)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn groups_start_at_their_prefix() {
        for (base, prefix) in [
            (TRAIN, "train."),
            (SHIP, "ship."),
            (SHIP_YAPF, "ship_yapf."),
            (RAIL, "rail."),
            (WATER, "water."),
        ] {
            assert!(NAMES[base].starts_with(prefix));
            assert!(base == 0 || !NAMES[base - 1].starts_with(prefix));
        }
        let mut length = 0;
        assert!(unsafe { openttd_rust_witness_name(NAMES.len(), &raw mut length) }.is_null());
    }
}
