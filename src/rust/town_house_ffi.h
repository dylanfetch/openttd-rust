/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_house_ffi.h Narrow native map leaves for Rust house lifecycle. */
#ifndef RUST_TOWN_HOUSE_FFI_H
#define RUST_TOWN_HOUSE_FFI_H
#include "services_ffi.h"
#include <cstdint>
struct OpenTTDTownLeaves;
/* Byte/map/spec reads are immediate; mutations borrow only native map storage.
 * Animation is bounded NewGRF resolution and may reenter Rust, never unwind.
 * No Rust owner reference or native map reference survives a service call. */
struct OpenTTDTownHouseServices {
	uint8_t (*byte)(uint32_t) noexcept;
	bool (*completed)(uint32_t) noexcept;
	void (*set_byte)(uint32_t, uint8_t) noexcept;
	void (*set_completed)(uint32_t, bool) noexcept;
	uint16_t (*house)(uint32_t) noexcept;
	uint8_t (*population)(uint16_t) noexcept;
	void (*construction_animation)(uint32_t, bool) noexcept;
};
struct OpenTTDTownHouseNorth { uint16_t house; int32_t diff; };
extern "C" {
OpenTTDTownHouseNorth openttd_rust_town_house_north(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, uint16_t);
void openttd_rust_town_advance_house(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, const OpenTTDTownHouseServices *, uint32_t);
void openttd_rust_town_yearly(const OpenTTDSharedServices *, const OpenTTDTownHouseServices *, uint32_t);
}
#endif /* RUST_TOWN_HOUSE_FFI_H */
