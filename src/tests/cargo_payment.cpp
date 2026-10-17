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
		p->fields = {&front, INT64_MIN, INT64_MIN, transfer ? INT64_MAX : 0};
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
	p->fields = {nullptr, INT64_MAX, 0, 0};
	openttd_rust_cargo_payment_destroy(p, &s, 0);
	CHECK(settlement == std::vector<std::tuple<uint8_t, int64_t, int64_t>>{{0, 0, 0}});
}
#endif
