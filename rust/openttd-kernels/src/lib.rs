/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Scalar game kernels exposed through the ABI documented in `src/rust/ffi.h`.

mod alternating;
mod integer;
mod landscape;

pub use alternating::{AlternatingState, AlternatingStep};
pub use integer::IntegerResult;

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

/// Parse borrowed arbitrary bytes using the original integer grammar.
///
/// # Safety
/// For nonzero length, `src` must point to `length` readable bytes in a single
/// live allocation, with length at most `isize::MAX` and no concurrent mutation
/// during the call. Zero length
/// permits a null pointer. No pointer is retained or allocation transferred.
/// Base must be 0/8/10/16 and width 1..=64; the C++ adapter supplies native integer
/// widths and preserves the original base assertion. Signed/clamp are 0 or 1.
/// This C ABI never unwinds; workspace build profiles abort on panic.
#[allow(unsafe_code)] // Export and the one borrowed-slice construction only.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_parse_integer(
    src: *const u8,
    length: usize,
    base: u8,
    width: u8,
    signed: u8,
    clamp: u8,
) -> IntegerResult {
    let bytes = if length == 0 {
        &[]
    } else {
        // SAFETY: The caller guarantees the readable allocation for this call.
        unsafe { std::slice::from_raw_parts(src, length) }
    };
    integer::parse(bytes, base, width, signed != 0, clamp != 0)
}

/// Return lexical integer skip length, independently of parse success/length.
///
/// # Safety
/// The pointer/length borrow has the same requirements as
/// `openttd_rust_parse_integer`; base must be 0/8/10/16. No pointer is retained.
/// Empty input permits null. This C ABI never unwinds.
#[allow(unsafe_code)] // Export and the one borrowed-slice construction only.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_skip_integer(
    src: *const u8,
    length: usize,
    base: u8,
) -> usize {
    let bytes = if length == 0 {
        &[]
    } else {
        // SAFETY: The caller guarantees the readable allocation for this call.
        unsafe { std::slice::from_raw_parts(src, length) }
    };
    integer::skip(bytes, base)
}

/// Initialize traversal position and side from the original live boundary fact.
/// All arguments are scalar, no pointer ownership or borrowing crosses the ABI.
/// End position is zero for begin, otherwise the nonnegative range distance.
/// Both side flags are 0/1; no C++ iterator is retained and panic aborts.
#[allow(unsafe_code)] // Exported symbol attribute only; traversal is safe Rust.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_alternating_initialize(
    end_position: usize,
    before_at_first: u8,
) -> AlternatingState {
    alternating::initialize(end_position, before_at_first != 0)
}

/// Choose a logical advance and typed movement using the current range size.
/// Position must be less than `live_size`; state must originate from initialize
/// and completed advances. No range length is cached and panic aborts.
#[allow(unsafe_code)] // Exported symbol attribute only; traversal is safe Rust.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_alternating_advance(
    state: AlternatingState,
    live_size: usize,
) -> AlternatingStep {
    alternating::advance(state, live_size)
}

/// Complete a nonterminal advance using its requested live boundary fact.
/// After ++after, boundary is before == first; after --before, boundary is
/// next(after) != last. It must be 0/1. C++ retains all typed iterators; no
/// pointer or allocation crosses the non-unwinding ABI and panic aborts.
#[allow(unsafe_code)] // Exported symbol attribute only; traversal is safe Rust.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_alternating_complete(
    state: AlternatingState,
    boundary: u8,
) -> AlternatingState {
    alternating::complete(state, boundary != 0)
}

/// Compare positions in states whose range/middle identity C++ has checked.
/// Other state fields deliberately do not participate. No pointers or ownership
/// cross this non-unwinding ABI; panic aborts.
#[allow(unsafe_code)] // Exported symbol attribute only; traversal is safe Rust.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_alternating_compare(
    left: AlternatingState,
    right: AlternatingState,
) -> i8 {
    alternating::compare(left, right)
}
