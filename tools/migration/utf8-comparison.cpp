/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file utf8-comparison.cpp Bounded corpus run against original and candidate sources. */
#include "stdafx.h"
#include "core/utf8.hpp"
#include "core/string_consumer.hpp"
#include <iostream>

// The bounded consumer inputs never trigger error logging. Abort if that changes,
// so this standalone adapter needs neither the game logger nor its runtime state.
void DebugPrint(std::string_view, int, std::string &&) { std::abort(); }

static void Decode(std::string_view bytes)
{
	auto [length, codepoint] = DecodeUtf8(bytes);
	std::cout << "decode " << bytes.size() << ':';
	for (unsigned char byte : bytes) std::cout << ' ' << unsigned(byte);
	std::cout << " -> " << length << ' ' << uint32_t(codepoint) << '\n';
}

static void Iterate(std::string_view bytes)
{
	Utf8View view(bytes);
	std::cout << "view " << bytes.size() << ':';
	for (unsigned char byte : bytes) std::cout << ' ' << unsigned(byte);
	std::cout << " forward";
	for (auto it = view.begin(); it != view.end(); it++) {
		std::cout << ' ' << it.GetByteOffset() << ':' << uint32_t(*it);
	}
	std::cout << " end=" << view.end().GetByteOffset() << " backward";
	auto it = view.end();
	while (it != view.begin()) {
		it--;
		std::cout << ' ' << it.GetByteOffset() << ':' << uint32_t(*it);
	}
	std::cout << " normalize";
	for (size_t offset = 0; offset <= bytes.size(); ++offset) {
		std::cout << ' ' << offset << ':' << view.GetIterAtByte(offset).GetByteOffset();
	}
	// Exercise the backward algorithm from every valid position, including inside
	// malformed leading runs which forward iteration can skip.
	std::cout << " next";
	for (size_t offset = 0; offset < bytes.size(); ++offset) {
		Utf8View::iterator interior(bytes, offset);
		++interior;
		std::cout << ' ' << offset << ':' << interior.GetByteOffset();
	}
	std::cout << " previous";
	for (size_t offset = 1; offset <= bytes.size(); ++offset) {
		Utf8View::iterator interior(bytes, offset);
		--interior;
		std::cout << ' ' << offset << ':' << interior.GetByteOffset();
	}
#ifdef NDEBUG
	// The original explicitly returns end for offset >= size when asserts are off.
	std::cout << " beyond=" << view.GetIterAtByte(bytes.size() + 1).GetByteOffset()
			  << ',' << view.GetIterAtByte(SIZE_MAX).GetByteOffset();
#endif
	std::cout << '\n';
}

int main()
{
	const uint32_t boundaries[] = {
		0, 1, 0x7e, 0x7f, 0x80, 0x81, 0x7fe, 0x7ff, 0x800, 0x801,
		0xd7ff, 0xd800, 0xd801, 0xdffe, 0xdfff, 0xe000,
		0xfffe, 0xffff, 0x10000, 0x10001, 0x10fffe, 0x10ffff, 0x110000,
		0x110001, UINT32_MAX,
	};
	for (uint32_t codepoint : boundaries) {
		auto encoded = EncodeUtf8(static_cast<char32_t>(codepoint));
		std::cout << "encode " << codepoint << " -> " << encoded.second;
		for (unsigned char byte : encoded.first) std::cout << ' ' << unsigned(byte);
		std::cout << '\n';
		Decode({encoded.first, encoded.second});
		// Include every truncation and invalid continuation position for a valid prefix.
		for (size_t length = 0; length < encoded.second; ++length) Decode({encoded.first, length});
		for (size_t position = 1; position < encoded.second; ++position) {
			for (unsigned char invalid : {0x41, 0xc0, 0xff}) {
				std::string corrupted(encoded.first, encoded.second);
				corrupted[position] = static_cast<char>(invalid);
				Decode(corrupted);
			}
		}
		std::string trailing(encoded.first, encoded.second);
		trailing.append("\xFF\x80", 2);
		Decode(trailing);
	}
	for (unsigned int byte = 0; byte < 256; ++byte) {
		std::cout << "part " << byte << " -> " << IsUtf8Part(static_cast<char>(byte)) << '\n';
	}
	const std::string_view malformed[] = {
		{}, "\0"sv, "\x80"sv, "\xBF"sv, "\xC0"sv, "\xC1"sv,
		"\xC0\x80"sv, "\xC1\xBF"sv, "\xE0\x80\x80"sv,
		"\xE0\x9F\xBF"sv, "\xF0\x80\x80\x80"sv, "\xF0\x8F\xBF\xBF"sv,
		"\xC2"sv, "\xE1"sv, "\xE1\x80"sv, "\xF1"sv, "\xF1\x80"sv,
		"\xF1\x80\x80"sv, "\xF4\x8F\xBF\xBF"sv,
		"\xF4\x90\x80\x80"sv, "\xF5\x80\x80\x80"sv,
		"\xF8\x80\x80\x80"sv, "\xFF\x80"sv,
		"\xED\xA0\x80"sv, "\xED\xBF\xBF"sv, "a\xFF\x80"sv,
		"\xC2\x80\xFF"sv, "\xE0\xA0\x80\xFF"sv, "\xF0\x90\x80\x80\xFF"sv,
	};
	for (auto bytes : malformed) Decode(bytes);
	const std::string_view views[] = {
		{}, "\0"sv, "\x80\xBF\x80"sv, "\x80\x80" "a"sv,
		"a\x80\x80"sv, "\xFF\x80\x80" "a\x80\xF0"sv,
		"\xC0\x80" "b\xC1\xBF"sv, "\xE0\x80\x80\0\x80"sv,
		"\x80\xFF\x80\xE1" "a\xBF"sv,
		"\u1234\x80\x80" "a\xFF\x80\x80" "b\xF0"sv,
		"\u1234a\0b\U00012345"sv,
	};
	for (auto bytes : views) Iterate(bytes);

	// Keep the actual unchanged consumer methods, demonstrating their deliberate
	// one-byte advance on failure versus the view's continuation-run skipping.
	const auto binary = "\xFF\x80\x80" "a"sv;
	StringConsumer read(binary), skip(binary), try_read(binary);
	auto failed = try_read.TryReadUtf8();
	std::cout << "consumer try=" << failed.has_value() << ':' << try_read.GetBytesRead();
	auto value = read.ReadUtf8();
	std::cout << " read=" << uint32_t(value) << ':' << read.GetBytesRead();
	skip.SkipUtf8();
	std::cout << " skip=" << skip.GetBytesRead();
	auto iter = Utf8View(binary).begin();
	++iter;
	std::cout << " view=" << iter.GetByteOffset() << '\n';
	if (failed || try_read.GetBytesRead() != 0 || value != '?' ||
			read.GetBytesRead() != 1 || skip.GetBytesRead() != 1 || iter.GetByteOffset() != 3) return 1;
}
