/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file townname.cpp Focused built-in rendering and builder append tests. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../strings_func.h"
#include "../strings_internal.h"
#include "../safeguards.h"

TEST_CASE("Town names - dispatch, prefixes, repeated appends and company formatting")
{
	/* Fixtures from the unchanged pinned generators: each ID at both u32 endpoints. */
	static constexpr std::string_view expected[][2] = {
		{"Invenville", "Fort Wenfingburg Springs"},
		{"Agincourt", "Alen\u00e7on"},
		{"Berlin", "W\u00fcrzwald"},
		{"Stanningville", "Old Stanthwaite Springs"},
		{"Caracas", "Santa Rosa"},
		{"Binkyton", "Griddlebridge"},
		{"Backarp", "Stora \u00d6ster\u00e5s"},
		{"Drogestad", "Klein Hilend"},
		{"Hiekkaharju", "Lieksa"},
		{"Frombork", "Opoczno"},
		{"Bratislava", "Zvolen"},
		{"Alta", "V\u00e5g\u00e5sen"},
		{"Ajka", "V\u00e1s\u00e1rosv\u00f6lgy"},
		{"Bruck", "Maria Weissenhaag ob der Ill"},
		{"Adjud", "Zal\u0103u"},
		{"A\u0161", "Lipi\u0161t\u011b"},
		{"Aarau", "Zug"},
		{"Agerbasse", "Kongens \u00c5lskov"},
		{"Ak\u00e7aaga\u00e7dere", "Yeniayva"},
		{"Roma", "Roccaforte"},
		{"Barcelona", "Vilafranca del Pened\u00e8s"},
	};
	/* Existing bytes include curse/vowel text, UTF-8 and NUL: output starts after them. */
	const std::string prefix("Cunt aou \u00e4\0", 12);
	for (size_t lang = 0; lang < std::size(expected); lang++) {
		for (size_t endpoint = 0; endpoint < 2; endpoint++) {
			const uint32_t seed = endpoint == 0 ? 0 : UINT32_MAX;
			std::string result = prefix;
			StringBuilder builder(result);
			GenerateTownNameString(builder, lang, seed);
			GenerateTownNameString(builder, lang, seed);
			CHECK(result == prefix + std::string(expected[lang][endpoint]) + std::string(expected[lang][endpoint]));
			result = prefix;
			auto params = MakeParameters(seed);
			GetStringWithArgs(builder, SPECSTR_COMPANY_NAME_START + static_cast<StringID>(lang), params);
			CHECK(result == prefix + std::string(expected[lang][endpoint]) + " Transport");
		}
	}
	/* Finnish scans only the newly appended stem; a prefix vowel must not force la. */
	std::string result = prefix;
	StringBuilder builder(result);
	GenerateTownNameString(builder, 8, 3797316664U);
	CHECK(result == prefix + "Hein\u00e4l\u00e4");
}
