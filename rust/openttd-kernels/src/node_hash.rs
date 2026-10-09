/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Fixed, unseeded hasher for lookup-only YAPF node maps.
//! Only use it for maps whose iteration order never affects results.
use std::collections::HashMap;
use std::hash::{BuildHasherDefault, Hasher};

/// Fx-style multiply-rotate hasher; all arithmetic wraps by design.
#[derive(Default, Clone, Copy)]
pub struct NodeHasher(u64);

const SEED: u64 = 0x51_7c_c1_b7_27_22_0a_95;

impl NodeHasher {
    fn add(&mut self, word: u64) {
        self.0 = (self.0.rotate_left(5) ^ word).wrapping_mul(SEED);
    }
}

impl Hasher for NodeHasher {
    fn write(&mut self, bytes: &[u8]) {
        for &byte in bytes {
            self.add(u64::from(byte));
        }
    }
    fn write_u8(&mut self, i: u8) {
        self.add(u64::from(i));
    }
    fn write_u32(&mut self, i: u32) {
        self.add(u64::from(i));
    }
    fn write_u64(&mut self, i: u64) {
        self.add(i);
    }
    fn write_usize(&mut self, i: usize) {
        self.add(i as u64);
    }
    fn finish(&self) -> u64 {
        self.0
    }
}

/// Lookup-only map with the fixed hasher.
pub type NodeMap<K, V> = HashMap<K, V, BuildHasherDefault<NodeHasher>>;
