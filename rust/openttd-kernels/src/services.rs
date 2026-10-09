/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Copied, synchronous shared-world leaves. C++ wrappers are noexcept: environmental
//! failures terminate there. No world reference or borrow crosses a leaf call.
#![allow(unsafe_code, clippy::cast_possible_truncation)]
use std::ffi::c_void;

/// Immutable call table supplied by the game (or isolated test world). No global
/// registration or game symbol imports are needed by standalone Cargo binaries.
/// The context and function addresses outlive the component invocation; callbacks
/// do not throw or reenter, and output pointers are borrowed for that call only.
/// Map predicates take a `TileIndex` value and return the accessor's underlying type.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Services {
    /// Opaque context for test fixtures; production leaves ignore it.
    pub context: *mut c_void,
    /// One shared game Random draw, including debug logging.
    pub random: extern "C" fn(*mut c_void) -> u32,
    /// `TileVirtXY` query: 0 not industry, 1 industry, 2 bubble catcher; writes tile.
    pub industry: extern "C" fn(i32, i32, *mut u32) -> u32,
    /// `GetTileType`.
    pub tile_type: extern "C" fn(u32) -> u8,
    /// `IsBridgeAbove`.
    pub bridge_above: extern "C" fn(u32) -> bool,
    /// `GetTropicZone`.
    pub tropic_zone: extern "C" fn(u32) -> u8,
    /// `GetTileZ`.
    pub tile_z: extern "C" fn(u32) -> i32,
    /// `GetTileSlope`.
    pub tile_slope: extern "C" fn(u32) -> u8,
    /// `MarkTileDirtyByTile` with the default bridge offset.
    pub mark_dirty: extern "C" fn(u32),
    /// `SetTropicZone`.
    pub set_tropic_zone: extern "C" fn(u32, u8),
}
impl Services {
    /// Draw one shared random word.
    #[must_use]
    pub fn random(&self) -> u32 {
        (self.random)(self.context)
    }
    /// Scale one word with the original 64-bit multiply and shift.
    #[must_use]
    pub fn random_range(&self, limit: u32) -> u32 {
        ((u64::from(self.random()) * u64::from(limit)) >> 32) as u32
    }
}
/// Standalone test table: map predicates return zero and writes do nothing.
#[cfg(test)]
#[must_use]
pub fn fixture(
    context: *mut c_void,
    random: extern "C" fn(*mut c_void) -> u32,
    industry: extern "C" fn(i32, i32, *mut u32) -> u32,
) -> Services {
    extern "C" fn byte(_: u32) -> u8 {
        0
    }
    extern "C" fn flag(_: u32) -> bool {
        false
    }
    extern "C" fn height(_: u32) -> i32 {
        0
    }
    extern "C" fn tile(_: u32) {}
    extern "C" fn zone(_: u32, _: u8) {}
    Services {
        context,
        random,
        industry,
        tile_type: byte,
        bridge_above: flag,
        tropic_zone: byte,
        tile_z: height,
        tile_slope: byte,
        mark_dirty: tile,
        set_tropic_zone: zone,
    }
}
/// Source `Chance16I` uses uint32 multiplication/addition after uint16 truncation.
#[must_use]
pub fn chance16_i(a: u32, b: u32, r: u32) -> bool {
    (((r & 65535).wrapping_mul(b).wrapping_add(b / 2)) >> 16) < a
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn chance_retains_low_word_rounding_and_u32_wrap() {
        assert!(chance16_i(1, 200, 0));
        assert!(chance16_i(1, 200, 327));
        assert!(!chance16_i(1, 200, 328));
        assert!(!chance16_i(1, 200, 65535));
        assert!(chance16_i(1, 200, 0xffff_0000));
        assert_eq!(
            chance16_i(1, u32::MAX, 65535),
            ((65535_u32.wrapping_mul(u32::MAX).wrapping_add(u32::MAX / 2)) >> 16) < 1
        );
    }
}
