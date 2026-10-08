/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file train_reservation.cpp Controller reservation ABI layout checks. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#ifdef WITH_RUST
#include "../rust/abi_ffi.h"
#include "../rust/train_reservation_ffi.h"
TEST_CASE("Train controller reservation Rust ABI layouts")
{
	const auto Layout = [](uint8_t kind, std::initializer_list<size_t> fields) {
		uint8_t index = 0;
		for (size_t field : fields) CHECK(openttd_rust_abi_layout(kind, index++) == field);
	};
	Layout(160, {sizeof(OpenTTDTrainReservationView), alignof(OpenTTDTrainReservationView), offsetof(OpenTTDTrainReservationView, tile), offsetof(OpenTTDTrainReservationView, dest), offsetof(OpenTTDTrainReservationView, next), offsetof(OpenTTDTrainReservationView, destination), offsetof(OpenTTDTrainReservationView, last_station), offsetof(OpenTTDTrainReservationView, direction), offsetof(OpenTTDTrainReservationView, order), offsetof(OpenTTDTrainReservationView, num_orders), offsetof(OpenTTDTrainReservationView, order_index), offsetof(OpenTTDTrainReservationView, suppress), offsetof(OpenTTDTrainReservationView, nearest)});
	Layout(161, {sizeof(OpenTTDTrainReservationFollow), alignof(OpenTTDTrainReservationFollow), offsetof(OpenTTDTrainReservationFollow, old_tile), offsetof(OpenTTDTrainReservationFollow, new_tile), offsetof(OpenTTDTrainReservationFollow, skipped), offsetof(OpenTTDTrainReservationFollow, dirs), offsetof(OpenTTDTrainReservationFollow, old_td), offsetof(OpenTTDTrainReservationFollow, exitdir), offsetof(OpenTTDTrainReservationFollow, tunnel), offsetof(OpenTTDTrainReservationFollow, bridge), offsetof(OpenTTDTrainReservationFollow, station), offsetof(OpenTTDTrainReservationFollow, error)});
	Layout(162, {sizeof(OpenTTDTrainReservationPbs), alignof(OpenTTDTrainReservationPbs), offsetof(OpenTTDTrainReservationPbs, tile), offsetof(OpenTTDTrainReservationPbs, other), offsetof(OpenTTDTrainReservationPbs, td), offsetof(OpenTTDTrainReservationPbs, okay)});
	Layout(163, {sizeof(OpenTTDTrainReservationStep), alignof(OpenTTDTrainReservationStep), offsetof(OpenTTDTrainReservationStep, value), offsetof(OpenTTDTrainReservationStep, action), offsetof(OpenTTDTrainReservationStep, id), offsetof(OpenTTDTrainReservationStep, tile), offsetof(OpenTTDTrainReservationStep, final_dest), offsetof(OpenTTDTrainReservationStep, td), offsetof(OpenTTDTrainReservationStep, dir), offsetof(OpenTTDTrainReservationStep, tracks), offsetof(OpenTTDTrainReservationStep, reserve), offsetof(OpenTTDTrainReservationStep, found), offsetof(OpenTTDTrainReservationStep, got), offsetof(OpenTTDTrainReservationStep, okay)});
	Layout(164, {sizeof(OpenTTDTrainReservationLeaves), alignof(OpenTTDTrainReservationLeaves), offsetof(OpenTTDTrainReservationLeaves, observe), offsetof(OpenTTDTrainReservationLeaves, leaf), offsetof(OpenTTDTrainReservationLeaves, owner), offsetof(OpenTTDTrainReservationLeaves, follow), offsetof(OpenTTDTrainReservationLeaves, origin)});
}
#endif
