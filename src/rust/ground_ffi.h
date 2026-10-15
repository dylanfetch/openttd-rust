/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ground_ffi.h Consumed terrain fields and direct synchronous leaves. */
#ifndef RUST_GROUND_FFI_H
#define RUST_GROUND_FFI_H
#include "services_ffi.h"
struct OpenTTDClearGround { uint8_t ground; uint8_t density; uint8_t counter; bool snow; };
struct OpenTTDFieldGround { uint16_t industry; uint8_t type; };
struct OpenTTDClearServices {
	OpenTTDClearGround (*observe)(uint32_t) noexcept;
	void (*set_ground)(uint32_t, uint8_t, uint32_t) noexcept;
	void (*set_density)(uint32_t, uint32_t) noexcept;
	void (*set_counter)(uint32_t, uint32_t) noexcept;
	void (*clear_snow)(uint32_t) noexcept;
	void (*make_snow)(uint32_t, uint32_t) noexcept;
	void (*make_clear)(uint32_t, uint8_t, uint32_t) noexcept;
	uint32_t (*fence)(uint32_t, uint8_t) noexcept;
	void (*set_fence)(uint32_t, uint8_t, uint32_t) noexcept;
	OpenTTDFieldGround (*field)(uint32_t) noexcept;
	void (*set_field)(uint32_t, uint32_t) noexcept;
	bool (*has_water_class)(uint32_t) noexcept;
	uint8_t (*water_class)(uint32_t) noexcept;
	bool (*ambient)(uint32_t) noexcept;
};
struct OpenTTDRailSnowGround { int32_t z; uint8_t slope; uint8_t tracks; bool plain; };
struct OpenTTDRailFenceGround { uint8_t tracks; uint8_t owner; bool plain; };
struct OpenTTDFoundationSlope { int32_t z; uint8_t slope; };
struct OpenTTDRailServices {
	uint8_t (*ground)(uint32_t) noexcept;
	void (*set_ground)(uint32_t, uint8_t) noexcept;
	OpenTTDRailSnowGround (*snow)(uint32_t) noexcept;
	OpenTTDRailFenceGround (*fences)(uint32_t) noexcept;
	uint8_t (*foundation)(uint8_t, uint8_t) noexcept;
	bool (*owned_land)(uint32_t) noexcept;
	void (*water_loop)(uint32_t) noexcept;
};
struct OpenTTDTunnelGround { int32_t z; bool snow; };
struct OpenTTDTunnelServices {
	OpenTTDTunnelGround (*snow)(uint32_t) noexcept;
	bool (*snow_or_desert)(uint32_t) noexcept;
	void (*set_snow_or_desert)(uint32_t, bool) noexcept;
};
const OpenTTDClearServices &GetRustClearServices() noexcept;
const OpenTTDRailServices &GetRustRailGroundServices() noexcept;
const OpenTTDTunnelServices &GetRustTunnelGroundServices() noexcept;
extern "C" {
void openttd_rust_clear_tile_loop(uint32_t, uint8_t, bool, uint32_t, const OpenTTDClearServices *, const OpenTTDSharedServices *);
void openttd_rust_rail_tile_loop(uint32_t, uint8_t, uint32_t, const OpenTTDRailServices *, const OpenTTDSharedServices *);
void openttd_rust_tunnel_tile_loop(uint32_t, uint8_t, const OpenTTDTunnelServices *, const OpenTTDSharedServices *);
}
#endif /* RUST_GROUND_FFI_H */
