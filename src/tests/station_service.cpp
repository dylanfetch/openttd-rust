/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file station_service.cpp Canonical owner lifetime and cross-language layouts. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "mock_environment.h"
#ifdef WITH_RUST
#include "../rust/station_service_ffi.h"
#include "../station_base.h"
#include "../station_func.h"
#include "../station_kdtree.h"
#include "../waypoint_base.h"
#include "../timer/timer_game_tick.h"
#include "../openttd.h"
#include "../rust/abi_ffi.h"
#include "../rust/station_queue_save.hpp"

extern void OnTick_Station();

TEST_CASE("Station service - scalar ABI layouts")
{
	auto layout = [](uint8_t type, std::initializer_list<size_t> fields) {
		uint8_t i = 0;
		for (size_t expected : fields) CHECK(openttd_rust_abi_layout(type, i++) == expected);
		CHECK(openttd_rust_abi_layout(type, i) == SIZE_MAX);
	};
	layout(110, {sizeof(OpenTTDStationCargo), alignof(OpenTTDStationCargo), offsetof(OpenTTDStationCargo, max_waiting_cargo), offsetof(OpenTTDStationCargo, status), offsetof(OpenTTDStationCargo, time_since_pickup), offsetof(OpenTTDStationCargo, rating), offsetof(OpenTTDStationCargo, last_speed), offsetof(OpenTTDStationCargo, last_age), offsetof(OpenTTDStationCargo, amount_fract)});
	layout(111, {sizeof(OpenTTDStationFields), alignof(OpenTTDStationFields), offsetof(OpenTTDStationFields, always_accepted), offsetof(OpenTTDStationFields, delete_ctr), offsetof(OpenTTDStationFields, time_since_load), offsetof(OpenTTDStationFields, time_since_unload), offsetof(OpenTTDStationFields, last_vehicle_type)});
	layout(112, {sizeof(OpenTTDStationServices), alignof(OpenTTDStationServices), offsetof(OpenTTDStationServices, get), offsetof(OpenTTDStationServices, owner), offsetof(OpenTTDStationServices, read), offsetof(OpenTTDStationServices, effect), offsetof(OpenTTDStationServices, rating_callback), offsetof(OpenTTDStationServices, tiles), offsetof(OpenTTDStationServices, tile_next), offsetof(OpenTTDStationServices, tile_destroy), offsetof(OpenTTDStationServices, accept_tile), offsetof(OpenTTDStationServices, truncate), offsetof(OpenTTDStationServices, truncate_next), offsetof(OpenTTDStationServices, truncate_destroy), offsetof(OpenTTDStationServices, random)});
	layout(113, {sizeof(OpenTTDStationEdge), alignof(OpenTTDStationEdge), offsetof(OpenTTDStationEdge, destination), offsetof(OpenTTDStationEdge, last_update), offsetof(OpenTTDStationEdge, unrestricted), offsetof(OpenTTDStationEdge, restricted), offsetof(OpenTTDStationEdge, distance), offsetof(OpenTTDStationEdge, node)});
	layout(114, {sizeof(OpenTTDStationLinks), alignof(OpenTTDStationLinks), offsetof(OpenTTDStationLinks, graph), offsetof(OpenTTDStationLinks, read), offsetof(OpenTTDStationLinks, edge), offsetof(OpenTTDStationLinks, effect), offsetof(OpenTTDStationLinks, order_list), offsetof(OpenTTDStationLinks, order_read), offsetof(OpenTTDStationLinks, order_vehicle), offsetof(OpenTTDStationLinks, next_vehicle), offsetof(OpenTTDStationLinks, vehicle_read), offsetof(OpenTTDStationLinks, refresh), offsetof(OpenTTDStationLinks, reroute)});
	layout(115, {sizeof(OpenTTDStationLoading), alignof(OpenTTDStationLoading), offsetof(OpenTTDStationLoading, read), offsetof(OpenTTDStationLoading, write), offsetof(OpenTTDStationLoading, next), offsetof(OpenTTDStationLoading, next_stations), offsetof(OpenTTDStationLoading, next_stations_destroy), offsetof(OpenTTDStationLoading, cargo), offsetof(OpenTTDStationLoading, load_callback), offsetof(OpenTTDStationLoading, payment), offsetof(OpenTTDStationLoading, effect), offsetof(OpenTTDStationLoading, refit)});
}
TEST_CASE("Station service - stable aliases, ordered queue and replacement")
{
	auto *owner = openttd_rust_station_service_new();
	auto *fields = openttd_rust_station_service_fields(owner);
	auto *cargo = openttd_rust_station_service_cargo(owner, 63);
	CHECK(fields->delete_ctr == 0);
	CHECK(fields->time_since_load == 255);
	CHECK(fields->time_since_unload == 255);
	CHECK(fields->last_vehicle_type == 255);
	CHECK(fields->always_accepted == 0);
	CHECK(cargo->rating == 175);
	CHECK(cargo->time_since_pickup == 255);
	CHECK(cargo->last_age == 255);
	cargo->rating = 1;
	fields->delete_ctr = 7;
	int first, second;
	for (size_t i = 0; i < 128; ++i) openttd_rust_station_queue_push(owner, &first);
	openttd_rust_station_queue_push(owner, &second);
	openttd_rust_station_queue_push(owner, &first);
	CHECK(fields == openttd_rust_station_service_fields(owner));
	CHECK(cargo == openttd_rust_station_service_cargo(owner, 63));
	CHECK(cargo->rating == 1);
	CHECK(fields->delete_ctr == 7);
	openttd_rust_station_queue_erase(owner, 0);
	CHECK(openttd_rust_station_queue_get(owner, 127) == &second);
	openttd_rust_station_queue_remove(owner, &first);
	REQUIRE(openttd_rust_station_queue_size(owner) == 1);
	CHECK(openttd_rust_station_queue_get(owner, 0) == &second);
	openttd_rust_station_queue_clear(owner);
	CHECK(openttd_rust_station_queue_size(owner) == 0);
	openttd_rust_station_service_destroy(owner);
	owner = openttd_rust_station_service_new();
	CHECK(openttd_rust_station_service_cargo(owner, 63)->rating == 175);
	CHECK(openttd_rust_station_service_fields(owner)->delete_ctr == 0);
	openttd_rust_station_service_destroy(owner);
}
TEST_CASE("Station service - loading facade keeps list node identity")
{
	std::unique_ptr<OpenTTDStationService, decltype(&openttd_rust_station_service_destroy)> owner(openttd_rust_station_service_new(), openttd_rust_station_service_destroy);
	RustStationLoadingQueue queue(owner.get());
	int first, second, third;
	auto *a = reinterpret_cast<Vehicle *>(&first);
	auto *b = reinterpret_cast<Vehicle *>(&second);
	auto *c = reinterpret_cast<Vehicle *>(&third);
	for (Vehicle *v : {a, b, a, c}) queue.push_back(v);
	auto retained = queue.begin();
	++retained;
	auto duplicate = retained;
	++duplicate;
	CHECK(duplicate != queue.begin());
	CHECK(*duplicate == a);
	CHECK(queue.erase(queue.begin()) == retained);
	CHECK(*retained == b);
	/* Preserve a preincremented iterator while removing its predecessor. */
	auto next = retained++;
	CHECK(*next == b);
	queue.remove(b);
	CHECK(retained == duplicate);
	CHECK(*retained == a);
	queue.push_back(a);
	queue.remove(a);
	REQUIRE(queue.size() == 1);
	CHECK(queue.front() == c);
	CHECK(queue.erase(queue.begin()) == queue.end());
	CHECK(queue.empty());
}

TEST_CASE("Station service - loading save staging retains partial and nested effects")
{
	std::unique_ptr<OpenTTDStationService, decltype(&openttd_rust_station_service_destroy)> owner(openttd_rust_station_service_new(), openttd_rust_station_service_destroy);
	std::unique_ptr<OpenTTDStationService, decltype(&openttd_rust_station_service_destroy)> nested_owner(openttd_rust_station_service_new(), openttd_rust_station_service_destroy);
	RustStationLoadingQueue queue(owner.get()), nested(nested_owner.get());
	int first, second;
	auto *a = reinterpret_cast<Vehicle *>(&first);
	auto *b = reinterpret_cast<Vehicle *>(&second);
	queue.push_back(a);
	queue.push_back(b);
	nested.push_back(b);
	{
		RustStationLoadingQueueSaveScope save(&queue, false);
		auto *list = static_cast<std::list<void *> *>(RustStationLoadingQueueSaveScope::Address(queue));
		CHECK(*list == std::list<void *>{a, b});
		list->clear();
	}
	CHECK(queue.size() == 2);
	/* SlRefList clears storage and appends the next default entry before
	 * reading its reference. A failure therefore retains that null entry. */
	try {
		RustStationLoadingQueueSaveScope load(&queue, true);
		auto *list = static_cast<std::list<void *> *>(RustStationLoadingQueueSaveScope::Address(queue));
		list->clear();
		list->push_back(a);
		list->emplace_back();
		throw 1;
	} catch (int) {}
	CHECK(std::vector<Vehicle *>(queue.begin(), queue.end()) == std::vector<Vehicle *>{a, nullptr});
	/* A later nested error keeps each fixed prefix and restores the outer
	 * descriptor's context before it unwinds. */
	try {
		RustStationLoadingQueueSaveScope fix(&queue, true);
		auto *list = static_cast<std::list<void *> *>(RustStationLoadingQueueSaveScope::Address(queue));
		list->front() = b;
		try {
			RustStationLoadingQueueSaveScope inner(&nested, true);
			auto *inner_list = static_cast<std::list<void *> *>(RustStationLoadingQueueSaveScope::Address(nested));
			inner_list->front() = a;
			CHECK(RustStationLoadingQueueSaveScope::Address(queue) == list);
			throw 2;
		} catch (int) {}
		CHECK(nested.front() == a);
		CHECK(RustStationLoadingQueueSaveScope::Address(queue) == list);
		throw 3;
	} catch (int) {}
	CHECK(std::vector<Vehicle *>(queue.begin(), queue.end()) == std::vector<Vehicle *>{b, nullptr});
}
TEST_CASE("Station service - indexed shells, retirement and pool reuse")
{
	MockEnvironment::Instance();
	REQUIRE(BaseStation::CanAllocateItem());
	auto *st = new Station(TileIndex(1));
	st->owner = OWNER_NONE;
	_station_kdtree.Insert(st->index);
	StationID id = st->index;
	st->goods[63].rating = 1;
	st->goods[63].amount_fract = 255;
	st->delete_ctr = 7;
	const auto old_counter = TimerGameTick::counter;
	const auto old_mode = _game_mode;
	TimerGameTick::counter = (250 - id.base() % 250) % 250;
	_game_mode = GM_NORMAL;
	OnTick_Station(); // Real empty-station expiry uses ordinary destruction.
	CHECK_FALSE(BaseStation::IsValidID(id));
	REQUIRE(BaseStation::CanAllocateItem());
	st = new Station(TileIndex(1));
	CHECK(st->index == id);
	CHECK(st->delete_ctr == 0);
	CHECK(st->goods[63].rating == 175);
	CHECK(st->goods[63].amount_fract == 0);
	CHECK(st->loading_vehicles.empty());
	CHECK(st->time_since_load == 255);
	auto *indexed = new (StationID(17)) Station(TileIndex(2));
	CHECK(indexed->index == StationID(17));
	CHECK(indexed->goods[0].time_since_pickup == 255);
	CHECK(indexed->last_vehicle_type == VEH_INVALID);
	auto *waypoint = new (StationID(19)) Waypoint(TileIndex(3));
	CHECK(waypoint->delete_ctr == 0);
	indexed->goods[0].last_speed = 255;
	_station_pool.CleanPool(); // Derived cleanup early returns still drop owners.
	RebuildStationKdtree();
	TimerGameTick::counter = old_counter;
	_game_mode = old_mode;
}
#endif
