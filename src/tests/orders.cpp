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
	Layout(260, "OpenTTDOrderFields", {sizeof(OpenTTDOrderFields), alignof(OpenTTDOrderFields), offsetof(OpenTTDOrderFields, type), offsetof(OpenTTDOrderFields, flags), offsetof(OpenTTDOrderFields, destination), offsetof(OpenTTDOrderFields, refit_cargo), offsetof(OpenTTDOrderFields, wait_time), offsetof(OpenTTDOrderFields, travel_time), offsetof(OpenTTDOrderFields, max_speed)});
	Layout(261, "OpenTTDConsistState", {sizeof(OpenTTDConsistState), alignof(OpenTTDConsistState), offsetof(OpenTTDConsistState, current_order_time), offsetof(OpenTTDConsistState, lateness_counter), offsetof(OpenTTDConsistState, timetable_start), offsetof(OpenTTDConsistState, last_departure), offsetof(OpenTTDConsistState, next_departure), offsetof(OpenTTDConsistState, round_trip_time), offsetof(OpenTTDConsistState, real_index), offsetof(OpenTTDConsistState, implicit_index), offsetof(OpenTTDConsistState, vehicle_flags)});
	Layout(262, "OpenTTDVehicleOrderState", {sizeof(OpenTTDVehicleOrderState), alignof(OpenTTDVehicleOrderState), offsetof(OpenTTDVehicleOrderState, current), offsetof(OpenTTDVehicleOrderState, orders), offsetof(OpenTTDVehicleOrderState, next_shared), offsetof(OpenTTDVehicleOrderState, previous_shared)});
	Layout(263, "OpenTTDOrderListState", {sizeof(OpenTTDOrderListState), alignof(OpenTTDOrderListState), offsetof(OpenTTDOrderListState, manual), offsetof(OpenTTDOrderListState, vehicles), offsetof(OpenTTDOrderListState, first_shared), offsetof(OpenTTDOrderListState, timetable_duration), offsetof(OpenTTDOrderListState, total_duration)});
	Layout(264, "OpenTTDOrderBackupState", {sizeof(OpenTTDOrderBackupState), alignof(OpenTTDOrderBackupState), offsetof(OpenTTDOrderBackupState, user), offsetof(OpenTTDOrderBackupState, tile), offsetof(OpenTTDOrderBackupState, group), offsetof(OpenTTDOrderBackupState, clone)});
	Layout(265, "OpenTTDOrdersLeaves", {sizeof(OpenTTDOrdersLeaves), alignof(OpenTTDOrdersLeaves), offsetof(OpenTTDOrdersLeaves, vehicle), offsetof(OpenTTDOrdersLeaves, consist), offsetof(OpenTTDOrdersLeaves, list), offsetof(OpenTTDOrdersLeaves, vector), offsetof(OpenTTDOrdersLeaves, backup), offsetof(OpenTTDOrdersLeaves, backup_vector), offsetof(OpenTTDOrdersLeaves, backup_consist), offsetof(OpenTTDOrdersLeaves, first_vehicle), offsetof(OpenTTDOrdersLeaves, last_station), offsetof(OpenTTDOrdersLeaves, ownerless_station), offsetof(OpenTTDOrdersLeaves, vehicle_type), offsetof(OpenTTDOrdersLeaves, vehicle_status), offsetof(OpenTTDOrdersLeaves, tick_counter), offsetof(OpenTTDOrdersLeaves, primary_vehicle), offsetof(OpenTTDOrdersLeaves, vehicle_ownership), offsetof(OpenTTDOrdersLeaves, ticks_per_second), offsetof(OpenTTDOrdersLeaves, unit_number), offsetof(OpenTTDOrdersLeaves, economy_date), offsetof(OpenTTDOrdersLeaves, economy_fraction), offsetof(OpenTTDOrdersLeaves, maximum_date), offsetof(OpenTTDOrdersLeaves, timetable_year_limit), offsetof(OpenTTDOrdersLeaves, stopped_or_crashed), offsetof(OpenTTDOrdersLeaves, allocate_list), offsetof(OpenTTDOrdersLeaves, suppress_implicit), offsetof(OpenTTDOrdersLeaves, shared_window), offsetof(OpenTTDOrdersLeaves, vehicle_id), offsetof(OpenTTDOrdersLeaves, percent_filled), offsetof(OpenTTDOrdersLeaves, reliability), offsetof(OpenTTDOrdersLeaves, engine_reliability), offsetof(OpenTTDOrdersLeaves, display_speed), offsetof(OpenTTDOrdersLeaves, age_years), offsetof(OpenTTDOrdersLeaves, needs_service), offsetof(OpenTTDOrdersLeaves, remaining_years), offsetof(OpenTTDOrdersLeaves, airport_tile), offsetof(OpenTTDOrdersLeaves, base_station_tile), offsetof(OpenTTDOrdersLeaves, station_tile), offsetof(OpenTTDOrdersLeaves, depot_tile), offsetof(OpenTTDOrdersLeaves, distance), offsetof(OpenTTDOrdersLeaves, station_location), offsetof(OpenTTDOrdersLeaves, destination_tile), offsetof(OpenTTDOrdersLeaves, aircraft_flying), offsetof(OpenTTDOrdersLeaves, target_airport), offsetof(OpenTTDOrdersLeaves, waypoint_tile), offsetof(OpenTTDOrdersLeaves, at_station), offsetof(OpenTTDOrdersLeaves, tile_station), offsetof(OpenTTDOrdersLeaves, ship_station_tile), offsetof(OpenTTDOrdersLeaves, valid_station), offsetof(OpenTTDOrdersLeaves, station_owner), offsetof(OpenTTDOrdersLeaves, can_use_station), offsetof(OpenTTDOrdersLeaves, owner_check), offsetof(OpenTTDOrdersLeaves, station_error), offsetof(OpenTTDOrdersLeaves, has_hangar), offsetof(OpenTTDOrdersLeaves, valid_depot), offsetof(OpenTTDOrdersLeaves, depot_owner), offsetof(OpenTTDOrdersLeaves, rail_depot), offsetof(OpenTTDOrdersLeaves, road_depot), offsetof(OpenTTDOrdersLeaves, ship_depot), offsetof(OpenTTDOrdersLeaves, valid_waypoint), offsetof(OpenTTDOrdersLeaves, waypoint_facilities), offsetof(OpenTTDOrdersLeaves, waypoint_owner), offsetof(OpenTTDOrdersLeaves, list_capacity), offsetof(OpenTTDOrdersLeaves, next_backup), offsetof(OpenTTDOrdersLeaves, next_vehicle), offsetof(OpenTTDOrdersLeaves, aircraft_range), offsetof(OpenTTDOrdersLeaves, aircraft_range_square), offsetof(OpenTTDOrdersLeaves, bus), offsetof(OpenTTDOrdersLeaves, backup_capacity), offsetof(OpenTTDOrdersLeaves, create_backup), offsetof(OpenTTDOrdersLeaves, networking), offsetof(OpenTTDOrdersLeaves, network_server), offsetof(OpenTTDOrdersLeaves, network_client), offsetof(OpenTTDOrdersLeaves, server_client), offsetof(OpenTTDOrdersLeaves, default_group), offsetof(OpenTTDOrdersLeaves, vehicle_tile), offsetof(OpenTTDOrdersLeaves, vehicle_group), offsetof(OpenTTDOrdersLeaves, unique_backup_name), offsetof(OpenTTDOrdersLeaves, backup_id), offsetof(OpenTTDOrdersLeaves, backup_hangar), offsetof(OpenTTDOrdersLeaves, review_setting), offsetof(OpenTTDOrdersLeaves, local_owner), offsetof(OpenTTDOrdersLeaves, day_counter), offsetof(OpenTTDOrdersLeaves, fast_aircraft), offsetof(OpenTTDOrdersLeaves, short_strip), offsetof(OpenTTDOrdersLeaves, no_jet_crash), offsetof(OpenTTDOrdersLeaves, append_station), offsetof(OpenTTDOrdersLeaves, invalidate_station_list), offsetof(OpenTTDOrdersLeaves, command_error), offsetof(OpenTTDOrdersLeaves, timetable_dirty), offsetof(OpenTTDOrdersLeaves, invalidate_order), offsetof(OpenTTDOrdersLeaves, vehicle_dirty), offsetof(OpenTTDOrdersLeaves, delete_order_news), offsetof(OpenTTDOrdersLeaves, suppress_implicit_write), offsetof(OpenTTDOrdersLeaves, invalidate_vehicle_list), offsetof(OpenTTDOrdersLeaves, close_shared_window), offsetof(OpenTTDOrdersLeaves, invalidate_shared_window), offsetof(OpenTTDOrdersLeaves, last_station_write), offsetof(OpenTTDOrdersLeaves, dirty_vehicle_windows), offsetof(OpenTTDOrdersLeaves, capture_backup_metadata), offsetof(OpenTTDOrdersLeaves, clear_backup_name), offsetof(OpenTTDOrdersLeaves, restore_backup_metadata), offsetof(OpenTTDOrdersLeaves, order_news), offsetof(OpenTTDOrdersLeaves, debug_list), offsetof(OpenTTDOrdersLeaves, assert_departure_range), offsetof(OpenTTDOrdersLeaves, delete_list), offsetof(OpenTTDOrdersLeaves, leave_station), offsetof(OpenTTDOrdersLeaves, reverse_train), offsetof(OpenTTDOrdersLeaves, next_airport), offsetof(OpenTTDOrdersLeaves, set_destination), offsetof(OpenTTDOrdersLeaves, closest_depot), offsetof(OpenTTDOrdersLeaves, share_command), offsetof(OpenTTDOrdersLeaves, group_command), offsetof(OpenTTDOrdersLeaves, delete_backup), offsetof(OpenTTDOrdersLeaves, clear_backup_gui), offsetof(OpenTTDOrdersLeaves, clear_backup_post), offsetof(OpenTTDOrdersLeaves, missing_aircraft_orders), offsetof(OpenTTDOrdersLeaves, change_timetable_command)});
	Layout(266, "OpenTTDOrdersClosest", {sizeof(OpenTTDOrdersClosest), alignof(OpenTTDOrdersClosest), offsetof(OpenTTDOrdersClosest, tile), offsetof(OpenTTDOrdersClosest, destination), offsetof(OpenTTDOrdersClosest, reverse), offsetof(OpenTTDOrdersClosest, found)});
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
TEST_CASE("Order list owners retire and reconstruct at a reused pool index")
{
	REQUIRE(OrderList::CanAllocateItem());
	OrderList *list = new OrderList();
	OrderListID index = list->index;
	Order order;
	order.MakeDummy();
	list->InsertOrderAt(std::move(order), 0);
	CHECK(list->GetNumOrders() == 1);
	CHECK(list->GetNumManualOrders() == 1);
	delete list;

	/* Save/load uses indexed construction before populating the new owner. */
	list = new (index) OrderList();
	CHECK(list->index == index);
	CHECK(list->GetNumOrders() == 0);
	CHECK(list->GetNumManualOrders() == 0);
	CHECK(list->GetNumVehicles() == 0);
	CHECK(list->GetTimetableDurationIncomplete() == 0);
	delete list;
}
