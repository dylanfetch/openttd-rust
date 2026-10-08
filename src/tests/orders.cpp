/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file orders.cpp Typed order storage lifetime and native ABI agreement. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../order_base.h"
#include "../base_consist.h"
#ifdef WITH_RUST
#include "../rust/abi_ffi.h"
TEST_CASE("Order owner native layouts")
{
	auto Layout = [](uint16_t type, const char *, std::initializer_list<size_t> values) {
		uint8_t item = 0;
		for (size_t expected : values) CHECK(openttd_rust_abi_layout(type, item++) == expected);
		CHECK(openttd_rust_abi_layout(type, item) == SIZE_MAX);
	};
	Layout(240, "OpenTTDOrderFields", {sizeof(OpenTTDOrderFields), alignof(OpenTTDOrderFields), offsetof(OpenTTDOrderFields, type), offsetof(OpenTTDOrderFields, flags), offsetof(OpenTTDOrderFields, destination), offsetof(OpenTTDOrderFields, refit_cargo), offsetof(OpenTTDOrderFields, wait_time), offsetof(OpenTTDOrderFields, travel_time), offsetof(OpenTTDOrderFields, max_speed)});
	Layout(241, "OpenTTDConsistState", {sizeof(OpenTTDConsistState), alignof(OpenTTDConsistState), offsetof(OpenTTDConsistState, current_order_time), offsetof(OpenTTDConsistState, lateness_counter), offsetof(OpenTTDConsistState, timetable_start), offsetof(OpenTTDConsistState, last_departure), offsetof(OpenTTDConsistState, next_departure), offsetof(OpenTTDConsistState, round_trip_time), offsetof(OpenTTDConsistState, real_index), offsetof(OpenTTDConsistState, implicit_index), offsetof(OpenTTDConsistState, vehicle_flags)});
	Layout(242, "OpenTTDVehicleOrderState", {sizeof(OpenTTDVehicleOrderState), alignof(OpenTTDVehicleOrderState), offsetof(OpenTTDVehicleOrderState, current), offsetof(OpenTTDVehicleOrderState, orders), offsetof(OpenTTDVehicleOrderState, next_shared), offsetof(OpenTTDVehicleOrderState, previous_shared)});
	Layout(243, "OpenTTDOrderListState", {sizeof(OpenTTDOrderListState), alignof(OpenTTDOrderListState), offsetof(OpenTTDOrderListState, manual), offsetof(OpenTTDOrderListState, vehicles), offsetof(OpenTTDOrderListState, first_shared), offsetof(OpenTTDOrderListState, timetable_duration), offsetof(OpenTTDOrderListState, total_duration)});
	Layout(244, "OpenTTDOrderBackupState", {sizeof(OpenTTDOrderBackupState), alignof(OpenTTDOrderBackupState), offsetof(OpenTTDOrderBackupState, user), offsetof(OpenTTDOrderBackupState, tile), offsetof(OpenTTDOrderBackupState, group), offsetof(OpenTTDOrderBackupState, clone)});
	Layout(245, "OpenTTDOrdersLeaves", {sizeof(OpenTTDOrdersLeaves), alignof(OpenTTDOrdersLeaves), offsetof(OpenTTDOrdersLeaves, vehicle), offsetof(OpenTTDOrdersLeaves, consist), offsetof(OpenTTDOrdersLeaves, list), offsetof(OpenTTDOrdersLeaves, vector), offsetof(OpenTTDOrdersLeaves, backup), offsetof(OpenTTDOrdersLeaves, backup_vector), offsetof(OpenTTDOrdersLeaves, backup_consist), offsetof(OpenTTDOrdersLeaves, query), offsetof(OpenTTDOrdersLeaves, write)});
	Layout(246, "OpenTTDOrdersAction", {sizeof(OpenTTDOrdersAction), alignof(OpenTTDOrdersAction), offsetof(OpenTTDOrdersAction, operation), offsetof(OpenTTDOrdersAction, context), offsetof(OpenTTDOrdersAction, a), offsetof(OpenTTDOrdersAction, b), offsetof(OpenTTDOrdersAction, c)});
	Layout(247, "OpenTTDOrdersClosest", {sizeof(OpenTTDOrdersClosest), alignof(OpenTTDOrdersClosest), offsetof(OpenTTDOrdersClosest, tile), offsetof(OpenTTDOrdersClosest, destination), offsetof(OpenTTDOrdersClosest, reverse), offsetof(OpenTTDOrdersClosest, found)});
}
TEST_CASE("Order vector preserves values and allocation invalidation")
{
	RustOrderVector orders;
	std::vector<Order> original;
	for (uint16_t i = 0; i < 300; ++i) {
		Order order;
		order.MakeGoToStation(StationID(i));
		auto before = orders.data();
		auto reference_before = original.data();
		orders.emplace_back(Order(order));
		original.emplace_back(order);
		CHECK((before != orders.data()) == (reference_before != original.data()));
		CHECK(orders.size() == original.size());
	}
	/* Input aliasing survives a growth reallocation and overlapping insertion. */
	orders.emplace(orders.begin() + 2, std::move(orders[0]));
	original.emplace(original.begin() + 2, std::move(original[0]));
	orders.erase(orders.begin() + 1);
	original.erase(original.begin() + 1);
	orders.Move(2, 9);
	std::rotate(original.begin() + 2, original.begin() + 3, original.begin() + 10);
	for (size_t i = 0; i < original.size(); ++i) CHECK(orders[i].Equals(original[i]));
	auto allocation = orders.data();
	orders.clear();
	orders.resize(2);
	CHECK(orders.data() == allocation);
	CHECK(orders[0].Equals(Order{}));
	CHECK(orders[1].Equals(Order{}));
}
#endif
TEST_CASE("Base consist copies preserve values with independent aliases")
{
	BaseConsist original;
	original.name = "copy";
	original.round_trip_time = 4200;
	original.depot_unbunching_last_departure = 1700;
	original.cur_real_order_index = 23;
	original.vehicle_flags.Set(VehicleFlag::LoadingFinished);
	BaseConsist copied(original);
	CHECK(copied.round_trip_time == 4200);
	CHECK(copied.depot_unbunching_last_departure == 1700);
	CHECK(copied.cur_real_order_index == 23);
	CHECK(copied.vehicle_flags == original.vehicle_flags);
	CHECK(&copied.round_trip_time != &original.round_trip_time);
	copied.round_trip_time = 1;
	CHECK(original.round_trip_time == 4200);
	copied = original;
	CHECK(copied.round_trip_time == 4200);
}
