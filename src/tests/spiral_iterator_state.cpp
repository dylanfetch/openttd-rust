/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file spiral_iterator_state.cpp Audited rectangular-map and value-state gaps. */

#include "stdafx.h"
#include "3rdparty/catch2/catch.hpp"
#include "tilearea_type.h"
#include "map_func.h"

#include "safeguards.h"

struct SpiralCoord {
	uint x, y;
};

static void CheckSpiral(SpiralTileSequence sequence, std::span<const SpiralCoord> expected)
{
	std::vector<TileIndex> actual;
	for (auto tile : sequence) actual.push_back(tile);
	REQUIRE(actual.size() == expected.size());
	for (size_t i = 0; i < actual.size(); ++i) {
		CHECK(TileX(actual[i]) == expected[i].x);
		CHECK(TileY(actual[i]) == expected[i].y);
	}
}

TEST_CASE("SpiralTileSequence - rectangular clipped edges")
{
	/* Complete ordered fixtures captured from the pinned, unchanged C++ iterator. */
	Map::Allocate(128, 64);
	const SpiralCoord wide[] = {
		{126, 1}, {127, 0}, {126, 0}, {125, 0}, {125, 1}, {125, 2}, {126, 2}, {127, 2},
		{127, 1}, {124, 0}, {124, 1}, {124, 2}, {124, 3}, {125, 3}, {126, 3}, {127, 3},
	};
	CheckSpiral(SpiralTileSequence(TileXY(126, 1), 5), wide);
	const SpiralCoord hole[] = {
		{127, 62}, {126, 62}, {126, 63}, {127, 63}, {127, 61}, {126, 61}, {125, 61}, {125, 62}, {125, 63},
	};
	CheckSpiral(SpiralTileSequence(TileXY(126, 62), 2, 1, 0), hole);
	Map::Allocate(64, 128);
	const SpiralCoord tall[] = {
		{2, 126}, {1, 126}, {1, 127}, {2, 127}, {3, 125}, {2, 125}, {1, 125}, {0, 125},
		{0, 126}, {0, 127}, {3, 127}, {3, 126},
	};
	CheckSpiral(SpiralTileSequence(TileXY(1, 126), 4), tall);
}

TEST_CASE("SpiralTileIterator - live map dimensions")
{
	Map::Allocate(64, 64);
	auto iterator = SpiralTileSequence(TileXY(63, 1), 3).begin();
	Map::Allocate(128, 64);
	++iterator;
	CHECK(TileX(*iterator) == 64);
	CHECK(TileY(*iterator) == 0);
	Map::Allocate(64, 64);
	++iterator;
	CHECK(TileX(*iterator) == 63);
	CHECK(TileY(*iterator) == 0);
}

TEST_CASE("SpiralTileIterator - copies postfix and coordinate equality")
{
	Map::Allocate(64, 128);
	const auto sequence = SpiralTileSequence(TileXY(4, 70), 3);
	auto first = sequence.begin();
	auto copy = first;
	CHECK(first == copy);
	/* Radius limits differ, but the public comparison is coordinates only. */
	CHECK(first == SpiralTileSequence(TileXY(4, 70), 5).begin());
	CHECK(first == SpiralTileSequence(TileXY(4, 70), 1).begin());
	auto previous = first++;
	CHECK(previous == copy);
	CHECK(previous != first);
	CHECK(*previous == TileXY(4, 70));
	CHECK(*first == TileXY(5, 69));
	CHECK(&++copy == &copy);
	CHECK(copy == first);
	CHECK(*previous == TileXY(4, 70));
	auto ended = sequence.begin();
	uint count = 0;
	while (ended != sequence.end()) {
		++count;
		++ended;
	}
	CHECK(count == 9);
	CHECK(ended == std::default_sentinel);
	auto end_copy = ended;
	CHECK(end_copy == ended);
	CHECK(end_copy == std::default_sentinel);
	/* The original terminal coordinate is (6,68), distinct from end-state. */
	auto same_coordinate = SpiralTileSequence(TileXY(6, 68), 1).begin();
	CHECK(ended == same_coordinate);
	CHECK_FALSE(same_coordinate == std::default_sentinel);
}

TEST_CASE("SpiralTileIterator - wrapping hole initialization")
{
	Map::Allocate(64, 64);
	/* x + UINT32_MAX + 1 and the initial movement count wrap; no new extent cap. */
	auto iterator = SpiralTileSequence(TileXY(0, 0), 1, UINT32_MAX, 0).begin();
	CHECK(*iterator == TileXY(0, 0));
	CHECK_FALSE(iterator == std::default_sentinel);
	CHECK(iterator == SpiralTileSequence(TileXY(0, 0), 1).begin());
	/* Do not traverse this enormous inherited perimeter just to test its constructor. */
}
