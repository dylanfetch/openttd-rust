/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_protocol.hpp Narrow expansion-mode and callback-reentry gaps absent from console saves. */
#ifndef TOWN_PROTOCOL_HPP
#define TOWN_PROTOCOL_HPP
#include "../rust/town_ffi.h"
#include <algorithm>
#include <array>
#include <memory>
#include <vector>

struct TownProtocolWorld {
	OpenTTDTownState *owner;
	uint32_t houses = 0, population = 0, draws = 0, company = 7;
	bool house_callback = false;
	std::vector<std::array<uint32_t, 4>> writes;
};
static inline TownProtocolWorld *town_protocol_world;
static inline void TownProtocolObserve(uint32_t kind, uint32_t id, uint32_t *v) noexcept
{
	std::fill_n(v, 32, 0);
	auto &w = *town_protocol_world;
	switch (kind) {
		case 0: v[0] = v[1] = 64; v[3] = 2; v[4] = 70; v[9] = 13; v[11] = 15; v[12] = 16; v[14] = 1950; break;
		case 1: v[0] = 2080; v[2] = w.houses; v[3] = w.population; std::fill_n(v + 6, 5, 100); break;
		case 2: v[4] = v[5] = 1; v[6] = v[12] = UINT32_MAX; v[8] = 63; break;
		case 3: v[0] = 0xFFFF; v[1] = 1; v[2] = id == 0 ? 1 : 16; v[4] = 65535; v[5] = 1; v[6] = 0; v[7] = 5000; v[9] = 10; v[10] = w.house_callback; break;
	}
}
static inline uint64_t TownProtocolLeaf(uint32_t op, uint32_t id, uint32_t a, uint32_t b, uint32_t c) noexcept
{
	auto &w = *town_protocol_world;
	switch (op) {
		case 8: { uint32_t previous = w.company; w.company = a; return previous; }
		case 10: w.writes.push_back({id, a, b, c}); break;
		case 13: w.houses += a; break;
		case 14: w.population += a; break;
		case 17: return 1;
		case 20: return UINT32_MAX;
	}
	return 0;
}
static inline OpenTTDTownState *TownProtocolState(uint32_t) noexcept { return town_protocol_world->owner; }
static inline void TownProtocolStations(uint32_t, uint32_t, void *, void (*)(void *, uint32_t, uint32_t, uint32_t)) noexcept {}
static inline uint32_t TownProtocolRandom(void *) noexcept { town_protocol_world->draws++; return 0xABCDEF01; }
static inline void TownProtocolTile(void *, uint32_t, uint32_t *) noexcept {}
static inline void TownProtocolWrite(void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) noexcept {}
static inline float TownProtocolTrig(uint32_t, float x) noexcept { return x; }
static inline uint32_t TownProtocolIndustry(int32_t, int32_t, uint32_t *) noexcept { return 0; }

template <typename Check> void CheckTownProtocol(Check check)
{
	/* Fixture outcomes are pinned to GrowTown/CmdExpandTown and BuildTownHouse:
	 * first-road returns success even if execute failed; buildings-only cannot
	 * create that road; expansion failure tries 25 times. Reentry writes a flag
	 * while Rust is suspended, which subsequent singleton writes must preserve. */
	auto owner = std::unique_ptr<OpenTTDTownState, decltype(&openttd_rust_town_destroy)>(openttd_rust_town_new(), openttd_rust_town_destroy);
	TownProtocolWorld w{}; w.owner = owner.get(); town_protocol_world = &w;
	const OpenTTDTownLeaves leaves{TownProtocolObserve, TownProtocolLeaf, TownProtocolState, TownProtocolStations};
	const OpenTTDSharedServices services{nullptr, TownProtocolRandom, TownProtocolTile, TownProtocolWrite, TownProtocolTrig, TownProtocolIndustry};
	auto begin = [&](uint32_t op, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0, uint32_t d = 0) {
		return std::unique_ptr<OpenTTDTownTask, decltype(&openttd_rust_town_task_destroy)>(openttd_rust_town_begin(&leaves, &services, op, 0, 2080, a, b, c, d), openttd_rust_town_task_destroy);
	};
	OpenTTDTownAction action{};
	auto buildings = begin(0, 1);
	check(openttd_rust_town_step(buildings.get(), 0, 0, &action) == 1 && action.a == 0 && w.draws == 0 && w.company == 7);
	auto roads = begin(0, 2);
	check(openttd_rust_town_step(roads.get(), 0, 0, &action) == 0 && action.kind == 2 && w.company == 15);
	check(openttd_rust_town_step(roads.get(), 1, 0, &action) == 0 && action.kind == 1 && action.c == 2 && w.draws == 1);
	check(openttd_rust_town_step(roads.get(), 0, 0, &action) == 1 && action.a == 1 && w.company == 7);
	auto expansion = begin(6, 1, 2);
	uint32_t failed = 0;
	while (!openttd_rust_town_step(expansion.get(), 0, 0, &action)) { check(action.kind == 2); failed++; }
	check(failed == 25 * 13 && w.company == 7 && w.draws == 1);
	w.house_callback = true;
	auto selection = begin(1, 1);
	check(openttd_rust_town_step(selection.get(), 0, 0, &action) == 0 && action.kind == 2);
	check(openttd_rust_town_step(selection.get(), 1, 0, &action) == 0 && action.kind == 7);
	openttd_rust_town_set(owner.get(), 4, 0x88); // Reentrant callback updates the same live owner.
	check(openttd_rust_town_step(selection.get(), 1, 0, &action) == 0 && action.kind == 6 && w.houses == 1);
	check(openttd_rust_town_step(selection.get(), 1, 0, &action) == 0 && action.kind == 8 && w.writes.size() == 1);
	check(openttd_rust_town_get(owner.get(), 4) == 0x88);
	check(openttd_rust_town_step(selection.get(), 0, 0, &action) == 1 && action.a == 1);
	w.writes.clear(); w.house_callback = false;
	auto multi = begin(2, 1, 43, 1, 1);
	uint32_t clears = 0, animations = 0;
	while (!openttd_rust_town_step(multi.get(), 1, 0, &action)) {
		if (action.kind == 6) clears++;
		else { check(action.kind == 8); animations++; }
	}
	check(clears == 4 && animations == 4 && w.writes.size() == 4);
	check(w.writes[0][0] == 2080 && w.writes[1][0] == 2144 && w.writes[2][0] == 2081 && w.writes[3][0] == 2145);
	check(w.writes[0][2] == 1 && w.writes[1][2] == 2 && w.writes[2][2] == 3 && w.writes[3][2] == 4);
	check(w.population == 10 && w.houses == 2);
	town_protocol_world = nullptr;
}
#endif /* TOWN_PROTOCOL_HPP */
