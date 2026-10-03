/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Migrated game and text kernels exposed through the documented `src/rust` ABIs.

mod alternating;
mod consumer;
mod integer;
mod landscape;

pub use alternating::{AlternatingState, AlternatingStep};
pub use consumer::{
    BoundResult as ConsumerBound, ByteResult as ConsumerByte, MatchResult as ConsumerMatch,
    SeparatorResult as ConsumerSeparator,
};
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

mod builder;
pub use builder::{FormattedInteger, LittleEndian};

/// Encode a scalar as eight little-endian bytes; the C++ facade selects its width.
/// Returns owned by-value storage without pointers, allocation, or ownership transfer.
#[allow(unsafe_code)] // Export attribute only; the implementation is safe Rust.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_encode_uint_le(value: u64) -> LittleEndian {
    builder::little_endian(value)
}

/// Format integer bits into the original fixed 32-byte scratch capacity.
/// Base must be 2..36; negative is one only for a signed negative input converted
/// directly to `uint64_t` modulo 2^64. Zero length suppresses the C++ sink call.
/// No pointer, allocation, ownership transfer, callback, or state crosses this ABI.
/// Panics abort, and this C ABI never unwinds into C++.
///
/// # Panics
/// Invalid bases violate the original `std::to_chars` precondition and abort.
#[allow(unsafe_code)] // Export attribute only; the implementation is safe Rust.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_format_integer(
    bits: u64,
    negative: u8,
    base: i32,
) -> FormattedInteger {
    builder::integer(bits, negative != 0, base)
}

/// Bound requested bytes and decide shortfall before C++ logs/commits the cursor.
/// Position must not exceed size. `usize::MAX` requests all remaining bytes.
/// No pointer, allocation, callback, or ownership crosses this non-unwinding ABI.
#[allow(unsafe_code)] // Exported symbol attribute only.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_consumer_bound(
    size: usize,
    position: usize,
    requested: usize,
) -> ConsumerBound {
    consumer::bound(size, position, requested)
}

/// Read unsigned little-endian bits without signed conversion or cursor changes.
/// # Safety
/// Data addresses one allocation of readable immutable bytes for the call only,
/// length <= `isize::MAX`; empty spans permit null. Width is 1/2/4/8 bytes.
/// No pointer is retained, no callback occurs while borrowing, and panic aborts.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_consumer_little_endian(
    data: *const u8,
    length: usize,
    width: u8,
) -> ConsumerByte {
    // SAFETY: The caller guarantees the call-local read-only span above.
    consumer::little_endian(unsafe { utf8::borrow(data, length) }, width)
}

/// Match a prefix, including an empty prefix, and return conditional consumption.
/// # Safety
/// Both spans have the same borrow contract as `openttd_rust_consumer_little_endian`;
/// they may overlap read-only. Empty patterns are valid. No pointer is retained.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_consumer_prefix(
    data: *const u8,
    length: usize,
    pattern: *const u8,
    pattern_length: usize,
) -> ConsumerMatch {
    // SAFETY: Both spans are call-local and immutable under the caller contract.
    consumer::prefix(unsafe { utf8::borrow(data, length) }, unsafe {
        utf8::borrow(pattern, pattern_length)
    })
}

/// Find a substring, member byte, or nonmember byte; not-found is `usize::MAX`.
/// # Safety
/// Both spans have the prefix borrow contract, including read-only overlap.
/// Pattern must be nonempty; mode is 0 substring, 1 membership, 2 nonmembership.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_consumer_find(
    data: *const u8,
    length: usize,
    pattern: *const u8,
    pattern_length: usize,
    mode: u8,
) -> usize {
    // SAFETY: Both spans are call-local and immutable under the caller contract.
    consumer::find(
        unsafe { utf8::borrow(data, length) },
        unsafe { utf8::borrow(pattern, pattern_length) },
        mode,
    )
}

/// Test byte membership and return its unsigned bits only when it matches.
/// # Safety
/// Both spans have the prefix borrow contract, including read-only overlap.
/// Pattern must be nonempty; member is 0/1. No pointer or allocation is retained.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_consumer_character(
    data: *const u8,
    length: usize,
    pattern: *const u8,
    pattern_length: usize,
    member: u8,
) -> ConsumerByte {
    // SAFETY: Both spans are call-local and immutable under the caller contract.
    consumer::character(
        unsafe { utf8::borrow(data, length) },
        unsafe { utf8::borrow(pattern, pattern_length) },
        member != 0,
    )
}

/// Decide returned and consumed lengths for whole, nonoverlapping separators.
/// # Safety
/// Both spans have the prefix borrow contract, including read-only overlap.
/// Separator must be nonempty. Policies 0..4 follow C++ `SeparatorUsage`; all other
/// values retain KEEP behavior. No pointer is retained and panic aborts.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_consumer_separator(
    data: *const u8,
    length: usize,
    pattern: *const u8,
    pattern_length: usize,
    policy: i32,
) -> ConsumerSeparator {
    // SAFETY: Both spans are call-local and immutable under the caller contract.
    consumer::separator(
        unsafe { utf8::borrow(data, length) },
        unsafe { utf8::borrow(pattern, pattern_length) },
        policy,
    )
}

mod spiral;
pub use spiral::SpiralState;

/// Initialize a square spiral with the live map dimensions; diameter is positive.
/// All scalar arithmetic wraps at 32 bits. No pointer/ownership crosses the ABI.
#[allow(unsafe_code)] // Export attribute only; implementation is safe Rust.
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_spiral_square(
    x: u32,
    y: u32,
    diameter: u32,
    size_x: u32,
    size_y: u32,
) -> SpiralState {
    spiral::square(x, y, diameter, size_x, size_y)
}

/// Initialize a rectangular-hole spiral; radius is positive, extents are unsigned.
/// Live map dimensions are supplied by the facade; no map/storage borrow occurs.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_spiral_hole(
    x: u32,
    y: u32,
    radius: u32,
    width: u32,
    height: u32,
    size_x: u32,
    size_y: u32,
) -> SpiralState {
    spiral::hole(x, y, radius, width, height, size_x, size_y)
}

/// Advance a non-ended initialized state and skip coordinates outside the live map.
/// State/results are independent copies; panic aborts without unwinding into C++.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_spiral_advance(
    state: SpiralState,
    size_x: u32,
    size_y: u32,
) -> SpiralState {
    state.advance(size_x, size_y)
}

/// End means current radius equals its limit with a non-invalid direction.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_spiral_end(state: SpiralState) -> u8 {
    u8::from(state.is_end())
}

/// Iterator equality compares coordinates only, independent of other state.
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub extern "C" fn openttd_rust_spiral_equal(left: SpiralState, right: SpiralState) -> u8 {
    u8::from(spiral::equal(left, right))
}
