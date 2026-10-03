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

int main()
{
	Layouts();
	Calls();
	Encoded();
	History();
	Locale();
	std::printf("ABI audit passed\n");
}
