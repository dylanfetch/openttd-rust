/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_save.hpp Scoped modern/legacy staging, including partial loads. */
#ifndef RUST_TOWN_SAVE_HPP
#define RUST_TOWN_SAVE_HPP
#include "../town.h"

struct TownGrowthSaveScope {
	struct Values { uint16_t counter, rate; uint8_t funding, road, flags; };
	static inline Values current{};
	Values previous;
	Town *town;
	bool loading;
	TownGrowthSaveScope(Town *town, bool loading) : previous(current), town(town), loading(loading)
	{
		current = {town->grow_counter, town->growth_rate, town->fund_buildings_months, town->road_build_months, static_cast<TownFlags>(town->flags).base()};
	}
	~TownGrowthSaveScope()
	{
		if (this->loading) {
			this->town->grow_counter = current.counter;
			this->town->growth_rate = current.rate;
			this->town->fund_buildings_months = current.funding;
			this->town->road_build_months = current.road;
			openttd_rust_town_set(this->town->growth_owner.get(), 4, current.flags);
		}
		current = this->previous;
	}
	TownGrowthSaveScope(const TownGrowthSaveScope &) = delete;
	TownGrowthSaveScope &operator=(const TownGrowthSaveScope &) = delete;
};
#endif /* RUST_TOWN_SAVE_HPP */
