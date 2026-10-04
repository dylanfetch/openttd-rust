/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ship_control_ffi.h Canonical ship controller and private state. */
#ifndef OPENTTD_SHIP_CONTROL_FFI_H
#define OPENTTD_SHIP_CONTROL_FFI_H
#include "services_ffi.h"
#include "water_regions_ffi.h"
#include <cstdint>
#include <cstddef>
struct OpenTTDShipState;
enum ShipOperation : uint32_t {
	SHIP_OP_DEPOT_DIR = 1,
	SHIP_OP_DEPOT_AXIS = 2,
	SHIP_OP_IS_DEPOT = 3,
	SHIP_OP_DEPOT_INDEX = 4,
	SHIP_OP_WAIT_UNBUNCH = 5,
	SHIP_OP_CHAIN_DEPOT = 6,
	SHIP_OP_SERVINT = 7,
	SHIP_OP_NEEDS_SERVICE = 8,
	SHIP_OP_MAX_DISTANCE = 9,
	SHIP_OP_TILE_VALID = 10,
	SHIP_OP_TILE_TYPE = 11,
	SHIP_OP_WATER_CLASS = 12,
	SHIP_OP_LOCK_MIDDLE = 13,
	SHIP_OP_LOCK_DIR = 14,
	SHIP_OP_TILE_MIN_Z = 15,
	SHIP_OP_TILE_MAX_Z = 16,
	SHIP_OP_TRACK_STATUS = 17,
	SHIP_OP_OFFSET = 18,
	SHIP_OP_DIAG_BETWEEN = 19,
	SHIP_OP_DIST_SQUARE = 20,
	SHIP_OP_DIST_MANHATTAN = 21,
	SHIP_OP_DOCKING = 22,
	SHIP_OP_DOCK = 23,
	SHIP_OP_DOCK_WATER = 24,
	SHIP_OP_STATION = 25,
	SHIP_OP_INDUSTRY_STATION = 26,
	SHIP_OP_OILRIG = 27,
	SHIP_OP_STATION_USE = 28,
	SHIP_OP_STATION_XY = 29,
	SHIP_OP_STATION_CONTAINS = 30,
	SHIP_OP_STATION_DOCK = 31,
	SHIP_OP_STATION_VISITS = 32,
	SHIP_OP_VISIT_SET = 33,
	SHIP_OP_ARRIVAL = 34,
	SHIP_OP_SERVICE = 35,
	SHIP_OP_LEAVE_UNBUNCH = 36,
	SHIP_OP_PATH_RESULT = 37,
	SHIP_OP_ORDER_FREE = 38,
	SHIP_OP_ORDER_DUMMY = 39,
	SHIP_OP_ORDER_DEPOT = 40,
	SHIP_OP_ORDER_LEAVE = 41,
	SHIP_OP_ORDER_INCREMENT = 42,
	SHIP_OP_TIMETABLE = 43,
	SHIP_OP_POSITION = 44,
	SHIP_OP_START_DIRTY = 45,
	SHIP_OP_DEPOT_DIRTY = 46,
	SHIP_OP_DEPOT_INVALIDATE = 47,
	SHIP_OP_SHIPS_DIRTY = 48,
	SHIP_OP_DETAILS_DIRTY = 49,
	SHIP_OP_AGE = 50,
	SHIP_OP_ECONOMY_AGE = 51,
	SHIP_OP_DECREASE_VALUE = 52,
	SHIP_OP_CHECK_BREAKDOWN = 53,
	SHIP_OP_CHECK_ORDERS = 54,
	SHIP_OP_RUNNING_COST = 55,
	SHIP_OP_COST_DIVISOR = 56,
	SHIP_OP_PAY_RUNNING = 57,
	SHIP_OP_SPEED_DEFAULT = 58,
	SHIP_OP_AGE_DEFAULT = 59,
	SHIP_OP_SPEED_FRAC = 60,
	SHIP_OP_PROPERTY = 61,
	SHIP_OP_UPDATE_VISUAL = 62,
	SHIP_OP_CACHE_INVALIDATE = 63,
	SHIP_OP_CAPACITY = 64,
	SHIP_OP_BUILD_SHARED = 65,
	SHIP_OP_SPRITE_DIRECTION = 66,
	SHIP_OP_ADVANCE = 67,
	SHIP_OP_NEW_POSITION = 68,
	SHIP_OP_VIRT_TILE = 69,
	SHIP_OP_EXIT_DIR = 70,
	SHIP_OP_TRACK_DIRECTION = 71,
	SHIP_OP_TRACKS_REACH = 72,
	SHIP_OP_BUSY_TILE = 73,
	SHIP_OP_PATH_SIZE = 74,
	SHIP_OP_PATH_BACK = 75,
	SHIP_OP_PATH_POP = 76,
	SHIP_OP_PATH_CLEAR = 77,
	SHIP_OP_ENTER_TILE = 128,
	SHIP_OP_ENTER_DEPOT = 129,
	SHIP_OP_PROCESS_ORDERS = 130,
	SHIP_OP_LOADING = 131,
	SHIP_OP_BEGIN_LOADING = 132,
	SHIP_OP_BREAKDOWN = 133,
	SHIP_OP_VIEWPORT = 134,
	SHIP_OP_BASE_VIEWPORT = 135,
	SHIP_OP_VISUAL = 136,
	SHIP_OP_CACHE = 137,
	SHIP_OP_PLAY_SOUND = 138,
	SHIP_OP_YAPF_REVERSE = 139,
	SHIP_OP_YAPF_CHOOSE = 140,
	SHIP_OP_UPDATE_DELTA = 141,
	SHIP_OP_TILE_X = 90,
	SHIP_OP_TILE_Y = 91,
	SHIP_OP_BUILD_FLAG = 92,
	SHIP_OP_BUILD_RANDOM = 93,
};
enum ShipField : uint32_t {
	SHIP_WRITE_TILE = 0,
	SHIP_WRITE_X = 1,
	SHIP_WRITE_Y = 2,
	SHIP_WRITE_Z = 3,
	SHIP_WRITE_DIRECTION = 4,
	SHIP_WRITE_SPEED = 5,
	SHIP_WRITE_TICK = 6,
	SHIP_WRITE_RUNNING = 7,
	SHIP_WRITE_DAY = 8,
	SHIP_WRITE_ORDER_TIME = 9,
	SHIP_WRITE_PROGRESS = 10,
	SHIP_WRITE_LAST_STATION = 11,
	SHIP_WRITE_HIDDEN = 12,
	SHIP_WRITE_MAX_SPEED = 13,
	SHIP_WRITE_CARGO_AGE = 14,
	SHIP_WRITE_DEST = 15,
};
struct OpenTTDShipView {
	uint32_t tile;
	uint32_t dest;
	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t direction;
	uint32_t speed;
	uint32_t tick;
	uint32_t running;
	uint32_t day;
	uint32_t order_time;
	uint32_t progress;
	uint32_t status;
	uint32_t owner;
	uint32_t engine;
	uint32_t last_station;
	uint32_t order_destination;
	uint32_t order_type;
	uint32_t order_max_speed;
	uint32_t acceleration;
	uint32_t max_speed;
};
struct OpenTTDShipAction { uint32_t op, id; uint64_t a, b, c; };
struct OpenTTDShipDepot { uint32_t id, tile, owner, ship; };
struct OpenTTDShipLeaves {
	void (*observe)(uint32_t, OpenTTDShipView *) noexcept;
	void (*write)(uint32_t, uint32_t, uint64_t) noexcept;
	uint64_t (*leaf)(uint32_t, uint32_t, uint64_t, uint64_t, uint64_t) noexcept;
	OpenTTDShipState *(*owner)(uint32_t) noexcept;
	OpenTTDWaterPatch (*patch)(uint32_t) noexcept;
	size_t (*neighbours)(OpenTTDWaterPatch, OpenTTDWaterPatch *) noexcept;
	size_t (*depots)(OpenTTDShipDepot *, size_t) noexcept;
};
extern "C" {
/* Game-thread shell ownership; new zeroes state/coordinates and sets INVALID_DIR.
 * Scalars 0..3 are state:u8, rotation:u8, rotation-x:i16, rotation-y:i16.
 * Accessors retain no borrow; setters narrow exactly like C++.
 * Modern/legacy staging commits partial loads on unwind outside Rust frames.
 * Panics/OOM abort. One destruction follows PreDestructor, including pool cleanup. */
void openttd_rust_ship_control_profile_enable();
uint64_t openttd_rust_ship_control_profile(uint8_t);
void openttd_rust_ship_control_reload_rotation();
OpenTTDShipState *openttd_rust_ship_state_new();
void openttd_rust_ship_state_destroy(OpenTTDShipState *);
uint16_t openttd_rust_ship_state_get(const OpenTTDShipState *, uint8_t);
void openttd_rust_ship_state_set(OpenTTDShipState *, uint8_t, uint16_t);
/* Tables are copied. Leaves are synchronous noexcept shared operations; only
 * actual owner reentry (tile/depot/order/loading/viewport/YAPF) returns an action.
 * No world or owner borrow spans any service; invocation destruction touches no
 * world. Depot/neighbor callbacks copy original traversal order only.
 * Neighbour output spans 320 entries (256 aqueducts + 4*16 border neighbours).
 * C++ exceptions unwind solely through its task RAII. */
void *openttd_rust_ship_control_create(uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, const OpenTTDShipLeaves *, const OpenTTDSharedServices *);
OpenTTDShipAction openttd_rust_ship_control_advance(void *, uint64_t);
void openttd_rust_ship_control_destroy(void *);
}
#endif
