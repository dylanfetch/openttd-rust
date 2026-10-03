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
#include <cstdlib>
#include <stdexcept>

// The bounded consumer inputs never trigger error logging. Abort if that changes,
// so this standalone adapter needs neither the game logger nor its runtime state.
void DebugPrint(std::string_view, int, std::string &&) { std::abort(); }

/* The actual unchanged source exposes its local shared loop to this bounded
 * fixture, allowing observation of consumer-before-output and typed failures. */
static bool fail_cpp_allocation;
void *operator new(size_t size)
{
	if (fail_cpp_allocation) throw std::bad_alloc{};
	if (void *result = std::malloc(size == 0 ? 1 : size)) return result;
	throw std::bad_alloc{};
}
void operator delete(void *pointer) noexcept { std::free(pointer); }
void operator delete(void *pointer, size_t) noexcept { std::free(pointer); }
[[noreturn]] void NOT_REACHED(const std::source_location) { throw std::runtime_error("NOT_REACHED"); }
#include "string.cpp"

static void Hex(std::string_view bytes)
{
	static constexpr char alphabet[] = "0123456789abcdef";
	for (uint8_t byte : bytes) std::cout << alphabet[byte >> 4] << alphabet[byte & 15];
	std::cout << '/';
}
static void Pair(const char *name, InPlaceReplacement &pair, std::span<const char> buffer)
{
	std::cout << name << ' ' << pair.consumer.GetBytesRead() << ' ' << pair.builder.GetBytesWritten()
			  << ' ' << pair.builder.GetBytesUnused() << ' ' << pair.builder.AnyBytesUnused() << ' ';
	Hex({buffer.data(), buffer.size()}); std::cout << '\n';
}
struct ObservedBuilder : BaseStringBuilder {
	StringConsumer &consumer;
	std::string output;
	std::array<size_t, 100> progress{};
	size_t calls = 0;
	bool reenter = false;
	explicit ObservedBuilder(StringConsumer &consumer) : consumer(consumer) {}
	void PutBuffer(std::span<const char> bytes) override
	{
		progress[calls++] = this->consumer.GetBytesRead();
		if (reenter) { reenter = false; auto nested = StrMakeValid("inner"sv); if (nested != "inner") std::abort(); this->consumer.Skip(1); }
		output.append(bytes.data(), bytes.size());
	}
};
static void Validation()
{
	using namespace std::literals::string_literals;
	std::vector<std::string> corpus{{}, "plain", "\r\n\r\t\n", "\0tail"s, "a\0bad\xFF"s,
		"\x80\xBF\xC0\x80"s, "\xE0\x80\x80"s, "\xF0\x80\x80\x80"s,
		"a\xC2"s, "\xE1\x80"s, "\xF1\x80\x80"s, "\xF4\x90\x80\x80"s,
		"x\xFF\x80y"s, "\xED\xA0\x80\xED\xBF\xBF"s};
	const char32_t boundaries[]{0,1,9,10,13,0x1E,0x1F,0x20,0x7F,0x80,0xD7FF,0xD800,0xDFFF,0xE000,0xE001,0xE002,0xE003,0xE004,0xE1FF,0xE200,0xE2FF,0xE300,0x10FFFF};
	std::string mixed = "a\r\n\r\t\n";
	for (char32_t codepoint : boundaries) {
		auto [bytes, length] = EncodeUtf8(codepoint); corpus.emplace_back(bytes, length);
		if (codepoint != 0) mixed.append(bytes, length);
	}
	corpus.push_back(mixed);
	for (uint8_t flags = 0; flags < 16; ++flags) for (uint8_t unknown : {uint8_t{0}, uint8_t{0xF0}}) {
		StringValidationSettings settings(static_cast<uint8_t>(flags | unknown));
		for (size_t index = 0; index < corpus.size(); ++index) {
			auto input = corpus[index]; auto output = StrMakeValid(std::string_view(input), settings);
			auto inplace = input; StrMakeValidInPlace(inplace, settings);
			std::vector<char> cstring(input.begin(), input.end()); cstring.push_back('\0'); cstring.push_back('T'); cstring.push_back('!');
			StrMakeValidInPlace(cstring.data(), settings);
			std::cout << "validation " << unsigned(flags | unknown) << ' ' << index << ' '; Hex(output); Hex(inplace); Hex({cstring.data(),cstring.size()}); std::cout << '\n';
		}
	}
	for (size_t index = 0; index < corpus.size(); ++index) {
		auto terminated = corpus[index]; terminated.push_back('\0'); terminated.append("\xFFtail", 5);
		std::cout << "valid " << index << ' ' << StrValid(std::span(corpus[index].data(), corpus[index].size()))
				  << ' ' << StrValid(std::span(terminated.data(), terminated.size())) << '\n';
	}
	for (bool fail : {false,true}) for (bool reenter : {false,true}) {
		std::string input(40, 'A'); StringConsumer consumer(input); ObservedBuilder builder(consumer); builder.reenter = reenter;
		bool threw = false; fail_cpp_allocation = fail;
		try { StrMakeValid(builder, consumer, StringValidationSetting::ReplaceWithQuestionMark); } catch (const std::bad_alloc &) { threw = true; }
		fail_cpp_allocation = false;
		std::cout << "append-progress " << fail << ' ' << reenter << ' ' << threw << ' ' << consumer.GetBytesRead() << ' ' << builder.output.size() << ' ' << builder.calls;
		for (size_t call = 0; call < builder.calls; ++call) std::cout << ' ' << builder.progress[call];
		std::cout << '\n';
	}
	{
		std::array<char,8> buffer{'a','b','c','d','e','f','g','h'}; InPlaceReplacement first(buffer);
		first.consumer.Skip(4); first.builder.Put("12"sv); InPlaceReplacement copy(first);
		first.consumer.Skip(2); first.builder.PutChar('A'); copy.builder.PutChar('B'); copy.consumer.Skip(1); copy.builder.PutChar('C');
		Pair("pair-first",first,buffer); Pair("pair-copy",copy,buffer);
		std::array<char,8> other{}; InPlaceReplacement assigned(other); assigned = first;
		assigned.consumer.Skip(1); assigned.builder.PutChar('D'); first = first;
		Pair("pair-assigned",assigned,buffer); Pair("pair-self",first,buffer); Pair("pair-copy-after",copy,buffer);
		InPlaceReplacement rvalue(std::move(copy)); rvalue.consumer.Skip(1); rvalue.builder.PutChar('E');
		Pair("pair-rvalue",rvalue,buffer); Pair("pair-rvalue-source",copy,buffer);
	}
	{
		std::array<char,8> buffer{'a','b','c','d','e','f','g','h'}; InPlaceReplacement pair(buffer);
		pair.consumer.Skip(6); pair.builder.PutBuffer(std::span(buffer).subspan(2,4)); Pair("left-overlap",pair,buffer);
		/* Reassigning the public consumer exposes original native subtraction wrap. */
		pair.consumer = StringConsumer(std::span<const char>(buffer)); pair.consumer.Skip(1); pair.builder.PutChar('Z'); Pair("live-wrap-capacity",pair,buffer);
	}
	{
		std::array<char,4> buffer{'a','b','c','d'}; InPlaceReplacement pair(buffer); bool threw = false;
		try { pair.builder.PutChar('X'); } catch (const std::runtime_error &) { threw = true; }
		std::cout << "overtake " << threw << '\n'; Pair("overtake-state",pair,buffer);
		pair.builder.PutBuffer({}); Pair("zero-write",pair,buffer);
	}
}

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
	Validation();
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
