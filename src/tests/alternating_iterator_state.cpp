/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file alternating_iterator_state.cpp Public state/lifetime gaps beyond the original fixed sequences. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../misc/alternating_iterator.hpp"
#include <list>
#include "../safeguards.h"

TEST_CASE("AlternatingIterator empty and singleton")
{
	std::vector<int> empty;
	AlternatingView empty_view(empty, empty.begin());
	CHECK(empty_view.begin() == empty_view.end());
	CHECK(empty_view.begin().Base() == empty.begin());
	CHECK(empty_view.end().Base() == empty.end());

	std::vector<int> singleton = {7};
	AlternatingView singleton_view(singleton, singleton.begin());
	auto iterator = singleton_view.begin();
	CHECK(*iterator == 7);
	CHECK(iterator != singleton_view.end());
	++iterator;
	CHECK(iterator == singleton_view.end());
	CHECK(iterator.Base() == singleton.begin());
}

TEST_CASE("AlternatingIterator copies postfix and position ordering")
{
	std::vector<int> values = {0, 1, 2, 3, 4};
	AlternatingView view(values, values.begin() + 2);
	auto first = view.begin();
	auto copy = first;
	auto old = copy++;
	CHECK(first == old);
	CHECK(*first == 2);
	CHECK(*old == 2);
	CHECK(*copy == 1);
	CHECK(first < copy);
	CHECK(copy > first);
	CHECK(&++first == &first);
	CHECK(first == copy);
	++copy;
	CHECK(*first == 1);
	CHECK(*copy == 3);
	CHECK(first < copy);
}

TEST_CASE("AlternatingIterator distinct equal end Base values")
{
	std::vector<int> values = {0, 1, 2, 3, 4};
	auto middle = values.begin() + 2;
	AlternatingView view(values, middle);
	auto constructed_end = view.end();
	auto iterated_end = view.begin();
	while (iterated_end != constructed_end) ++iterated_end;
	CHECK(iterated_end == constructed_end);
	CHECK((iterated_end <=> constructed_end) == std::strong_ordering::equal);
	CHECK(constructed_end.Base() == middle);
	CHECK(iterated_end.Base() == values.begin() + 4);
}

TEST_CASE("AlternatingIterator live noncontiguous range insertion")
{
	std::list<int> values = {0, 1, 2, 3, 4};
	auto middle = std::next(values.begin(), 2);
	AlternatingView view(values, middle);
	auto iterator = view.begin();
	std::vector<int> sequence = {*iterator};
	++iterator;
	sequence.push_back(*iterator);
	/* Existing iterators stay valid; both traversal and range length stay live. */
	values.insert(std::next(middle), 25);
	++iterator;
	while (iterator != view.end()) {
		sequence.push_back(*iterator);
		++iterator;
	}
	CHECK(sequence == std::vector<int>{2, 1, 25, 0, 3, 4});
	CHECK(iterator.Base() == std::prev(values.end()));
}

namespace {

struct IteratorOperations {
	size_t increments = 0;
	size_t decrements = 0;
};

/* Observe typed operations, including increments of copies made by distance/next. */
class ObservedIterator {
public:
	using value_type = int;
	using difference_type = std::ptrdiff_t;
	using iterator_category = std::bidirectional_iterator_tag;
	using pointer = int *;
	using reference = int &;

	ObservedIterator() = default;
	ObservedIterator(int *position, IteratorOperations &operations) : position(position), operations(&operations) {}
	int &operator*() const { return *this->position; }
	bool operator==(const ObservedIterator &rhs) const { return this->position == rhs.position; }
	ObservedIterator &operator++() { ++this->operations->increments; ++this->position; return *this; }
	ObservedIterator &operator--() { ++this->operations->decrements; --this->position; return *this; }

private:
	int *position;
	IteratorOperations *operations;
};

} // namespace

TEST_CASE("AlternatingIterator preserves typed operation order and counts")
{
	int values[] = {0, 1, 2, 3};
	IteratorOperations operations;
	AlternatingIterator iterator(ObservedIterator(values, operations), ObservedIterator(values + 4, operations), ObservedIterator(values + 2, operations), true);
	CHECK(operations.increments == 0);
	CHECK(operations.decrements == 0);
	++iterator; // Four distance increments, --before, then one next(after) increment.
	CHECK(operations.increments == 5);
	CHECK(operations.decrements == 1);
	operations = {};
	++iterator; // Four distance increments and ++after; no next(after) query.
	CHECK(operations.increments == 5);
	CHECK(operations.decrements == 0);
	operations = {};
	++iterator;
	CHECK(operations.increments == 5);
	CHECK(operations.decrements == 1);
	operations = {};
	++iterator; // Logical end performs only distance; the selected Base remains.
	CHECK(operations.increments == 4);
	CHECK(operations.decrements == 0);
	CHECK(iterator.Base() == ObservedIterator(values, operations));
}
