/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file signal.cpp Native layout checks for the signal owner. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#ifdef WITH_RUST
#include "../rust/abi_ffi.h"
#include "../rust/signal_ffi.h"

TEST_CASE("Signal - native table layouts")
{
	auto layout = [](uint16_t type, std::initializer_list<size_t> fields) {
		uint8_t item = 0;
		for (size_t expected : fields) CHECK(openttd_rust_abi_layout(type, item++) == expected);
		CHECK(openttd_rust_abi_layout(type, item) == SIZE_MAX);
	};
	layout(820, {sizeof(OpenTTDSignalServices), alignof(OpenTTDSignalServices), offsetof(OpenTTDSignalServices, assertions), offsetof(OpenTTDSignalServices, assert_condition), offsetof(OpenTTDSignalServices, assert_wormhole_start), offsetof(OpenTTDSignalServices, train_on_tile), offsetof(OpenTTDSignalServices, train_on_tracks), offsetof(OpenTTDSignalServices, overflow_tbu), offsetof(OpenTTDSignalServices, overflow_tbd), offsetof(OpenTTDSignalServices, overflow_global), offsetof(OpenTTDSignalServices, assert_company)});
	layout(821, {sizeof(OpenTTDRailMapServices), alignof(OpenTTDRailMapServices), offsetof(OpenTTDRailMapServices, traversal), offsetof(OpenTTDRailMapServices, signal_presence), offsetof(OpenTTDRailMapServices, signal_update), offsetof(OpenTTDRailMapServices, station_admission), offsetof(OpenTTDRailMapServices, crossing_admission), offsetof(OpenTTDRailMapServices, wormhole_admission), offsetof(OpenTTDRailMapServices, is_rail_depot), offsetof(OpenTTDRailMapServices, rail_depot_direction), offsetof(OpenTTDRailMapServices, rail_tracks), offsetof(OpenTTDRailMapServices, has_signals), offsetof(OpenTTDRailMapServices, has_signal_track), offsetof(OpenTTDRailMapServices, signal_type), offsetof(OpenTTDRailMapServices, has_signal_trackdir), offsetof(OpenTTDRailMapServices, signal_state), offsetof(OpenTTDRailMapServices, set_signal_state), offsetof(OpenTTDRailMapServices, other_tunnel_bridge_end), offsetof(OpenTTDRailMapServices, rail_track_status)});
	layout(822, {sizeof(OpenTTDSignalSnapshot), alignof(OpenTTDSignalSnapshot), offsetof(OpenTTDSignalSnapshot, tbu_count), offsetof(OpenTTDSignalSnapshot, tbd_count), offsetof(OpenTTDSignalSnapshot, global_count), offsetof(OpenTTDSignalSnapshot, overflow), offsetof(OpenTTDSignalSnapshot, last_owner), offsetof(OpenTTDSignalSnapshot, tbu), offsetof(OpenTTDSignalSnapshot, tbd), offsetof(OpenTTDSignalSnapshot, global)});
	layout(823, {sizeof(OpenTTDSignalItem), alignof(OpenTTDSignalItem), offsetof(OpenTTDSignalItem, tile), offsetof(OpenTTDSignalItem, dir)});
	layout(824, {sizeof(OpenTTDRailTraversal), alignof(OpenTTDRailTraversal), offsetof(OpenTTDRailTraversal, depot), offsetof(OpenTTDRailTraversal, tracks)});
	layout(825, {sizeof(OpenTTDRailSignalPresence), alignof(OpenTTDRailSignalPresence), offsetof(OpenTTDRailSignalPresence, signals), offsetof(OpenTTDRailSignalPresence, track), offsetof(OpenTTDRailSignalPresence, kind), offsetof(OpenTTDRailSignalPresence, forward), offsetof(OpenTTDRailSignalPresence, reverse)});
	layout(826, {sizeof(OpenTTDRailSignalUpdate), alignof(OpenTTDRailSignalUpdate), offsetof(OpenTTDRailSignalUpdate, kind), offsetof(OpenTTDRailSignalUpdate, reservations)});
	layout(827, {sizeof(OpenTTDRailStationAdmission), alignof(OpenTTDRailStationAdmission), offsetof(OpenTTDRailStationAdmission, rail), offsetof(OpenTTDRailStationAdmission, owner), offsetof(OpenTTDRailStationAdmission, axis), offsetof(OpenTTDRailStationAdmission, blocked)});
	layout(828, {sizeof(OpenTTDRailCrossingAdmission), alignof(OpenTTDRailCrossingAdmission), offsetof(OpenTTDRailCrossingAdmission, crossing), offsetof(OpenTTDRailCrossingAdmission, owner), offsetof(OpenTTDRailCrossingAdmission, axis)});
	layout(829, {sizeof(OpenTTDRailWormholeAdmission), alignof(OpenTTDRailWormholeAdmission), offsetof(OpenTTDRailWormholeAdmission, owner), offsetof(OpenTTDRailWormholeAdmission, transport), offsetof(OpenTTDRailWormholeAdmission, direction)});
	layout(830, {sizeof(OpenTTDSignalContext), alignof(OpenTTDSignalContext), offsetof(OpenTTDSignalContext, shared), offsetof(OpenTTDSignalContext, rail), offsetof(OpenTTDSignalContext, native), offsetof(OpenTTDSignalContext, log_x)});
	layout(831, {sizeof(OpenTTDSignalSetTrace), alignof(OpenTTDSignalSetTrace), offsetof(OpenTTDSignalSetTrace, duplicate_count), offsetof(OpenTTDSignalSetTrace, duplicate_pop), offsetof(OpenTTDSignalSetTrace, full_failed), offsetof(OpenTTDSignalSetTrace, overflow_full), offsetof(OpenTTDSignalSetTrace, first_after_remove), offsetof(OpenTTDSignalSetTrace, refill_succeeded), offsetof(OpenTTDSignalSetTrace, refill_pop), offsetof(OpenTTDSignalSetTrace, drained_count), offsetof(OpenTTDSignalSetTrace, overflow_drained), offsetof(OpenTTDSignalSetTrace, overflow_reset)});
}
#endif /* WITH_RUST */
