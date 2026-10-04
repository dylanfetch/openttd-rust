/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_owner.hpp Noncopyable Rust town owner and thin field access facade. */
#ifndef RUST_TOWN_OWNER_HPP
#define RUST_TOWN_OWNER_HPP
#include "town_ffi.h"
#include <memory>

struct RustTownDelete {
	void operator()(OpenTTDTownState *state) const { openttd_rust_town_destroy(state); }
};
using RustTownOwner = std::unique_ptr<OpenTTDTownState, RustTownDelete>;

template <typename T, uint8_t Field>
struct RustTownScalar {
	OpenTTDTownState *owner;
	operator T() const { return static_cast<T>(openttd_rust_town_get(this->owner, Field)); }
	RustTownScalar &operator=(T value) { openttd_rust_town_set(this->owner, Field, value); return *this; }
	T operator--(int) { T value = *this; *this = value - 1; return value; }
};

template <typename Flags, typename Flag>
struct RustTownFlags {
	OpenTTDTownState *owner;
	operator Flags() const { return Flags(static_cast<uint8_t>(openttd_rust_town_get(this->owner, 4))); }
	uint8_t base() const { return static_cast<Flags>(*this).base(); }
	bool Test(Flag flag) const { return static_cast<Flags>(*this).Test(flag); }
	bool Any(Flags flags) const { return static_cast<Flags>(*this).Any(flags); }
	void Set(Flags flags) { Flags value = *this; value.Set(flags); openttd_rust_town_set(this->owner, 4, value.base()); }
	void Set(Flag flag) { this->Set(Flags(flag)); }
	void Reset(Flags flags) { Flags value = *this; value.Reset(flags); openttd_rust_town_set(this->owner, 4, value.base()); }
	void Reset(Flag flag) { this->Reset(Flags(flag)); }
	void Reset() { openttd_rust_town_set(this->owner, 4, 0); }
};
#endif /* RUST_TOWN_OWNER_HPP */
