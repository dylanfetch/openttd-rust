/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fleet.cpp WIP fleet owner layout, scalar alias and copy lifetime checks. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#ifdef WITH_RUST
#include "../group.h"
#include "../autoreplace_base.h"
#include "../company_base.h"
#include "../rust/abi_ffi.h"
#endif
#include "../safeguards.h"
#ifdef WITH_RUST
TEST_CASE("Fleet - canonical state layouts and independent lifetimes")
{
	auto layout = [](uint16_t type, std::initializer_list<size_t> fields) {
		uint8_t i = 0;
		for (size_t expected : fields) CHECK(openttd_rust_abi_layout(type, i++) == expected);
		CHECK(openttd_rust_abi_layout(type, i) == SIZE_MAX);
	};
	layout(340, {sizeof(FleetGroupFields), alignof(FleetGroupFields), offsetof(FleetGroupFields, owner), offsetof(FleetGroupFields, vehicle_type), offsetof(FleetGroupFields, flags), offsetof(FleetGroupFields, livery), offsetof(FleetGroupFields, parent), offsetof(FleetGroupFields, number)});
	layout(341, {sizeof(FleetStatisticsFields), alignof(FleetStatisticsFields), offsetof(FleetStatisticsFields, profit_last_year), offsetof(FleetStatisticsFields, profit_last_year_min_age), offsetof(FleetStatisticsFields, num_vehicle), offsetof(FleetStatisticsFields, num_vehicle_min_age), offsetof(FleetStatisticsFields, autoreplace_defined), offsetof(FleetStatisticsFields, autoreplace_finished)});
	layout(342, {sizeof(FleetRenewFields), alignof(FleetRenewFields), offsetof(FleetRenewFields, from), offsetof(FleetRenewFields, to), offsetof(FleetRenewFields, next), offsetof(FleetRenewFields, group_id), offsetof(FleetRenewFields, replace_when_old)});
	FleetOwner<FleetGroupFields, openttd_rust_fleet_group_create, openttd_rust_fleet_group_destroy> group;
	CHECK(group.state->parent == GroupID::Invalid());
	CHECK(group.state->owner == INVALID_OWNER);
	const std::string name = "Group \xE2\x98\x83";
	openttd_rust_fleet_set_name(group.state, reinterpret_cast<const uint8_t *>(name.data()), name.size());
	std::string copy(openttd_rust_fleet_name(group.state, nullptr, 0), '\0');
	openttd_rust_fleet_name(group.state, reinterpret_cast<uint8_t *>(copy.data()), copy.size());
	CHECK(copy == name);
	FleetChildren children{group.state};
	children.insert(GroupID(7));
	children.insert(GroupID(2));
	children.insert(GroupID(7));
	CHECK(std::vector<GroupID>(children.begin(), children.end()) == std::vector<GroupID>{GroupID(2), GroupID(7)});
	children.erase(GroupID(2));
	CHECK(std::vector<GroupID>(children.begin(), children.end()) == std::vector<GroupID>{GroupID(7)});
	GroupStatistics stats;
	CHECK(&stats.profit_last_year == &stats.state.state->profit_last_year);
	stats.num_engines[EngineID(3)]--;
	CHECK(openttd_rust_fleet_engine_count(stats.state.state, 3) == UINT16_MAX);
	stats.num_engines[EngineID(3)]++;
	CHECK(openttd_rust_fleet_engine_count(stats.state.state, 3) == 0);
	stats.profit_last_year = INT64_MIN;
	stats.num_engines[EngineID(4)] += 3;
	GroupStatistics copied = stats;
	CHECK(copied.state.state != stats.state.state);
	CHECK(&copied.profit_last_year == &copied.state.state->profit_last_year);
	CHECK(copied.profit_last_year == Money(INT64_MIN));
	copied.profit_last_year = 17;
	copied.num_engines[EngineID(4)]++;
	CHECK(stats.profit_last_year == Money(INT64_MIN));
	CHECK(openttd_rust_fleet_engine_count(stats.state.state, 4) == 3);
	stats = copied;
	CHECK(stats.profit_last_year == Money(17));
	CHECK(openttd_rust_fleet_engine_count(stats.state.state, 4) == 4);
	CompanyProperties company;
	CHECK(company.RenewalList() == nullptr);
	CompanyProperties detached = company;
	CHECK(&detached.RenewalList() != &company.RenewalList());
	FleetOwner<GroupID, openttd_rust_fleet_membership_create, openttd_rust_fleet_membership_destroy, nullptr, true> membership;
	CHECK(*membership.state == GroupID::Invalid());
	*membership.state = DEFAULT_GROUP;
	CHECK(*membership.state == DEFAULT_GROUP);
}
#endif
