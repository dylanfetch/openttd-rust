/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_ffi.h Canonical town state and synchronous typed services. */
#ifndef RUST_TOWN_FFI_H
#define RUST_TOWN_FFI_H
#include "services_ffi.h"
#include <cstdint>
#include <cstddef>
struct OpenTTDTownState;
struct OpenTTDTownReceived { uint16_t old_max, new_max, old_act, new_act; };
struct OpenTTDTownSimulationFields {
	uint64_t valid_history;
	uint32_t xy, num_houses, population, radius[5], goal[6];
	int16_t ratings[15];
	OpenTTDTownReceived received[6];
	uint16_t noise_reached, time_until_rebuild, statues, have_ratings;
	uint8_t unwanted[15], exclusivity, exclusive_counter;
	bool larger_town;
	uint8_t layout;
};
struct OpenTTDTownSuppliedHistory { uint32_t production, transported; };
struct OpenTTDTownSupplied { uint8_t cargo; OpenTTDTownSuppliedHistory history[61]; };
struct OpenTTDTownSlots { void *data; size_t size; };
struct OpenTTDTownCommandResult { int64_t cost; bool success; };
struct OpenTTDTownLeaves {
	void (*settings)(uint32_t *) noexcept;
	void (*town)(uint32_t, uint32_t *) noexcept;
	void (*tile)(uint32_t, uint32_t *) noexcept;
	void (*house)(uint32_t, uint32_t *) noexcept;
	void (*road_info)(uint32_t, uint32_t *) noexcept;
	OpenTTDTownState *(*state)(uint32_t) noexcept;
	void (*stations)(uint32_t, uint32_t, void *, void (*)(void *, uint32_t, uint32_t, uint32_t)) noexcept;
	void (*dirty_town)(uint32_t town) noexcept;
	uint32_t (*tropic_zone)(uint32_t tile) noexcept;
	uint32_t (*tile_height)(uint32_t tile) noexcept;
	uint32_t (*foundation_slope)(uint32_t tile) noexcept;
	int64_t (*terraform_price)() noexcept;
	uint32_t (*clean_road_bits)(uint32_t tile, uint32_t bits) noexcept;
	void (*set_road_town)(uint32_t tile, uint32_t town) noexcept;
	uint32_t (*exchange_company)(uint32_t company) noexcept;
	void (*increase_building_count)(uint32_t town, uint32_t house) noexcept;
	void (*make_house)(uint32_t tile, uint32_t town, uint32_t house, uint32_t construction) noexcept;
	void (*add_animation)(uint32_t tile) noexcept;
	void (*dirty_tile)(uint32_t tile) noexcept;
	void (*change_house_count)(uint32_t town, uint32_t amount) noexcept;
	void (*change_population)(uint32_t town, uint32_t amount) noexcept;
	void (*nearby_stations)(uint32_t town, uint32_t tile, uint32_t size) noexcept;
	void (*update_radius)(uint32_t town) noexcept;
	uint32_t (*house_count)() noexcept;
	uint16_t (*building_count)(uint32_t town, uint32_t house, uint32_t house_class) noexcept;
	void (*update_max_pass)(uint32_t town) noexcept;
	uint32_t (*next_town)(uint32_t from) noexcept;
	uint64_t (*road_mask)() noexcept;
	bool (*build_road)(uint32_t town, uint32_t tile, uint32_t bits, uint32_t road_type, uint32_t mode) noexcept;
	bool (*clear_land)(uint32_t tile, uint32_t execute) noexcept;
	OpenTTDTownCommandResult (*terraform_land)(uint32_t tile, uint32_t slope, uint32_t up, uint32_t execute) noexcept;
	bool (*build_bridge)(uint32_t tile, uint32_t end, uint32_t bridge, uint32_t road_type, uint32_t execute) noexcept;
	bool (*build_tunnel)(uint32_t tile, uint32_t road_type, uint32_t execute) noexcept;
	void (*clear_house_land)(uint32_t tile) noexcept;
	bool (*allow_construction)(uint32_t town, uint32_t tile, uint32_t house, uint32_t random) noexcept;
	void (*construction_animation)(uint32_t tile) noexcept;
};
/* Services are synchronous noexcept calls. No owner reference spans one. Commands
 * and bounded NewGRF resolution may reenter; ordinary gameplay does not throw.
 * Environmental failures and Rust panics abort. Save/load staging stays native. */
extern "C" {
OpenTTDTownSimulationFields *openttd_rust_town_simulation(OpenTTDTownState *);
OpenTTDTownSlots openttd_rust_town_supplied_view(OpenTTDTownState *);
void openttd_rust_town_supplied_reserve(OpenTTDTownState *, size_t);
OpenTTDTownSupplied *openttd_rust_town_supplied_insert(OpenTTDTownState *, size_t, uint8_t);
OpenTTDTownSlots openttd_rust_town_id_counts(OpenTTDTownState *);
void openttd_rust_town_id_counts_resize(OpenTTDTownState *, size_t);
OpenTTDTownSlots openttd_rust_town_global_id_counts();
void openttd_rust_town_global_id_counts_resize(size_t);
OpenTTDTownSlots openttd_rust_town_class_counts(OpenTTDTownState *);
void openttd_rust_town_class_counts_resize(OpenTTDTownState *, size_t);
OpenTTDTownSlots openttd_rust_town_global_class_counts();
void openttd_rust_town_global_class_counts_resize(size_t);
void openttd_rust_town_increase_building(OpenTTDTownState *, uint16_t, uint16_t);
void openttd_rust_town_decrease_building(OpenTTDTownState *, uint16_t, uint16_t);
OpenTTDTownState *openttd_rust_town_new();
void openttd_rust_town_destroy(OpenTTDTownState *);
uint16_t openttd_rust_town_get(const OpenTTDTownState *, uint8_t);
void openttd_rust_town_set(OpenTTDTownState *, uint8_t, uint16_t);
bool openttd_rust_town_grow(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, uint32_t town, uint32_t modes);
bool openttd_rust_town_try_house(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, uint32_t town, uint32_t tile, uint32_t modes);
void openttd_rust_town_build_house(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, uint32_t town, uint32_t tile, uint32_t house, uint32_t random, bool complete, bool is_protected);
void openttd_rust_town_tick_all(const OpenTTDTownLeaves *, const OpenTTDSharedServices *);
void openttd_rust_town_update_rate(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, uint32_t town);
void openttd_rust_town_update_growth(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, uint32_t town);
void openttd_rust_town_expand(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, uint32_t town, uint32_t amount, uint32_t modes);
void openttd_rust_town_monthly_counters(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, uint32_t town);
void openttd_rust_town_custom_rate(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, uint32_t town, uint32_t rate);
void openttd_rust_town_fund(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, uint32_t town);
uint32_t openttd_rust_town_road_type(const OpenTTDTownLeaves *, const OpenTTDSharedServices *);
bool openttd_rust_town_tunnel(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, uint32_t town, uint32_t tile, uint32_t dir);
}
#endif /* RUST_TOWN_FFI_H */
