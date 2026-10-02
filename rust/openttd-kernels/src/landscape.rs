/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

const TILE_SIZE: i32 = 16;
const TILE_HEIGHT: i32 = 8;
const HALF_TILE: u8 = 0x20;
const STEEP: u8 = 0x10;

/// The caller bounds x and y to 0..16. Every selected arithmetic expression
/// then lies in 0..=32 and each returned height in 0..=16, so signed arithmetic
/// and the final checked unsigned conversion reproduce the C++ calculations.
pub(super) fn partial_pixel_z(x: i32, y: i32, corners: u8) -> Option<u32> {
    if corners & HALF_TILE != 0 {
        let leveled = match (corners >> 6) & 3 {
            0 => x > y,
            1 => x + y >= TILE_SIZE,
            2 => x <= y,
            3 => x + y < TILE_SIZE,
            _ => unreachable!(), // A two-bit value is always 0..=3.
        };
        if leveled {
            // The complete slope has HALF_TILE set, so it is never SLOPE_FLAT.
            return Some(if corners & STEEP != 0 { 16 } else { 8 });
        }
    }

    // Clear all half-tile bits even if the half-tile flag itself was unset,
    // exactly as RemoveHalftileSlope does. Validate only after the early return.
    let z = match corners & 0x1f {
        0x00 => 0, // Flat.
        0x08 => {
            if x + y <= TILE_SIZE {
                (TILE_SIZE - x - y) >> 1
            } else {
                0
            }
        }
        0x04 => {
            if y >= x {
                (1 + y - x) >> 1
            } else {
                0
            }
        }
        0x02 => {
            if x + y >= TILE_SIZE {
                (1 + x + y - TILE_SIZE) >> 1
            } else {
                0
            }
        }
        0x01 => {
            if x >= y {
                (x - y) >> 1
            } else {
                0
            }
        }
        0x0c => (TILE_SIZE - x) >> 1,
        0x06 => (y + 1) >> 1,
        0x03 => (x + 1) >> 1,
        0x09 => (TILE_SIZE - y) >> 1,
        0x0d => {
            if x + y >= TILE_SIZE {
                TILE_HEIGHT - ((1 + x + y - TILE_SIZE) >> 1)
            } else {
                TILE_HEIGHT
            }
        }
        0x0e => {
            if y < x {
                TILE_HEIGHT - ((x - y) >> 1)
            } else {
                TILE_HEIGHT
            }
        }
        0x07 => {
            if x + y <= TILE_SIZE {
                TILE_HEIGHT - ((TILE_SIZE - x - y) >> 1)
            } else {
                TILE_HEIGHT
            }
        }
        0x0b => {
            if x < y {
                TILE_HEIGHT - ((1 + y - x) >> 1)
            } else {
                TILE_HEIGHT
            }
        }
        0x0a => {
            if x + y < TILE_SIZE {
                (TILE_SIZE - x - y) >> 1
            } else {
                (1 + x + y - TILE_SIZE) >> 1
            }
        }
        0x05 => {
            if x >= y {
                (x - y) >> 1
            } else {
                (1 + y - x) >> 1
            }
        }
        0x0f => TILE_HEIGHT,
        0x1d => (TILE_SIZE - x + TILE_SIZE - y) >> 1,
        0x1e => (TILE_SIZE + 1 + y - x) >> 1,
        0x17 => (1 + x + y) >> 1,
        0x1b => (TILE_SIZE + x - y) >> 1,
        _ => return None,
    };
    u32::try_from(z).ok()
}

#[cfg(test)]
mod tests {
    use crate::openttd_rust_get_partial_pixel_z as height;

    #[test]
    fn direct_flat_and_elevated_gap() {
        for x in 0..16 {
            for y in 0..16 {
                assert_eq!(height(x, y, 0), 0);
                assert_eq!(height(x, y, 0x0f), 8);
            }
        }
    }

    #[test]
    fn half_tile_precedes_invalid_base_validation() {
        // Invalid steep-flat base: northern leveled half still has defined height.
        assert_eq!(height(0, 0, 0xf0), 16);
        assert_eq!(height(15, 15, 0xf0), u32::MAX);
        // Half-flat maximum is eight, even though the removed base is flat.
        assert_eq!(height(0, 0, 0xe0), 8);
        assert_eq!(height(15, 15, 0xe0), 0);
    }

    #[test]
    fn invalid_inputs_report_fatal_sentinel() {
        assert_eq!(height(0, 0, 0x10), u32::MAX);
        assert_eq!(height(-1, 0, 0), u32::MAX);
        assert_eq!(height(0, 16, 0), u32::MAX);
    }
}
