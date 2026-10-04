/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file train-edge-comparison.cpp Bounded NewGRF-only copied-world curve/reversal gap. */
#include "stdafx.h"
#include "core/math_func.hpp"
#include "direction_func.h"
#include "rust/train_ffi.h"
#include <cstdio>
#include <vector>

/* The original selected bodies are included unchanged. Shared world operations
 * only record their call order; no map, train controller, or NewGRF is emulated. */
using TileIndex = uint32_t;
constexpr uint VEHICLE_LENGTH = 8;
constexpr uint8_t TRACK_BIT_DEPOT = 0x80, TRACK_BIT_WORMHOLE = 0x40;
constexpr int GVF_GOINGUP_BIT = 0, GVF_GOINGDOWN_BIT = 1, MP_TUNNELBRIDGE = 9;
constexpr int AM_ORIGINAL = 0;
struct { struct { int train_acceleration_model; } vehicle; } _settings_game;
struct RailTypeInfo { uint8_t curve_speed; } rail;
static uint8_t GetRailType(TileIndex) { return 0; }
static const RailTypeInfo *GetRailTypeInfo(uint8_t) { return &rail; }
enum class VehState { Hidden = 0 };
struct Status {
	uint8_t bits;
	bool Test(VehState) const { return (this->bits & 1) != 0; }
	void Set(VehState, bool value) { this->bits = (this->bits & ~1) | uint8_t(value); }
};
static std::vector<uint64_t> events;
struct Train {
	uint32_t id;
	Train *next;
	uint8_t track;
	Direction direction;
	int x_pos, y_pos, z_pos;
	TileIndex tile;
	uint16_t gv_flags;
	Status vehstatus;
	struct { uint16_t cached_veh_length; } gcache;
	struct { bool cached_tilt; int16_t cached_curve_speed_mod; } tcache;
	OpenTTDTrainState *state;
	const Train *First() const;
	Train *Next() const { return this->next; }
	uint16_t GetCurveSpeedLimit() const;
	void UpdatePosition() { events.push_back(0x100 + this->id); }
	void UpdateViewport(bool a, bool b) { assert(a && b); events.push_back(0x200 + this->id); }
	void UpdateInclination(bool, bool) { std::abort(); }
};
static Train trains[16];
const Train *Train::First() const { return trains; }
static void VehicleEnterTile(Train *v, TileIndex tile, int x, int y)
{
	assert(tile == v->tile && x == v->x_pos && y == v->y_pos);
	events.push_back(0x300 + v->id);
}
static TileIndex TileVirtXY(int x, int y) { return uint32_t((y >> 4) * 256 + (x >> 4)); }
static bool IsTileType(TileIndex, int) { return false; }
static bool IsBridgeTile(TileIndex) { return false; }
#include "train-reference.inc"

static void Observe(uint32_t id, OpenTTDTrainView *out) noexcept
{
	const Train &v = trains[id];
	*out = {};
	out->id = id; out->first = 0; out->next = v.next == nullptr ? UINT32_MAX : v.next->id;
	out->direction = v.direction; out->length = v.gcache.cached_veh_length;
	out->tile = v.tile; out->x = v.x_pos; out->y = v.y_pos; out->z = v.z_pos;
	out->gv_flags = v.gv_flags; out->status = v.vehstatus.bits;
	out->articulated = uint8_t(id % 2); // Topology can contain articulated parts.
}
static void Write(uint32_t id, uint32_t field, uint64_t value) noexcept
{
	Train &v = trains[id];
	switch (field) {
		case TRAIN_WRITE_TILE: v.tile = uint32_t(value); break;
		case TRAIN_WRITE_X: v.x_pos = int32_t(value); break;
		case TRAIN_WRITE_Y: v.y_pos = int32_t(value); break;
		case TRAIN_WRITE_Z: v.z_pos = int32_t(value); break;
		case TRAIN_WRITE_DIRECTION: v.direction = Direction(value); break;
		case TRAIN_WRITE_GV_FLAGS: v.gv_flags = uint16_t(value); break;
		case TRAIN_WRITE_STATUS: v.vehstatus.bits = uint8_t(value); break;
		default: std::abort();
	}
}
static uint64_t Leaf(uint32_t op, uint32_t id, uint64_t a, uint64_t b, uint64_t) noexcept
{
	switch (op) {
		case TRAIN_OP_ACC_MODEL: return _settings_game.vehicle.train_acceleration_model;
		case TRAIN_OP_CURVE_ADVANTAGE: return rail.curve_speed;
		case TRAIN_OP_POSITION: trains[id].UpdatePosition(); return 0;
		case TRAIN_OP_VIEWPORT: trains[id].UpdateViewport(a != 0, b != 0); return 0;
		case TRAIN_OP_TILE_VIRT: return TileVirtXY(int32_t(a), int32_t(b));
		case TRAIN_OP_IS_TUNNELBRIDGE: return 0;
		case TRAIN_OP_PROFILE: return 0;
		default: std::abort();
	}
}
static OpenTTDTrainState *Owner(uint32_t id) noexcept { return trains[id].state; }
static size_t Nearby(uint32_t, uint32_t, int32_t, int32_t, uint32_t *, size_t) noexcept { std::abort(); }
static uint32_t UnexpectedRandom(void *) noexcept { std::abort(); }
static void UnexpectedObserve(void *, uint32_t, uint32_t *) noexcept { std::abort(); }
static void UnexpectedWrite(void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) noexcept { std::abort(); }
static float UnexpectedTrig(uint32_t, float) noexcept { std::abort(); }
static uint32_t UnexpectedIndustry(int32_t, int32_t, uint32_t *) noexcept { std::abort(); }
static uint64_t Rust(uint32_t kind, uint64_t a = 0, uint64_t b = 0)
{
	const OpenTTDTrainServices leaves{Observe, Write, Leaf, Owner, Nearby};
	/* No selected body uses shared RNG/map services. Unexpected use aborts. */
	const OpenTTDSharedServices shared{nullptr, UnexpectedRandom, UnexpectedObserve, UnexpectedWrite, UnexpectedTrig, UnexpectedIndustry};
	void *task = openttd_rust_train_create(kind, 0, a, b, 0, &leaves, &shared);
	uint64_t reply = 0;
	for (;;) {
		auto action = openttd_rust_train_advance(task, reply);
		if (action.op == UINT32_MAX) { openttd_rust_train_destroy(task); return action.a; }
		assert(action.op == TRAIN_OP_ENTER_TILE);
		VehicleEnterTile(&trains[action.id], uint32_t(action.a), int32_t(action.b), int32_t(action.c));
		reply = 0;
	}
}
static uint32_t random_state = 130;
static uint32_t Draw() { random_state = random_state * 1664525U + 1013904223U; return random_state; }
static void Sync()
{
	for (Train &v : trains) {
		openttd_rust_train_state_set(v.state, 5, v.track);
		openttd_rust_train_state_set(v.state, 7, v.tcache.cached_tilt);
		openttd_rust_train_state_set(v.state, 9, uint16_t(v.tcache.cached_curve_speed_mod));
	}
}
static std::vector<uint64_t> State()
{
	auto result = events;
	for (Train &v : trains) {
		result.insert(result.end(), {uint64_t(v.track), uint64_t(v.direction), uint64_t(uint32_t(v.x_pos)), uint64_t(uint32_t(v.y_pos)), uint64_t(uint32_t(v.z_pos)), v.tile, v.gv_flags, v.vehstatus.bits});
	}
	return result;
}
int main()
{
	for (uint32_t i = 0; i < 16; ++i) { trains[i].id = i; trains[i].state = openttd_rust_train_state_new(); }
	for (uint32_t trial = 0; trial < 20000; ++trial) {
		uint32_t count = 1 + Draw() % 16;
		_settings_game.vehicle.train_acceleration_model = trial % 2;
		rail.curve_speed = uint8_t(Draw());
		for (uint32_t i = 0; i < 16; ++i) {
			Train &v = trains[i];
			v.next = i + 1 < count ? &trains[i + 1] : nullptr;
			v.direction = Direction(Draw() % 8); v.gcache.cached_veh_length = 1 + Draw() % 8;
			v.tcache.cached_tilt = (Draw() & 1) != 0; v.tcache.cached_curve_speed_mod = int16_t(Draw());
			v.track = uint8_t(1 << (Draw() % 8)); v.gv_flags = uint16_t(Draw()); v.vehstatus.bits = uint8_t(Draw());
			v.x_pos = int(Draw() % 4000); v.y_pos = int(Draw() % 4000); v.z_pos = int(Draw() % 256); v.tile = Draw() % 65536;
		}
		Sync();
		if (trains[0].GetCurveSpeedLimit() != Rust(1)) { std::fprintf(stderr, "curve mismatch %u\n", trial); return 1; }
		Train original[16]; std::copy(std::begin(trains), std::end(trains), std::begin(original));
		int l = int(Draw() % count), r = int(Draw() % count);
		events.clear(); ReverseTrainSwapVeh(trains, l, r); auto expected = State();
		std::copy(std::begin(original), std::end(original), std::begin(trains)); Sync(); events.clear(); Rust(13, l, r);
		for (Train &v : trains) v.track = uint8_t(openttd_rust_train_state_get(v.state, 5));
		if (expected != State()) { std::fprintf(stderr, "reversal mismatch %u\n", trial); return 1; }
	}
	for (Train &v : trains) openttd_rust_train_state_destroy(v.state);
	std::printf("20000 variable-length curve and 20000 reversal cases equal through production Rust entries\n");
}
