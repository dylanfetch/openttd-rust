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
template <typename T, uint32_t Kind> struct FleetOwner {
	T *state;
	FleetOwner() : state(static_cast<T *>(openttd_rust_fleet_state_create(Kind))) {
		if constexpr (Kind == 4) { new (state) T(T::Invalid()); } else { new (state) T(); }
	}
	FleetOwner(const FleetOwner &other) : FleetOwner() { openttd_rust_fleet_state_copy(Kind, state, other.state); }
	FleetOwner &operator=(const FleetOwner &other) { openttd_rust_fleet_state_copy(Kind, state, other.state); return *this; }
	~FleetOwner() { state->~T(); openttd_rust_fleet_state_destroy(Kind, state); }
};
/** Call-local ordered exports; WIP command traversal remains in C++. */
struct FleetChildren {
	void *owner;
	bool empty() const { return openttd_rust_fleet_child(owner, 0) == UINT32_MAX; }
	void insert(GroupID id) { openttd_rust_fleet_child_change(owner, id.base(), true); }
	void erase(GroupID id) { openttd_rust_fleet_child_change(owner, id.base(), false); }
	std::vector<GroupID> Export() const {
		std::vector<GroupID> out;
		for (uint32_t from = 0, id; (id = openttd_rust_fleet_child(owner, from)) != UINT32_MAX; from = id + 1) out.emplace_back(id);
		return out;
	}
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
