/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Scalar game kernels exposed through the ABI documented in `src/rust/ffi.h`.

mod landscape;

/// Height at a coordinate within a tile, preserving `OpenTTD`'s slope rounding.
///
/// Coordinates must be in 0..16. Returns `u32::MAX` for an invalid coordinate or
/// an unsupported base slope reached after half-tile handling; the C++ adapter
/// invokes the original `NOT_REACHED` fatal handler on that sentinel.
///
/// No pointers, ownership transfer, allocation, or mutable state cross this ABI.
/// The fixed symbol is unique to this crate. Normal arithmetic is bounded by
/// tile dimensions; release overflow checks stay enabled. Both build profiles
/// abort on panic, and the non-unwinding C ABI also prevents unwinding into C++.
#[allow(unsafe_code)] // Only the exported symbol attribute; the function is safe Rust.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_get_partial_pixel_z(x: i32, y: i32, corners: u8) -> u32 {
    if !(0..16).contains(&x) || !(0..16).contains(&y) {
        return u32::MAX;
    }
    landscape::partial_pixel_z(x, y, corners).unwrap_or(u32::MAX)
}
