/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry.cpp Canonical industry slot/history and native ABI checks. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#ifdef WITH_RUST
#include "../industry.h"
#include "../rust/abi_ffi.h"
static_assert(std::is_trivially_destructible_v<TileArea>);
static_assert(sizeof(TileArea) == sizeof(OpenTTDIndustryLocation));
static_assert(alignof(TileArea) == alignof(OpenTTDIndustryMetadata));
static_assert(offsetof(TileArea, tile) == offsetof(OpenTTDIndustryMetadata, tile));
static_assert(offsetof(TileArea, w) == offsetof(OpenTTDIndustryMetadata, width));
static_assert(offsetof(TileArea, h) == offsetof(OpenTTDIndustryMetadata, height));
static_assert(sizeof(IndustryID) == sizeof(uint16_t) && alignof(IndustryID) == alignof(uint16_t));
static_assert(sizeof(Owner) == sizeof(uint8_t) && alignof(Owner) == alignof(uint8_t));
static_assert(sizeof(Colours) == sizeof(uint8_t));
static_assert(sizeof(TimerGameCalendar::Date) == sizeof(int32_t));

TEST_CASE("Industry - canonical native layouts")
{
	auto layout = [](uint16_t type, std::initializer_list<size_t> fields) {
		uint8_t item = 0;
		for (size_t expected : fields) CHECK(openttd_rust_abi_layout(type, item++) == expected);
		CHECK(openttd_rust_abi_layout(type, item) == SIZE_MAX);
	};
	layout(560, {sizeof(OpenTTDIndustryMetadata), alignof(OpenTTDIndustryMetadata), offsetof(OpenTTDIndustryMetadata, tile), offsetof(OpenTTDIndustryMetadata, width), offsetof(OpenTTDIndustryMetadata, height), offsetof(OpenTTDIndustryMetadata, construction_date), offsetof(OpenTTDIndustryMetadata, random), offsetof(OpenTTDIndustryMetadata, type), offsetof(OpenTTDIndustryMetadata, owner), offsetof(OpenTTDIndustryMetadata, random_colour), offsetof(OpenTTDIndustryMetadata, founder), offsetof(OpenTTDIndustryMetadata, construction_type), offsetof(OpenTTDIndustryMetadata, selected_layout), offsetof(OpenTTDIndustryMetadata, exclusive_supplier), offsetof(OpenTTDIndustryMetadata, exclusive_consumer)});
	layout(561, {sizeof(OpenTTDIndustryCleanupTile), alignof(OpenTTDIndustryCleanupTile), offsetof(OpenTTDIndustryCleanupTile, industry), offsetof(OpenTTDIndustryCleanupTile, type), offsetof(OpenTTDIndustryCleanupTile, oilrig)});
	layout(562, {sizeof(OpenTTDIndustryReseed), alignof(OpenTTDIndustryReseed), offsetof(OpenTTDIndustryReseed, self), offsetof(OpenTTDIndustryReseed, parent), offsetof(OpenTTDIndustryReseed, used)});
	layout(563, {sizeof(OpenTTDIndustryRandomisationSpec), alignof(OpenTTDIndustryRandomisationSpec), offsetof(OpenTTDIndustryRandomisationSpec, gfx), offsetof(OpenTTDIndustryRandomisationSpec, enabled)});
	layout(564, {sizeof(OpenTTDIndustryAnimationSpec), alignof(OpenTTDIndustryAnimationSpec), offsetof(OpenTTDIndustryAnimationSpec, spec), offsetof(OpenTTDIndustryAnimationSpec, triggers)});
	layout(565, {sizeof(OpenTTDIndustryLifecycleServices), alignof(OpenTTDIndustryLifecycleServices), offsetof(OpenTTDIndustryLifecycleServices, neutral_station), offsetof(OpenTTDIndustryLifecycleServices, cleanup_tile), offsetof(OpenTTDIndustryLifecycleServices, delete_tile_inspect), offsetof(OpenTTDIndustryLifecycleServices, make_water), offsetof(OpenTTDIndustryLifecycleServices, delete_oilrig), offsetof(OpenTTDIndustryLifecycleServices, clear_docking), offsetof(OpenTTDIndustryLifecycleServices, industry_field), offsetof(OpenTTDIndustryLifecycleServices, disassociate_field), offsetof(OpenTTDIndustryLifecycleServices, release_disasters), offsetof(OpenTTDIndustryLifecycleServices, delete_storage), offsetof(OpenTTDIndustryLifecycleServices, delete_news), offsetof(OpenTTDIndustryLifecycleServices, close_view), offsetof(OpenTTDIndustryLifecycleServices, close_production), offsetof(OpenTTDIndustryLifecycleServices, delete_inspect), offsetof(OpenTTDIndustryLifecycleServices, delete_subsidies), offsetof(OpenTTDIndustryLifecycleServices, invalidate_cargo), offsetof(OpenTTDIndustryLifecycleServices, next_near_station), offsetof(OpenTTDIndustryLifecycleServices, remove_delivery), offsetof(OpenTTDIndustryLifecycleServices, belongs), offsetof(OpenTTDIndustryLifecycleServices, randomisation_spec), offsetof(OpenTTDIndustryLifecycleServices, random_triggers), offsetof(OpenTTDIndustryLifecycleServices, set_random_triggers), offsetof(OpenTTDIndustryLifecycleServices, resolve_reseed), offsetof(OpenTTDIndustryLifecycleServices, random_bits), offsetof(OpenTTDIndustryLifecycleServices, set_random_bits), offsetof(OpenTTDIndustryLifecycleServices, mark_dirty), offsetof(OpenTTDIndustryLifecycleServices, animation_spec), offsetof(OpenTTDIndustryLifecycleServices, change_frame)});
	layout(130, {sizeof(OpenTTDIndustryFields), alignof(OpenTTDIndustryFields), offsetof(OpenTTDIndustryFields, valid_history), offsetof(OpenTTDIndustryFields, last_prod_year), offsetof(OpenTTDIndustryFields, counter), offsetof(OpenTTDIndustryFields, prod_level), offsetof(OpenTTDIndustryFields, was_cargo_delivered), offsetof(OpenTTDIndustryFields, ctlflags)});
	layout(131, {sizeof(OpenTTDIndustryBuildFields), alignof(OpenTTDIndustryBuildFields), offsetof(OpenTTDIndustryBuildFields, probability), offsetof(OpenTTDIndustryBuildFields, min_number), offsetof(OpenTTDIndustryBuildFields, target_count), offsetof(OpenTTDIndustryBuildFields, max_wait), offsetof(OpenTTDIndustryBuildFields, wait_count)});
	layout(132, {sizeof(OpenTTDIndustryBuilderFields), alignof(OpenTTDIndustryBuilderFields), offsetof(OpenTTDIndustryBuilderFields, builddata), offsetof(OpenTTDIndustryBuilderFields, wanted_inds), offsetof(OpenTTDIndustryBuilderFields, daily_counter), offsetof(OpenTTDIndustryBuilderFields, daily_increment), offsetof(OpenTTDIndustryBuilderFields, sound_tile), offsetof(OpenTTDIndustryBuilderFields, sound_ctr)});
	layout(133, {sizeof(OpenTTDIndustrySlots), alignof(OpenTTDIndustrySlots), offsetof(OpenTTDIndustrySlots, data), offsetof(OpenTTDIndustrySlots, size)});
	layout(134, {sizeof(Industry::ProducedCargo), alignof(Industry::ProducedCargo), offsetof(Industry::ProducedCargo, cargo), offsetof(Industry::ProducedCargo, waiting), offsetof(Industry::ProducedCargo, rate), offsetof(Industry::ProducedCargo, history)});
	layout(135, {sizeof(Industry::AcceptedCargo), alignof(Industry::AcceptedCargo), offsetof(Industry::AcceptedCargo, cargo), offsetof(Industry::AcceptedCargo, waiting), offsetof(Industry::AcceptedCargo, accumulated_waiting), offsetof(Industry::AcceptedCargo, last_accepted), offsetof(Industry::AcceptedCargo, history)});
	layout(136, {sizeof(OpenTTDIndustryObservation), alignof(OpenTTDIndustryObservation), offsetof(OpenTTDIndustryObservation, owner), offsetof(OpenTTDIndustryObservation, tile), offsetof(OpenTTDIndustryObservation, behaviour), offsetof(OpenTTDIndustryObservation, id), offsetof(OpenTTDIndustryObservation, width), offsetof(OpenTTDIndustryObservation, height), offsetof(OpenTTDIndustryObservation, callbacks), offsetof(OpenTTDIndustryObservation, sound_count), offsetof(OpenTTDIndustryObservation, life), offsetof(OpenTTDIndustryObservation, original), offsetof(OpenTTDIndustryObservation, minimal_cargo), offsetof(OpenTTDIndustryObservation, type), offsetof(OpenTTDIndustryObservation, up_text), offsetof(OpenTTDIndustryObservation, down_text), offsetof(OpenTTDIndustryObservation, closure_text)});
	layout(137, {sizeof(OpenTTDIndustryServices), alignof(OpenTTDIndustryServices), offsetof(OpenTTDIndustryServices, next_tick), offsetof(OpenTTDIndustryServices, sound_count), offsetof(OpenTTDIndustryServices, behaviour), offsetof(OpenTTDIndustryServices, location), offsetof(OpenTTDIndustryServices, industry_sound), offsetof(OpenTTDIndustryServices, play_sound), offsetof(OpenTTDIndustryServices, special_effect), offsetof(OpenTTDIndustryServices, tick_trigger), offsetof(OpenTTDIndustryServices, production_callback), offsetof(OpenTTDIndustryServices, scale_cargo), offsetof(OpenTTDIndustryServices, random), offsetof(OpenTTDIndustryServices, random_range), offsetof(OpenTTDIndustryServices, move_goods), offsetof(OpenTTDIndustryServices, tile_add_wrap), offsetof(OpenTTDIndustryServices, farm_tile), offsetof(OpenTTDIndustryServices, tile_z), offsetof(OpenTTDIndustryServices, snow_line), offsetof(OpenTTDIndustryServices, make_field), offsetof(OpenTTDIndustryServices, fence_wanted), offsetof(OpenTTDIndustryServices, set_fence), offsetof(OpenTTDIndustryServices, tile_completed), offsetof(OpenTTDIndustryServices, harvest), offsetof(OpenTTDIndustryServices, observe), offsetof(OpenTTDIndustryServices, production_rate), offsetof(OpenTTDIndustryServices, change_callback), offsetof(OpenTTDIndustryServices, custom_text), offsetof(OpenTTDIndustryServices, news), offsetof(OpenTTDIndustryServices, rate_news), offsetof(OpenTTDIndustryServices, callback_error), offsetof(OpenTTDIndustryServices, set_dirty), offsetof(OpenTTDIndustryServices, destroy), offsetof(OpenTTDIndustryServices, advertise), offsetof(OpenTTDIndustryServices, create), offsetof(OpenTTDIndustryServices, random_industry), offsetof(OpenTTDIndustryServices, get), offsetof(OpenTTDIndustryServices, type_count), offsetof(OpenTTDIndustryServices, total), offsetof(OpenTTDIndustryServices, type_info), offsetof(OpenTTDIndustryServices, probability_callback), offsetof(OpenTTDIndustryServices, scale_by_map_size), offsetof(OpenTTDIndustryServices, company_none), offsetof(OpenTTDIndustryServices, restore_company), offsetof(OpenTTDIndustryServices, directory_dirty), offsetof(OpenTTDIndustryServices, recession), offsetof(OpenTTDIndustryServices, economy_month), offsetof(OpenTTDIndustryServices, economy_year), offsetof(OpenTTDIndustryServices, days_since_last_month), offsetof(OpenTTDIndustryServices, landscape), offsetof(OpenTTDIndustryServices, economy_type), offsetof(OpenTTDIndustryServices, passengers), offsetof(OpenTTDIndustryServices, fund_only), offsetof(OpenTTDIndustryServices, calendar_year), offsetof(OpenTTDIndustryServices, deity)});
	layout(138, {sizeof(OpenTTDIndustryProductionResult), alignof(OpenTTDIndustryProductionResult), offsetof(OpenTTDIndustryProductionResult, subtract), offsetof(OpenTTDIndustryProductionResult, add), offsetof(OpenTTDIndustryProductionResult, again), offsetof(OpenTTDIndustryProductionResult, cargo_input), offsetof(OpenTTDIndustryProductionResult, cargo_output), offsetof(OpenTTDIndustryProductionResult, version), offsetof(OpenTTDIndustryProductionResult, num_input), offsetof(OpenTTDIndustryProductionResult, num_output), offsetof(OpenTTDIndustryProductionResult, present)});
	layout(139, {sizeof(OpenTTDIndustryMap), alignof(OpenTTDIndustryMap), offsetof(OpenTTDIndustryMap, size_x), offsetof(OpenTTDIndustryMap, size_y), offsetof(OpenTTDIndustryMap, landscape)});
	layout(140, {sizeof(OpenTTDIndustryTickRecord), alignof(OpenTTDIndustryTickRecord), offsetof(OpenTTDIndustryTickRecord, counter), offsetof(OpenTTDIndustryTickRecord, map), offsetof(OpenTTDIndustryTickRecord, interval), offsetof(OpenTTDIndustryTickRecord, ambient), offsetof(OpenTTDIndustryTickRecord, editor)});
	layout(141, {sizeof(OpenTTDIndustryEntry), alignof(OpenTTDIndustryEntry), offsetof(OpenTTDIndustryEntry, industry), offsetof(OpenTTDIndustryEntry, owner), offsetof(OpenTTDIndustryEntry, id), offsetof(OpenTTDIndustryEntry, callbacks)});
	layout(142, {sizeof(OpenTTDIndustryLocation), alignof(OpenTTDIndustryLocation), offsetof(OpenTTDIndustryLocation, tile), offsetof(OpenTTDIndustryLocation, width), offsetof(OpenTTDIndustryLocation, height)});
	layout(143, {sizeof(OpenTTDIndustryFarmTile), alignof(OpenTTDIndustryFarmTile), offsetof(OpenTTDIndustryFarmTile, type), offsetof(OpenTTDIndustryFarmTile, snow), offsetof(OpenTTDIndustryFarmTile, ground), offsetof(OpenTTDIndustryFarmTile, grown)});
	layout(144, {sizeof(OpenTTDIndustryChange), alignof(OpenTTDIndustryChange), offsetof(OpenTTDIndustryChange, result), offsetof(OpenTTDIndustryChange, reg)});
	layout(145, {sizeof(OpenTTDIndustryTypeInfo), alignof(OpenTTDIndustryTypeInfo), offsetof(OpenTTDIndustryTypeInfo, behaviour), offsetof(OpenTTDIndustryTypeInfo, enabled), offsetof(OpenTTDIndustryTypeInfo, layouts), offsetof(OpenTTDIndustryTypeInfo, appear)});
	REQUIRE(NUM_INDUSTRYTYPES == 240);
	REQUIRE(std::size(OpenTTDIndustryBuilderFields{}.builddata) == NUM_INDUSTRYTYPES);
	REQUIRE(HISTORY_RECORDS == 61);
	REQUIRE(sizeof(Industry::ProducedHistory) == 4);
	REQUIRE(sizeof(Industry::AcceptedHistory) == 4);
	REQUIRE(sizeof(TimerGameEconomy::Year) == sizeof(int32_t));
	REQUIRE(sizeof(TimerGameEconomy::Date) == sizeof(int32_t));
	REQUIRE(sizeof(IndustryControlFlags) == sizeof(uint8_t));
	REQUIRE(sizeof(IndustryTypeBuildData) == sizeof(OpenTTDIndustryBuildFields));
}
TEST_CASE("Industry - canonical allocation and history lifetime")
{
	RustIndustryOwner owner(openttd_rust_industry_new(), openttd_rust_industry_destroy);
	RustIndustryVector<Industry::AcceptedCargo, false> accepted(owner.get());
	RustIndustryVector<Industry::ProducedCargo, true> produced(owner.get());
	accepted.emplace_back().waiting = 65535;
	auto &history = accepted[0].GetOrCreateHistory();
	history[THIS_MONTH].accepted = 65535;
	auto *allocation = accepted[0].history.get();
	accepted.reserve(256);
	CHECK(accepted[0].history.get() == allocation);
	CHECK(accepted[0].waiting == 65535);
	CHECK(accepted[0].GetOrCreateHistory()[THIS_MONTH].accepted == 65535);
	accepted.emplace_back().cargo = INVALID_CARGO;
	produced.emplace_back().cargo = 3;
	produced.emplace_back().cargo = INVALID_CARGO;
	openttd_rust_industry_trim(owner.get());
	CHECK(accepted.size() == 1);
	CHECK(produced.size() == 1);
	CHECK(accepted[0].history.get() == allocation);
	accepted.clear();
	CHECK(accepted.empty());
	CHECK(accepted.emplace_back().history == nullptr);
}
TEST_CASE("Industry - construction owner and ascending type registry")
{
	RustIndustryOwner owner{openttd_rust_industry_new(), openttd_rust_industry_destroy};
	auto *metadata = openttd_rust_industry_metadata(owner.get());
	CHECK(metadata->tile == INVALID_TILE.base());
	CHECK(metadata->width == 0);
	CHECK(metadata->owner == INVALID_OWNER);
	CHECK(metadata->founder == INVALID_OWNER);
	CHECK(metadata->exclusive_supplier == INVALID_OWNER);
	CHECK(metadata->exclusive_consumer == INVALID_OWNER);
	std::construct_at(reinterpret_cast<TileArea *>(&metadata->tile), TileIndex(456), 7, 9);
	CHECK(metadata->tile == 456);
	CHECK(metadata->width == 7);
	CHECK(metadata->height == 9);
	const uint8_t text[] = {0, 0xFF, 0x1E, 'a'};
	openttd_rust_industry_set_text(owner.get(), text, sizeof(text));
	uint8_t output[sizeof(text)]{};
	CHECK(openttd_rust_industry_text(owner.get(), output, sizeof(output)) == sizeof(text));
	CHECK(std::equal(std::begin(text), std::end(text), std::begin(output)));
	openttd_rust_industry_set_text(owner.get(), nullptr, 0);
	CHECK(openttd_rust_industry_text(owner.get(), nullptr, 0) == 0);
	RustIndustryBuilderOwner builder{openttd_rust_industry_builder_new(), openttd_rust_industry_builder_destroy};
	for (uint16_t id : {9, 1, 9, 5}) openttd_rust_industry_registry_insert(builder.get(), 3, id);
	const uint16_t expected[] = {1, 5, 9};
	auto view = openttd_rust_industry_registry_view(builder.get(), 3);
	CHECK(view.size == 3);
	CHECK(std::equal(std::begin(expected), std::end(expected), static_cast<uint16_t *>(view.data)));
	openttd_rust_industry_registry_erase(builder.get(), 3, 5);
	openttd_rust_industry_registry_erase(builder.get(), 3, 99);
	CHECK(openttd_rust_industry_registry_view(builder.get(), 3).size == 2);
	CHECK(openttd_rust_industry_registry_view(builder.get(), 4).size == 0);
	openttd_rust_industry_registry_insert(builder.get(), 3, 5);
	CHECK(openttd_rust_industry_registry_view(builder.get(), 3).size == 3);
	openttd_rust_industry_registry_clear(builder.get(), 3);
	CHECK(openttd_rust_industry_registry_view(builder.get(), 3).size == 0);
}
#endif
