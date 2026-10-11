/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_growth.cpp Narrow owner/legacy staging gaps outside semantic console scenarios. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../town.h"
#ifdef WITH_RUST
#include "../rust/town_save.hpp"
#include "town_protocol.hpp"
#endif
#include "../safeguards.h"

#ifdef WITH_RUST
TEST_CASE("Town growth - command boundaries and callback reentry")
{
	CheckTownProtocol([](bool result) { CHECK(result); });
}

TEST_CASE("Town lifecycle - canonical metadata, histories and partial staging")
{
	REQUIRE(Town::GetNumItems() == 0);
	auto *a = new (TownID(17)) Town(TileIndex{41});
	auto *b = new (TownID(19)) Town();
	CHECK(a->xy == TileIndex{41});
	CHECK(a->exclusivity == CompanyID::Invalid());
	a->cache.population = 999;
	a->ratings[CompanyID(2)] = -123;
	a->received[TAE_FOOD].old_act = 65535;
	a->goal[TAE_WATER] = TOWN_GROWTH_DESERT;
	a->noise_reached = 77;
	CHECK(openttd_rust_town_simulation(a->growth_owner.get())->population == 999);
	CHECK(openttd_rust_town_simulation(a->growth_owner.get())->ratings[2] == -123);
	{
		TownSimulationSaveScope saving(a, false);
		CHECK(TownSimulationSaveScope::current.received[TAE_FOOD].old_act == 65535);
		{
			TownSimulationSaveScope loading(b, true);
			TownSimulationSaveScope::current.population = 3;
			TownSimulationSaveScope::current.ratings[CompanyID(2)] = 456;
		}
		CHECK((b->cache.population == 3 && b->ratings[CompanyID(2)] == 456));
		CHECK(TownSimulationSaveScope::current.population == 999);
	}
	try {
		TownSimulationSaveScope loading(a, true);
		TownSimulationSaveScope::current.noise_reached = 19;
		throw 7;
	} catch (int) {}
	CHECK(a->noise_reached == 19);
	a->GetOrCreateCargoSupplied(5).history[THIS_MONTH].production = 123;
	a->GetOrCreateCargoSupplied(2).history[LAST_MONTH].transported = 77;
	const auto view = openttd_rust_town_supplied_view(a->growth_owner.get());
	REQUIRE(view.size == 2);
	const auto *slots = static_cast<const OpenTTDTownSupplied *>(view.data);
	CHECK((slots[0].cargo == 2 && slots[0].history[LAST_MONTH].transported == 77));
	CHECK((slots[1].cargo == 5 && slots[1].history[THIS_MONTH].production == 123));
	const auto snapshot = a->CopyTownCache();
	a->cache.population = 1000;
	CHECK((snapshot.population == 999 && snapshot != a->cache));
	_town_pool.CleanPool();
	a = new (TownID(17)) Town();
	CHECK((a->supplied.empty() && a->cache.population == 0));
	_town_pool.CleanPool();
}

TEST_CASE("Town growth - indexed construction, partial staging and pool reuse")
{
	REQUIRE(Town::GetNumItems() == 0);
	auto *a = new (TownID(17)) Town();
	auto *b = new (TownID(19)) Town();
	CHECK(static_cast<uint16_t>(a->grow_counter) == 0);
	CHECK(static_cast<uint16_t>(a->growth_rate) == 0);
	CHECK(a->flags.base() == 0);
	a->grow_counter = 65535; a->growth_rate = 0x8005;
	a->flags.Set(TownFlag::HasChurch);
	{
		TownGrowthSaveScope saving(a, false);
		CHECK(TownGrowthSaveScope::current.rate == 0x8005);
		{
			TownGrowthSaveScope loading(b, true);
			TownGrowthSaveScope::current = {37, 65535, 255, 254, 0xFA};
			CHECK(static_cast<uint16_t>(b->grow_counter) == 0);
		}
		CHECK(static_cast<uint16_t>(b->grow_counter) == 37);
		CHECK(b->flags.base() == 0xFA);
		CHECK(TownGrowthSaveScope::current.counter == 65535);
	}
	try {
		TownGrowthSaveScope loading(a, true);
		TownGrowthSaveScope::current.counter = 61;
		throw 1;
	} catch (int) {}
	CHECK(static_cast<uint16_t>(a->grow_counter) == 61);
	CHECK(static_cast<uint16_t>(a->growth_rate) == 0x8005);
	/* The pre-SLV165 adapter's high bit is preserved before conversion, including
	 * the disabled sentinel. Widths/narrowing are the original CITY/TTD descriptors. */
	if (a->growth_rate & 0x8000) a->flags.Set(TownFlag::CustomGrowth);
	a->growth_rate = TownTicksToGameTicks(a->growth_rate & ~0x8000);
	a->grow_counter = TownTicksToGameTicks(a->grow_counter) + a->index.base() % Ticks::TOWN_GROWTH_TICKS;
	CHECK(a->flags.Test(TownFlag::CustomGrowth));
	CHECK(static_cast<uint16_t>(a->growth_rate) == 419);
	CHECK(static_cast<uint16_t>(a->grow_counter) == 4356);
	_town_pool.CleanPool(); // Town::~Town's early return must still destroy its owner member.
	a = new (TownID(17)) Town();
	CHECK(static_cast<uint16_t>(a->grow_counter) == 0);
	CHECK(static_cast<uint16_t>(a->growth_rate) == 0);
	CHECK(static_cast<uint8_t>(a->fund_buildings_months) == 0);
	CHECK(static_cast<uint8_t>(a->road_build_months) == 0);
	CHECK(a->flags.base() == 0);
	_town_pool.CleanPool();
}
#endif
