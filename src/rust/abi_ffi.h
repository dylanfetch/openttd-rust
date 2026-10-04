/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file abi_ffi.h Bounded native layout agreement checks, not an algorithm interface. */
#ifndef RUST_ABI_FFI_H
#define RUST_ABI_FFI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/**
 * Type IDs: 0 integer, 1 UTF8 encoded, 2 UTF8 decoded, 3 LE bytes, 4 formatted,
 * 5 alternating state, 6 alternating step, 7 consumer bound, 8 consumer byte,
 * 9 consumer match, 10 consumer separator, 11 encoded parameter, 12 encoded view,
 * 13 encoded diagnostic, 14 spiral state, 15 byte trim,
 * 16 history descriptor, 17 history step, 18 station cargo,
 * 19 cipher leaves, 20 Poly1305 layout, 21 AEAD layout, 22 BLAKE2b layout,
 * 23 Packet state, 24 Packet framing offsets, 25 X25519 leaves,
 * 26 string-validation step, 27 in-place write result.
 * 39 effect view, 40 retired effect cursor, 41 effect map leaves, 42 shared game services.
 * 46 water patch, 47 water diagnostic snapshot, 48 water shared-service leaves.
 * 52 ship YAPF input, 53 leaves, 54 follower, 55 tile, 56 choice result.
 * Item 0 size, 1 alignment, then every field offset in declaration order.
 * Unknown type/item returns SIZE_MAX. No memory borrow, ownership or callback.
 * Native extern-C convention (cdecl on i686 MSVC); Rust panic never unwinds.
 */
size_t openttd_rust_abi_layout(uint8_t type_id, uint8_t item);
#ifdef __cplusplus
}
#endif
#endif /* RUST_ABI_FFI_H */
