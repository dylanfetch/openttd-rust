/*
	* This file is part of OpenTTD.
	* OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
	* OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	* See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
	*/

/** @file train_reservation_ffi.h Train controller reservation owner. */
#ifndef OPENTTD_RUST_TRAIN_RESERVATION_FFI_H
#define OPENTTD_RUST_TRAIN_RESERVATION_FFI_H
#include <stdint.h>
extern "C" {
struct OpenTTDTrainState;
struct OpenTTDTrainReservationTask;
struct OpenTTDTrainReservationView {
	uint32_t tile, dest, next;
	uint16_t destination, last_station;
	uint8_t direction, order, num_orders, order_index, suppress, nearest;
};
struct OpenTTDTrainReservationFollow {
	uint32_t old_tile, new_tile;
	int32_t skipped;
	uint16_t dirs;
	uint8_t old_td, exitdir, tunnel, bridge, station, error;
};
struct OpenTTDTrainReservationPbs {
	uint32_t tile, other;
	uint8_t td, okay;
};
struct OpenTTDTrainReservationStep {
	uint64_t value;
	uint32_t action, id, tile, final_dest;
	uint8_t td, dir, tracks, reserve, found, got, okay;
};
struct OpenTTDTrainReservationLeaves {
	void (*observe)(uint32_t, OpenTTDTrainReservationView *) noexcept;
	uint64_t (*leaf)(void *, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t) noexcept;
	OpenTTDTrainState *(*owner)(uint32_t) noexcept;
	uint8_t (*follow)(uint32_t, uint64_t, OpenTTDTrainReservationFollow *) noexcept;
	OpenTTDTrainReservationPbs (*origin)(uint32_t, uint8_t) noexcept;
};
/* All callbacks operate on the serialized game thread. Rust copies private state
	* through scalar accessors and retains no borrowed train/world state. A task
	* yields only before the named potentially reentrant order/YAPF services.
	* C++ preserves the generic Order snapshot; Rust owns its restoration and index.
	* Destroy a task exactly once, before its C++ context expires, also on exceptions.
	* Panics/allocation failure abort; environmental exceptions terminate in leaves. */
OpenTTDTrainReservationTask *openttd_rust_train_reservation_new(uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, const OpenTTDTrainReservationLeaves *, void *);
OpenTTDTrainReservationStep openttd_rust_train_reservation_step(OpenTTDTrainReservationTask *, OpenTTDTrainReservationStep);
void openttd_rust_train_reservation_destroy(OpenTTDTrainReservationTask *);
}
constexpr uint32_t TR_BACKOFF = 0;
constexpr uint32_t TR_RESERVE_PATHS = 1;
constexpr uint32_t TR_FORBID90 = 2;
constexpr uint32_t TR_LINE_REVERSE = 3;
constexpr uint32_t TR_SHOW_RES = 4;
constexpr uint32_t TR_IS_STATION = 5;
constexpr uint32_t TR_IS_WAYPOINT = 6;
constexpr uint32_t TR_IS_RAILWAY = 7;
constexpr uint32_t TR_IS_TUNNEL = 8;
constexpr uint32_t TR_IS_DEPOT = 9;
constexpr uint32_t TR_IS_PLAIN = 10;
constexpr uint32_t TR_STATION = 11;
constexpr uint32_t TR_DEPOT_DIR = 12;
constexpr uint32_t TR_TUNNEL_DIR = 13;
constexpr uint32_t TR_OTHER_END = 14;
constexpr uint32_t TR_IS_BRIDGE = 15;
constexpr uint32_t TR_TUNNEL_FREE = 16;
constexpr uint32_t TR_SET_TUNNEL = 17;
constexpr uint32_t TR_MARK_BRIDGE = 18;
constexpr uint32_t TR_MARK_TILE = 19;
constexpr uint32_t TR_COMPAT_STATION = 20;
constexpr uint32_t TR_SET_PLATFORM = 21;
constexpr uint32_t TR_UNRESERVE = 22;
constexpr uint32_t TR_RESERVED = 23;
constexpr uint32_t TR_OVERLAP = 24;
constexpr uint32_t TR_HAS_RESERVED = 25;
constexpr uint32_t TR_HAS_SIGNAL = 26;
constexpr uint32_t TR_HAS_PBS = 27;
constexpr uint32_t TR_IS_PBS = 28;
constexpr uint32_t TR_GREEN = 29;
constexpr uint32_t TR_SET_SIGNAL = 30;
constexpr uint32_t TR_ONEWAY = 31;
constexpr uint32_t TR_BLOCKING = 32;
constexpr uint32_t TR_SIGNAL_BUFFER = 33;
constexpr uint32_t TR_UPDATE_BUFFER = 34;
constexpr uint32_t TR_RAIL90 = 35;
constexpr uint32_t TR_EXIT_DIR = 36;
constexpr uint32_t TR_REACH_TRACKS = 37;
constexpr uint32_t TR_REACH_DIRS = 38;
constexpr uint32_t TR_CROSS_TRACKS = 39;
constexpr uint32_t TR_CROSS_DIRS = 40;
constexpr uint32_t TR_ENTER_TD = 41;
constexpr uint32_t TR_TILE_ADD = 42;
constexpr uint32_t TR_TILE_OFFSET = 43;
constexpr uint32_t TR_SAFE = 44;
constexpr uint32_t TR_FREE = 45;
constexpr uint32_t TR_TRY_TRACK = 46;
constexpr uint32_t TR_DEPOT_RESERVED = 47;
constexpr uint32_t TR_SET_DEPOT = 48;
constexpr uint32_t TR_TRACK_STATUS = 49;
constexpr uint32_t TR_DIAG_REACH_DIRS = 50;
constexpr uint32_t TR_TRACKDIR = 51;
constexpr uint32_t TR_ALL_COMPAT = 52;
constexpr uint32_t TR_ORDER_STOP = 53;
constexpr uint32_t TR_STUCK = 54;
constexpr uint32_t TR_START_STOP = 55;
constexpr uint32_t TR_PATH_RESULT = 56;
constexpr uint32_t TR_SAVE_ORDER = 57;
constexpr uint32_t TR_RESTORE_ORDER = 58;
constexpr uint32_t TR_WRITE_DEST = 59;
constexpr uint32_t TR_WRITE_LAST = 60;
constexpr uint32_t TR_WRITE_SUPPRESS = 61;
constexpr uint32_t TR_ORDER_TYPE = 62;
constexpr uint32_t TR_ORDER_SERVICE = 63;
constexpr uint32_t TR_NEEDS_SERVICE = 64;
constexpr uint32_t TR_COPY_ORDER = 65;
constexpr uint32_t TR_SET_DEPOT_DEST = 66;
constexpr uint32_t TR_STATION_TRAIN = 67;
constexpr uint32_t TR_STATION_XY = 68;
constexpr uint32_t TR_INCREMENT_ORDER = 69;
constexpr uint32_t TR_DIAG_TRACK = 70;
constexpr uint32_t TR_BITS_TRACK = 71;
constexpr uint32_t TR_STATION_RAIL = 73;
constexpr uint32_t TR_CHECK_REVERSE = 74;
constexpr uint32_t TR_CONDITIONAL = 75;
constexpr uint32_t TR_SERVICE = 76;
#endif
