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

/// Synchronous borrowed bytes, including embedded NULs. The producing entry
/// specifies lifetime; no native callback may retain this view after mutation.
#[repr(C)]
pub struct ByteSpan {
    /// Readable bytes for the producing entry's specified lifetime.
    pub data: *const u8,
    /// Full byte count; embedded NULs do not terminate the span.
    pub size: usize,
}

/// Allocation descriptor acquired once by an audited native entry. The base
/// pointer comes from `Tile::base_tiles.get()`, with the full allocation's provenance.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TileTypeAllocation {
    /// Allocation-derived bytes; null selects the original callback backend.
    pub base: *const u8,
    /// Actual native `Map::Size()` at acquisition, not a copied tile snapshot.
    pub count: u32,
}

/// Layout-only shell for the actual native base tile. Never create a reference,
/// read or copy a value of this type; it supplies stride/ABI metadata only.
#[repr(C)]
pub(crate) struct BaseTileLayout {
    pub kind: u8,
    pub height: u8,
    pub m2: u16,
    pub m1: u8,
    pub m3: u8,
    pub m4: u8,
    pub m5: u8,
}

/// One shared reader with callback and audited entry-resolved raw backends.
/// No clone/copy implementation: allocation validity covers this entry-local
/// reader's entire lifetime. No map reference or slice is ever formed.
pub(crate) struct TileTypeAccess {
    allocation: TileTypeAllocation,
    callback: extern "C" fn(u32) -> u8,
}
impl TileTypeAccess {
    /// Default for existing owners/fixtures. Its null allocation folds away the
    /// raw branch after inlining; overrides preserve the original callback result.
    pub(crate) const fn callback(callback: extern "C" fn(u32) -> u8) -> Self {
        Self {
            allocation: TileTypeAllocation {
                base: std::ptr::null(),
                count: 0,
            },
            callback,
        }
    }

    /// Resolve the audited allocation once, retaining only descriptor metadata.
    /// # Safety
    /// A nonnull base must retain provenance for `count * sizeof(TileBase)` live,
    /// initialized bytes, within `isize::MAX`, until this reader is dropped. Map
    /// reset and game-state-mutex yield are excluded throughout the entry. Each
    /// read must be serialized with native writes; native aliases are permitted.
    pub(crate) unsafe fn raw(
        allocation: TileTypeAllocation,
        callback: extern "C" fn(u32) -> u8,
    ) -> Self {
        Self {
            allocation,
            callback,
        }
    }

    /// Read the original high nibble at this observation point. Out-of-range
    /// reads go through the original native accessor/assertion policy; no new
    /// rejection, panic or unchecked in-range-only timing shortcut is added.
    #[inline]
    #[allow(clippy::cast_possible_truncation)]
    pub(crate) fn read(&self, tile: u32) -> u8 {
        if !self.allocation.base.is_null() && tile < self.allocation.count {
            // SAFETY: Constructor contract gives live allocation provenance and
            // serialization. Original map bounds keep the byte offset in isize
            // even on 32-bit targets. Exactly one initialized u8 is read; no
            // Rust/C++ field reference is made or retained across a callback.
            let offset = (tile as usize) * std::mem::size_of::<BaseTileLayout>()
                + std::mem::offset_of!(BaseTileLayout, kind);
            #[cfg(landscape_loop_count)]
            crate::witness::hit(crate::witness::LANDSCAPE + 13);
            unsafe { self.allocation.base.add(offset).read() >> 4 }
        } else {
            #[cfg(landscape_loop_count)]
            crate::witness::hit(crate::witness::LANDSCAPE + 14);
            (self.callback)(tile)
        }
    }
}

/// Default native ABI probe of the same shared reader used by every owner.
/// # Safety
/// For nonnull allocations, uphold `TileTypeAccess::raw`'s validity/serialization
/// contract for this call. The callback is the original noexcept accessor or an
/// equivalent live fixture; it keeps the native out-of-range assertion policy.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_shared_tile_type(
    allocation: TileTypeAllocation,
    callback: extern "C" fn(u32) -> u8,
    tile: u32,
) -> u8 {
    unsafe { TileTypeAccess::raw(allocation, callback) }.read(tile)
}

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
    /// `GetTileOwner`.
    pub tile_owner: extern "C" fn(u32) -> u8,
    /// `GetStationIndex`.
    pub station_index: extern "C" fn(u32) -> u16,
    /// `GetDepotIndex`.
    pub depot_index: extern "C" fn(u32) -> u16,
    /// `IsLevelCrossingTile`.
    pub is_level_crossing: extern "C" fn(u32) -> bool,
    /// `GetTunnelBridgeDirection`.
    pub tunnel_bridge_direction: extern "C" fn(u32) -> u8,
    /// Bridge specification speed at this bridge tile.
    pub bridge_speed: extern "C" fn(u32) -> u16,
    /// `IsWaterTile`: water category and clear subtype (sea, canal or river).
    pub is_water_tile: extern "C" fn(u32) -> bool,
}
impl Services {
    /// The shared callback backend for owners without an audited allocation scope.
    #[inline]
    #[must_use]
    pub fn tile_type(&self, tile: u32) -> u8 {
        TileTypeAccess::callback(self.tile_type).read(tile)
    }

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
    extern "C" fn word(_: u32) -> u16 {
        0
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
        tile_owner: byte,
        station_index: word,
        depot_index: word,
        is_level_crossing: flag,
        tunnel_bridge_direction: byte,
        bridge_speed: word,
        is_water_tile: flag,
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
