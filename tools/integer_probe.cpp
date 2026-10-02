/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file integer_probe.cpp Narrow oracle probe; compile against unchanged reference or candidate headers/source. */

#include "stdafx.h"
#include "core/string_consumer.hpp"
#include <iostream>
#include <iomanip>

static StringConsumer *logging_consumer = nullptr;
static std::vector<std::pair<size_t, std::string>> messages;

static std::string Hex(std::string_view bytes)
{
	std::string result;
	for (unsigned char byte : bytes) {
		result += "0123456789abcdef"[byte >> 4];
		result += "0123456789abcdef"[byte & 15];
	}
	return result;
}

void DebugPrint(std::string_view, int, std::string &&message)
{
	messages.emplace_back(logging_consumer->GetBytesRead(), std::move(message));
}

#if defined(STRGEN) || defined(SETTINGSGEN)
[[noreturn]] void FatalErrorI(const std::string &message)
{
	std::cout << "fatal " << logging_consumer->GetBytesRead() << ' ' << Hex(message) << '\n';
	std::exit(2);
}
#endif

template <typename T>
static uint64_t Bits(T value)
{
	return static_cast<std::make_unsigned_t<T>>(value);
}

template <typename T>
static void Probe(std::string_view src, int base, bool clamp)
{
	StringConsumer peek(src), attempt(src), read(src), skip(src);
	auto [len, value] = peek.PeekIntegerBase<T>(base, clamp);
	auto tried = attempt.TryReadIntegerBase<T>(base, clamp);
	logging_consumer = &read;
	messages.clear();
	T result = read.ReadIntegerBase<T>(base, 42, clamp);
	skip.SkipIntegerBase(base);
	auto whole = ParseInteger<T>(src, base, clamp);
	std::cout << len << ' ' << Bits(value) << ' ' << peek.GetBytesRead() << ' '
		<< tried.has_value() << ' ' << Bits(tried.value_or(42)) << ' ' << attempt.GetBytesRead() << ' '
		<< Bits(result) << ' ' << read.GetBytesRead() << ' ' << skip.GetBytesRead() << ' '
		<< whole.has_value() << ' ' << Bits(whole.value_or(42)) << ' ' << messages.size();
	for (const auto &[position, message] : messages) std::cout << ' ' << position << ' ' << Hex(message);
	std::cout << '\n';
}

int main()
{
	unsigned width, is_signed, base, clamp;
	std::string hex;
	while (std::cin >> width >> is_signed >> base >> clamp >> hex) {
		std::string input;
		if (hex != "-") {
			for (size_t i = 0; i < hex.size(); i += 2) input += static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16));
		}
		std::string_view src = input.empty() ? std::string_view{} : std::string_view(input);
		if (is_signed) {
			switch (width) {
				case 8: Probe<int8_t>(src, base, clamp); break;
				case 16: Probe<int16_t>(src, base, clamp); break;
				case 32: Probe<int32_t>(src, base, clamp); break;
				case 64: Probe<int64_t>(src, base, clamp); break;
			}
		} else {
			switch (width) {
				case 8: Probe<uint8_t>(src, base, clamp); break;
				case 16: Probe<uint16_t>(src, base, clamp); break;
				case 32: Probe<uint32_t>(src, base, clamp); break;
				case 64: Probe<uint64_t>(src, base, clamp); break;
			}
		}
	}
}
