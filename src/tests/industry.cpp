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
TEST_CASE("Industry - canonical native layouts")
{
	auto layout = [](uint8_t type, std::initializer_list<size_t> fields) {
		uint8_t item = 0;
		for (size_t expected : fields) CHECK(openttd_rust_abi_layout(type, item++) == expected);
		CHECK(openttd_rust_abi_layout(type, item) == SIZE_MAX);
	};
	layout(130, {sizeof(OpenTTDIndustryFields), alignof(OpenTTDIndustryFields), offsetof(OpenTTDIndustryFields, valid_history), offsetof(OpenTTDIndustryFields, last_prod_year), offsetof(OpenTTDIndustryFields, counter), offsetof(OpenTTDIndustryFields, prod_level), offsetof(OpenTTDIndustryFields, was_cargo_delivered), offsetof(OpenTTDIndustryFields, ctlflags)});
	layout(131, {sizeof(OpenTTDIndustryBuildFields), alignof(OpenTTDIndustryBuildFields), offsetof(OpenTTDIndustryBuildFields, probability), offsetof(OpenTTDIndustryBuildFields, min_number), offsetof(OpenTTDIndustryBuildFields, target_count), offsetof(OpenTTDIndustryBuildFields, max_wait), offsetof(OpenTTDIndustryBuildFields, wait_count)});
	layout(132, {sizeof(OpenTTDIndustryBuilderFields), alignof(OpenTTDIndustryBuilderFields), offsetof(OpenTTDIndustryBuilderFields, builddata), offsetof(OpenTTDIndustryBuilderFields, wanted_inds), offsetof(OpenTTDIndustryBuilderFields, daily_counter), offsetof(OpenTTDIndustryBuilderFields, daily_increment), offsetof(OpenTTDIndustryBuilderFields, sound_tile), offsetof(OpenTTDIndustryBuilderFields, sound_ctr)});
	layout(133, {sizeof(OpenTTDIndustrySlots), alignof(OpenTTDIndustrySlots), offsetof(OpenTTDIndustrySlots, data), offsetof(OpenTTDIndustrySlots, size)});
	layout(134, {sizeof(Industry::ProducedCargo), alignof(Industry::ProducedCargo), offsetof(Industry::ProducedCargo, cargo), offsetof(Industry::ProducedCargo, waiting), offsetof(Industry::ProducedCargo, rate), offsetof(Industry::ProducedCargo, history)});
	layout(135, {sizeof(Industry::AcceptedCargo), alignof(Industry::AcceptedCargo), offsetof(Industry::AcceptedCargo, cargo), offsetof(Industry::AcceptedCargo, waiting), offsetof(Industry::AcceptedCargo, accumulated_waiting), offsetof(Industry::AcceptedCargo, last_accepted), offsetof(Industry::AcceptedCargo, history)});
	layout(136, {sizeof(OpenTTDIndustryObservation), alignof(OpenTTDIndustryObservation), offsetof(OpenTTDIndustryObservation, owner), offsetof(OpenTTDIndustryObservation, tile), offsetof(OpenTTDIndustryObservation, behaviour), offsetof(OpenTTDIndustryObservation, id), offsetof(OpenTTDIndustryObservation, width), offsetof(OpenTTDIndustryObservation, height), offsetof(OpenTTDIndustryObservation, callbacks), offsetof(OpenTTDIndustryObservation, sound_count), offsetof(OpenTTDIndustryObservation, life), offsetof(OpenTTDIndustryObservation, original), offsetof(OpenTTDIndustryObservation, minimal_cargo), offsetof(OpenTTDIndustryObservation, type), offsetof(OpenTTDIndustryObservation, up_text), offsetof(OpenTTDIndustryObservation, down_text), offsetof(OpenTTDIndustryObservation, closure_text)});
	layout(137, {sizeof(OpenTTDIndustryServices), alignof(OpenTTDIndustryServices), offsetof(OpenTTDIndustryServices, observe), offsetof(OpenTTDIndustryServices, next), offsetof(OpenTTDIndustryServices, setting), offsetof(OpenTTDIndustryServices, world)});
	layout(138, {sizeof(OpenTTDIndustryProductionResult), alignof(OpenTTDIndustryProductionResult), offsetof(OpenTTDIndustryProductionResult, subtract), offsetof(OpenTTDIndustryProductionResult, add), offsetof(OpenTTDIndustryProductionResult, again), offsetof(OpenTTDIndustryProductionResult, cargo_input), offsetof(OpenTTDIndustryProductionResult, cargo_output), offsetof(OpenTTDIndustryProductionResult, version), offsetof(OpenTTDIndustryProductionResult, num_input), offsetof(OpenTTDIndustryProductionResult, num_output), offsetof(OpenTTDIndustryProductionResult, present)});
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
#endif
