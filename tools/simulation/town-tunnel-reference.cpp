/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town-tunnel-reference.cpp Controlled services for unchanged town tunnel bodies, absent from the save corpus. */
#include "../../src/tests/town_protocol.hpp"
#include "../../src/rust/town_ffi.h"
#include "../../src/rust/town_house_ffi.h"
#include "../../src/rust/town_placement_ffi.h"
#include "../../src/rust/town_names_ffi.h"
#include <set>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <initializer_list>
#include <memory>
#include <map>
#include <numeric>
#include <span>
#include <ranges>
#include <limits>
#include <unordered_map>
#include <vector>
#include <bit>
#include <variant>
#define EMPTY_BASES
#include "../../src/rust/town_bytes.hpp"

using TileIndex = uint32_t;
using TileIndexDiff = int32_t;
using DiagDirection = uint32_t;
using RoadBits = uint32_t;
using RoadType = uint32_t;
using Slope = uint32_t;
constexpr uint32_t DIAGDIR_END = 4, MP_ROAD = 2, MP_RAILWAY = 1, MP_STATION = 5, MP_TUNNELBRIDGE = 9;
constexpr uint32_t TRANSPORT_ROAD = 1, DRD_NONE = 0;
constexpr size_t NUM_HOUSE_ZONES = 5;
enum class HouseZone : uint8_t { TownEdge, TownOutskirt, TownOuterSuburb, TownInnerSuburb, TownCentre };
static uint8_t to_underlying(HouseZone zone) { return static_cast<uint8_t>(zone); }
template <typename T> T ClampTo(uint64_t value) { return static_cast<T>(std::min<uint64_t>(value, std::numeric_limits<T>::max())); }
struct Town {
	uint32_t index = 17;
	struct { uint32_t population = 0, num_houses = 0; std::array<uint32_t, 5> squared_town_zone_radius{}; } cache;
	struct SuppliedHistory {
		uint32_t production = 0, transported = 0;
#include "town-percent-original.hpp"
	};
	std::array<int16_t, 15> ratings{};
	struct { uint16_t bits = 0; void Set(uint8_t company) { this->bits |= 1 << company; } } have_ratings;
};
template <typename T> T SumHistory(std::span<const T>);
struct { struct { bool allow_town_level_crossings = true; uint8_t dist_local_authority = 20; } economy; struct { uint8_t town_council_tolerance = 0; } difficulty; } _settings_game;
enum class DoCommandFlag { Auto = 1, NoWater = 2, Execute = 4, NoModifyTownRating = 8, NoTestTownRating = 16 };
struct Flags {
	uint32_t bits = 0;
	Flags() = default;
	Flags(std::initializer_list<DoCommandFlag> flags) { for (auto flag : flags) this->Set(flag); }
	bool Test(DoCommandFlag flag) const { return (this->bits & static_cast<uint32_t>(flag)) != 0; }
	Flags &Set(DoCommandFlag flag) { this->bits |= static_cast<uint32_t>(flag); return *this; }
};
using DoCommandFlags = Flags;
static bool _town_rating_test = false;
static std::map<const Town *, int> _town_test_ratings;
static uint8_t _current_company;
static struct { struct { bool value = false; } magic_bulldozer; } _cheats;
static uint32_t rating_valid_calls, rating_dirty_calls;
struct Company { static bool IsValidID(uint8_t company) { rating_valid_calls++; return company == 0 || company == 4; } };
constexpr uint32_t WC_TOWN_AUTHORITY = 1;
static void SetWindowDirty(uint32_t, uint32_t) { rating_dirty_calls++; }
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

enum class TownRatingCheckType : uint8_t { RoadRemove, TunnelBridgeRemove, End };
static constexpr uint8_t to_underlying(TownRatingCheckType type) { return static_cast<uint8_t>(type); }
constexpr int TOWN_COUNCIL_PERMISSIVE = 3, RATING_VERYPOOR = -200, RATING_MINIMUM = -1000;
constexpr int RATING_ROAD_NEEDED_LENIENT = 16, RATING_ROAD_NEEDED_NEUTRAL = 64, RATING_ROAD_NEEDED_HOSTILE = 112, RATING_ROAD_NEEDED_PERMISSIVE = RATING_MINIMUM;
constexpr int RATING_TUNNEL_BRIDGE_NEEDED_LENIENT = 144, RATING_TUNNEL_BRIDGE_NEEDED_NEUTRAL = 208, RATING_TUNNEL_BRIDGE_NEEDED_HOSTILE = 400, RATING_TUNNEL_BRIDGE_NEEDED_PERMISSIVE = RATING_MINIMUM;
constexpr uint32_t STR_ERROR_LOCAL_AUTHORITY_REFUSES_TO_ALLOW_THIS = 19;
struct CommandCost { bool ok = true; uint32_t error = 0; uint64_t parameter = 0; };
static CommandCost CommandCostWithParam(uint32_t error, uint64_t parameter) { return {false, error, parameter}; }
static Town *authority_town;
static OpenTTDTownState *authority_owner;
static uint32_t authority_closest_calls;
static Town *ClosestTownFromTile(TileIndex tile, uint32_t threshold)
{
	assert(tile == 129 && threshold == _settings_game.economy.dist_local_authority);
	authority_closest_calls++; return authority_town;
}

#include "town-tunnel-original.hpp"

static uint64_t Leaf(uint32_t op, uint32_t, uint32_t, uint32_t, uint32_t) noexcept { assert(op == 21); RoadMask(); return 1; }
static OpenTTDTownState *State(uint32_t id) noexcept { assert(id == world->town.index); return world->owner; }
static void Stations(uint32_t, uint32_t, void *, void (*)(void *, const void *, uint32_t)) noexcept {}
static uint32_t Random(void *) noexcept { world->draws++; return 0; }
static uint32_t Industry(int32_t, int32_t, uint32_t *) noexcept { return 0; }
static uint32_t Read_map_log_x() noexcept { return 10; }
static uint8_t Read_tile_type(uint32_t id) noexcept { return uint8_t(At(id).type); }
static bool Read_tile_valid(uint32_t id) noexcept { return IsValidTile(id); }
static uint8_t Read_tile_slope(uint32_t id) noexcept { return uint8_t(GetTileSlope(id)); }
static int32_t Read_tile_z(uint32_t id) noexcept { return int32_t(GetTileZ(id)); }
static uint8_t Read_road_bits(uint32_t id, uint8_t type, bool straight) noexcept { assert(type == 0 && straight); return uint8_t(GetTownRoadBits(id)); }
static OpenTTDTownRoadChoice Read_road_info(uint8_t id) noexcept { RoadInfo(id); return {.introduction_date = 0, .max_speed = 50, .town_build = true}; }
static uint64_t RoadTypes() noexcept { return Leaf(21, 0, 0, 0, 0); }
static bool BuildRoad(uint32_t town, uint32_t tile, uint32_t bits, uint32_t type, uint32_t mode) noexcept { assert(mode == 0); return RoadCommand(tile, bits, type, town).Succeeded(); }
static bool BuildTunnel(uint32_t tile, uint32_t type, uint32_t execute) noexcept { return TunnelCommand(tile, type, execute != 0).Succeeded(); }
static bool Candidate()
{
	openttd_rust_town_simulation(world->owner)->population = world->town.cache.population;
	auto leaves = TownProtocolServices();
	leaves.map_log_x = Read_map_log_x; leaves.road_info = Read_road_info;
	leaves.state = State; leaves.stations = Stations; leaves.road_mask = RoadTypes; leaves.build_road = BuildRoad; leaves.build_tunnel = BuildTunnel;
	OpenTTDSharedServices services = OpenTTDFixtureSharedServices(nullptr, Random, Industry);
	services.tile_type = Read_tile_type; services.tile_valid = Read_tile_valid;
	services.tile_slope = Read_tile_slope; services.tile_z = Read_tile_z; services.road_bits = Read_road_bits;
	services.tunnel_transport = [](uint32_t id) noexcept { return uint8_t(GetTunnelBridgeTransportType(id)); };
	services.tunnel_bridge_direction = [](uint32_t id) noexcept { return uint8_t(GetTunnelBridgeDirection(id)); };
	services.is_drive_through_stop = [](uint32_t id) noexcept { return IsDriveThroughStopTile(id); };
	services.drive_through_axis = [](uint32_t id) noexcept { return uint8_t(GetDriveThroughStopAxis(id)); };
	services.is_bay_stop = [](uint32_t id) noexcept { return IsBayRoadStopTile(id); };
	services.bay_stop_direction = [](uint32_t id) noexcept { return uint8_t(GetBayRoadStopDir(id)); };
	services.is_road_depot = [](uint32_t id) noexcept { return IsRoadDepot(id); };
	services.road_depot_direction = [](uint32_t id) noexcept { return uint8_t(GetRoadDepotDirection(id)); };
	leaves.allow_crossings = []() noexcept { return _settings_game.economy.allow_town_level_crossings; };
	return openttd_rust_town_tunnel(&leaves, &services, world->town.index, world->start, world->direction);
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
static void CompareHistories()
{
	uint32_t checked = 0;
	for (const size_t count : {1U, 3U, 4U, 61U}) {
		for (const uint32_t production : {0U, 1U, 0x7FFFFFFFU, 0x80000000U, UINT32_MAX}) {
			for (const uint32_t transported : {0U, 1U, 0x7FFFFFFFU, 0x80000000U, UINT32_MAX}) {
				for (const uint32_t stride : {0U, 17U, 0x40000000U, 0x80000000U}) {
					std::array<Town::SuppliedHistory, 61> reference{};
					std::array<OpenTTDTownSuppliedHistory, 61> candidate{};
					for (size_t i = 0; i < count; ++i) {
						const auto p = production + static_cast<uint32_t>(i) * stride;
						const auto t = transported + static_cast<uint32_t>(i) * stride;
						reference[i] = {p, t}; candidate[i] = {p, t};
					}
					const auto expected = SumHistory<Town::SuppliedHistory>(std::span{reference.data(), count});
					const auto actual = openttd_rust_town_sum_history(candidate.data(), count);
					if (actual.production != expected.production || actual.transported != expected.transported) {
						std::fprintf(stderr, "history mismatch count=%zu production=%u transported=%u stride=%u expected=%u/%u actual=%u/%u\n",
							count, production, transported, stride, expected.production, expected.transported, actual.production, actual.transported);
						std::exit(1);
					}
					checked++;
				}
			}
		}
	}
	std::printf("\nPASS %u unchanged-reference history cases\n", checked);
}
static bool RatingCompanyValid(uint8_t company) noexcept { return Company::IsValidID(company); }
static void RatingDirty(uint32_t town) noexcept { SetWindowDirty(WC_TOWN_AUTHORITY, town); }
static void CompareRatings()
{
	Town town;
	auto owner = std::unique_ptr<OpenTTDTownState, decltype(&openttd_rust_town_destroy)>(openttd_rust_town_new(), openttd_rust_town_destroy);
	auto leaves = TownProtocolServices(); leaves.dirty_authority = RatingDirty;
	OpenTTDSharedServices services = OpenTTDFixtureSharedServices(nullptr, Random, Industry);
	services.company_valid = RatingCompanyValid;
	uint32_t checked = 0;
	auto reset = [&](int16_t initial) {
		town.ratings.fill(initial); town.have_ratings.bits = 0;
		auto *state = openttd_rust_town_simulation(owner.get());
		std::fill_n(state->ratings, 15, initial); state->have_ratings = 0;
	};
	auto mode = [&](bool enabled) { SetTownRatingTestMode(enabled); openttd_rust_town_rating_test(enabled); };
	auto check = [&](int add, int maximum, uint8_t company, bool no_modify = false, bool bulldozer = false, bool null_town = false) {
		_current_company = company; _cheats.magic_bulldozer.value = bulldozer;
		rating_valid_calls = rating_dirty_calls = 0;
		ChangeTownRating(null_town ? nullptr : &town, add, maximum, no_modify ? Flags{DoCommandFlag::NoModifyTownRating} : Flags{});
		const auto expected_valid = rating_valid_calls, expected_dirty = rating_dirty_calls;
		rating_valid_calls = rating_dirty_calls = 0;
		openttd_rust_town_change_rating(&leaves, &services, null_town ? nullptr : owner.get(), town.index,
			reinterpret_cast<uintptr_t>(&town), company, add, maximum, no_modify, bulldozer);
		const auto *state = openttd_rust_town_simulation(owner.get());
		bool equal = expected_valid == rating_valid_calls && expected_dirty == rating_dirty_calls && state->have_ratings == town.have_ratings.bits && std::equal(town.ratings.begin(), town.ratings.end(), state->ratings);
		if (company == 0 || company == 4) equal &= GetRating(&town) == openttd_rust_town_rating(owner.get(), reinterpret_cast<uintptr_t>(&town), company);
		if (!equal) {
			std::fprintf(stderr, "rating mismatch case=%u add=%d max=%d company=%u mode=%u no-modify=%u cheat=%u null=%u\n", checked, add, maximum, company, _town_rating_test, no_modify, bulldozer, null_town);
			std::exit(1);
		}
		checked++;
	};
	for (const int16_t initial : {-32768, -1000, 0, 1000, 32767}) {
		for (const int add : {-40000, -1, 0, 1, 40000}) {
			for (const int maximum : {-50000, -1000, 0, 1000, 50000}) {
				reset(initial); check(add, maximum, 0); check(add, maximum, 4);
				reset(initial); mode(true); check(add, maximum, 0);
				mode(true); check(add, maximum, 4); mode(false); check(-add, maximum, 0); mode(false);
				check(add, maximum, 0, true); check(add, maximum, 255); check(add, maximum, 4, false, true); check(add, maximum, 0, false, false, true);
			}
		}
	}
	/* Source keys only the native shell address, including across company and
	 * owner reuse within nested test mode; starting a new outer mode clears it. */
	reset(10); mode(true); check(100, 1000, 0); mode(true);
	owner.reset(openttd_rust_town_new()); reset(-100); check(1, 1000, 4); mode(false); check(-1, -1000, 0); mode(false);
	mode(true); check(1, 1000, 4); mode(false);
	std::printf("\nPASS %u unchanged-reference rating cases\n", checked);
}
static void CompareRadii()
{
	Town town;
	auto owner = std::unique_ptr<OpenTTDTownState, decltype(&openttd_rust_town_destroy)>(openttd_rust_town_new(), openttd_rust_town_destroy);
	std::vector<uint32_t> counts;
	for (uint32_t houses = 0; houses <= 100; ++houses) counts.push_back(houses);
	for (uint32_t houses : {1000U, 10000U, 0x1000000U, 0x7FFFFFFFU, 0x80000000U, UINT32_MAX}) counts.push_back(houses);
	for (uint32_t houses : counts) {
		town.cache.num_houses = houses; UpdateTownRadius(&town);
		auto *state = openttd_rust_town_simulation(owner.get()); state->num_houses = houses;
		openttd_rust_town_update_radius(owner.get());
		if (!std::equal(town.cache.squared_town_zone_radius.begin(), town.cache.squared_town_zone_radius.end(), state->radius)) {
			std::fprintf(stderr, "radius mismatch houses=%u\n", houses); std::exit(1);
		}
	}
	std::printf("\nPASS %zu unchanged-reference radius cases\n", counts.size());
}
static void CompareAuthority()
{
	Town town;
	auto owner = std::unique_ptr<OpenTTDTownState, decltype(&openttd_rust_town_destroy)>(openttd_rust_town_new(), openttd_rust_town_destroy);
	authority_owner = owner.get();
	OpenTTDSharedServices shared = OpenTTDFixtureSharedServices(nullptr, Random, Industry);
	shared.current_company = +[]() noexcept { return _current_company; };
	shared.company_valid = +[](uint8_t id) noexcept { return Company::IsValidID(id); };
	shared.closest_town = +[](uint32_t tile, uint32_t threshold) noexcept { return reinterpret_cast<const OpenTTDTownHandle *>(ClosestTownFromTile(tile, threshold)); };
	shared.town_owner = +[](const OpenTTDTownHandle *) noexcept { return authority_owner; };
	const OpenTTDTownAuthorityServices a{
		.council_tolerance = +[]() noexcept { return _settings_game.difficulty.town_council_tolerance; },
		.bulldozer = +[]() noexcept { return _cheats.magic_bulldozer.value; },
		.authority_distance = +[]() noexcept { return _settings_game.economy.dist_local_authority; },
		.town_index = +[](const OpenTTDTownHandle *t) noexcept { return static_cast<uint16_t>(reinterpret_cast<const Town *>(t)->index); },
		.refusal = STR_ERROR_LOCAL_AUTHORITY_REFUSES_TO_ALLOW_THIS,
	};
	OpenTTDCommandCostServices costs{
		.zero = +[](OpenTTDCommandCost *) noexcept { std::abort(); },
		.error = +[](OpenTTDCommandCost *, uint32_t) noexcept { std::abort(); },
		.expense_money = +[](OpenTTDCommandCost *, uint8_t, int64_t) noexcept { std::abort(); },
		.error_param = +[](OpenTTDCommandCost *, uint32_t, uint64_t) noexcept { std::abort(); },
		.add = +[](OpenTTDCommandCost *, OpenTTDCommandCost *) noexcept { std::abort(); },
		.move = +[](OpenTTDCommandCost *, OpenTTDCommandCost *) noexcept { std::abort(); },
		.amount = +[](OpenTTDCommandCost *, int64_t) noexcept { std::abort(); },
		.success = +[](const OpenTTDCommandCost *) noexcept -> bool { std::abort(); },
		.error_message = +[](const OpenTTDCommandCost *) noexcept -> uint32_t { std::abort(); },
		.money = +[](const OpenTTDCommandCost *) noexcept -> int64_t { std::abort(); },
	};
	costs.zero = +[](OpenTTDCommandCost *out) noexcept { *reinterpret_cast<CommandCost *>(out) = {}; };
	costs.error_param = +[](OpenTTDCommandCost *out, uint32_t error, uint64_t parameter) noexcept { *reinterpret_cast<CommandCost *>(out) = CommandCostWithParam(error, parameter); };
	auto leaves = TownProtocolServices(); leaves.dirty_authority = RatingDirty;
	uint32_t checked = 0;
	for (uint8_t company : {0, 4, 255}) for (uint8_t council = 0; council < 4; ++council) {
		for (bool no_test : {false, true}) for (bool cheat : {false, true}) for (bool absent : {false, true}) {
			for (int16_t rating : {-32768, -1001, -1000, -999, -201, -200, -199, 15, 16, 17, 63, 64, 65, 111, 112, 113, 143, 144, 145, 207, 208, 209, 399, 400, 401, 32767}) for (bool testing : {false, true}) {
				_current_company = company; _settings_game.difficulty.town_council_tolerance = council;
				_cheats.magic_bulldozer.value = cheat; authority_town = absent ? nullptr : &town;
				town.ratings.fill(rating); auto *state = openttd_rust_town_simulation(owner.get()); std::fill_n(state->ratings, 15, rating);
				Flags flags; if (no_test) flags.Set(DoCommandFlag::NoTestTownRating);
				if (testing) {
					SetTownRatingTestMode(true); openttd_rust_town_rating_test(true);
					SetTownRatingTestMode(true); openttd_rust_town_rating_test(true);
					ChangeTownRating(&town, -40000, -1000, Flags{});
					openttd_rust_town_change_rating(&leaves, &shared, owner.get(), town.index, reinterpret_cast<uintptr_t>(&town), company, -40000, -1000, false, cheat);
				}
				for (uint8_t kind = 0; kind < 3; ++kind) {
					rating_valid_calls = authority_closest_calls = 0;
					CommandCost expected = kind == 2 ? CheckIfAuthorityAllowsNewStation(129, flags) : CheckforTownRating(flags, authority_town, static_cast<TownRatingCheckType>(kind));
					uint32_t valid_calls = rating_valid_calls, closest_calls = authority_closest_calls;
					rating_valid_calls = authority_closest_calls = 0; CommandCost actual;
					if (kind == 2) openttd_rust_town_authority_station(&a, &shared, &costs, reinterpret_cast<OpenTTDCommandCost *>(&actual), 129, no_test);
					else openttd_rust_town_authority_remove(&a, &shared, &costs, reinterpret_cast<OpenTTDCommandCost *>(&actual), reinterpret_cast<const OpenTTDTownHandle *>(authority_town), no_test, kind);
					if (actual.ok != expected.ok || actual.error != expected.error || actual.parameter != expected.parameter || rating_valid_calls != valid_calls || authority_closest_calls != closest_calls) {
						std::fprintf(stderr, "authority mismatch case=%u kind=%u rating=%d company=%u council=%u\n", checked, kind, rating, company, council); std::exit(1);
					}
					checked++;
				}
				if (testing) {
					SetTownRatingTestMode(false); openttd_rust_town_rating_test(false);
					SetTownRatingTestMode(false); openttd_rust_town_rating_test(false);
				}
			}
		}
	}
	std::printf("\nPASS %u unchanged-reference authority cases\n", checked);
}
static void ComparePercentTransported()
{
	uint32_t checked = 0;
	for (uint32_t production : {0U, 1U, 255U, 256U, 0x1000000U, UINT32_MAX}) {
		for (uint32_t transported : {0U, 1U, 255U, 256U, 0x1000000U, UINT32_MAX}) {
			Town::SuppliedHistory original{production, transported};
			OpenTTDTownSuppliedHistory candidate{production, transported};
			if (original.PctTransported() != openttd_rust_town_percent_transported(&candidate)) {
				std::fprintf(stderr, "transported percent mismatch production=%u transported=%u\n", production, transported); std::exit(1);
			}
			checked++;
		}
	}
	std::printf("\nPASS %u unchanged-reference percent cases\n", checked);
}
namespace house_probe {
using uint = unsigned int;
constexpr uint32_t MP_HOUSE = 3, TOWN_HOUSE_COMPLETED = 3;
struct Map { static auto Iterate() { return std::views::iota(uint32_t(0), uint32_t(256)); } };
enum class BuildingFlag : uint8_t { Size2x1 = 2 };
using HouseID = uint16_t;
struct BuildingFlags { uint8_t bits; bool Test(BuildingFlag flag) const { return (this->bits & (1 << static_cast<uint8_t>(flag))) != 0; } bool Any(BuildingFlags other) const { return (this->bits & other.bits) != 0; } };
constexpr BuildingFlags BUILDING_HAS_1_TILE{29}, BUILDING_2_TILES_Y{24}, BUILDING_2_TILES_X{20}, BUILDING_HAS_4_TILES{16};
struct Cell { uint8_t byte = 0, type = 3; bool completed = false; uint16_t house = 0; bool operator ==(const Cell &) const = default; };
struct Town { uint32_t population = 0; static Town *GetByTile(uint32_t); };
struct HouseSpec { BuildingFlags building_flags{1}; uint8_t population = 10; static const HouseSpec *Get(uint16_t); };
struct World {
	std::array<Cell, 256> cells{};
	Town town;
	HouseSpec spec;
	bool candidate = false;
	uint8_t reentry = 0;
	OpenTTDTownState *owner = nullptr;
	std::vector<std::array<uint32_t, 3>> events;
};
static World *w;
static std::array<HouseSpec, 3> north_specs;
static uint16_t north_house;
static bool north_mode;
Town *Town::GetByTile(uint32_t) { return &w->town; }
const HouseSpec *HouseSpec::Get(uint16_t house) {
	if (north_mode && house < north_house && north_house - house <= 3) return &north_specs[north_house - house - 1];
	return &w->spec;
}
struct Tile {
	uint32_t index;
	Tile(uint32_t index) : index(index) {}
	uint8_t &m5() { return w->cells[this->index].byte; }
};
static bool IsTileType(Tile tile, uint32_t type) { return w->cells[tile.index].type == type; }
static bool IsHouseCompleted(Tile tile) { return w->cells[tile.index].completed; }
static void SetHouseCompleted(Tile tile, bool value) { w->cells[tile.index].completed = value; }
static uint16_t GetHouseType(uint32_t tile) { return w->cells[tile].house; }
static TileIndexDiff TileDiffXY(int x, int y) { return x + y * 64; }
static uint32_t TileAddXY(uint32_t tile, int x, int y) { return tile + TileDiffXY(x, y); }
static void ChangePopulation(Town *town, int mod) { town->population += mod; w->events.push_back({2, 0, town->population}); }
static void TriggerHouseAnimation_ConstructionStageChanged(uint32_t tile, bool initial)
{
	assert(!initial); w->events.push_back({1, tile, initial});
	if (w->reentry) {
		/* An ordinary presentation/native callback reenters the same owner. */
		if (w->candidate) openttd_rust_town_simulation(w->owner)->population += 100;
		else w->town.population += 100;
		if (w->reentry == 2) w->cells[tile].completed = !w->cells[tile].completed;
	}
}
static void MarkTileDirtyByTile(uint32_t tile) { w->events.push_back({3, tile, 0}); }
#define debug_inline gnu::always_inline
#include "town-house-original.hpp"
#undef debug_inline
static OpenTTDTownHouseServices Services()
{
	return {
		.byte = [](uint32_t tile) noexcept { return w->cells[tile].byte; },
		.completed = [](uint32_t tile) noexcept { return w->cells[tile].completed; },
		.set_byte = [](uint32_t tile, uint8_t byte) noexcept { w->cells[tile].byte = byte; },
		.set_completed = [](uint32_t tile, bool value) noexcept { w->cells[tile].completed = value; },
		.house = [](uint32_t tile) noexcept { return w->cells[tile].house; },
		.population = [](uint16_t) noexcept { return w->spec.population; },
		.construction_animation = [](uint32_t tile, bool initial) noexcept { TriggerHouseAnimation_ConstructionStageChanged(tile, initial); },
	};
}
static void Compare()
{
	uint32_t checked = 0;
	auto owner = std::unique_ptr<OpenTTDTownState, decltype(&openttd_rust_town_destroy)>(openttd_rust_town_new(), openttd_rust_town_destroy);
	auto leaves = TownProtocolServices();
	leaves.state = [](uint32_t) noexcept { return w->owner; };
	leaves.house_flags = [](uint16_t house) noexcept { return HouseSpec::Get(house)->building_flags.bits; };
	leaves.population_changed = [](uint32_t) noexcept { w->events.push_back({2, 0, openttd_rust_town_simulation(w->owner)->population}); };
	auto shared = OpenTTDFixtureSharedServices(nullptr, Random, Industry);
	shared.town_index = [](uint32_t) noexcept { return uint16_t(0); };
	shared.mark_dirty = [](uint32_t tile) noexcept { MarkTileDirtyByTile(tile); };
	shared.tile_type = [](uint32_t tile) noexcept { return w->cells[tile].type; };
	const auto services = Services();
	for (uint8_t flags : {1, 4, 8, 16}) {
		for (uint8_t reentry : {0, 1, 2}) {
			for (bool complete : {false, true}) {
				for (uint32_t byte = 0; byte < 256; byte++) {
					World original; original.owner = owner.get(); original.reentry = reentry; original.spec.building_flags.bits = flags; original.town.population = 7;
					for (uint32_t tile : {128, 129, 192, 193}) { original.cells[tile].byte = static_cast<uint8_t>(byte); original.cells[tile].completed = complete; }
					World candidate = original; candidate.candidate = true;
					w = &original; AdvanceHouseConstruction(128);
					openttd_rust_town_simulation(owner.get())->population = candidate.town.population;
					w = &candidate; openttd_rust_town_advance_house(&leaves, &shared, &services, 128);
					if (original.cells != candidate.cells || original.town.population != openttd_rust_town_simulation(owner.get())->population || original.events != candidate.events) {
						std::fprintf(stderr, "house construction mismatch flags=%u byte=%u reentry=%u complete=%u\n", flags, byte, reentry, complete); std::exit(1);
					}
					checked++;
				}
			}
		}
	}
	for (bool complete : {false, true}) {
		for (uint8_t pattern : {0, 1, 2}) {
			World original; original.owner = owner.get();
			for (uint32_t tile = 0; tile < 256; tile++) original.cells[tile] = {static_cast<uint8_t>(tile), uint8_t(pattern == 0 || (pattern == 1 && tile % 2 == 0) ? MP_HOUSE : 0), complete};
			World candidate = original;
			w = &original; ReferenceYearly();
			w = &candidate; openttd_rust_town_yearly(&shared, &services, 256);
			if (original.cells != candidate.cells) { std::fprintf(stderr, "house yearly mismatch complete=%u pattern=%u\n", complete, pattern); std::exit(1); }
			checked += 256;
		}
	}
	north_mode = true;
	World north_world; w = &north_world;
	for (uint16_t house : {0, 1, 2, 3, 4, 35, 110, 256, 1024, 4095}) {
		north_house = house;
		for (size_t slot = 0; slot < 3; slot++) {
			for (uint32_t flags = 0; flags < 256; flags++) {
				for (auto &spec : north_specs) spec.building_flags.bits = 0;
				north_specs[slot].building_flags.bits = static_cast<uint8_t>(flags);
				HouseID expected = house;
				const auto diff = GetHouseNorthPart(expected);
				const auto actual = openttd_rust_town_house_north(&leaves, &shared, house);
				if (expected != actual.house || diff != actual.diff) { std::fprintf(stderr, "house north mismatch house=%u slot=%zu flags=%u\n", house, slot, flags); std::exit(1); }
				checked++;
			}
		}
	}
	north_mode = false;
	std::printf("\nPASS %u unchanged-reference house cases\n", checked);
}
} // namespace house_probe

namespace placement_probe {
using uint = unsigned int;
constexpr uint32_t MP_CLEAR = 0, MP_TREES = 4, CLEAR_ROUGH = 1, TREE_GROUND_ROUGH = 1;
constexpr uint32_t DIAGDIR_BEGIN = 0, INVALID_DIAGDIR = 255, DIR_W = 0, INVALID_TILE = UINT32_MAX;
enum TownLayout : uint8_t { TL_ORIGINAL, TL_BETTER_ROADS, TL_2X2_GRID, TL_3X3_GRID, TL_RANDOM };
enum ExpensesType : uint8_t { EXPENSES_OTHER = 12 };
constexpr uint32_t STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP_SUB = 31, STR_ERROR_TOO_CLOSE_TO_ANOTHER_TOWN = 32, STR_ERROR_SITE_UNSUITABLE = 33;
constexpr uint32_t CUSTOM_TOWN_NUMBER_DIFFICULTY = 4;
struct CommandCost {
	bool ok = true; uint32_t error = 0; uint8_t expense = 255;
	CommandCost() = default;
	explicit CommandCost(uint32_t error) : ok(false), error(error) {}
	explicit CommandCost(ExpensesType expense) : expense(expense) {}
	bool Failed() const { return !this->ok; }
	bool operator ==(const CommandCost &) const = default;
};
struct Cell { uint8_t type = MP_CLEAR, slope = 0, ground = 0; int z = 4, max_z = 4; };
struct Town { uint32_t xy = 0; static Town *Get(uint32_t); };
struct World {
	uint32_t width = 64, height = 64, pattern = 0;
	bool empty = true;
	Town town;
	OpenTTDTownState *owner = nullptr;
	std::map<uint32_t, Cell> cells;
	std::vector<uint32_t> nearest, water;
};
static World *w;
static Cell At(uint32_t tile) {
	auto found = w->cells.find(tile); if (found != w->cells.end()) return found->second;
	if (w->pattern == 1) return {.type = 6};
	if (w->pattern == 2) return {.slope = 1};
	if (w->pattern == 3) return {.type = uint8_t(tile % 7), .slope = uint8_t(tile % 5 == 0), .ground = uint8_t(tile % 3 == 0), .z = int(tile % 3 + 3), .max_z = int(tile % 3 + 3)};
	return {};
}
static uint32_t TileX(uint32_t tile) { return tile & (w->width - 1); }
static uint32_t TileY(uint32_t tile) { return tile / w->width; }
static uint32_t TileXY(uint32_t x, uint32_t y) { return x + y * w->width; }
struct Diff { int x, y; };
static Diff TileIndexDiffCByDir(uint32_t) { return {1, -1}; }
static Diff TileIndexDiffCByDiagDir(uint32_t dir) { return std::array<Diff, 4>{{{-1, 0}, {0, 1}, {1, 0}, {0, -1}}}[dir]; }
#include "town-ceil-original.hpp"
struct Map {
	static uint32_t SizeX() { return w->width; } static uint32_t SizeY() { return w->height; }
	static uint32_t MaxX() { return w->width - 1; } static uint32_t MaxY() { return w->height - 1; }
	static uint32_t LogX() { return std::countr_zero(w->width); } static uint32_t LogY() { return std::countr_zero(w->height); }
#include "town-scale-original.hpp"
};
struct Index {
	size_t Count() const { return !w->empty; }
	uint32_t FindNearest(uint32_t x, uint32_t y) const { w->nearest.push_back(TileXY(x, y)); return 0; }
};
static Index _town_kdtree;
Town *Town::Get(uint32_t) { return &w->town; }
static uint32_t DistanceManhattan(uint32_t tile, uint32_t town) { return std::abs(int(TileX(tile)) - int(TileX(town))) + std::abs(int(TileY(tile)) - int(TileY(town))); }
static uint8_t GetTileType(uint32_t tile) { return At(tile).type; }
static bool IsTileType(uint32_t tile, uint32_t type) { return GetTileType(tile) == type; }
static bool IsTileFlat(uint32_t tile) { return At(tile).slope == 0; }
static uint8_t GetClearGround(uint32_t tile) { return At(tile).ground; }
static uint8_t GetTreeGround(uint32_t tile) { return At(tile).ground; }
static int GetTileZ(uint32_t tile) { return At(tile).z; }
static int GetTileMaxZ(uint32_t tile) { return At(tile).max_z; }
static uint32_t GetClosestWaterDistance(uint32_t tile, bool water) { assert(water); w->water.push_back(tile); return (tile * 2654435761U) >> 28; }
static struct { struct { uint8_t number_towns = 0; } difficulty; } _settings_game;
static struct { struct { uint16_t custom_town_number = 0; } game_creation; } _settings_newgame;
#include "town-placement-original.hpp"
static OpenTTDTownPlacementServices Services() {
	return {
		.spatial_empty = []() noexcept { return _town_kdtree.Count() == 0; },
		.nearest_town = [](uint32_t tile) noexcept { auto id = _town_kdtree.FindNearest(TileX(tile), TileY(tile)); return reinterpret_cast<const OpenTTDTownHandle *>(Town::Get(id)); },
		.clear_ground = [](uint32_t tile) noexcept { return GetClearGround(tile); },
		.tree_ground = [](uint32_t tile) noexcept { return GetTreeGround(tile); },
		.water_distance = [](uint32_t tile, bool water) noexcept { return GetClosestWaterDistance(tile, water); },
		.number_towns = []() noexcept { return _settings_game.difficulty.number_towns; },
		.custom_towns = []() noexcept { return _settings_newgame.game_creation.custom_town_number; },
		.edge_error = STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP_SUB, .town_error = STR_ERROR_TOO_CLOSE_TO_ANOTHER_TOWN, .site_error = STR_ERROR_SITE_UNSUITABLE,
	};
}
static void Compare() {
	uint32_t checked = 0;
	auto owner = std::unique_ptr<OpenTTDTownState, decltype(&openttd_rust_town_destroy)>(openttd_rust_town_new(), openttd_rust_town_destroy);
	auto leaves = TownProtocolServices();
	leaves.map_log_x = []() noexcept { return Map::LogX(); }; leaves.map_size_y = []() noexcept { return Map::SizeY(); };
	auto shared = OpenTTDFixtureSharedServices(nullptr, Random, Industry);
	shared.town_owner = [](const OpenTTDTownHandle *) noexcept { return w->owner; };
	shared.distance_from_edge = [](uint32_t tile) noexcept { return DistanceFromEdge(tile); };
	shared.tile_type = [](uint32_t tile) noexcept { return GetTileType(tile); };
	shared.tile_slope = [](uint32_t tile) noexcept { return At(tile).slope; };
	shared.tile_z = [](uint32_t tile) noexcept { return GetTileZ(tile); };
	shared.tile_max_z = [](uint32_t tile) noexcept { return GetTileMaxZ(tile); };
	const auto services = Services();
	OpenTTDCommandCostServices costs{};
	costs.error = [](OpenTTDCommandCost *out, uint32_t error) noexcept { *reinterpret_cast<CommandCost *>(out) = CommandCost(error); };
	costs.expense_money = [](OpenTTDCommandCost *out, uint8_t expense, int64_t amount) noexcept { assert(expense == EXPENSES_OTHER && amount == 0); *reinterpret_cast<CommandCost *>(out) = CommandCost(EXPENSES_OTHER); };
	for (uint32_t width : {64, 128}) for (uint32_t height : {64, 128}) {
		World base; base.width = width; base.height = height; base.owner = owner.get(); w = &base;
		const auto center = TileXY(24, 24);
		for (uint32_t valid : {0, 11, 12, 13, 25}) for (uint8_t type : {uint8_t(MP_CLEAR), uint8_t(MP_TREES), uint8_t(2)}) for (int level : {0, 4, 255}) {
			base.cells.clear(); uint32_t index = 0;
			for (auto tile : SpiralTileSequence(center, 5)) { base.cells[tile] = {.type = type, .ground = uint8_t(index++ >= valid), .z = level, .max_z = level}; }
			for (uint32_t distance : {0, 19, 20, 21}) for (uint8_t slope : {0, 1, 16}) for (bool surrounding : {false, true}) {
				World original = base; original.empty = distance == 0; original.town.xy = center + distance; original.cells[center].slope = slope;
				World candidate = original;
				w = &original; const auto expected = TownCanBePlacedHere(center, surrounding);
				openttd_rust_town_simulation(owner.get())->xy = candidate.town.xy;
				w = &candidate; CommandCost actual; openttd_rust_town_site(&leaves, &shared, &services, &costs, reinterpret_cast<OpenTTDCommandCost *>(&actual), center, surrounding);
				if (expected != actual || original.nearest != candidate.nearest) { std::fprintf(stderr, "town site mismatch valid=%u type=%u level=%d distance=%u slope=%u surrounding=%u\n", valid, type, level, distance, slope, surrounding); std::exit(1); } checked++;
			}
		}
		for (uint32_t edge : {0, 11, 12}) for (TownLayout layout : {TL_ORIGINAL, TL_BETTER_ROADS, TL_2X2_GRID, TL_3X3_GRID, TL_RANDOM}) for (uint32_t pattern : {0, 1, 2, 3}) {
			World original; original.width = width; original.height = height; original.owner = owner.get(); original.pattern = pattern;
			const auto tile = TileXY(edge, height / 2); World candidate = original;
			w = &original; const auto expected = FindNearestGoodCoastalTownSpot(tile, layout); const auto alignment = AlignTileToGrid(tile, layout);
			w = &candidate; const auto actual = openttd_rust_town_coastal(&leaves, &shared, &services, tile, layout);
			if (expected != actual || original.nearest != candidate.nearest || original.water != candidate.water || alignment != openttd_rust_town_align(&leaves, &shared, tile, layout)) { std::fprintf(stderr, "town coastal mismatch width=%u height=%u edge=%u layout=%u pattern=%u\n", width, height, edge, layout, pattern); std::exit(1); } checked++;
		}
		for (uint8_t number = 0; number <= 4; number++) for (uint16_t custom : {0, 1, 5000, 65535}) {
			_settings_game.difficulty.number_towns = number; _settings_newgame.game_creation.custom_town_number = custom;
			w = &base; if (GetDefaultTownsForMapSize() != openttd_rust_town_default_number(&leaves, &shared, &services)) { std::fprintf(stderr, "town default count mismatch\n"); std::exit(1); } checked++;
		}
	}
	std::printf("\nPASS %u unchanged-reference placement cases\n", checked);
}
} // namespace placement_probe

namespace town_bytes_probe {
struct Original {
	std::string name{}, cached_name{};
	void FillCachedName() const { const_cast<Original *>(this)->cached_name = "Generated"; }
#include "town-cached-name-original.hpp"
};
struct Candidate {
	RustTownName name;
	std::string cached_name{};
	explicit Candidate(OpenTTDTownState *owner) : name(owner) {}
	void FillCachedName() const { const_cast<Candidate *>(this)->cached_name = "Generated"; }
#include "town-cached-name-original.hpp"
};
static void Compare()
{
	auto owner = std::unique_ptr<OpenTTDTownState, decltype(&openttd_rust_town_destroy)>(openttd_rust_town_new(), openttd_rust_town_destroy);
	const std::array<std::string, 8> corpus = {"", "A", "Renamed town", std::string("A\0B", 3), std::string("\0", 1), std::string("\xff\x80\0Z", 4), std::string("\xe2\x82\xac"), std::string(4096, 'X')};
	uint32_t checked = 0;
	for (const auto &bytes : corpus) {
		Original original; Candidate candidate(owner.get());
		original.name = "Initial"; candidate.name = "Initial";
		const auto &reference = original.GetCachedName(); const auto &actual = candidate.GetCachedName();
		original.name = bytes; candidate.name = bytes;
		if (reference != actual || candidate.name.empty() != original.name.empty() || candidate.name.view() != original.name || candidate.name != std::string_view(original.name) || std::string_view(original.name) != candidate.name) std::abort();
		if (original.GetCachedName() != candidate.GetCachedName() || &actual != &static_cast<const std::string &>(candidate.name)) std::abort();
		/* Copy an aliased owner input before replacing its allocation. */
		auto alias = openttd_rust_town_name(owner.get()); openttd_rust_town_set_name(owner.get(), alias);
		if (candidate.name.view() != original.name) std::abort();
		original.cached_name.clear(); candidate.cached_name.clear(); original.name.clear(); candidate.name.clear();
		if (!reference.empty() || !actual.empty() || original.GetCachedName() != candidate.GetCachedName()) std::abort();
		RustTownText text(owner.get()); const auto encoded = EncodedString::FromEncodedView(bytes);
		text = encoded; if (text.empty() != encoded.empty() || text.encoded().GetEncodedView() != encoded.GetEncodedView()) std::abort();
		alias = openttd_rust_town_text(owner.get()); openttd_rust_town_set_text(owner.get(), alias);
		if (text.encoded().GetEncodedView() != bytes) std::abort();
		text.clear(); if (!text.empty() || !text.encoded().empty()) std::abort();
		checked++;
	}
	std::printf("\nPASS %u unchanged-reference town-byte cases\n", checked);
}
} // namespace town_bytes_probe

namespace town_names_probe {
using TownNames = std::set<std::string>;
constexpr size_t MAX_LENGTH_TOWN_NAME_CHARS = 32;
constexpr uint16_t SPECSTR_TOWNNAME_START = 0x20c0;
struct Town {
	uint16_t index;
	uint32_t townnamegrfid, townnameparts;
	uint16_t townnametype;
	std::string name;
	OpenTTDTownState *owner;
	static const std::vector<const Town *> &Iterate();
};
struct Fixture {
	std::vector<std::unique_ptr<Town>> towns;
	std::vector<const Town *> pool;
	std::array<std::string, 4> names;
	std::vector<std::array<uint32_t, 4>> trace;
	OpenTTDTownNameParams selected{0, SPECSTR_TOWNNAME_START};
	std::vector<uint32_t> random;
	uint32_t draws = 0;
	~Fixture() { for (auto &t : this->towns) openttd_rust_town_destroy(t->owner); }
	void Add(uint16_t id, std::string custom, uint32_t parts, uint32_t grfid = 0) {
		auto t = std::make_unique<Town>(Town{id, grfid, parts, SPECSTR_TOWNNAME_START, std::move(custom), openttd_rust_town_new()});
		auto fields = openttd_rust_town_simulation(t->owner); fields->townnamegrfid = grfid; fields->townnametype = t->townnametype; fields->townnameparts = parts;
		openttd_rust_town_set_name(t->owner, {reinterpret_cast<const uint8_t *>(t->name.data()), t->name.size()});
		this->pool.push_back(t.get()); this->towns.push_back(std::move(t));
	}
};
static Fixture *fixture;
const std::vector<const Town *> &Town::Iterate() { return fixture->pool; }
static const void *GetGRFTownName(uint32_t id) { fixture->trace.push_back({1, id, 0, 0}); return id == 7 ? fixture : nullptr; }
struct TownNameParams {
	uint32_t grfid;
	uint16_t type;
	explicit TownNameParams(uint8_t) { fixture->trace.push_back({2, 0, 0, 0}); this->grfid = fixture->selected.grfid; this->type = fixture->selected.kind; }
	TownNameParams(const Town *t);
};
struct { struct { uint8_t town_name = 0; } game_creation; } _settings_game;
struct Randomizer { uint32_t Next() { fixture->trace.push_back({3, fixture->draws, 0, 0}); return fixture->random[fixture->draws++ % fixture->random.size()]; } };
static std::string GetTownName(const TownNameParams *params, uint32_t parts) {
	fixture->trace.push_back({4, params->grfid, params->type, parts});
	return fixture->names[(parts + (params->grfid == 7 ? 1 : 0)) & 3];
}
template <typename T> constexpr unsigned GB(T value, unsigned start, unsigned length) { return (static_cast<std::make_unsigned_t<T>>(value) >> start) & ((1U << length) - 1); }
std::pair<size_t, char32_t> DecodeUtf8(std::string_view);
#include "town-names-original.hpp"
static const OpenTTDTownNameServices &Services()
{
	static const OpenTTDTownNameServices services{
		.params = []() noexcept { TownNameParams p(_settings_game.game_creation.town_name); return OpenTTDTownNameParams{p.grfid, p.type}; },
		.next_random = [](OpenTTDTownRandomizer *r) noexcept { return reinterpret_cast<Randomizer *>(r)->Next(); },
		.next_town = [](uint32_t from) noexcept { for (auto t : fixture->pool) if (t->index >= from) return OpenTTDTownNameCursor{reinterpret_cast<const OpenTTDTownHandle *>(t), t->index}; return OpenTTDTownNameCursor{}; },
		.grf_valid = [](uint32_t id) noexcept { return GetGRFTownName(id) != nullptr; },
		.render = [](OpenTTDTownNameParams p, uint32_t parts, void *context, bool (*consume)(void *, OpenTTDByteSpan)) noexcept {
			TownNameParams params(uint8_t{0}); fixture->trace.pop_back(); params.grfid = p.grfid; params.type = p.kind;
			auto name = GetTownName(&params, parts); return consume(context, {reinterpret_cast<const uint8_t *>(name.data()), name.size()});
		},
	};
	return services;
}
static void Compare()
{
	uint32_t checked = 0;
	const std::array<std::string, 10> corpus{"", "A", std::string(31, 'X'), std::string(32, 'X'), std::string(33, 'X'), std::string("A\0B", 3), std::string("\xff\x80\0Z", 4), std::string(96, '\x80'), std::string(31, '\xe2'), std::string("\xe2\x82\xac")};
	for (const auto &bytes : corpus) for (uint32_t mode = 0; mode < 6; mode++) {
		Fixture f; fixture = &f; f.names = {bytes, "Other", "Other", "Last"}; f.random = {0, 1, 2, 3};
		if (mode == 1) f.Add(1, bytes, 0);
		if (mode == 2) f.Add(2, "", 2);
		if (mode == 3) { f.Add(1, "", 0, 99); f.Add(65534, "Other", 1); }
		if (mode == 4) { f.Add(2, "", 3, 7); f.Add(7, "Other", 1); }
		auto shared = OpenTTDFixtureSharedServices(nullptr, Random, Industry);
		shared.town_owner = [](const OpenTTDTownHandle *t) noexcept { return reinterpret_cast<const Town *>(t)->owner; };
		OpenTTDTownNamesScratch scratch; openttd_rust_town_names_init(&scratch);
		TownNames original;
		if (mode == 5) { original.insert(bytes); openttd_rust_town_names_insert(&scratch, {reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size()}); }
		for (bool set : {false, true}) for (uint32_t parts = 0; parts < 4; parts++) {
			TownNameParams p(uint8_t{0}); f.trace.clear(); bool expected = VerifyTownName(parts, &p, set ? &original : nullptr); auto trace = f.trace; f.trace.clear();
			bool actual = openttd_rust_town_verify_name(&Services(), &shared, parts, {p.grfid, p.type}, set ? &scratch : nullptr);
			if (actual != expected || f.trace != trace) std::abort(); checked++;
		}
		for (const auto &name : corpus) {
			bool expected = IsUniqueTownName(name);
			if (expected != openttd_rust_town_unique_name(&Services(), &shared, {reinterpret_cast<const uint8_t *>(name.data()), name.size()})) std::abort(); checked++;
		}
		for (bool set : {false, true}) for (bool failure : {false, true}) {
			if (failure) { f.names.fill(std::string(32, 'X')); }
			Randomizer randomizer; uint32_t expected_parts = 0xfeedbeef, actual_parts = expected_parts; f.trace.clear(); f.draws = 0;
			bool expected = GenerateTownName(randomizer, &expected_parts, set ? &original : nullptr); auto trace = f.trace; auto draws = f.draws; f.trace.clear(); f.draws = 0;
			bool actual = openttd_rust_town_generate_name(&Services(), &shared, reinterpret_cast<OpenTTDTownRandomizer *>(&randomizer), &actual_parts, set ? &scratch : nullptr);
			if (actual != expected || actual_parts != expected_parts || f.trace != trace || f.draws != draws) std::abort(); checked++;
		}
		openttd_rust_town_names_clear(&scratch); original.clear();
		TownNameParams p(uint8_t{0}); f.names.fill("After clear");
		if (VerifyTownName(0, &p, &original) != openttd_rust_town_verify_name(&Services(), &shared, 0, {p.grfid, p.type}, &scratch)) std::abort(); checked++;
		openttd_rust_town_names_destroy(&scratch);
	}
	std::printf("\nPASS %u unchanged-reference name-policy cases\n", checked);
}
} // namespace town_names_probe

int main()
{
	town_names_probe::Compare();
	town_bytes_probe::Compare();
	placement_probe::Compare();
	house_probe::Compare();
	CompareHistories();
	CompareRatings();
	CompareRadii();
	CompareAuthority();
	ComparePercentTransported();
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
