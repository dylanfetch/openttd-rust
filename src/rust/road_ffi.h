/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file road_ffi.h Canonical road vehicle controller and private state. */
#ifndef RUST_ROAD_FFI_H
#define RUST_ROAD_FFI_H
#include <cstdint>
#include <cstddef>
#include "services_ffi.h"
#if defined(_MSC_VER)
#define OPENTTD_ROAD_CALL __cdecl
#elif defined(__i386__)
#define OPENTTD_ROAD_CALL __attribute__((cdecl))
#else
#define OPENTTD_ROAD_CALL
#endif
/** Immediate shared services and named arbitrary-code/owner-reentry actions. */
enum RoadOperation : uint32_t {
	ROAD_OP_ACC_MODEL = 1,
	ROAD_OP_ROAD_SIDE = 2,
	ROAD_OP_TILE_TYPE = 3,
	ROAD_OP_HAS_ROAD = 4,
	ROAD_OP_TRACK_STATUS = 5,
	ROAD_OP_TILE_OWNER = 6,
	ROAD_OP_DEPOT_DIR = 7,
	ROAD_OP_BAY_DIR = 8,
	ROAD_OP_IS_DEPOT = 9,
	ROAD_OP_NORMAL_ROAD = 10,
	ROAD_OP_ROAD_WORKS = 11,
	ROAD_OP_DISALLOWED = 12,
	ROAD_OP_BAY_STOP = 13,
	ROAD_OP_IS_DT_STOP = 14,
	ROAD_OP_STOP_TYPE = 15,
	ROAD_OP_FREE_BAY = 16,
	ROAD_OP_ANY_ROAD_BITS = 17,
	ROAD_OP_ROAD_BITS = 18,
	ROAD_OP_OFFSET = 20,
	ROAD_OP_TILE_X = 21,
	ROAD_OP_TILE_Y = 22,
	ROAD_OP_STATION = 23,
	ROAD_OP_CONTINUATION = 24,
	ROAD_OP_BRIDGE_SPEED = 25,
	ROAD_OP_MAX_PENALTY = 26,
	ROAD_OP_SERVINT = 27,
	ROAD_OP_NEEDS_SERVICE = 28,
	ROAD_OP_WAIT_UNBUNCH = 30,
	ROAD_OP_ORDER_STOP = 31,
	ROAD_OP_ROAD_TYPE = 32,
	ROAD_OP_QUEUE = 33,
	ROAD_OP_TUNNEL_DIR = 34,
	ROAD_OP_ACCELERATION = 35,
	ROAD_OP_UPDATE_SPEED = 36,
	ROAD_OP_ADVANCE = 37,
	ROAD_OP_POSITION = 39,
	ROAD_OP_BASE_VIEWPORT = 41,
	ROAD_OP_LAST_SPEED = 42,
	ROAD_OP_ROADSTOP_LEAVE = 43,
	ROAD_OP_ENTRANCE_SET = 44,
	ROAD_OP_ENTRANCE_BUSY = 45,
	ROAD_OP_ORDER_FREE = 46,
	ROAD_OP_SET_NEXT = 47,
	ROAD_OP_START_STOP_DIRTY = 48,
	ROAD_OP_DEPOT_DIRTY = 49,
	ROAD_OP_DETAILS_DIRTY = 50,
	ROAD_OP_SERVICE = 51,
	ROAD_OP_LEAVE_UNBUNCH = 52,
	ROAD_OP_RESET_UNBUNCH = 53,
	ROAD_OP_PATH_RESULT = 54,
	ROAD_OP_ORDER_DUMMY = 55,
	ROAD_OP_ORDER_DEPOT = 56,
	ROAD_OP_DEPOT_INDEX = 57,
	ROAD_OP_DECREASE_VALUE = 58,
	ROAD_OP_AGE = 59,
	ROAD_OP_ECONOMY_AGE = 60,
	ROAD_OP_CHECK_BREAKDOWN = 61,
	ROAD_OP_CHECK_ORDERS = 62,
	ROAD_OP_PAY_RUNNING = 63,
	ROAD_OP_COST_CLASS = 64,
	ROAD_OP_COST_FACTOR = 65,
	ROAD_OP_GET_PRICE = 66,
	ROAD_OP_GRF_VERSION = 67,
	ROAD_OP_LENGTH_DEFAULT = 68,
	ROAD_OP_AGE_DEFAULT = 69,
	ROAD_OP_SPEED_DEFAULT = 70,
	ROAD_OP_LENGTH_ERROR = 71,
	ROAD_OP_DISCONNECT = 72,
	ROAD_OP_EXPLOSION = 73,
	ROAD_OP_SOUND_DEFAULT = 74,
	ROAD_OP_SOUND = 75,
	ROAD_OP_SOUND_OLD1 = 76,
	ROAD_OP_SOUND_OLD2 = 77,
	ROAD_OP_ENGINE_INVALID = 79,
	ROAD_OP_INVALID_PRICE = 80,
	ROAD_OP_COST_DIVISOR = 81,
	ROAD_OP_IS_CROSSING = 82,
	ROAD_OP_NEW_POSITION = 83,
	ROAD_OP_VIRT_TILE = 84,
	ROAD_OP_IS_ROAD_STOP = 85,
	ROAD_OP_SET_DEST = 86,
	ROAD_OP_ENTER_TILE = 128,
	ROAD_OP_ENTER_DEPOT = 129,
	ROAD_OP_PROCESS_ORDERS = 130,
	ROAD_OP_LOADING = 131,
	ROAD_OP_BEGIN_LOADING = 132,
	ROAD_OP_TRAM_PROBE = 133,
	ROAD_OP_PROPERTY = 134,
	ROAD_OP_LENGTH_CALLBACK = 135,
	ROAD_OP_PLAY_SOUND = 136,
	ROAD_OP_VISUAL = 137,
	ROAD_OP_UPDATE_VISUAL = 138,
	ROAD_OP_CARGO_CHANGED = 139,
	ROAD_OP_CACHE_INVALIDATE = 140,
	ROAD_OP_LENGTH_CHANGED = 141,
	ROAD_OP_BREAKDOWN = 142,
	ROAD_OP_DELETE = 143,
	ROAD_OP_GROUND_CRASH = 144,
	ROAD_OP_ARRIVAL = 145,
	ROAD_OP_STOP_RANDOM = 146,
	ROAD_OP_STOP_ANIMATION = 147,
	ROAD_OP_CRASH_NEWS = 148,
	ROAD_OP_YAPF = 149,
	ROAD_OP_FIND_DEPOT = 150,
	ROAD_OP_INCLINATION = 151,
	ROAD_OP_VIEWPORT = 152,
};
enum RoadSharedField : uint32_t {
	ROAD_WRITE_TILE = 0,
	ROAD_WRITE_X = 1,
	ROAD_WRITE_Y = 2,
	ROAD_WRITE_DIRECTION = 3,
	ROAD_WRITE_SPEED = 4,
	ROAD_WRITE_TICK = 5,
	ROAD_WRITE_RUNNING = 6,
	ROAD_WRITE_DAY = 7,
	ROAD_WRITE_ORDER_TIME = 8,
	ROAD_WRITE_PROGRESS = 9,
	ROAD_WRITE_LAST_STATION = 10,
	ROAD_WRITE_HIDDEN = 11,
	ROAD_WRITE_FIRST_ENGINE = 12,
	ROAD_WRITE_LENGTH = 13,
	ROAD_WRITE_TOTAL_LENGTH = 14,
	ROAD_WRITE_CARGO_AGE = 15,
	ROAD_WRITE_MAX_SPEED = 16,
	ROAD_WRITE_SUPPRESS_IMPLICIT = 17,
};
struct OpenTTDRoadState;
struct OpenTTDRoadPathElement {
	uint8_t trackdir;
	uint32_t tile;
};
struct OpenTTDRoadView {
	uint32_t type;
	uint32_t first;
	uint32_t next;
	uint32_t previous;
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
	uint32_t breakdown;
	uint32_t max_track_speed;
	uint32_t length;
	uint32_t total_length;
	uint32_t roadtype;
	uint32_t front;
	uint32_t articulated;
	uint32_t tram;
	uint32_t bus;
	uint32_t order_nonstop;
};
struct OpenTTDRoadAction { uint32_t op, id; uint64_t a, b, c; };
struct OpenTTDRoadLeaves {
	void (OPENTTD_ROAD_CALL *observe)(uint32_t, OpenTTDRoadView *) noexcept;
	void (OPENTTD_ROAD_CALL *write)(uint32_t, uint32_t, uint64_t) noexcept;
	uint64_t (OPENTTD_ROAD_CALL *leaf)(uint32_t, uint32_t, uint64_t, uint64_t, uint64_t) noexcept;
	OpenTTDRoadState *(OPENTTD_ROAD_CALL *owner)(uint32_t) noexcept;
	size_t (OPENTTD_ROAD_CALL *nearby)(uint32_t, uint32_t, int32_t, int32_t, uint32_t *, size_t) noexcept;
};
extern "C" {
/* Entry kinds:0 tick,1 individual(previous ID or UINT32_MAX),2 leave depot(first),
 * 3 crash(flooded),4 cache(same length),5 calendar day,6 economy day,7 running cost,
 * 8 max speed,9 speed update,10 destination,11 turn(execute),12 trackdir,13 slope.
 * Tables are copied before use. View/IDs are values, never borrowed world storage.
 * observe/write/leaf/owner/nearby are synchronous noexcept nonreentrant services;
 * nearby copies only pool IDs in original traversal order. No Rust owner borrow
 * spans a service. create owns only invocation control, separate from shell state.
 * advance returns op0/result when complete; other ops dispatch after Rust returns:
 * tile/depot entry; ProcessOrders/HandleLoading/BeginLoading; CMD_BUILD_ROAD probe;
 * NewGRF property/length/sound/visual/stop callbacks; cache length warnings;
 * HandleBreakdown; destruction; generic GroundVehicle crash; YAPF calls that
 * invoke RoadVehicle trackdir; and inclination/viewport calls that resolve
 * NewGRF sprites or reenter road slope policy. No RNG continuation exists.
 * Errors unwind solely through C++ RAII; task_destroy does not access the world. */
void *OPENTTD_ROAD_CALL openttd_rust_road_create(uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, const OpenTTDRoadLeaves *, const OpenTTDSharedServices *);
OpenTTDRoadAction OPENTTD_ROAD_CALL openttd_rust_road_advance(void *, uint64_t);
void OPENTTD_ROAD_CALL openttd_rust_road_task_destroy(void *);

/* Each ordinary or indexed-load shell owns one zero-created state. Destruction
 * follows PreDestructor and occurs exactly once, also during pool cleanup.
 * Scalars 0..6: state:u8, frame:u8, blocked:u16, overtaking:u8,
 * overtaking-counter:u8, crashed:u16, reverse:u8. Set narrows to source widths.
 * No returned references or persistent C++ cache. Save/load stages fields and
 * path outside Rust frames, commits partial loads on unwind and preserves nesting.
 * Path access requires the same index/nonempty preconditions as std::vector.
 * Replacing copies in forward order; count zero permits a null input.
 * Game-thread access only; panic/OOM abort and no exception crosses this ABI. */
OpenTTDRoadState *OPENTTD_ROAD_CALL openttd_rust_road_new();
void OPENTTD_ROAD_CALL openttd_rust_road_destroy(OpenTTDRoadState *);
uint16_t OPENTTD_ROAD_CALL openttd_rust_road_get(const OpenTTDRoadState *, uint8_t);
void OPENTTD_ROAD_CALL openttd_rust_road_set(OpenTTDRoadState *, uint8_t, uint16_t);
uint8_t OPENTTD_ROAD_CALL openttd_rust_road_stop_frame(uint32_t);
uint16_t OPENTTD_ROAD_CALL openttd_rust_road_drive_entry(uint8_t, uint8_t, uint8_t);
size_t OPENTTD_ROAD_CALL openttd_rust_road_path_size(const OpenTTDRoadState *);
OpenTTDRoadPathElement OPENTTD_ROAD_CALL openttd_rust_road_path_get(const OpenTTDRoadState *, size_t);
void OPENTTD_ROAD_CALL openttd_rust_road_path_replace(OpenTTDRoadState *, const OpenTTDRoadPathElement *, size_t);
void OPENTTD_ROAD_CALL openttd_rust_road_path_clear(OpenTTDRoadState *);
void OPENTTD_ROAD_CALL openttd_rust_road_path_push(OpenTTDRoadState *, OpenTTDRoadPathElement);
void OPENTTD_ROAD_CALL openttd_rust_road_path_pop(OpenTTDRoadState *);
}
#endif /* RUST_ROAD_FFI_H */
