/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file string_consumer_bytes.cpp Concrete byte/cursor gaps beyond unchanged consumer tests. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../core/string_consumer.hpp"
#include "../safeguards.h"

TEST_CASE("StringConsumer - empty prefix and borrowed offsets")
{
	std::string_view data = "X\xFF\0Z"sv;
	StringConsumer consumer(data);
	consumer.Skip(1);
	CHECK(consumer.Peek(0).data() == data.data() + 1);
	CHECK(consumer.Read(0).data() == data.data() + 1);
	CHECK(consumer.PeekIf(""));
	CHECK(consumer.ReadIf(""));
	consumer.SkipIf("");
	CHECK(consumer.GetBytesRead() == 1);
	CHECK(consumer.Peek(SIZE_MAX - 1) == data.substr(1));
	consumer.SkipAll();
	CHECK(consumer.PeekIf(""));
	CHECK(consumer.ReadIf(""));
	CHECK(!consumer.ReadIf("Z"));
	CHECK(consumer.GetBytesRead() == data.size());
	CHECK(consumer.Peek(0).data() == data.data() + data.size());
	CHECK(consumer.Read(StringConsumer::npos).data() == data.data() + data.size());
	StringConsumer empty(std::string_view{});
	CHECK(empty.ReadIf(""));
	CHECK(empty.Peek(0).data() == nullptr);
	CHECK(empty.Read(0).data() == nullptr);
}

TEST_CASE("StringConsumer - partial binary TryRead preserves cursor")
{
	std::string_view data = "X\1\2\3"sv;
	StringConsumer consumer(data);
	consumer.Skip(1);
	CHECK(!consumer.TryReadUint32LE().has_value());
	CHECK(!consumer.TryReadSint32LE().has_value());
	CHECK(!consumer.TryReadUint64LE().has_value());
	CHECK(!consumer.TryReadSint64LE().has_value());
	CHECK(consumer.GetBytesRead() == 1);
	CHECK(consumer.TryReadUint16LE() == 0x0201);
	CHECK(consumer.GetBytesRead() == 3);
	CHECK(!consumer.TryReadUint16LE().has_value());
	CHECK(!consumer.TryReadSint16LE().has_value());
	CHECK(consumer.GetBytesRead() == 3);
	CHECK(consumer.ReadUint16LE(42) == 42);
	CHECK(consumer.GetBytesRead() == data.size());
	CHECK(!consumer.TryReadUint8().has_value());
	CHECK(!consumer.TryReadSint8().has_value());
	CHECK(!consumer.TryReadChar().has_value());
	CHECK(consumer.GetBytesRead() == data.size());
}

TEST_CASE("StringConsumer - whole multi-byte separator decisions")
{
	using Consumer = StringConsumer;
	std::string_view data = "xxabc<><>tail"sv;
	struct Case { Consumer::SeparatorUsage policy; std::string_view result; size_t consumed; };
	const Case cases[] = {
		{Consumer::READ_ALL_SEPARATORS, "abc<><>", 7},
		{Consumer::READ_ONE_SEPARATOR, "abc<>", 5},
		{Consumer::KEEP_SEPARATOR, "abc", 3},
		{Consumer::SKIP_ONE_SEPARATOR, "abc", 5},
		{Consumer::SKIP_ALL_SEPARATORS, "abc", 7},
		{static_cast<Consumer::SeparatorUsage>(7), "abc", 3}, // Unnamed value within the enum's representable range.
	};
	for (const auto &test : cases) {
		Consumer read(data), skip(data);
		read.Skip(2);
		skip.Skip(2);
		auto peeked = read.PeekUntil("<>", test.policy);
		CHECK(peeked == test.result);
		CHECK(peeked.data() == data.data() + 2);
		CHECK(read.GetBytesRead() == 2);
		auto result = read.ReadUntil("<>", test.policy);
		CHECK(result == test.result);
		CHECK(result.data() == data.data() + 2);
		CHECK(read.GetBytesRead() == 2 + test.consumed);
		skip.SkipUntil("<>", test.policy);
		CHECK(skip.GetBytesRead() == read.GetBytesRead());
	}
	Consumer overlapping("abababaZ"sv);
	CHECK(overlapping.ReadUntil("aba", Consumer::READ_ALL_SEPARATORS) == "aba");
	CHECK(overlapping.GetLeftData() == "babaZ");
	Consumer missing(data);
	missing.Skip(2);
	CHECK(missing.ReadUntil("missing", Consumer::SKIP_ALL_SEPARATORS) == data.substr(2));
	CHECK(missing.GetBytesRead() == data.size());
	CHECK(missing.ReadUntil("<>", Consumer::READ_ALL_SEPARATORS).data() == data.data() + data.size());
}

TEST_CASE("StringConsumer - byte sets and overlapping borrowed patterns")
{
	std::string_view data = "X\xFF\0ab"sv;
	StringConsumer consumer(data);
	consumer.Skip(1);
	CHECK(consumer.Find(data.substr(1, 2)) == 0); // Pattern overlaps the read-only input.
	CHECK(consumer.ReadIf(data.substr(1, 1)));
	CHECK(consumer.GetBytesRead() == 2);
	CHECK(!consumer.PeekCharIfNotIn("\0"sv).has_value());
	CHECK(!consumer.ReadCharIfNotIn("\0"sv).has_value());
	consumer.SkipCharIfNotIn("\0"sv);
	CHECK(consumer.GetBytesRead() == 2);
	CHECK(consumer.ReadCharIfIn("\0"sv) == '\0');
	CHECK(consumer.PeekCharIfNotIn("x") == 'a');
	CHECK(consumer.ReadCharIfNotIn("x") == 'a');
	consumer.SkipCharIfNotIn("x");
	CHECK(consumer.GetBytesRead() == data.size());
	CHECK(!consumer.ReadCharIfNotIn("x").has_value());
	CHECK(!consumer.ReadCharIfIn("x").has_value());
	CHECK(consumer.GetBytesRead() == data.size());
}
