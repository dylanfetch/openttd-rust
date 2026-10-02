/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ffi.h Narrow scalar ABI for migrated Rust kernels. */

#ifndef RUST_FFI_H
#define RUST_FFI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Height for tile-relative x/y in [0, 15] and the original eight-bit slope.
 * No pointers or ownership transfer. UINT32_MAX requests the C++ fatal handler.
 * Rust never unwinds into C++; panic aborts. Actual heights are at most 16.
 */
uint32_t openttd_rust_get_partial_pixel_z(int32_t x, int32_t y, uint8_t corners);

#ifdef __cplusplus
}
#endif

#endif /* RUST_FFI_H */
