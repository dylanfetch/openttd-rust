/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Scalar game kernels exposed through the ABI documented in `src/rust/ffi.h`.

mod integer;
mod landscape;

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

mod utf8;
pub use utf8::{Decoded as Utf8Decoded, Encoded as Utf8Encoded};

/// Encode a 32-bit codepoint by value without allocation or ownership transfer.
/// Surrogates are accepted; invalid values return zero length and zero bytes.
#[allow(unsafe_code)] // Export attribute only.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_encode_utf8(codepoint: u32) -> Utf8Encoded {
    utf8::encode(codepoint)
}

/// Classify an original byte, returning one for continuation bytes and zero otherwise.
#[allow(unsafe_code)] // Export attribute only.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_is_utf8_part(byte: u8) -> u8 {
    u8::from(utf8::is_part(byte))
}

/// Decode the first sequence, without examining trailing bytes or rejecting surrogates.
///
/// # Safety
/// `data` must address `length` readable initialized bytes in one allocation, valid
/// and unmodified for this call, with length at most `isize::MAX`. Zero length permits
/// null. No pointer is retained; read-only overlapping spans are allowed. Panics abort.
#[allow(unsafe_code)] // Export attribute and the single borrowed-slice construction.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_decode_utf8(data: *const u8, length: usize) -> Utf8Decoded {
    // SAFETY: The caller supplies the immutable span described above.
    utf8::decode(unsafe { utf8::borrow(data, length) })
}

/// Advance one byte and skip its following continuation run, without decoding.
///
/// # Safety
/// The same input-span requirements as `openttd_rust_decode_utf8` apply, and
/// `position < length`. The original C++ assertion remains in the facade.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_utf8_next(
    data: *const u8,
    length: usize,
    position: usize,
) -> usize {
    // SAFETY: The caller supplies the immutable span described above.
    utf8::next(unsafe { utf8::borrow(data, length) }, position)
}

/// Retreat to the preceding non-continuation byte or offset zero, without decoding.
///
/// # Safety
/// The same input-span requirements as `openttd_rust_decode_utf8` apply, and
/// `0 < position <= length`. The original C++ assertion remains in the facade.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_utf8_previous(
    data: *const u8,
    length: usize,
    position: usize,
) -> usize {
    // SAFETY: The caller supplies the immutable span described above.
    utf8::previous(unsafe { utf8::borrow(data, length) }, position)
}

/// Normalize a byte offset by backward scanning; offsets at or beyond length yield end.
///
/// # Safety
/// The same input-span requirements as `openttd_rust_decode_utf8` apply.
/// The facade preserves its assertion of `offset <= length` before this call.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_utf8_at_byte(
    data: *const u8,
    length: usize,
    offset: usize,
) -> usize {
    // SAFETY: The caller supplies the immutable span described above.
    utf8::at_byte(unsafe { utf8::borrow(data, length) }, offset)
}
