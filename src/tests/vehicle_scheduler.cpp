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
#include "../roadveh.h"
#include "../rust/vehicle_services.h"
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
	Layout(744, "OpenTTDVehicleCargoAddresses", {sizeof(OpenTTDVehicleCargoAddresses), alignof(OpenTTDVehicleCargoAddresses), offsetof(OpenTTDVehicleCargoAddresses, front), offsetof(OpenTTDVehicleCargoAddresses, period), offsetof(OpenTTDVehicleCargoAddresses, counter)});
	Layout(745, "OpenTTDVehicleMotionAddresses", {sizeof(OpenTTDVehicleMotionAddresses), alignof(OpenTTDVehicleMotionAddresses), offsetof(OpenTTDVehicleMotionAddresses, motion), offsetof(OpenTTDVehicleMotionAddresses, speed), offsetof(OpenTTDVehicleMotionAddresses, front_type), offsetof(OpenTTDVehicleMotionAddresses, subtype), offsetof(OpenTTDVehicleMotionAddresses, tick)});
	Layout(748, "OpenTTDVehicleStationLookup", {sizeof(OpenTTDVehicleStationLookup), alignof(OpenTTDVehicleStationLookup), offsetof(OpenTTDVehicleStationLookup, station), offsetof(OpenTTDVehicleStationLookup, id)});
	Layout(749, "OpenTTDVehicleSchedulerServices", {sizeof(OpenTTDVehicleSchedulerServices), alignof(OpenTTDVehicleSchedulerServices), offsetof(OpenTTDVehicleSchedulerServices, next), offsetof(OpenTTDVehicleSchedulerServices, day_at), offsetof(OpenTTDVehicleSchedulerServices, dispatch), offsetof(OpenTTDVehicleSchedulerServices, day_callback_read), offsetof(OpenTTDVehicleSchedulerServices, day_callback), offsetof(OpenTTDVehicleSchedulerServices, randomise_callback), offsetof(OpenTTDVehicleSchedulerServices, first_dirty), offsetof(OpenTTDVehicleSchedulerServices, unknown_day_callback), offsetof(OpenTTDVehicleSchedulerServices, other_tick), offsetof(OpenTTDVehicleSchedulerServices, assert_survival), offsetof(OpenTTDVehicleSchedulerServices, cargo_addresses), offsetof(OpenTTDVehicleSchedulerServices, cargo_owner), offsetof(OpenTTDVehicleSchedulerServices, motion_addresses), offsetof(OpenTTDVehicleSchedulerServices, running_sound), offsetof(OpenTTDVehicleSchedulerServices, periodic_sound), offsetof(OpenTTDVehicleSchedulerServices, next_station), offsetof(OpenTTDVehicleSchedulerServices, economy_begin), offsetof(OpenTTDVehicleSchedulerServices, economy_end), offsetof(OpenTTDVehicleSchedulerServices, reset_accumulators), offsetof(OpenTTDVehicleSchedulerServices, train_begin), offsetof(OpenTTDVehicleSchedulerServices, road_begin), offsetof(OpenTTDVehicleSchedulerServices, ship_begin), offsetof(OpenTTDVehicleSchedulerServices, aircraft_begin), offsetof(OpenTTDVehicleSchedulerServices, train_end), offsetof(OpenTTDVehicleSchedulerServices, road_end), offsetof(OpenTTDVehicleSchedulerServices, ship_end), offsetof(OpenTTDVehicleSchedulerServices, aircraft_end)});
	Layout(750, "OpenTTDVehicleSchedulerTables", {sizeof(OpenTTDVehicleSchedulerTables), alignof(OpenTTDVehicleSchedulerTables), offsetof(OpenTTDVehicleSchedulerTables, scheduler), offsetof(OpenTTDVehicleSchedulerTables, road), offsetof(OpenTTDVehicleSchedulerTables, train), offsetof(OpenTTDVehicleSchedulerTables, ship), offsetof(OpenTTDVehicleSchedulerTables, aircraft), offsetof(OpenTTDVehicleSchedulerTables, shared), offsetof(OpenTTDVehicleSchedulerTables, cargo), offsetof(OpenTTDVehicleSchedulerTables, station), offsetof(OpenTTDVehicleSchedulerTables, loading), offsetof(OpenTTDVehicleSchedulerTables, pending), offsetof(OpenTTDVehicleSchedulerTables, transaction), offsetof(OpenTTDVehicleSchedulerTables, group)});
}
/* Saved state cannot establish transient member identity or write-through. Use
 * actual native shells, including pool array growth and subsequent ID reuse. */
TEST_CASE("Base vehicle scheduler - scalar member identity and lifetime")
{
	REQUIRE(Vehicle::CanAllocateItem(2));
	auto *v = new RoadVehicle();
	const auto id = v->index;
	const auto &services = GetRustVehicleSchedulerServices();
	const auto cargo = services.cargo_addresses(RustVehicle(v));
	const auto motion = services.motion_addresses(RustVehicle(v), cargo.front);
	static_assert(std::is_same_v<decltype(cargo.period), const uint16_t *>);
	static_assert(std::is_same_v<decltype(cargo.counter), uint16_t *>);
	static_assert(std::is_same_v<decltype(motion.motion), uint32_t *>);
	static_assert(std::is_same_v<decltype(motion.speed), const uint16_t *>);
	static_assert(std::is_same_v<decltype(motion.front_type), const VehicleType *>);
	static_assert(std::is_same_v<decltype(motion.subtype), const uint8_t *>);
	static_assert(std::is_same_v<decltype(motion.tick), const uint8_t *>);
	CHECK(cargo.front == RustVehicle(v->First()));
	CHECK(cargo.period == &v->vcache.cached_cargo_age_period);
	CHECK(cargo.counter == &v->cargo_age_counter);
	CHECK(motion.motion == &v->motion_counter);
	CHECK(motion.speed == &v->cur_speed);
	CHECK(motion.front_type == &v->type);
	CHECK(motion.subtype == &v->subtype);
	CHECK(motion.tick == &v->tick_counter);
	*cargo.counter = UINT16_MAX;
	*motion.motion = UINT32_MAX;
	CHECK(v->cargo_age_counter == UINT16_MAX);
	CHECK(v->motion_counter == UINT32_MAX);
	v->cur_speed = 37;
	CHECK(*motion.speed == RustVehicleSpeed(v));
	/* Force growth of the pool's indexing array: separately allocated shells
	 * and their named members retain their addresses. */
	const auto grow_id = VehicleID(static_cast<uint32_t>(id.base()) + 2048);
	REQUIRE(Vehicle::CanAllocateItem(1));
	new (grow_id) RoadVehicle();
	CHECK(Vehicle::Get(id) == v);
	CHECK(services.cargo_addresses(RustVehicle(v)).counter == cargo.counter);
	CHECK(services.motion_addresses(RustVehicle(v), cargo.front).motion == motion.motion);
#if !defined(NDEBUG) || defined(WITH_ASSERT)
	CHECK(services.assert_survival != nullptr);
	services.assert_survival(RustVehicle(v), id.base(), true);
#else
	CHECK(services.assert_survival == nullptr);
#endif
	/* Descriptors above are dead now, regardless of allocator address reuse. */
	_vehicle_pool.CleanPool();
	auto *reused = new (id) RoadVehicle();
	CHECK(reused->cargo_age_counter == 0);
	CHECK(reused->motion_counter == 0);
	const auto fresh = services.cargo_addresses(RustVehicle(reused));
	CHECK(fresh.counter == &reused->cargo_age_counter);
	CHECK(fresh.front == RustVehicle(reused));
	_vehicle_pool.CleanPool();
}
#endif /* WITH_RUST */
