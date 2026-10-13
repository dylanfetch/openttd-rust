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
	auto layout = [](uint16_t type, std::initializer_list<size_t> fields) {
		uint8_t i = 0;
		for (size_t expected : fields) CHECK(openttd_rust_abi_layout(type, i++) == expected);
		CHECK(openttd_rust_abi_layout(type, i) == SIZE_MAX);
	};
	layout(500, {sizeof(OpenTTDRatingCounts), alignof(OpenTTDRatingCounts), offsetof(OpenTTDRatingCounts, available), offsetof(OpenTTDRatingCounts, destinations)});
	layout(501, {sizeof(OpenTTDStationExclusivity), alignof(OpenTTDStationExclusivity), offsetof(OpenTTDStationExclusivity, counter), offsetof(OpenTTDStationExclusivity, exclusive)});
	layout(502, {sizeof(OpenTTDCapacityRemaining), alignof(OpenTTDCapacityRemaining), offsetof(OpenTTDCapacityRemaining, remaining), offsetof(OpenTTDCapacityRemaining, capacity)});
	layout(503, {sizeof(OpenTTDCargoRemaining), alignof(OpenTTDCargoRemaining), offsetof(OpenTTDCargoRemaining, remaining), offsetof(OpenTTDCargoRemaining, capacity), offsetof(OpenTTDCargoRemaining, cargo)});
	layout(504, {sizeof(OpenTTDCargoReservation), alignof(OpenTTDCargoReservation), offsetof(OpenTTDCargoReservation, mask), offsetof(OpenTTDCargoReservation, reserved), offsetof(OpenTTDCargoReservation, capacity), offsetof(OpenTTDCargoReservation, cargo)});
	layout(505, {sizeof(OpenTTDCargoStored), alignof(OpenTTDCargoStored), offsetof(OpenTTDCargoStored, stored), offsetof(OpenTTDCargoStored, capacity)});
	layout(506, {sizeof(OpenTTDDeliveryRemaining), alignof(OpenTTDDeliveryRemaining), offsetof(OpenTTDDeliveryRemaining, remaining), offsetof(OpenTTDDeliveryRemaining, deliver), offsetof(OpenTTDDeliveryRemaining, capacity)});
	layout(507, {sizeof(OpenTTDLoadEngine), alignof(OpenTTDLoadEngine), offsetof(OpenTTDLoadEngine, engine), offsetof(OpenTTDLoadEngine, amount)});
	layout(508, {sizeof(OpenTTDLoadingWait), alignof(OpenTTDLoadingWait), offsetof(OpenTTDLoadingWait, lateness), offsetof(OpenTTDLoadingWait, wait)});
	layout(509, {sizeof(OpenTTDOrderDestination), alignof(OpenTTDOrderDestination), offsetof(OpenTTDOrderDestination, destination), offsetof(OpenTTDOrderDestination, is_station)});
	layout(510, {sizeof(OpenTTDTruncatedSource), alignof(OpenTTDTruncatedSource), offsetof(OpenTTDTruncatedSource, amount), offsetof(OpenTTDTruncatedSource, source), offsetof(OpenTTDTruncatedSource, present)});
	layout(511, {sizeof(OpenTTDCargoSource), alignof(OpenTTDCargoSource), offsetof(OpenTTDCargoSource, id), offsetof(OpenTTDCargoSource, type)});
	layout(512, {sizeof(OpenTTDLoadingControl), alignof(OpenTTDLoadingControl), offsetof(OpenTTDLoadingControl, ticks), offsetof(OpenTTDLoadingControl, vehicle_type), offsetof(OpenTTDLoadingControl, unload_type), offsetof(OpenTTDLoadingControl, load_type), offsetof(OpenTTDLoadingControl, refit_cargo), offsetof(OpenTTDLoadingControl, refit), offsetof(OpenTTDLoadingControl, full_load), offsetof(OpenTTDLoadingControl, stop), offsetof(OpenTTDLoadingControl, gradual), offsetof(OpenTTDLoadingControl, improved)});
	layout(513, {sizeof(OpenTTDLoadingPart), alignof(OpenTTDLoadingPart), offsetof(OpenTTDLoadingPart, capacity), offsetof(OpenTTDLoadingPart, cargo), offsetof(OpenTTDLoadingPart, vehicle_type), offsetof(OpenTTDLoadingPart, unloading)});
	layout(514, {sizeof(OpenTTDLoadingStats), alignof(OpenTTDLoadingStats), offsetof(OpenTTDLoadingStats, stored), offsetof(OpenTTDLoadingStats, load_count), offsetof(OpenTTDLoadingStats, speed), offsetof(OpenTTDLoadingStats, age), offsetof(OpenTTDLoadingStats, capacity), offsetof(OpenTTDLoadingStats, cargo)});
	layout(110, {sizeof(OpenTTDStationCargo), alignof(OpenTTDStationCargo), offsetof(OpenTTDStationCargo, max_waiting_cargo), offsetof(OpenTTDStationCargo, status), offsetof(OpenTTDStationCargo, time_since_pickup), offsetof(OpenTTDStationCargo, rating), offsetof(OpenTTDStationCargo, last_speed), offsetof(OpenTTDStationCargo, last_age), offsetof(OpenTTDStationCargo, amount_fract)});
	layout(111, {sizeof(OpenTTDStationFields), alignof(OpenTTDStationFields), offsetof(OpenTTDStationFields, always_accepted), offsetof(OpenTTDStationFields, delete_ctr), offsetof(OpenTTDStationFields, time_since_load), offsetof(OpenTTDStationFields, time_since_unload), offsetof(OpenTTDStationFields, last_vehicle_type)});
	layout(112, {sizeof(OpenTTDStationServices), alignof(OpenTTDStationServices), offsetof(OpenTTDStationServices, base_station), offsetof(OpenTTDStationServices, station), offsetof(OpenTTDStationServices, station_owner), offsetof(OpenTTDStationServices, station_exclusivity), offsetof(OpenTTDStationServices, station_next), offsetof(OpenTTDStationServices, rating_counts), offsetof(OpenTTDStationServices, get), offsetof(OpenTTDStationServices, owner), offsetof(OpenTTDStationServices, pool_size), offsetof(OpenTTDStationServices, tick_counter), offsetof(OpenTTDStationServices, editor), offsetof(OpenTTDStationServices, facilities), offsetof(OpenTTDStationServices, in_use), offsetof(OpenTTDStationServices, is_station), offsetof(OpenTTDStationServices, local_owner), offsetof(OpenTTDStationServices, rectangle_empty), offsetof(OpenTTDStationServices, town_statue), offsetof(OpenTTDStationServices, select_goods), offsetof(OpenTTDStationServices, rating_cheat), offsetof(OpenTTDStationServices, available), offsetof(OpenTTDStationServices, cargo_valid), offsetof(OpenTTDStationServices, rating_callback_enabled), offsetof(OpenTTDStationServices, passenger_cargo), offsetof(OpenTTDStationServices, packet_available), offsetof(OpenTTDStationServices, rating_redraw), offsetof(OpenTTDStationServices, set_demand), offsetof(OpenTTDStationServices, acceptance_news), offsetof(OpenTTDStationServices, acceptance_redraw), offsetof(OpenTTDStationServices, delete_station), offsetof(OpenTTDStationServices, acceptance_animation), offsetof(OpenTTDStationServices, watched_animation), offsetof(OpenTTDStationServices, append_packet), offsetof(OpenTTDStationServices, update_supply), offsetof(OpenTTDStationServices, station_list_invalidation), offsetof(OpenTTDStationServices, new_cargo_animation), offsetof(OpenTTDStationServices, waiting_redraw), offsetof(OpenTTDStationServices, rating_callback), offsetof(OpenTTDStationServices, with_tiles), offsetof(OpenTTDStationServices, tile_next), offsetof(OpenTTDStationServices, accept_tile), offsetof(OpenTTDStationServices, acceptance_amounts), offsetof(OpenTTDStationServices, with_truncation), offsetof(OpenTTDStationServices, truncate_next), offsetof(OpenTTDStationServices, shared)});
	layout(113, {sizeof(OpenTTDStationEdge), alignof(OpenTTDStationEdge), offsetof(OpenTTDStationEdge, destination), offsetof(OpenTTDStationEdge, last_update), offsetof(OpenTTDStationEdge, distance)});
	layout(114, {sizeof(OpenTTDStationLinks), alignof(OpenTTDStationLinks), offsetof(OpenTTDStationLinks, graph), offsetof(OpenTTDStationLinks, automatic_distribution), offsetof(OpenTTDStationLinks, node), offsetof(OpenTTDStationLinks, edge_count), offsetof(OpenTTDStationLinks, date), offsetof(OpenTTDStationLinks, station_id), offsetof(OpenTTDStationLinks, edge_last_update), offsetof(OpenTTDStationLinks, last_compression), offsetof(OpenTTDStationLinks, edge), offsetof(OpenTTDStationLinks, edge_unrestricted), offsetof(OpenTTDStationLinks, edge_restricted), offsetof(OpenTTDStationLinks, delete_flows), offsetof(OpenTTDStationLinks, restrict_edge), offsetof(OpenTTDStationLinks, restrict_flows), offsetof(OpenTTDStationLinks, release_edge), offsetof(OpenTTDStationLinks, remove_edge), offsetof(OpenTTDStationLinks, compress), offsetof(OpenTTDStationLinks, order_list), offsetof(OpenTTDStationLinks, order_pool_size), offsetof(OpenTTDStationLinks, order_count), offsetof(OpenTTDStationLinks, order_destination), offsetof(OpenTTDStationLinks, order_vehicle), offsetof(OpenTTDStationLinks, next_vehicle), offsetof(OpenTTDStationLinks, next_shared), offsetof(OpenTTDStationLinks, stopped_in_depot), offsetof(OpenTTDStationLinks, last_service), offsetof(OpenTTDStationLinks, cargo_type), offsetof(OpenTTDStationLinks, refresh), offsetof(OpenTTDStationLinks, reroute)});
	layout(115, {sizeof(OpenTTDStationLoading), alignof(OpenTTDStationLoading), offsetof(OpenTTDStationLoading, control), offsetof(OpenTTDStationLoading, part), offsetof(OpenTTDStationLoading, set_refit_capacity_and_stats), offsetof(OpenTTDStationLoading, wait_observation), offsetof(OpenTTDStationLoading, load_engine), offsetof(OpenTTDStationLoading, grf_version), offsetof(OpenTTDStationLoading, engine_load_callback), offsetof(OpenTTDStationLoading, engine_no_multiplier), offsetof(OpenTTDStationLoading, delivery_remaining), offsetof(OpenTTDStationLoading, cargo_stored), offsetof(OpenTTDStationLoading, cargo_reservation), offsetof(OpenTTDStationLoading, cargo_remaining), offsetof(OpenTTDStationLoading, capacity_remaining), offsetof(OpenTTDStationLoading, vehicle_type), offsetof(OpenTTDStationLoading, cargo_type), offsetof(OpenTTDStationLoading, cargo_cap), offsetof(OpenTTDStationLoading, stored_count), offsetof(OpenTTDStationLoading, unload_count), offsetof(OpenTTDStationLoading, deliver_count), offsetof(OpenTTDStationLoading, transfer_count), offsetof(OpenTTDStationLoading, load_count), offsetof(OpenTTDStationLoading, unload_type), offsetof(OpenTTDStationLoading, has_orders), offsetof(OpenTTDStationLoading, load_unload_ticks), offsetof(OpenTTDStationLoading, status), offsetof(OpenTTDStationLoading, has_articulated_part), offsetof(OpenTTDStationLoading, is_articulated_part), offsetof(OpenTTDStationLoading, is_rear_dualheaded), offsetof(OpenTTDStationLoading, is_normal_aircraft), offsetof(OpenTTDStationLoading, is_multiheaded), offsetof(OpenTTDStationLoading, cargo_multiplier), offsetof(OpenTTDStationLoading, cached_total_length), offsetof(OpenTTDStationLoading, full_load_order), offsetof(OpenTTDStationLoading, vehicle_owner), offsetof(OpenTTDStationLoading, passenger_cargo), offsetof(OpenTTDStationLoading, station_length_penalty), offsetof(OpenTTDStationLoading, current_company), offsetof(OpenTTDStationLoading, last_station_visited), offsetof(OpenTTDStationLoading, total_count), offsetof(OpenTTDStationLoading, order_time), offsetof(OpenTTDStationLoading, refit_mask), offsetof(OpenTTDStationLoading, set_cargo_unloading), offsetof(OpenTTDStationLoading, set_loading_finished), offsetof(OpenTTDStationLoading, set_stop), offsetof(OpenTTDStationLoading, set_load_ticks), offsetof(OpenTTDStationLoading, set_speed), offsetof(OpenTTDStationLoading, subtract_profit), offsetof(OpenTTDStationLoading, unknown_load_result), offsetof(OpenTTDStationLoading, refresh_capacity), offsetof(OpenTTDStationLoading, vehicle_dirty), offsetof(OpenTTDStationLoading, update_indicator), offsetof(OpenTTDStationLoading, set_current_company), offsetof(OpenTTDStationLoading, next_vehicle), offsetof(OpenTTDStationLoading, next_articulated), offsetof(OpenTTDStationLoading, other_multiheaded), offsetof(OpenTTDStationLoading, first_engine_part), offsetof(OpenTTDStationLoading, first_vehicle), offsetof(OpenTTDStationLoading, previous_vehicle), offsetof(OpenTTDStationLoading, with_next_stations), offsetof(OpenTTDStationLoading, stage), offsetof(OpenTTDStationLoading, return_cargo), offsetof(OpenTTDStationLoading, unload_cargo), offsetof(OpenTTDStationLoading, load_cargo), offsetof(OpenTTDStationLoading, reserve_cargo), offsetof(OpenTTDStationLoading, deliver_to_transfer), offsetof(OpenTTDStationLoading, deliver_to_keep), offsetof(OpenTTDStationLoading, has_cargo_for), offsetof(OpenTTDStationLoading, platform_length), offsetof(OpenTTDStationLoading, vehicle_tile), offsetof(OpenTTDStationLoading, station_index), offsetof(OpenTTDStationLoading, station_cargo_total), offsetof(OpenTTDStationLoading, load_property), offsetof(OpenTTDStationLoading, load_callback), offsetof(OpenTTDStationLoading, get_payment), offsetof(OpenTTDStationLoading, create_payment), offsetof(OpenTTDStationLoading, delete_payment), offsetof(OpenTTDStationLoading, flush_delivery), offsetof(OpenTTDStationLoading, vehicle_new_cargo), offsetof(OpenTTDStationLoading, cargo_taken), offsetof(OpenTTDStationLoading, train_loads), offsetof(OpenTTDStationLoading, road_loads), offsetof(OpenTTDStationLoading, vehicle_empty), offsetof(OpenTTDStationLoading, station_dirty), offsetof(OpenTTDStationLoading, test_refit), offsetof(OpenTTDStationLoading, execute_refit)});
}
struct StationLoadingProbe {
	OpenTTDStationService *owner;
	int vehicle, payment;
	uint8_t phase = 0;
	bool frame_live = false;
};
static StationLoadingProbe *_station_loading_probe;

TEST_CASE("Station service - direct prepare preserves native frames and same-owner reentry")
{
	std::unique_ptr<OpenTTDStationService, decltype(&openttd_rust_station_service_destroy)> owner(openttd_rust_station_service_new(), openttd_rust_station_service_destroy);
	StationLoadingProbe probe{owner.get(), 0, 0};
	_station_loading_probe = &probe;
	auto *cargo = openttd_rust_station_service_cargo(owner.get(), 0);
	cargo->status = 2;
	cargo->rating = 1;
	const OpenTTDStationServices services = {
		.base_station = [](OpenTTDStation *st) noexcept -> OpenTTDBaseStation * { CHECK(static_cast<void *>(st) == _station_loading_probe); return reinterpret_cast<OpenTTDBaseStation *>(_station_loading_probe); },
		.station = [](uint16_t id) noexcept -> OpenTTDStation * { CHECK(id == 0); return reinterpret_cast<OpenTTDStation *>(_station_loading_probe); },
		.station_owner = nullptr,
		.station_exclusivity = nullptr,
		.station_next = nullptr,
		.rating_counts = nullptr,
		.get = nullptr,
		.owner = [](OpenTTDBaseStation *st) noexcept { CHECK(static_cast<void *>(st) == _station_loading_probe); return _station_loading_probe->owner; },
		.pool_size = nullptr,
		.tick_counter = nullptr,
		.editor = nullptr,
		.facilities = nullptr,
		.in_use = nullptr,
		.is_station = nullptr,
		.local_owner = nullptr,
		.rectangle_empty = nullptr,
		.town_statue = nullptr,
		.select_goods = nullptr,
		.rating_cheat = nullptr,
		.available = nullptr,
		.cargo_valid = nullptr,
		.rating_callback_enabled = nullptr,
		.passenger_cargo = nullptr,
		.packet_available = nullptr,
		.rating_redraw = nullptr,
		.set_demand = nullptr,
		.acceptance_news = nullptr,
		.acceptance_redraw = nullptr,
		.delete_station = nullptr,
		.acceptance_animation = nullptr,
		.watched_animation = nullptr,
		.append_packet = nullptr,
		.update_supply = nullptr,
		.station_list_invalidation = nullptr,
		.new_cargo_animation = nullptr,
		.waiting_redraw = nullptr,
		.rating_callback = nullptr,
		.with_tiles = nullptr,
		.tile_next = nullptr,
		.accept_tile = nullptr,
		.acceptance_amounts = nullptr,
		.with_truncation = nullptr,
		.truncate_next = nullptr,
		.shared = nullptr,
	};
	const OpenTTDStationLoading loading = {
		.control = nullptr,
		.part = nullptr,
		.set_refit_capacity_and_stats = nullptr,
		.wait_observation = nullptr,
		.load_engine = nullptr,
		.grf_version = nullptr,
		.engine_load_callback = nullptr,
		.engine_no_multiplier = nullptr,
		.delivery_remaining = nullptr,
		.cargo_stored = nullptr,
		.cargo_reservation = nullptr,
		.cargo_remaining = nullptr,
		.capacity_remaining = nullptr,
		.vehicle_type = nullptr,
		.cargo_type = nullptr,
		.cargo_cap = [](OpenTTDVehicle *) noexcept -> uint16_t { CHECK(_station_loading_probe->phase++ == 5); return 1; },
		.stored_count = nullptr,
		.unload_count = [](OpenTTDVehicle *) noexcept -> uint32_t { CHECK(_station_loading_probe->phase++ == 9); return 1; },
		.deliver_count = nullptr,
		.transfer_count = nullptr,
		.load_count = nullptr,
		.unload_type = [](OpenTTDVehicle *) noexcept -> uint8_t { CHECK(_station_loading_probe->phase++ == 7); return 0; },
		.has_orders = [](OpenTTDVehicle *) noexcept { CHECK(_station_loading_probe->phase++ == 4); return false; },
		.load_unload_ticks = nullptr,
		.status = nullptr,
		.has_articulated_part = nullptr,
		.is_articulated_part = nullptr,
		.is_rear_dualheaded = nullptr,
		.is_normal_aircraft = nullptr,
		.is_multiheaded = nullptr,
		.cargo_multiplier = nullptr,
		.cached_total_length = nullptr,
		.full_load_order = nullptr,
		.vehicle_owner = nullptr,
		.passenger_cargo = nullptr,
		.station_length_penalty = nullptr,
		.current_company = nullptr,
		.last_station_visited = [](OpenTTDVehicle *) noexcept -> uint16_t { return 0; },
		.total_count = [](OpenTTDVehicle *) noexcept -> uint32_t { CHECK(_station_loading_probe->phase++ == 6); return 1; },
		.order_time = nullptr,
		.refit_mask = nullptr,
		.set_cargo_unloading = [](OpenTTDVehicle *, bool value) noexcept { CHECK(_station_loading_probe->phase++ == 10); CHECK(value); },
		.set_loading_finished = [](OpenTTDVehicle *, bool value) noexcept { CHECK(_station_loading_probe->phase++ == 0); CHECK_FALSE(value); },
		.set_stop = nullptr,
		.set_load_ticks = [](OpenTTDVehicle *, uint16_t value) noexcept { CHECK(_station_loading_probe->phase++ == 1); CHECK(value == 1); },
		.set_speed = nullptr,
		.subtract_profit = nullptr,
		.unknown_load_result = nullptr,
		.refresh_capacity = nullptr,
		.vehicle_dirty = nullptr,
		.update_indicator = nullptr,
		.set_current_company = nullptr,
		.next_vehicle = [](OpenTTDVehicle *) noexcept -> OpenTTDVehicle * { CHECK(_station_loading_probe->phase++ == 11); return nullptr; },
		.next_articulated = nullptr,
		.other_multiheaded = nullptr,
		.first_engine_part = nullptr,
		.first_vehicle = nullptr,
		.previous_vehicle = nullptr,
		.with_next_stations = [](OpenTTDVehicle *, void *context, OpenTTDStationScope body) noexcept {
			CHECK(_station_loading_probe->phase++ == 3);
			int destination = 42;
			_station_loading_probe->frame_live = true;
			body(context, &destination);
			CHECK(_station_loading_probe->phase++ == 12);
			CHECK(destination == 42);
			_station_loading_probe->frame_live = false;
		},
		.stage = [](OpenTTDStation *st, OpenTTDVehicle *, uint8_t unload, void *next, OpenTTDNativeCargoPayment *payment) noexcept {
			CHECK(_station_loading_probe->phase++ == 8);
			CHECK(static_cast<void *>(st) == _station_loading_probe);
			CHECK(_station_loading_probe->frame_live);
			CHECK(*static_cast<int *>(next) == 42);
			CHECK(static_cast<void *>(payment) == &_station_loading_probe->payment);
			CHECK(unload == 0);
			CHECK(openttd_rust_station_queue_size(_station_loading_probe->owner) == 1);
			openttd_rust_station_modify_rating(_station_loading_probe->owner, 5);
			CHECK(openttd_rust_station_service_cargo(_station_loading_probe->owner, 0)->rating == 6);
		},
		.return_cargo = nullptr,
		.unload_cargo = nullptr,
		.load_cargo = nullptr,
		.reserve_cargo = nullptr,
		.deliver_to_transfer = nullptr,
		.deliver_to_keep = nullptr,
		.has_cargo_for = nullptr,
		.platform_length = nullptr,
		.vehicle_tile = nullptr,
		.station_index = nullptr,
		.station_cargo_total = nullptr,
		.load_property = nullptr,
		.load_callback = nullptr,
		.get_payment = nullptr,
		.create_payment = [](OpenTTDVehicle *) noexcept -> OpenTTDNativeCargoPayment * { CHECK(_station_loading_probe->phase++ == 2); return reinterpret_cast<OpenTTDNativeCargoPayment *>(&_station_loading_probe->payment); },
		.delete_payment = nullptr,
		.flush_delivery = nullptr,
		.vehicle_new_cargo = nullptr,
		.cargo_taken = nullptr,
		.train_loads = nullptr,
		.road_loads = nullptr,
		.vehicle_empty = nullptr,
		.station_dirty = nullptr,
		.test_refit = nullptr,
		.execute_refit = nullptr,
	};
	openttd_rust_station_prepare(reinterpret_cast<OpenTTDVehicle *>(&probe.vehicle), &services, &loading);
	CHECK(probe.phase == 13);
	CHECK_FALSE(probe.frame_live);
	CHECK(cargo->rating == 6);
	CHECK(openttd_rust_station_queue_get(owner.get(), 0) == &probe.vehicle);
	_station_loading_probe = nullptr;
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
