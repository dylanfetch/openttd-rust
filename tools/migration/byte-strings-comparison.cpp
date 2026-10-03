/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file byte-strings-comparison.cpp Bounded public-API byte/aliasing gap records. */
#include "stdafx.h"
#include "string_func.h"
#include "core/string_consumer.hpp"
#include <clocale>
#include <iostream>
#include <sstream>
#include <sys/mman.h>

static std::string FromHex(std::string_view value)
{
	if (value == "-") return {};
	std::string result;
	for (size_t i = 0; i < value.size(); i += 2) {
		result.push_back(static_cast<char>(std::stoul(std::string(value.substr(i, 2)), nullptr, 16)));
	}
	return result;
}

static std::string ToHex(std::string_view value)
{
	static constexpr char digits[] = "0123456789ABCDEF";
	std::string result;
	for (unsigned char byte : value) {
		result.push_back(digits[byte >> 4]);
		result.push_back(digits[byte & 15]);
	}
	return result.empty() ? "-" : result;
}

static void WideLengths()
{
	if constexpr (sizeof(size_t) > sizeof(int)) {
		size_t length = static_cast<size_t>(INT_MAX) + 17;
		/* Real anonymous readable zero-filled storage: no invalid synthetic view.
		 * Empty/minimal comparisons do not fault in the whole virtual mapping. */
		void *allocation = mmap(nullptr, length, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (allocation != MAP_FAILED) {
			std::string_view view(static_cast<const char *>(allocation), length);
			std::string_view empty;
			std::string_view one = view.substr(0, 1);
			std::cout << "wide " << StrCompareIgnoreCase(view, empty) << ' '
				<< StrCompareIgnoreCase(empty, view) << ' '
				<< StrCompareIgnoreCase(view, one) << ' '
				<< StrCompareIgnoreCase(one, view) << ' '
				<< StrCompareIgnoreCase(empty, empty) << '\n';
			munmap(allocation, length);
			return;
		}
	}
	std::cout << "wide unavailable\n";
}

int main(int argc, char **argv)
{
	if (argc != 2 || std::setlocale(LC_ALL, argv[1]) == nullptr) return 2;
	std::cout << "locale " << std::setlocale(LC_ALL, nullptr) << " signed " << (CHAR_MIN < 0) << '\n';
	WideLengths();
	std::string record;
	while (std::getline(std::cin, record)) {
		std::istringstream input(record);
		std::string operation, first, second;
		input >> operation >> first;
		std::string left = FromHex(first);
		if (operation == "pair") {
			input >> second;
			std::string right = FromHex(second);
			/* Bounded views surrounded by bytes prevent incidental C-string use. */
			left = "!" + left + "?";
			right = "?" + right + "!";
			std::string_view a(left.data() + 1, left.size() - 2), b(right.data() + 1, right.size() - 2);
			std::cout << StrCompareIgnoreCase(a, b) << ' ' << StrEqualsIgnoreCase(a, b)
				<< ' ' << StrStartsWithIgnoreCase(a, b) << ' ' << StrEndsWithIgnoreCase(a, b)
				<< ' ' << StrContainsIgnoreCase(a, b) << ' ' << StrNaturalContainsIgnoreCase(a, b) << '\n';
		} else if (operation == "lower") {
			size_t offset;
			input >> offset;
			bool changed = strtolower(left, offset);
			std::cout << changed << ' ' << ToHex(left) << '\n';
		} else if (operation == "encode") {
			auto data = std::span(reinterpret_cast<const uint8_t *>(left.data()), left.size());
			std::cout << ToHex(FormatArrayAsHex(data)) << '\n';
		} else if (operation == "decode") {
			size_t size;
			input >> size;
			std::vector<uint8_t> backing(size + 2, 0xA5);
			bool result = ConvertHexToBytes(left, std::span(backing.data() + 1, size));
			std::cout << result << ' ' << ToHex(std::string_view(reinterpret_cast<const char *>(backing.data()), backing.size())) << '\n';
		} else if (operation == "overlap") {
			size_t source_offset, source_length, output_offset, output_length;
			input >> source_offset >> source_length >> output_offset >> output_length;
			/* Caller-valid overlapping views over one live initialized allocation. */
			bool result = ConvertHexToBytes(std::string_view(left).substr(source_offset, source_length),
				std::span(reinterpret_cast<uint8_t *>(left.data()) + output_offset, output_length));
			std::cout << result << ' ' << ToHex(left) << '\n';
		} else if (operation == "trim") {
			input >> second;
			std::string set = FromHex(second);
			std::string_view view = first == "-" ? std::string_view{} : std::string_view(left);
			auto result = StrTrimView(view, set);
			std::cout << (result.data() == nullptr) << ' ';
			if (result.data() == nullptr) std::cout << '-';
			else std::cout << (result.data() - view.data());
			std::cout << ' ' << result.size() << ' ' << ToHex(result) << '\n';
		} else if (operation == "inplace") {
			StrTrimInPlace(left);
			std::cout << ToHex(left) << '\n';
		} else if (operation == "null") {
			std::string_view view;
			std::span<uint8_t> bytes;
			auto result = StrTrimView(view, {});
			std::cout << StrCompareIgnoreCase(view, view) << ' ' << StrEqualsIgnoreCase(view, view)
				<< ' ' << StrStartsWithIgnoreCase(view, view) << ' ' << StrEndsWithIgnoreCase(view, view)
				<< ' ' << StrContainsIgnoreCase(view, view) << ' ' << ConvertHexToBytes(view, bytes)
				<< ' ' << (result.data() == nullptr) << '\n';
		} else {
			return 3;
		}
	}
}
