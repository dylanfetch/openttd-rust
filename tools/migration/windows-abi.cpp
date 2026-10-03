/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file windows-abi.cpp Bounded first-32-bit layout and real C ABI call checks. */
#include "rust/abi_ffi.h"
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
#include <type_traits>
#include "misc/history_type.hpp"

#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (0)

static void Layout(uint8_t type, const char *name, std::initializer_list<size_t> values)
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
	Layout(25, "OpenTTDX25519Leaves", {sizeof(OpenTTDX25519Leaves), alignof(OpenTTDX25519Leaves), offsetof(OpenTTDX25519Leaves, wipe), offsetof(OpenTTDX25519Leaves, verify32)});
	CHECK(openttd_rust_abi_layout(255, 0) == SIZE_MAX);
	CHECK(static_cast<size_t>(PTRDIFF_MAX) == (SIZE_MAX >> 1));
	std::printf("pointer_bytes %zu sentinel %zu borrow_limit %zu\n", sizeof(void *), SIZE_MAX, static_cast<size_t>(PTRDIFF_MAX));
}

static void Calls()
{
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
	std::printf("x25519 four exports, cdecl wipe/verify32, physical wipe sizes/order counts, forward trim and 255/256 high-bit distinction passed\n");
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

int main()
{
	Layouts();
	Calls();
	Encoded();
	History();
	Math();
	CryptoPrimitives();
	Blake2b();
	X25519();
	StationCargo();
	Locale();
	std::printf("ABI audit passed\n");
}
