/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file encoded-comparison.cpp Scoped oracle fixture using existing public EncodedString APIs. */
#include "stdafx.h"
#include "strings_func.h"
#include "misc/endian_buffer.hpp"
#ifdef WITH_RUST
#include "rust/encoded_adapter.hpp"
#endif
#include <iostream>
#include <sstream>
#include <utility>

extern void FixSCCEncoded(std::string &str, bool fix_code);
extern void FixSCCEncodedNegative(std::string &str);
static bool throw_on_log = false;
static bool fail_next_allocation = false;

void *operator new(size_t size)
{
	if (std::exchange(fail_next_allocation, false)) throw std::bad_alloc();
	if (void *result = std::malloc(size == 0 ? 1 : size)) return result;
	throw std::bad_alloc();
}
void operator delete(void *pointer) noexcept { std::free(pointer); }
void operator delete(void *pointer, size_t) noexcept { std::free(pointer); }

static std::string Hex(std::string_view value)
{
	std::string result;
	for (unsigned char byte : value) {
		result += "0123456789abcdef"[byte >> 4];
		result += "0123456789abcdef"[byte & 15];
	}
	return result.empty() ? "-" : result;
}

static std::string Unhex(std::string_view value)
{
	std::string result;
	if (value == "-") return result;
	for (size_t i = 0; i < value.size(); i += 2) result += static_cast<char>(std::stoi(std::string(value.substr(i, 2)), nullptr, 16));
	return result;
}

void DebugPrint(std::string_view, int, std::string &&message)
{
	std::cout << "log " << Hex(message) << '\n' << std::flush;
	if (throw_on_log) throw std::runtime_error("injected logger exception");
}

[[noreturn]] void AssertFailedError(std::string_view expression, const std::source_location)
{
	std::cout << "assert " << Hex(expression) << '\n' << std::flush;
	std::exit(3);
}

static void Dump(std::string_view value)
{
	std::cout << "out " << Hex(value) << '\n';
}

static void Dump(const EncodedString &value)
{
	auto bytes = EndianBufferWriter<>::FromValue(value);
	bytes.pop_back(); // Remove only the writer's final terminator; preserve interior NUL.
	Dump(std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
}

static StringParameter Parameter(std::string_view token)
{
	if (token == "E") return std::monostate{};
	if (token.front() == 'N') return std::stoull(std::string(token.substr(1)), nullptr, 16);
	if (token.front() == 'D') return std::stoll(std::string(token.substr(1)), nullptr, 10);
	return Unhex(token.substr(1));
}

static void Execute(std::string_view command)
{
	std::istringstream stream{std::string(command)};
	std::string mode, bytes;
	stream >> mode;
	if (mode == "legacy" || mode == "negative") {
		unsigned fix_code = 0;
		if (mode == "legacy") stream >> fix_code;
		stream >> bytes;
		std::string input = Unhex(bytes);
		try {
			if (mode == "legacy") FixSCCEncoded(input, fix_code != 0);
			else FixSCCEncodedNegative(input);
			Dump(input);
		} catch (const std::runtime_error &) {
			std::cout << "exception " << Hex(input) << '\n'; // Original input remains uncommitted.
		}
		return;
	}
	if (mode == "replace") {
		size_t index;
		std::string token;
		stream >> index >> token >> bytes;
		std::string input = Unhex(bytes);
		std::vector<uint8_t> buffer(input.begin(), input.end());
		buffer.push_back(0);
		EncodedString encoded = EndianBufferReader::ToValue<EncodedString>(buffer);
		try {
			Dump(encoded.ReplaceParam(index, Parameter(token)));
		} catch (const std::runtime_error &) {
			std::cout << "exception-input "; Dump(encoded);
		}
		return;
	}
	if (mode == "default") {
		size_t index;
		std::string token;
		stream >> index >> token;
		Dump(EncodedString{}.ReplaceParam(index, Parameter(token)));
		return;
	}
	uint32_t id;
	stream >> id;
	size_t index = 0;
	std::string replacement;
	if (mode == "public-replace") stream >> index >> replacement;
	std::vector<StringParameter> parameters;
	std::string token;
	while (stream >> token) parameters.push_back(Parameter(token));
	auto encoded = GetEncodedStringWithArgs(id, parameters);
	if (mode == "clear") { encoded.clear(); Dump(encoded.ReplaceParam(0, 42)); }
	else if (mode == "public-replace") Dump(encoded.ReplaceParam(index, Parameter(replacement)));
	else Dump(encoded);
}

int main(int argc, char **argv)
{
#ifdef WITH_RUST
	if (argc == 2 && std::string_view(argv[1]) == "--copy-throw") {
		std::string input = "\uE000" + std::string(200, 'x');
		try {
			auto owner = OwnRustEncoded(openttd_rust_encoded_legacy(reinterpret_cast<const uint8_t *>(input.data()), input.size(), 0));
			fail_next_allocation = true;
			auto output = CopyRustEncoded(owner, input);
			return 1; // A successful copy would miss the intended exceptional path.
		} catch (const std::bad_alloc &) {
			std::cout << "copy-exception " << Hex(input) << '\n';
			return 0; // LeakSanitizer checks the destroyed Rust owner on process exit.
		}
	}
#endif
	throw_on_log = argc == 2 && std::string_view(argv[1]) == "--throw";
	std::string line;
	while (std::getline(std::cin, line)) Execute(line);
}
