/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tgp_ffi.h Rust-owned TerraGenesis height map. */
#ifndef TGP_FFI_H
#define TGP_FFI_H
#include <cstdint>
extern "C" {
/* Settings (15 uint32_t fields): map logs x/y, terrain, custom terrain, height limit, smoothness,
 * landscape, variety, sea quantity, custom sea percent, freeform, borders, seed, configured map logs x/y.
 * Valid game settings only. The pointer is borrowed for the call. */
uint32_t openttd_rust_tgp_estimate(const uint32_t *values);
/* RNG callbacks are nonthrowing leaf operations on the shared game random state.
 * Create owns the map; advance owns the complete generation stage machine.
 * Progress runs between calls in C++, so exceptions never cross Rust frames. */
void *openttd_rust_tgp_create(const uint32_t *values, uint32_t (*random)(), uint32_t (*range)(uint32_t));
/* Advance returns 1 for a progress event; dispatch it before advancing again.
 * Returns 0 once generation is complete. */
uint8_t openttd_rust_tgp_advance(void *owner);
/* Write map-size bytes row-major; output cannot alias the live owner. */
void openttd_rust_tgp_heights(const void *owner, uint8_t *output);
/* Destroy exactly once, including genworld abortion; no retained views. */
void openttd_rust_tgp_destroy(void *owner);
}
#endif /* TGP_FFI_H */
