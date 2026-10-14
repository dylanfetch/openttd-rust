/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_scheduler.cpp Consumed live-scheduler boundary layouts. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../rust/vehicle_ffi.h"
#include "../rust/abi_ffi.h"
#include "../safeguards.h"

#ifdef WITH_RUST
TEST_CASE("Base vehicle scheduler - consumed boundary layouts")
{
	auto Layout = [](uint16_t type, const char *, std::initializer_list<size_t> fields) {
		uint8_t item = 0;
		for (size_t expected : fields) CHECK(openttd_rust_abi_layout(type, item++) == expected);
		CHECK(openttd_rust_abi_layout(type, item) == SIZE_MAX);
	};
	Layout(740, "OpenTTDVehicleTickOwner", {sizeof(OpenTTDVehicleTickOwner), alignof(OpenTTDVehicleTickOwner), offsetof(OpenTTDVehicleTickOwner, road), offsetof(OpenTTDVehicleTickOwner, train), offsetof(OpenTTDVehicleTickOwner, ship), offsetof(OpenTTDVehicleTickOwner, aircraft)});
	Layout(741, "OpenTTDVehicleDispatch", {sizeof(OpenTTDVehicleDispatch), alignof(OpenTTDVehicleDispatch), offsetof(OpenTTDVehicleDispatch, vehicle), offsetof(OpenTTDVehicleDispatch, owner), offsetof(OpenTTDVehicleDispatch, id), offsetof(OpenTTDVehicleDispatch, type), offsetof(OpenTTDVehicleDispatch, subtype), offsetof(OpenTTDVehicleDispatch, road_tick)});
	Layout(742, "OpenTTDVehicleDayLookup", {sizeof(OpenTTDVehicleDayLookup), alignof(OpenTTDVehicleDayLookup), offsetof(OpenTTDVehicleDayLookup, vehicle), offsetof(OpenTTDVehicleDayLookup, end)});
	Layout(743, "OpenTTDVehicleDayCallback", {sizeof(OpenTTDVehicleDayCallback), alignof(OpenTTDVehicleDayCallback), offsetof(OpenTTDVehicleDayCallback, day), offsetof(OpenTTDVehicleDayCallback, has_engine)});
	Layout(744, "OpenTTDVehicleCargoTick", {sizeof(OpenTTDVehicleCargoTick), alignof(OpenTTDVehicleCargoTick), offsetof(OpenTTDVehicleCargoTick, front), offsetof(OpenTTDVehicleCargoTick, cargo), offsetof(OpenTTDVehicleCargoTick, period), offsetof(OpenTTDVehicleCargoTick, counter)});
	Layout(745, "OpenTTDVehicleMotionTick", {sizeof(OpenTTDVehicleMotionTick), alignof(OpenTTDVehicleMotionTick), offsetof(OpenTTDVehicleMotionTick, motion), offsetof(OpenTTDVehicleMotionTick, speed), offsetof(OpenTTDVehicleMotionTick, front_status), offsetof(OpenTTDVehicleMotionTick, status), offsetof(OpenTTDVehicleMotionTick, front_type)});
	Layout(746, "OpenTTDVehicleSoundTick", {sizeof(OpenTTDVehicleSoundTick), alignof(OpenTTDVehicleSoundTick), offsetof(OpenTTDVehicleSoundTick, motion), offsetof(OpenTTDVehicleSoundTick, speed), offsetof(OpenTTDVehicleSoundTick, subtype)});
	Layout(747, "OpenTTDVehiclePeriodicSound", {sizeof(OpenTTDVehiclePeriodicSound), alignof(OpenTTDVehiclePeriodicSound), offsetof(OpenTTDVehiclePeriodicSound, speed), offsetof(OpenTTDVehiclePeriodicSound, tick), offsetof(OpenTTDVehiclePeriodicSound, front_status)});
	Layout(748, "OpenTTDVehicleStationLookup", {sizeof(OpenTTDVehicleStationLookup), alignof(OpenTTDVehicleStationLookup), offsetof(OpenTTDVehicleStationLookup, station), offsetof(OpenTTDVehicleStationLookup, id)});
	Layout(749, "OpenTTDVehicleSchedulerServices", {sizeof(OpenTTDVehicleSchedulerServices), alignof(OpenTTDVehicleSchedulerServices), offsetof(OpenTTDVehicleSchedulerServices, next), offsetof(OpenTTDVehicleSchedulerServices, day_at), offsetof(OpenTTDVehicleSchedulerServices, dispatch), offsetof(OpenTTDVehicleSchedulerServices, day_callback_read), offsetof(OpenTTDVehicleSchedulerServices, day_callback), offsetof(OpenTTDVehicleSchedulerServices, randomise_callback), offsetof(OpenTTDVehicleSchedulerServices, first_dirty), offsetof(OpenTTDVehicleSchedulerServices, unknown_day_callback), offsetof(OpenTTDVehicleSchedulerServices, other_tick), offsetof(OpenTTDVehicleSchedulerServices, assert_survival), offsetof(OpenTTDVehicleSchedulerServices, cargo_read), offsetof(OpenTTDVehicleSchedulerServices, write_cargo_counter), offsetof(OpenTTDVehicleSchedulerServices, cargo_period), offsetof(OpenTTDVehicleSchedulerServices, motion_read), offsetof(OpenTTDVehicleSchedulerServices, write_motion), offsetof(OpenTTDVehicleSchedulerServices, sound_read), offsetof(OpenTTDVehicleSchedulerServices, running_sound), offsetof(OpenTTDVehicleSchedulerServices, periodic_read), offsetof(OpenTTDVehicleSchedulerServices, periodic_sound), offsetof(OpenTTDVehicleSchedulerServices, next_station), offsetof(OpenTTDVehicleSchedulerServices, economy_begin), offsetof(OpenTTDVehicleSchedulerServices, economy_end), offsetof(OpenTTDVehicleSchedulerServices, reset_accumulators), offsetof(OpenTTDVehicleSchedulerServices, train_begin), offsetof(OpenTTDVehicleSchedulerServices, road_begin), offsetof(OpenTTDVehicleSchedulerServices, ship_begin), offsetof(OpenTTDVehicleSchedulerServices, aircraft_begin), offsetof(OpenTTDVehicleSchedulerServices, train_end), offsetof(OpenTTDVehicleSchedulerServices, road_end), offsetof(OpenTTDVehicleSchedulerServices, ship_end), offsetof(OpenTTDVehicleSchedulerServices, aircraft_end)});
	Layout(750, "OpenTTDVehicleSchedulerTables", {sizeof(OpenTTDVehicleSchedulerTables), alignof(OpenTTDVehicleSchedulerTables), offsetof(OpenTTDVehicleSchedulerTables, scheduler), offsetof(OpenTTDVehicleSchedulerTables, road), offsetof(OpenTTDVehicleSchedulerTables, train), offsetof(OpenTTDVehicleSchedulerTables, ship), offsetof(OpenTTDVehicleSchedulerTables, aircraft), offsetof(OpenTTDVehicleSchedulerTables, shared), offsetof(OpenTTDVehicleSchedulerTables, cargo), offsetof(OpenTTDVehicleSchedulerTables, station), offsetof(OpenTTDVehicleSchedulerTables, loading), offsetof(OpenTTDVehicleSchedulerTables, pending), offsetof(OpenTTDVehicleSchedulerTables, transaction), offsetof(OpenTTDVehicleSchedulerTables, group)});
}
#endif /* WITH_RUST */
