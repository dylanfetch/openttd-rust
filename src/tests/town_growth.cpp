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
