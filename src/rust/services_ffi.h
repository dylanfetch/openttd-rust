/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file services_ffi.h Synchronous shared game services for Rust owners. */
#ifndef RUST_SERVICES_FFI_H
#define RUST_SERVICES_FFI_H
#include <cstdint>

/* Copied immutable function table. All functions are noexcept and cannot reenter
 * Rust. Context may be null in the game; test fixtures supply their own world.
 * No Rust references into the map/pools survive calls. Output is call-scoped.
 * Tile observation[10]: type, bridge, tropic zone, height, ground, density,
 * tree species/count/growth, snow/coast/one-raised-corner bits. Only valid fields
 * for the tile type are read. Writes 0..9: make tree, ground/density, add count,
 * add growth, set growth, dirty, make clear, make shore, make snow, set zone.
 * Random logging still runs, but its debug source location names this wrapper.
 * Industry query returns 0 not industry,1 industry,2 bubble catcher and writes
 * TileVirtXY(x,y). It observes only type and industry graphics.
 * An environmental exception terminates rather than unwinding through Rust. */
struct OpenTTDSharedServices {
	void *context;
	uint32_t (*random)(void *) noexcept;
	void (*observe_tile)(void *, uint32_t, uint32_t *) noexcept;
	void (*write_tile)(void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) noexcept;
	float (*trig)(uint32_t, float) noexcept;
	uint32_t (*industry)(int32_t, int32_t, uint32_t *) noexcept;
};
const OpenTTDSharedServices &GetRustSharedServices() noexcept;
#endif /* RUST_SERVICES_FFI_H */
