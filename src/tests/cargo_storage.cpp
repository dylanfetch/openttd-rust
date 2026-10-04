/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file cargo_storage.cpp Native layout contracts for canonical cargo state. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#ifdef WITH_RUST
#include "../rust/cargo_storage_ffi.h"
#include "../rust/cargo_flow_ffi.h"
#include "../rust/abi_ffi.h"
#endif
#include "../safeguards.h"

#ifdef WITH_RUST
TEST_CASE("Cargo storage - scalar ABI layouts")
{
	auto Layout = [](uint16_t type, const char *, std::initializer_list<size_t> fields) {
		uint8_t item = 0;
		for (size_t expected : fields) CHECK(openttd_rust_abi_layout(type, item++) == expected);
		CHECK(openttd_rust_abi_layout(type, item) == SIZE_MAX);
	};
	Layout(280, "OpenTTDCargoPacketFields", {sizeof(OpenTTDCargoPacketFields), alignof(OpenTTDCargoPacketFields), offsetof(OpenTTDCargoPacketFields, feeder_share), offsetof(OpenTTDCargoPacketFields, source_xy), offsetof(OpenTTDCargoPacketFields, count), offsetof(OpenTTDCargoPacketFields, periods_in_transit), offsetof(OpenTTDCargoPacketFields, first_station), offsetof(OpenTTDCargoPacketFields, next_hop), offsetof(OpenTTDCargoPacketFields, source_id), offsetof(OpenTTDCargoPacketFields, travelled_x), offsetof(OpenTTDCargoPacketFields, travelled_y), offsetof(OpenTTDCargoPacketFields, source_type), offsetof(OpenTTDCargoPacketFields, in_vehicle)});
	Layout(281, "OpenTTDCargoListFields", {sizeof(OpenTTDCargoListFields), alignof(OpenTTDCargoListFields), offsetof(OpenTTDCargoListFields, cargo_periods_in_transit), offsetof(OpenTTDCargoListFields, feeder_share), offsetof(OpenTTDCargoListFields, count), offsetof(OpenTTDCargoListFields, reserved_count), offsetof(OpenTTDCargoListFields, action_counts)});
	Layout(282, "OpenTTDCargoStorageServices", {sizeof(OpenTTDCargoStorageServices), alignof(OpenTTDCargoStorageServices), offsetof(OpenTTDCargoStorageServices, can_allocate), offsetof(OpenTTDCargoStorageServices, create), offsetof(OpenTTDCargoStorageServices, packet), offsetof(OpenTTDCargoStorageServices, destroy), offsetof(OpenTTDCargoStorageServices, random), offsetof(OpenTTDCargoStorageServices, coordinate), offsetof(OpenTTDCargoStorageServices, flow), offsetof(OpenTTDCargoStorageServices, pay), offsetof(OpenTTDCargoStorageServices, origin), offsetof(OpenTTDCargoStorageServices, flow_owner), offsetof(OpenTTDCargoStorageServices, random_draw), offsetof(OpenTTDCargoStorageServices, packet_next)});
	Layout(300, "OpenTTDCargoShare", {sizeof(OpenTTDCargoShare), alignof(OpenTTDCargoShare), offsetof(OpenTTDCargoShare, cumulative), offsetof(OpenTTDCargoShare, station), offsetof(OpenTTDCargoShare, found)});
	Layout(301, "OpenTTDCargoOrigin", {sizeof(OpenTTDCargoOrigin), alignof(OpenTTDCargoOrigin), offsetof(OpenTTDCargoOrigin, flow), offsetof(OpenTTDCargoOrigin, origin), offsetof(OpenTTDCargoOrigin, found)});
	Layout(302, "OpenTTDCargoFlowServices", {sizeof(OpenTTDCargoFlowServices), alignof(OpenTTDCargoFlowServices), offsetof(OpenTTDCargoFlowServices, context), offsetof(OpenTTDCargoFlowServices, read), offsetof(OpenTTDCargoFlowServices, job_flows), offsetof(OpenTTDCargoFlowServices, live_flows), offsetof(OpenTTDCargoFlowServices, reroute), offsetof(OpenTTDCargoFlowServices, finish)});
	Layout(310, "OpenTTDCargoCapacityVehicle", {sizeof(OpenTTDCargoCapacityVehicle), alignof(OpenTTDCargoCapacityVehicle), offsetof(OpenTTDCargoCapacityVehicle, list), offsetof(OpenTTDCargoCapacityVehicle, capacity), offsetof(OpenTTDCargoCapacityVehicle, cargo), offsetof(OpenTTDCargoCapacityVehicle, train), offsetof(OpenTTDCargoCapacityVehicle, articulated)});
	Layout(311, "OpenTTDCargoCapacityServices", {sizeof(OpenTTDCargoCapacityServices), alignof(OpenTTDCargoCapacityServices), offsetof(OpenTTDCargoCapacityServices, read), offsetof(OpenTTDCargoCapacityServices, pointer), offsetof(OpenTTDCargoCapacityServices, cargo)});
}
#endif
