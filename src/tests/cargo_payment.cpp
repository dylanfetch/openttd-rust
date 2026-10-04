/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file cargo_payment.cpp Payment lifetime and native serialization ABI. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../core/overflowsafe_type.hpp"
#ifdef WITH_RUST
#include "cargo_payment_stubs.hpp"
#include "../rust/abi_ffi.h"
#endif
#include "../safeguards.h"

#ifdef WITH_RUST
static std::vector<std::tuple<uint8_t, int64_t, int64_t>> settlement;
TEST_CASE("Cargo payment - settlement order, Money boundaries and cleanup")
{
	OpenTTDCargoServices s = CargoTestServices();
	s.settle = [](void *, uint8_t op, int64_t first, int64_t second) noexcept -> uint32_t {
		settlement.emplace_back(op, first, second);
		return op == 1 ? 17 : op == 4 ? 1 : 0;
	};
	for (bool cleaning : {false, true}) for (bool transfer : {false, true}) {
		settlement.clear();
		int front;
		auto *p = openttd_rust_cargo_payment_new(&front, 42);
		OpenTTDCargoPaymentFields fields{&front, INT64_MIN, INT64_MIN, transfer ? INT64_MAX : 0};
		openttd_rust_cargo_payment_import(p, &fields);
		CHECK(openttd_rust_cargo_payment_front(p) == &front);
		openttd_rust_cargo_payment_destroy(p, &s, cleaning);
		if (cleaning) { CHECK(settlement.empty()); continue; }
		REQUIRE(settlement.size() == 9);
		std::vector<uint8_t> order;
		for (auto [op, first, second] : settlement) order.push_back(op);
		CHECK(order == std::vector<uint8_t>{0, 1, 2, 3, 4, 8, 9, uint8_t(transfer ? 5 : 6), 7});
		CHECK(std::get<1>(settlement[2]) == INT64_MAX); // saturated -route
		CHECK(std::get<1>(settlement[3]) == (transfer ? -256 : 0)); // native << 8
		CHECK(std::get<2>(settlement[7]) == INT64_MAX); // saturated -visual
		CHECK(std::get<1>(settlement[8]) == 17); // previous current company
	}
	settlement.clear();
	auto *p = openttd_rust_cargo_payment_new(nullptr, 0xFFFF);
	OpenTTDCargoPaymentFields fields{nullptr, INT64_MAX, 0, 0};
	openttd_rust_cargo_payment_import(p, &fields);
	openttd_rust_cargo_payment_destroy(p, &s, 0);
	CHECK(settlement == std::vector<std::tuple<uint8_t, int64_t, int64_t>>{{0, 0, 0}});
}

TEST_CASE("Cargo payment - scalar ABI layouts")
{
	auto layout = [](uint8_t type, std::initializer_list<size_t> fields) {
		uint8_t i = 0;
		for (size_t expected : fields) CHECK(openttd_rust_abi_layout(type, i++) == expected);
		CHECK(openttd_rust_abi_layout(type, i) == SIZE_MAX);
	};
	layout(49, {sizeof(OpenTTDCargoSpec), alignof(OpenTTDCargoSpec), offsetof(OpenTTDCargoSpec, payment), offsetof(OpenTTDCargoSpec, valid), offsetof(OpenTTDCargoSpec, callback), offsetof(OpenTTDCargoSpec, periods1), offsetof(OpenTTDCargoSpec, periods2)});
	layout(50, {sizeof(OpenTTDCargoPaymentFields), alignof(OpenTTDCargoPaymentFields), offsetof(OpenTTDCargoPaymentFields, front), offsetof(OpenTTDCargoPaymentFields, route_profit), offsetof(OpenTTDCargoPaymentFields, visual_profit), offsetof(OpenTTDCargoPaymentFields, visual_transfer)});
	layout(51, {sizeof(OpenTTDCargoServices), alignof(OpenTTDCargoServices), offsetof(OpenTTDCargoServices, spec), offsetof(OpenTTDCargoServices, callback), offsetof(OpenTTDCargoServices, near), offsetof(OpenTTDCargoServices, station_read), offsetof(OpenTTDCargoServices, industry_read), offsetof(OpenTTDCargoServices, industry_write), offsetof(OpenTTDCargoServices, refuses), offsetof(OpenTTDCargoServices, accept), offsetof(OpenTTDCargoServices, statistics), offsetof(OpenTTDCargoServices, monitor), offsetof(OpenTTDCargoServices, subsidised), offsetof(OpenTTDCargoServices, industry_effect), offsetof(OpenTTDCargoServices, vehicle_read), offsetof(OpenTTDCargoServices, settle), offsetof(OpenTTDCargoServices, feeder), offsetof(OpenTTDCargoServices, setting)});
}
#endif
