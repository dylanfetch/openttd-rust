/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town-tunnel-reference.cpp Controlled services for unchanged town tunnel bodies, absent from the save corpus. */
#include "../../src/rust/town_ffi.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <initializer_list>
#include <memory>
#include <unordered_map>
#include <vector>

using TileIndex = uint32_t;
using TileIndexDiff = int32_t;
using DiagDirection = uint32_t;
using RoadBits = uint32_t;
using RoadType = uint32_t;
using Slope = uint32_t;
constexpr uint32_t DIAGDIR_END = 4, MP_ROAD = 2, MP_RAILWAY = 1, MP_STATION = 5, MP_TUNNELBRIDGE = 9;
constexpr uint32_t TRANSPORT_ROAD = 1, DRD_NONE = 0;
struct Town { uint32_t index = 17; struct { uint32_t population = 0; } cache; };
struct { struct { bool allow_town_level_crossings = true; } economy; } _settings_game;
enum class DoCommandFlag { Auto = 1, NoWater = 2, Execute = 4 };
struct Flags {
	uint32_t bits = 0;
	Flags() = default;
	Flags(std::initializer_list<DoCommandFlag> flags) { for (auto flag : flags) this->Set(flag); }
	Flags &Set(DoCommandFlag flag) { this->bits |= static_cast<uint32_t>(flag); return *this; }
};
enum { CMD_BUILD_ROAD, CMD_BUILD_TUNNEL };
template <int> constexpr uint32_t GetCommandFlags() { return 0; }
static Flags CommandFlagsToDCFlags(uint32_t) { return {}; }
struct Cost { bool ok; bool Succeeded() const { return this->ok; } };
struct Tile {
	uint32_t type = 0, slope = 0, z = 4, roads = 0, stop = 0, facing = 0, transport = TRANSPORT_ROAD;
	bool valid = true;
};
struct World {
	Town town;
	uint32_t start = 512 * 1024 + 512, direction = 0, length = 0, draws = 0, effects = 0;
	bool road_test = true, tunnel_test = true, tunnel_execute = true;
	std::unordered_map<uint32_t, Tile> tiles;
	std::vector<std::array<uint32_t, 7>> trace;
	OpenTTDTownState *owner = nullptr;
};
static World *world;
static Tile At(TileIndex tile)
{
	if (tile >= 1024 * 1024 || tile % 1024 == 1023 || tile / 1024 == 1023) return Tile{.valid = false};
	auto it = world->tiles.find(tile);
	return it == world->tiles.end() ? Tile{} : it->second;
}
static TileIndexDiff TileOffsByDiagDir(DiagDirection dir) { return std::array<int32_t, 4>{-1, 1024, 1, -1024}[dir]; }
static DiagDirection ReverseDiagDir(DiagDirection dir) { return (dir + 2) & 3; }
static uint32_t DiagDirToAxis(DiagDirection dir) { return dir & 1; }
static RoadBits DiagDirToRoadBits(DiagDirection dir) { return 1 << (3 - dir); }
static TileIndex TileAddByDiagDir(TileIndex tile, DiagDirection dir) { return tile + TileOffsByDiagDir(dir); }
static Slope InclinedSlope(DiagDirection dir) { return std::array<uint32_t, 4>{12, 6, 3, 9}[dir]; }
static Slope GetTileSlope(TileIndex tile) { return At(tile).slope; }
static uint32_t GetTileZ(TileIndex tile) { return At(tile).z; }
static bool IsValidTile(TileIndex tile) { return At(tile).valid; }
static bool IsTileType(TileIndex tile, uint32_t type) { return At(tile).type == type; }
static bool IsSteepSlope(Slope slope) { return (slope & 16) != 0; }
static bool IsSlopeWithOneCornerRaised(Slope slope) { return slope == 1 || slope == 2 || slope == 4 || slope == 8; }
static RoadBits GetTownRoadBits(TileIndex tile) { return At(tile).roads; }
static uint32_t GetTunnelBridgeTransportType(TileIndex tile) { return At(tile).transport; }
static DiagDirection GetTunnelBridgeDirection(TileIndex tile) { return At(tile).facing; }
static bool IsDriveThroughStopTile(TileIndex tile) { return At(tile).stop == 1; }
static uint32_t GetDriveThroughStopAxis(TileIndex tile) { return At(tile).facing; }
static bool IsBayRoadStopTile(TileIndex tile) { return At(tile).stop == 2; }
static DiagDirection GetBayRoadStopDir(TileIndex tile) { return At(tile).facing; }
static bool IsRoadDepot(TileIndex tile) { return At(tile).stop == 3; }
static DiagDirection GetRoadDepotDirection(TileIndex tile) { return At(tile).facing; }
static void RoadMask() { world->trace.push_back({21, 0, 0, 0, 0, 0, 0}); }
static void RoadInfo(uint32_t type) { world->trace.push_back({24, type, 0, 0, 0, 0, 0}); }
/* One active town-buildable type is a controlled shared service, not a second
 * copy of the migrated road-type-selection algorithm. */
static RoadType GetTownRoadType() { RoadMask(); RoadInfo(0); return 0; }
static Cost RoadCommand(TileIndex tile, RoadBits bits, RoadType rt, uint32_t town)
{
	world->trace.push_back({1, town, tile, bits, rt, 0, 0});
	return {world->road_test};
}
static Cost TunnelCommand(TileIndex tile, RoadType rt, bool execute)
{
	world->trace.push_back({5, 0, tile, rt, execute, 0, 0});
	/* A failing execute can leave effects; the original ignores its return. */
	if (execute) world->effects++;
	return {execute ? world->tunnel_execute : world->tunnel_test};
}
template <int command> struct Command {
	static Cost Do(Flags flags, TileIndex tile, uint32_t a, uint32_t b, uint32_t = 0, uint32_t town = 0)
	{
		if constexpr (command == CMD_BUILD_ROAD) {
			assert(flags.bits == 3);
			return RoadCommand(tile, a, b, town);
		} else {
			assert(a == TRANSPORT_ROAD && (flags.bits == 0 || flags.bits == 4));
			return TunnelCommand(tile, b, (flags.bits & 4) != 0);
		}
	}
};

#include "town-tunnel-original.hpp"

static void Observe(uint32_t kind, uint32_t id, uint32_t *v) noexcept
{
	std::fill_n(v, 32, 0);
	switch (kind) {
		case 0: v[0] = v[1] = 1024; v[8] = _settings_game.economy.allow_town_level_crossings; v[17] = INT32_MAX; break;
		case 1: v[3] = world->town.cache.population; break;
		case 2: {
			auto tile = At(id);
			v[0] = tile.type; v[1] = tile.slope; v[3] = tile.z; v[4] = tile.valid; v[7] = tile.roads;
			v[12] = tile.stop == 3 ? tile.facing : UINT32_MAX;
			v[13] = tile.stop < 3 ? tile.stop : 0; v[14] = tile.facing; v[15] = tile.transport; v[16] = tile.facing;
			break;
		}
		case 4: RoadInfo(id); v[0] = 1; v[2] = 50; break;
	}
}
static uint64_t Leaf(uint32_t op, uint32_t, uint32_t, uint32_t, uint32_t) noexcept { assert(op == 21); RoadMask(); return 1; }
static OpenTTDTownState *State(uint32_t id) noexcept { assert(id == world->town.index); return world->owner; }
static void Stations(uint32_t, uint32_t, void *, void (*)(void *, uint32_t, uint32_t, uint32_t)) noexcept {}
static uint32_t Random(void *) noexcept { world->draws++; return 0; }
static void ObserveTile(void *, uint32_t, uint32_t *) noexcept {}
static void WriteTile(void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) noexcept {}
static float Trig(uint32_t, float value) noexcept { return value; }
static uint32_t Industry(int32_t, int32_t, uint32_t *) noexcept { return 0; }
static bool Candidate()
{
	const OpenTTDTownLeaves leaves{Observe, Leaf, State, Stations};
	const OpenTTDSharedServices services{nullptr, Random, ObserveTile, WriteTile, Trig, Industry};
	auto task = std::unique_ptr<OpenTTDTownTask, decltype(&openttd_rust_town_task_destroy)>(openttd_rust_town_begin(&leaves, &services, 11, world->town.index, world->start, world->direction, 0, 0, 0), openttd_rust_town_task_destroy);
	OpenTTDTownAction action{};
	uint64_t result = 0;
	while (!openttd_rust_town_step(task.get(), result, 37, &action)) {
		assert(action.c == 0 && action.d == 0);
		if (action.kind == 1) result = RoadCommand(action.tile, action.a, action.b, action.town).Succeeded();
		else { assert(action.kind == 5); result = TunnelCommand(action.tile, action.a, action.b != 0).Succeeded(); }
	}
	return action.a != 0;
}
static uint32_t Along(const World &w, int distance) { return w.start + distance * TileOffsByDiagDir(w.direction); }
static World Fixture(uint32_t dir, uint32_t length, bool mountain)
{
	World w;
	w.direction = dir; w.length = length;
	w.tiles[Along(w, -1)].roads = DiagDirToRoadBits(dir);
	for (uint32_t i = 0; i <= length; i++) {
		auto &t = w.tiles[Along(w, i)];
		t.z = i == 0 || i == length ? 4 : 5;
		t.slope = InclinedSlope(dir);
	}
	if (!mountain) w.tiles[Along(w, 1)].type = MP_STATION;
	return w;
}
static uint32_t cases;
static void Compare(World input, const char *label)
{
	World reference = input, candidate = input;
	world = &reference;
	bool expected = GrowTownWithTunnel(&world->town, world->start, world->direction);
	world = &candidate;
	candidate.owner = openttd_rust_town_new();
	bool actual = Candidate();
	openttd_rust_town_destroy(candidate.owner);
	if (expected != actual || reference.trace != candidate.trace || reference.effects != candidate.effects || reference.draws != candidate.draws) {
		std::fprintf(stderr, "tunnel mismatch: case=%u %s dir=%u length=%u population=%u result=%u/%u trace=%zu/%zu effects=%u/%u RNG=%u/%u\n", cases, label, input.direction, input.length, input.town.cache.population, expected, actual, reference.trace.size(), candidate.trace.size(), reference.effects, candidate.effects, reference.draws, candidate.draws);
		for (size_t i = 0; i < std::max(reference.trace.size(), candidate.trace.size()); i++) {
			std::fprintf(stderr, "trace[%zu]", i);
			for (const auto *trace : {&reference.trace, &candidate.trace}) {
				std::fprintf(stderr, " |");
				if (i < trace->size()) for (auto value : (*trace)[i]) std::fprintf(stderr, " %u", value);
			}
			std::fprintf(stderr, "\n");
		}
		std::exit(1);
	}
	/* Emit the oracle-derived ordered transcript, not expected values duplicated
 * from Rust. Python retains this small evidence beside the generated bodies. */
	std::printf("%u %s dir=%u length=%u population=%u result=%u effects=%u RNG=%u", cases++, label, input.direction, input.length, input.town.cache.population, expected, reference.effects, reference.draws);
	for (const auto &entry : reference.trace) { std::printf(" |"); for (auto value : entry) std::printf(" %u", value); }
	std::puts("");
}
int main()
{
	for (uint32_t dir = 0; dir < 4; dir++) {
		for (bool mountain : {false, true}) {
			for (uint32_t length : {1, 2, 4, 5, 6, 7, 8, 9, 255, 256, 257}) {
				for (uint32_t population : {0, 999, 1000, 249000, 250000}) {
					auto w = Fixture(dir, length, mountain); w.town.cache.population = population; Compare(w, mountain ? "mountain-length" : "obstruction-length");
				}
			}
		}
		for (uint32_t slope = 0; slope < 32; slope++) {
			auto w = Fixture(dir, 4, true); w.tiles[w.start].slope = slope; Compare(w, "entrance-slope");
			w = Fixture(dir, 4, true); w.tiles[Along(w, 2)].slope = slope; Compare(w, "mountain-slope");
		}
		auto w = Fixture(dir, 4, false); w.tiles[Along(w, -1)].roads = 0; Compare(w, "disconnected");
		for (uint32_t place : {1, 5}) {
			for (uint32_t type : {0U, MP_ROAD, MP_RAILWAY, MP_STATION, MP_TUNNELBRIDGE}) {
				for (uint32_t stop = 0; stop < 4; stop++) {
					for (uint32_t facing = 0; facing < 4; facing++) {
						w = Fixture(dir, 4, true); auto &t = w.tiles[Along(w, place)]; t.type = type; t.stop = stop; t.facing = facing;
						Compare(w, place == 1 ? "start-continuation" : "exit-continuation");
						if (type == MP_TUNNELBRIDGE) { t.transport = 0; Compare(w, "rail-tunnel"); }
					}
				}
			}
			w = Fixture(dir, 4, true); w.tiles[Along(w, place)].valid = false; Compare(w, "invalid-continuation");
		}
		for (uint32_t place : {1, 2, 3, 4}) { w = Fixture(dir, 4, true); w.tiles[Along(w, place)].valid = false; Compare(w, "invalid-mountain-end"); }
		_settings_game.economy.allow_town_level_crossings = false;
		w = Fixture(dir, 4, true); w.tiles[Along(w, 5)].type = MP_RAILWAY; Compare(w, "crossing-disabled");
		_settings_game.economy.allow_town_level_crossings = true;
		for (uint32_t failure = 0; failure < 3; failure++) {
			w = Fixture(dir, 4, true);
			if (failure == 0) w.road_test = false;
			if (failure == 1) w.tunnel_test = false;
			if (failure == 2) w.tunnel_execute = false;
			Compare(w, "command-failure");
		}
	}
	std::printf("PASS %u unchanged-reference tunnel cases\n", cases);
}
