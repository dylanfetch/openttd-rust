/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file engine.cpp Native engine owner lifetime and ABI evidence. */
#include "../stdafx.h"
#include "../engine_base.h"
#ifdef WITH_RUST
#include "../rust/engine_ffi.h"
#include "../rust/abi_ffi.h"
#endif
#include "../3rdparty/catch2/catch.hpp"
#include "../safeguards.h"

TEST_CASE("Engine lifecycle canonical ABI", "[rust][engine][abi]")
{
#ifdef WITH_RUST
	auto layout = [](uint16_t type, std::initializer_list<size_t> values) {
		uint8_t field = 0;
		for (size_t value : values) { REQUIRE(openttd_rust_abi_layout(type, field) == value); ++field; }
	};
	layout(860, {sizeof(OpenTTDEngineLifecycle), alignof(OpenTTDEngineLifecycle), offsetof(OpenTTDEngineLifecycle, company_avail), offsetof(OpenTTDEngineLifecycle, company_hidden), offsetof(OpenTTDEngineLifecycle, preview_asked), offsetof(OpenTTDEngineLifecycle, intro_date), offsetof(OpenTTDEngineLifecycle, age), offsetof(OpenTTDEngineLifecycle, reliability), offsetof(OpenTTDEngineLifecycle, reliability_spd_dec), offsetof(OpenTTDEngineLifecycle, reliability_start), offsetof(OpenTTDEngineLifecycle, reliability_max), offsetof(OpenTTDEngineLifecycle, reliability_final), offsetof(OpenTTDEngineLifecycle, duration_phase_1), offsetof(OpenTTDEngineLifecycle, duration_phase_2), offsetof(OpenTTDEngineLifecycle, duration_phase_3), offsetof(OpenTTDEngineLifecycle, flags), offsetof(OpenTTDEngineLifecycle, preview_company), offsetof(OpenTTDEngineLifecycle, preview_wait)});
	layout(861, {sizeof(OpenTTDEngineRef), alignof(OpenTTDEngineRef), offsetof(OpenTTDEngineRef, shell), offsetof(OpenTTDEngineRef, state), offsetof(OpenTTDEngineRef, id), offsetof(OpenTTDEngineRef, type)});
	layout(862, {sizeof(OpenTTDEngineSyncVariant), alignof(OpenTTDEngineSyncVariant), offsetof(OpenTTDEngineSyncVariant, parent), offsetof(OpenTTDEngineSyncVariant, sync)});
	layout(863, {sizeof(OpenTTDEngineDisplayVariant), alignof(OpenTTDEngineDisplayVariant), offsetof(OpenTTDEngineDisplayVariant, parent), offsetof(OpenTTDEngineDisplayVariant, display)});
	layout(864, {sizeof(OpenTTDEnginePreviewVariant), alignof(OpenTTDEnginePreviewVariant), offsetof(OpenTTDEnginePreviewVariant, parent), offsetof(OpenTTDEnginePreviewVariant, join)});
	layout(865, {sizeof(OpenTTDEngineReliability), alignof(OpenTTDEngineReliability), offsetof(OpenTTDEngineReliability, base_life), offsetof(OpenTTDEngineReliability, retire_early)});
	layout(866, {sizeof(OpenTTDEngineStartup), alignof(OpenTTDEngineStartup), offsetof(OpenTTDEngineStartup, base_intro), offsetof(OpenTTDEngineStartup, grfid), offsetof(OpenTTDEngineStartup, decay_speed), offsetof(OpenTTDEngineStartup, base_life), offsetof(OpenTTDEngineStartup, climate)});
	layout(867, {sizeof(OpenTTDEngineSeed), alignof(OpenTTDEngineSeed), offsetof(OpenTTDEngineSeed, base_intro), offsetof(OpenTTDEngineSeed, lifelength), offsetof(OpenTTDEngineSeed, decay_speed), offsetof(OpenTTDEngineSeed, retire_early)});
	layout(868, {sizeof(OpenTTDEngineYear), alignof(OpenTTDEngineYear), offsetof(OpenTTDEngineYear, base_intro), offsetof(OpenTTDEngineYear, lifelength)});
	layout(869, {sizeof(OpenTTDEngineYMD), alignof(OpenTTDEngineYMD), offsetof(OpenTTDEngineYMD, year), offsetof(OpenTTDEngineYMD, month), offsetof(OpenTTDEngineYMD, day)});
	layout(870, {sizeof(OpenTTDEngineStartSettings), alignof(OpenTTDEngineStartSettings), offsetof(OpenTTDEngineStartSettings, generation_seed), offsetof(OpenTTDEngineStartSettings, starting_year), offsetof(OpenTTDEngineStartSettings, date)});
	layout(871, {sizeof(OpenTTDEngineCompany), alignof(OpenTTDEngineCompany), offsetof(OpenTTDEngineCompany, shell), offsetof(OpenTTDEngineCompany, id)});
	layout(872, {sizeof(OpenTTDEngineCompanyPreview), alignof(OpenTTDEngineCompanyPreview), offsetof(OpenTTDEngineCompanyPreview, blocked), offsetof(OpenTTDEngineCompanyPreview, performance)});
	layout(873, {sizeof(OpenTTDEngineVehicle), alignof(OpenTTDEngineVehicle), offsetof(OpenTTDEngineVehicle, shell), offsetof(OpenTTDEngineVehicle, id), offsetof(OpenTTDEngineVehicle, owner), offsetof(OpenTTDEngineVehicle, type)});
	layout(874, {sizeof(OpenTTDEngineVehicleCargo), alignof(OpenTTDEngineVehicleCargo), offsetof(OpenTTDEngineVehicleCargo, engine), offsetof(OpenTTDEngineVehicleCargo, cargo)});
	layout(875, {sizeof(OpenTTDRandomSeeds), alignof(OpenTTDRandomSeeds), offsetof(OpenTTDRandomSeeds, random), offsetof(OpenTTDRandomSeeds, interactive)});
	layout(876, {sizeof(OpenTTDRandomServices), alignof(OpenTTDRandomServices), offsetof(OpenTTDRandomServices, save), offsetof(OpenTTDRandomServices, restore), offsetof(OpenTTDRandomServices, set_seed)});
	layout(877, {sizeof(OpenTTDEngineServices), alignof(OpenTTDEngineServices), offsetof(OpenTTDEngineServices, shared), offsetof(OpenTTDEngineServices, rng), offsetof(OpenTTDEngineServices, next), offsetof(OpenTTDEngineServices, resolve), offsetof(OpenTTDEngineServices, sync_variant), offsetof(OpenTTDEngineServices, display_variant), offsetof(OpenTTDEngineServices, preview_variant), offsetof(OpenTTDEngineServices, extra_flags), offsetof(OpenTTDEngineServices, engine_id), offsetof(OpenTTDEngineServices, reliability), offsetof(OpenTTDEngineServices, startup), offsetof(OpenTTDEngineServices, seed_config), offsetof(OpenTTDEngineServices, base_intro), offsetof(OpenTTDEngineServices, aging_year), offsetof(OpenTTDEngineServices, calendar_year), offsetof(OpenTTDEngineServices, never_expire), offsetof(OpenTTDEngineServices, start_settings), offsetof(OpenTTDEngineServices, train_limit), offsetof(OpenTTDEngineServices, road_limit), offsetof(OpenTTDEngineServices, ship_limit), offsetof(OpenTTDEngineServices, aircraft_limit), offsetof(OpenTTDEngineServices, ai_train_disabled), offsetof(OpenTTDEngineServices, ai_road_disabled), offsetof(OpenTTDEngineServices, ai_ship_disabled), offsetof(OpenTTDEngineServices, ai_aircraft_disabled), offsetof(OpenTTDEngineServices, date_to_ymd), offsetof(OpenTTDEngineServices, ymd_to_date), offsetof(OpenTTDEngineServices, enabled), offsetof(OpenTTDEngineServices, climate), offsetof(OpenTTDEngineServices, rail_kind), offsetof(OpenTTDEngineServices, clear_last_variant), offsetof(OpenTTDEngineServices, refresh_type), offsetof(OpenTTDEngineServices, refresh_lists), offsetof(OpenTTDEngineServices, company_next), offsetof(OpenTTDEngineServices, company_preview), offsetof(OpenTTDEngineServices, vehicle_next), offsetof(OpenTTDEngineServices, vehicle_cargo), offsetof(OpenTTDEngineServices, can_carry), offsetof(OpenTTDEngineServices, refit_mask), offsetof(OpenTTDEngineServices, update_railtypes), offsetof(OpenTTDEngineServices, update_roadtypes), offsetof(OpenTTDEngineServices, date_company_types), offsetof(OpenTTDEngineServices, company_valid), offsetof(OpenTTDEngineServices, compatible_railtypes), offsetof(OpenTTDEngineServices, powered_roadtypes), offsetof(OpenTTDEngineServices, company_railtypes), offsetof(OpenTTDEngineServices, company_roadtypes), offsetof(OpenTTDEngineServices, company_engine_count), offsetof(OpenTTDEngineServices, block_preview), offsetof(OpenTTDEngineServices, introduces_railtypes), offsetof(OpenTTDEngineServices, introduces_roadtypes), offsetof(OpenTTDEngineServices, set_railtypes), offsetof(OpenTTDEngineServices, set_roadtypes), offsetof(OpenTTDEngineServices, current_company), offsetof(OpenTTDEngineServices, local_company), offsetof(OpenTTDEngineServices, toolbar_enable), offsetof(OpenTTDEngineServices, toolbar_release), offsetof(OpenTTDEngineServices, close_preview), offsetof(OpenTTDEngineServices, invalidate_preview), offsetof(OpenTTDEngineServices, offer_preview), offsetof(OpenTTDEngineServices, available_event), offsetof(OpenTTDEngineServices, available_news), offsetof(OpenTTDEngineServices, no_engines), offsetof(OpenTTDEngineServices, screen_dirty), offsetof(OpenTTDEngineServices, utf8_length)});
#endif
}

TEST_CASE("Engine temporary owners survive relocation and replacement", "[rust][engine][lifetime]")
{
#ifdef WITH_RUST
	std::vector<Engine> staged;
	staged.emplace_back();
	staged[0].age = -123;
	staged[0].preview_company = CompanyID{3};
	staged[0].name = std::string_view("name\0bytes", 10);
	auto *allocation = staged[0].rust_lifecycle.state;
	auto *age = &staged[0].age;
	for (size_t i = 1; i < 1024; ++i) staged.emplace_back();
	REQUIRE(staged[0].rust_lifecycle.state == allocation);
	REQUIRE(&staged[0].age == age);
	REQUIRE(staged[0].age == -123);
	REQUIRE(staged[0].preview_company == CompanyID{3});
	REQUIRE(staged[0].name.View() == std::string_view("name\0bytes", 10));
	/* Placement replacement is used by FixTTOEngines after native staging. */
	staged[0].~Engine();
	::new (&staged[0]) Engine();
	REQUIRE(staged[0].age == 0);
	REQUIRE(staged[0].preview_company == CompanyID::Invalid());
	REQUIRE(staged[0].name.empty());
	/* Canonical aliases immediately see partial decoded writes. */
	staged[0].rust_lifecycle.state->age = INT32_MIN;
	REQUIRE(staged[0].age == INT32_MIN);
	staged.clear();
#endif
}
