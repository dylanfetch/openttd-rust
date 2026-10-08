/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file train.cpp Train controller ABI layouts. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#ifdef WITH_RUST
#include "../rust/abi_ffi.h"
#include "../rust/train_ffi.h"
TEST_CASE("Train controller Rust ABI layouts")
{
	const auto Layout = [](uint8_t kind, std::initializer_list<size_t> fields) {
		uint8_t index = 0;
		for (size_t field : fields) CHECK(openttd_rust_abi_layout(kind, index++) == field);
	};
	Layout(151, {sizeof(OpenTTDTrainView), alignof(OpenTTDTrainView), offsetof(OpenTTDTrainView, id), offsetof(OpenTTDTrainView, first), offsetof(OpenTTDTrainView, next), offsetof(OpenTTDTrainView, previous), offsetof(OpenTTDTrainView, next_unit), offsetof(OpenTTDTrainView, last), offsetof(OpenTTDTrainView, tile), offsetof(OpenTTDTrainView, dest), offsetof(OpenTTDTrainView, x), offsetof(OpenTTDTrainView, y), offsetof(OpenTTDTrainView, z), offsetof(OpenTTDTrainView, order_time), offsetof(OpenTTDTrainView, power), offsetof(OpenTTDTrainView, weight), offsetof(OpenTTDTrainView, length), offsetof(OpenTTDTrainView, total_length), offsetof(OpenTTDTrainView, max_speed), offsetof(OpenTTDTrainView, max_track_speed), offsetof(OpenTTDTrainView, speed), offsetof(OpenTTDTrainView, gv_flags), offsetof(OpenTTDTrainView, cargo_cap), offsetof(OpenTTDTrainView, refit_cap), offsetof(OpenTTDTrainView, engine), offsetof(OpenTTDTrainView, first_engine), offsetof(OpenTTDTrainView, order_destination), offsetof(OpenTTDTrainView, last_station), offsetof(OpenTTDTrainView, direction), offsetof(OpenTTDTrainView, status), offsetof(OpenTTDTrainView, tick), offsetof(OpenTTDTrainView, running), offsetof(OpenTTDTrainView, day), offsetof(OpenTTDTrainView, progress), offsetof(OpenTTDTrainView, subspeed), offsetof(OpenTTDTrainView, acceleration), offsetof(OpenTTDTrainView, order), offsetof(OpenTTDTrainView, nonstop), offsetof(OpenTTDTrainView, breakdown), offsetof(OpenTTDTrainView, front), offsetof(OpenTTDTrainView, free_wagon), offsetof(OpenTTDTrainView, articulated), offsetof(OpenTTDTrainView, engine_part), offsetof(OpenTTDTrainView, multiheaded), offsetof(OpenTTDTrainView, owner), offsetof(OpenTTDTrainView, vis_effect)});
	Layout(152, {sizeof(OpenTTDTrainServices), alignof(OpenTTDTrainServices), offsetof(OpenTTDTrainServices, observe), offsetof(OpenTTDTrainServices, write), offsetof(OpenTTDTrainServices, leaf), offsetof(OpenTTDTrainServices, owner), offsetof(OpenTTDTrainServices, nearby)});
	Layout(153, {sizeof(OpenTTDTrainAction), alignof(OpenTTDTrainAction), offsetof(OpenTTDTrainAction, op), offsetof(OpenTTDTrainAction, id), offsetof(OpenTTDTrainAction, a), offsetof(OpenTTDTrainAction, b), offsetof(OpenTTDTrainAction, c)});
}
#endif
