/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file builder_ffi.h By-value numeric byte encoding ABI; no borrowed output. */
#ifndef RUST_BUILDER_FFI_H
#define RUST_BUILDER_FFI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Low byte first; typed C++ methods use the first 1, 2, 4, or 8 bytes. */
typedef struct {
	uint8_t bytes[8];
} OpenTTDRustLittleEndian;

/** The original 32-byte capacity includes a minus sign; zero length means failure. */
typedef struct {
	uint8_t bytes[32];
	size_t length;
} OpenTTDRustFormattedInteger;

/** Pure scalar input/output: no allocation, ownership transfer, pointers, or callbacks. */
OpenTTDRustLittleEndian openttd_rust_encode_uint_le(uint64_t value);
/**
 * Base must be 2..36. For a signed negative input, convert directly to uint64_t
 * (modulo 2^64) and set negative=1; all other inputs use negative=0. Rust obtains
 * the magnitude by unsigned negation, including INT64_MIN. Returns lowercase,
 * prefix-free text; zero length means the original scratch capacity is exceeded.
 * The C++ adapter calls the virtual sink synchronously only on success, using a
 * temporary span into its own returned object. Rust calls no C++ callback, keeps
 * no state or pointers, and never unwinds into C++; panic aborts.
 */
OpenTTDRustFormattedInteger openttd_rust_format_integer(uint64_t bits, uint8_t negative, int32_t base);

#ifdef __cplusplus
}
#endif
#endif /* RUST_BUILDER_FFI_H */
