/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file math_ffi.h Pointer-free integer math ABI. */

#ifndef RUST_MATH_FFI_H
#define RUST_MATH_FFI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Nearest integer square root over the entire uint32 domain, including 65536. */
uint32_t openttd_rust_int_sqrt(uint32_t value);

/**
 * Widths are 1 (bool), 8, 16, 32, or 64; signed selectors are 0/1.
 * Values enter as modulo-2^64 bits; Rust masks/sign-interprets the source width.
 * Returns destination value bits; C++20 typed conversion reconstructs the result.
 * Accepted wider unsigned C++ destinations request width 64, retaining the entire
 * standard source value, then widen that result in C++.
 * bool values are 0/1. No signed intermediate is used for unsigned uint64 values.
 * No pointers, allocations, ownership, state, callbacks, or exceptions cross
 * these functions. Rust arithmetic uses explicit bounded or wrapping operations;
 * panic aborts and the C ABI cannot unwind into C++.
 */
uint64_t openttd_rust_clamp_to(uint64_t value, uint8_t from_width, uint8_t from_signed, uint8_t to_width, uint8_t to_signed);

/**
 * width is 8/16/32/64 (bool is not a valid original SoftClamp instantiation).
 * Reversed signed 8/16-bit intervals preserve 32-bit C++ integer promotions,
 * including conversion of negative min to its narrow unsigned counterpart.
 * Other reversed intervals use unsigned arithmetic at the specified width.
 * Ordinary intervals retain <= and >= endpoint decisions.
 */
uint64_t openttd_rust_soft_clamp(uint64_t value, uint64_t min, uint64_t max, uint8_t width, uint8_t is_signed);

#ifdef __cplusplus
}
#endif

#endif /* RUST_MATH_FFI_H */
