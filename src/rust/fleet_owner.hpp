/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fleet_owner.hpp Scalar lifetime views over canonical Rust allocations. */
#ifndef RUST_FLEET_OWNER_HPP
#define RUST_FLEET_OWNER_HPP
#include "fleet_ffi.h"
template <typename T, auto Create, auto Destroy, auto Copy = nullptr, bool Invalid = false> struct FleetOwner {
	T *state;
	FleetOwner() : state(static_cast<T *>(Create())) {
		if constexpr (Invalid) { new (state) T(T::Invalid()); } else { new (state) T(); }
	}
	FleetOwner(const FleetOwner &other) : FleetOwner() { Copy(state, other.state); }
	FleetOwner &operator=(const FleetOwner &other) { Copy(state, other.state); return *this; }
	~FleetOwner() { state->~T(); Destroy(state); }
};
/** Nonallocating ordered facade; Rust owns the selected child traversals. */
struct FleetChildren {
	void *owner;
	bool empty() const { return openttd_rust_fleet_child(owner, 0) == UINT32_MAX; }
	void insert(GroupID id) { openttd_rust_fleet_child_change(owner, id.base(), true); }
	void erase(GroupID id) { openttd_rust_fleet_child_change(owner, id.base(), false); }
	struct Iterator {
		using iterator_category = std::input_iterator_tag;
		using value_type = GroupID;
		using difference_type = ptrdiff_t;
		using pointer = void;
		using reference = GroupID;
		void *owner;
		uint32_t current;
		GroupID operator*() const { return GroupID(current); }
		Iterator &operator++() { current = openttd_rust_fleet_child(owner, current + 1); return *this; }
		bool operator==(const Iterator &other) const { return current == other.current; }
	};
	Iterator begin() const { return {owner, openttd_rust_fleet_child(owner, 0)}; }
	Iterator end() const { return {owner, UINT32_MAX}; }

};
/** Mutations alias the sole Rust engine map; entries retain original uint16 wrapping. */
struct FleetEngineCounts {
	void *owner;
	struct Entry {
		void *owner;
		EngineID engine;
		void operator+=(int delta) { openttd_rust_fleet_engine_change(owner, engine.base(), delta); }
		void operator++(int) { *this += 1; }
		void operator--(int) { *this += -1; }
	};
	Entry operator[](EngineID engine) { return {owner, engine}; }
};
#endif /* RUST_FLEET_OWNER_HPP */
