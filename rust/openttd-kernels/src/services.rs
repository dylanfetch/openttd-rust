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
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Services {
    /// Opaque context for test fixtures; production leaves ignore it.
    pub context: *mut c_void,
    /// One shared game Random draw, including debug logging.
    pub random: extern "C" fn(*mut c_void) -> u32,
    /// Copy ten tile words; output exists only during this call.
    pub observe_tile: extern "C" fn(*mut c_void, u32, *mut u32),
    /// Apply one map write at the original mutation point.
    pub write_tile: extern "C" fn(*mut c_void, u32, u32, u32, u32, u32, u32),
    /// Native sinf/cosf used by the original grove construction.
    pub trig: extern "C" fn(u32, f32) -> f32,
    /// `TileVirtXY` query: 0 not industry, 1 industry, 2 bubble catcher; writes tile.
    pub industry: extern "C" fn(i32, i32, *mut u32) -> u32,
    /// `GetTileType` at the original observation point.
    pub tile_type: extern "C" fn(u32) -> u8,
    /// `GetTileOwner` at the original observation point.
    pub tile_owner: extern "C" fn(u32) -> u8,
    /// `GetStationIndex` at the original observation point.
    pub station_index: extern "C" fn(u32) -> u16,
    /// `GetDepotIndex` at the original observation point.
    pub depot_index: extern "C" fn(u32) -> u16,
    /// `IsLevelCrossingTile` at the original observation point.
    pub is_level_crossing: extern "C" fn(u32) -> bool,
    /// `GetTunnelBridgeDirection` at the original observation point.
    pub tunnel_bridge_direction: extern "C" fn(u32) -> u8,
    /// `GetBridgeSpec(GetBridgeType(tile))->speed`.
    pub bridge_speed: extern "C" fn(u32) -> u16,
}
impl Services {
    /// Draw one shared random word.
    #[must_use]
    pub fn random(self) -> u32 {
        (self.random)(self.context)
    }
    /// Scale one word with the original 64-bit multiply and shift.
    #[must_use]
    pub fn random_range(self, limit: u32) -> u32 {
        ((u64::from(self.random()) * u64::from(limit)) >> 32) as u32
    }
    /// Observe fields valid for this tile type as a copied record.
    #[must_use]
    pub fn tile(self, tile: u32) -> [u32; 10] {
        let mut values = [0; 10];
        (self.observe_tile)(self.context, tile, values.as_mut_ptr());
        values
    }
    /// Apply one of the header-documented scalar tile operations.
    pub fn write(self, op: u32, tile: u32, args: [u32; 4]) {
        (self.write_tile)(self.context, op, tile, args[0], args[1], args[2], args[3]);
    }
}
/// Map dimensions read once per entry.
#[derive(Clone, Copy)]
pub struct Map {
    /// `Map::LogX()` captured for this synchronous entry.
    pub log_x: u8,
}
impl Map {
    /// `Map::SizeX()`, from the dimension captured at entry.
    #[must_use]
    pub fn size_x(self) -> u32 {
        1_u32 << self.log_x
    }
    /// `TileX`.
    #[must_use]
    pub fn tile_x(self, tile: u32) -> u32 {
        tile & (self.size_x() - 1)
    }
    /// `TileY`.
    #[must_use]
    pub fn tile_y(self, tile: u32) -> u32 {
        tile >> self.log_x
    }
    /// `TileDiffXY`: `y * Map::SizeX() + x` in unsigned arithmetic.
    #[must_use]
    pub fn diff(self, x: i32, y: i32) -> u32 {
        y.cast_unsigned()
            .wrapping_mul(self.size_x())
            .wrapping_add(x.cast_unsigned())
    }
    /// `tile + TileOffsByDiagDir(dir)`.
    #[must_use]
    pub fn add_diag(self, tile: u32, dir: u8) -> u32 {
        const OFFSETS: [(i32, i32); 4] = [(-1, 0), (0, 1), (1, 0), (0, -1)];
        let (x, y) = OFFSETS[usize::from(dir)];
        tile.wrapping_add(self.diff(x, y))
    }
    /// `TileAddByDir(tile, dir)`.
    #[must_use]
    pub fn add_dir(self, tile: u32, dir: u8) -> u32 {
        const OFFSETS: [(i32, i32); 8] = [
            (-1, -1),
            (-1, 0),
            (-1, 1),
            (0, 1),
            (1, 1),
            (1, 0),
            (1, -1),
            (0, -1),
        ];
        let (x, y) = OFFSETS[usize::from(dir)];
        tile.wrapping_add(self.diff(x, y))
    }
}

/// Source `Chance16I` uses uint32 multiplication/addition after uint16 truncation.
#[must_use]
pub fn chance16_i(a: u32, b: u32, r: u32) -> bool {
    (((r & 65535).wrapping_mul(b).wrapping_add(b / 2)) >> 16) < a
}

#[cfg(test)]
pub(crate) extern "C" fn unexpected_byte(_: u32) -> u8 {
    panic!("unexpected map byte query")
}
#[cfg(test)]
pub(crate) extern "C" fn unexpected_word(_: u32) -> u16 {
    panic!("unexpected map word query")
}
#[cfg(test)]
pub(crate) extern "C" fn unexpected_predicate(_: u32) -> bool {
    panic!("unexpected map predicate query")
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
