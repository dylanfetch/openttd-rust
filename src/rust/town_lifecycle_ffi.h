/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_lifecycle_ffi.h Narrow native shell leaves for founding policy. */
#ifndef RUST_TOWN_LIFECYCLE_FFI_H
#define RUST_TOWN_LIFECYCLE_FFI_H
#include "services_ffi.h"
#include <cstddef>
struct OpenTTDTownLeaves;
struct OpenTTDTownNameParams { uint32_t grfid; uint16_t kind; };
struct OpenTTDTownCountSizes { size_t houses; size_t classes; };
/* Persistent count storage belongs to Rust. Native spec/class registries supply
 * only sizes; no view or owner borrow survives structural count initialization. */
OpenTTDTownCountSizes GetRustTownCountSizes() noexcept;
struct OpenTTDTownCreateServices {
	OpenTTDTownCountSizes (*count_sizes)() noexcept;
	void (*insert_spatial)(const OpenTTDTownHandle *) noexcept;
	bool (*acceptance_cargo)(uint8_t) noexcept;
	OpenTTDTownNameParams (*name_params)() noexcept;
	uint8_t (*initial_city_size)() noexcept;
	void (*show_zone)(const OpenTTDTownHandle *, bool) noexcept;
	void (*virt_coord)(const OpenTTDTownHandle *) noexcept;
	void (*dirty_directory)() noexcept;
	void (*update_noise)() noexcept;
};
extern "C" {
void openttd_rust_town_initialize_counts(OpenTTDTownState *, OpenTTDTownCountSizes);
void openttd_rust_town_initialize_layout(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, const OpenTTDTownHandle *, uint8_t);
void openttd_rust_town_create(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, const OpenTTDTownCreateServices *, const OpenTTDTownHandle *, uint16_t, uint32_t, uint32_t, uint8_t, bool, uint8_t, bool);
}
#endif /* RUST_TOWN_LIFECYCLE_FFI_H */
