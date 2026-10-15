/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_scheduler.cpp Actual scheduler scalar/pool contracts and live traversal. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../roadveh.h"
#include "../rust/vehicle_services.h"
#include "../rust/abi_ffi.h"
#include "../rust/fleet_ffi.h"
#include "../safeguards.h"
#ifdef WITH_RUST
TEST_CASE("Base vehicle scheduler - consumed boundary layouts")
{
	auto Layout = [](uint16_t type, const char *, std::initializer_list<size_t> fields) {
		uint8_t item = 0;
		for (size_t expected : fields) CHECK(openttd_rust_abi_layout(type, item++) == expected);
		CHECK(openttd_rust_abi_layout(type, item) == SIZE_MAX);
	};
	Layout(740, "OpenTTDVehiclePoolCursor", {sizeof(OpenTTDVehiclePoolCursor), alignof(OpenTTDVehiclePoolCursor), offsetof(OpenTTDVehiclePoolCursor, slots), offsetof(OpenTTDVehiclePoolCursor, slots_len), offsetof(OpenTTDVehiclePoolCursor, first_unused)});
	Layout(741, "OpenTTDVehiclePostTick", {sizeof(OpenTTDVehiclePostTick), alignof(OpenTTDVehiclePostTick), offsetof(OpenTTDVehiclePostTick, type), offsetof(OpenTTDVehiclePostTick, period), offsetof(OpenTTDVehiclePostTick, counter), offsetof(OpenTTDVehiclePostTick, motion), offsetof(OpenTTDVehiclePostTick, speed), offsetof(OpenTTDVehiclePostTick, front_type), offsetof(OpenTTDVehiclePostTick, subtype), offsetof(OpenTTDVehiclePostTick, tick), offsetof(OpenTTDVehiclePostTick, front_status), offsetof(OpenTTDVehiclePostTick, status)});
	Layout(743, "OpenTTDVehicleDayCallback", {sizeof(OpenTTDVehicleDayCallback), alignof(OpenTTDVehicleDayCallback), offsetof(OpenTTDVehicleDayCallback, day), offsetof(OpenTTDVehicleDayCallback, has_engine)});
	Layout(748, "OpenTTDVehicleStationLookup", {sizeof(OpenTTDVehicleStationLookup), alignof(OpenTTDVehicleStationLookup), offsetof(OpenTTDVehicleStationLookup, station), offsetof(OpenTTDVehicleStationLookup, id)});
	Layout(749, "OpenTTDVehicleSchedulerServices", {sizeof(OpenTTDVehicleSchedulerServices), alignof(OpenTTDVehicleSchedulerServices), offsetof(OpenTTDVehicleSchedulerServices, pool), offsetof(OpenTTDVehicleSchedulerServices, assertions), offsetof(OpenTTDVehicleSchedulerServices, tick), offsetof(OpenTTDVehicleSchedulerServices, post_tick), offsetof(OpenTTDVehicleSchedulerServices, calendar_day), offsetof(OpenTTDVehicleSchedulerServices, economy_day), offsetof(OpenTTDVehicleSchedulerServices, day_callback_read), offsetof(OpenTTDVehicleSchedulerServices, day_callback), offsetof(OpenTTDVehicleSchedulerServices, randomise_callback), offsetof(OpenTTDVehicleSchedulerServices, first_dirty), offsetof(OpenTTDVehicleSchedulerServices, unknown_day_callback), offsetof(OpenTTDVehicleSchedulerServices, cargo_owner), offsetof(OpenTTDVehicleSchedulerServices, running_sound), offsetof(OpenTTDVehicleSchedulerServices, periodic_sound), offsetof(OpenTTDVehicleSchedulerServices, next_station), offsetof(OpenTTDVehicleSchedulerServices, with_loading_timer), offsetof(OpenTTDVehicleSchedulerServices, reset_accumulators)});
	Layout(750, "OpenTTDVehicleSchedulerTables", {sizeof(OpenTTDVehicleSchedulerTables), alignof(OpenTTDVehicleSchedulerTables), offsetof(OpenTTDVehicleSchedulerTables, scheduler), offsetof(OpenTTDVehicleSchedulerTables, cargo), offsetof(OpenTTDVehicleSchedulerTables, station), offsetof(OpenTTDVehicleSchedulerTables, loading), offsetof(OpenTTDVehicleSchedulerTables, pending), offsetof(OpenTTDVehicleSchedulerTables, transaction), offsetof(OpenTTDVehicleSchedulerTables, group)});
}
TEST_CASE("Base vehicle scheduler - scalar member identity and lifetime")
{
	REQUIRE(Vehicle::CanAllocateItem(2));
	auto *v = new RoadVehicle();
	auto *part = new RoadVehicle();
	v->SetNext(part);
	const auto &services = GetRustVehicleSchedulerServices();
	const auto post = services.post_tick(RustVehicle(part));
	static_assert(std::is_same_v<decltype(post.period), const uint16_t *>);
	static_assert(std::is_same_v<decltype(post.counter), uint16_t *>);
	static_assert(std::is_same_v<decltype(post.motion), uint32_t *>);
	static_assert(std::is_same_v<decltype(post.speed), const uint16_t *>);
	static_assert(std::is_same_v<decltype(post.front_type), const VehicleType *>);
	static_assert(std::is_same_v<decltype(post.front_status), const uint8_t *>);
	static_assert(std::is_same_v<decltype(post.status), const uint8_t *>);
	static_assert(std::is_same_v<decltype(v->vehstatus.base_address()), const uint8_t *>);
	CHECK(post.type == VEH_ROAD);
	CHECK(part->First() == v);
	CHECK(post.period == &part->vcache.cached_cargo_age_period);
	CHECK(post.counter == &part->cargo_age_counter);
	CHECK(post.motion == &part->motion_counter);
	CHECK(post.speed == &v->cur_speed);
	CHECK(post.front_type == &v->type);
	CHECK(post.subtype == &part->subtype);
	CHECK(post.tick == &part->tick_counter);
	CHECK(post.front_status == v->vehstatus.base_address());
	CHECK(post.status == part->vehstatus.base_address());
	CHECK(post.front_status != post.status);
	v->vehstatus.Set(VehState::Stopped);
	part->vehstatus.Set(VehState::Hidden);
	CHECK(*post.front_status == RustVehicleStatus(v));
	CHECK(*post.status == RustVehicleStatus(part));
	v->vehstatus.Reset(VehState::Stopped);
	CHECK(*post.front_status == 0);
	*post.counter = UINT16_MAX;
	*post.motion = UINT32_MAX;
	CHECK(part->cargo_age_counter == UINT16_MAX);
	CHECK(part->motion_counter == UINT32_MAX);
	const auto old_id = v->index;
	const auto grow_id = VehicleID(static_cast<uint32_t>(_vehicle_pool.data.size()));
	REQUIRE(Vehicle::CanAllocateItem());
	new (grow_id) RoadVehicle();
	CHECK(services.post_tick(RustVehicle(part)).counter == post.counter);
	CHECK(services.post_tick(RustVehicle(part)).front_status == post.front_status);
	_vehicle_pool.CleanPool(); // All earlier descriptors are discarded here.
	REQUIRE(Vehicle::CanAllocateItem());
	auto *reused = new (old_id) RoadVehicle();
	const auto fresh = services.post_tick(RustVehicle(reused));
	CHECK(fresh.counter == &reused->cargo_age_counter);
	CHECK(*fresh.counter == 1);
	CHECK(*fresh.motion == 0);
	_vehicle_pool.CleanPool();
}
/* Mutate the actual pool after each visit, identically for Rust and unchanged
 * PoolIterator. No copied IDs or test replica of native indexing metadata. */
static std::vector<uint32_t> VehicleCursorTrace(bool rust)
{
	_vehicle_pool.CleanPool();
	REQUIRE(Vehicle::CanAllocateItem(3));
	new (VehicleID(0)) Vehicle(VEH_EFFECT);
	new (VehicleID(2)) Vehicle(VEH_EFFECT);
	new (VehicleID(4)) Vehicle(VEH_EFFECT);
	const auto &services = GetRustVehicleSchedulerServices();
	auto cursor = services.pool();
	static_assert(std::is_same_v<decltype(cursor.slots), Vehicle *const *>);
	static_assert(std::is_same_v<decltype(cursor.first_unused), const size_t *>);
	CHECK(cursor.slots == _vehicle_pool.data.data());
	CHECK(cursor.slots_len == _vehicle_pool.data.size());
	CHECK(cursor.first_unused == &_vehicle_pool.first_unused);
	std::vector<uint32_t> trace;
	auto Mutate = [&](Vehicle *v) {
		const auto id = v->index.base();
		trace.push_back(id);
		if (id == 0) {
			delete v; // Deleted-current and reuse behind the advancing cursor.
			REQUIRE(Vehicle::CanAllocateItem(3));
			new (VehicleID(0)) Vehicle(VEH_EFFECT);
			delete Vehicle::Get(VehicleID(2));
			new (VehicleID(2)) Vehicle(VEH_EFFECT); // Reuse ahead must still be visited.
			new (VehicleID(3)) Vehicle(VEH_EFFECT); // Raise extent without indexing growth.
		} else if (id == 2) {
			delete Vehicle::Get(VehicleID(4)); // Hole ahead.
			const auto growth = static_cast<uint32_t>(_vehicle_pool.data.size());
			REQUIRE(Vehicle::CanAllocateItem(2));
			new (VehicleID(growth)) Vehicle(VEH_EFFECT); // Growth invalidates captured slots.
			new (VehicleID(growth + 1)) Vehicle(VEH_EFFECT);
			CHECK(*cursor.first_unused == _vehicle_pool.first_unused); // Stable canonical address.
		}
	};
	if (rust) {
		size_t from = 0;
		while (auto *shell = openttd_rust_vehicle_cursor_next(&services, &cursor, &from)) Mutate(VehicleOf(shell));
		CHECK(cursor.slots == _vehicle_pool.data.data());
		CHECK(cursor.slots_len == _vehicle_pool.data.size());
	} else {
		for (Vehicle *v : Vehicle::Iterate()) Mutate(v);
	}
	_vehicle_pool.CleanPool();
	cursor = services.pool(); // Fresh acquisition after reset; empty pool ends.
	size_t from = 0;
	CHECK(openttd_rust_vehicle_cursor_next(&services, &cursor, &from) == nullptr);
	return trace;
}
TEST_CASE("Base vehicle scheduler - live cursor equals unchanged PoolIterator")
{
	const auto source = VehicleCursorTrace(false);
	CHECK(VehicleCursorTrace(true) == source);
	REQUIRE(source.size() == 5);
	CHECK(source[0] == 0);
	CHECK(source[1] == 2);
	CHECK(source[2] == 3);
}
/* Synthetic virtual Tick on actual pool shells checks the transient ordering
 * saves cannot show: growth before assertion, timing scope return before post,
 * post-Tick type mutation, then deletion with no post access. */
static bool _scheduler_tick_scope_ended;
static uint32_t _scheduler_post_visits;
static uint32_t _scheduler_pool_reads;
struct SchedulerProbeVehicle final : Vehicle {
	SchedulerProbeVehicle() : Vehicle(VEH_EFFECT) {}
	bool Tick() override
	{
		struct EndScope { ~EndScope() { _scheduler_tick_scope_ended = true; } } scope;
		_scheduler_tick_scope_ended = false;
		if (this->index.base() == 0) {
			CHECK(Vehicle::CanAllocateItem());
			new (VehicleID(static_cast<uint32_t>(_vehicle_pool.data.size()))) SchedulerProbeVehicle();
			this->type = VEH_DISASTER;
			return true;
		}
		delete this;
		return false;
	}
};
TEST_CASE("Base vehicle scheduler - Tick growth, timing return and fresh type")
{
	_vehicle_pool.CleanPool();
	REQUIRE(Vehicle::CanAllocateItem());
	new (VehicleID(0)) SchedulerProbeVehicle();
	_scheduler_post_visits = _scheduler_pool_reads = 0;
	auto services = GetRustVehicleSchedulerServices();
	services.pool = []() noexcept {
		++_scheduler_pool_reads;
		return GetRustVehicleSchedulerServices().pool();
	};
	services.post_tick = [](OpenTTDVehicle *shell) noexcept {
		CHECK(_scheduler_tick_scope_ended);
		CHECK(_scheduler_pool_reads == 2); // Initial acquisition, then growth before assert/post.
		++_scheduler_post_visits;
		const auto post = GetRustVehicleSchedulerServices().post_tick(shell);
		CHECK(post.type == VEH_DISASTER); // Mutable pre-Tick Effect tag must not be reused.
		CHECK(post.front_type == nullptr);
		CHECK(post.period == nullptr);
		CHECK(post.status == nullptr);
		return post;
	};
	services.next_station = [](uint32_t) noexcept { return OpenTTDVehicleStationLookup{}; };
	OpenTTDFleetGroupServices group{};
	group.current_company = []() noexcept -> uint8_t { return 0; };
	OpenTTDFleetPendingServices pending{};
	pending.set_current = [](uint8_t) noexcept {};
	OpenTTDFleetTransactionServices transaction{};
	OpenTTDVehicleSchedulerTables tables{};
	tables.scheduler = &services;
	tables.pending = &pending;
	tables.transaction = &transaction;
	tables.group = &group;
	openttd_rust_vehicle_ticks(&tables, false, 0, nullptr);
	CHECK(_scheduler_tick_scope_ended);
	CHECK(_scheduler_post_visits == 1); // Deleted higher shell must never be dereferenced.
	CHECK(_vehicle_pool.items == 1);
	_vehicle_pool.CleanPool();
}
#endif /* WITH_RUST */
