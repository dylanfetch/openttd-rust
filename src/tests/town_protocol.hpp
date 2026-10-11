/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_protocol.hpp Narrow expansion-mode and callback-reentry gaps absent from console saves. */
#ifndef TOWN_PROTOCOL_HPP
#define TOWN_PROTOCOL_HPP
#include "shared_services.hpp"
#include "../rust/town_ffi.h"
#include <algorithm>
#include <array>
#include <memory>
#include <vector>

struct TownProtocolWorld {
	OpenTTDTownState *owner;
	uint32_t houses = 0, population = 0, draws = 0, company = 7;
	bool house_callback = false, clear_ok = true;
	uint32_t clears = 0, animations = 0, roads = 0;
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
		case 13: w.houses += a; openttd_rust_town_simulation(w.owner)->num_houses += a; break;
		case 14: w.population += a; openttd_rust_town_simulation(w.owner)->population += a; break;
		case 17: return 1;
		case 20: return UINT32_MAX;
	}
	return 0;
}
static inline OpenTTDTownState *TownProtocolState(uint32_t) noexcept { return town_protocol_world->owner; }
static inline void TownProtocolStations(uint32_t, uint32_t, void *, void (*)(void *, const void *, uint32_t)) noexcept {}
static inline OpenTTDTownStationActivity TownProtocolStationActivity(const void *) noexcept { return {}; }
static inline uint8_t TownProtocolStationOwner(const void *) noexcept { return UINT8_MAX; }
static inline uint32_t TownProtocolRandom(void *) noexcept { town_protocol_world->draws++; return 0xABCDEF01; }
static inline void TownProtocolTile(void *, uint32_t, uint32_t *) noexcept {}
static inline void TownProtocolWrite(void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) noexcept {}
static inline float TownProtocolTrig(uint32_t, float x) noexcept { return x; }
static inline uint32_t TownProtocolIndustry(int32_t, int32_t, uint32_t *) noexcept { return 0; }

static inline void TownProtocol_settings(uint32_t *v) noexcept { TownProtocolObserve(0, 0, v); }
static inline void TownProtocol_tile(uint32_t id, uint32_t *v) noexcept { TownProtocolObserve(2, id, v); }
static inline void TownProtocol_house(uint32_t id, uint32_t *v) noexcept { TownProtocolObserve(3, id, v); }
static inline void TownProtocol_road_info(uint32_t id, uint32_t *v) noexcept { TownProtocolObserve(4, id, v); }
static inline void TownProtocol_dirty_town(uint32_t town) noexcept { TownProtocolLeaf(1, town, 0, 0, 0); }
static inline uint32_t TownProtocol_tropic_zone(uint32_t tile) noexcept { return TownProtocolLeaf(2, tile, 0, 0, 0); }
static inline uint32_t TownProtocol_tile_height(uint32_t tile) noexcept { return TownProtocolLeaf(3, tile, 0, 0, 0); }
static inline uint32_t TownProtocol_foundation_slope(uint32_t tile) noexcept { return TownProtocolLeaf(4, tile, 0, 0, 0); }
static inline int64_t TownProtocol_terraform_price() noexcept { return TownProtocolLeaf(5, 0, 0, 0, 0); }
static inline uint32_t TownProtocol_clean_road_bits(uint32_t tile, uint32_t bits) noexcept { return TownProtocolLeaf(6, tile, bits, 0, 0); }
static inline void TownProtocol_set_road_town(uint32_t tile, uint32_t town) noexcept { TownProtocolLeaf(7, tile, town, 0, 0); }
static inline uint32_t TownProtocol_exchange_company(uint32_t company) noexcept { return TownProtocolLeaf(8, 0, company, 0, 0); }
static inline void TownProtocol_increase_building_count(uint32_t town, uint32_t house) noexcept { TownProtocolLeaf(9, town, house, 0, 0); }
static inline void TownProtocol_make_house(uint32_t tile, uint32_t town, uint32_t house, uint32_t construction) noexcept { TownProtocolLeaf(10, tile, town, house, construction); }
static inline void TownProtocol_add_animation(uint32_t tile) noexcept { TownProtocolLeaf(11, tile, 0, 0, 0); }
static inline void TownProtocol_dirty_tile(uint32_t tile) noexcept { TownProtocolLeaf(12, tile, 0, 0, 0); }
static inline void TownProtocol_change_house_count(uint32_t town, uint32_t amount) noexcept { TownProtocolLeaf(13, town, amount, 0, 0); }
static inline void TownProtocol_change_population(uint32_t town, uint32_t amount) noexcept { TownProtocolLeaf(14, town, amount, 0, 0); }
static inline void TownProtocol_nearby_stations(uint32_t town, uint32_t tile, uint32_t size) noexcept { TownProtocolLeaf(15, town, tile, size, 0); }
static inline void TownProtocol_update_radius(uint32_t town) noexcept { TownProtocolLeaf(16, town, 0, 0, 0); }
static inline uint32_t TownProtocol_house_count() noexcept { return TownProtocolLeaf(17, 0, 0, 0, 0); }
static inline uint16_t TownProtocol_building_count(uint32_t town, uint32_t house, uint32_t house_class) noexcept { return TownProtocolLeaf(18, town, house, house_class, 0); }
static inline void TownProtocol_update_max_pass(uint32_t town) noexcept { TownProtocolLeaf(19, town, 0, 0, 0); }
static inline uint32_t TownProtocol_next_town(uint32_t from) noexcept { return TownProtocolLeaf(20, 0, from, 0, 0); }
static inline uint64_t TownProtocol_road_mask() noexcept { return TownProtocolLeaf(21, 0, 0, 0, 0); }
static inline bool TownProtocol_build_road(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t mode) noexcept
{
	auto &w = *town_protocol_world; w.roads++; return mode == 0;
}
static inline bool TownProtocol_clear_land(uint32_t, uint32_t) noexcept { auto &w = *town_protocol_world; w.clears++; return w.clear_ok; }
static inline OpenTTDTownCommandResult TownProtocol_terraform_land(uint32_t, uint32_t, uint32_t, uint32_t) noexcept { return {}; }
static inline bool TownProtocol_build_bridge(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) noexcept { return false; }
static inline bool TownProtocol_build_tunnel(uint32_t, uint32_t, uint32_t) noexcept { return false; }
static inline void TownProtocol_clear_house_land(uint32_t) noexcept { town_protocol_world->clears++; }
static inline bool TownProtocol_allow_construction(uint32_t, uint32_t, uint32_t, uint32_t) noexcept
{
	openttd_rust_town_growth(town_protocol_world->owner)->flags = 0x88; return true;
}
static inline void TownProtocol_construction_animation(uint32_t) noexcept { town_protocol_world->animations++; }
static inline OpenTTDTownLeaves TownProtocolServices()
{
	return {
		.settings = TownProtocol_settings,
		.tile = TownProtocol_tile,
		.house = TownProtocol_house,
		.road_info = TownProtocol_road_info,
		.state = TownProtocolState,
		.stations = TownProtocolStations,
		.station_activity = TownProtocolStationActivity,
		.station_owner = TownProtocolStationOwner,
		.dirty_authority = TownProtocol_dirty_town,
		.dirty_town = TownProtocol_dirty_town,
		.tropic_zone = TownProtocol_tropic_zone,
		.tile_height = TownProtocol_tile_height,
		.foundation_slope = TownProtocol_foundation_slope,
		.terraform_price = TownProtocol_terraform_price,
		.clean_road_bits = TownProtocol_clean_road_bits,
		.set_road_town = TownProtocol_set_road_town,
		.exchange_company = TownProtocol_exchange_company,
		.increase_building_count = TownProtocol_increase_building_count,
		.make_house = TownProtocol_make_house,
		.add_animation = TownProtocol_add_animation,
		.dirty_tile = TownProtocol_dirty_tile,
		.change_house_count = TownProtocol_change_house_count,
		.change_population = TownProtocol_change_population,
		.nearby_stations = TownProtocol_nearby_stations,
		.update_radius = TownProtocol_update_radius,
		.house_count = TownProtocol_house_count,
		.building_count = TownProtocol_building_count,
		.update_max_pass = TownProtocol_update_max_pass,
		.next_town = TownProtocol_next_town,
		.road_mask = TownProtocol_road_mask,
		.build_road = TownProtocol_build_road,
		.clear_land = TownProtocol_clear_land,
		.terraform_land = TownProtocol_terraform_land,
		.build_bridge = TownProtocol_build_bridge,
		.build_tunnel = TownProtocol_build_tunnel,
		.clear_house_land = TownProtocol_clear_house_land,
		.allow_construction = TownProtocol_allow_construction,
		.construction_animation = TownProtocol_construction_animation,
	};
}
template <typename Check> void CheckTownProtocol(Check check)
{
	/* Source quirks: first-road execute failure still returns success; a failed
	 * expansion tries 25 times; reentry mutates the canonical owner's flags. */
	auto owner = std::unique_ptr<OpenTTDTownState, decltype(&openttd_rust_town_destroy)>(openttd_rust_town_new(), openttd_rust_town_destroy);
	TownProtocolWorld w{}; w.owner = owner.get(); town_protocol_world = &w;
	auto *fields = openttd_rust_town_simulation(owner.get()); fields->xy = 2080; std::fill_n(fields->radius, 5, 100);
	const auto leaves = TownProtocolServices();
	const OpenTTDSharedServices services{.context = nullptr, .random = TownProtocolRandom, .observe_tile = TownProtocolTile, .write_tile = TownProtocolWrite, .trig = TownProtocolTrig, .industry = TownProtocolIndustry, .company_valid = UnusedCompanyValid, .next_company = UnusedNextCompany};
	check(!openttd_rust_town_grow(&leaves, &services, 0, 1) && w.draws == 0 && w.company == 7);
	check(openttd_rust_town_grow(&leaves, &services, 0, 2) && w.roads == 1 && w.draws == 1 && w.company == 7);
	w.clears = 0; w.clear_ok = false;
	openttd_rust_town_expand(&leaves, &services, 0, 1, 2);
	check(w.clears == 25 * 13 && w.company == 7 && w.draws == 1);
	w.clear_ok = true; w.house_callback = true;
	check(openttd_rust_town_try_house(&leaves, &services, 0, 2080, 1));
	check(w.houses == 1 && w.writes.size() == 1 && w.animations == 1 && openttd_rust_town_growth(owner.get())->flags == 0x88);
	w.writes.clear(); w.house_callback = false; w.clears = 0; w.animations = 0;
	openttd_rust_town_build_house(&leaves, &services, 0, 2080, 1, 43, true, true);
	check(w.clears == 4 && w.animations == 4 && w.writes.size() == 4);
	check(w.writes[0][0] == 2080 && w.writes[1][0] == 2144 && w.writes[2][0] == 2081 && w.writes[3][0] == 2145);
	check(w.writes[0][2] == 1 && w.writes[1][2] == 2 && w.writes[2][2] == 3 && w.writes[3][2] == 4);
	check(w.population == 10 && w.houses == 2);
	town_protocol_world = nullptr;
}
#endif /* TOWN_PROTOCOL_HPP */
