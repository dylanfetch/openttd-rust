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
#include "rust/ship_yapf_ffi.h"
#include "rust/rail_yapf_ffi.h"
#include "rust/train_ffi.h"
#include "rust/train_reservation_ffi.h"
#include "rust/linkgraph_ffi.h"
#include "rust/trees_ffi.h"
#include "rust/disaster_ffi.h"
#include "rust/aircraft_ffi.h"
#include "rust/townname_ffi.h"
#include "rust/effect_ffi.h"
#include "rust/road_ffi.h"
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
	Layout(38, "OpenTTDTreeAction", {sizeof(OpenTTDTreeAction), alignof(OpenTTDTreeAction), offsetof(OpenTTDTreeAction, kind), offsetof(OpenTTDTreeAction, tile), offsetof(OpenTTDTreeAction, a), offsetof(OpenTTDTreeAction, b), offsetof(OpenTTDTreeAction, cost)});
	Layout(42, "OpenTTDSharedServices", {sizeof(OpenTTDSharedServices), alignof(OpenTTDSharedServices), offsetof(OpenTTDSharedServices, context), offsetof(OpenTTDSharedServices, random), offsetof(OpenTTDSharedServices, observe_tile), offsetof(OpenTTDSharedServices, write_tile), offsetof(OpenTTDSharedServices, trig), offsetof(OpenTTDSharedServices, industry)});
	Layout(37, "OpenTTDLinkGraphShare", {sizeof(OpenTTDLinkGraphShare), alignof(OpenTTDLinkGraphShare), offsetof(OpenTTDLinkGraphShare, node), offsetof(OpenTTDLinkGraphShare, origin), offsetof(OpenTTDLinkGraphShare, via), offsetof(OpenTTDLinkGraphShare, cumulative), offsetof(OpenTTDLinkGraphShare, unrestricted), offsetof(OpenTTDLinkGraphShare, has_share)});
	Layout(39, "OpenTTDEffectView", {sizeof(OpenTTDEffectView), alignof(OpenTTDEffectView), offsetof(OpenTTDEffectView, x), offsetof(OpenTTDEffectView, y), offsetof(OpenTTDEffectView, z), offsetof(OpenTTDEffectView, sprite), offsetof(OpenTTDEffectView, progress), offsetof(OpenTTDEffectView, spritenum), offsetof(OpenTTDEffectView, subtype), offsetof(OpenTTDEffectView, ambient)});
	Layout(41, "OpenTTDEffectLeaves", {sizeof(OpenTTDEffectLeaves), alignof(OpenTTDEffectLeaves), offsetof(OpenTTDEffectLeaves, observe), offsetof(OpenTTDEffectLeaves, write), offsetof(OpenTTDEffectLeaves, viewport), offsetof(OpenTTDEffectLeaves, sound), offsetof(OpenTTDEffectLeaves, animated)});
	Layout(80, "OpenTTDRoadPathElement", {sizeof(OpenTTDRoadPathElement), alignof(OpenTTDRoadPathElement), offsetof(OpenTTDRoadPathElement, trackdir), offsetof(OpenTTDRoadPathElement, tile)});
	Layout(81, "OpenTTDRoadView", {sizeof(OpenTTDRoadView), alignof(OpenTTDRoadView), offsetof(OpenTTDRoadView, type), offsetof(OpenTTDRoadView, first), offsetof(OpenTTDRoadView, next), offsetof(OpenTTDRoadView, previous), offsetof(OpenTTDRoadView, tile), offsetof(OpenTTDRoadView, dest), offsetof(OpenTTDRoadView, x), offsetof(OpenTTDRoadView, y), offsetof(OpenTTDRoadView, z), offsetof(OpenTTDRoadView, direction), offsetof(OpenTTDRoadView, speed), offsetof(OpenTTDRoadView, tick), offsetof(OpenTTDRoadView, running), offsetof(OpenTTDRoadView, day), offsetof(OpenTTDRoadView, order_time), offsetof(OpenTTDRoadView, progress), offsetof(OpenTTDRoadView, status), offsetof(OpenTTDRoadView, owner), offsetof(OpenTTDRoadView, engine), offsetof(OpenTTDRoadView, last_station), offsetof(OpenTTDRoadView, order_destination), offsetof(OpenTTDRoadView, order_type), offsetof(OpenTTDRoadView, order_max_speed), offsetof(OpenTTDRoadView, breakdown), offsetof(OpenTTDRoadView, max_track_speed), offsetof(OpenTTDRoadView, length), offsetof(OpenTTDRoadView, total_length), offsetof(OpenTTDRoadView, roadtype), offsetof(OpenTTDRoadView, front), offsetof(OpenTTDRoadView, articulated), offsetof(OpenTTDRoadView, tram), offsetof(OpenTTDRoadView, bus), offsetof(OpenTTDRoadView, order_nonstop)});
	Layout(82, "OpenTTDRoadLeaves", {sizeof(OpenTTDRoadLeaves), alignof(OpenTTDRoadLeaves), offsetof(OpenTTDRoadLeaves, observe), offsetof(OpenTTDRoadLeaves, write), offsetof(OpenTTDRoadLeaves, leaf), offsetof(OpenTTDRoadLeaves, owner), offsetof(OpenTTDRoadLeaves, nearby)});
	Layout(83, "OpenTTDRoadAction", {sizeof(OpenTTDRoadAction), alignof(OpenTTDRoadAction), offsetof(OpenTTDRoadAction, op), offsetof(OpenTTDRoadAction, id), offsetof(OpenTTDRoadAction, a), offsetof(OpenTTDRoadAction, b), offsetof(OpenTTDRoadAction, c)});
	Layout(44, "OpenTTDDisasterState", {sizeof(OpenTTDDisasterState), alignof(OpenTTDDisasterState), offsetof(OpenTTDDisasterState, image_override), offsetof(OpenTTDDisasterState, target), offsetof(OpenTTDDisasterState, state), offsetof(OpenTTDDisasterState, flags)});
	Layout(45, "OpenTTDDisasterAction", {sizeof(OpenTTDDisasterAction), alignof(OpenTTDDisasterAction), offsetof(OpenTTDDisasterAction, kind), offsetof(OpenTTDDisasterAction, id), offsetof(OpenTTDDisasterAction, other), offsetof(OpenTTDDisasterAction, a), offsetof(OpenTTDDisasterAction, b), offsetof(OpenTTDDisasterAction, c), offsetof(OpenTTDDisasterAction, d)});
	Layout(46, "OpenTTDWaterPatch", {sizeof(OpenTTDWaterPatch), alignof(OpenTTDWaterPatch), offsetof(OpenTTDWaterPatch, x), offsetof(OpenTTDWaterPatch, y), offsetof(OpenTTDWaterPatch, label)});
	Layout(47, "OpenTTDWaterSnapshot", {sizeof(OpenTTDWaterSnapshot), alignof(OpenTTDWaterSnapshot), offsetof(OpenTTDWaterSnapshot, edges), offsetof(OpenTTDWaterSnapshot, labels), offsetof(OpenTTDWaterSnapshot, patches), offsetof(OpenTTDWaterSnapshot, aqueducts)});
	Layout(48, "OpenTTDWaterLeaves", {sizeof(OpenTTDWaterLeaves), alignof(OpenTTDWaterLeaves), offsetof(OpenTTDWaterLeaves, tracks), offsetof(OpenTTDWaterLeaves, follow), offsetof(OpenTTDWaterLeaves, aqueduct), offsetof(OpenTTDWaterLeaves, debug)});
	Layout(180, "OpenTTDAircraftState", {sizeof(OpenTTDAircraftState), alignof(OpenTTDAircraftState), offsetof(OpenTTDAircraftState, cached_max_range_sqr), offsetof(OpenTTDAircraftState, cached_max_range), offsetof(OpenTTDAircraftState, cache_padding), offsetof(OpenTTDAircraftState, crashed_counter), offsetof(OpenTTDAircraftState, targetairport), offsetof(OpenTTDAircraftState, pos), offsetof(OpenTTDAircraftState, previous_pos), offsetof(OpenTTDAircraftState, state), offsetof(OpenTTDAircraftState, last_direction), offsetof(OpenTTDAircraftState, number_consecutive_turns), offsetof(OpenTTDAircraftState, turn_counter), offsetof(OpenTTDAircraftState, flags)});
	Layout(181, "OpenTTDAircraftAction", {sizeof(OpenTTDAircraftAction), alignof(OpenTTDAircraftAction), offsetof(OpenTTDAircraftAction, kind), offsetof(OpenTTDAircraftAction, id), offsetof(OpenTTDAircraftAction, other), offsetof(OpenTTDAircraftAction, a), offsetof(OpenTTDAircraftAction, b), offsetof(OpenTTDAircraftAction, c), offsetof(OpenTTDAircraftAction, d)});

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

	Layout(240, "OpenTTDOrderFields", {sizeof(OpenTTDOrderFields), alignof(OpenTTDOrderFields), offsetof(OpenTTDOrderFields, type), offsetof(OpenTTDOrderFields, flags), offsetof(OpenTTDOrderFields, destination), offsetof(OpenTTDOrderFields, refit_cargo), offsetof(OpenTTDOrderFields, wait_time), offsetof(OpenTTDOrderFields, travel_time), offsetof(OpenTTDOrderFields, max_speed)});
	Layout(241, "OpenTTDConsistState", {sizeof(OpenTTDConsistState), alignof(OpenTTDConsistState), offsetof(OpenTTDConsistState, current_order_time), offsetof(OpenTTDConsistState, lateness_counter), offsetof(OpenTTDConsistState, timetable_start), offsetof(OpenTTDConsistState, last_departure), offsetof(OpenTTDConsistState, next_departure), offsetof(OpenTTDConsistState, round_trip_time), offsetof(OpenTTDConsistState, real_index), offsetof(OpenTTDConsistState, implicit_index), offsetof(OpenTTDConsistState, vehicle_flags)});
	Layout(242, "OpenTTDVehicleOrderState", {sizeof(OpenTTDVehicleOrderState), alignof(OpenTTDVehicleOrderState), offsetof(OpenTTDVehicleOrderState, current), offsetof(OpenTTDVehicleOrderState, orders), offsetof(OpenTTDVehicleOrderState, next_shared), offsetof(OpenTTDVehicleOrderState, previous_shared)});
	Layout(243, "OpenTTDOrderListState", {sizeof(OpenTTDOrderListState), alignof(OpenTTDOrderListState), offsetof(OpenTTDOrderListState, manual), offsetof(OpenTTDOrderListState, vehicles), offsetof(OpenTTDOrderListState, first_shared), offsetof(OpenTTDOrderListState, timetable_duration), offsetof(OpenTTDOrderListState, total_duration)});
	Layout(244, "OpenTTDOrderBackupState", {sizeof(OpenTTDOrderBackupState), alignof(OpenTTDOrderBackupState), offsetof(OpenTTDOrderBackupState, user), offsetof(OpenTTDOrderBackupState, tile), offsetof(OpenTTDOrderBackupState, group), offsetof(OpenTTDOrderBackupState, clone)});
	Layout(245, "OpenTTDOrdersLeaves", {sizeof(OpenTTDOrdersLeaves), alignof(OpenTTDOrdersLeaves), offsetof(OpenTTDOrdersLeaves, vehicle), offsetof(OpenTTDOrdersLeaves, consist), offsetof(OpenTTDOrdersLeaves, list), offsetof(OpenTTDOrdersLeaves, vector), offsetof(OpenTTDOrdersLeaves, backup), offsetof(OpenTTDOrdersLeaves, backup_vector), offsetof(OpenTTDOrdersLeaves, backup_consist), offsetof(OpenTTDOrdersLeaves, first_vehicle), offsetof(OpenTTDOrdersLeaves, last_station), offsetof(OpenTTDOrdersLeaves, ownerless_station), offsetof(OpenTTDOrdersLeaves, vehicle_type), offsetof(OpenTTDOrdersLeaves, vehicle_status), offsetof(OpenTTDOrdersLeaves, tick_counter), offsetof(OpenTTDOrdersLeaves, primary_vehicle), offsetof(OpenTTDOrdersLeaves, vehicle_ownership), offsetof(OpenTTDOrdersLeaves, ticks_per_second), offsetof(OpenTTDOrdersLeaves, unit_number), offsetof(OpenTTDOrdersLeaves, economy_date), offsetof(OpenTTDOrdersLeaves, economy_fraction), offsetof(OpenTTDOrdersLeaves, maximum_date), offsetof(OpenTTDOrdersLeaves, timetable_year_limit), offsetof(OpenTTDOrdersLeaves, stopped_or_crashed), offsetof(OpenTTDOrdersLeaves, allocate_list), offsetof(OpenTTDOrdersLeaves, suppress_implicit), offsetof(OpenTTDOrdersLeaves, shared_window), offsetof(OpenTTDOrdersLeaves, vehicle_id), offsetof(OpenTTDOrdersLeaves, percent_filled), offsetof(OpenTTDOrdersLeaves, reliability), offsetof(OpenTTDOrdersLeaves, engine_reliability), offsetof(OpenTTDOrdersLeaves, display_speed), offsetof(OpenTTDOrdersLeaves, age_years), offsetof(OpenTTDOrdersLeaves, needs_service), offsetof(OpenTTDOrdersLeaves, remaining_years), offsetof(OpenTTDOrdersLeaves, airport_tile), offsetof(OpenTTDOrdersLeaves, base_station_tile), offsetof(OpenTTDOrdersLeaves, station_tile), offsetof(OpenTTDOrdersLeaves, depot_tile), offsetof(OpenTTDOrdersLeaves, distance), offsetof(OpenTTDOrdersLeaves, station_location), offsetof(OpenTTDOrdersLeaves, destination_tile), offsetof(OpenTTDOrdersLeaves, aircraft_flying), offsetof(OpenTTDOrdersLeaves, target_airport), offsetof(OpenTTDOrdersLeaves, waypoint_tile), offsetof(OpenTTDOrdersLeaves, at_station), offsetof(OpenTTDOrdersLeaves, tile_station), offsetof(OpenTTDOrdersLeaves, ship_station_tile), offsetof(OpenTTDOrdersLeaves, valid_station), offsetof(OpenTTDOrdersLeaves, station_owner), offsetof(OpenTTDOrdersLeaves, can_use_station), offsetof(OpenTTDOrdersLeaves, owner_check), offsetof(OpenTTDOrdersLeaves, station_error), offsetof(OpenTTDOrdersLeaves, has_hangar), offsetof(OpenTTDOrdersLeaves, valid_depot), offsetof(OpenTTDOrdersLeaves, depot_owner), offsetof(OpenTTDOrdersLeaves, rail_depot), offsetof(OpenTTDOrdersLeaves, road_depot), offsetof(OpenTTDOrdersLeaves, ship_depot), offsetof(OpenTTDOrdersLeaves, valid_waypoint), offsetof(OpenTTDOrdersLeaves, waypoint_facilities), offsetof(OpenTTDOrdersLeaves, waypoint_owner), offsetof(OpenTTDOrdersLeaves, list_capacity), offsetof(OpenTTDOrdersLeaves, next_backup), offsetof(OpenTTDOrdersLeaves, next_vehicle), offsetof(OpenTTDOrdersLeaves, aircraft_range), offsetof(OpenTTDOrdersLeaves, aircraft_range_square), offsetof(OpenTTDOrdersLeaves, bus), offsetof(OpenTTDOrdersLeaves, backup_capacity), offsetof(OpenTTDOrdersLeaves, create_backup), offsetof(OpenTTDOrdersLeaves, networking), offsetof(OpenTTDOrdersLeaves, network_server), offsetof(OpenTTDOrdersLeaves, network_client), offsetof(OpenTTDOrdersLeaves, server_client), offsetof(OpenTTDOrdersLeaves, default_group), offsetof(OpenTTDOrdersLeaves, vehicle_tile), offsetof(OpenTTDOrdersLeaves, vehicle_group), offsetof(OpenTTDOrdersLeaves, unique_backup_name), offsetof(OpenTTDOrdersLeaves, backup_id), offsetof(OpenTTDOrdersLeaves, backup_hangar), offsetof(OpenTTDOrdersLeaves, review_setting), offsetof(OpenTTDOrdersLeaves, local_owner), offsetof(OpenTTDOrdersLeaves, day_counter), offsetof(OpenTTDOrdersLeaves, fast_aircraft), offsetof(OpenTTDOrdersLeaves, short_strip), offsetof(OpenTTDOrdersLeaves, no_jet_crash), offsetof(OpenTTDOrdersLeaves, append_station), offsetof(OpenTTDOrdersLeaves, invalidate_station_list), offsetof(OpenTTDOrdersLeaves, command_error), offsetof(OpenTTDOrdersLeaves, timetable_dirty), offsetof(OpenTTDOrdersLeaves, invalidate_order), offsetof(OpenTTDOrdersLeaves, vehicle_dirty), offsetof(OpenTTDOrdersLeaves, delete_order_news), offsetof(OpenTTDOrdersLeaves, suppress_implicit_write), offsetof(OpenTTDOrdersLeaves, invalidate_vehicle_list), offsetof(OpenTTDOrdersLeaves, close_shared_window), offsetof(OpenTTDOrdersLeaves, invalidate_shared_window), offsetof(OpenTTDOrdersLeaves, last_station_write), offsetof(OpenTTDOrdersLeaves, dirty_vehicle_windows), offsetof(OpenTTDOrdersLeaves, capture_backup_metadata), offsetof(OpenTTDOrdersLeaves, clear_backup_name), offsetof(OpenTTDOrdersLeaves, restore_backup_metadata), offsetof(OpenTTDOrdersLeaves, order_news), offsetof(OpenTTDOrdersLeaves, debug_list), offsetof(OpenTTDOrdersLeaves, assert_departure_range), offsetof(OpenTTDOrdersLeaves, delete_list), offsetof(OpenTTDOrdersLeaves, leave_station), offsetof(OpenTTDOrdersLeaves, reverse_train), offsetof(OpenTTDOrdersLeaves, next_airport), offsetof(OpenTTDOrdersLeaves, set_destination), offsetof(OpenTTDOrdersLeaves, closest_depot), offsetof(OpenTTDOrdersLeaves, share_command), offsetof(OpenTTDOrdersLeaves, group_command), offsetof(OpenTTDOrdersLeaves, delete_backup), offsetof(OpenTTDOrdersLeaves, clear_backup_gui), offsetof(OpenTTDOrdersLeaves, clear_backup_post), offsetof(OpenTTDOrdersLeaves, missing_aircraft_orders), offsetof(OpenTTDOrdersLeaves, change_timetable_command)});
	Layout(247, "OpenTTDOrdersClosest", {sizeof(OpenTTDOrdersClosest), alignof(OpenTTDOrdersClosest), offsetof(OpenTTDOrdersClosest, tile), offsetof(OpenTTDOrdersClosest, destination), offsetof(OpenTTDOrdersClosest, reverse), offsetof(OpenTTDOrdersClosest, found)});
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

	Layout(151, "OpenTTDTrainView", {sizeof(OpenTTDTrainView), alignof(OpenTTDTrainView), offsetof(OpenTTDTrainView, id), offsetof(OpenTTDTrainView, first), offsetof(OpenTTDTrainView, next), offsetof(OpenTTDTrainView, previous), offsetof(OpenTTDTrainView, next_unit), offsetof(OpenTTDTrainView, last), offsetof(OpenTTDTrainView, tile), offsetof(OpenTTDTrainView, dest), offsetof(OpenTTDTrainView, x), offsetof(OpenTTDTrainView, y), offsetof(OpenTTDTrainView, z), offsetof(OpenTTDTrainView, order_time), offsetof(OpenTTDTrainView, power), offsetof(OpenTTDTrainView, weight), offsetof(OpenTTDTrainView, length), offsetof(OpenTTDTrainView, total_length), offsetof(OpenTTDTrainView, max_speed), offsetof(OpenTTDTrainView, max_track_speed), offsetof(OpenTTDTrainView, speed), offsetof(OpenTTDTrainView, gv_flags), offsetof(OpenTTDTrainView, cargo_cap), offsetof(OpenTTDTrainView, refit_cap), offsetof(OpenTTDTrainView, engine), offsetof(OpenTTDTrainView, first_engine), offsetof(OpenTTDTrainView, order_destination), offsetof(OpenTTDTrainView, last_station), offsetof(OpenTTDTrainView, direction), offsetof(OpenTTDTrainView, status), offsetof(OpenTTDTrainView, tick), offsetof(OpenTTDTrainView, running), offsetof(OpenTTDTrainView, day), offsetof(OpenTTDTrainView, progress), offsetof(OpenTTDTrainView, subspeed), offsetof(OpenTTDTrainView, acceleration), offsetof(OpenTTDTrainView, order), offsetof(OpenTTDTrainView, nonstop), offsetof(OpenTTDTrainView, breakdown), offsetof(OpenTTDTrainView, front), offsetof(OpenTTDTrainView, free_wagon), offsetof(OpenTTDTrainView, articulated), offsetof(OpenTTDTrainView, engine_part), offsetof(OpenTTDTrainView, multiheaded), offsetof(OpenTTDTrainView, owner), offsetof(OpenTTDTrainView, vis_effect)});
	Layout(152, "OpenTTDTrainServices", {sizeof(OpenTTDTrainServices), alignof(OpenTTDTrainServices), offsetof(OpenTTDTrainServices, observe), offsetof(OpenTTDTrainServices, write), offsetof(OpenTTDTrainServices, leaf), offsetof(OpenTTDTrainServices, owner), offsetof(OpenTTDTrainServices, nearby)});
	Layout(153, "OpenTTDTrainAction", {sizeof(OpenTTDTrainAction), alignof(OpenTTDTrainAction), offsetof(OpenTTDTrainAction, op), offsetof(OpenTTDTrainAction, id), offsetof(OpenTTDTrainAction, a), offsetof(OpenTTDTrainAction, b), offsetof(OpenTTDTrainAction, c)});
	Layout(160, "OpenTTDTrainReservationView", {sizeof(OpenTTDTrainReservationView), alignof(OpenTTDTrainReservationView), offsetof(OpenTTDTrainReservationView, tile), offsetof(OpenTTDTrainReservationView, dest), offsetof(OpenTTDTrainReservationView, next), offsetof(OpenTTDTrainReservationView, destination), offsetof(OpenTTDTrainReservationView, last_station), offsetof(OpenTTDTrainReservationView, direction), offsetof(OpenTTDTrainReservationView, order), offsetof(OpenTTDTrainReservationView, num_orders), offsetof(OpenTTDTrainReservationView, order_index), offsetof(OpenTTDTrainReservationView, suppress), offsetof(OpenTTDTrainReservationView, nearest)});
	Layout(161, "OpenTTDTrainReservationFollow", {sizeof(OpenTTDTrainReservationFollow), alignof(OpenTTDTrainReservationFollow), offsetof(OpenTTDTrainReservationFollow, old_tile), offsetof(OpenTTDTrainReservationFollow, new_tile), offsetof(OpenTTDTrainReservationFollow, skipped), offsetof(OpenTTDTrainReservationFollow, dirs), offsetof(OpenTTDTrainReservationFollow, old_td), offsetof(OpenTTDTrainReservationFollow, exitdir), offsetof(OpenTTDTrainReservationFollow, tunnel), offsetof(OpenTTDTrainReservationFollow, bridge), offsetof(OpenTTDTrainReservationFollow, station), offsetof(OpenTTDTrainReservationFollow, error)});
	Layout(162, "OpenTTDTrainReservationPbs", {sizeof(OpenTTDTrainReservationPbs), alignof(OpenTTDTrainReservationPbs), offsetof(OpenTTDTrainReservationPbs, tile), offsetof(OpenTTDTrainReservationPbs, other), offsetof(OpenTTDTrainReservationPbs, td), offsetof(OpenTTDTrainReservationPbs, okay)});
	Layout(163, "OpenTTDTrainReservationStep", {sizeof(OpenTTDTrainReservationStep), alignof(OpenTTDTrainReservationStep), offsetof(OpenTTDTrainReservationStep, value), offsetof(OpenTTDTrainReservationStep, action), offsetof(OpenTTDTrainReservationStep, id), offsetof(OpenTTDTrainReservationStep, tile), offsetof(OpenTTDTrainReservationStep, final_dest), offsetof(OpenTTDTrainReservationStep, td), offsetof(OpenTTDTrainReservationStep, dir), offsetof(OpenTTDTrainReservationStep, tracks), offsetof(OpenTTDTrainReservationStep, reserve), offsetof(OpenTTDTrainReservationStep, found), offsetof(OpenTTDTrainReservationStep, got), offsetof(OpenTTDTrainReservationStep, okay)});
	Layout(164, "OpenTTDTrainReservationLeaves", {sizeof(OpenTTDTrainReservationLeaves), alignof(OpenTTDTrainReservationLeaves), offsetof(OpenTTDTrainReservationLeaves, observe), offsetof(OpenTTDTrainReservationLeaves, leaf), offsetof(OpenTTDTrainReservationLeaves, owner), offsetof(OpenTTDTrainReservationLeaves, follow), offsetof(OpenTTDTrainReservationLeaves, origin)});
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

// ABI/lifetime probe, not simulation evidence. The full game harness compares map/DATE.
struct TreeProbe {
	std::array<uint64_t, 12> settings{0, 15, 35, 4096, 64, 64, 10, 0, 0, 2, 15, 0};
	std::array<uint32_t, 10> tile{7};
	uint32_t writes = 0;
	bool cancel = false;
};
static void TreeSettings(void *context, uint64_t *out) noexcept { auto &p = *static_cast<TreeProbe *>(context); std::copy(p.settings.begin(), p.settings.end(), out); }
static void TreeObserve(void *context, uint32_t, uint32_t *out) noexcept { auto &p = *static_cast<TreeProbe *>(context); std::copy(p.tile.begin(), p.tile.end(), out); }
static void TreeWrite(void *context, uint32_t op, uint32_t, uint32_t a, uint32_t b, uint32_t c, uint32_t d) noexcept
{
	auto &p = *static_cast<TreeProbe *>(context);
	++p.writes;
	CHECK(op == 0 && a == 12 && b == 2 && c == 6 && d == (2U << 8));
}
static float TreeTrig(uint32_t, float value) noexcept { return value; }
static uint32_t TreeRandom(void *) noexcept { return UINT32_MAX; }
static uint64_t TreeLeaf(void *context, uint32_t op, uint32_t, uint32_t, uint32_t) noexcept
{
	return (op == 1 || op == 2) && static_cast<TreeProbe *>(context)->cancel;
}
static void Trees()
{
	using Owner = std::unique_ptr<void, decltype(&openttd_rust_trees_destroy)>;
	TreeProbe probe;
	const OpenTTDSharedServices services{&probe, TreeRandom, TreeObserve, TreeWrite, TreeTrig, EffectTestIndustry};
	auto make = [&](uint32_t kind, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0) {
		return Owner(openttd_rust_trees_create(kind, 1, a, b, c, &probe, TreeSettings, &services, TreeLeaf), openttd_rust_trees_destroy);
	};
	uint8_t *counter = openttd_rust_tree_counter();
	openttd_rust_trees_initialize();
	CHECK(*counter == 0);
	*counter = 37; // Same boundary write made by DATE/TTD descriptors.
	auto tick = make(6);
	CHECK(openttd_rust_trees_advance(tick.get(), 0, 0).kind == 0);
	CHECK(*counter == 36 && counter == openttd_rust_tree_counter());
	probe.settings[3] = 4096 * 4096; probe.settings[4] = probe.settings[5] = 4096;
	tick = make(6);
	CHECK(openttd_rust_trees_advance(tick.get(), 0, 0).kind == 0 && *counter == 36);
	probe.tile = {0, 0, 0, 0, 0, 2};
	auto plant = make(3, 12, 2, 6);
	CHECK(openttd_rust_trees_advance(plant.get(), 0, 0).kind == 0 && probe.writes == 1);
	probe.tile = {4, 0, 0, 0, 3, 3, 12, 1, 3};
	auto loop = make(5);
	CHECK(openttd_rust_trees_advance(loop.get(), 0, 0).kind == 4);
	// Abandon a flooding action: C++ exception cleanup must not call the world.
	loop.reset();
	CHECK(probe.writes == 1);
	openttd_rust_trees_initialize();
	CHECK(*counter == 0 && counter == openttd_rust_tree_counter());
	probe.cancel = true; probe.settings[8] = 1;
	auto generator = make(0);
	CHECK(openttd_rust_trees_advance(generator.get(), 0, 0).kind == 12);
	generator.reset(); // Abort callback and throw are handled only after Rust returns.
	CHECK(probe.writes == 1);
	std::printf("tree_direct_services_counter_cancel_and_reentry_lifetime passed\n");
}

/* Complete typed tables exercise road cdecl calls without importing game globals.
 * These are boundary/lifetime checks, not a second simulation oracle. */
struct RoadAbiProbe {
	OpenTTDRoadState *state;
	OpenTTDRoadView view{};
	uint32_t leaves = 0;
};
static RoadAbiProbe *road_abi_probe;
static void OPENTTD_ROAD_CALL RoadAbiObserve(uint32_t, OpenTTDRoadView *view) noexcept { *view = road_abi_probe->view; }
static void OPENTTD_ROAD_CALL RoadAbiWrite(uint32_t, uint32_t field, uint64_t value) noexcept
{
	CHECK(field == ROAD_WRITE_TICK);
	road_abi_probe->view.tick = static_cast<uint8_t>(value);
}
static uint64_t OPENTTD_ROAD_CALL RoadAbiLeaf(uint32_t op, uint32_t, uint64_t, uint64_t, uint64_t) noexcept
{
	CHECK(op == ROAD_OP_ROADSTOP_LEAVE);
	++road_abi_probe->leaves;
	return 0;
}
static OpenTTDRoadState *OPENTTD_ROAD_CALL RoadAbiOwner(uint32_t) noexcept { return road_abi_probe->state; }
static size_t OPENTTD_ROAD_CALL RoadAbiNearby(uint32_t, uint32_t, int32_t, int32_t, uint32_t *, size_t) noexcept { return 0; }
static void RoadBoundary()
{
	RoadAbiProbe probe{openttd_rust_road_new()};
	road_abi_probe = &probe;
	probe.view.tick = 255;
	const OpenTTDRoadLeaves leaves{RoadAbiObserve, RoadAbiWrite, RoadAbiLeaf, RoadAbiOwner, RoadAbiNearby};
	/* All shared fields are typed functions, even unused fields. */
	EffectTestWorld world;
	effect_test_world = &world;
	const OpenTTDSharedServices services{&world, EffectTestRandom, EffectTestTile, EffectTestMapWrite, EffectTestTrig, EffectTestIndustry};
	auto create = [&](uint32_t kind) {
		return std::unique_ptr<void, decltype(&openttd_rust_road_task_destroy)>(openttd_rust_road_create(kind, 17, 0, 0, 0, &leaves, &services), openttd_rust_road_task_destroy);
	};
	auto tick = create(0);
	CHECK(openttd_rust_road_advance(tick.get(), 0).op == 0);
	CHECK(probe.view.tick == 0 && world.draws == 0);
	auto crash = create(3);
	CHECK(openttd_rust_road_advance(crash.get(), 0).op == ROAD_OP_GROUND_CRASH);
	/* An actual callback may mutate owner state before the continuation resumes. */
	probe.view.front = 1;
	openttd_rust_road_set(probe.state, 0, 64);
	auto done = openttd_rust_road_advance(crash.get(), 7);
	CHECK(done.op == 0 && done.a == 8 && probe.leaves == 1);
	CHECK(openttd_rust_road_get(probe.state, 5) == 1);
	crash.reset();
	tick.reset();
	openttd_rust_road_destroy(probe.state);
	std::printf("road_native_tables_owner_reentry_and_tick_wrap passed\n");
}

/* Moving and partly entered consists cannot use IsChainInDepot's service path,
 * but FindClosestRoadDepot still selects their current depot at distance zero. */
struct RoadServiceProbe {
	OpenTTDRoadState *head = openttd_rust_road_new();
	OpenTTDRoadState *tail = openttd_rust_road_new();
	OpenTTDRoadView view{};
	bool depot_tile = true;
	uint32_t depot_orders = 0;
	uint32_t services = 0;
};
static RoadServiceProbe *road_service_probe;
static void OPENTTD_ROAD_CALL RoadServiceObserve(uint32_t id, OpenTTDRoadView *view) noexcept
{
	*view = road_service_probe->view;
	view->next = id == 17 ? 18 : UINT32_MAX;
}
static void OPENTTD_ROAD_CALL RoadServiceWrite(uint32_t, uint32_t field, uint64_t) noexcept
{
	CHECK(field == ROAD_WRITE_DAY || field == ROAD_WRITE_SUPPRESS_IMPLICIT);
}
static uint64_t OPENTTD_ROAD_CALL RoadServiceLeaf(uint32_t op, uint32_t, uint64_t a, uint64_t, uint64_t) noexcept
{
	auto &probe = *road_service_probe;
	switch (op) {
		case ROAD_OP_SERVINT: case ROAD_OP_NEEDS_SERVICE: return 1;
		case ROAD_OP_IS_DEPOT: CHECK(a == probe.view.tile); return probe.depot_tile;
		case ROAD_OP_MAX_PENALTY: return 300;
		case ROAD_OP_DEPOT_INDEX: CHECK(a == probe.view.tile); return 55;
		case ROAD_OP_ORDER_DEPOT: CHECK(a == 55); ++probe.depot_orders; break;
		case ROAD_OP_SET_DEST: CHECK(a == probe.view.tile); probe.view.dest = static_cast<uint32_t>(a); break;
		case ROAD_OP_SERVICE: ++probe.services; break;
		case ROAD_OP_ECONOMY_AGE: case ROAD_OP_CHECK_BREAKDOWN:
		case ROAD_OP_CHECK_ORDERS: case ROAD_OP_START_STOP_DIRTY: break;
		default: CHECK(false);
	}
	return 0;
}
static OpenTTDRoadState *OPENTTD_ROAD_CALL RoadServiceOwner(uint32_t id) noexcept
{
	return id == 17 ? road_service_probe->head : road_service_probe->tail;
}
static void RoadServiceBoundary()
{
	RoadServiceProbe probe;
	road_service_probe = &probe;
	probe.view.front = 1;
	probe.view.first = 17;
	probe.view.tile = 3091;
	probe.view.speed = 5;
	const OpenTTDRoadLeaves leaves{RoadServiceObserve, RoadServiceWrite, RoadServiceLeaf, RoadServiceOwner, RoadAbiNearby};
	EffectTestWorld world;
	effect_test_world = &world;
	const OpenTTDSharedServices services{&world, EffectTestRandom, EffectTestTile, EffectTestMapWrite, EffectTestTrig, EffectTestIndustry};
	using Task = std::unique_ptr<void, decltype(&openttd_rust_road_task_destroy)>;
	auto day = [&]() { return Task(openttd_rust_road_create(6, 17, 0, 0, 0, &leaves, &services), openttd_rust_road_task_destroy); };
	for (bool moving : {true, false}) {
		probe.view.speed = moving ? 5 : 0;
		probe.view.dest = 42;
		openttd_rust_road_set(probe.head, 0, 254);
		openttd_rust_road_set(probe.tail, 0, moving ? 254 : 1);
		openttd_rust_road_path_push(probe.head, {10, 10815});
		auto run = day();
		CHECK(openttd_rust_road_advance(run.get(), 0).op == 0);
		CHECK(probe.view.dest == probe.view.tile && openttd_rust_road_path_size(probe.head) == 0);
	}
	CHECK(probe.depot_orders == 2 && probe.services == 0 && world.draws == 0);
	openttd_rust_road_set(probe.tail, 0, 254);
	auto parked = day();
	CHECK(openttd_rust_road_advance(parked.get(), 0).op == 0);
	CHECK(probe.services == 1 && probe.depot_orders == 2);
	probe.depot_tile = false;
	auto outside = day();
	const auto search = openttd_rust_road_advance(outside.get(), 0);
	CHECK(search.op == ROAD_OP_FIND_DEPOT && search.a == 300);
	outside.reset();
	parked.reset();
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
static void DisasterProbeTile(void *, uint32_t, uint32_t *) noexcept {}
static void DisasterProbeTileWrite(void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) noexcept {}
static float DisasterProbeTrig(uint32_t, float value) noexcept { return value; }
static int64_t OPENTTD_DISASTER_CALL DisasterProbeService(void *, const OpenTTDDisasterAction *) noexcept { return 0; }
static uint32_t DisasterProbeIndustry(int32_t, int32_t, uint32_t *) noexcept { return 0; }
static void Disasters()
{
	using Owner = std::unique_ptr<OpenTTDDisasterRun, decltype(&openttd_rust_disaster_destroy)>;
	using StateOwner = std::unique_ptr<OpenTTDDisasterState, decltype(&openttd_rust_disaster_state_destroy)>;
	DisasterProbe probe;
	const OpenTTDSharedServices services{&probe, DisasterProbeRandom, DisasterProbeTile, DisasterProbeTileWrite, DisasterProbeTrig, DisasterProbeIndustry};
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
	RoadBoundary();
	RoadServiceBoundary();
	WaterProbe::Run([](bool result) { CHECK(result); });
	ShipYapfProbe::Run([](bool result) { CHECK(result); });
	CheckEffectProtocol([](bool result) { CHECK(result); });
	LinkGraphJob();
	Trees();
	Disasters();
	AircraftOwnership();
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
