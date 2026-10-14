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

#[cfg(test)]
/// First train controller/reservation counter; indices follow the train order.
pub const TRAIN: usize = 0;
/// First ship controller counter.
#[cfg(test)]
pub const SHIP: usize = 31;
/// First ship YAPF counter; 0..13 are the search statistics.
#[cfg(test)]
pub const SHIP_YAPF: usize = 47;
/// First rail YAPF counter.
#[cfg(test)]
pub const RAIL: usize = 64;
/// First water-region map-query counter.
#[cfg(test)]
pub const WATER: usize = 87;

/// `train.tick_first` witness.
pub const TRAIN_TICK_FIRST: usize = 0;
/// `train.tick_second` witness.
pub const TRAIN_TICK_SECOND: usize = 1;
/// `train.reversal` witness.
pub const TRAIN_REVERSAL: usize = 2;
/// `train.wormhole_swap` witness.
pub const TRAIN_WORMHOLE_SWAP: usize = 3;
/// `train.unequal_before` witness.
pub const TRAIN_UNEQUAL_BEFORE: usize = 4;
/// `train.unequal_after` witness.
pub const TRAIN_UNEQUAL_AFTER: usize = 5;
/// `train.depot_start` witness.
pub const TRAIN_DEPOT_START: usize = 6;
/// `train.depot_service` witness.
pub const TRAIN_DEPOT_SERVICE: usize = 7;
/// `train.red_oneway` witness.
pub const TRAIN_RED_ONEWAY: usize = 8;
/// `train.red_twoway` witness.
pub const TRAIN_RED_TWOWAY: usize = 9;
/// `train.force_signal` witness.
pub const TRAIN_FORCE_SIGNAL: usize = 10;
/// `train.stuck_retry` witness.
pub const TRAIN_STUCK_RETRY: usize = 11;
/// `train.stuck_reverse` witness.
pub const TRAIN_STUCK_REVERSE: usize = 12;
/// `train.cross_bar` witness.
pub const TRAIN_CROSS_BAR: usize = 13;
/// `train.cross_unbar` witness.
pub const TRAIN_CROSS_UNBAR: usize = 14;
/// `train.collision` witness.
pub const TRAIN_COLLISION: usize = 15;
/// `train.crash_delete` witness.
pub const TRAIN_CRASH_DELETE: usize = 16;
/// `train.free_wagon_delete` witness.
pub const TRAIN_FREE_WAGON_DELETE: usize = 17;
/// `train.articulated_move` witness.
pub const TRAIN_ARTICULATED_MOVE: usize = 18;
/// `train.first_tile_rail` witness.
pub const TRAIN_FIRST_TILE_RAIL: usize = 19;
/// `train.wormhole_exit` witness.
pub const TRAIN_WORMHOLE_EXIT: usize = 20;
/// `train.depot_reentry` witness.
pub const TRAIN_DEPOT_REENTRY: usize = 21;
/// `train.controller_extension` witness.
pub const TRAIN_CONTROLLER_EXTENSION: usize = 22;
/// `train.extension_fail` witness.
pub const TRAIN_EXTENSION_FAIL: usize = 23;
/// `train.extension_rollback` witness.
pub const TRAIN_EXTENSION_ROLLBACK: usize = 24;
/// `train.extension_opposing_red` witness.
pub const TRAIN_EXTENSION_OPPOSING_RED: usize = 25;
/// `train.extension_restore` witness.
pub const TRAIN_EXTENSION_RESTORE: usize = 26;
/// `train.free_path` witness.
pub const TRAIN_FREE_PATH: usize = 27;
/// `train.choose_track` witness.
pub const TRAIN_CHOOSE_TRACK: usize = 28;
/// `train.order_lookahead` witness.
pub const TRAIN_ORDER_LOOKAHEAD: usize = 29;
/// `train.order_restore` witness.
pub const TRAIN_ORDER_RESTORE: usize = 30;
/// `ship.lock_up` witness.
pub const SHIP_LOCK_UP: usize = 31;
/// `ship.lock_down` witness.
pub const SHIP_LOCK_DOWN: usize = 32;
/// `ship.reverse` witness.
pub const SHIP_REVERSE: usize = 33;
/// `ship.rotate` witness.
pub const SHIP_ROTATE: usize = 34;
/// `ship.depot_leave` witness.
pub const SHIP_DEPOT_LEAVE: usize = 35;
/// `ship.auto_service` witness.
pub const SHIP_AUTO_SERVICE: usize = 36;
/// `ship.buoy` witness.
pub const SHIP_BUOY: usize = 37;
/// `ship.loading` witness.
pub const SHIP_LOADING: usize = 38;
/// `ship.water_change` witness.
pub const SHIP_WATER_CHANGE: usize = 39;
/// `ship.aqueduct` witness.
pub const SHIP_AQUEDUCT: usize = 40;
/// `ship.rotation_reload` witness.
pub const SHIP_ROTATION_RELOAD: usize = 41;
/// `ship.depot_search` witness.
pub const SHIP_DEPOT_SEARCH: usize = 42;
/// `ship.build` witness.
pub const SHIP_BUILD: usize = 43;
/// `ship.economy_day` witness.
pub const SHIP_ECONOMY_DAY: usize = 44;
/// `ship.path_cache` witness.
pub const SHIP_PATH_CACHE: usize = 45;
/// `ship.depot_enter` witness.
pub const SHIP_DEPOT_ENTER: usize = 46;
/// `ship_yapf.region_nodes` witness.
pub const SHIP_YAPF_REGION_NODES: usize = 47;
/// `ship_yapf.track_nodes` witness.
pub const SHIP_YAPF_TRACK_NODES: usize = 48;
/// `ship_yapf.retries` witness.
pub const SHIP_YAPF_RETRIES: usize = 49;
/// `ship_yapf.track_limits` witness.
pub const SHIP_YAPF_TRACK_LIMITS: usize = 50;
/// `ship_yapf.intermediate` witness.
pub const SHIP_YAPF_INTERMEDIATE: usize = 51;
/// `ship_yapf.cache_truncations` witness.
pub const SHIP_YAPF_CACHE_TRUNCATIONS: usize = 52;
/// `ship_yapf.final_region_clears` witness.
pub const SHIP_YAPF_FINAL_REGION_CLEARS: usize = 53;
/// `ship_yapf.lost` witness.
pub const SHIP_YAPF_LOST: usize = 54;
/// `ship_yapf.random_draws` witness.
pub const SHIP_YAPF_RANDOM_DRAWS: usize = 55;
/// `ship_yapf.reverse_origins` witness.
pub const SHIP_YAPF_REVERSE_ORIGINS: usize = 56;
/// `ship_yapf.blocked_fallbacks` witness.
pub const SHIP_YAPF_BLOCKED_FALLBACKS: usize = 57;
/// `ship_yapf.region_limits` witness.
pub const SHIP_YAPF_REGION_LIMITS: usize = 58;
/// `ship_yapf.alternate_docking` witness.
pub const SHIP_YAPF_ALTERNATE_DOCKING: usize = 59;
/// `ship_yapf.choose_calls` witness.
pub const SHIP_YAPF_CHOOSE_CALLS: usize = 60;
/// `ship_yapf.reverse_calls` witness.
pub const SHIP_YAPF_REVERSE_CALLS: usize = 61;
/// `ship_yapf.blocked_calls` witness.
pub const SHIP_YAPF_BLOCKED_CALLS: usize = 62;
/// `ship_yapf.reverse_chosen` witness.
pub const SHIP_YAPF_REVERSE_CHOSEN: usize = 63;
/// `rail.choose` witness.
pub const RAIL_CHOOSE: usize = 64;
/// `rail.reverse` witness.
pub const RAIL_REVERSE: usize = 65;
/// `rail.depot` witness.
pub const RAIL_DEPOT: usize = 66;
/// `rail.safe` witness.
pub const RAIL_SAFE: usize = 67;
/// `rail.cached` witness.
pub const RAIL_CACHED: usize = 68;
/// `rail.uncached` witness.
pub const RAIL_UNCACHED: usize = 69;
/// `rail.partial_paths` witness.
pub const RAIL_PARTIAL_PATHS: usize = 70;
/// `rail.bounded_depot` witness.
pub const RAIL_BOUNDED_DEPOT: usize = 71;
/// `rail.found` witness.
pub const RAIL_FOUND: usize = 72;
/// `rail.allow90` witness.
pub const RAIL_ALLOW90: usize = 73;
/// `rail.forbid90` witness.
pub const RAIL_FORBID90: usize = 74;
/// `rail.cache_flushes` witness.
pub const RAIL_CACHE_FLUSHES: usize = 75;
/// `rail.reserve_attempts` witness.
pub const RAIL_RESERVE_ATTEMPTS: usize = 76;
/// `rail.target_busy` witness.
pub const RAIL_TARGET_BUSY: usize = 77;
/// `rail.reserve_success` witness.
pub const RAIL_RESERVE_SUCCESS: usize = 78;
/// `rail.rollback` witness.
pub const RAIL_ROLLBACK: usize = 79;
/// `rail.invalidations` witness.
pub const RAIL_INVALIDATIONS: usize = 80;
/// `rail.reverse_chosen` witness.
pub const RAIL_REVERSE_CHOSEN: usize = 81;
/// `rail.depot_found` witness.
pub const RAIL_DEPOT_FOUND: usize = 82;
/// `rail.safe_found` witness.
pub const RAIL_SAFE_FOUND: usize = 83;
/// `rail.limit_stops` witness.
pub const RAIL_LIMIT_STOPS: usize = 84;
/// `rail.signals_red` witness.
pub const RAIL_SIGNALS_RED: usize = 85;
/// `rail.signals_restored` witness.
pub const RAIL_SIGNALS_RESTORED: usize = 86;
/// `water.tracks` witness.
pub const WATER_TRACKS: usize = 87;
/// `water.follows` witness.
pub const WATER_FOLLOWS: usize = 88;
/// `water.aqueducts` witness.
pub const WATER_AQUEDUCTS: usize = 89;
/// `water.rebuilds` witness.
pub const WATER_REBUILDS: usize = 90;

/// Road tick entries.
pub const ROAD_TICKS: usize = 91;
/// Road calls into typed C++ services.
pub const ROAD_SERVICES: usize = 92;
/// Native traversal calls back into the road Rust visitor.
pub const ROAD_VISITORS: usize = 93;

/// Selected road controller entries, including reentrant helpers.
pub const ROAD_ENTRIES: usize = 94;

/// First landscape count, compiled into dispatch only for the diagnostic build.
#[cfg(landscape_loop_count)]
pub const LANDSCAPE: usize = 95;

/// Counter names, grouped by the prefix the harness selects.
pub const NAMES: [&str; 112] = [
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
    "road.ticks",
    "road.services",
    "road.visitors",
    "road.entries",
    "landscape.clear_visits",
    "landscape.rail_visits",
    "landscape.road_visits",
    "landscape.house_visits",
    "landscape.tree_visits",
    "landscape.station_visits",
    "landscape.water_visits",
    "landscape.void_visits",
    "landscape.industry_visits",
    "landscape.tunnel_bridge_visits",
    "landscape.object_visits",
    "landscape.batches",
    "landscape.tile_zero",
    "landscape.clear_observations",
    "landscape.water_observations",
    "landscape.clear_fast_returns",
    "landscape.water_fast_returns",
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
