/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_placement_ffi.h Native spatial/spec leaves for Rust town-site policy. */
#ifndef RUST_TOWN_PLACEMENT_FFI_H
#define RUST_TOWN_PLACEMENT_FFI_H
#include "services_ffi.h"
#include "command_cost_ffi.h"
struct OpenTTDTownLeaves;
struct OpenTTDTownPlacementServices {
	bool (*spatial_empty)() noexcept;
	const OpenTTDTownHandle *(*nearest_town)(uint32_t) noexcept;
	uint8_t (*clear_ground)(uint32_t) noexcept;
	uint8_t (*tree_ground)(uint32_t) noexcept;
	uint32_t (*water_distance)(uint32_t, bool) noexcept;
	uint8_t (*number_towns)() noexcept;
	uint16_t (*custom_towns)() noexcept;
	uint32_t edge_error;
	uint32_t town_error;
	uint32_t site_error;
};
extern "C" {
void openttd_rust_town_site(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, const OpenTTDTownPlacementServices *, const OpenTTDCommandCostServices *, OpenTTDCommandCost *, uint32_t, bool);
uint32_t openttd_rust_town_align(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, uint32_t, uint8_t);
uint32_t openttd_rust_town_coastal(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, const OpenTTDTownPlacementServices *, uint32_t, uint8_t);
uint32_t openttd_rust_town_default_number(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, const OpenTTDTownPlacementServices *);
}
#endif /* RUST_TOWN_PLACEMENT_FFI_H */
