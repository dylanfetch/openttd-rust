/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file landscape-loop-reference.cpp Unchanged scheduler versus Rust for unreachable map phases. */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "rust/landscape_loop_ffi.h"

static uint8_t log_x, log_y;
static uint64_t observed_hash, observed_count;
static bool rust_run, reenter, nested;
static uint32_t starting_cursor;
struct TileIndex {
	uint32_t value = 0;
	TileIndex() = default;
	explicit TileIndex(uint32_t value) : value(value) {}
	uint32_t base() const { return this->value; }
	bool operator!=(uint32_t other) const { return this->value != other; }
};
struct Map {
	static uint8_t LogX() { return log_x; }
	static uint8_t LogY() { return log_y; }
};
struct TimerGameTick { static inline uint64_t counter; };
struct PerformanceAccumulator { explicit PerformanceAccumulator(int) {} };
static constexpr int PFE_GL_LANDSCAPE = 0;
static constexpr uint8_t MIN_MAP_SIZE_BITS = 6, MAX_MAP_SIZE_BITS = 12;
static constexpr uint32_t TILE_UPDATE_FREQUENCY_LOG = 8, TILE_UPDATE_FREQUENCY = 256;
template<typename T, size_t N> constexpr size_t lengthof(T (&)[N]) { return N; }
using uint = unsigned int;

static void NativeVisit(uint32_t tile) noexcept;
static void ReferenceVisit(TileIndex tile) { NativeVisit(tile.base()); }
static uint8_t Type(uint32_t tile) noexcept { return static_cast<uint8_t>((tile % 4)); }
static uint8_t GetTileType(uint32_t tile) { return Type(tile); }
static uint8_t GetTileType(TileIndex tile) { return Type(tile.base()); }
struct TileTypeProcs { void (*tile_loop_proc)(TileIndex); };
static const TileTypeProcs procs{ReferenceVisit};
static const TileTypeProcs * const _tile_type_procs[16] = {&procs, &procs, &procs, &procs};
static TileIndex _cur_tileloop_tile;
#include <cassert>
#include "landscape-loop-reference.inc"

static uint32_t Random(void *) noexcept { std::abort(); }
static uint32_t Industry(int32_t, int32_t, uint32_t *) noexcept { std::abort(); }
static OpenTTDSharedServices shared = OpenTTDFixtureSharedServices(nullptr, Random, Industry);
static const OpenTTDLandscapeLoopServices services{
	.shared = &shared,
	.trees = nullptr,
	.industry = {},
	.clear_loop = NativeVisit,
	.rail_loop = NativeVisit,
	.road_loop = NativeVisit,
	.house_loop = NativeVisit,
	.station_loop = NativeVisit,
	.water_loop = NativeVisit,
	.void_loop = NativeVisit,
	.tunnel_bridge_loop = NativeVisit,
	.object_loop = NativeVisit,
};
static void RustRun()
{
	const OpenTTDTreeLoopSettings settings{
		.tick_counter = TimerGameTick::counter,
		.size_x = 1U << log_x,
		.climate = 0,
		.extra = 0,
		.ambient = false,
	};
	openttd_rust_run_tile_loop(log_x, log_y, settings, &services);
}
static void NativeVisit(uint32_t tile) noexcept
{
	observed_hash = (observed_hash ^ tile) * UINT64_C(1099511628211);
	observed_count++;
	if (reenter && !nested) {
		nested = true;
		/* Both entries must retain the pre-batch canonical cursor until return. */
		uint32_t cursor = rust_run ? *openttd_rust_tile_loop_cursor() : _cur_tileloop_tile.base();
		if (cursor != starting_cursor) std::abort();
		if (rust_run) RustRun(); else RunTileLoop();
	}
}
int main()
{
	shared.tile_type = Type;
	unsigned cases = 0;
	for (log_x = 6; log_x <= 12; log_x++) {
		for (log_y = 6; log_y <= 12; log_y++) {
			for (uint64_t tick : {UINT64_C(0), UINT64_C(1), UINT64_C(255), UINT64_C(256), UINT64_MAX}) {
				for (uint32_t start : {1U, (1U << (log_x + log_y)) - 1, 0x5A5U}) {
					for (bool enter : {false, true}) {
						TimerGameTick::counter = tick;
						reenter = enter;
						starting_cursor = start;
						_cur_tileloop_tile = TileIndex{start};
						observed_count = observed_hash = 0;
						nested = rust_run = false;
						RunTileLoop();
						const uint64_t expected_hash = observed_hash, expected_count = observed_count;
						const uint32_t expected_cursor = _cur_tileloop_tile.base();
						*openttd_rust_tile_loop_cursor() = start;
						observed_count = observed_hash = 0;
						nested = false;
						rust_run = true;
						RustRun();
						if (observed_hash != expected_hash || observed_count != expected_count || *openttd_rust_tile_loop_cursor() != expected_cursor) std::abort();
						cases++;
					}
				}
			}
		}
	}
	std::printf("landscape scheduler: %u unchanged-body map/phase/cursor/reentry cases passed\n", cases);
}
