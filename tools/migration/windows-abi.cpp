/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file windows-abi.cpp Bounded first-32-bit layout and real C ABI call checks. */
#include "rust/abi_ffi.h"
#include "rust/water_regions_ffi.h"
#include "rust/cargo_payment_ffi.h"
#include "rust/cargo_storage_ffi.h"
#include "rust/cargo_flow_ffi.h"
#include "rust/ship_yapf_ffi.h"
#include "rust/rail_yapf_ffi.h"
#include "rust/train_ffi.h"
#include "rust/train_reservation_ffi.h"
#include "rust/linkgraph_ffi.h"
#include "rust/trees_ffi.h"
#include "rust/disaster_ffi.h"
#include "rust/aircraft_ffi.h"
#include "aircraft-boundary.hpp"
#include "rust/townname_ffi.h"
#include "rust/effect_ffi.h"
#include "rust/road_ffi.h"
#include "rust/ship_control_ffi.h"
#include "rust/orders_ffi.h"
#include "rust/road_yapf_ffi.h"
#include "tests/effect_protocol.hpp"
#include "tests/water_regions_protocol.hpp"
#include "tests/ship_yapf_protocol.hpp"
#include "tests/town_protocol.hpp"
#include "rust/ffi.h"
#include "rust/utf8_ffi.h"
#include "rust/builder_ffi.h"
#include "rust/alternating_ffi.h"
#include "rust/consumer_ffi.h"
#include "rust/encoded_ffi.h"
#include "rust/spiral_ffi.h"
#include "rust/byte_strings_ffi.h"
#include "rust/history_ffi.h"
#include "rust/math_ffi.h"
#include "rust/station_cargo_ffi.h"
#include "rust/script_list_ffi.h"
#include "rust/packet_ffi.h"
#include "rust/string_validation_ffi.h"
#include "rust/crypto_primitives_ffi.h"
#include "rust/blake2b_ffi.h"
#include "rust/x25519_ffi.h"
#include "3rdparty/monocypher/monocypher.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <stdexcept>
#include <type_traits>
#include "misc/history_type.hpp"

#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (0)

static void Layout(uint16_t type, const char *name, std::initializer_list<size_t> values)
{
	uint8_t item = 0;
	std::printf("layout %s", name);
	for (size_t expected : values) {
		size_t actual = openttd_rust_abi_layout(type, item++);
		CHECK(actual == expected);
		std::printf(" %zu", actual);
	}
	CHECK(openttd_rust_abi_layout(type, item) == SIZE_MAX);
	std::printf("\n");
}

static void Layouts()
{
	Layout(320, "OpenTTDShipPosition", {sizeof(OpenTTDShipPosition), alignof(OpenTTDShipPosition), offsetof(OpenTTDShipPosition, x), offsetof(OpenTTDShipPosition, y), offsetof(OpenTTDShipPosition, old_tile), offsetof(OpenTTDShipPosition, new_tile)});
	Layout(321, "OpenTTDShipLeaves", {sizeof(OpenTTDShipLeaves), alignof(OpenTTDShipLeaves), offsetof(OpenTTDShipLeaves, depot_dir), offsetof(OpenTTDShipLeaves, depot_axis), offsetof(OpenTTDShipLeaves, is_depot), offsetof(OpenTTDShipLeaves, depot_index), offsetof(OpenTTDShipLeaves, wait_unbunch), offsetof(OpenTTDShipLeaves, chain_depot), offsetof(OpenTTDShipLeaves, servint), offsetof(OpenTTDShipLeaves, needs_service), offsetof(OpenTTDShipLeaves, max_distance), offsetof(OpenTTDShipLeaves, tile_valid), offsetof(OpenTTDShipLeaves, tile_type), offsetof(OpenTTDShipLeaves, water_class), offsetof(OpenTTDShipLeaves, lock_middle), offsetof(OpenTTDShipLeaves, lock_dir), offsetof(OpenTTDShipLeaves, tile_min_z), offsetof(OpenTTDShipLeaves, tile_max_z), offsetof(OpenTTDShipLeaves, track_status), offsetof(OpenTTDShipLeaves, offset), offsetof(OpenTTDShipLeaves, diag_between), offsetof(OpenTTDShipLeaves, dist_square), offsetof(OpenTTDShipLeaves, dist_manhattan), offsetof(OpenTTDShipLeaves, docking), offsetof(OpenTTDShipLeaves, dock), offsetof(OpenTTDShipLeaves, dock_water), offsetof(OpenTTDShipLeaves, station), offsetof(OpenTTDShipLeaves, industry_station), offsetof(OpenTTDShipLeaves, oilrig), offsetof(OpenTTDShipLeaves, station_use), offsetof(OpenTTDShipLeaves, station_xy), offsetof(OpenTTDShipLeaves, station_contains), offsetof(OpenTTDShipLeaves, station_dock), offsetof(OpenTTDShipLeaves, station_visits), offsetof(OpenTTDShipLeaves, visit_set), offsetof(OpenTTDShipLeaves, arrival), offsetof(OpenTTDShipLeaves, service), offsetof(OpenTTDShipLeaves, leave_unbunch), offsetof(OpenTTDShipLeaves, path_result), offsetof(OpenTTDShipLeaves, order_free), offsetof(OpenTTDShipLeaves, order_dummy), offsetof(OpenTTDShipLeaves, order_depot), offsetof(OpenTTDShipLeaves, order_leave), offsetof(OpenTTDShipLeaves, order_increment), offsetof(OpenTTDShipLeaves, timetable), offsetof(OpenTTDShipLeaves, position), offsetof(OpenTTDShipLeaves, start_dirty), offsetof(OpenTTDShipLeaves, depot_dirty), offsetof(OpenTTDShipLeaves, depot_invalidate), offsetof(OpenTTDShipLeaves, ships_dirty), offsetof(OpenTTDShipLeaves, details_dirty), offsetof(OpenTTDShipLeaves, age), offsetof(OpenTTDShipLeaves, economy_age), offsetof(OpenTTDShipLeaves, decrease_value), offsetof(OpenTTDShipLeaves, check_breakdown), offsetof(OpenTTDShipLeaves, check_orders), offsetof(OpenTTDShipLeaves, running_cost), offsetof(OpenTTDShipLeaves, cost_divisor), offsetof(OpenTTDShipLeaves, pay_running), offsetof(OpenTTDShipLeaves, speed_default), offsetof(OpenTTDShipLeaves, age_default), offsetof(OpenTTDShipLeaves, speed_frac), offsetof(OpenTTDShipLeaves, speed_property), offsetof(OpenTTDShipLeaves, age_property), offsetof(OpenTTDShipLeaves, update_visual), offsetof(OpenTTDShipLeaves, cache_invalidate), offsetof(OpenTTDShipLeaves, capacity), offsetof(OpenTTDShipLeaves, sprite_direction), offsetof(OpenTTDShipLeaves, tile_x), offsetof(OpenTTDShipLeaves, tile_y), offsetof(OpenTTDShipLeaves, build_flag), offsetof(OpenTTDShipLeaves, build_random), offsetof(OpenTTDShipLeaves, new_position), offsetof(OpenTTDShipLeaves, exit_dir), offsetof(OpenTTDShipLeaves, track_direction), offsetof(OpenTTDShipLeaves, tracks_reach), offsetof(OpenTTDShipLeaves, busy_tile), offsetof(OpenTTDShipLeaves, path_size), offsetof(OpenTTDShipLeaves, path_back), offsetof(OpenTTDShipLeaves, path_pop), offsetof(OpenTTDShipLeaves, path_clear), offsetof(OpenTTDShipLeaves, enter_tile), offsetof(OpenTTDShipLeaves, enter_depot), offsetof(OpenTTDShipLeaves, process_orders), offsetof(OpenTTDShipLeaves, loading), offsetof(OpenTTDShipLeaves, begin_loading), offsetof(OpenTTDShipLeaves, breakdown), offsetof(OpenTTDShipLeaves, viewport), offsetof(OpenTTDShipLeaves, base_viewport), offsetof(OpenTTDShipLeaves, visual), offsetof(OpenTTDShipLeaves, cache), offsetof(OpenTTDShipLeaves, play_sound), offsetof(OpenTTDShipLeaves, yapf_reverse), offsetof(OpenTTDShipLeaves, yapf_choose), offsetof(OpenTTDShipLeaves, update_delta), offsetof(OpenTTDShipLeaves, build_owner), offsetof(OpenTTDShipLeaves, build_z), offsetof(OpenTTDShipLeaves, build_properties), offsetof(OpenTTDShipLeaves, build_dates), offsetof(OpenTTDShipLeaves, build_acceleration), offsetof(OpenTTDShipLeaves, build_prototype), offsetof(OpenTTDShipLeaves, build_interval_percent), offsetof(OpenTTDShipLeaves, build_capacity), offsetof(OpenTTDShipLeaves, set_tile), offsetof(OpenTTDShipLeaves, set_x), offsetof(OpenTTDShipLeaves, set_y), offsetof(OpenTTDShipLeaves, set_z), offsetof(OpenTTDShipLeaves, set_direction), offsetof(OpenTTDShipLeaves, set_speed), offsetof(OpenTTDShipLeaves, set_tick), offsetof(OpenTTDShipLeaves, set_running), offsetof(OpenTTDShipLeaves, set_day), offsetof(OpenTTDShipLeaves, set_order_time), offsetof(OpenTTDShipLeaves, set_progress), offsetof(OpenTTDShipLeaves, set_last_station), offsetof(OpenTTDShipLeaves, set_hidden), offsetof(OpenTTDShipLeaves, set_max_speed), offsetof(OpenTTDShipLeaves, set_cargo_age), offsetof(OpenTTDShipLeaves, set_dest), offsetof(OpenTTDShipLeaves, tile), offsetof(OpenTTDShipLeaves, dest), offsetof(OpenTTDShipLeaves, x), offsetof(OpenTTDShipLeaves, y), offsetof(OpenTTDShipLeaves, z), offsetof(OpenTTDShipLeaves, direction), offsetof(OpenTTDShipLeaves, speed), offsetof(OpenTTDShipLeaves, tick), offsetof(OpenTTDShipLeaves, running), offsetof(OpenTTDShipLeaves, day), offsetof(OpenTTDShipLeaves, order_time), offsetof(OpenTTDShipLeaves, progress), offsetof(OpenTTDShipLeaves, status), offsetof(OpenTTDShipLeaves, owner), offsetof(OpenTTDShipLeaves, last_station), offsetof(OpenTTDShipLeaves, order_destination), offsetof(OpenTTDShipLeaves, order_type), offsetof(OpenTTDShipLeaves, order_max_speed), offsetof(OpenTTDShipLeaves, acceleration), offsetof(OpenTTDShipLeaves, max_speed), offsetof(OpenTTDShipLeaves, state_owner), offsetof(OpenTTDShipLeaves, patch), offsetof(OpenTTDShipLeaves, neighbours), offsetof(OpenTTDShipLeaves, next_depot)});
	Layout(322, "OpenTTDShipReverseResult", {sizeof(OpenTTDShipReverseResult), alignof(OpenTTDShipReverseResult), offsetof(OpenTTDShipReverseResult, reverse), offsetof(OpenTTDShipReverseResult, trackdir)});
	Layout(323, "OpenTTDShipDepot", {sizeof(OpenTTDShipDepot), alignof(OpenTTDShipDepot), offsetof(OpenTTDShipDepot, id), offsetof(OpenTTDShipDepot, tile), offsetof(OpenTTDShipDepot, owner), offsetof(OpenTTDShipDepot, ship)});
	Layout(324, "OpenTTDShipTrackResult", {sizeof(OpenTTDShipTrackResult), alignof(OpenTTDShipTrackResult), offsetof(OpenTTDShipTrackResult, track), offsetof(OpenTTDShipTrackResult, found)});
	Layout(325, "OpenTTDShipDepotResult", {sizeof(OpenTTDShipDepotResult), alignof(OpenTTDShipDepotResult), offsetof(OpenTTDShipDepotResult, tile), offsetof(OpenTTDShipDepotResult, id), offsetof(OpenTTDShipDepotResult, valid)});
	Layout(0, "OpenTTDRustIntegerResult", {sizeof(OpenTTDRustIntegerResult), alignof(OpenTTDRustIntegerResult), offsetof(OpenTTDRustIntegerResult, value_bits), offsetof(OpenTTDRustIntegerResult, length), offsetof(OpenTTDRustIntegerResult, error_offset), offsetof(OpenTTDRustIntegerResult, error_length), offsetof(OpenTTDRustIntegerResult, error_kind)});
	Layout(1, "OpenTTDRustUtf8Encoded", {sizeof(OpenTTDRustUtf8Encoded), alignof(OpenTTDRustUtf8Encoded), offsetof(OpenTTDRustUtf8Encoded, bytes), offsetof(OpenTTDRustUtf8Encoded, length)});
	Layout(2, "OpenTTDRustUtf8Decoded", {sizeof(OpenTTDRustUtf8Decoded), alignof(OpenTTDRustUtf8Decoded), offsetof(OpenTTDRustUtf8Decoded, length), offsetof(OpenTTDRustUtf8Decoded, codepoint)});
	Layout(3, "OpenTTDRustLittleEndian", {sizeof(OpenTTDRustLittleEndian), alignof(OpenTTDRustLittleEndian), offsetof(OpenTTDRustLittleEndian, bytes)});
	Layout(4, "OpenTTDRustFormattedInteger", {sizeof(OpenTTDRustFormattedInteger), alignof(OpenTTDRustFormattedInteger), offsetof(OpenTTDRustFormattedInteger, bytes), offsetof(OpenTTDRustFormattedInteger, length)});
	Layout(5, "OpenTTDRustAlternatingState", {sizeof(OpenTTDRustAlternatingState), alignof(OpenTTDRustAlternatingState), offsetof(OpenTTDRustAlternatingState, position), offsetof(OpenTTDRustAlternatingState, next_after), offsetof(OpenTTDRustAlternatingState, current_after)});
	Layout(6, "OpenTTDRustAlternatingStep", {sizeof(OpenTTDRustAlternatingStep), alignof(OpenTTDRustAlternatingStep), offsetof(OpenTTDRustAlternatingStep, state), offsetof(OpenTTDRustAlternatingStep, movement)});
	Layout(7, "OpenTTDRustConsumerBound", {sizeof(OpenTTDRustConsumerBound), alignof(OpenTTDRustConsumerBound), offsetof(OpenTTDRustConsumerBound, length), offsetof(OpenTTDRustConsumerBound, position), offsetof(OpenTTDRustConsumerBound, shortfall)});
	Layout(8, "OpenTTDRustConsumerByte", {sizeof(OpenTTDRustConsumerByte), alignof(OpenTTDRustConsumerByte), offsetof(OpenTTDRustConsumerByte, value_bits), offsetof(OpenTTDRustConsumerByte, length)});
	Layout(9, "OpenTTDRustConsumerMatch", {sizeof(OpenTTDRustConsumerMatch), alignof(OpenTTDRustConsumerMatch), offsetof(OpenTTDRustConsumerMatch, length), offsetof(OpenTTDRustConsumerMatch, matched)});
	Layout(10, "OpenTTDRustConsumerSeparator", {sizeof(OpenTTDRustConsumerSeparator), alignof(OpenTTDRustConsumerSeparator), offsetof(OpenTTDRustConsumerSeparator, result_length), offsetof(OpenTTDRustConsumerSeparator, consumed_length)});
	Layout(11, "OpenTTDRustEncodedParameter", {sizeof(OpenTTDRustEncodedParameter), alignof(OpenTTDRustEncodedParameter), offsetof(OpenTTDRustEncodedParameter, value), offsetof(OpenTTDRustEncodedParameter, bytes), offsetof(OpenTTDRustEncodedParameter, length), offsetof(OpenTTDRustEncodedParameter, kind)});
	Layout(12, "OpenTTDRustEncodedView", {sizeof(OpenTTDRustEncodedView), alignof(OpenTTDRustEncodedView), offsetof(OpenTTDRustEncodedView, bytes), offsetof(OpenTTDRustEncodedView, length), offsetof(OpenTTDRustEncodedView, diagnostic_count), offsetof(OpenTTDRustEncodedView, assertion_offset), offsetof(OpenTTDRustEncodedView, assertion_length), offsetof(OpenTTDRustEncodedView, assertion_codepoint), offsetof(OpenTTDRustEncodedView, apply), offsetof(OpenTTDRustEncodedView, assertion_kind)});
	Layout(13, "OpenTTDRustEncodedDiagnostic", {sizeof(OpenTTDRustEncodedDiagnostic), alignof(OpenTTDRustEncodedDiagnostic), offsetof(OpenTTDRustEncodedDiagnostic, offset), offsetof(OpenTTDRustEncodedDiagnostic, length), offsetof(OpenTTDRustEncodedDiagnostic, tail_length), offsetof(OpenTTDRustEncodedDiagnostic, kind)});
	Layout(14, "OpenTTDRustSpiralState", {sizeof(OpenTTDRustSpiralState), alignof(OpenTTDRustSpiralState), offsetof(OpenTTDRustSpiralState, max_radius), offsetof(OpenTTDRustSpiralState, extent), offsetof(OpenTTDRustSpiralState, cur_radius), offsetof(OpenTTDRustSpiralState, position), offsetof(OpenTTDRustSpiralState, x), offsetof(OpenTTDRustSpiralState, y), offsetof(OpenTTDRustSpiralState, direction)});
	Layout(15, "OpenTTDRustByteTrim", {sizeof(OpenTTDRustByteTrim), alignof(OpenTTDRustByteTrim), offsetof(OpenTTDRustByteTrim, offset), offsetof(OpenTTDRustByteTrim, length)});
	Layout(16, "OpenTTDRustHistoryDescriptor", {sizeof(OpenTTDRustHistoryDescriptor), alignof(OpenTTDRustHistoryDescriptor), offsetof(OpenTTDRustHistoryDescriptor, child), offsetof(OpenTTDRustHistoryDescriptor, periods), offsetof(OpenTTDRustHistoryDescriptor, records), offsetof(OpenTTDRustHistoryDescriptor, first), offsetof(OpenTTDRustHistoryDescriptor, last), offsetof(OpenTTDRustHistoryDescriptor, division), offsetof(OpenTTDRustHistoryDescriptor, total_division), offsetof(OpenTTDRustHistoryDescriptor, child_periods), offsetof(OpenTTDRustHistoryDescriptor, child_division)});
	Layout(17, "OpenTTDRustHistoryStep", {sizeof(OpenTTDRustHistoryStep), alignof(OpenTTDRustHistoryStep), offsetof(OpenTTDRustHistoryStep, kind), offsetof(OpenTTDRustHistoryStep, count), offsetof(OpenTTDRustHistoryStep, first), offsetof(OpenTTDRustHistoryStep, last), offsetof(OpenTTDRustHistoryStep, target), offsetof(OpenTTDRustHistoryStep, token), offsetof(OpenTTDRustHistoryStep, value)});
	Layout(18, "OpenTTDCargoCollector", {sizeof(OpenTTDCargoCollector), alignof(OpenTTDCargoCollector), offsetof(OpenTTDCargoCollector, amount), offsetof(OpenTTDCargoCollector, previous), offsetof(OpenTTDCargoCollector, last_key), offsetof(OpenTTDCargoCollector, other), offsetof(OpenTTDCargoCollector, origin), offsetof(OpenTTDCargoCollector, selector), offsetof(OpenTTDCargoCollector, finalized)});
	Layout(19, "OpenTTDCryptoLeaves", {sizeof(OpenTTDCryptoLeaves), alignof(OpenTTDCryptoLeaves), offsetof(OpenTTDCryptoLeaves, wipe), offsetof(OpenTTDCryptoLeaves, verify16)});
	Layout(20, "OpenTTDPolyLayout", {sizeof(OpenTTDPolyLayout), alignof(OpenTTDPolyLayout), offsetof(OpenTTDPolyLayout, size), offsetof(OpenTTDPolyLayout, alignment), offsetof(OpenTTDPolyLayout, c), offsetof(OpenTTDPolyLayout, c_idx), offsetof(OpenTTDPolyLayout, r), offsetof(OpenTTDPolyLayout, pad), offsetof(OpenTTDPolyLayout, h)});
	Layout(21, "OpenTTDAeadLayout", {sizeof(OpenTTDAeadLayout), alignof(OpenTTDAeadLayout), offsetof(OpenTTDAeadLayout, size), offsetof(OpenTTDAeadLayout, alignment), offsetof(OpenTTDAeadLayout, counter), offsetof(OpenTTDAeadLayout, key), offsetof(OpenTTDAeadLayout, nonce)});
	Layout(22, "OpenTTDBlake2bLayout", {sizeof(OpenTTDBlake2bLayout), alignof(OpenTTDBlake2bLayout), offsetof(OpenTTDBlake2bLayout, size), offsetof(OpenTTDBlake2bLayout, alignment), offsetof(OpenTTDBlake2bLayout, hash), offsetof(OpenTTDBlake2bLayout, input_offset), offsetof(OpenTTDBlake2bLayout, input), offsetof(OpenTTDBlake2bLayout, input_idx), offsetof(OpenTTDBlake2bLayout, hash_size)});
	Layout(23, "OpenTTDPacketState", {sizeof(OpenTTDPacketState), alignof(OpenTTDPacketState), offsetof(OpenTTDPacketState, limit), offsetof(OpenTTDPacketState, position)});
	Layout(24, "OpenTTDPacketFrame", {sizeof(OpenTTDPacketFrame), alignof(OpenTTDPacketFrame), offsetof(OpenTTDPacketFrame, message), offsetof(OpenTTDPacketFrame, payload)});
	Layout(25, "OpenTTDX25519Leaves", {sizeof(OpenTTDX25519Leaves), alignof(OpenTTDX25519Leaves), offsetof(OpenTTDX25519Leaves, wipe), offsetof(OpenTTDX25519Leaves, verify32)});
	Layout(26, "OpenTTDValidationStep", {sizeof(OpenTTDValidationStep), alignof(OpenTTDValidationStep), offsetof(OpenTTDValidationStep, consumed), offsetof(OpenTTDValidationStep, output), offsetof(OpenTTDValidationStep, stopped)});
	Layout(27, "OpenTTDInplaceWrite", {sizeof(OpenTTDInplaceWrite), alignof(OpenTTDInplaceWrite), offsetof(OpenTTDInplaceWrite, position), offsetof(OpenTTDInplaceWrite, accepted)});
	Layout(34, "OpenTTDLinkGraphNode", {sizeof(OpenTTDLinkGraphNode), alignof(OpenTTDLinkGraphNode), offsetof(OpenTTDLinkGraphNode, supply), offsetof(OpenTTDLinkGraphNode, demand), offsetof(OpenTTDLinkGraphNode, station), offsetof(OpenTTDLinkGraphNode, x), offsetof(OpenTTDLinkGraphNode, y), offsetof(OpenTTDLinkGraphNode, edge_begin), offsetof(OpenTTDLinkGraphNode, edge_count)});
	Layout(35, "OpenTTDLinkGraphEdge", {sizeof(OpenTTDLinkGraphEdge), alignof(OpenTTDLinkGraphEdge), offsetof(OpenTTDLinkGraphEdge, capacity), offsetof(OpenTTDLinkGraphEdge, travel_time), offsetof(OpenTTDLinkGraphEdge, dest)});
	Layout(36, "OpenTTDLinkGraphSettings", {sizeof(OpenTTDLinkGraphSettings), alignof(OpenTTDLinkGraphSettings), offsetof(OpenTTDLinkGraphSettings, accuracy), offsetof(OpenTTDLinkGraphSettings, demand_distance), offsetof(OpenTTDLinkGraphSettings, demand_size), offsetof(OpenTTDLinkGraphSettings, saturation), offsetof(OpenTTDLinkGraphSettings, distribution), offsetof(OpenTTDLinkGraphSettings, express), offsetof(OpenTTDLinkGraphSettings, map_max_x), offsetof(OpenTTDLinkGraphSettings, map_max_y), offsetof(OpenTTDLinkGraphSettings, runtime)});
	Layout(38, "OpenTTDTreeServices", {sizeof(OpenTTDTreeServices), alignof(OpenTTDTreeServices), offsetof(OpenTTDTreeServices, plant_observation), offsetof(OpenTTDTreeServices, tree_observation), offsetof(OpenTTDTreeServices, snow_line), offsetof(OpenTTDTreeServices, sin), offsetof(OpenTTDTreeServices, cos), offsetof(OpenTTDTreeServices, make_tree), offsetof(OpenTTDTreeServices, set_ground_density), offsetof(OpenTTDTreeServices, add_count), offsetof(OpenTTDTreeServices, add_growth), offsetof(OpenTTDTreeServices, set_growth), offsetof(OpenTTDTreeServices, make_clear), offsetof(OpenTTDTreeServices, make_shore), offsetof(OpenTTDTreeServices, make_snow), offsetof(OpenTTDTreeServices, clear_neighbour_flooding), offsetof(OpenTTDTreeServices, tile_loop_water), offsetof(OpenTTDTreeServices, ambient), offsetof(OpenTTDTreeServices, play_sound), offsetof(OpenTTDTreeServices, progress), offsetof(OpenTTDTreeServices, progress_total), offsetof(OpenTTDTreeServices, clear_square), offsetof(OpenTTDTreeServices, town_rating), offsetof(OpenTTDTreeServices, command_begin), offsetof(OpenTTDTreeServices, command_tile), offsetof(OpenTTDTreeServices, command_next), offsetof(OpenTTDTreeServices, command_debit), offsetof(OpenTTDTreeServices, landscape_clear)});
	Layout(28, "OpenTTDTreePlantObservation", {sizeof(OpenTTDTreePlantObservation), alignof(OpenTTDTreePlantObservation), offsetof(OpenTTDTreePlantObservation, tile_type), offsetof(OpenTTDTreePlantObservation, bridge), offsetof(OpenTTDTreePlantObservation, ground), offsetof(OpenTTDTreePlantObservation, density), offsetof(OpenTTDTreePlantObservation, snow), offsetof(OpenTTDTreePlantObservation, coast), offsetof(OpenTTDTreePlantObservation, zone)});
	Layout(31, "OpenTTDTreeObservation", {sizeof(OpenTTDTreeObservation), alignof(OpenTTDTreeObservation), offsetof(OpenTTDTreeObservation, ground), offsetof(OpenTTDTreeObservation, density), offsetof(OpenTTDTreeObservation, species), offsetof(OpenTTDTreeObservation, count), offsetof(OpenTTDTreeObservation, growth), offsetof(OpenTTDTreeObservation, zone)});
	Layout(32, "OpenTTDTreeLoopSettings", {sizeof(OpenTTDTreeLoopSettings), alignof(OpenTTDTreeLoopSettings), offsetof(OpenTTDTreeLoopSettings, tick_counter), offsetof(OpenTTDTreeLoopSettings, size_x), offsetof(OpenTTDTreeLoopSettings, climate), offsetof(OpenTTDTreeLoopSettings, extra), offsetof(OpenTTDTreeLoopSettings, ambient)});
	Layout(33, "OpenTTDTreeTickSettings", {sizeof(OpenTTDTreeTickSettings), alignof(OpenTTDTreeTickSettings), offsetof(OpenTTDTreeTickSettings, tick_counter), offsetof(OpenTTDTreeTickSettings, size), offsetof(OpenTTDTreeTickSettings, climate), offsetof(OpenTTDTreeTickSettings, extra)});
	Layout(43, "OpenTTDTreeGenerateSettings", {sizeof(OpenTTDTreeGenerateSettings), alignof(OpenTTDTreeGenerateSettings), offsetof(OpenTTDTreeGenerateSettings, size), offsetof(OpenTTDTreeGenerateSettings, size_x), offsetof(OpenTTDTreeGenerateSettings, size_y), offsetof(OpenTTDTreeGenerateSettings, climate), offsetof(OpenTTDTreeGenerateSettings, placer), offsetof(OpenTTDTreeGenerateSettings, height_limit), offsetof(OpenTTDTreeGenerateSettings, editor), offsetof(OpenTTDTreeGenerateSettings, freeform_edges)});
	Layout(57, "OpenTTDTreeCommandSettings", {sizeof(OpenTTDTreeCommandSettings), alignof(OpenTTDTreeCommandSettings), offsetof(OpenTTDTreeCommandSettings, build_price), offsetof(OpenTTDTreeCommandSettings, size), offsetof(OpenTTDTreeCommandSettings, climate), offsetof(OpenTTDTreeCommandSettings, editor), offsetof(OpenTTDTreeCommandSettings, execute), offsetof(OpenTTDTreeCommandSettings, company_valid)});
	Layout(58, "OpenTTDTreeCommandResult", {sizeof(OpenTTDTreeCommandResult), alignof(OpenTTDTreeCommandResult), offsetof(OpenTTDTreeCommandResult, cost), offsetof(OpenTTDTreeCommandResult, status), offsetof(OpenTTDTreeCommandResult, message)});
	Layout(59, "OpenTTDTreeLandscapeClear", {sizeof(OpenTTDTreeLandscapeClear), alignof(OpenTTDTreeLandscapeClear), offsetof(OpenTTDTreeLandscapeClear, cost), offsetof(OpenTTDTreeLandscapeClear, failed)});
	Layout(42, "OpenTTDSharedServices", {sizeof(OpenTTDSharedServices), alignof(OpenTTDSharedServices), offsetof(OpenTTDSharedServices, context), offsetof(OpenTTDSharedServices, random), offsetof(OpenTTDSharedServices, industry), offsetof(OpenTTDSharedServices, tile_type), offsetof(OpenTTDSharedServices, bridge_above), offsetof(OpenTTDSharedServices, tropic_zone), offsetof(OpenTTDSharedServices, tile_z), offsetof(OpenTTDSharedServices, tile_slope), offsetof(OpenTTDSharedServices, mark_dirty), offsetof(OpenTTDSharedServices, set_tropic_zone)});
	Layout(37, "OpenTTDLinkGraphShare", {sizeof(OpenTTDLinkGraphShare), alignof(OpenTTDLinkGraphShare), offsetof(OpenTTDLinkGraphShare, node), offsetof(OpenTTDLinkGraphShare, origin), offsetof(OpenTTDLinkGraphShare, via), offsetof(OpenTTDLinkGraphShare, cumulative), offsetof(OpenTTDLinkGraphShare, unrestricted), offsetof(OpenTTDLinkGraphShare, has_share)});
	Layout(39, "OpenTTDEffectView", {sizeof(OpenTTDEffectView), alignof(OpenTTDEffectView), offsetof(OpenTTDEffectView, x), offsetof(OpenTTDEffectView, y), offsetof(OpenTTDEffectView, z), offsetof(OpenTTDEffectView, sprite), offsetof(OpenTTDEffectView, progress), offsetof(OpenTTDEffectView, spritenum), offsetof(OpenTTDEffectView, subtype), offsetof(OpenTTDEffectView, ambient)});
	Layout(41, "OpenTTDEffectLeaves", {sizeof(OpenTTDEffectLeaves), alignof(OpenTTDEffectLeaves), offsetof(OpenTTDEffectLeaves, observe), offsetof(OpenTTDEffectLeaves, write), offsetof(OpenTTDEffectLeaves, viewport), offsetof(OpenTTDEffectLeaves, sound), offsetof(OpenTTDEffectLeaves, animated)});
	Layout(80, "OpenTTDRoadPathElement", {sizeof(OpenTTDRoadPathElement), alignof(OpenTTDRoadPathElement), offsetof(OpenTTDRoadPathElement, trackdir), offsetof(OpenTTDRoadPathElement, tile)});
	Layout(81, "OpenTTDRoadSpeedLimits", {sizeof(OpenTTDRoadSpeedLimits), alignof(OpenTTDRoadSpeedLimits), offsetof(OpenTTDRoadSpeedLimits, max_track_speed), offsetof(OpenTTDRoadSpeedLimits, order_max_speed)});
	Layout(82, "OpenTTDRoadLeaves", {sizeof(OpenTTDRoadLeaves), alignof(OpenTTDRoadLeaves), offsetof(OpenTTDRoadLeaves, read_z), offsetof(OpenTTDRoadLeaves, read_type), offsetof(OpenTTDRoadLeaves, op_acc_model), offsetof(OpenTTDRoadLeaves, op_road_side), offsetof(OpenTTDRoadLeaves, op_tile_type), offsetof(OpenTTDRoadLeaves, op_has_road), offsetof(OpenTTDRoadLeaves, op_track_status), offsetof(OpenTTDRoadLeaves, op_tile_owner), offsetof(OpenTTDRoadLeaves, op_depot_dir), offsetof(OpenTTDRoadLeaves, op_bay_dir), offsetof(OpenTTDRoadLeaves, op_is_depot), offsetof(OpenTTDRoadLeaves, op_normal_road), offsetof(OpenTTDRoadLeaves, op_road_works), offsetof(OpenTTDRoadLeaves, op_disallowed), offsetof(OpenTTDRoadLeaves, op_bay_stop), offsetof(OpenTTDRoadLeaves, op_is_dt_stop), offsetof(OpenTTDRoadLeaves, op_stop_type), offsetof(OpenTTDRoadLeaves, op_free_bay), offsetof(OpenTTDRoadLeaves, op_any_road_bits), offsetof(OpenTTDRoadLeaves, op_road_bits), offsetof(OpenTTDRoadLeaves, op_offset), offsetof(OpenTTDRoadLeaves, op_tile_x), offsetof(OpenTTDRoadLeaves, op_tile_y), offsetof(OpenTTDRoadLeaves, op_station), offsetof(OpenTTDRoadLeaves, op_continuation), offsetof(OpenTTDRoadLeaves, op_bridge_speed), offsetof(OpenTTDRoadLeaves, op_max_penalty), offsetof(OpenTTDRoadLeaves, op_servint), offsetof(OpenTTDRoadLeaves, op_needs_service), offsetof(OpenTTDRoadLeaves, op_wait_unbunch), offsetof(OpenTTDRoadLeaves, op_order_stop), offsetof(OpenTTDRoadLeaves, op_road_type), offsetof(OpenTTDRoadLeaves, op_queue), offsetof(OpenTTDRoadLeaves, op_tunnel_dir), offsetof(OpenTTDRoadLeaves, op_acceleration), offsetof(OpenTTDRoadLeaves, op_update_speed), offsetof(OpenTTDRoadLeaves, op_advance), offsetof(OpenTTDRoadLeaves, op_position), offsetof(OpenTTDRoadLeaves, op_base_viewport), offsetof(OpenTTDRoadLeaves, op_last_speed), offsetof(OpenTTDRoadLeaves, op_roadstop_leave), offsetof(OpenTTDRoadLeaves, op_entrance_set), offsetof(OpenTTDRoadLeaves, op_entrance_busy), offsetof(OpenTTDRoadLeaves, op_order_free), offsetof(OpenTTDRoadLeaves, op_set_next), offsetof(OpenTTDRoadLeaves, op_start_stop_dirty), offsetof(OpenTTDRoadLeaves, op_depot_dirty), offsetof(OpenTTDRoadLeaves, op_details_dirty), offsetof(OpenTTDRoadLeaves, op_service), offsetof(OpenTTDRoadLeaves, op_leave_unbunch), offsetof(OpenTTDRoadLeaves, op_reset_unbunch), offsetof(OpenTTDRoadLeaves, op_path_result), offsetof(OpenTTDRoadLeaves, op_order_dummy), offsetof(OpenTTDRoadLeaves, op_order_depot), offsetof(OpenTTDRoadLeaves, op_depot_index), offsetof(OpenTTDRoadLeaves, op_decrease_value), offsetof(OpenTTDRoadLeaves, op_age), offsetof(OpenTTDRoadLeaves, op_economy_age), offsetof(OpenTTDRoadLeaves, op_check_breakdown), offsetof(OpenTTDRoadLeaves, op_check_orders), offsetof(OpenTTDRoadLeaves, op_pay_running), offsetof(OpenTTDRoadLeaves, op_cost_class), offsetof(OpenTTDRoadLeaves, op_cost_factor), offsetof(OpenTTDRoadLeaves, op_get_price), offsetof(OpenTTDRoadLeaves, op_grf_version), offsetof(OpenTTDRoadLeaves, op_length_default), offsetof(OpenTTDRoadLeaves, op_age_default), offsetof(OpenTTDRoadLeaves, op_speed_default), offsetof(OpenTTDRoadLeaves, op_length_error), offsetof(OpenTTDRoadLeaves, op_disconnect), offsetof(OpenTTDRoadLeaves, op_explosion), offsetof(OpenTTDRoadLeaves, op_sound_default), offsetof(OpenTTDRoadLeaves, op_sound), offsetof(OpenTTDRoadLeaves, op_sound_old1), offsetof(OpenTTDRoadLeaves, op_sound_old2), offsetof(OpenTTDRoadLeaves, op_engine_invalid), offsetof(OpenTTDRoadLeaves, op_invalid_price), offsetof(OpenTTDRoadLeaves, op_cost_divisor), offsetof(OpenTTDRoadLeaves, op_is_crossing), offsetof(OpenTTDRoadLeaves, op_new_position), offsetof(OpenTTDRoadLeaves, op_virt_tile), offsetof(OpenTTDRoadLeaves, op_is_road_stop), offsetof(OpenTTDRoadLeaves, op_set_dest), offsetof(OpenTTDRoadLeaves, op_cache_invalidate), offsetof(OpenTTDRoadLeaves, op_arrival), offsetof(OpenTTDRoadLeaves, op_crash_news), offsetof(OpenTTDRoadLeaves, op_station_visits), offsetof(OpenTTDRoadLeaves, op_station_visit_set), offsetof(OpenTTDRoadLeaves, op_local_company), offsetof(OpenTTDRoadLeaves, op_enter_tile), offsetof(OpenTTDRoadLeaves, op_enter_depot), offsetof(OpenTTDRoadLeaves, op_process_orders), offsetof(OpenTTDRoadLeaves, op_loading), offsetof(OpenTTDRoadLeaves, op_begin_loading), offsetof(OpenTTDRoadLeaves, op_tram_probe), offsetof(OpenTTDRoadLeaves, op_property), offsetof(OpenTTDRoadLeaves, op_length_callback), offsetof(OpenTTDRoadLeaves, op_play_sound), offsetof(OpenTTDRoadLeaves, op_visual), offsetof(OpenTTDRoadLeaves, op_update_visual), offsetof(OpenTTDRoadLeaves, op_cargo_changed), offsetof(OpenTTDRoadLeaves, op_length_changed), offsetof(OpenTTDRoadLeaves, op_breakdown), offsetof(OpenTTDRoadLeaves, op_delete), offsetof(OpenTTDRoadLeaves, op_ground_crash), offsetof(OpenTTDRoadLeaves, op_stop_random), offsetof(OpenTTDRoadLeaves, op_stop_animation), offsetof(OpenTTDRoadLeaves, op_yapf), offsetof(OpenTTDRoadLeaves, op_find_depot), offsetof(OpenTTDRoadLeaves, op_inclination), offsetof(OpenTTDRoadLeaves, op_viewport), offsetof(OpenTTDRoadLeaves, set_tile), offsetof(OpenTTDRoadLeaves, set_x), offsetof(OpenTTDRoadLeaves, set_y), offsetof(OpenTTDRoadLeaves, set_direction), offsetof(OpenTTDRoadLeaves, set_speed), offsetof(OpenTTDRoadLeaves, set_tick), offsetof(OpenTTDRoadLeaves, set_running), offsetof(OpenTTDRoadLeaves, set_day), offsetof(OpenTTDRoadLeaves, set_order_time), offsetof(OpenTTDRoadLeaves, set_progress), offsetof(OpenTTDRoadLeaves, set_last_station), offsetof(OpenTTDRoadLeaves, set_hidden), offsetof(OpenTTDRoadLeaves, set_first_engine), offsetof(OpenTTDRoadLeaves, set_length), offsetof(OpenTTDRoadLeaves, set_total_length), offsetof(OpenTTDRoadLeaves, set_cargo_age), offsetof(OpenTTDRoadLeaves, set_max_speed), offsetof(OpenTTDRoadLeaves, set_suppress_implicit), offsetof(OpenTTDRoadLeaves, read_day), offsetof(OpenTTDRoadLeaves, read_dest), offsetof(OpenTTDRoadLeaves, read_direction), offsetof(OpenTTDRoadLeaves, read_engine), offsetof(OpenTTDRoadLeaves, read_first), offsetof(OpenTTDRoadLeaves, read_front), offsetof(OpenTTDRoadLeaves, read_last_station), offsetof(OpenTTDRoadLeaves, read_length), offsetof(OpenTTDRoadLeaves, read_next), offsetof(OpenTTDRoadLeaves, read_order_type), offsetof(OpenTTDRoadLeaves, read_previous), offsetof(OpenTTDRoadLeaves, read_progress), offsetof(OpenTTDRoadLeaves, read_running), offsetof(OpenTTDRoadLeaves, read_speed), offsetof(OpenTTDRoadLeaves, read_status), offsetof(OpenTTDRoadLeaves, read_tick), offsetof(OpenTTDRoadLeaves, read_tile), offsetof(OpenTTDRoadLeaves, read_total_length), offsetof(OpenTTDRoadLeaves, read_tram), offsetof(OpenTTDRoadLeaves, speed_limits), offsetof(OpenTTDRoadLeaves, consist_speed), offsetof(OpenTTDRoadLeaves, close_origin), offsetof(OpenTTDRoadLeaves, close_candidate), offsetof(OpenTTDRoadLeaves, overtake_origin), offsetof(OpenTTDRoadLeaves, overtake_speed), offsetof(OpenTTDRoadLeaves, sliding_position), offsetof(OpenTTDRoadLeaves, height_speed), offsetof(OpenTTDRoadLeaves, collision_part), offsetof(OpenTTDRoadLeaves, collision_origin), offsetof(OpenTTDRoadLeaves, crash_direction), offsetof(OpenTTDRoadLeaves, path_vehicle), offsetof(OpenTTDRoadLeaves, depot_part), offsetof(OpenTTDRoadLeaves, depot_orders), offsetof(OpenTTDRoadLeaves, vehicle_tile), offsetof(OpenTTDRoadLeaves, arrival_vehicle), offsetof(OpenTTDRoadLeaves, tunnel_vehicle), offsetof(OpenTTDRoadLeaves, move_vehicle), offsetof(OpenTTDRoadLeaves, move_transition), offsetof(OpenTTDRoadLeaves, move_position), offsetof(OpenTTDRoadLeaves, block_vehicle), offsetof(OpenTTDRoadLeaves, stop_order), offsetof(OpenTTDRoadLeaves, move_stop), offsetof(OpenTTDRoadLeaves, order_clock), offsetof(OpenTTDRoadLeaves, controller_part), offsetof(OpenTTDRoadLeaves, service_origin), offsetof(OpenTTDRoadLeaves, service_order), offsetof(OpenTTDRoadLeaves, track_direction), offsetof(OpenTTDRoadLeaves, slope_origin), offsetof(OpenTTDRoadLeaves, slope_part), offsetof(OpenTTDRoadLeaves, turn_vehicle), offsetof(OpenTTDRoadLeaves, owner), offsetof(OpenTTDRoadLeaves, visit_close), offsetof(OpenTTDRoadLeaves, visit_tunnel), offsetof(OpenTTDRoadLeaves, visit_tile), offsetof(OpenTTDRoadLeaves, visit_train), offsetof(OpenTTDRoadLeaves, read_bus)});
	Layout(215, "OpenTTDRoadConsistSpeed", {sizeof(OpenTTDRoadConsistSpeed), alignof(OpenTTDRoadConsistSpeed), offsetof(OpenTTDRoadConsistSpeed, direction), offsetof(OpenTTDRoadConsistSpeed, next), offsetof(OpenTTDRoadConsistSpeed, status), offsetof(OpenTTDRoadConsistSpeed, tile)});
	Layout(216, "OpenTTDRoadCloseOrigin", {sizeof(OpenTTDRoadCloseOrigin), alignof(OpenTTDRoadCloseOrigin), offsetof(OpenTTDRoadCloseOrigin, first), offsetof(OpenTTDRoadCloseOrigin, z)});
	Layout(217, "OpenTTDRoadCloseCandidate", {sizeof(OpenTTDRoadCloseCandidate), alignof(OpenTTDRoadCloseCandidate), offsetof(OpenTTDRoadCloseCandidate, direction), offsetof(OpenTTDRoadCloseCandidate, first), offsetof(OpenTTDRoadCloseCandidate, x), offsetof(OpenTTDRoadCloseCandidate, y), offsetof(OpenTTDRoadCloseCandidate, z)});
	Layout(218, "OpenTTDRoadOvertakeOrigin", {sizeof(OpenTTDRoadOvertakeOrigin), alignof(OpenTTDRoadOvertakeOrigin), offsetof(OpenTTDRoadOvertakeOrigin, articulated), offsetof(OpenTTDRoadOvertakeOrigin, direction), offsetof(OpenTTDRoadOvertakeOrigin, tile), offsetof(OpenTTDRoadOvertakeOrigin, tram)});
	Layout(219, "OpenTTDRoadOvertakeSpeed", {sizeof(OpenTTDRoadOvertakeSpeed), alignof(OpenTTDRoadOvertakeSpeed), offsetof(OpenTTDRoadOvertakeSpeed, direction), offsetof(OpenTTDRoadOvertakeSpeed, speed), offsetof(OpenTTDRoadOvertakeSpeed, status), offsetof(OpenTTDRoadOvertakeSpeed, tile)});
	Layout(220, "OpenTTDRoadSlidingPosition", {sizeof(OpenTTDRoadSlidingPosition), alignof(OpenTTDRoadSlidingPosition), offsetof(OpenTTDRoadSlidingPosition, direction), offsetof(OpenTTDRoadSlidingPosition, x), offsetof(OpenTTDRoadSlidingPosition, y)});
	Layout(221, "OpenTTDRoadHeightSpeed", {sizeof(OpenTTDRoadHeightSpeed), alignof(OpenTTDRoadHeightSpeed), offsetof(OpenTTDRoadHeightSpeed, max_track_speed), offsetof(OpenTTDRoadHeightSpeed, speed), offsetof(OpenTTDRoadHeightSpeed, z)});
	Layout(222, "OpenTTDRoadCollisionPart", {sizeof(OpenTTDRoadCollisionPart), alignof(OpenTTDRoadCollisionPart), offsetof(OpenTTDRoadCollisionPart, next), offsetof(OpenTTDRoadCollisionPart, tile), offsetof(OpenTTDRoadCollisionPart, z)});
	Layout(223, "OpenTTDRoadCollisionOrigin", {sizeof(OpenTTDRoadCollisionOrigin), alignof(OpenTTDRoadCollisionOrigin), offsetof(OpenTTDRoadCollisionOrigin, x), offsetof(OpenTTDRoadCollisionOrigin, y)});
	Layout(224, "OpenTTDRoadCrashDirection", {sizeof(OpenTTDRoadCrashDirection), alignof(OpenTTDRoadCrashDirection), offsetof(OpenTTDRoadCrashDirection, direction)});
	Layout(225, "OpenTTDRoadPathVehicle", {sizeof(OpenTTDRoadPathVehicle), alignof(OpenTTDRoadPathVehicle), offsetof(OpenTTDRoadPathVehicle, articulated), offsetof(OpenTTDRoadPathVehicle, owner), offsetof(OpenTTDRoadPathVehicle, tile), offsetof(OpenTTDRoadPathVehicle, tram)});
	Layout(226, "OpenTTDRoadDepotPart", {sizeof(OpenTTDRoadDepotPart), alignof(OpenTTDRoadDepotPart), offsetof(OpenTTDRoadDepotPart, next), offsetof(OpenTTDRoadDepotPart, tile)});
	Layout(227, "OpenTTDRoadDepotOrders", {sizeof(OpenTTDRoadDepotOrders), alignof(OpenTTDRoadDepotOrders), offsetof(OpenTTDRoadDepotOrders, dest), offsetof(OpenTTDRoadDepotOrders, order_type)});
	Layout(228, "OpenTTDRoadVehicleTile", {sizeof(OpenTTDRoadVehicleTile), alignof(OpenTTDRoadVehicleTile), offsetof(OpenTTDRoadVehicleTile, tile)});
	Layout(229, "OpenTTDRoadArrivalVehicle", {sizeof(OpenTTDRoadArrivalVehicle), alignof(OpenTTDRoadArrivalVehicle), offsetof(OpenTTDRoadArrivalVehicle, owner), offsetof(OpenTTDRoadArrivalVehicle, tram)});
	Layout(230, "OpenTTDRoadTunnelVehicle", {sizeof(OpenTTDRoadTunnelVehicle), alignof(OpenTTDRoadTunnelVehicle), offsetof(OpenTTDRoadTunnelVehicle, direction), offsetof(OpenTTDRoadTunnelVehicle, front)});
	Layout(231, "OpenTTDRoadMoveVehicle", {sizeof(OpenTTDRoadMoveVehicle), alignof(OpenTTDRoadMoveVehicle), offsetof(OpenTTDRoadMoveVehicle, front), offsetof(OpenTTDRoadMoveVehicle, tile), offsetof(OpenTTDRoadMoveVehicle, tram)});
	Layout(232, "OpenTTDRoadMoveTransition", {sizeof(OpenTTDRoadMoveTransition), alignof(OpenTTDRoadMoveTransition), offsetof(OpenTTDRoadMoveTransition, length), offsetof(OpenTTDRoadMoveTransition, next), offsetof(OpenTTDRoadMoveTransition, tile)});
	Layout(233, "OpenTTDRoadMovePosition", {sizeof(OpenTTDRoadMovePosition), alignof(OpenTTDRoadMovePosition), offsetof(OpenTTDRoadMovePosition, order_type), offsetof(OpenTTDRoadMovePosition, owner), offsetof(OpenTTDRoadMovePosition, speed), offsetof(OpenTTDRoadMovePosition, tile)});
	Layout(234, "OpenTTDRoadBlockVehicle", {sizeof(OpenTTDRoadBlockVehicle), alignof(OpenTTDRoadBlockVehicle), offsetof(OpenTTDRoadBlockVehicle, direction), offsetof(OpenTTDRoadBlockVehicle, front), offsetof(OpenTTDRoadBlockVehicle, owner), offsetof(OpenTTDRoadBlockVehicle, tile)});
	Layout(235, "OpenTTDRoadStopOrder", {sizeof(OpenTTDRoadStopOrder), alignof(OpenTTDRoadStopOrder), offsetof(OpenTTDRoadStopOrder, order_destination), offsetof(OpenTTDRoadStopOrder, order_type), offsetof(OpenTTDRoadStopOrder, tile)});
	Layout(236, "OpenTTDRoadMoveStop", {sizeof(OpenTTDRoadMoveStop), alignof(OpenTTDRoadMoveStop), offsetof(OpenTTDRoadMoveStop, order_type), offsetof(OpenTTDRoadMoveStop, tile)});
	Layout(237, "OpenTTDRoadOrderClock", {sizeof(OpenTTDRoadOrderClock), alignof(OpenTTDRoadOrderClock), offsetof(OpenTTDRoadOrderClock, order_time)});
	Layout(238, "OpenTTDRoadControllerPart", {sizeof(OpenTTDRoadControllerPart), alignof(OpenTTDRoadControllerPart), offsetof(OpenTTDRoadControllerPart, next), offsetof(OpenTTDRoadControllerPart, status)});
	Layout(239, "OpenTTDRoadServiceOrigin", {sizeof(OpenTTDRoadServiceOrigin), alignof(OpenTTDRoadServiceOrigin), offsetof(OpenTTDRoadServiceOrigin, first), offsetof(OpenTTDRoadServiceOrigin, speed), offsetof(OpenTTDRoadServiceOrigin, tile)});
	Layout(240, "OpenTTDRoadServiceOrder", {sizeof(OpenTTDRoadServiceOrder), alignof(OpenTTDRoadServiceOrder), offsetof(OpenTTDRoadServiceOrder, order_nonstop), offsetof(OpenTTDRoadServiceOrder, order_type)});
	Layout(241, "OpenTTDRoadTrackDirection", {sizeof(OpenTTDRoadTrackDirection), alignof(OpenTTDRoadTrackDirection), offsetof(OpenTTDRoadTrackDirection, direction), offsetof(OpenTTDRoadTrackDirection, status), offsetof(OpenTTDRoadTrackDirection, tile)});
	Layout(242, "OpenTTDRoadSlopeOrigin", {sizeof(OpenTTDRoadSlopeOrigin), alignof(OpenTTDRoadSlopeOrigin), offsetof(OpenTTDRoadSlopeOrigin, direction), offsetof(OpenTTDRoadSlopeOrigin, first)});
	Layout(243, "OpenTTDRoadSlopePart", {sizeof(OpenTTDRoadSlopePart), alignof(OpenTTDRoadSlopePart), offsetof(OpenTTDRoadSlopePart, direction), offsetof(OpenTTDRoadSlopePart, next)});
	Layout(244, "OpenTTDRoadTurnVehicle", {sizeof(OpenTTDRoadTurnVehicle), alignof(OpenTTDRoadTurnVehicle), offsetof(OpenTTDRoadTurnVehicle, breakdown), offsetof(OpenTTDRoadTurnVehicle, direction), offsetof(OpenTTDRoadTurnVehicle, order_type), offsetof(OpenTTDRoadTurnVehicle, status), offsetof(OpenTTDRoadTurnVehicle, tile)});
	Layout(245, "OpenTTDRoadPosition", {sizeof(OpenTTDRoadPosition), alignof(OpenTTDRoadPosition), offsetof(OpenTTDRoadPosition, x), offsetof(OpenTTDRoadPosition, y)});
	Layout(246, "OpenTTDRoadTrackChoice", {sizeof(OpenTTDRoadTrackChoice), alignof(OpenTTDRoadTrackChoice), offsetof(OpenTTDRoadTrackChoice, trackdir), offsetof(OpenTTDRoadTrackChoice, found)});
	Layout(247, "OpenTTDRoadDepotResult", {sizeof(OpenTTDRoadDepotResult), alignof(OpenTTDRoadDepotResult), offsetof(OpenTTDRoadDepotResult, tile), offsetof(OpenTTDRoadDepotResult, length)});
	Layout(44, "OpenTTDDisasterState", {sizeof(OpenTTDDisasterState), alignof(OpenTTDDisasterState), offsetof(OpenTTDDisasterState, image_override), offsetof(OpenTTDDisasterState, target), offsetof(OpenTTDDisasterState, state), offsetof(OpenTTDDisasterState, flags)});
	Layout(45, "OpenTTDDisasterAction", {sizeof(OpenTTDDisasterAction), alignof(OpenTTDDisasterAction), offsetof(OpenTTDDisasterAction, kind), offsetof(OpenTTDDisasterAction, id), offsetof(OpenTTDDisasterAction, other), offsetof(OpenTTDDisasterAction, a), offsetof(OpenTTDDisasterAction, b), offsetof(OpenTTDDisasterAction, c), offsetof(OpenTTDDisasterAction, d)});
	Layout(46, "OpenTTDWaterPatch", {sizeof(OpenTTDWaterPatch), alignof(OpenTTDWaterPatch), offsetof(OpenTTDWaterPatch, x), offsetof(OpenTTDWaterPatch, y), offsetof(OpenTTDWaterPatch, label)});
	Layout(47, "OpenTTDWaterSnapshot", {sizeof(OpenTTDWaterSnapshot), alignof(OpenTTDWaterSnapshot), offsetof(OpenTTDWaterSnapshot, edges), offsetof(OpenTTDWaterSnapshot, labels), offsetof(OpenTTDWaterSnapshot, patches), offsetof(OpenTTDWaterSnapshot, aqueducts)});
	Layout(48, "OpenTTDWaterLeaves", {sizeof(OpenTTDWaterLeaves), alignof(OpenTTDWaterLeaves), offsetof(OpenTTDWaterLeaves, tracks), offsetof(OpenTTDWaterLeaves, follow), offsetof(OpenTTDWaterLeaves, aqueduct), offsetof(OpenTTDWaterLeaves, debug)});
	Layout(180, "OpenTTDAircraftState", {sizeof(OpenTTDAircraftState), alignof(OpenTTDAircraftState), offsetof(OpenTTDAircraftState, cached_max_range_sqr), offsetof(OpenTTDAircraftState, cached_max_range), offsetof(OpenTTDAircraftState, cache_padding), offsetof(OpenTTDAircraftState, crashed_counter), offsetof(OpenTTDAircraftState, targetairport), offsetof(OpenTTDAircraftState, pos), offsetof(OpenTTDAircraftState, previous_pos), offsetof(OpenTTDAircraftState, state), offsetof(OpenTTDAircraftState, last_direction), offsetof(OpenTTDAircraftState, number_consecutive_turns), offsetof(OpenTTDAircraftState, turn_counter), offsetof(OpenTTDAircraftState, flags)});
	Layout(181, "OpenTTDAircraftVehicle", {sizeof(OpenTTDAircraftVehicle), alignof(OpenTTDAircraftVehicle), offsetof(OpenTTDAircraftVehicle, handle), offsetof(OpenTTDAircraftVehicle, state), offsetof(OpenTTDAircraftVehicle, id)});
	Layout(182, "OpenTTDAircraftNode", {sizeof(OpenTTDAircraftNode), alignof(OpenTTDAircraftNode), offsetof(OpenTTDAircraftNode, next), offsetof(OpenTTDAircraftNode, blocks), offsetof(OpenTTDAircraftNode, position), offsetof(OpenTTDAircraftNode, next_position), offsetof(OpenTTDAircraftNode, heading)});
	Layout(183, "OpenTTDAircraftMoving", {sizeof(OpenTTDAircraftMoving), alignof(OpenTTDAircraftMoving), offsetof(OpenTTDAircraftMoving, x), offsetof(OpenTTDAircraftMoving, y), offsetof(OpenTTDAircraftMoving, flags), offsetof(OpenTTDAircraftMoving, direction)});
	Layout(184, "OpenTTDAircraftPosition", {sizeof(OpenTTDAircraftPosition), alignof(OpenTTDAircraftPosition), offsetof(OpenTTDAircraftPosition, x), offsetof(OpenTTDAircraftPosition, y), offsetof(OpenTTDAircraftPosition, tile)});
	Layout(185, "OpenTTDAircraftLeaves", {sizeof(OpenTTDAircraftLeaves), alignof(OpenTTDAircraftLeaves), offsetof(OpenTTDAircraftLeaves, subtype), offsetof(OpenTTDAircraftLeaves, x), offsetof(OpenTTDAircraftLeaves, set_x), offsetof(OpenTTDAircraftLeaves, y), offsetof(OpenTTDAircraftLeaves, set_y), offsetof(OpenTTDAircraftLeaves, z), offsetof(OpenTTDAircraftLeaves, set_z), offsetof(OpenTTDAircraftLeaves, tile), offsetof(OpenTTDAircraftLeaves, set_tile), offsetof(OpenTTDAircraftLeaves, direction), offsetof(OpenTTDAircraftLeaves, set_direction), offsetof(OpenTTDAircraftLeaves, tick_counter), offsetof(OpenTTDAircraftLeaves, set_tick_counter), offsetof(OpenTTDAircraftLeaves, owner), offsetof(OpenTTDAircraftLeaves, vehicle_status), offsetof(OpenTTDAircraftLeaves, set_vehicle_status), offsetof(OpenTTDAircraftLeaves, current_speed), offsetof(OpenTTDAircraftLeaves, set_current_speed), offsetof(OpenTTDAircraftLeaves, subspeed), offsetof(OpenTTDAircraftLeaves, set_subspeed), offsetof(OpenTTDAircraftLeaves, progress), offsetof(OpenTTDAircraftLeaves, set_progress), offsetof(OpenTTDAircraftLeaves, acceleration), offsetof(OpenTTDAircraftLeaves, maximum_speed), offsetof(OpenTTDAircraftLeaves, set_maximum_speed), offsetof(OpenTTDAircraftLeaves, set_breakdown_counter), offsetof(OpenTTDAircraftLeaves, order_type), offsetof(OpenTTDAircraftLeaves, order_destination), offsetof(OpenTTDAircraftLeaves, running_ticks), offsetof(OpenTTDAircraftLeaves, set_running_ticks), offsetof(OpenTTDAircraftLeaves, order_time), offsetof(OpenTTDAircraftLeaves, set_order_time), offsetof(OpenTTDAircraftLeaves, day_counter), offsetof(OpenTTDAircraftLeaves, set_day_counter), offsetof(OpenTTDAircraftLeaves, profit), offsetof(OpenTTDAircraftLeaves, set_profit), offsetof(OpenTTDAircraftLeaves, last_station), offsetof(OpenTTDAircraftLeaves, set_last_station), offsetof(OpenTTDAircraftLeaves, set_economy_service), offsetof(OpenTTDAircraftLeaves, set_calendar_service), offsetof(OpenTTDAircraftLeaves, set_breakdowns), offsetof(OpenTTDAircraftLeaves, set_reliability), offsetof(OpenTTDAircraftLeaves, set_cargo_age), offsetof(OpenTTDAircraftLeaves, next), offsetof(OpenTTDAircraftLeaves, map_size_x), offsetof(OpenTTDAircraftLeaves, map_max_x), offsetof(OpenTTDAircraftLeaves, map_max_y), offsetof(OpenTTDAircraftLeaves, plane_speed), offsetof(OpenTTDAircraftLeaves, no_jetcrash), offsetof(OpenTTDAircraftLeaves, plane_crashes), offsetof(OpenTTDAircraftLeaves, service_at_helipad), offsetof(OpenTTDAircraftLeaves, disaster_sound), offsetof(OpenTTDAircraftLeaves, economy_date), offsetof(OpenTTDAircraftLeaves, calendar_date), offsetof(OpenTTDAircraftLeaves, station), offsetof(OpenTTDAircraftLeaves, airport_tile), offsetof(OpenTTDAircraftLeaves, station_tile), offsetof(OpenTTDAircraftLeaves, rotation), offsetof(OpenTTDAircraftLeaves, airport_width), offsetof(OpenTTDAircraftLeaves, airport_height), offsetof(OpenTTDAircraftLeaves, airport_type), offsetof(OpenTTDAircraftLeaves, station_owner), offsetof(OpenTTDAircraftLeaves, has_hangar), offsetof(OpenTTDAircraftLeaves, has_airport), offsetof(OpenTTDAircraftLeaves, airport_fta), offsetof(OpenTTDAircraftLeaves, airport_blocks), offsetof(OpenTTDAircraftLeaves, had_vehicle), offsetof(OpenTTDAircraftLeaves, dummy_airport), offsetof(OpenTTDAircraftLeaves, airport_elements), offsetof(OpenTTDAircraftLeaves, helipads), offsetof(OpenTTDAircraftLeaves, airport_flags), offsetof(OpenTTDAircraftLeaves, airport_delta_z), offsetof(OpenTTDAircraftLeaves, node), offsetof(OpenTTDAircraftLeaves, fta), offsetof(OpenTTDAircraftLeaves, moving), offsetof(OpenTTDAircraftLeaves, engine_speed), offsetof(OpenTTDAircraftLeaves, engine_subtype), offsetof(OpenTTDAircraftLeaves, engine_sound), offsetof(OpenTTDAircraftLeaves, engine_reliability), offsetof(OpenTTDAircraftLeaves, vehicle_type), offsetof(OpenTTDAircraftLeaves, slope), offsetof(OpenTTDAircraftLeaves, tile_height), offsetof(OpenTTDAircraftLeaves, airport_entry), offsetof(OpenTTDAircraftLeaves, direction_towards), offsetof(OpenTTDAircraftLeaves, new_position), offsetof(OpenTTDAircraftLeaves, hangar_height), offsetof(OpenTTDAircraftLeaves, terminal_count), offsetof(OpenTTDAircraftLeaves, hangar_exit), offsetof(OpenTTDAircraftLeaves, can_use_station), offsetof(OpenTTDAircraftLeaves, service_interval), offsetof(OpenTTDAircraftLeaves, needs_service), offsetof(OpenTTDAircraftLeaves, chain_in_depot), offsetof(OpenTTDAircraftLeaves, waiting_unbunching), offsetof(OpenTTDAircraftLeaves, nearest_depot_order), offsetof(OpenTTDAircraftLeaves, part_of_orders), offsetof(OpenTTDAircraftLeaves, next_station), offsetof(OpenTTDAircraftLeaves, next_aircraft), offsetof(OpenTTDAircraftLeaves, random), offsetof(OpenTTDAircraftLeaves, update_position), offsetof(OpenTTDAircraftLeaves, rotor_image), offsetof(OpenTTDAircraftLeaves, copy_sprite), offsetof(OpenTTDAircraftLeaves, position_viewport), offsetof(OpenTTDAircraftLeaves, create_effect), offsetof(OpenTTDAircraftLeaves, dirty_start_stop), offsetof(OpenTTDAircraftLeaves, play_sound), offsetof(OpenTTDAircraftLeaves, truncate_cargo), offsetof(OpenTTDAircraftLeaves, crash_news), offsetof(OpenTTDAircraftLeaves, station_rating), offsetof(OpenTTDAircraftLeaves, landing_rating), offsetof(OpenTTDAircraftLeaves, free_order), offsetof(OpenTTDAircraftLeaves, service_in_depot), offsetof(OpenTTDAircraftLeaves, leave_unbunching), offsetof(OpenTTDAircraftLeaves, dirty_depot), offsetof(OpenTTDAircraftLeaves, first_arrival), offsetof(OpenTTDAircraftLeaves, begin_loading), offsetof(OpenTTDAircraftLeaves, dirty_details), offsetof(OpenTTDAircraftLeaves, update_delta), offsetof(OpenTTDAircraftLeaves, touchdown_animation), offsetof(OpenTTDAircraftLeaves, destination_too_far), offsetof(OpenTTDAircraftLeaves, delete_range_news), offsetof(OpenTTDAircraftLeaves, handle_breakdown), offsetof(OpenTTDAircraftLeaves, handle_loading), offsetof(OpenTTDAircraftLeaves, service_order), offsetof(OpenTTDAircraftLeaves, dummy_order), offsetof(OpenTTDAircraftLeaves, age_vehicle), offsetof(OpenTTDAircraftLeaves, economy_age), offsetof(OpenTTDAircraftLeaves, decrease_value), offsetof(OpenTTDAircraftLeaves, check_orders), offsetof(OpenTTDAircraftLeaves, check_breakdown), offsetof(OpenTTDAircraftLeaves, running_cost), offsetof(OpenTTDAircraftLeaves, subtract_cost), offsetof(OpenTTDAircraftLeaves, dirty_lists), offsetof(OpenTTDAircraftLeaves, next_stopping_station), offsetof(OpenTTDAircraftLeaves, remove_depot_orders), offsetof(OpenTTDAircraftLeaves, assert_flying), offsetof(OpenTTDAircraftLeaves, invalid_movement), offsetof(OpenTTDAircraftLeaves, invalid_position), offsetof(OpenTTDAircraftLeaves, invalid_scheme), offsetof(OpenTTDAircraftLeaves, unreachable), offsetof(OpenTTDAircraftLeaves, speed_property), offsetof(OpenTTDAircraftLeaves, cargo_age_property), offsetof(OpenTTDAircraftLeaves, range_property), offsetof(OpenTTDAircraftLeaves, start_sound), offsetof(OpenTTDAircraftLeaves, touchdown_sound), offsetof(OpenTTDAircraftLeaves, rotor_image_if_changed), offsetof(OpenTTDAircraftLeaves, update_rotor_image), offsetof(OpenTTDAircraftLeaves, process_orders), offsetof(OpenTTDAircraftLeaves, enter_depot), offsetof(OpenTTDAircraftLeaves, vehicle_crash), offsetof(OpenTTDAircraftLeaves, delete_aircraft), offsetof(OpenTTDAircraftLeaves, send_to_depot), offsetof(OpenTTDAircraftLeaves, sample_count), offsetof(OpenTTDAircraftLeaves, helicopter_sound), offsetof(OpenTTDAircraftLeaves, explosion_sound), offsetof(OpenTTDAircraftLeaves, skid_sound), offsetof(OpenTTDAircraftLeaves, ticks_per_year), offsetof(OpenTTDAircraftLeaves, fta_blocks), offsetof(OpenTTDAircraftLeaves, fta_heading), offsetof(OpenTTDAircraftLeaves, fta_next_position), offsetof(OpenTTDAircraftLeaves, fta_next), offsetof(OpenTTDAircraftLeaves, block_node), offsetof(OpenTTDAircraftLeaves, route_node), offsetof(OpenTTDAircraftLeaves, block_choice)});
	Layout(186, "OpenTTDAircraftBlockNode", {sizeof(OpenTTDAircraftBlockNode), alignof(OpenTTDAircraftBlockNode), offsetof(OpenTTDAircraftBlockNode, blocks), offsetof(OpenTTDAircraftBlockNode, position), offsetof(OpenTTDAircraftBlockNode, next_position)});
	Layout(187, "OpenTTDAircraftRouteNode", {sizeof(OpenTTDAircraftRouteNode), alignof(OpenTTDAircraftRouteNode), offsetof(OpenTTDAircraftRouteNode, next), offsetof(OpenTTDAircraftRouteNode, next_position), offsetof(OpenTTDAircraftRouteNode, heading)});
	Layout(188, "OpenTTDAircraftBlockChoice", {sizeof(OpenTTDAircraftBlockChoice), alignof(OpenTTDAircraftBlockChoice), offsetof(OpenTTDAircraftBlockChoice, next), offsetof(OpenTTDAircraftBlockChoice, blocks), offsetof(OpenTTDAircraftBlockChoice, heading)});

	Layout(340, "OpenTTDTrainHandle", {sizeof(OpenTTDTrainHandle), alignof(OpenTTDTrainHandle), offsetof(OpenTTDTrainHandle, shell), offsetof(OpenTTDTrainHandle, owner)});
	Layout(341, "OpenTTDTrainConsistChangedRead", {sizeof(OpenTTDTrainConsistChangedRead), alignof(OpenTTDTrainConsistChangedRead), offsetof(OpenTTDTrainConsistChangedRead, engine), offsetof(OpenTTDTrainConsistChangedRead, front)});
	Layout(342, "OpenTTDTrainConsistChanged1Read", {sizeof(OpenTTDTrainConsistChanged1Read), alignof(OpenTTDTrainConsistChanged1Read), offsetof(OpenTTDTrainConsistChanged1Read, engine), offsetof(OpenTTDTrainConsistChanged1Read, engine_part)});
	Layout(343, "OpenTTDTrainConsistChanged2Read", {sizeof(OpenTTDTrainConsistChanged2Read), alignof(OpenTTDTrainConsistChanged2Read), offsetof(OpenTTDTrainConsistChanged2Read, cargo_cap)});
	Layout(344, "OpenTTDTrainCurveLimitRead", {sizeof(OpenTTDTrainCurveLimitRead), alignof(OpenTTDTrainCurveLimitRead), offsetof(OpenTTDTrainCurveLimitRead, next), offsetof(OpenTTDTrainCurveLimitRead, direction)});
	Layout(345, "OpenTTDTrainStopLocationRead", {sizeof(OpenTTDTrainStopLocationRead), alignof(OpenTTDTrainStopLocationRead), offsetof(OpenTTDTrainStopLocationRead, length), offsetof(OpenTTDTrainStopLocationRead, total_length), offsetof(OpenTTDTrainStopLocationRead, order_destination), offsetof(OpenTTDTrainStopLocationRead, order)});
	Layout(346, "OpenTTDTrainCurrentMaxSpeedRead", {sizeof(OpenTTDTrainCurrentMaxSpeedRead), alignof(OpenTTDTrainCurrentMaxSpeedRead), offsetof(OpenTTDTrainCurrentMaxSpeedRead, tile), offsetof(OpenTTDTrainCurrentMaxSpeedRead, max_track_speed), offsetof(OpenTTDTrainCurrentMaxSpeedRead, speed)});
	Layout(347, "OpenTTDTrainCurrentMaxSpeed6Read", {sizeof(OpenTTDTrainCurrentMaxSpeed6Read), alignof(OpenTTDTrainCurrentMaxSpeed6Read), offsetof(OpenTTDTrainCurrentMaxSpeed6Read, next), offsetof(OpenTTDTrainCurrentMaxSpeed6Read, tile), offsetof(OpenTTDTrainCurrentMaxSpeed6Read, status)});
	Layout(348, "OpenTTDTrainUpdateAccelerationRead", {sizeof(OpenTTDTrainUpdateAccelerationRead), alignof(OpenTTDTrainUpdateAccelerationRead), offsetof(OpenTTDTrainUpdateAccelerationRead, power), offsetof(OpenTTDTrainUpdateAccelerationRead, weight)});
	Layout(349, "OpenTTDTrainUpdateSpeedRead", {sizeof(OpenTTDTrainUpdateSpeedRead), alignof(OpenTTDTrainUpdateSpeedRead), offsetof(OpenTTDTrainUpdateSpeedRead, status), offsetof(OpenTTDTrainUpdateSpeedRead, acceleration)});
	Layout(350, "OpenTTDTrainTrackdirRead", {sizeof(OpenTTDTrainTrackdirRead), alignof(OpenTTDTrainTrackdirRead), offsetof(OpenTTDTrainTrackdirRead, tile), offsetof(OpenTTDTrainTrackdirRead, direction), offsetof(OpenTTDTrainTrackdirRead, status)});
	Layout(351, "OpenTTDTrainCanLeaveRead", {sizeof(OpenTTDTrainCanLeaveRead), alignof(OpenTTDTrainCanLeaveRead), offsetof(OpenTTDTrainCanLeaveRead, tile), offsetof(OpenTTDTrainCanLeaveRead, direction)});
	Layout(353, "OpenTTDTrainCrossingApproachRead", {sizeof(OpenTTDTrainCrossingApproachRead), alignof(OpenTTDTrainCrossingApproachRead), offsetof(OpenTTDTrainCrossingApproachRead, status), offsetof(OpenTTDTrainCrossingApproachRead, front)});
	Layout(354, "OpenTTDTrainNextOffsetRead", {sizeof(OpenTTDTrainNextOffsetRead), alignof(OpenTTDTrainNextOffsetRead), offsetof(OpenTTDTrainNextOffsetRead, next), offsetof(OpenTTDTrainNextOffsetRead, length)});
	Layout(355, "OpenTTDTrainAfterSwapRead", {sizeof(OpenTTDTrainAfterSwapRead), alignof(OpenTTDTrainAfterSwapRead), offsetof(OpenTTDTrainAfterSwapRead, tile), offsetof(OpenTTDTrainAfterSwapRead, x), offsetof(OpenTTDTrainAfterSwapRead, y)});
	Layout(356, "OpenTTDTrainReverseSwapRead", {sizeof(OpenTTDTrainReverseSwapRead), alignof(OpenTTDTrainReverseSwapRead), offsetof(OpenTTDTrainReverseSwapRead, tile), offsetof(OpenTTDTrainReverseSwapRead, x), offsetof(OpenTTDTrainReverseSwapRead, y), offsetof(OpenTTDTrainReverseSwapRead, z), offsetof(OpenTTDTrainReverseSwapRead, direction), offsetof(OpenTTDTrainReverseSwapRead, status)});
	Layout(357, "OpenTTDTrainApproachingEndRead", {sizeof(OpenTTDTrainApproachingEndRead), alignof(OpenTTDTrainApproachingEndRead), offsetof(OpenTTDTrainApproachingEndRead, x), offsetof(OpenTTDTrainApproachingEndRead, y), offsetof(OpenTTDTrainApproachingEndRead, length), offsetof(OpenTTDTrainApproachingEndRead, speed), offsetof(OpenTTDTrainApproachingEndRead, direction)});
	Layout(358, "OpenTTDTrainLineEndsRead", {sizeof(OpenTTDTrainLineEndsRead), alignof(OpenTTDTrainLineEndsRead), offsetof(OpenTTDTrainLineEndsRead, tile), offsetof(OpenTTDTrainLineEndsRead, speed), offsetof(OpenTTDTrainLineEndsRead, breakdown)});
	Layout(359, "OpenTTDTrainSpeedZRead", {sizeof(OpenTTDTrainSpeedZRead), alignof(OpenTTDTrainSpeedZRead), offsetof(OpenTTDTrainSpeedZRead, z), offsetof(OpenTTDTrainSpeedZRead, max_track_speed), offsetof(OpenTTDTrainSpeedZRead, speed)});
	Layout(360, "OpenTTDTrainMoveVehicleRead", {sizeof(OpenTTDTrainMoveVehicleRead), alignof(OpenTTDTrainMoveVehicleRead), offsetof(OpenTTDTrainMoveVehicleRead, tile), offsetof(OpenTTDTrainMoveVehicleRead, x), offsetof(OpenTTDTrainMoveVehicleRead, y), offsetof(OpenTTDTrainMoveVehicleRead, direction), offsetof(OpenTTDTrainMoveVehicleRead, front), offsetof(OpenTTDTrainMoveVehicleRead, articulated)});
	Layout(361, "OpenTTDTrainMoveVehicle20Read", {sizeof(OpenTTDTrainMoveVehicle20Read), alignof(OpenTTDTrainMoveVehicle20Read), offsetof(OpenTTDTrainMoveVehicle20Read, speed), offsetof(OpenTTDTrainMoveVehicle20Read, status), offsetof(OpenTTDTrainMoveVehicle20Read, front)});
	Layout(362, "OpenTTDTrainCollisionOneRead", {sizeof(OpenTTDTrainCollisionOneRead), alignof(OpenTTDTrainCollisionOneRead), offsetof(OpenTTDTrainCollisionOneRead, first), offsetof(OpenTTDTrainCollisionOneRead, x), offsetof(OpenTTDTrainCollisionOneRead, y), offsetof(OpenTTDTrainCollisionOneRead, z), offsetof(OpenTTDTrainCollisionOneRead, length), offsetof(OpenTTDTrainCollisionOneRead, owner)});
	Layout(363, "OpenTTDTrainCollisionOne22Read", {sizeof(OpenTTDTrainCollisionOne22Read), alignof(OpenTTDTrainCollisionOne22Read), offsetof(OpenTTDTrainCollisionOne22Read, x), offsetof(OpenTTDTrainCollisionOne22Read, y), offsetof(OpenTTDTrainCollisionOne22Read, z), offsetof(OpenTTDTrainCollisionOne22Read, length), offsetof(OpenTTDTrainCollisionOne22Read, owner)});
	Layout(364, "OpenTTDTrainDeleteLastRead", {sizeof(OpenTTDTrainDeleteLastRead), alignof(OpenTTDTrainDeleteLastRead), offsetof(OpenTTDTrainDeleteLastRead, tile), offsetof(OpenTTDTrainDeleteLastRead, owner)});
	Layout(365, "OpenTTDTrainStayDepotRead", {sizeof(OpenTTDTrainStayDepotRead), alignof(OpenTTDTrainStayDepotRead), offsetof(OpenTTDTrainStayDepotRead, tile), offsetof(OpenTTDTrainStayDepotRead, power)});
	Layout(366, "OpenTTDTrainLocoRead", {sizeof(OpenTTDTrainLocoRead), alignof(OpenTTDTrainLocoRead), offsetof(OpenTTDTrainLocoRead, speed), offsetof(OpenTTDTrainLocoRead, status), offsetof(OpenTTDTrainLocoRead, order)});
	Layout(367, "OpenTTDTrainLoco26Read", {sizeof(OpenTTDTrainLoco26Read), alignof(OpenTTDTrainLoco26Read), offsetof(OpenTTDTrainLoco26Read, tile), offsetof(OpenTTDTrainLoco26Read, order_destination), offsetof(OpenTTDTrainLoco26Read, order), offsetof(OpenTTDTrainLoco26Read, nonstop)});
	Layout(368, "OpenTTDTrainTickRead", {sizeof(OpenTTDTrainTickRead), alignof(OpenTTDTrainTickRead), offsetof(OpenTTDTrainTickRead, speed), offsetof(OpenTTDTrainTickRead, status), offsetof(OpenTTDTrainTickRead, running), offsetof(OpenTTDTrainTickRead, front), offsetof(OpenTTDTrainTickRead, free_wagon)});
	Layout(369, "OpenTTDTrainNeedsServiceRead", {sizeof(OpenTTDTrainNeedsServiceRead), alignof(OpenTTDTrainNeedsServiceRead), offsetof(OpenTTDTrainNeedsServiceRead, order_destination), offsetof(OpenTTDTrainNeedsServiceRead, order)});
	Layout(370, "OpenTTDTrainNextForceRead", {sizeof(OpenTTDTrainNextForceRead), alignof(OpenTTDTrainNextForceRead), offsetof(OpenTTDTrainNextForceRead, tile), offsetof(OpenTTDTrainNextForceRead, status)});
	Layout(371, "OpenTTDTrainReverseCommandRead", {sizeof(OpenTTDTrainReverseCommandRead), alignof(OpenTTDTrainReverseCommandRead), offsetof(OpenTTDTrainReverseCommandRead, status), offsetof(OpenTTDTrainReverseCommandRead, breakdown), offsetof(OpenTTDTrainReverseCommandRead, front)});
	Layout(372, "OpenTTDTrainServices", {sizeof(OpenTTDTrainServices), alignof(OpenTTDTrainServices), offsetof(OpenTTDTrainServices, read_first), offsetof(OpenTTDTrainServices, read_next), offsetof(OpenTTDTrainServices, read_previous), offsetof(OpenTTDTrainServices, read_next_unit), offsetof(OpenTTDTrainServices, read_last), offsetof(OpenTTDTrainServices, read_tile), offsetof(OpenTTDTrainServices, read_dest), offsetof(OpenTTDTrainServices, read_order_time), offsetof(OpenTTDTrainServices, read_length), offsetof(OpenTTDTrainServices, read_total_length), offsetof(OpenTTDTrainServices, read_max_track_speed), offsetof(OpenTTDTrainServices, read_speed), offsetof(OpenTTDTrainServices, read_gv_flags), offsetof(OpenTTDTrainServices, read_refit_cap), offsetof(OpenTTDTrainServices, read_last_station), offsetof(OpenTTDTrainServices, read_direction), offsetof(OpenTTDTrainServices, read_status), offsetof(OpenTTDTrainServices, read_tick), offsetof(OpenTTDTrainServices, read_running), offsetof(OpenTTDTrainServices, read_day), offsetof(OpenTTDTrainServices, read_progress), offsetof(OpenTTDTrainServices, read_order), offsetof(OpenTTDTrainServices, read_front), offsetof(OpenTTDTrainServices, read_articulated), offsetof(OpenTTDTrainServices, read_multiheaded), offsetof(OpenTTDTrainServices, read_owner), offsetof(OpenTTDTrainServices, read_vis_effect), offsetof(OpenTTDTrainServices, read_consist_changed), offsetof(OpenTTDTrainServices, read_consist_changed_1), offsetof(OpenTTDTrainServices, read_consist_changed_2), offsetof(OpenTTDTrainServices, read_curve_limit), offsetof(OpenTTDTrainServices, read_stop_location), offsetof(OpenTTDTrainServices, read_current_max_speed), offsetof(OpenTTDTrainServices, read_current_max_speed_6), offsetof(OpenTTDTrainServices, read_update_acceleration), offsetof(OpenTTDTrainServices, read_update_speed), offsetof(OpenTTDTrainServices, read_trackdir), offsetof(OpenTTDTrainServices, read_can_leave), offsetof(OpenTTDTrainServices, read_crossing_approach), offsetof(OpenTTDTrainServices, read_next_offset), offsetof(OpenTTDTrainServices, read_after_swap), offsetof(OpenTTDTrainServices, read_reverse_swap), offsetof(OpenTTDTrainServices, read_approaching_end), offsetof(OpenTTDTrainServices, read_line_ends), offsetof(OpenTTDTrainServices, read_speed_z), offsetof(OpenTTDTrainServices, read_move_vehicle), offsetof(OpenTTDTrainServices, read_move_vehicle_20), offsetof(OpenTTDTrainServices, read_collision_one), offsetof(OpenTTDTrainServices, read_collision_one_22), offsetof(OpenTTDTrainServices, read_delete_last), offsetof(OpenTTDTrainServices, read_stay_depot), offsetof(OpenTTDTrainServices, read_loco), offsetof(OpenTTDTrainServices, read_loco_26), offsetof(OpenTTDTrainServices, read_tick_state), offsetof(OpenTTDTrainServices, read_needs_service), offsetof(OpenTTDTrainServices, read_next_force), offsetof(OpenTTDTrainServices, read_reverse_command), offsetof(OpenTTDTrainServices, write_tile), offsetof(OpenTTDTrainServices, write_dest), offsetof(OpenTTDTrainServices, write_x), offsetof(OpenTTDTrainServices, write_y), offsetof(OpenTTDTrainServices, write_z), offsetof(OpenTTDTrainServices, write_direction), offsetof(OpenTTDTrainServices, write_speed), offsetof(OpenTTDTrainServices, write_tick), offsetof(OpenTTDTrainServices, write_running), offsetof(OpenTTDTrainServices, write_day), offsetof(OpenTTDTrainServices, write_order_time), offsetof(OpenTTDTrainServices, write_progress), offsetof(OpenTTDTrainServices, write_subspeed), offsetof(OpenTTDTrainServices, write_gv_flags), offsetof(OpenTTDTrainServices, write_acceleration), offsetof(OpenTTDTrainServices, write_length), offsetof(OpenTTDTrainServices, write_total_length), offsetof(OpenTTDTrainServices, write_first_engine), offsetof(OpenTTDTrainServices, write_max_speed), offsetof(OpenTTDTrainServices, write_cargo_cap), offsetof(OpenTTDTrainServices, write_refit_cap), offsetof(OpenTTDTrainServices, write_cargo_age), offsetof(OpenTTDTrainServices, write_last_station), offsetof(OpenTTDTrainServices, write_colourmap), offsetof(OpenTTDTrainServices, write_status), offsetof(OpenTTDTrainServices, acceleration), offsetof(OpenTTDTrainServices, acc_model), offsetof(OpenTTDTrainServices, acc_type), offsetof(OpenTTDTrainServices, advance_distance), offsetof(OpenTTDTrainServices, age), offsetof(OpenTTDTrainServices, all_powered), offsetof(OpenTTDTrainServices, ambient_sound), offsetof(OpenTTDTrainServices, arrival_news), offsetof(OpenTTDTrainServices, arrival_triggers), offsetof(OpenTTDTrainServices, axis_diag), offsetof(OpenTTDTrainServices, backoff), offsetof(OpenTTDTrainServices, base_viewport), offsetof(OpenTTDTrainServices, begin_loading), offsetof(OpenTTDTrainServices, bridge_speed), offsetof(OpenTTDTrainServices, cache_override), offsetof(OpenTTDTrainServices, callback_length), offsetof(OpenTTDTrainServices, capacity), offsetof(OpenTTDTrainServices, capacity_error), offsetof(OpenTTDTrainServices, cargo_age_default), offsetof(OpenTTDTrainServices, cargo_changed), offsetof(OpenTTDTrainServices, chain_depot), offsetof(OpenTTDTrainServices, check_breakdown), offsetof(OpenTTDTrainServices, check_next), offsetof(OpenTTDTrainServices, check_orders), offsetof(OpenTTDTrainServices, check_reverse), offsetof(OpenTTDTrainServices, choose_track), offsetof(OpenTTDTrainServices, clear_reservation), offsetof(OpenTTDTrainServices, compatible_rail_owner), offsetof(OpenTTDTrainServices, consist_windows), offsetof(OpenTTDTrainServices, cost_class), offsetof(OpenTTDTrainServices, cost_default), offsetof(OpenTTDTrainServices, cost_divisor), offsetof(OpenTTDTrainServices, count_chain), offsetof(OpenTTDTrainServices, crash_event), offsetof(OpenTTDTrainServices, crash_ground), offsetof(OpenTTDTrainServices, crash_news), offsetof(OpenTTDTrainServices, crash_rating), offsetof(OpenTTDTrainServices, crash_sound), offsetof(OpenTTDTrainServices, crossing_barred), offsetof(OpenTTDTrainServices, crossing_rail_axis), offsetof(OpenTTDTrainServices, crossing_reserved), offsetof(OpenTTDTrainServices, crossing_road_axis), offsetof(OpenTTDTrainServices, crossing_sound), offsetof(OpenTTDTrainServices, curve_advantage), offsetof(OpenTTDTrainServices, curve_mod), offsetof(OpenTTDTrainServices, day_ticks), offsetof(OpenTTDTrainServices, decrease_value), offsetof(OpenTTDTrainServices, delete_vehicle), offsetof(OpenTTDTrainServices, depot_dir), offsetof(OpenTTDTrainServices, depot_dirty), offsetof(OpenTTDTrainServices, depot_index), offsetof(OpenTTDTrainServices, depot_track), offsetof(OpenTTDTrainServices, depot_window), offsetof(OpenTTDTrainServices, diag_axis), offsetof(OpenTTDTrainServices, diag_between), offsetof(OpenTTDTrainServices, diag_reaches_tracks), offsetof(OpenTTDTrainServices, diag_trackdir), offsetof(OpenTTDTrainServices, dirty_tile), offsetof(OpenTTDTrainServices, dir_diag), offsetof(OpenTTDTrainServices, disaster_sound), offsetof(OpenTTDTrainServices, disconnect), offsetof(OpenTTDTrainServices, economy_age), offsetof(OpenTTDTrainServices, engine_power), offsetof(OpenTTDTrainServices, enter_depot), offsetof(OpenTTDTrainServices, enter_tile), offsetof(OpenTTDTrainServices, find_depot), offsetof(OpenTTDTrainServices, first_track), offsetof(OpenTTDTrainServices, free_reservation), offsetof(OpenTTDTrainServices, grf_version), offsetof(OpenTTDTrainServices, handle_breakdown), offsetof(OpenTTDTrainServices, has_depot_res), offsetof(OpenTTDTrainServices, has_reserved), offsetof(OpenTTDTrainServices, has_signal), offsetof(OpenTTDTrainServices, has_signals), offsetof(OpenTTDTrainServices, has_signal_td), offsetof(OpenTTDTrainServices, hide_fill), offsetof(OpenTTDTrainServices, inclination), offsetof(OpenTTDTrainServices, invalidate_grf), offsetof(OpenTTDTrainServices, invalid_price), offsetof(OpenTTDTrainServices, is_bridge), offsetof(OpenTTDTrainServices, is_crossing), offsetof(OpenTTDTrainServices, is_depot), offsetof(OpenTTDTrainServices, is_plain_rail), offsetof(OpenTTDTrainServices, is_railway), offsetof(OpenTTDTrainServices, is_station), offsetof(OpenTTDTrainServices, is_station_any), offsetof(OpenTTDTrainServices, is_tunnelbridge), offsetof(OpenTTDTrainServices, large_explosion), offsetof(OpenTTDTrainServices, last_speed), offsetof(OpenTTDTrainServices, leave_sound), offsetof(OpenTTDTrainServices, leave_station), offsetof(OpenTTDTrainServices, leave_unbunch), offsetof(OpenTTDTrainServices, length_callback), offsetof(OpenTTDTrainServices, length_changed), offsetof(OpenTTDTrainServices, length_default), offsetof(OpenTTDTrainServices, length_error), offsetof(OpenTTDTrainServices, loading), offsetof(OpenTTDTrainServices, local_company), offsetof(OpenTTDTrainServices, lost_warn), offsetof(OpenTTDTrainServices, map_size), offsetof(OpenTTDTrainServices, max_depot_penalty), offsetof(OpenTTDTrainServices, needs_service), offsetof(OpenTTDTrainServices, no_90), offsetof(OpenTTDTrainServices, oneway_blocking), offsetof(OpenTTDTrainServices, order_depot_service), offsetof(OpenTTDTrainServices, order_dummy), offsetof(OpenTTDTrainServices, order_free), offsetof(OpenTTDTrainServices, order_max_speed), offsetof(OpenTTDTrainServices, order_stop), offsetof(OpenTTDTrainServices, other_end), offsetof(OpenTTDTrainServices, pay_running), offsetof(OpenTTDTrainServices, pbs_signal_type), offsetof(OpenTTDTrainServices, platform_ahead), offsetof(OpenTTDTrainServices, platform_length), offsetof(OpenTTDTrainServices, position), offsetof(OpenTTDTrainServices, pow_wag_power)});
	Layout(384, "OpenTTDTrainServices", {sizeof(OpenTTDTrainServices), alignof(OpenTTDTrainServices), offsetof(OpenTTDTrainServices, price), offsetof(OpenTTDTrainServices, process_orders), offsetof(OpenTTDTrainServices, profile), offsetof(OpenTTDTrainServices, property), offsetof(OpenTTDTrainServices, railveh_wagon), offsetof(OpenTTDTrainServices, rail_tilt), offsetof(OpenTTDTrainServices, rail_type), offsetof(OpenTTDTrainServices, rail_types), offsetof(OpenTTDTrainServices, reserve_paths), offsetof(OpenTTDTrainServices, reserve_under), offsetof(OpenTTDTrainServices, reset_unbunch), offsetof(OpenTTDTrainServices, reverse_at_signals), offsetof(OpenTTDTrainServices, reverse_single_blocked), offsetof(OpenTTDTrainServices, reverse_windows), offsetof(OpenTTDTrainServices, running_windows), offsetof(OpenTTDTrainServices, service), offsetof(OpenTTDTrainServices, servint), offsetof(OpenTTDTrainServices, set_depot_res), offsetof(OpenTTDTrainServices, set_next), offsetof(OpenTTDTrainServices, set_platform_res), offsetof(OpenTTDTrainServices, set_signal_state), offsetof(OpenTTDTrainServices, set_tunnel_res), offsetof(OpenTTDTrainServices, show_effect), offsetof(OpenTTDTrainServices, show_reservation), offsetof(OpenTTDTrainServices, signals_both), offsetof(OpenTTDTrainServices, signals_update), offsetof(OpenTTDTrainServices, signals_update_owner), offsetof(OpenTTDTrainServices, signal_has_pbs), offsetof(OpenTTDTrainServices, signal_pbs), offsetof(OpenTTDTrainServices, signal_type), offsetof(OpenTTDTrainServices, sigseg_full), offsetof(OpenTTDTrainServices, sigseg_pbs), offsetof(OpenTTDTrainServices, small_explosion), offsetof(OpenTTDTrainServices, speed_default), offsetof(OpenTTDTrainServices, start_stop_dirty), offsetof(OpenTTDTrainServices, station), offsetof(OpenTTDTrainServices, station_axis), offsetof(OpenTTDTrainServices, station_compatible), offsetof(OpenTTDTrainServices, station_dest), offsetof(OpenTTDTrainServices, stopped_in_depot), offsetof(OpenTTDTrainServices, stop_location), offsetof(OpenTTDTrainServices, stuck_news), offsetof(OpenTTDTrainServices, suppress_implicit), offsetof(OpenTTDTrainServices, ticks_leave_depot), offsetof(OpenTTDTrainServices, tile_add_diag), offsetof(OpenTTDTrainServices, tile_offset_axis), offsetof(OpenTTDTrainServices, tile_offset_diag), offsetof(OpenTTDTrainServices, tile_owner), offsetof(OpenTTDTrainServices, tile_rail_type), offsetof(OpenTTDTrainServices, tile_virt), offsetof(OpenTTDTrainServices, trackdir_exit), offsetof(OpenTTDTrainServices, trackdir_reaches), offsetof(OpenTTDTrainServices, track_bits), offsetof(OpenTTDTrainServices, track_crosses), offsetof(OpenTTDTrainServices, track_direction), offsetof(OpenTTDTrainServices, track_status), offsetof(OpenTTDTrainServices, train_list), offsetof(OpenTTDTrainServices, train_visit), offsetof(OpenTTDTrainServices, truncate_cargo), offsetof(OpenTTDTrainServices, try_path), offsetof(OpenTTDTrainServices, try_reserve), offsetof(OpenTTDTrainServices, tunnel_dir), offsetof(OpenTTDTrainServices, unreserve), offsetof(OpenTTDTrainServices, update_delta), offsetof(OpenTTDTrainServices, update_speed), offsetof(OpenTTDTrainServices, user_default), offsetof(OpenTTDTrainServices, veh_exit_dir), offsetof(OpenTTDTrainServices, viewport), offsetof(OpenTTDTrainServices, view_window), offsetof(OpenTTDTrainServices, visit_type), offsetof(OpenTTDTrainServices, vis_effect), offsetof(OpenTTDTrainServices, wagon_override), offsetof(OpenTTDTrainServices, wagon_speed_limits), offsetof(OpenTTDTrainServices, wait_oneway), offsetof(OpenTTDTrainServices, wait_pbs), offsetof(OpenTTDTrainServices, wait_twoway), offsetof(OpenTTDTrainServices, wait_unbunch), offsetof(OpenTTDTrainServices, write_crossing_bar), offsetof(OpenTTDTrainServices, write_crossing_res), offsetof(OpenTTDTrainServices, write_visit_type), offsetof(OpenTTDTrainServices, visit_tile), offsetof(OpenTTDTrainServices, visit_near)});
	Layout(383, "OpenTTDTrainDepot", {sizeof(OpenTTDTrainDepot), alignof(OpenTTDTrainDepot), offsetof(OpenTTDTrainDepot,tile), offsetof(OpenTTDTrainDepot,length)});
	Layout(373, "OpenTTDTrainReservationFreeRead", {sizeof(OpenTTDTrainReservationFreeRead), alignof(OpenTTDTrainReservationFreeRead), offsetof(OpenTTDTrainReservationFreeRead, tile)});
	Layout(374, "OpenTTDTrainReservationFree1Read", {sizeof(OpenTTDTrainReservationFree1Read), alignof(OpenTTDTrainReservationFree1Read), offsetof(OpenTTDTrainReservationFree1Read, tile), offsetof(OpenTTDTrainReservationFree1Read, next)});
	Layout(375, "OpenTTDTrainReservationNewRead", {sizeof(OpenTTDTrainReservationNewRead), alignof(OpenTTDTrainReservationNewRead), offsetof(OpenTTDTrainReservationNewRead, dest), offsetof(OpenTTDTrainReservationNewRead, last_station), offsetof(OpenTTDTrainReservationNewRead, order_index), offsetof(OpenTTDTrainReservationNewRead, suppress)});
	Layout(376, "OpenTTDTrainReservationChooseRead", {sizeof(OpenTTDTrainReservationChooseRead), alignof(OpenTTDTrainReservationChooseRead), offsetof(OpenTTDTrainReservationChooseRead, tile), offsetof(OpenTTDTrainReservationChooseRead, dest), offsetof(OpenTTDTrainReservationChooseRead, destination), offsetof(OpenTTDTrainReservationChooseRead, order)});
	Layout(377, "OpenTTDTrainReservationChoose4Read", {sizeof(OpenTTDTrainReservationChoose4Read), alignof(OpenTTDTrainReservationChoose4Read), offsetof(OpenTTDTrainReservationChoose4Read, order), offsetof(OpenTTDTrainReservationChoose4Read, nearest)});
	Layout(378, "OpenTTDTrainReservationCheckNextRead", {sizeof(OpenTTDTrainReservationCheckNextRead), alignof(OpenTTDTrainReservationCheckNextRead), offsetof(OpenTTDTrainReservationCheckNextRead, tile), offsetof(OpenTTDTrainReservationCheckNextRead, dest), offsetof(OpenTTDTrainReservationCheckNextRead, destination), offsetof(OpenTTDTrainReservationCheckNextRead, order), offsetof(OpenTTDTrainReservationCheckNextRead, num_orders)});
	Layout(379, "OpenTTDTrainReservationFollow", {sizeof(OpenTTDTrainReservationFollow), alignof(OpenTTDTrainReservationFollow), offsetof(OpenTTDTrainReservationFollow, old_tile), offsetof(OpenTTDTrainReservationFollow, new_tile), offsetof(OpenTTDTrainReservationFollow, skipped), offsetof(OpenTTDTrainReservationFollow, dirs), offsetof(OpenTTDTrainReservationFollow, old_td), offsetof(OpenTTDTrainReservationFollow, exitdir), offsetof(OpenTTDTrainReservationFollow, tunnel), offsetof(OpenTTDTrainReservationFollow, bridge), offsetof(OpenTTDTrainReservationFollow, station), offsetof(OpenTTDTrainReservationFollow, error)});
	Layout(380, "OpenTTDTrainReservationPbs", {sizeof(OpenTTDTrainReservationPbs), alignof(OpenTTDTrainReservationPbs), offsetof(OpenTTDTrainReservationPbs, tile), offsetof(OpenTTDTrainReservationPbs, other), offsetof(OpenTTDTrainReservationPbs, td), offsetof(OpenTTDTrainReservationPbs, okay)});
	Layout(381, "OpenTTDTrainReservationSearch", {sizeof(OpenTTDTrainReservationSearch),alignof(OpenTTDTrainReservationSearch),offsetof(OpenTTDTrainReservationSearch,track),offsetof(OpenTTDTrainReservationSearch,tile),offsetof(OpenTTDTrainReservationSearch,final_dest),offsetof(OpenTTDTrainReservationSearch,td),offsetof(OpenTTDTrainReservationSearch,found),offsetof(OpenTTDTrainReservationSearch,okay)});
	Layout(382, "OpenTTDTrainReservationLeaves", {sizeof(OpenTTDTrainReservationLeaves), alignof(OpenTTDTrainReservationLeaves), offsetof(OpenTTDTrainReservationLeaves, read_tile), offsetof(OpenTTDTrainReservationLeaves, read_next), offsetof(OpenTTDTrainReservationLeaves, read_last_station), offsetof(OpenTTDTrainReservationLeaves, read_direction), offsetof(OpenTTDTrainReservationLeaves, read_order), offsetof(OpenTTDTrainReservationLeaves, read_num_orders), offsetof(OpenTTDTrainReservationLeaves, read_order_index), offsetof(OpenTTDTrainReservationLeaves, read_free), offsetof(OpenTTDTrainReservationLeaves, read_free_1), offsetof(OpenTTDTrainReservationLeaves, read_new), offsetof(OpenTTDTrainReservationLeaves, read_choose), offsetof(OpenTTDTrainReservationLeaves, read_choose_4), offsetof(OpenTTDTrainReservationLeaves, read_check_next), offsetof(OpenTTDTrainReservationLeaves, all_compat), offsetof(OpenTTDTrainReservationLeaves, backoff), offsetof(OpenTTDTrainReservationLeaves, bits_track), offsetof(OpenTTDTrainReservationLeaves, blocking), offsetof(OpenTTDTrainReservationLeaves, check_reverse), offsetof(OpenTTDTrainReservationLeaves, compat_station), offsetof(OpenTTDTrainReservationLeaves, conditional), offsetof(OpenTTDTrainReservationLeaves, copy_order), offsetof(OpenTTDTrainReservationLeaves, cross_dirs), offsetof(OpenTTDTrainReservationLeaves, cross_tracks), offsetof(OpenTTDTrainReservationLeaves, depot_dir), offsetof(OpenTTDTrainReservationLeaves, depot_reserved), offsetof(OpenTTDTrainReservationLeaves, diag_reach_dirs), offsetof(OpenTTDTrainReservationLeaves, diag_track), offsetof(OpenTTDTrainReservationLeaves, enter_td), offsetof(OpenTTDTrainReservationLeaves, exit_dir), offsetof(OpenTTDTrainReservationLeaves, free), offsetof(OpenTTDTrainReservationLeaves, green), offsetof(OpenTTDTrainReservationLeaves, has_pbs), offsetof(OpenTTDTrainReservationLeaves, has_reserved), offsetof(OpenTTDTrainReservationLeaves, has_signal), offsetof(OpenTTDTrainReservationLeaves, increment_order), offsetof(OpenTTDTrainReservationLeaves, is_bridge), offsetof(OpenTTDTrainReservationLeaves, is_depot), offsetof(OpenTTDTrainReservationLeaves, is_pbs), offsetof(OpenTTDTrainReservationLeaves, is_plain), offsetof(OpenTTDTrainReservationLeaves, is_railway), offsetof(OpenTTDTrainReservationLeaves, is_station), offsetof(OpenTTDTrainReservationLeaves, is_tunnel), offsetof(OpenTTDTrainReservationLeaves, is_waypoint), offsetof(OpenTTDTrainReservationLeaves, line_reverse), offsetof(OpenTTDTrainReservationLeaves, mark_bridge), offsetof(OpenTTDTrainReservationLeaves, mark_tile), offsetof(OpenTTDTrainReservationLeaves, needs_service), offsetof(OpenTTDTrainReservationLeaves, oneway), offsetof(OpenTTDTrainReservationLeaves, order_service), offsetof(OpenTTDTrainReservationLeaves, order_stop), offsetof(OpenTTDTrainReservationLeaves, order_type), offsetof(OpenTTDTrainReservationLeaves, other_end), offsetof(OpenTTDTrainReservationLeaves, overlap), offsetof(OpenTTDTrainReservationLeaves, path_result), offsetof(OpenTTDTrainReservationLeaves, profile), offsetof(OpenTTDTrainReservationLeaves, rail90), offsetof(OpenTTDTrainReservationLeaves, reach_dirs), offsetof(OpenTTDTrainReservationLeaves, reach_tracks), offsetof(OpenTTDTrainReservationLeaves, reserved), offsetof(OpenTTDTrainReservationLeaves, reserve_paths), offsetof(OpenTTDTrainReservationLeaves, restore_order), offsetof(OpenTTDTrainReservationLeaves, safe), offsetof(OpenTTDTrainReservationLeaves, save_order), offsetof(OpenTTDTrainReservationLeaves, service), offsetof(OpenTTDTrainReservationLeaves, set_depot), offsetof(OpenTTDTrainReservationLeaves, set_depot_dest), offsetof(OpenTTDTrainReservationLeaves, set_platform), offsetof(OpenTTDTrainReservationLeaves, set_signal), offsetof(OpenTTDTrainReservationLeaves, set_tunnel), offsetof(OpenTTDTrainReservationLeaves, show_res), offsetof(OpenTTDTrainReservationLeaves, signal_buffer), offsetof(OpenTTDTrainReservationLeaves, start_stop), offsetof(OpenTTDTrainReservationLeaves, station), offsetof(OpenTTDTrainReservationLeaves, station_train), offsetof(OpenTTDTrainReservationLeaves, station_xy), offsetof(OpenTTDTrainReservationLeaves, stuck), offsetof(OpenTTDTrainReservationLeaves, tile_add), offsetof(OpenTTDTrainReservationLeaves, tile_offset), offsetof(OpenTTDTrainReservationLeaves, trackdir), offsetof(OpenTTDTrainReservationLeaves, track_status), offsetof(OpenTTDTrainReservationLeaves, try_track), offsetof(OpenTTDTrainReservationLeaves, tunnel_dir), offsetof(OpenTTDTrainReservationLeaves, tunnel_free), offsetof(OpenTTDTrainReservationLeaves, unreserve), offsetof(OpenTTDTrainReservationLeaves, update_buffer), offsetof(OpenTTDTrainReservationLeaves, write_dest), offsetof(OpenTTDTrainReservationLeaves, write_last), offsetof(OpenTTDTrainReservationLeaves, write_suppress), offsetof(OpenTTDTrainReservationLeaves, follow), offsetof(OpenTTDTrainReservationLeaves, origin), offsetof(OpenTTDTrainReservationLeaves, pathfind), offsetof(OpenTTDTrainReservationLeaves, safe_track), offsetof(OpenTTDTrainReservationLeaves, process_orders), offsetof(OpenTTDTrainReservationLeaves, update_order_dest)});
	Layout(385, "OpenTTDTrainReservationChoice", {sizeof(OpenTTDTrainReservationChoice),alignof(OpenTTDTrainReservationChoice),offsetof(OpenTTDTrainReservationChoice,track),offsetof(OpenTTDTrainReservationChoice,reserved)});
	Layout(49, "OpenTTDCargoSpec", {sizeof(OpenTTDCargoSpec), alignof(OpenTTDCargoSpec), offsetof(OpenTTDCargoSpec, payment), offsetof(OpenTTDCargoSpec, valid), offsetof(OpenTTDCargoSpec, callback), offsetof(OpenTTDCargoSpec, periods1), offsetof(OpenTTDCargoSpec, periods2)});
	Layout(50, "OpenTTDCargoPaymentFields", {sizeof(OpenTTDCargoPaymentFields), alignof(OpenTTDCargoPaymentFields), offsetof(OpenTTDCargoPaymentFields, front), offsetof(OpenTTDCargoPaymentFields, route_profit), offsetof(OpenTTDCargoPaymentFields, visual_profit), offsetof(OpenTTDCargoPaymentFields, visual_transfer)});
	Layout(51, "OpenTTDCargoServices", {sizeof(OpenTTDCargoServices), alignof(OpenTTDCargoServices), offsetof(OpenTTDCargoServices, spec), offsetof(OpenTTDCargoServices, callback), offsetof(OpenTTDCargoServices, near), offsetof(OpenTTDCargoServices, station_read), offsetof(OpenTTDCargoServices, industry_read), offsetof(OpenTTDCargoServices, industry_write), offsetof(OpenTTDCargoServices, refuses), offsetof(OpenTTDCargoServices, accept), offsetof(OpenTTDCargoServices, statistics), offsetof(OpenTTDCargoServices, monitor), offsetof(OpenTTDCargoServices, subsidised), offsetof(OpenTTDCargoServices, industry_effect), offsetof(OpenTTDCargoServices, vehicle_read), offsetof(OpenTTDCargoServices, settle), offsetof(OpenTTDCargoServices, feeder), offsetof(OpenTTDCargoServices, setting)});
	Layout(52, "OpenTTDShipYapfInput", {sizeof(OpenTTDShipYapfInput), alignof(OpenTTDShipYapfInput), offsetof(OpenTTDShipYapfInput, map_x), offsetof(OpenTTDShipYapfInput, map_y), offsetof(OpenTTDShipYapfInput, tile), offsetof(OpenTTDShipYapfInput, dest_tile), offsetof(OpenTTDShipYapfInput, curve90), offsetof(OpenTTDShipYapfInput, curve45), offsetof(OpenTTDShipYapfInput, max_speed), offsetof(OpenTTDShipYapfInput, dest_dirs), offsetof(OpenTTDShipYapfInput, reverse_dirs), offsetof(OpenTTDShipYapfInput, trackdir), offsetof(OpenTTDShipYapfInput, ocean_frac), offsetof(OpenTTDShipYapfInput, canal_frac), offsetof(OpenTTDShipYapfInput, station), offsetof(OpenTTDShipYapfInput, unit_number)});
	Layout(53, "OpenTTDShipYapfLeaves", {sizeof(OpenTTDShipYapfLeaves), alignof(OpenTTDShipYapfLeaves), offsetof(OpenTTDShipYapfLeaves, destination), offsetof(OpenTTDShipYapfLeaves, follow), offsetof(OpenTTDShipYapfLeaves, tile), offsetof(OpenTTDShipYapfLeaves, patch), offsetof(OpenTTDShipYapfLeaves, visit_new), offsetof(OpenTTDShipYapfLeaves, visit_next), offsetof(OpenTTDShipYapfLeaves, visit_destroy), offsetof(OpenTTDShipYapfLeaves, debug)});
	Layout(54, "OpenTTDShipFollow", {sizeof(OpenTTDShipFollow), alignof(OpenTTDShipFollow), offsetof(OpenTTDShipFollow, tile), offsetof(OpenTTDShipFollow, skipped), offsetof(OpenTTDShipFollow, dirs), offsetof(OpenTTDShipFollow, followed)});
	Layout(55, "OpenTTDShipTile", {sizeof(OpenTTDShipTile), alignof(OpenTTDShipTile), offsetof(OpenTTDShipTile, ships), offsetof(OpenTTDShipTile, docking), offsetof(OpenTTDShipTile, sea), offsetof(OpenTTDShipTile, lock_middle), offsetof(OpenTTDShipTile, destination)});
	Layout(56, "OpenTTDShipYapfResult", {sizeof(OpenTTDShipYapfResult), alignof(OpenTTDShipYapfResult), offsetof(OpenTTDShipYapfResult, direction), offsetof(OpenTTDShipYapfResult, found), offsetof(OpenTTDShipYapfResult, origin), offsetof(OpenTTDShipYapfResult, stats)});

	Layout(60, "OpenTTDTownAction", {sizeof(OpenTTDTownAction), alignof(OpenTTDTownAction), offsetof(OpenTTDTownAction, kind), offsetof(OpenTTDTownAction, town), offsetof(OpenTTDTownAction, tile), offsetof(OpenTTDTownAction, a), offsetof(OpenTTDTownAction, b), offsetof(OpenTTDTownAction, c), offsetof(OpenTTDTownAction, d), offsetof(OpenTTDTownAction, cost)});
	Layout(61, "OpenTTDTownLeaves", {sizeof(OpenTTDTownLeaves), alignof(OpenTTDTownLeaves), offsetof(OpenTTDTownLeaves, observe), offsetof(OpenTTDTownLeaves, leaf), offsetof(OpenTTDTownLeaves, state), offsetof(OpenTTDTownLeaves, stations)});
	Layout(90, "OpenTTDRailSettings", {sizeof(OpenTTDRailSettings), alignof(OpenTTDRailSettings), offsetof(OpenTTDRailSettings, max_nodes), offsetof(OpenTTDRailSettings, firstred), offsetof(OpenTTDRailSettings, firstred_exit), offsetof(OpenTTDRailSettings, lastred), offsetof(OpenTTDRailSettings, lastred_exit), offsetof(OpenTTDRailSettings, station), offsetof(OpenTTDRailSettings, slope), offsetof(OpenTTDRailSettings, curve45), offsetof(OpenTTDRailSettings, curve90), offsetof(OpenTTDRailSettings, depot_reverse), offsetof(OpenTTDRailSettings, crossing), offsetof(OpenTTDRailSettings, lookahead), offsetof(OpenTTDRailSettings, p0), offsetof(OpenTTDRailSettings, p1), offsetof(OpenTTDRailSettings, p2), offsetof(OpenTTDRailSettings, pbs_cross), offsetof(OpenTTDRailSettings, pbs_station), offsetof(OpenTTDRailSettings, pbs_back), offsetof(OpenTTDRailSettings, doubleslip), offsetof(OpenTTDRailSettings, longer), offsetof(OpenTTDRailSettings, longer_tile), offsetof(OpenTTDRailSettings, shorter), offsetof(OpenTTDRailSettings, shorter_tile), offsetof(OpenTTDRailSettings, firstred_eol)});
	Layout(91, "OpenTTDRailTile", {sizeof(OpenTTDRailTile), alignof(OpenTTDRailTile), offsetof(OpenTTDRailTile, flags), offsetof(OpenTTDRailTile, other_end), offsetof(OpenTTDRailTile, station), offsetof(OpenTTDRailTile, railtype), offsetof(OpenTTDRailTile, tracks), offsetof(OpenTTDRailTile, reserved), offsetof(OpenTTDRailTile, station_track), offsetof(OpenTTDRailTile, tunnel_dir), offsetof(OpenTTDRailTile, uphill), offsetof(OpenTTDRailTile, flat_ramp), offsetof(OpenTTDRailTile, signal_along), offsetof(OpenTTDRailTile, signal_against), offsetof(OpenTTDRailTile, signal_green), offsetof(OpenTTDRailTile, signal_type), offsetof(OpenTTDRailTile, oneway)});
	Layout(92, "OpenTTDRailFollow", {sizeof(OpenTTDRailFollow), alignof(OpenTTDRailFollow), offsetof(OpenTTDRailFollow, tile), offsetof(OpenTTDRailFollow, skipped), offsetof(OpenTTDRailFollow, min_speed), offsetof(OpenTTDRailFollow, max_speed), offsetof(OpenTTDRailFollow, dirs), offsetof(OpenTTDRailFollow, followed), offsetof(OpenTTDRailFollow, error), offsetof(OpenTTDRailFollow, station)});
	Layout(93, "OpenTTDRailLeaves", {sizeof(OpenTTDRailLeaves), alignof(OpenTTDRailLeaves), offsetof(OpenTTDRailLeaves, train), offsetof(OpenTTDRailLeaves, tile), offsetof(OpenTTDRailLeaves, follow), offsetof(OpenTTDRailLeaves, safe), offsetof(OpenTTDRailLeaves, free), offsetof(OpenTTDRailLeaves, compatible_station), offsetof(OpenTTDRailLeaves, platform_length), offsetof(OpenTTDRailLeaves, closest_station), offsetof(OpenTTDRailLeaves, destination_dirs), offsetof(OpenTTDRailLeaves, origin), offsetof(OpenTTDRailLeaves, write), offsetof(OpenTTDRailLeaves, output), offsetof(OpenTTDRailLeaves, debug)});
	Layout(94, "OpenTTDRailInput", {sizeof(OpenTTDRailInput), alignof(OpenTTDRailInput), offsetof(OpenTTDRailInput, context), offsetof(OpenTTDRailInput, settings), offsetof(OpenTTDRailInput, map_x), offsetof(OpenTTDRailInput, tile), offsetof(OpenTTDRailInput, max_cost), offsetof(OpenTTDRailInput, desync), offsetof(OpenTTDRailInput, kind), offsetof(OpenTTDRailInput, td), offsetof(OpenTTDRailInput, override_railtype), offsetof(OpenTTDRailInput, forbid90), offsetof(OpenTTDRailInput, reserve)});
	Layout(95, "OpenTTDRailStep", {sizeof(OpenTTDRailStep), alignof(OpenTTDRailStep), offsetof(OpenTTDRailStep, tile), offsetof(OpenTTDRailStep, destination), offsetof(OpenTTDRailStep, target_tile), offsetof(OpenTTDRailStep, best_length), offsetof(OpenTTDRailStep, action), offsetof(OpenTTDRailStep, td), offsetof(OpenTTDRailStep, found), offsetof(OpenTTDRailStep, reverse), offsetof(OpenTTDRailStep, value), offsetof(OpenTTDRailStep, target_td), offsetof(OpenTTDRailStep, target_okay)});
	Layout(97, "OpenTTDRailTrain", {sizeof(OpenTTDRailTrain), alignof(OpenTTDRailTrain), offsetof(OpenTTDRailTrain, compatible), offsetof(OpenTTDRailTrain, all_compatible), offsetof(OpenTTDRailTrain, tile), offsetof(OpenTTDRailTrain, rear_tile), offsetof(OpenTTDRailTrain, virtual_tile), offsetof(OpenTTDRailTrain, rear_virtual_tile), offsetof(OpenTTDRailTrain, dest_tile), offsetof(OpenTTDRailTrain, length), offsetof(OpenTTDRailTrain, speed), offsetof(OpenTTDRailTrain, order_destination), offsetof(OpenTTDRailTrain, td), offsetof(OpenTTDRailTrain, rear_td), offsetof(OpenTTDRailTrain, wormhole), offsetof(OpenTTDRailTrain, rear_wormhole), offsetof(OpenTTDRailTrain, order), offsetof(OpenTTDRailTrain, nearest_depot), offsetof(OpenTTDRailTrain, complex_waypoint)});

	Layout(280, "OpenTTDCargoPacketFields", {sizeof(OpenTTDCargoPacketFields), alignof(OpenTTDCargoPacketFields), offsetof(OpenTTDCargoPacketFields, feeder_share), offsetof(OpenTTDCargoPacketFields, source_xy), offsetof(OpenTTDCargoPacketFields, count), offsetof(OpenTTDCargoPacketFields, periods_in_transit), offsetof(OpenTTDCargoPacketFields, first_station), offsetof(OpenTTDCargoPacketFields, next_hop), offsetof(OpenTTDCargoPacketFields, source_id), offsetof(OpenTTDCargoPacketFields, travelled_x), offsetof(OpenTTDCargoPacketFields, travelled_y), offsetof(OpenTTDCargoPacketFields, source_type), offsetof(OpenTTDCargoPacketFields, in_vehicle)});
	Layout(281, "OpenTTDCargoListFields", {sizeof(OpenTTDCargoListFields), alignof(OpenTTDCargoListFields), offsetof(OpenTTDCargoListFields, cargo_periods_in_transit), offsetof(OpenTTDCargoListFields, feeder_share), offsetof(OpenTTDCargoListFields, count), offsetof(OpenTTDCargoListFields, reserved_count), offsetof(OpenTTDCargoListFields, action_counts)});
	Layout(282, "OpenTTDCargoStorageServices", {sizeof(OpenTTDCargoStorageServices), alignof(OpenTTDCargoStorageServices), offsetof(OpenTTDCargoStorageServices, can_allocate), offsetof(OpenTTDCargoStorageServices, create), offsetof(OpenTTDCargoStorageServices, packet), offsetof(OpenTTDCargoStorageServices, destroy), offsetof(OpenTTDCargoStorageServices, random), offsetof(OpenTTDCargoStorageServices, coordinate), offsetof(OpenTTDCargoStorageServices, flow), offsetof(OpenTTDCargoStorageServices, pay), offsetof(OpenTTDCargoStorageServices, origin), offsetof(OpenTTDCargoStorageServices, flow_owner), offsetof(OpenTTDCargoStorageServices, random_draw), offsetof(OpenTTDCargoStorageServices, packet_next)});
	Layout(300, "OpenTTDCargoShare", {sizeof(OpenTTDCargoShare), alignof(OpenTTDCargoShare), offsetof(OpenTTDCargoShare, cumulative), offsetof(OpenTTDCargoShare, station), offsetof(OpenTTDCargoShare, found)});
	Layout(301, "OpenTTDCargoOrigin", {sizeof(OpenTTDCargoOrigin), alignof(OpenTTDCargoOrigin), offsetof(OpenTTDCargoOrigin, flow), offsetof(OpenTTDCargoOrigin, origin), offsetof(OpenTTDCargoOrigin, found)});
	Layout(302, "OpenTTDCargoFlowServices", {sizeof(OpenTTDCargoFlowServices), alignof(OpenTTDCargoFlowServices), offsetof(OpenTTDCargoFlowServices, context), offsetof(OpenTTDCargoFlowServices, read), offsetof(OpenTTDCargoFlowServices, job_flows), offsetof(OpenTTDCargoFlowServices, live_flows), offsetof(OpenTTDCargoFlowServices, reroute), offsetof(OpenTTDCargoFlowServices, finish)});
	Layout(310, "OpenTTDCargoCapacityVehicle", {sizeof(OpenTTDCargoCapacityVehicle), alignof(OpenTTDCargoCapacityVehicle), offsetof(OpenTTDCargoCapacityVehicle, list), offsetof(OpenTTDCargoCapacityVehicle, capacity), offsetof(OpenTTDCargoCapacityVehicle, cargo), offsetof(OpenTTDCargoCapacityVehicle, train), offsetof(OpenTTDCargoCapacityVehicle, articulated)});
	Layout(311, "OpenTTDCargoCapacityServices", {sizeof(OpenTTDCargoCapacityServices), alignof(OpenTTDCargoCapacityServices), offsetof(OpenTTDCargoCapacityServices, read), offsetof(OpenTTDCargoCapacityServices, next_part), offsetof(OpenTTDCargoCapacityServices, last_engine_part), offsetof(OpenTTDCargoCapacityServices, other_multiheaded_part), offsetof(OpenTTDCargoCapacityServices, cargo)});
	Layout(260, "OpenTTDOrderFields", {sizeof(OpenTTDOrderFields), alignof(OpenTTDOrderFields), offsetof(OpenTTDOrderFields, type), offsetof(OpenTTDOrderFields, flags), offsetof(OpenTTDOrderFields, destination), offsetof(OpenTTDOrderFields, refit_cargo), offsetof(OpenTTDOrderFields, wait_time), offsetof(OpenTTDOrderFields, travel_time), offsetof(OpenTTDOrderFields, max_speed)});
	Layout(261, "OpenTTDConsistState", {sizeof(OpenTTDConsistState), alignof(OpenTTDConsistState), offsetof(OpenTTDConsistState, current_order_time), offsetof(OpenTTDConsistState, lateness_counter), offsetof(OpenTTDConsistState, timetable_start), offsetof(OpenTTDConsistState, last_departure), offsetof(OpenTTDConsistState, next_departure), offsetof(OpenTTDConsistState, round_trip_time), offsetof(OpenTTDConsistState, real_index), offsetof(OpenTTDConsistState, implicit_index), offsetof(OpenTTDConsistState, vehicle_flags)});
	Layout(262, "OpenTTDVehicleOrderState", {sizeof(OpenTTDVehicleOrderState), alignof(OpenTTDVehicleOrderState), offsetof(OpenTTDVehicleOrderState, current), offsetof(OpenTTDVehicleOrderState, orders), offsetof(OpenTTDVehicleOrderState, next_shared), offsetof(OpenTTDVehicleOrderState, previous_shared)});
	Layout(263, "OpenTTDOrderListState", {sizeof(OpenTTDOrderListState), alignof(OpenTTDOrderListState), offsetof(OpenTTDOrderListState, manual), offsetof(OpenTTDOrderListState, vehicles), offsetof(OpenTTDOrderListState, first_shared), offsetof(OpenTTDOrderListState, timetable_duration), offsetof(OpenTTDOrderListState, total_duration)});
	Layout(264, "OpenTTDOrderBackupState", {sizeof(OpenTTDOrderBackupState), alignof(OpenTTDOrderBackupState), offsetof(OpenTTDOrderBackupState, user), offsetof(OpenTTDOrderBackupState, tile), offsetof(OpenTTDOrderBackupState, group), offsetof(OpenTTDOrderBackupState, clone)});
	Layout(265, "OpenTTDOrdersLeaves", {sizeof(OpenTTDOrdersLeaves), alignof(OpenTTDOrdersLeaves), offsetof(OpenTTDOrdersLeaves, vehicle), offsetof(OpenTTDOrdersLeaves, consist), offsetof(OpenTTDOrdersLeaves, list), offsetof(OpenTTDOrdersLeaves, vector), offsetof(OpenTTDOrdersLeaves, backup), offsetof(OpenTTDOrdersLeaves, backup_vector), offsetof(OpenTTDOrdersLeaves, backup_consist), offsetof(OpenTTDOrdersLeaves, first_vehicle), offsetof(OpenTTDOrdersLeaves, last_station), offsetof(OpenTTDOrdersLeaves, ownerless_station), offsetof(OpenTTDOrdersLeaves, vehicle_type), offsetof(OpenTTDOrdersLeaves, vehicle_status), offsetof(OpenTTDOrdersLeaves, tick_counter), offsetof(OpenTTDOrdersLeaves, primary_vehicle), offsetof(OpenTTDOrdersLeaves, vehicle_ownership), offsetof(OpenTTDOrdersLeaves, ticks_per_second), offsetof(OpenTTDOrdersLeaves, unit_number), offsetof(OpenTTDOrdersLeaves, economy_date), offsetof(OpenTTDOrdersLeaves, economy_fraction), offsetof(OpenTTDOrdersLeaves, maximum_date), offsetof(OpenTTDOrdersLeaves, timetable_year_limit), offsetof(OpenTTDOrdersLeaves, stopped_or_crashed), offsetof(OpenTTDOrdersLeaves, allocate_list), offsetof(OpenTTDOrdersLeaves, suppress_implicit), offsetof(OpenTTDOrdersLeaves, shared_window), offsetof(OpenTTDOrdersLeaves, vehicle_id), offsetof(OpenTTDOrdersLeaves, percent_filled), offsetof(OpenTTDOrdersLeaves, reliability), offsetof(OpenTTDOrdersLeaves, engine_reliability), offsetof(OpenTTDOrdersLeaves, display_speed), offsetof(OpenTTDOrdersLeaves, age_years), offsetof(OpenTTDOrdersLeaves, needs_service), offsetof(OpenTTDOrdersLeaves, remaining_years), offsetof(OpenTTDOrdersLeaves, airport_tile), offsetof(OpenTTDOrdersLeaves, base_station_tile), offsetof(OpenTTDOrdersLeaves, station_tile), offsetof(OpenTTDOrdersLeaves, depot_tile), offsetof(OpenTTDOrdersLeaves, distance), offsetof(OpenTTDOrdersLeaves, station_location), offsetof(OpenTTDOrdersLeaves, destination_tile), offsetof(OpenTTDOrdersLeaves, aircraft_flying), offsetof(OpenTTDOrdersLeaves, target_airport), offsetof(OpenTTDOrdersLeaves, waypoint_tile), offsetof(OpenTTDOrdersLeaves, at_station), offsetof(OpenTTDOrdersLeaves, tile_station), offsetof(OpenTTDOrdersLeaves, ship_station_tile), offsetof(OpenTTDOrdersLeaves, valid_station), offsetof(OpenTTDOrdersLeaves, station_owner), offsetof(OpenTTDOrdersLeaves, can_use_station), offsetof(OpenTTDOrdersLeaves, owner_check), offsetof(OpenTTDOrdersLeaves, station_error), offsetof(OpenTTDOrdersLeaves, has_hangar), offsetof(OpenTTDOrdersLeaves, valid_depot), offsetof(OpenTTDOrdersLeaves, depot_owner), offsetof(OpenTTDOrdersLeaves, rail_depot), offsetof(OpenTTDOrdersLeaves, road_depot), offsetof(OpenTTDOrdersLeaves, ship_depot), offsetof(OpenTTDOrdersLeaves, valid_waypoint), offsetof(OpenTTDOrdersLeaves, waypoint_facilities), offsetof(OpenTTDOrdersLeaves, waypoint_owner), offsetof(OpenTTDOrdersLeaves, list_capacity), offsetof(OpenTTDOrdersLeaves, next_backup), offsetof(OpenTTDOrdersLeaves, next_vehicle), offsetof(OpenTTDOrdersLeaves, aircraft_range), offsetof(OpenTTDOrdersLeaves, aircraft_range_square), offsetof(OpenTTDOrdersLeaves, bus), offsetof(OpenTTDOrdersLeaves, backup_capacity), offsetof(OpenTTDOrdersLeaves, create_backup), offsetof(OpenTTDOrdersLeaves, networking), offsetof(OpenTTDOrdersLeaves, network_server), offsetof(OpenTTDOrdersLeaves, network_client), offsetof(OpenTTDOrdersLeaves, server_client), offsetof(OpenTTDOrdersLeaves, default_group), offsetof(OpenTTDOrdersLeaves, vehicle_tile), offsetof(OpenTTDOrdersLeaves, vehicle_group), offsetof(OpenTTDOrdersLeaves, unique_backup_name), offsetof(OpenTTDOrdersLeaves, backup_id), offsetof(OpenTTDOrdersLeaves, backup_hangar), offsetof(OpenTTDOrdersLeaves, review_setting), offsetof(OpenTTDOrdersLeaves, local_owner), offsetof(OpenTTDOrdersLeaves, day_counter), offsetof(OpenTTDOrdersLeaves, fast_aircraft), offsetof(OpenTTDOrdersLeaves, short_strip), offsetof(OpenTTDOrdersLeaves, no_jet_crash), offsetof(OpenTTDOrdersLeaves, append_station), offsetof(OpenTTDOrdersLeaves, invalidate_station_list), offsetof(OpenTTDOrdersLeaves, command_error), offsetof(OpenTTDOrdersLeaves, timetable_dirty), offsetof(OpenTTDOrdersLeaves, invalidate_order), offsetof(OpenTTDOrdersLeaves, vehicle_dirty), offsetof(OpenTTDOrdersLeaves, delete_order_news), offsetof(OpenTTDOrdersLeaves, suppress_implicit_write), offsetof(OpenTTDOrdersLeaves, invalidate_vehicle_list), offsetof(OpenTTDOrdersLeaves, close_shared_window), offsetof(OpenTTDOrdersLeaves, invalidate_shared_window), offsetof(OpenTTDOrdersLeaves, last_station_write), offsetof(OpenTTDOrdersLeaves, dirty_vehicle_windows), offsetof(OpenTTDOrdersLeaves, capture_backup_metadata), offsetof(OpenTTDOrdersLeaves, clear_backup_name), offsetof(OpenTTDOrdersLeaves, restore_backup_metadata), offsetof(OpenTTDOrdersLeaves, order_news), offsetof(OpenTTDOrdersLeaves, debug_list), offsetof(OpenTTDOrdersLeaves, assert_departure_range), offsetof(OpenTTDOrdersLeaves, delete_list), offsetof(OpenTTDOrdersLeaves, leave_station), offsetof(OpenTTDOrdersLeaves, reverse_train), offsetof(OpenTTDOrdersLeaves, next_airport), offsetof(OpenTTDOrdersLeaves, set_destination), offsetof(OpenTTDOrdersLeaves, closest_depot), offsetof(OpenTTDOrdersLeaves, share_command), offsetof(OpenTTDOrdersLeaves, group_command), offsetof(OpenTTDOrdersLeaves, delete_backup), offsetof(OpenTTDOrdersLeaves, clear_backup_gui), offsetof(OpenTTDOrdersLeaves, clear_backup_post), offsetof(OpenTTDOrdersLeaves, missing_aircraft_orders), offsetof(OpenTTDOrdersLeaves, change_timetable_command)});
	Layout(266, "OpenTTDOrdersClosest", {sizeof(OpenTTDOrdersClosest), alignof(OpenTTDOrdersClosest), offsetof(OpenTTDOrdersClosest, tile), offsetof(OpenTTDOrdersClosest, destination), offsetof(OpenTTDOrdersClosest, reverse), offsetof(OpenTTDOrdersClosest, found)});
	CHECK(openttd_rust_abi_layout(255, 0) == SIZE_MAX);
	CHECK(static_cast<size_t>(PTRDIFF_MAX) == (SIZE_MAX >> 1));
	std::printf("pointer_bytes %zu sentinel %zu borrow_limit %zu\n", sizeof(void *), SIZE_MAX, static_cast<size_t>(PTRDIFF_MAX));
	Layout(29, "OpenTTDListControlInput", {sizeof(OpenTTDListControlInput), alignof(OpenTTDListControlInput), offsetof(OpenTTDListControlInput, a), offsetof(OpenTTDListControlInput, b), offsetof(OpenTTDListControlInput, flag), offsetof(OpenTTDListControlInput, kind)});
	Layout(30, "OpenTTDListControlAction", {sizeof(OpenTTDListControlAction), alignof(OpenTTDListControlAction), offsetof(OpenTTDListControlAction, a), offsetof(OpenTTDListControlAction, kind)});

	Layout(100, "OpenTTDRoadYapfInput", {sizeof(OpenTTDRoadYapfInput), alignof(OpenTTDRoadYapfInput), offsetof(OpenTTDRoadYapfInput, map_x), offsetof(OpenTTDRoadYapfInput, map_y), offsetof(OpenTTDRoadYapfInput, tile), offsetof(OpenTTDRoadYapfInput, dest_tile), offsetof(OpenTTDRoadYapfInput, max_nodes), offsetof(OpenTTDRoadYapfInput, slope), offsetof(OpenTTDRoadYapfInput, crossing), offsetof(OpenTTDRoadYapfInput, stop), offsetof(OpenTTDRoadYapfInput, occupied), offsetof(OpenTTDRoadYapfInput, bay), offsetof(OpenTTDRoadYapfInput, curve), offsetof(OpenTTDRoadYapfInput, display_speed), offsetof(OpenTTDRoadYapfInput, order_destination), offsetof(OpenTTDRoadYapfInput, order_speed), offsetof(OpenTTDRoadYapfInput, order_type), offsetof(OpenTTDRoadYapfInput, bus), offsetof(OpenTTDRoadYapfInput, articulated), offsetof(OpenTTDRoadYapfInput, trackdir)});
	Layout(101, "OpenTTDRoadYapfTile", {sizeof(OpenTTDRoadYapfTile), alignof(OpenTTDRoadYapfTile), offsetof(OpenTTDRoadYapfTile, occupied), offsetof(OpenTTDRoadYapfTile, length), offsetof(OpenTTDRoadYapfTile, station), offsetof(OpenTTDRoadYapfTile, type), offsetof(OpenTTDRoadYapfTile, station_type), offsetof(OpenTTDRoadYapfTile, depot), offsetof(OpenTTDRoadYapfTile, depot_dir), offsetof(OpenTTDRoadYapfTile, crossing), offsetof(OpenTTDRoadYapfTile, waypoint), offsetof(OpenTTDRoadYapfTile, drive_through), offsetof(OpenTTDRoadYapfTile, continuation), offsetof(OpenTTDRoadYapfTile, busy_bays)});
	Layout(102, "OpenTTDRoadYapfFollow", {sizeof(OpenTTDRoadYapfFollow), alignof(OpenTTDRoadYapfFollow), offsetof(OpenTTDRoadYapfFollow, tile), offsetof(OpenTTDRoadYapfFollow, skipped), offsetof(OpenTTDRoadYapfFollow, max_speed), offsetof(OpenTTDRoadYapfFollow, min_speed), offsetof(OpenTTDRoadYapfFollow, dirs), offsetof(OpenTTDRoadYapfFollow, followed)});
	Layout(103, "OpenTTDRoadYapfArea", {sizeof(OpenTTDRoadYapfArea), alignof(OpenTTDRoadYapfArea), offsetof(OpenTTDRoadYapfArea, tile), offsetof(OpenTTDRoadYapfArea, width), offsetof(OpenTTDRoadYapfArea, height), offsetof(OpenTTDRoadYapfArea, valid), offsetof(OpenTTDRoadYapfArea, stop), offsetof(OpenTTDRoadYapfArea, drive_through), offsetof(OpenTTDRoadYapfArea, next)});
	Layout(104, "OpenTTDRoadYapfLeaves", {sizeof(OpenTTDRoadYapfLeaves), alignof(OpenTTDRoadYapfLeaves), offsetof(OpenTTDRoadYapfLeaves, tile), offsetof(OpenTTDRoadYapfLeaves, follow), offsetof(OpenTTDRoadYapfLeaves, tracks), offsetof(OpenTTDRoadYapfLeaves, height), offsetof(OpenTTDRoadYapfLeaves, closest), offsetof(OpenTTDRoadYapfLeaves, area)});
	Layout(105, "OpenTTDRoadYapfResult", {sizeof(OpenTTDRoadYapfResult), alignof(OpenTTDRoadYapfResult), offsetof(OpenTTDRoadYapfResult, tile), offsetof(OpenTTDRoadYapfResult, cost), offsetof(OpenTTDRoadYapfResult, direction), offsetof(OpenTTDRoadYapfResult, found), offsetof(OpenTTDRoadYapfResult, rounds), offsetof(OpenTTDRoadYapfResult, open), offsetof(OpenTTDRoadYapfResult, closed), offsetof(OpenTTDRoadYapfResult, calcs), offsetof(OpenTTDRoadYapfResult, distance)});

}

static void Calls()
{
	CheckTownProtocol([](bool condition) { CHECK(condition); });
	std::unique_ptr<OpenTTDRoadState, decltype(&openttd_rust_road_destroy)> road(openttd_rust_road_new(), openttd_rust_road_destroy);
	for (uint8_t field = 0; field < 7; ++field) {
		CHECK(openttd_rust_road_get(road.get(), field) == 0);
		openttd_rust_road_set(road.get(), field, 65535);
		CHECK(openttd_rust_road_get(road.get(), field) == (field == 2 || field == 5 ? 65535 : 255));
	}
	CHECK(openttd_rust_road_path_size(road.get()) == 0);
	const OpenTTDRoadPathElement road_path[] = {{255, UINT32_MAX}, {9, 123456}};
	openttd_rust_road_path_replace(road.get(), road_path, 2);
	CHECK(openttd_rust_road_path_get(road.get(), 1).tile == 123456);
	openttd_rust_road_path_push(road.get(), {6, 42});
	CHECK(openttd_rust_road_path_get(road.get(), 2).trackdir == 6);
	openttd_rust_road_path_pop(road.get());
	CHECK(openttd_rust_road_path_size(road.get()) == 2);
	openttd_rust_road_path_clear(road.get());
	openttd_rust_road_path_replace(road.get(), nullptr, 0);
	CHECK(openttd_rust_road_path_size(road.get()) == 0);
	road.reset();
	const std::array<uint8_t, 8> bytes{0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE};
	constexpr uint64_t bits = UINT64_C(0xFEDCBA9876543210);
	auto le = openttd_rust_encode_uint_le(bits);
	CHECK(std::memcmp(le.bytes, bytes.data(), bytes.size()) == 0);
	auto number = openttd_rust_consumer_little_endian(bytes.data(), bytes.size(), 8);
	CHECK(number.value_bits == bits && number.length == 8);
	const std::string maximum = "ffffffffffffffff";
	auto integer = openttd_rust_parse_integer(reinterpret_cast<const uint8_t *>(maximum.data()), maximum.size(), 16, 64, 0, 0);
	CHECK(integer.value_bits == UINT64_MAX && integer.length == 16 && integer.error_kind == 0);
	auto formatted = openttd_rust_format_integer(UINT64_MAX, 0, 16);
	CHECK(formatted.length == maximum.size() && std::memcmp(formatted.bytes, maximum.data(), maximum.size()) == 0);
	auto empty_integer = openttd_rust_parse_integer(nullptr, 0, 10, 64, 0, 0);
	CHECK(empty_integer.length == 0 && empty_integer.error_kind == 1);
	CHECK(openttd_rust_skip_integer(nullptr, 0, 10) == 0);
	auto utf8 = openttd_rust_encode_utf8(0x1F600);
	CHECK(utf8.length == 4 && utf8.bytes[0] == 0xF0 && utf8.bytes[3] == 0x80);
	auto decoded = openttd_rust_decode_utf8(utf8.bytes, utf8.length);
	CHECK(decoded.length == 4 && decoded.codepoint == 0x1F600);
	CHECK(openttd_rust_decode_utf8(nullptr, 0).length == 0);
	CHECK(openttd_rust_utf8_next(utf8.bytes, utf8.length, 0) == 4);
	CHECK(openttd_rust_utf8_previous(utf8.bytes, utf8.length, 4) == 0);
	CHECK(openttd_rust_utf8_at_byte(utf8.bytes, utf8.length, 2) == 0);
	auto state = openttd_rust_alternating_initialize(SIZE_MAX - 1, 1);
	CHECK(state.position == SIZE_MAX - 1 && state.current_after == 1);
	auto step = openttd_rust_alternating_advance(state, SIZE_MAX);
	CHECK(step.state.position == SIZE_MAX && step.movement == 0 && step.state.current_after == 1);
	CHECK(openttd_rust_alternating_compare(state, step.state) == -1);
	auto bound = openttd_rust_consumer_bound(31, 7, SIZE_MAX);
	CHECK(bound.length == 24 && bound.position == 31 && bound.shortfall == 0);
	auto match = openttd_rust_consumer_prefix(nullptr, 0, nullptr, 0);
	CHECK(match.length == 0 && match.matched == 1);
	CHECK(openttd_rust_consumer_find(bytes.data(), bytes.size(), utf8.bytes, 1, 0) == SIZE_MAX);
	auto trimmed = openttd_rust_bytes_trim(nullptr, 0, nullptr, 0);
	CHECK(trimmed.offset == 0 && trimmed.length == 0);
	CHECK(openttd_rust_bytes_case(nullptr, 0, nullptr, 0, 0, std::is_signed_v<char>, 0) == 0);
	auto spiral = openttd_rust_spiral_square(10, 10, 3, 32, 32);
	CHECK(openttd_rust_spiral_equal(spiral, spiral) == 1 && openttd_rust_spiral_end(spiral) == 0);
	spiral = openttd_rust_spiral_advance(spiral, 32, 32);
	CHECK(openttd_rust_spiral_end(spiral) == 0);
	CHECK(openttd_rust_get_partial_pixel_z(3, 12, 0) == 0);
	CHECK(openttd_rust_get_partial_pixel_z(-2, 0, 0x03) == UINT32_MAX);
	CHECK(openttd_rust_get_partial_pixel_z(0, 0, 0x10) == UINT64_MAX);
	std::printf("real_calls high_bits=%llx passed\n", static_cast<unsigned long long>(number.value_bits));
}

using EncodedOwner = std::unique_ptr<OpenTTDRustEncodedResult, decltype(&openttd_rust_encoded_destroy)>;

static void Encoded()
{
	const std::array<uint8_t, 3> bytes{'A', 0, 'B'};
	const std::array<OpenTTDRustEncodedParameter, 3> parameters{{{UINT64_MAX, nullptr, 0, 1}, {0, bytes.data(), bytes.size(), 2}, {0, nullptr, 0, 0}}};
	std::printf("descriptor_array_alignment %zu address_mod %zu\n", alignof(OpenTTDRustEncodedParameter), reinterpret_cast<uintptr_t>(parameters.data()) % alignof(OpenTTDRustEncodedParameter));
	EncodedOwner owner(openttd_rust_encoded_serialize(42, parameters.data(), parameters.size(), 0), openttd_rust_encoded_destroy);
	auto view = openttd_rust_encoded_view(owner.get());
	std::string expected = "\xEE\x80\x81" "2a\x1E\xEE\x80\x82" "ffffffffffffffff\x1E\xEE\x80\x83";
	expected.append("A\0B", 3);
	expected.push_back('\x1E');
	CHECK(view.length == expected.size() && std::memcmp(view.bytes, expected.data(), view.length) == 0);
	std::string copied(reinterpret_cast<const char *>(view.bytes), view.length);
	owner.reset();
	CHECK(copied == expected);
	/* Byte storage is deliberately not aligned as a Rust descriptor. The boundary
	 * copies initialized field representations without any typed C++-storage borrow. */
	alignas(8) std::array<uint8_t, sizeof(parameters) + 8> storage{};
	std::memcpy(storage.data() + 1, parameters.data(), sizeof(parameters));
	auto raw = reinterpret_cast<const OpenTTDRustEncodedParameter *>(storage.data() + 1);
	CHECK(reinterpret_cast<uintptr_t>(raw) % alignof(OpenTTDRustEncodedParameter) != 0);
	owner.reset(openttd_rust_encoded_serialize(42, raw, parameters.size(), 0));
	view = openttd_rust_encoded_view(owner.get());
	CHECK(view.length == expected.size() && std::memcmp(view.bytes, expected.data(), view.length) == 0);
	owner.reset(openttd_rust_encoded_serialize(42, nullptr, 0, 0));
	view = openttd_rust_encoded_view(owner.get());
	CHECK(view.length == 5);
	owner.reset(openttd_rust_encoded_replace(reinterpret_cast<const uint8_t *>(expected.data()), expected.size(), 0, parameters[0], 0, 0));
	view = openttd_rust_encoded_view(owner.get());
	/* Original replacement does not parse a final empty record after its last separator. */
	CHECK(view.length == expected.size() - 1 && std::memcmp(view.bytes, expected.data(), view.length) == 0);
	const std::string invalid = "\xEE\x80\x80" "0\x1E\xEE\x80\x82" "10000000000000000";
	owner.reset(openttd_rust_encoded_negatives(reinterpret_cast<const uint8_t *>(invalid.data()), invalid.size()));
	view = openttd_rust_encoded_view(owner.get());
	CHECK(view.diagnostic_count == 1);
	auto diagnostic = openttd_rust_encoded_diagnostic(owner.get(), 0);
	CHECK(diagnostic.kind == 2 && diagnostic.offset == 8 && diagnostic.length == 17 && diagnostic.tail_length == 0);
	openttd_rust_encoded_destroy(nullptr);
	std::printf("encoded owners, by-value replacement/view/diagnostic and raw unaligned array passed\n");
}

static void Locale()
{
	CHECK(std::setlocale(LC_CTYPE, "C") != nullptr);
#ifdef _WIN32
	int previous = _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
	CHECK(previous != -1);
	CHECK(std::setlocale(LC_CTYPE, ".1252") != nullptr);
#endif
	uint8_t byte = 0xC9;
	uint8_t expected = static_cast<uint8_t>(std::tolower(static_cast<unsigned char>(byte)));
#ifdef _WIN32
	CHECK(expected == 0xE9); // A real non-C mapping is required to distinguish locale state.
#endif
	openttd_rust_bytes_lower(&byte, 1, 0);
	CHECK(byte == expected);
	std::printf("shared_locale %s lower_C9 %u\n", std::setlocale(LC_CTYPE, nullptr), static_cast<unsigned>(byte));
	CHECK(std::setlocale(LC_CTYPE, "C") != nullptr);
#ifdef _WIN32
	CHECK(_configthreadlocale(previous) != -1);
#endif
}

using HistoryOwner = std::unique_ptr<OpenTTDRustHistoryEngine, decltype(&openttd_rust_history_destroy)>;

static void HistoryDescribe(OpenTTDRustHistoryEngine *owner, const HistoryRange &range)
{
	auto step = openttd_rust_history_next(owner);
	CHECK(step.kind == 1 && step.token == reinterpret_cast<uintptr_t>(&range));
	openttd_rust_history_describe(owner, {reinterpret_cast<uintptr_t>(range.hr), range.periods, range.records, range.first, range.last, range.division, range.total_division, range.hr != nullptr ? range.hr->periods : uint8_t{0}, range.hr != nullptr ? range.hr->division : uint8_t{0}});
}

static void History()
{
	const HistoryRange leaf{2};
	const HistoryRange parent{leaf, 1, 2};
	constexpr uint64_t bits = UINT64_C(0xFEDCBA9876543210);
	HistoryOwner owner(openttd_rust_history_create(0, reinterpret_cast<uintptr_t>(&leaf), bits, 0, 0), openttd_rust_history_destroy);
	HistoryDescribe(owner.get(), leaf);
	auto step = openttd_rust_history_next(owner.get());
	CHECK(step.kind == 0 && step.value == (bits | 2));
	owner.reset(openttd_rust_history_create(1, reinterpret_cast<uintptr_t>(&leaf), bits | 2, 0, 0));
	HistoryDescribe(owner.get(), leaf);
	step = openttd_rust_history_next(owner.get());
	CHECK(step.kind == 0 && step.value == 1);
	owner.reset(openttd_rust_history_create(2, reinterpret_cast<uintptr_t>(&leaf), bits | 2, 0, 1));
	HistoryDescribe(owner.get(), leaf);
	step = openttd_rust_history_next(owner.get());
	CHECK(step.kind == 2 && step.first == 1 && step.last == 3);
	step = openttd_rust_history_next(owner.get());
	CHECK(step.kind == 3 && step.first == 0 && step.target == 1);
	step = openttd_rust_history_next(owner.get());
	CHECK(step.kind == 4 && step.target == 0);
	CHECK(openttd_rust_history_next(owner.get()).kind == 0);
	owner.reset(openttd_rust_history_create(3, reinterpret_cast<uintptr_t>(&parent), bits | 2, 0, 0));
	HistoryDescribe(owner.get(), parent);
	step = openttd_rust_history_next(owner.get());
	CHECK(step.kind == 6 && step.count == 1);
	openttd_rust_history_phase(owner.get(), 0);
	step = openttd_rust_history_next(owner.get());
	CHECK(step.kind == 7 && step.target == 0);
	HistoryDescribe(owner.get(), leaf);
	step = openttd_rust_history_next(owner.get());
	CHECK(step.kind == 9 && step.first == 1);
	CHECK(openttd_rust_history_complete(owner.get()) == 1);
	step = openttd_rust_history_next(owner.get());
	CHECK(step.kind == 8 && step.count == 1);
	CHECK(openttd_rust_history_complete(owner.get()) == 1);
	step = openttd_rust_history_next(owner.get());
	CHECK(step.kind == 0 && step.value == 1);
	std::printf("history descriptor identities, high-bit mask and staged by-value calls passed\n");
}

static void Math()
{
	/* Full uint32 input/output, including the rounded result beyond uint16. */
	CHECK(openttd_rust_int_sqrt(0) == 0);
	CHECK(openttd_rust_int_sqrt(6) == 2);
	CHECK(openttd_rust_int_sqrt(7) == 3);
	CHECK(openttd_rust_int_sqrt(UINT32_C(0x80000000)) == 46341);
	CHECK(openttd_rust_int_sqrt(UINT32_C(4294836225)) == 65535);
	CHECK(openttd_rust_int_sqrt(UINT32_MAX) == 65536);
	constexpr uint64_t bits = UINT64_C(0xFEDCBA9876543210);
	constexpr uint64_t high = UINT64_C(0x8000000000000000);
	CHECK(openttd_rust_clamp_to(bits, 64, 0, 64, 0) == bits);
	CHECK(openttd_rust_clamp_to(UINT64_MAX, 64, 0, 64, 1) == INT64_MAX);
	CHECK(openttd_rust_clamp_to(high, 64, 1, 64, 0) == 0);
	CHECK(openttd_rust_clamp_to(high, 64, 1, 64, 1) == high);
	CHECK(openttd_rust_clamp_to(UINT64_MAX, 64, 0, 32, 0) == UINT32_MAX);
	CHECK(openttd_rust_clamp_to(UINT32_MAX, 32, 0, 32, 1) == INT32_MAX);
	CHECK(openttd_rust_clamp_to(UINT16_MAX, 16, 0, 16, 1) == INT16_MAX);
	CHECK(openttd_rust_clamp_to(256, 16, 0, 8, 0) == UINT8_MAX);
	CHECK(openttd_rust_clamp_to(0x80, 8, 1, 64, 1) == static_cast<uint64_t>(INT8_MIN));
	CHECK(openttd_rust_clamp_to(UINT64_MAX, 64, 1, 8, 1) == UINT64_MAX);
	CHECK(openttd_rust_clamp_to(2, 8, 0, 1, 0) == 1);
	CHECK(openttd_rust_clamp_to(1, 1, 0, 64, 1) == 1);
	/* The narrow signed reversed intervals intentionally retain C++ promotions. */
	CHECK(openttd_rust_soft_clamp(bits, high, UINT64_MAX, 64, 0) == bits);
	CHECK(openttd_rust_soft_clamp(0, high, UINT64_MAX, 64, 0) == high);
	CHECK(openttd_rust_soft_clamp(UINT64_MAX, 0, bits, 64, 0) == bits);
	CHECK(openttd_rust_soft_clamp(0, UINT64_MAX, 0, 64, 0) == high);
	CHECK(openttd_rust_soft_clamp(0, UINT64_MAX, UINT64_MAX - 2, 8, 1) == 126);
	CHECK(openttd_rust_soft_clamp(0, UINT64_MAX, UINT64_MAX - 2, 16, 1) == 32766);
	CHECK(openttd_rust_soft_clamp(0, 1500000000, static_cast<uint64_t>(-1500000000), 32, 1) == 0);
	CHECK(openttd_rust_soft_clamp(high, high, INT64_MAX, 64, 1) == high);
	std::printf("math int_sqrt, clamp_to and soft_clamp scalar high-bit, width and boundary calls passed\n");
}

static void CryptoPrimitives()
{
	/* Actual public facades invoke all 15 Rust functions with native descriptors.
	 * Counter high bits, initialized-only Poly1305 storage, callback convention,
	 * in-place failure/retry and rekey are observed on the current native target. */
	std::array<uint8_t, 32> key{}, h{};
	std::array<uint8_t, 24> nonce{};
	std::array<uint8_t, 65> cipher{}, plain{}, decoded{};
	for (size_t i = 0; i < key.size(); ++i) key[i] = static_cast<uint8_t>(i * 13 + 9);
	for (size_t i = 0; i < nonce.size(); ++i) nonce[i] = static_cast<uint8_t>(i * 7 + 101);
	nonce[0] = 0x12; nonce[1] = 0x34; nonce[2] = 0x56; nonce[3] = 0xF8;
	crypto_chacha20_h(h.data(), key.data(), nonce.data());
	CHECK(crypto_chacha20_djb(cipher.data(), nullptr, 65, key.data(), nonce.data(), UINT64_MAX) == 1);
	CHECK(crypto_chacha20_ietf(cipher.data(), nullptr, 65, key.data(), nonce.data(), UINT32_MAX) == 1);
	CHECK(crypto_chacha20_x(cipher.data(), nullptr, 65, key.data(), nonce.data(), UINT64_MAX) == 1);
	plain.fill(0x42);
	CHECK(crypto_chacha20_djb(cipher.data(), plain.data(), plain.size(), key.data(), nonce.data(), UINT64_C(0xFEDCBA9876543210)) == UINT64_C(0xFEDCBA9876543212));
	crypto_chacha20_djb(decoded.data(), cipher.data(), cipher.size(), key.data(), nonce.data(), UINT64_C(0xFEDCBA9876543210));
	CHECK(decoded == plain);
	crypto_poly1305_ctx poly;
	std::fill_n(reinterpret_cast<uint8_t *>(&poly), sizeof(poly), 0xA5);
	crypto_poly1305_init(&poly, key.data());
	CHECK(std::all_of(std::begin(poly.c), std::end(poly.c), [](uint8_t c) { return c == 0xA5; }));
	crypto_poly1305_update(&poly, nullptr, 0);
	crypto_poly1305_update(&poly, plain.data(), 15);
	crypto_poly1305_update(&poly, plain.data() + 15, plain.size() - 15);
	std::array<uint8_t, 16> mac{}, split{};
	crypto_poly1305_final(&poly, split.data());
	CHECK(std::all_of(reinterpret_cast<const uint8_t *>(&poly), reinterpret_cast<const uint8_t *>(&poly) + sizeof(poly), [](uint8_t c) { return c == 0; }));
	crypto_poly1305(mac.data(), plain.data(), plain.size(), key.data());
	CHECK(mac == split);
	for (unsigned variant = 0; variant != 3; ++variant) {
		crypto_aead_ctx send, receive;
		if (variant == 0) { crypto_aead_init_x(&send, key.data(), nonce.data()); crypto_aead_init_x(&receive, key.data(), nonce.data()); }
		if (variant == 1) { crypto_aead_init_djb(&send, key.data(), nonce.data()); crypto_aead_init_djb(&receive, key.data(), nonce.data()); }
		if (variant == 2) { crypto_aead_init_ietf(&send, key.data(), nonce.data()); crypto_aead_init_ietf(&receive, key.data(), nonce.data()); }
		auto counter = send.counter;
		CHECK(counter == (variant == 2 ? UINT64_C(0xF856341200000000) : uint64_t{0}));
		for (unsigned chunk = 0; chunk != 2; ++chunk) {
			crypto_aead_write(&send, cipher.data(), mac.data(), nonce.data(), 17, plain.data(), plain.size());
			CHECK(send.counter == counter);
			auto bad = mac; bad[0] ^= 1;
			decoded.fill(0xA5);
			auto before = receive;
			CHECK(crypto_aead_read(&receive, decoded.data(), bad.data(), nonce.data(), 17, cipher.data(), cipher.size()) == -1);
			CHECK(std::all_of(decoded.begin(), decoded.end(), [](uint8_t c) { return c == 0xA5; }));
			CHECK(receive.counter == before.counter && std::equal(std::begin(receive.key), std::end(receive.key), std::begin(before.key)));
			CHECK(crypto_aead_read(&receive, cipher.data(), mac.data(), nonce.data(), 17, cipher.data(), cipher.size()) == 0);
			CHECK(cipher == plain && receive.counter == counter && std::equal(std::begin(send.key), std::end(send.key), std::begin(receive.key)));
		}
	}
	crypto_aead_lock(cipher.data(), mac.data(), key.data(), nonce.data(), nonce.data(), 17, plain.data(), plain.size());
	CHECK(crypto_aead_unlock(decoded.data(), mac.data(), key.data(), nonce.data(), nonce.data(), 17, cipher.data(), cipher.size()) == 0 && decoded == plain);
	std::printf("crypto 15 Rust algorithms, cdecl leaves, caller layouts, counter high bits, partial init, padding and failed retry passed; poly_size=%zu poly_align=%zu aead_size=%zu aead_align=%zu\n", sizeof(poly), alignof(crypto_poly1305_ctx), sizeof(crypto_aead_ctx), alignof(crypto_aead_ctx));
}

static void Blake2b()
{
	std::array<uint8_t, 129> message;
	std::array<uint8_t, 128> key;
	for (size_t i = 0; i < message.size(); ++i) message[i] = static_cast<uint8_t>(i * 37 + 11);
	for (size_t i = 0; i < key.size(); ++i) key[i] = static_cast<uint8_t>(i * 13 + 7);
	std::array<uint8_t, 64> expected, actual;
	crypto_blake2b_keyed(expected.data(), 32, key.data(), 65, message.data(), message.size());
	crypto_blake2b_ctx ctx;
	std::fill_n(reinterpret_cast<uint8_t *>(&ctx), sizeof(ctx), 0xA5);
	crypto_blake2b_keyed_init(&ctx, 32, key.data(), 65);
	CHECK(ctx.input_idx == 128 && ctx.hash_size == 32 && ctx.input_offset[0] == 0 && ctx.input_offset[1] == 0);
	crypto_blake2b_update(&ctx, nullptr, 0);
	crypto_blake2b_update(&ctx, message.data(), 127);
	auto copy = ctx;
	crypto_blake2b_update(&ctx, message.data() + 127, 2);
	crypto_blake2b_final(&ctx, actual.data());
	CHECK(std::equal(actual.begin(), actual.begin() + 32, expected.begin()));
	CHECK(std::all_of(reinterpret_cast<const uint8_t *>(&ctx), reinterpret_cast<const uint8_t *>(&ctx) + sizeof(ctx), [](uint8_t x) { return x == 0; }));
	crypto_blake2b_update(&copy, message.data() + 127, 1);
	crypto_blake2b_final(&copy, actual.data());
	crypto_blake2b_keyed(expected.data(), 32, key.data(), 65, message.data(), 128);
	CHECK(std::equal(actual.begin(), actual.begin() + 32, expected.begin()));
	crypto_blake2b(expected.data(), 64, message.data(), message.size());
	crypto_blake2b_init(&ctx, 64);
	crypto_blake2b_update(&ctx, message.data(), 128);
	CHECK(ctx.input_idx == 128 && ctx.input_offset[0] == 0);
	crypto_blake2b_update(&ctx, message.data() + 128, 1);
	crypto_blake2b_final(&ctx, actual.data()); CHECK(actual == expected);
	crypto_blake2b_init(&ctx, 64); crypto_blake2b_update(&ctx, message.data(), 128);
	ctx.input_offset[0] = UINT64_MAX - 127; ctx.input_offset[1] = UINT64_C(0x1122334455667788);
	crypto_blake2b_update(&ctx, message.data() + 128, 1);
	CHECK(ctx.input_offset[0] == 0 && ctx.input_offset[1] == UINT64_C(0x1122334455667789) && ctx.input_idx == 1);
	crypto_blake2b_final(&ctx, actual.data());
	crypto_blake2b_init(&ctx, 0); crypto_blake2b_final(&ctx, nullptr);
	CHECK(std::all_of(reinterpret_cast<const uint8_t *>(&ctx), reinterpret_cast<const uint8_t *>(&ctx) + sizeof(ctx), [](uint8_t x) { return x == 0; }));
	std::array<uint8_t, 72> sentinel; sentinel.fill(0xA5);
	crypto_blake2b_keyed(sentinel.data(), 65, key.data(), 128, message.data(), message.size());
	CHECK(std::all_of(sentinel.begin() + 64, sentinel.end(), [](uint8_t x) { return x == 0xA5; }));
	std::printf("blake2b six facades, context size %zu align %zu offsets %zu %zu %zu %zu %zu; copy, carry, pending block, wipe and source sizes passed\n", sizeof(ctx), alignof(crypto_blake2b_ctx), offsetof(crypto_blake2b_ctx, hash), offsetof(crypto_blake2b_ctx, input_offset), offsetof(crypto_blake2b_ctx, input), offsetof(crypto_blake2b_ctx, input_idx), offsetof(crypto_blake2b_ctx, hash_size));
}

static size_t x25519_wipes, x25519_last_wipe;
static unsigned x25519_verifies;
static bool x25519_wipes_zero;
static void OPENTTD_CRYPTO_CALL X25519Wipe(void *data, size_t size) noexcept
{
	crypto_wipe(data, size);
	x25519_wipes++; x25519_last_wipe = size;
	const auto *bytes = static_cast<const uint8_t *>(data);
	x25519_wipes_zero &= std::all_of(bytes, bytes + size, [](uint8_t x) { return x == 0; });
}
static int32_t OPENTTD_CRYPTO_CALL X25519Verify32(const uint8_t *a, const uint8_t *b) noexcept
{
	x25519_verifies++;
	return crypto_verify32(a, b);
}
static void X25519()
{
	const OpenTTDX25519Leaves leaves{X25519Wipe, X25519Verify32};
	std::array<uint8_t, 32> secret, point{}, expected, actual;
	for (size_t i = 0; i < secret.size(); ++i) secret[i] = static_cast<uint8_t>(i * 37 + 13);
	point[0] = 9;
	crypto_x25519_public_key(expected.data(), secret.data());
	x25519_wipes = x25519_verifies = 0; x25519_wipes_zero = true;
	openttd_rust_x25519_public_key(&leaves, actual.data(), secret.data());
	CHECK(actual == expected && x25519_verifies == 4 && x25519_wipes == 29 && x25519_last_wipe == 32 && x25519_wipes_zero);
	x25519_wipes = x25519_verifies = 0;
	openttd_rust_x25519(&leaves, actual.data(), secret.data(), point.data());
	CHECK(actual == expected && x25519_verifies == 4 && x25519_wipes == 29 && x25519_last_wipe == 32 && x25519_wipes_zero);
	std::array<uint8_t, 64> storage;
	storage.fill(13); storage[33] = 0xA5;
	openttd_rust_x25519_trim(storage.data() + 1, storage.data());
	CHECK(storage[1] == 8 && storage[32] == 0x4D && storage[33] == 0xA5);
	CHECK(std::all_of(storage.begin() + 2, storage.begin() + 32, [](uint8_t x) { return x == 13; }));
	secret.fill(0); secret[31] = 0x80;
	std::array<uint8_t, 32> short_result, full_result;
	x25519_wipes = x25519_verifies = 0;
	openttd_rust_x25519_ladder(&leaves, short_result.data(), secret.data(), point.data(), 255);
	CHECK(x25519_verifies == 4 && x25519_wipes == 28 && x25519_last_wipe == 40 && x25519_wipes_zero);
	openttd_rust_x25519_ladder(&leaves, full_result.data(), secret.data(), point.data(), 256);
	CHECK(short_result != full_result && x25519_verifies == 8 && x25519_wipes == 56 && x25519_last_wipe == 40 && x25519_wipes_zero);
	std::printf("x25519 four exports, cdecl wipe/verify32, physical wipe counts/final sizes, forward trim and 255/256 high-bit distinction passed\n");
}

static void StationCargo()
{
	auto list = std::unique_ptr<OpenTTDScriptList, decltype(&openttd_rust_list_destroy)>(openttd_rust_list_new(), openttd_rust_list_destroy);
	OpenTTDCargoCollector state;
	CHECK(openttd_rust_cargo_plan(0, 1) == 1 && openttd_rust_cargo_plan(1, 3) == 3);
	CHECK(openttd_rust_cargo_plan(9, 0) == UINT8_MAX);
	openttd_rust_cargo_init(&state, 0, UINT16_MAX);
	openttd_rust_cargo_packet(&state, list.get(), UINT16_MAX, 2, UINT32_MAX);
	openttd_rust_cargo_packet(&state, list.get(), UINT16_MAX, 2, 2);
	openttd_rust_cargo_packet(&state, list.get(), 3, 2, 7);
	int64_t value = 0;
	CHECK(openttd_rust_list_get(list.get(), UINT16_MAX, &value) == 1 && value == 1);
	openttd_rust_cargo_finish(&state, list.get());
	CHECK(openttd_rust_list_get(list.get(), 3, &value) == 1 && value == 7);
	auto token = openttd_rust_list_token(list.get());
	openttd_rust_cargo_finish(&state, list.get());
	CHECK(openttd_rust_list_token(list.get()) == token);
	openttd_rust_cargo_init(&state, 2, 0);
	openttd_rust_cargo_origin(&state, 10);
	openttd_rust_cargo_share(&state, list.get(), 4, UINT32_MAX);
	openttd_rust_cargo_origin(&state, 11);
	openttd_rust_cargo_share(&state, list.get(), 4, 1);
	openttd_rust_cargo_finish(&state, list.get());
	CHECK(openttd_rust_list_get(list.get(), 4, &value) == 0);
	std::printf("station_cargo scalar layout, uint32 wrapping, origin reset and finalization passed\n");
}

static void PacketState()
{
	static_assert(sizeof(intptr_t) == sizeof(size_t));
	OpenTTDPacketState state;
	openttd_rust_packet_init(&state, SIZE_MAX);
	CHECK(state.limit == SIZE_MAX && state.position == 0);
	CHECK(openttd_rust_packet_can_write(&state, SIZE_MAX, 1) == 1);
	CHECK(openttd_rust_packet_boolean(128) == 1 && openttd_rust_packet_boolean(0) == 0);
	CHECK(openttd_rust_packet_buffer_size(SIZE_MAX) == 1 && openttd_rust_packet_prefix(65537) == 1);
	std::array<uint8_t, 8> bytes{8, 0, 1, 2, 3, 4, 5, 6};
	CHECK(openttd_rust_packet_parse_size(&state, bytes.data(), bytes.size()) == 8);
	openttd_rust_packet_read_start(&state);
	CHECK(openttd_rust_packet_has_size(&state) == 1 && openttd_rust_packet_can_read(&state, 8, 6) == 1);
	CHECK(openttd_rust_packet_recv(&state, bytes.data(), bytes.size(), 2) == 0x0201 && state.position == 4);
	uint16_t count = 1;
	uint8_t byte = 0;
	CHECK(openttd_rust_packet_buffer_next(&state, bytes.data(), bytes.size(), &count, &byte) == 1 && byte == 3 && state.position == 5);
	CHECK(openttd_rust_packet_buffer_next(&state, bytes.data(), bytes.size(), &count, &byte) == 0 && count == UINT16_MAX);
	CHECK(openttd_rust_packet_remaining(&state, 8) == 3 && openttd_rust_packet_transfer_amount(&state, 8, 2) == 2);
	openttd_rust_packet_transfer_commit(&state, -1);
	CHECK(state.position == 5);
	openttd_rust_packet_transfer_commit(&state, 65537);
	CHECK(state.position == 6);
	openttd_rust_packet_skip_mac(&state, 65537);
	CHECK(state.position == 7);
	OpenTTDPacketFrame frame;
	CHECK(openttd_rust_packet_frame(2, 8, 2, &frame) == 1 && frame.message == 4 && frame.payload == 4);
	CHECK(openttd_rust_packet_frame(2, 4, 2, &frame) == 0);
	CHECK(openttd_rust_packet_send_amount(&state, SIZE_MAX - 2, 4) == 2);
	openttd_rust_packet_write_header(bytes.data(), bytes.size());
	CHECK(bytes[0] == 8 && bytes[1] == 0);
	openttd_rust_packet_send_reset(&state);
	CHECK(state.position == 0);
	std::printf("packet scalar framing, binary reads and native cursor transitions passed\n");
}

static void StringValidation()
{
	static_assert(sizeof(char32_t) == sizeof(uint32_t));
	std::array<uint8_t, 4> input{13,10,0,0xFF};
	auto step = openttd_rust_validation_step(input.data(), input.size(), 15);
	CHECK(step.consumed == 1 && step.output.length == 0 && step.stopped == 0);
	step = openttd_rust_validation_step(input.data() + 1, input.size() - 1, 15);
	CHECK(step.consumed == 1 && step.output.length == 1 && step.output.bytes[0] == 10);
	step = openttd_rust_validation_step(input.data() + 2, 2, 15);
	CHECK(step.consumed == 1 && step.stopped == 1 && step.output.length == 0);
	step = openttd_rust_validation_step(input.data() + 3, 1, 255);
	CHECK(step.consumed == 1 && step.stopped == 0 && step.output.length == 0);
	std::array<uint8_t, 4> surrogate{0xED,0xA0,0x80,0};
	CHECK(openttd_rust_validation_valid(surrogate.data(), surrogate.size()) == 1);
	CHECK(openttd_rust_validation_valid(surrogate.data(), surrogate.size() - 1) == 0);
	std::array<uint8_t, 8> bytes{'a','b','c','d','e','f','g','h'};
	auto write = openttd_rust_inplace_write(bytes.data(), 0, 6, bytes.data() + 2, 4);
	CHECK(write.accepted == 1 && write.position == 4 && bytes[0] == 'c' && bytes[3] == 'f' && bytes[4] == 'e');
	write = openttd_rust_inplace_write(bytes.data(), 4, 4, input.data(), 1);
	CHECK(write.accepted == 0 && write.position == 4 && bytes[4] == 'e');
	write = openttd_rust_inplace_write(bytes.data(), 4, 1, input.data(), 1);
	CHECK(write.accepted == 1 && write.position == 5 && bytes[4] == 13);
	std::printf("string_validation historical policy, NUL, surrogate and live in-place copy passed\n");
}

static void ScriptListControl()
{
	auto *list = openttd_rust_list_new();
	auto *control = openttd_rust_list_control_new(5);
	OpenTTDListControlInput input{};
	OpenTTDListControlAction action{};
	auto step = [&] { openttd_rust_list_control_step(control, list, &input, &action); input = {}; };
	step(); CHECK(action.kind == LC_TYPE && action.a == 2);
	input.kind = 0x05000002; step(); CHECK(action.kind == LC_GET_INT);
	input.a = INT64_MIN; step(); CHECK(action.kind == LC_TYPE && action.a == 3);
	input.kind = 0x01000008; step(); CHECK(action.kind == LC_GET_BOOL);
	input.flag = UINT64_MAX; step(); CHECK(action.kind == LC_RETURN && action.a == 0);
	int64_t value;
	CHECK(openttd_rust_list_get(list, INT64_MIN, &value) == 1 && value == 1);
	openttd_rust_list_control_destroy(control);
	control = openttd_rust_list_control_new(4);
	step(); CHECK(action.kind == LC_TYPE);
	input.kind = 0x05000002; step(); CHECK(action.kind == LC_GET_INT);
	input.a = INT64_MIN; step(); CHECK(action.kind == LC_PUSH_INT && action.a == 1);
	step(); CHECK(action.kind == LC_RETURN && action.a == 1);
	openttd_rust_list_control_destroy(control);
	control = openttd_rust_list_control_new(1);
	step(); CHECK(action.kind == LC_TOP);
	input.a = 1; step(); CHECK(action.kind == LC_DISABLE);
	step(); CHECK(action.kind == LC_ITEM);
	input.flag = 1; step(); CHECK(action.kind == LC_ITEM);
	input.flag = 2; step(); CHECK(action.kind == LC_INDEX);
	input.a = INT64_MAX; step(); CHECK(action.kind == LC_ITEM);
	step(); CHECK(action.kind == LC_RETURN && action.a == 0);
	CHECK(openttd_rust_list_get(list, INT64_MAX, &value) == 1 && value == 0);
	openttd_rust_list_control_destroy(control);
	control = openttd_rust_list_control_new(3);
	step(); CHECK(action.kind == LC_TYPE && action.a == -1);
	input.kind = 0x01000001; step(); CHECK(action.kind == LC_RETURN && action.a == 0);
	openttd_rust_list_control_destroy(control);
	openttd_rust_list_destroy(list);
	std::printf("script_list control high-bit bool/key, typed filtering and load rejection passed\n");
}

static uint8_t OPENTTD_LINKGRAPH_CALL LinkGraphAbort(const void *context) noexcept
{
	auto *state = static_cast<const std::array<uint32_t, 2> *>(context);
	return (*state)[0];
}

static void LinkGraphJob()
{
	std::array<OpenTTDLinkGraphNode, 2> nodes{{{10, 0, 12, 1, 1, 0, 1}, {0, 1, 24, 4, 1, 1, 0}}};
	OpenTTDLinkGraphEdge edge{20, 0, 1};
	OpenTTDLinkGraphSettings settings{2, 100, 100, 80, 1, 1, 127, 127, 30};
	std::array<uint32_t, 2> context{0, 0};
	auto *owner = openttd_rust_linkgraph_run(nodes.data(), nodes.size(), &edge, 1, &settings, &context, LinkGraphAbort);
	size_t count = 0;
	const auto *shares = openttd_rust_linkgraph_shares(owner, &count);
	CHECK(count == 2 && shares[0].node == 0 && shares[1].node == 1);
	CHECK(shares[0].origin == 12 && shares[0].via == 24 && shares[0].cumulative == 10 && shares[0].unrestricted == 10);
	CHECK(shares[1].origin == 12 && shares[1].via == 24 && shares[1].cumulative == 10 && shares[1].unrestricted == 10);
	CHECK(openttd_rust_linkgraph_edges(owner)[0] == 10);
	openttd_rust_linkgraph_destroy(owner);
	context[0] = 1;
	owner = openttd_rust_linkgraph_run(nodes.data(), nodes.size(), &edge, 1, &settings, &context, LinkGraphAbort);
	openttd_rust_linkgraph_shares(owner, &count);
	CHECK(count == 0 && openttd_rust_linkgraph_edges(owner)[0] == 0);
	openttd_rust_linkgraph_destroy(owner);
	std::printf("linkgraph owned snapshot/result and cdecl abort callback passed\n");
}

// ABI/reentry probe, not simulation evidence. The full game harness compares map/DATE.
struct TreeProbe {
	OpenTTDTreePlantObservation plant{.tile_type = 7, .bridge = false, .ground = 0, .density = 0, .snow = false, .coast = false, .zone = 0};
	OpenTTDTreeObservation tree{.ground = 3, .density = 3, .species = 20, .count = 2, .growth = 3, .zone = 0};
	uint32_t writes = 0, waters = 0, clears = 0;
	int64_t nested_cost = 0;
	bool abort = false;
};
static TreeProbe tree_probe;
template <typename Result, typename... Args>
static Result TreeUnused(Args...) noexcept
{
	if constexpr (!std::is_void_v<Result>) return {};
}
static uint32_t TreeRandom(void *) noexcept { return UINT32_MAX; }
static const OpenTTDTreeServices tree_probe_services{
	.plant_observation = [](uint32_t) noexcept { return tree_probe.plant; },
	.tree_observation = [](uint32_t) noexcept { return tree_probe.tree; },
	.snow_line = TreeUnused<uint8_t>,
	.sin = [](float value) noexcept { return value; },
	.cos = [](float value) noexcept { return value; },
	.make_tree = [](uint32_t, uint8_t type, uint32_t count, uint8_t growth, uint8_t ground, uint32_t density) noexcept {
		++tree_probe.writes;
		CHECK(type == 12 && count == 2 && growth == 6 && ground == 0 && density == 2);
	},
	.set_ground_density = TreeUnused<void, uint32_t, uint8_t, uint32_t>,
	.add_count = TreeUnused<void, uint32_t, int32_t>,
	.add_growth = TreeUnused<void, uint32_t, int32_t>,
	.set_growth = TreeUnused<void, uint32_t, uint8_t>,
	.make_clear = TreeUnused<void, uint32_t, uint8_t, uint32_t>,
	.make_shore = TreeUnused<void, uint32_t>,
	.make_snow = TreeUnused<void, uint32_t, uint32_t>,
	.clear_neighbour_flooding = TreeUnused<void, uint32_t>,
	.tile_loop_water = [](uint32_t tile) noexcept {
		/* Flooding reenters tree code through a nested clear of a neighbouring tile. */
		++tree_probe.waters;
		tree_probe.nested_cost = openttd_rust_trees_clear_tile(nullptr, tile + 1, 5, true, false, &tree_probe_services);
	},
	.ambient = TreeUnused<bool, uint32_t>,
	.play_sound = TreeUnused<void, uint32_t, uint16_t>,
	.progress = []() noexcept { return tree_probe.abort; },
	.progress_total = TreeUnused<bool, uint32_t>,
	.clear_square = [](uint32_t) noexcept { ++tree_probe.clears; },
	.town_rating = TreeUnused<void, OpenTTDTreeCommand *, uint32_t, bool>,
	.command_begin = TreeUnused<int32_t, OpenTTDTreeCommand *, uint32_t, uint32_t, bool>,
	.command_tile = TreeUnused<uint32_t, OpenTTDTreeCommand *>,
	.command_next = TreeUnused<uint32_t, OpenTTDTreeCommand *>,
	.command_debit = TreeUnused<void, OpenTTDTreeCommand *>,
	.landscape_clear = TreeUnused<OpenTTDTreeLandscapeClear, OpenTTDTreeCommand *, uint32_t>,
};
static void Trees()
{
	const OpenTTDSharedServices shared = OpenTTDFixtureSharedServices(nullptr, TreeRandom, EffectTestIndustry);
	uint8_t *counter = openttd_rust_tree_counter();
	openttd_rust_trees_initialize();
	CHECK(*counter == 0);
	*counter = 37; // Same boundary write made by DATE/TTD descriptors.
	openttd_rust_trees_tick({.tick_counter = 0, .size = 4096, .climate = 0, .extra = 2}, &tree_probe_services, &shared);
	CHECK(*counter == 36 && counter == openttd_rust_tree_counter());
	openttd_rust_trees_tick({.tick_counter = 0, .size = 4096 * 4096, .climate = 0, .extra = 2}, &tree_probe_services, &shared);
	CHECK(*counter == 36 && tree_probe.writes == 0);
	tree_probe.plant = {.tile_type = 0, .bridge = false, .ground = 0, .density = 2, .snow = false, .coast = false, .zone = 0};
	openttd_rust_trees_plant(1, 12, 2, 6, &tree_probe_services, &shared);
	CHECK(tree_probe.writes == 1 && openttd_rust_trees_can_plant(1, false, &tree_probe_services));
	openttd_rust_trees_tile_loop(1, {.tick_counter = 0, .size_x = 64, .climate = 0, .extra = 2, .ambient = false}, &tree_probe_services, &shared);
	CHECK(tree_probe.waters == 1 && tree_probe.clears == 1 && tree_probe.nested_cost == 40 && tree_probe.writes == 1);
	openttd_rust_trees_initialize();
	CHECK(*counter == 0 && counter == openttd_rust_tree_counter());
	tree_probe.abort = true;
	const OpenTTDTreeGenerateSettings generate{.size = 4096, .size_x = 64, .size_y = 64, .climate = 0, .placer = 1, .height_limit = 15, .editor = false, .freeform_edges = true};
	CHECK(openttd_rust_trees_generate(generate, &tree_probe_services, &shared));
	CHECK(openttd_rust_trees_place_randomly(generate, &tree_probe_services, &shared) && tree_probe.writes == 1);
	std::printf("tree_direct_services_counter_abort_and_reentry passed\n");
}

/* Complete typed tables exercise road cdecl calls without importing game globals.
 * These are boundary/lifetime checks, not a second simulation oracle. */
template <typename Function> struct RoadUnused;
template <typename Result, typename... Args>
struct RoadUnused<Result (OPENTTD_ROAD_CALL *)(Args...) noexcept> {
	static Result OPENTTD_ROAD_CALL Call(Args...) noexcept
	{
		CHECK(false);
		if constexpr (!std::is_void_v<Result>) return {};
	}
};
static OpenTTDRoadLeaves RoadTestLeaves()
{
	OpenTTDRoadLeaves table;
	table.read_z = RoadUnused<decltype(table.read_z)>::Call;
	table.read_type = RoadUnused<decltype(table.read_type)>::Call;
	table.op_acc_model = RoadUnused<decltype(table.op_acc_model)>::Call;
	table.op_road_side = RoadUnused<decltype(table.op_road_side)>::Call;
	table.op_tile_type = RoadUnused<decltype(table.op_tile_type)>::Call;
	table.op_has_road = RoadUnused<decltype(table.op_has_road)>::Call;
	table.op_track_status = RoadUnused<decltype(table.op_track_status)>::Call;
	table.op_tile_owner = RoadUnused<decltype(table.op_tile_owner)>::Call;
	table.op_depot_dir = RoadUnused<decltype(table.op_depot_dir)>::Call;
	table.op_bay_dir = RoadUnused<decltype(table.op_bay_dir)>::Call;
	table.op_is_depot = RoadUnused<decltype(table.op_is_depot)>::Call;
	table.op_normal_road = RoadUnused<decltype(table.op_normal_road)>::Call;
	table.op_road_works = RoadUnused<decltype(table.op_road_works)>::Call;
	table.op_disallowed = RoadUnused<decltype(table.op_disallowed)>::Call;
	table.op_bay_stop = RoadUnused<decltype(table.op_bay_stop)>::Call;
	table.op_is_dt_stop = RoadUnused<decltype(table.op_is_dt_stop)>::Call;
	table.op_stop_type = RoadUnused<decltype(table.op_stop_type)>::Call;
	table.op_free_bay = RoadUnused<decltype(table.op_free_bay)>::Call;
	table.op_any_road_bits = RoadUnused<decltype(table.op_any_road_bits)>::Call;
	table.op_road_bits = RoadUnused<decltype(table.op_road_bits)>::Call;
	table.op_offset = RoadUnused<decltype(table.op_offset)>::Call;
	table.op_tile_x = RoadUnused<decltype(table.op_tile_x)>::Call;
	table.op_tile_y = RoadUnused<decltype(table.op_tile_y)>::Call;
	table.op_station = RoadUnused<decltype(table.op_station)>::Call;
	table.op_continuation = RoadUnused<decltype(table.op_continuation)>::Call;
	table.op_bridge_speed = RoadUnused<decltype(table.op_bridge_speed)>::Call;
	table.op_max_penalty = RoadUnused<decltype(table.op_max_penalty)>::Call;
	table.op_servint = RoadUnused<decltype(table.op_servint)>::Call;
	table.op_needs_service = RoadUnused<decltype(table.op_needs_service)>::Call;
	table.op_wait_unbunch = RoadUnused<decltype(table.op_wait_unbunch)>::Call;
	table.op_order_stop = RoadUnused<decltype(table.op_order_stop)>::Call;
	table.op_road_type = RoadUnused<decltype(table.op_road_type)>::Call;
	table.op_queue = RoadUnused<decltype(table.op_queue)>::Call;
	table.op_tunnel_dir = RoadUnused<decltype(table.op_tunnel_dir)>::Call;
	table.op_acceleration = RoadUnused<decltype(table.op_acceleration)>::Call;
	table.op_update_speed = RoadUnused<decltype(table.op_update_speed)>::Call;
	table.op_advance = RoadUnused<decltype(table.op_advance)>::Call;
	table.op_position = RoadUnused<decltype(table.op_position)>::Call;
	table.op_base_viewport = RoadUnused<decltype(table.op_base_viewport)>::Call;
	table.op_last_speed = RoadUnused<decltype(table.op_last_speed)>::Call;
	table.op_roadstop_leave = RoadUnused<decltype(table.op_roadstop_leave)>::Call;
	table.op_entrance_set = RoadUnused<decltype(table.op_entrance_set)>::Call;
	table.op_entrance_busy = RoadUnused<decltype(table.op_entrance_busy)>::Call;
	table.op_order_free = RoadUnused<decltype(table.op_order_free)>::Call;
	table.op_set_next = RoadUnused<decltype(table.op_set_next)>::Call;
	table.op_start_stop_dirty = RoadUnused<decltype(table.op_start_stop_dirty)>::Call;
	table.op_depot_dirty = RoadUnused<decltype(table.op_depot_dirty)>::Call;
	table.op_details_dirty = RoadUnused<decltype(table.op_details_dirty)>::Call;
	table.op_service = RoadUnused<decltype(table.op_service)>::Call;
	table.op_leave_unbunch = RoadUnused<decltype(table.op_leave_unbunch)>::Call;
	table.op_reset_unbunch = RoadUnused<decltype(table.op_reset_unbunch)>::Call;
	table.op_path_result = RoadUnused<decltype(table.op_path_result)>::Call;
	table.op_order_dummy = RoadUnused<decltype(table.op_order_dummy)>::Call;
	table.op_order_depot = RoadUnused<decltype(table.op_order_depot)>::Call;
	table.op_depot_index = RoadUnused<decltype(table.op_depot_index)>::Call;
	table.op_decrease_value = RoadUnused<decltype(table.op_decrease_value)>::Call;
	table.op_age = RoadUnused<decltype(table.op_age)>::Call;
	table.op_economy_age = RoadUnused<decltype(table.op_economy_age)>::Call;
	table.op_check_breakdown = RoadUnused<decltype(table.op_check_breakdown)>::Call;
	table.op_check_orders = RoadUnused<decltype(table.op_check_orders)>::Call;
	table.op_pay_running = RoadUnused<decltype(table.op_pay_running)>::Call;
	table.op_cost_class = RoadUnused<decltype(table.op_cost_class)>::Call;
	table.op_cost_factor = RoadUnused<decltype(table.op_cost_factor)>::Call;
	table.op_get_price = RoadUnused<decltype(table.op_get_price)>::Call;
	table.op_grf_version = RoadUnused<decltype(table.op_grf_version)>::Call;
	table.op_length_default = RoadUnused<decltype(table.op_length_default)>::Call;
	table.op_age_default = RoadUnused<decltype(table.op_age_default)>::Call;
	table.op_speed_default = RoadUnused<decltype(table.op_speed_default)>::Call;
	table.op_length_error = RoadUnused<decltype(table.op_length_error)>::Call;
	table.op_disconnect = RoadUnused<decltype(table.op_disconnect)>::Call;
	table.op_explosion = RoadUnused<decltype(table.op_explosion)>::Call;
	table.op_sound_default = RoadUnused<decltype(table.op_sound_default)>::Call;
	table.op_sound = RoadUnused<decltype(table.op_sound)>::Call;
	table.op_sound_old1 = RoadUnused<decltype(table.op_sound_old1)>::Call;
	table.op_sound_old2 = RoadUnused<decltype(table.op_sound_old2)>::Call;
	table.op_engine_invalid = RoadUnused<decltype(table.op_engine_invalid)>::Call;
	table.op_invalid_price = RoadUnused<decltype(table.op_invalid_price)>::Call;
	table.op_cost_divisor = RoadUnused<decltype(table.op_cost_divisor)>::Call;
	table.op_is_crossing = RoadUnused<decltype(table.op_is_crossing)>::Call;
	table.op_new_position = RoadUnused<decltype(table.op_new_position)>::Call;
	table.op_virt_tile = RoadUnused<decltype(table.op_virt_tile)>::Call;
	table.op_is_road_stop = RoadUnused<decltype(table.op_is_road_stop)>::Call;
	table.op_set_dest = RoadUnused<decltype(table.op_set_dest)>::Call;
	table.op_cache_invalidate = RoadUnused<decltype(table.op_cache_invalidate)>::Call;
	table.op_arrival = RoadUnused<decltype(table.op_arrival)>::Call;
	table.op_crash_news = RoadUnused<decltype(table.op_crash_news)>::Call;
	table.op_station_visits = RoadUnused<decltype(table.op_station_visits)>::Call;
	table.op_station_visit_set = RoadUnused<decltype(table.op_station_visit_set)>::Call;
	table.op_local_company = RoadUnused<decltype(table.op_local_company)>::Call;
	table.op_enter_tile = RoadUnused<decltype(table.op_enter_tile)>::Call;
	table.op_enter_depot = RoadUnused<decltype(table.op_enter_depot)>::Call;
	table.op_process_orders = RoadUnused<decltype(table.op_process_orders)>::Call;
	table.op_loading = RoadUnused<decltype(table.op_loading)>::Call;
	table.op_begin_loading = RoadUnused<decltype(table.op_begin_loading)>::Call;
	table.op_tram_probe = RoadUnused<decltype(table.op_tram_probe)>::Call;
	table.op_property = RoadUnused<decltype(table.op_property)>::Call;
	table.op_length_callback = RoadUnused<decltype(table.op_length_callback)>::Call;
	table.op_play_sound = RoadUnused<decltype(table.op_play_sound)>::Call;
	table.op_visual = RoadUnused<decltype(table.op_visual)>::Call;
	table.op_update_visual = RoadUnused<decltype(table.op_update_visual)>::Call;
	table.op_cargo_changed = RoadUnused<decltype(table.op_cargo_changed)>::Call;
	table.op_length_changed = RoadUnused<decltype(table.op_length_changed)>::Call;
	table.op_breakdown = RoadUnused<decltype(table.op_breakdown)>::Call;
	table.op_delete = RoadUnused<decltype(table.op_delete)>::Call;
	table.op_ground_crash = RoadUnused<decltype(table.op_ground_crash)>::Call;
	table.op_stop_random = RoadUnused<decltype(table.op_stop_random)>::Call;
	table.op_stop_animation = RoadUnused<decltype(table.op_stop_animation)>::Call;
	table.op_yapf = RoadUnused<decltype(table.op_yapf)>::Call;
	table.op_find_depot = RoadUnused<decltype(table.op_find_depot)>::Call;
	table.op_inclination = RoadUnused<decltype(table.op_inclination)>::Call;
	table.op_viewport = RoadUnused<decltype(table.op_viewport)>::Call;
	table.set_tile = RoadUnused<decltype(table.set_tile)>::Call;
	table.set_x = RoadUnused<decltype(table.set_x)>::Call;
	table.set_y = RoadUnused<decltype(table.set_y)>::Call;
	table.set_direction = RoadUnused<decltype(table.set_direction)>::Call;
	table.set_speed = RoadUnused<decltype(table.set_speed)>::Call;
	table.set_tick = RoadUnused<decltype(table.set_tick)>::Call;
	table.set_running = RoadUnused<decltype(table.set_running)>::Call;
	table.set_day = RoadUnused<decltype(table.set_day)>::Call;
	table.set_order_time = RoadUnused<decltype(table.set_order_time)>::Call;
	table.set_progress = RoadUnused<decltype(table.set_progress)>::Call;
	table.set_last_station = RoadUnused<decltype(table.set_last_station)>::Call;
	table.set_hidden = RoadUnused<decltype(table.set_hidden)>::Call;
	table.set_first_engine = RoadUnused<decltype(table.set_first_engine)>::Call;
	table.set_length = RoadUnused<decltype(table.set_length)>::Call;
	table.set_total_length = RoadUnused<decltype(table.set_total_length)>::Call;
	table.set_cargo_age = RoadUnused<decltype(table.set_cargo_age)>::Call;
	table.set_max_speed = RoadUnused<decltype(table.set_max_speed)>::Call;
	table.set_suppress_implicit = RoadUnused<decltype(table.set_suppress_implicit)>::Call;
	table.read_day = RoadUnused<decltype(table.read_day)>::Call;
	table.read_dest = RoadUnused<decltype(table.read_dest)>::Call;
	table.read_direction = RoadUnused<decltype(table.read_direction)>::Call;
	table.read_engine = RoadUnused<decltype(table.read_engine)>::Call;
	table.read_first = RoadUnused<decltype(table.read_first)>::Call;
	table.read_front = RoadUnused<decltype(table.read_front)>::Call;
	table.read_last_station = RoadUnused<decltype(table.read_last_station)>::Call;
	table.read_length = RoadUnused<decltype(table.read_length)>::Call;
	table.read_next = RoadUnused<decltype(table.read_next)>::Call;
	table.read_order_type = RoadUnused<decltype(table.read_order_type)>::Call;
	table.read_previous = RoadUnused<decltype(table.read_previous)>::Call;
	table.read_progress = RoadUnused<decltype(table.read_progress)>::Call;
	table.read_running = RoadUnused<decltype(table.read_running)>::Call;
	table.read_speed = RoadUnused<decltype(table.read_speed)>::Call;
	table.read_status = RoadUnused<decltype(table.read_status)>::Call;
	table.read_tick = RoadUnused<decltype(table.read_tick)>::Call;
	table.read_tile = RoadUnused<decltype(table.read_tile)>::Call;
	table.read_total_length = RoadUnused<decltype(table.read_total_length)>::Call;
	table.read_tram = RoadUnused<decltype(table.read_tram)>::Call;
	table.speed_limits = RoadUnused<decltype(table.speed_limits)>::Call;
	table.consist_speed = RoadUnused<decltype(table.consist_speed)>::Call;
	table.close_origin = RoadUnused<decltype(table.close_origin)>::Call;
	table.close_candidate = RoadUnused<decltype(table.close_candidate)>::Call;
	table.overtake_origin = RoadUnused<decltype(table.overtake_origin)>::Call;
	table.overtake_speed = RoadUnused<decltype(table.overtake_speed)>::Call;
	table.sliding_position = RoadUnused<decltype(table.sliding_position)>::Call;
	table.height_speed = RoadUnused<decltype(table.height_speed)>::Call;
	table.collision_part = RoadUnused<decltype(table.collision_part)>::Call;
	table.collision_origin = RoadUnused<decltype(table.collision_origin)>::Call;
	table.crash_direction = RoadUnused<decltype(table.crash_direction)>::Call;
	table.path_vehicle = RoadUnused<decltype(table.path_vehicle)>::Call;
	table.depot_part = RoadUnused<decltype(table.depot_part)>::Call;
	table.depot_orders = RoadUnused<decltype(table.depot_orders)>::Call;
	table.vehicle_tile = RoadUnused<decltype(table.vehicle_tile)>::Call;
	table.arrival_vehicle = RoadUnused<decltype(table.arrival_vehicle)>::Call;
	table.tunnel_vehicle = RoadUnused<decltype(table.tunnel_vehicle)>::Call;
	table.move_vehicle = RoadUnused<decltype(table.move_vehicle)>::Call;
	table.move_transition = RoadUnused<decltype(table.move_transition)>::Call;
	table.move_position = RoadUnused<decltype(table.move_position)>::Call;
	table.block_vehicle = RoadUnused<decltype(table.block_vehicle)>::Call;
	table.stop_order = RoadUnused<decltype(table.stop_order)>::Call;
	table.move_stop = RoadUnused<decltype(table.move_stop)>::Call;
	table.order_clock = RoadUnused<decltype(table.order_clock)>::Call;
	table.controller_part = RoadUnused<decltype(table.controller_part)>::Call;
	table.service_origin = RoadUnused<decltype(table.service_origin)>::Call;
	table.service_order = RoadUnused<decltype(table.service_order)>::Call;
	table.track_direction = RoadUnused<decltype(table.track_direction)>::Call;
	table.slope_origin = RoadUnused<decltype(table.slope_origin)>::Call;
	table.slope_part = RoadUnused<decltype(table.slope_part)>::Call;
	table.turn_vehicle = RoadUnused<decltype(table.turn_vehicle)>::Call;
	table.owner = RoadUnused<decltype(table.owner)>::Call;
	table.visit_close = RoadUnused<decltype(table.visit_close)>::Call;
	table.visit_tunnel = RoadUnused<decltype(table.visit_tunnel)>::Call;
	table.visit_tile = RoadUnused<decltype(table.visit_tile)>::Call;
	table.visit_train = RoadUnused<decltype(table.visit_train)>::Call;
	table.read_bus = RoadUnused<decltype(table.read_bus)>::Call;
	return table;
}
struct RoadAbiProbe {
	OpenTTDRoadState *state = openttd_rust_road_new();
	const OpenTTDRoadLeaves *leaves = nullptr;
	const OpenTTDSharedServices *services = nullptr;
	uint32_t tick = 255, front = 0, status = 0, order_time = 0;
	uint32_t length = 8, total_length = 8, dest = 0;
	uint32_t departures = 0, nested = 0, destroyed = 0, owner_reads = 0;
};
static RoadAbiProbe *road_abi_probe;
static void RoadBoundary()
{
	RoadAbiProbe probe;
	road_abi_probe = &probe;
	EffectTestWorld world;
	effect_test_world = &world;
	const OpenTTDSharedServices services = OpenTTDFixtureSharedServices(&world, EffectTestRandom, EffectTestIndustry);
	auto leaves = RoadTestLeaves();
	probe.leaves = &leaves;
	probe.services = &services;
	leaves.owner = [](uint32_t) noexcept { ++road_abi_probe->owner_reads; return road_abi_probe->state; };
	leaves.read_tick = [](uint32_t) noexcept { return road_abi_probe->tick; };
	leaves.set_tick = [](uint32_t, uint8_t tick) noexcept { road_abi_probe->tick = tick; };
	leaves.read_front = [](uint32_t) noexcept { return road_abi_probe->front; };
	CHECK(openttd_rust_road_tick(17, probe.state, &leaves, &services));
	CHECK(probe.tick == 0 && world.draws == 0 && probe.owner_reads == 0);

	leaves.op_ground_crash = [](uint32_t id, bool flooded) noexcept -> uint32_t {
		CHECK(id == 17 && !flooded);
		/* A direct callback changes canonical state before Rust reobserves it. */
		road_abi_probe->front = 1;
		openttd_rust_road_set(road_abi_probe->state, 0, 64);
		return 7;
	};
	leaves.op_roadstop_leave = [](uint32_t) noexcept { ++road_abi_probe->departures; };
	CHECK(openttd_rust_road_crash(17, probe.state, false, &leaves, &services) == 8);
	CHECK(probe.departures == 1 && openttd_rust_road_get(probe.state, 5) == 1);

	leaves.speed_limits = [](uint32_t) noexcept -> OpenTTDRoadSpeedLimits { return {80, 65535}; };
	leaves.consist_speed = [](uint32_t) noexcept -> OpenTTDRoadConsistSpeed { return {1, UINT32_MAX, 0, 100}; };
	leaves.op_acc_model = []() noexcept -> uint32_t { return 0; };
	leaves.op_bay_stop = [](uint32_t) noexcept -> uint32_t { return 0; };
	leaves.track_direction = [](uint32_t) noexcept -> OpenTTDRoadTrackDirection { return {1, 0, 100}; };
	leaves.read_dest = [](uint32_t) noexcept { return road_abi_probe->dest; };
	leaves.op_set_dest = [](uint32_t, uint32_t tile) noexcept { road_abi_probe->dest = tile; };
	leaves.op_cache_invalidate = [](uint32_t) noexcept {};
	leaves.set_total_length = [](uint32_t, uint16_t length) noexcept { road_abi_probe->total_length = length; };
	leaves.op_engine_invalid = []() noexcept -> uint32_t { return 65535; };
	leaves.set_first_engine = [](uint32_t, uint16_t) noexcept {};
	leaves.op_grf_version = [](uint32_t) noexcept -> uint32_t { return 8; };
	leaves.op_property = [](uint32_t id, uint8_t property, uint32_t fallback) noexcept -> uint32_t {
		auto &p = *road_abi_probe;
		CHECK(openttd_rust_road_max_speed(id, p.state, p.leaves, p.services) == 80);
		CHECK(openttd_rust_road_trackdir(id, p.state, p.leaves, p.services) == 0);
		++p.nested;
		openttd_rust_road_set_dest(id, p.state, 142 + p.nested, p.leaves, p.services);
		CHECK(openttd_rust_road_path_size(p.state) == 0);
		openttd_rust_road_path_push(p.state, {10, 10815});
		return property == 0x23 ? 0 : property == 0x15 ? 20 : fallback;
	};
	leaves.read_length = [](uint32_t) noexcept { return road_abi_probe->length; };
	leaves.set_length = [](uint32_t, uint8_t length) noexcept { road_abi_probe->length = length; };
	leaves.read_total_length = [](uint32_t) noexcept { return road_abi_probe->total_length; };
	leaves.op_update_visual = [](uint32_t) noexcept {};
	leaves.op_age_default = [](uint32_t) noexcept -> uint32_t { return 74; };
	leaves.set_cargo_age = [](uint32_t, uint16_t age) noexcept { CHECK(age == 74); };
	leaves.read_next = [](uint32_t) noexcept -> uint32_t { return UINT32_MAX; };
	leaves.set_max_speed = [](uint32_t, uint16_t speed) noexcept { CHECK(speed == 80); };
	openttd_rust_road_update_cache(17, probe.state, false, &leaves, &services);
	CHECK(probe.nested == 3 && probe.dest == 145 && probe.total_length == 8);
	CHECK(openttd_rust_road_path_get(probe.state, 0).tile == 10815);

	/* ShowVisualEffect's nested speed getter uses the same direct entry/table. */
	leaves.read_status = [](uint32_t) noexcept { return road_abi_probe->status; };
	leaves.set_running = [](uint32_t, uint8_t) noexcept {};
	leaves.read_running = [](uint32_t) noexcept -> uint32_t { return 0; };
	leaves.order_clock = [](uint32_t) noexcept -> OpenTTDRoadOrderClock { return {road_abi_probe->order_time}; };
	leaves.set_order_time = [](uint32_t, int32_t value) noexcept { road_abi_probe->order_time = static_cast<uint32_t>(value); };
	leaves.collision_part = [](uint32_t) noexcept -> OpenTTDRoadCollisionPart { return {UINT32_MAX, 100, 0}; };
	leaves.op_is_crossing = [](uint32_t) noexcept -> uint32_t { return 0; };
	leaves.op_breakdown = [](uint32_t) noexcept -> uint32_t { return 0; };
	leaves.op_process_orders = [](uint32_t id) noexcept { auto &p = *road_abi_probe; openttd_rust_road_set_dest(id, p.state, 100, p.leaves, p.services); };
	leaves.op_loading = [](uint32_t) noexcept {};
	leaves.read_order_type = [](uint32_t) noexcept -> uint32_t { return 0; };
	leaves.op_visual = [](uint32_t id) noexcept { auto &p = *road_abi_probe; CHECK(openttd_rust_road_max_speed(id, p.state, p.leaves, p.services) == 80); ++p.nested; };
	leaves.op_update_speed = [](uint32_t, uint32_t acceleration, int32_t minimum, int32_t maximum) noexcept -> int32_t { CHECK(acceleration == 256 && minimum == 0 && maximum == 80); return 0; };
	leaves.op_advance = [](uint32_t) noexcept -> uint32_t { return 1; };
	leaves.controller_part = [](uint32_t) noexcept -> OpenTTDRoadControllerPart { return {UINT32_MAX, 0}; };
	leaves.op_viewport = [](uint32_t, bool force, bool delta) noexcept { CHECK(!force && !delta); };
	leaves.read_progress = [](uint32_t) noexcept -> uint32_t { return 0; };
	leaves.set_progress = [](uint32_t, uint8_t progress) noexcept { CHECK(progress == 0); };
	leaves.op_last_speed = [](uint32_t) noexcept {};
	CHECK(openttd_rust_road_tick(17, probe.state, &leaves, &services));
	CHECK(probe.nested == 4 && probe.dest == 100 && world.draws == 0);

	/* The final-part deletion continuation returns its copied alive result only. */
	probe.tick = 31;
	probe.status = 128;
	openttd_rust_road_set(probe.state, 0, 0);
	openttd_rust_road_set(probe.state, 5, 2219);
	leaves.read_first = [](uint32_t) noexcept -> uint32_t { return 17; };
	leaves.op_set_next = [](uint32_t id, uint32_t next) noexcept { CHECK(id == 17 && next == UINT32_MAX); };
	leaves.read_last_station = [](uint32_t) noexcept -> uint32_t { return 23; };
	leaves.set_last_station = [](uint32_t id, uint16_t station) noexcept { CHECK(id == 17 && station == 23); };
	leaves.op_delete = [](uint32_t id) noexcept { CHECK(id == 17); openttd_rust_road_destroy(road_abi_probe->state); road_abi_probe->state = nullptr; ++road_abi_probe->destroyed; };
	CHECK(!openttd_rust_road_tick(17, probe.state, &leaves, &services));
	CHECK(probe.state == nullptr && probe.destroyed == 1 && probe.owner_reads == 0);
	std::printf("road_native_typed_reentry_path_and_owner_destruction passed\n");
}

/* Moving and partly entered consists cannot use IsChainInDepot's service path,
 * but FindClosestRoadDepot still selects their current depot at distance zero. */
struct RoadServiceProbe {
	OpenTTDRoadState *head = openttd_rust_road_new();
	OpenTTDRoadState *tail = openttd_rust_road_new();
	uint32_t tile = 3091, dest = 0, speed = 5;
	bool depot_tile = true;
	uint32_t depot_orders = 0, services = 0, searches = 0;
};
static RoadServiceProbe *road_service_probe;
static void RoadServiceBoundary()
{
	RoadServiceProbe probe;
	road_service_probe = &probe;
	EffectTestWorld world;
	effect_test_world = &world;
	const OpenTTDSharedServices services = OpenTTDFixtureSharedServices(&world, EffectTestRandom, EffectTestIndustry);
	auto leaves = RoadTestLeaves();
	leaves.owner = [](uint32_t id) noexcept { return id == 17 ? road_service_probe->head : road_service_probe->tail; };
	leaves.read_front = [](uint32_t) noexcept -> uint32_t { return 1; };
	leaves.read_day = [](uint32_t) noexcept -> uint32_t { return 0; };
	leaves.read_running = [](uint32_t) noexcept -> uint32_t { return 0; };
	leaves.set_day = [](uint32_t, uint8_t day) noexcept { CHECK(day == 1); };
	leaves.set_suppress_implicit = [](uint32_t) noexcept {};
	leaves.op_servint = [](uint32_t) noexcept -> uint32_t { return 1; };
	leaves.op_needs_service = [](uint32_t) noexcept -> uint32_t { return 1; };
	leaves.service_origin = [](uint32_t) noexcept -> OpenTTDRoadServiceOrigin { auto &p = *road_service_probe; return {17, p.speed, p.tile}; };
	leaves.depot_part = [](uint32_t id) noexcept -> OpenTTDRoadDepotPart { return {id == 17 ? 18U : UINT32_MAX, road_service_probe->tile}; };
	leaves.op_is_depot = [](uint32_t tile) noexcept -> uint32_t { CHECK(tile == road_service_probe->tile); return road_service_probe->depot_tile; };
	leaves.op_max_penalty = []() noexcept -> uint32_t { return 300; };
	leaves.op_depot_index = [](uint32_t tile) noexcept -> uint32_t { CHECK(tile == road_service_probe->tile); return 55; };
	leaves.op_order_depot = [](uint32_t, uint16_t depot) noexcept { CHECK(depot == 55); ++road_service_probe->depot_orders; };
	leaves.read_dest = [](uint32_t) noexcept { return road_service_probe->dest; };
	leaves.op_set_dest = [](uint32_t, uint32_t tile) noexcept { CHECK(tile == road_service_probe->tile); road_service_probe->dest = tile; };
	leaves.op_service = [](uint32_t) noexcept { ++road_service_probe->services; };
	leaves.service_order = [](uint32_t) noexcept -> OpenTTDRoadServiceOrder { return {}; };
	leaves.read_order_type = [](uint32_t) noexcept -> uint32_t { return 0; };
	leaves.op_find_depot = [](uint32_t, int32_t penalty) noexcept -> OpenTTDRoadDepotResult { CHECK(penalty == 300); ++road_service_probe->searches; return {UINT32_MAX, UINT32_MAX}; };
	leaves.op_economy_age = [](uint32_t) noexcept {};
	leaves.op_check_breakdown = [](uint32_t) noexcept {};
	leaves.op_check_orders = [](uint32_t) noexcept {};
	leaves.op_start_stop_dirty = [](uint32_t) noexcept {};
	auto day = [&]() { openttd_rust_road_economy_day(17, probe.head, &leaves, &services); };
	for (bool moving : {true, false}) {
		probe.speed = moving ? 5 : 0;
		probe.dest = 42;
		openttd_rust_road_set(probe.head, 0, 254);
		openttd_rust_road_set(probe.tail, 0, moving ? 254 : 1);
		openttd_rust_road_path_push(probe.head, {10, 10815});
		day();
		CHECK(probe.dest == probe.tile && openttd_rust_road_path_size(probe.head) == 0);
	}
	CHECK(probe.depot_orders == 2 && probe.services == 0 && world.draws == 0);
	openttd_rust_road_set(probe.tail, 0, 254);
	day();
	CHECK(probe.services == 1 && probe.depot_orders == 2);
	probe.depot_tile = false;
	day();
	CHECK(probe.searches == 1);
	openttd_rust_road_destroy(probe.head);
	openttd_rust_road_destroy(probe.tail);
	std::printf("road_service_moving_partial_parked_and_search_branches passed\n");
}

struct DisasterProbe { uint32_t reads = 0, writes = 0, draws = 0, random = 0; };
static void OPENTTD_DISASTER_CALL DisasterProbeRead(void *context, uint32_t kind, uint32_t, int64_t, int64_t, int64_t *out) noexcept
{
	auto *probe = static_cast<DisasterProbe *>(context); probe->reads++;
	if (kind == 0) { out[0] = 64; out[1] = 128; out[2] = 63; out[3] = 127; out[4] = 8191; out[5] = 1935; out[6] = 0; out[9] = 1; }
	if (kind == 1) { out[0] = 13; out[7] = 8880; }
}
static void OPENTTD_DISASTER_CALL DisasterProbeWrite(void *context, uint32_t, uint32_t, int64_t) noexcept
{
	static_cast<DisasterProbe *>(context)->writes++;
}
static uint32_t DisasterProbeRandom(void *context) noexcept { auto *probe = static_cast<DisasterProbe *>(context); probe->draws++; return probe->random; }
static int64_t OPENTTD_DISASTER_CALL DisasterProbeService(void *, const OpenTTDDisasterAction *) noexcept { return 0; }
static uint32_t DisasterProbeIndustry(int32_t, int32_t, uint32_t *) noexcept { return 0; }
static void Disasters()
{
	using Owner = std::unique_ptr<OpenTTDDisasterRun, decltype(&openttd_rust_disaster_destroy)>;
	using StateOwner = std::unique_ptr<OpenTTDDisasterState, decltype(&openttd_rust_disaster_state_destroy)>;
	DisasterProbe probe;
	const OpenTTDSharedServices services = OpenTTDFixtureSharedServices(&probe, DisasterProbeRandom, DisasterProbeIndustry);
	auto make = [&](uint32_t operation, uint32_t id = 0) { return Owner(openttd_rust_disaster_create(operation, id, 0, 0, 0, 0, &probe, DisasterProbeRead, DisasterProbeWrite, &services, DisasterProbeService), openttd_rust_disaster_destroy); };
	StateOwner state(openttd_rust_disaster_state_create(1048575), openttd_rust_disaster_state_destroy);
	auto *address = state.get(); state->state = 65535; state->flags = 0xAB; state->image_override = UINT32_MAX;
	uint16_t *delay = openttd_rust_disaster_delay(); *delay = 0;
	{ auto daily = make(1); CHECK(openttd_rust_disaster_advance(daily.get(), 0).kind == 0 && *delay == 65535 && probe.draws == 0); }
	*delay = 1; probe.random = 511;
	{ auto daily = make(1); CHECK(openttd_rust_disaster_advance(daily.get(), 0).kind == 0 && *delay == 1241 && probe.draws == 1); }
	probe.random = 0;
	{ auto startup = make(2); CHECK(openttd_rust_disaster_advance(startup.get(), 0).kind == 0 && *delay == 730 && probe.draws == 2); }
	CHECK(delay == openttd_rust_disaster_delay() && state.get() == address && state->state == 65535 && state->flags == 0xAB && state->image_override == UINT32_MAX);
	auto expired = make(0, 17);
	CHECK(openttd_rust_disaster_advance(expired.get(), 0).kind == 2);
	uint32_t reads = probe.reads, writes = probe.writes;
	expired.reset(); CHECK(probe.reads == reads && probe.writes == writes && probe.draws == 2);
	std::printf("disaster_private_counter_direct_rng_delete_cancel passed\n");
}


/* Saves cannot observe NOSAVE rotation coordinates. Compare their full bit domain
 * with native C++ narrowing, and exercise real nested owner mutation in a callback. */
struct ShipProbe {
	uint32_t x = 32768, y = static_cast<uint32_t>(-32769), dest = 0;
	uint32_t order_time = UINT32_MAX;
	uint8_t tick = 255, running = 255, direction = 1;
	uint16_t speed = 33;
	uint32_t stage = 0, paths = 0, positions = 0;
	OpenTTDShipState *owner = nullptr;
	const OpenTTDShipLeaves *leaves = nullptr;
	OpenTTDSharedServices services{};
};
static ShipProbe ship_probe;
template <typename Result, typename... Args>
static Result ShipProbeUnused(Args...) noexcept
{
	if constexpr (!std::is_void_v<Result>) return {};
}
static void ShipBoundary()
{
	using Owner = std::unique_ptr<OpenTTDShipState, decltype(&openttd_rust_ship_state_destroy)>;
	Owner state(openttd_rust_ship_state_new(), openttd_rust_ship_state_destroy);
	CHECK(openttd_rust_ship_get_state(state.get()) == 0 && openttd_rust_ship_get_rotation(state.get()) == 255);
	for (uint32_t value = 0; value <= UINT16_MAX; ++value) {
		openttd_rust_ship_set_state(state.get(), static_cast<uint8_t>(value));
		openttd_rust_ship_set_rotation(state.get(), static_cast<uint8_t>(value));
		openttd_rust_ship_set_rotation_x(state.get(), static_cast<int16_t>(value));
		openttd_rust_ship_set_rotation_y(state.get(), static_cast<int16_t>(value));
		CHECK(openttd_rust_ship_get_state(state.get()) == static_cast<uint8_t>(value));
		CHECK(openttd_rust_ship_get_rotation(state.get()) == static_cast<uint8_t>(value));
		CHECK(openttd_rust_ship_get_rotation_x(state.get()) == static_cast<int16_t>(value));
		CHECK(openttd_rust_ship_get_rotation_y(state.get()) == static_cast<int16_t>(value));
	}
	/* Initialize every function pointer, including paths unused in this concrete gap. */
	OpenTTDShipLeaves leaves{
		.depot_dir = ShipProbeUnused<uint32_t, uint32_t>, .depot_axis = ShipProbeUnused<uint32_t, uint32_t>,
		.is_depot = ShipProbeUnused<bool, uint32_t>, .depot_index = ShipProbeUnused<uint32_t, uint32_t>,
		.wait_unbunch = ShipProbeUnused<bool, uint32_t>, .chain_depot = ShipProbeUnused<bool, uint32_t>,
		.servint = ShipProbeUnused<uint32_t, uint32_t>, .needs_service = ShipProbeUnused<bool, uint32_t>,
		.max_distance = ShipProbeUnused<uint32_t>, .tile_valid = ShipProbeUnused<bool, uint32_t>,
		.tile_type = ShipProbeUnused<uint32_t, uint32_t>, .water_class = ShipProbeUnused<uint32_t, uint32_t>,
		.lock_middle = ShipProbeUnused<bool, uint32_t>, .lock_dir = ShipProbeUnused<uint32_t, uint32_t>,
		.tile_min_z = ShipProbeUnused<uint32_t, uint32_t>, .tile_max_z = ShipProbeUnused<uint32_t, uint32_t>,
		.track_status = ShipProbeUnused<uint32_t, uint32_t, uint32_t>, .offset = ShipProbeUnused<uint32_t, uint32_t>,
		.diag_between = ShipProbeUnused<uint32_t, uint32_t, uint32_t>, .dist_square = ShipProbeUnused<uint32_t, uint32_t, uint32_t>,
		.dist_manhattan = ShipProbeUnused<uint32_t, uint32_t, uint32_t>, .docking = ShipProbeUnused<bool, uint32_t>,
		.dock = ShipProbeUnused<bool, uint32_t>, .dock_water = ShipProbeUnused<bool, uint32_t>,
		.station = ShipProbeUnused<uint32_t, uint32_t>, .industry_station = ShipProbeUnused<uint32_t, uint32_t>,
		.oilrig = ShipProbeUnused<bool, uint32_t>, .station_use = ShipProbeUnused<bool, uint32_t, uint32_t>,
		.station_xy = ShipProbeUnused<uint32_t, uint32_t>, .station_contains = ShipProbeUnused<bool, uint32_t, uint32_t>,
		.station_dock = ShipProbeUnused<bool, uint32_t>, .station_visits = ShipProbeUnused<uint32_t, uint32_t>,
		.visit_set = ShipProbeUnused<void, uint32_t, uint32_t>, .arrival = ShipProbeUnused<void, uint32_t, uint32_t>,
		.service = ShipProbeUnused<void, uint32_t>, .leave_unbunch = ShipProbeUnused<void, uint32_t>,
		.path_result = ShipProbeUnused<void, uint32_t, bool>, .order_free = ShipProbeUnused<void, uint32_t>,
		.order_dummy = ShipProbeUnused<void, uint32_t>, .order_depot = ShipProbeUnused<void, uint32_t, uint32_t>,
		.order_leave = ShipProbeUnused<void, uint32_t>, .order_increment = ShipProbeUnused<void, uint32_t>,
		.timetable = ShipProbeUnused<void, uint32_t>, .position = ShipProbeUnused<void, uint32_t>,
		.start_dirty = ShipProbeUnused<void, uint32_t>, .depot_dirty = ShipProbeUnused<void, uint32_t>,
		.depot_invalidate = ShipProbeUnused<void, uint32_t>, .ships_dirty = ShipProbeUnused<void>,
		.details_dirty = ShipProbeUnused<void, uint32_t>, .age = ShipProbeUnused<void, uint32_t>,
		.economy_age = ShipProbeUnused<void, uint32_t>, .decrease_value = ShipProbeUnused<void, uint32_t>,
		.check_breakdown = ShipProbeUnused<void, uint32_t>, .check_orders = ShipProbeUnused<void, uint32_t>,
		.running_cost = ShipProbeUnused<int64_t, uint32_t>, .cost_divisor = ShipProbeUnused<uint32_t>,
		.pay_running = ShipProbeUnused<void, uint32_t, int64_t>, .speed_default = ShipProbeUnused<uint32_t, uint32_t>,
		.age_default = ShipProbeUnused<uint32_t, uint32_t>, .speed_frac = ShipProbeUnused<uint32_t, uint32_t, bool>,
		.speed_property = ShipProbeUnused<uint32_t, uint32_t, uint32_t>, .age_property = ShipProbeUnused<uint32_t, uint32_t, uint32_t>,
		.update_visual = ShipProbeUnused<void, uint32_t>, .cache_invalidate = ShipProbeUnused<void, uint32_t>,
		.capacity = ShipProbeUnused<uint32_t, uint32_t>, .sprite_direction = ShipProbeUnused<void, uint32_t>,
		.tile_x = ShipProbeUnused<uint32_t, uint32_t>, .tile_y = ShipProbeUnused<uint32_t, uint32_t>,
		.build_flag = ShipProbeUnused<bool, uint32_t>, .build_random = ShipProbeUnused<void, uint32_t, uint16_t>,
		.new_position = ShipProbeUnused<OpenTTDShipPosition, uint32_t>, .exit_dir = ShipProbeUnused<uint32_t, uint32_t, uint32_t>,
		.track_direction = ShipProbeUnused<uint32_t, uint32_t, uint32_t>, .tracks_reach = ShipProbeUnused<uint32_t, uint32_t>,
		.busy_tile = ShipProbeUnused<bool, uint32_t>, .path_size = ShipProbeUnused<size_t, uint32_t>,
		.path_back = ShipProbeUnused<uint32_t, uint32_t>, .path_pop = ShipProbeUnused<void, uint32_t>,
		.path_clear = ShipProbeUnused<void, uint32_t>, .enter_tile = ShipProbeUnused<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>,
		.enter_depot = ShipProbeUnused<void, uint32_t>, .process_orders = ShipProbeUnused<bool, uint32_t>,
		.loading = ShipProbeUnused<void, uint32_t>, .begin_loading = ShipProbeUnused<void, uint32_t>,
		.breakdown = ShipProbeUnused<bool, uint32_t>, .viewport = ShipProbeUnused<void, uint32_t, bool, bool>,
		.base_viewport = ShipProbeUnused<void, uint32_t>, .visual = ShipProbeUnused<void, uint32_t>,
		.cache = ShipProbeUnused<void, uint32_t>, .play_sound = ShipProbeUnused<void, uint32_t>,
		.yapf_reverse = ShipProbeUnused<OpenTTDShipReverseResult, uint32_t, bool>, .yapf_choose = ShipProbeUnused<OpenTTDShipTrackResult, uint32_t, uint32_t>,
		.update_delta = ShipProbeUnused<void, uint32_t>, .build_owner = ShipProbeUnused<void, uint32_t>,
		.build_z = ShipProbeUnused<void, uint32_t>, .build_properties = ShipProbeUnused<void, uint32_t, uint32_t>,
		.build_dates = ShipProbeUnused<void, uint32_t>, .build_acceleration = ShipProbeUnused<void, uint32_t, uint32_t>,
		.build_prototype = ShipProbeUnused<void, uint32_t>, .build_interval_percent = ShipProbeUnused<void, uint32_t>,
		.build_capacity = ShipProbeUnused<void, uint32_t, uint32_t>, .set_tile = ShipProbeUnused<void, uint32_t, uint32_t>,
		.set_x = ShipProbeUnused<void, uint32_t, int32_t>, .set_y = ShipProbeUnused<void, uint32_t, int32_t>,
		.set_z = ShipProbeUnused<void, uint32_t, int32_t>, .set_direction = ShipProbeUnused<void, uint32_t, uint8_t>,
		.set_speed = ShipProbeUnused<void, uint32_t, uint16_t>, .set_tick = ShipProbeUnused<void, uint32_t, uint8_t>,
		.set_running = ShipProbeUnused<void, uint32_t, uint8_t>, .set_day = ShipProbeUnused<void, uint32_t, uint8_t>,
		.set_order_time = ShipProbeUnused<void, uint32_t, int32_t>, .set_progress = ShipProbeUnused<void, uint32_t, uint8_t>,
		.set_last_station = ShipProbeUnused<void, uint32_t, uint16_t>, .set_hidden = ShipProbeUnused<void, uint32_t, bool>,
		.set_max_speed = ShipProbeUnused<void, uint32_t, uint16_t>, .set_cargo_age = ShipProbeUnused<void, uint32_t, uint16_t>,
		.set_dest = ShipProbeUnused<void, uint32_t, uint32_t>, .tile = ShipProbeUnused<uint32_t, uint32_t>,
		.dest = ShipProbeUnused<uint32_t, uint32_t>, .x = ShipProbeUnused<uint32_t, uint32_t>,
		.y = ShipProbeUnused<uint32_t, uint32_t>, .z = ShipProbeUnused<uint32_t, uint32_t>,
		.direction = ShipProbeUnused<uint32_t, uint32_t>, .speed = ShipProbeUnused<uint32_t, uint32_t>,
		.tick = ShipProbeUnused<uint32_t, uint32_t>, .running = ShipProbeUnused<uint32_t, uint32_t>,
		.day = ShipProbeUnused<uint32_t, uint32_t>, .order_time = ShipProbeUnused<uint32_t, uint32_t>,
		.progress = ShipProbeUnused<uint32_t, uint32_t>, .status = ShipProbeUnused<uint32_t, uint32_t>,
		.owner = ShipProbeUnused<uint32_t, uint32_t>, .last_station = ShipProbeUnused<uint32_t, uint32_t>,
		.order_destination = ShipProbeUnused<uint32_t, uint32_t>, .order_type = ShipProbeUnused<uint32_t, uint32_t>,
		.order_max_speed = ShipProbeUnused<uint32_t, uint32_t>, .acceleration = ShipProbeUnused<uint32_t, uint32_t>,
		.max_speed = ShipProbeUnused<uint32_t, uint32_t>, .state_owner = ShipProbeUnused<OpenTTDShipState *, uint32_t>,
		.patch = ShipProbeUnused<OpenTTDWaterPatch, uint32_t>, .neighbours = ShipProbeUnused<size_t, OpenTTDWaterPatch, OpenTTDWaterPatch *>,
		.next_depot = ShipProbeUnused<bool, uint32_t, OpenTTDShipDepot *>
	};
	ship_probe = {};
	ship_probe.owner = state.get();
	ship_probe.leaves = &leaves;
	ship_probe.services = OpenTTDFixtureSharedServices(nullptr, ShipProbeUnused<uint32_t, void *>, ShipProbeUnused<uint32_t, int32_t, int32_t, uint32_t *>);
	leaves.state_owner = [](uint32_t) noexcept { return ship_probe.owner; };
	leaves.x = [](uint32_t) noexcept { return static_cast<uint32_t>(ship_probe.x); };
	leaves.y = [](uint32_t) noexcept { return static_cast<uint32_t>(ship_probe.y); };
	leaves.dest = [](uint32_t) noexcept { return static_cast<uint32_t>(ship_probe.dest); };
	leaves.order_time = [](uint32_t) noexcept { return static_cast<uint32_t>(ship_probe.order_time); };
	leaves.tick = [](uint32_t) noexcept { return static_cast<uint32_t>(ship_probe.tick); };
	leaves.running = [](uint32_t) noexcept { return static_cast<uint32_t>(ship_probe.running); };
	leaves.direction = [](uint32_t) noexcept { return static_cast<uint32_t>(ship_probe.direction); };
	leaves.speed = [](uint32_t) noexcept { return static_cast<uint32_t>(ship_probe.speed); };
	leaves.set_tick = [](uint32_t, uint8_t value) noexcept { ship_probe.tick = value; };
	leaves.set_running = [](uint32_t, uint8_t value) noexcept { ship_probe.running = value; };
	leaves.set_order_time = [](uint32_t, int32_t value) noexcept { ship_probe.order_time = static_cast<uint32_t>(value); };
	leaves.set_direction = [](uint32_t, uint8_t value) noexcept { ship_probe.direction = value; };
	leaves.set_speed = [](uint32_t, uint16_t value) noexcept { ship_probe.speed = value; };
	leaves.set_dest = [](uint32_t, uint32_t value) noexcept { ship_probe.dest = value; };
	leaves.path_clear = [](uint32_t) noexcept { ++ship_probe.paths; };
	leaves.position = [](uint32_t) noexcept { ++ship_probe.positions; };
	leaves.breakdown = [](uint32_t) noexcept {
		CHECK(ship_probe.stage++ == 0);
		CHECK(ship_probe.tick == 0 && ship_probe.running == 0 && ship_probe.order_time == 0);
		return false;
	};
	leaves.process_orders = [](uint32_t) noexcept { CHECK(ship_probe.stage++ == 1); return true; };
	leaves.yapf_reverse = [](uint32_t, bool trackdir) noexcept {
		CHECK(ship_probe.stage++ == 2 && !trackdir);
		return OpenTTDShipReverseResult{true, 255};
	};
	leaves.viewport = [](uint32_t id, bool update_delta, bool force) noexcept {
		CHECK(ship_probe.stage++ == 3 && update_delta && force);
		CHECK(ship_probe.direction == 5 && ship_probe.speed == 0 && ship_probe.paths == 1 && ship_probe.positions == 1);
		CHECK(openttd_rust_ship_get_rotation_x(ship_probe.owner) == static_cast<int16_t>(ship_probe.x));
		CHECK(openttd_rust_ship_get_rotation_y(ship_probe.owner) == static_cast<int16_t>(ship_probe.y));
		/* The outer call holds no Rust reference to this owner during nested mutation. */
		openttd_rust_ship_destination(id, 17, ship_probe.leaves, &ship_probe.services);
		CHECK(ship_probe.dest == 17 && ship_probe.paths == 2);
		openttd_rust_ship_set_rotation(ship_probe.owner, 3);
	};
	CHECK(openttd_rust_ship_tick(4, &leaves, &ship_probe.services));
	CHECK(ship_probe.stage == 4 && openttd_rust_ship_get_rotation(state.get()) == 3);
	CHECK(!openttd_rust_ship_find_depot(4, 80, &leaves, &ship_probe.services).valid);
	leaves.owner = [](uint32_t) noexcept { return uint32_t{3}; };
	leaves.next_depot = [](uint32_t first, OpenTTDShipDepot *out) noexcept {
		if (first != 0) return false;
		*out = {0x1234, 0x80000017, 3, 1};
		return true;
	};
	leaves.dist_square = [](uint32_t, uint32_t) noexcept { return uint32_t{49}; };
	auto depot = openttd_rust_ship_find_depot(4, 80, &leaves, &ship_probe.services);
	CHECK(depot.valid && depot.tile == 0x80000017 && depot.id == 0x1234);
	state.reset(openttd_rust_ship_state_new());
	CHECK(openttd_rust_ship_get_rotation_x(state.get()) == 0 && openttd_rust_ship_get_rotation(state.get()) == 255);
	std::printf("ship scalar widths, native transient narrowing, direct order and nested owner mutation passed\n");
}
/* Direct callbacks reenter with the same live scalar allocation. Deletion models
 * Aircraft::PreDestructor releasing airport blocks before freeing that owner. */
static struct AircraftBoundaryProbe {
	OpenTTDAircraftLeaves leaves;
	OpenTTDAircraftVehicle vehicle;
	uint64_t blocks = UINT64_MAX;
	unsigned orders = 0, deleted = 0;
	uint16_t maximum_speed = 0, cargo_age = 0;
	bool live = true;
} aircraft_probe;
static void AircraftDirectBoundary()
{
	auto &p = aircraft_probe;
	p.leaves = AircraftTestLeaves();
	auto &leaves = p.leaves;
	leaves.vehicle_status = [](OpenTTDAircraftVehicle) noexcept { CHECK(aircraft_probe.live); return uint8_t{0}; };
	leaves.order_type = [](OpenTTDAircraftVehicle) noexcept { return uint8_t{3}; };
	leaves.handle_breakdown = [](OpenTTDAircraftVehicle) noexcept {};
	leaves.handle_loading = [](OpenTTDAircraftVehicle, bool) noexcept {};
	leaves.process_orders = [](OpenTTDAircraftVehicle v) noexcept {
		CHECK(aircraft_probe.live);
		aircraft_probe.orders++;
		openttd_rust_aircraft_cache(&aircraft_probe.leaves, v, true);
		v.state->pos = 19;
	};
	leaves.speed_property = [](OpenTTDAircraftVehicle) noexcept { return uint32_t{200}; };
	leaves.cargo_age_property = [](OpenTTDAircraftVehicle) noexcept { return uint32_t{71}; };
	leaves.range_property = [](OpenTTDAircraftVehicle) noexcept { return uint32_t{41}; };
	leaves.set_maximum_speed = [](OpenTTDAircraftVehicle, uint16_t speed) noexcept { aircraft_probe.maximum_speed = speed; };
	leaves.set_cargo_age = [](OpenTTDAircraftVehicle, uint16_t age) noexcept { aircraft_probe.cargo_age = age; };
	leaves.next = [](OpenTTDAircraftVehicle v) noexcept { return v; };
	p.vehicle = {&p, openttd_rust_aircraft_state_new(), 17};
	CHECK(openttd_rust_aircraft_event(&leaves, p.vehicle, false));
	CHECK(p.orders == 1 && p.vehicle.state->pos == 19 && p.vehicle.state->cached_max_range == 41 && p.vehicle.state->cached_max_range_sqr == 1681);
	CHECK(p.maximum_speed == 2560 && p.cargo_age == 71);
	leaves.vehicle_status = [](OpenTTDAircraftVehicle) noexcept { CHECK(aircraft_probe.live); return uint8_t{128}; };
	leaves.station = [](uint16_t) noexcept -> const void * { CHECK(aircraft_probe.live); return &aircraft_probe; };
	leaves.airport_tile = [](const void *) noexcept { return uint32_t{7}; };
	leaves.airport_fta = [](const void *s) noexcept -> const void * { return s; };
	leaves.airport_blocks = [](const void *) noexcept { CHECK(aircraft_probe.live); return &aircraft_probe.blocks; };
	leaves.node = [](const void *ap, uint8_t) noexcept -> const void * { return ap; };
	leaves.fta = [](const void *) noexcept { return OpenTTDAircraftNode{nullptr, uint64_t{1} << 9, 0, 0, 0}; };
	leaves.fta_blocks = [](const void *) noexcept { return uint64_t{1} << 9; };
	leaves.delete_aircraft = [](OpenTTDAircraftVehicle v) noexcept {
		CHECK(aircraft_probe.live && v.state->crashed_counter == 10002);
		openttd_rust_aircraft_release_blocks(&aircraft_probe.leaves, v);
		CHECK((aircraft_probe.blocks & ((uint64_t{1} << 8) | (uint64_t{1} << 9) | (uint64_t{1} << 29))) == 0);
		openttd_rust_aircraft_state_destroy(v.state);
		aircraft_probe.live = false;
		aircraft_probe.deleted++;
	};
	p.vehicle.state->targetairport = 7;
	p.vehicle.state->crashed_counter = 9999;
	CHECK(!openttd_rust_aircraft_event(&leaves, p.vehicle, false));
	CHECK(!p.live && p.deleted == 1);
	/* A disaster has its own flags and no Aircraft state, even with subtype zero. */
	leaves.map_size_x = []() noexcept { return uint32_t{64}; };
	leaves.map_max_x = leaves.map_max_y = []() noexcept { return uint32_t{63}; };
	leaves.x = leaves.y = leaves.z = [](OpenTTDAircraftVehicle v) noexcept { CHECK(v.state == nullptr); return int32_t{0}; };
	leaves.tile_height = [](uint32_t) noexcept { return int32_t{0}; };
	leaves.subtype = [](OpenTTDAircraftVehicle) noexcept { return uint8_t{0}; };
	leaves.vehicle_type = [](OpenTTDAircraftVehicle) noexcept { return uint8_t{4}; };
	leaves.direction = [](OpenTTDAircraftVehicle) noexcept { return uint8_t{4}; };
	leaves.maximum_speed = [](OpenTTDAircraftVehicle) noexcept { return uint16_t{1}; };
	uint8_t disaster_flags = 0;
	int32_t min = 0, max = 0;
	OpenTTDAircraftVehicle disaster{&p, nullptr, 81};
	openttd_rust_aircraft_flight_bounds(&leaves, disaster, &min, &max);
	CHECK(min == 30 && max == 270);
	CHECK(openttd_rust_aircraft_flight_level(&leaves, disaster, &disaster_flags, false) == 1 && disaster_flags == 4);
	CHECK(openttd_rust_aircraft_flight_level(&leaves, disaster, &disaster_flags, true) == 2 && disaster_flags == 4);
	std::printf("aircraft direct nested mutation, live block release/deletion and disaster flight identity passed\n");
}
static void AircraftOwnership()
{
	std::unique_ptr<OpenTTDAircraftState, decltype(&openttd_rust_aircraft_state_destroy)> state{openttd_rust_aircraft_state_new(), openttd_rust_aircraft_state_destroy};
	std::unique_ptr<uint64_t, decltype(&openttd_rust_airport_blocks_destroy)> airport{openttd_rust_airport_blocks_new(), openttd_rust_airport_blocks_destroy};
	CHECK(state->targetairport == 65535 && state->last_direction == 255 && state->cached_max_range_sqr == 0 && *airport == 0);
	uint8_t &position = state->pos; uint16_t &counter = state->crashed_counter; uint64_t &blocks = *airport;
	position = 255; counter = 65535; blocks = UINT64_MAX; state->cached_max_range = 65535; state->cached_max_range_sqr = UINT32_MAX;
	CHECK(state->pos == 255 && state->crashed_counter == 65535 && *airport == UINT64_MAX && state->cached_max_range == 65535 && state->cached_max_range_sqr == UINT32_MAX);
	std::printf("aircraft_airport_canonical_scalar_lifetime passed\n");
}

int main()
{
	Layouts();
	ShipBoundary();
	RoadBoundary();
	RoadServiceBoundary();
	WaterProbe::Run([](bool result) { CHECK(result); });
	ShipYapfProbe::Run([](bool result) { CHECK(result); });
	CheckEffectProtocol([](bool result) { CHECK(result); });
	LinkGraphJob();
	Trees();
	Disasters();
	AircraftOwnership();
	AircraftDirectBoundary();
	/* Opaque owner, native size_t, immutable byte borrow and complete UTF-8 output. */
	auto *townname = openttd_rust_townname_generate(1, UINT32_MAX);
	size_t name_length;
	const char *name = openttd_rust_townname_data(townname, &name_length);
	CHECK(name_length == 8 && std::memcmp(name, "Alen\xc3\xa7on", name_length) == 0);
	openttd_rust_townname_destroy(townname);
	Calls();
	Encoded();
	History();
	Math();
	CryptoPrimitives();
	Blake2b();
	X25519();
	StationCargo();
	PacketState();
	StringValidation();
	ScriptListControl();
	Locale();
	std::printf("ABI audit passed\n");
}
