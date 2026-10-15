/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Flooding ownership, including live vehicle-hash traversal and nested command
//! results. Native leaves are noexcept. No Rust borrow denotes a native pool/map
//! object across calls; each opaque iterator is native-constructed stack storage.
#![allow(
    unsafe_code,
    missing_docs,
    clippy::too_many_lines,
    clippy::cast_possible_truncation,
    clippy::cast_sign_loss,
    clippy::cast_possible_wrap,
    clippy::collapsible_match
)]
use crate::{
    ground::{Call, ClearServices, CostServices, FoundationSlope, RailServices},
    services::{
        self, ClearGroundType, FloodingBehaviour, Foundation, RailGroundType, Services, Slope,
        Track, TrackBits, TreeGround,
    },
    vehicle::{VehicleRef, VehicleServices, VehicleType},
};
use std::mem::MaybeUninit;

#[repr(C)]
pub struct AirportGround {
    pub station: *const crate::station::Station,
    pub tile: u32,
    pub width: u16,
    pub height: u16,
}
#[repr(C)]
pub struct TileVehicleCursor {
    pub storage: [usize; 2],
}
#[repr(C)]
pub struct FloodVictim {
    pub status: u8,
    pub kind: VehicleType,
}
#[repr(C)]
pub struct AircraftFloodPose {
    pub tile: u32,
    pub z: i32,
    pub subtype: u8,
}
#[repr(C)]
pub struct GroundFloodPose {
    pub z: i32,
    pub first: *mut crate::vehicle::OpenTTDVehicle,
}
#[repr(C)]
pub struct WaterServices {
    pub rail: *const RailServices,
    pub clear: *const ClearServices,
    pub vehicles: *const VehicleServices,
    pub costs: *const CostServices,
    pub ambient: extern "C" fn(u32),
    pub non_flooding: extern "C" fn(u32) -> bool,
    pub set_non_flooding: extern "C" fn(u32, bool),
    pub coast: extern "C" fn(u32) -> bool,
    pub tree_ground: extern "C" fn(u32) -> TreeGround,
    pub set_tree_ground: extern "C" fn(u32, TreeGround, u32),
    pub make_clear: extern "C" fn(u32, ClearGroundType, u32),
    pub make_shore: extern "C" fn(u32),
    pub make_sea: extern "C" fn(u32),
    pub foundation: extern "C" fn(u32) -> FoundationSlope,
    pub buoy_or_dock: extern "C" fn(u32) -> bool,
    pub canal_or_river: extern "C" fn(u32) -> bool,
    pub possible_docking: extern "C" fn(u32) -> bool,
    pub check_docking: extern "C" fn(u32),
    pub invalidate_region: extern "C" fn(u32),
    pub update_signals: extern "C" fn(),
    pub set_company: extern "C" fn(u8),
    pub tracks: extern "C" fn(u32) -> TrackBits,
    pub remove_rail: extern "C" fn(*mut services::CommandCost, u32, Track),
    pub apply_foundation: extern "C" fn(Foundation, Slope) -> FoundationSlope,
    pub iterator_begin: extern "C" fn(*mut TileVehicleCursor, u32),
    pub iterator_current: extern "C" fn(*mut TileVehicleCursor) -> Option<VehicleRef>,
    pub iterator_next: extern "C" fn(*mut TileVehicleCursor),
    pub iterator_end: extern "C" fn(*mut TileVehicleCursor),
    pub airport_tile: extern "C" fn(u32) -> bool,
    pub airport: extern "C" fn(u32) -> AirportGround,
    pub airport_member: extern "C" fn(*const crate::station::Station, u32) -> bool,
    pub airport_z: extern "C" fn(u32) -> i32,
    pub victim: extern "C" fn(VehicleRef) -> FloodVictim,
    pub aircraft_pose: extern "C" fn(VehicleRef) -> AircraftFloodPose,
    pub ground_pose: extern "C" fn(VehicleRef) -> GroundFloodPose,
    pub crash: extern "C" fn(VehicleRef) -> u32,
    pub ai_event: extern "C" fn(VehicleRef, u32),
    pub game_event: extern "C" fn(VehicleRef, u32),
    pub news: extern "C" fn(VehicleRef, u32),
    pub bridge_height: extern "C" fn(u32) -> i32,
}
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
const FLOOD_FROM: [u8; 15] = [
    0b1010_1010,
    0b0000_1010,
    0b1000_0010,
    0b0000_0010,
    0b1010_0000,
    0,
    0b1000_0000,
    0b1000_0011,
    0b0010_1000,
    0b0000_1000,
    0,
    0b0000_1110,
    0b0010_0000,
    0b0011_1000,
    0b1110_0000,
];
fn one_corner(slope: u8) -> bool {
    matches!(slope, 1 | 2 | 4 | 8)
}
fn three_corners(slope: u8) -> bool {
    matches!(slope, 7 | 11 | 13 | 14)
}
fn offset(tile: u32, direction: usize, size_x: u32) -> u32 {
    let (dx, dy) = OFFSETS[direction];
    tile.wrapping_add_signed(dx)
        .wrapping_add(size_x.wrapping_mul(dy as u32))
}
fn wrap(tile: u32, direction: usize, size_x: u32, size_y: u32) -> u32 {
    let (dx, dy) = OFFSETS[direction];
    // Native map dimensions are powers of two (TileX/TileY use mask/shift).
    let x = (tile & (size_x - 1)).wrapping_add_signed(dx);
    let y = (tile >> size_x.trailing_zeros()).wrapping_add_signed(dy);
    if x >= size_x || y >= size_y {
        u32::MAX
    } else {
        y * size_x + x
    }
}
struct World<'a> {
    w: &'a WaterServices,
    s: &'a Services,
    r: &'a RailServices,
    c: &'a ClearServices,
    v: &'a VehicleServices,
    costs: &'a CostServices,
}
impl World<'_> {
    unsafe fn new(w: *const WaterServices, s: *const Services) -> Self {
        // SAFETY: Entry contract supplies immutable live nested tables.
        unsafe {
            Self {
                w: &*w,
                s: &*s,
                r: &*(*w).rail,
                c: &*(*w).clear,
                v: &*(*w).vehicles,
                costs: &*(*w).costs,
            }
        }
    }
    fn behaviour(&self, tile: u32) -> u8 {
        match (self.s.tile_type)(tile) {
            6 => {
                if (self.w.coast)(tile) {
                    return if one_corner((self.s.tile_slope)(tile)) {
                        1
                    } else {
                        3
                    };
                }
                u8::from((self.c.water_class)(tile).0 == 0)
            }
            5 | 8 | 10 => u8::from((self.c.water_class)(tile).0 == 0),
            1 => {
                if (self.r.ground)(tile).0 == 13 {
                    if one_corner((self.s.tile_slope)(tile)) {
                        1
                    } else {
                        3
                    }
                } else {
                    0
                }
            }
            4 => {
                if (self.w.tree_ground)(tile).0 == 3 {
                    3
                } else {
                    0
                }
            }
            7 => 1,
            _ => 0,
        }
    }
    fn result(&self, call: *mut Call, out: *const services::CommandCost) -> bool {
        // SAFETY: prepare constructed this result; release ends its native lifetime.
        let success = unsafe { ((*self.costs.costs).success)(out) };
        (self.costs.release)(call);
        success
    }
    fn clear(&self, tile: u32, call: *mut Call) -> bool {
        let out = (self.costs.prepare)(call);
        // SAFETY: Immutable shared table; native storage outlives command/reentry.
        unsafe {
            ((*self.costs.commands).landscape_clear)(out, tile, 1);
        }
        self.result(call, out)
    }
    fn half_flood(&self, tile: u32, call: *mut Call) -> bool {
        let mut flooded = false;
        if (self.r.ground)(tile).0 == 13 {
            return false;
        }
        let slope = (self.s.tile_slope)(tile);
        let mut tracks = (self.w.tracks)(tile).0;
        if one_corner(slope) {
            let corner = (slope.trailing_zeros() ^ 2) as usize;
            let lower = [16, 8, 32, 4][corner];
            let remove = lower & tracks;
            if remove != 0 {
                let company = (self.s.current_company)();
                (self.w.set_company)(17);
                let out = (self.costs.prepare)(call);
                (self.w.remove_rail)(out, tile, Track(remove.trailing_zeros() as u8));
                flooded = self.result(call, out);
                (self.w.set_company)(company);
                if !flooded {
                    return false;
                }
                crate::ground::hit(crate::ground::Witness::HalfRailRemove);
                tracks &= !remove;
                if tracks == 0 {
                    (self.w.make_shore)(tile);
                    (self.s.mark_dirty)(tile);
                    return true;
                }
            }
            if (5..10).contains(&(self.r.foundation)(Slope(slope), TrackBits(tracks)).0) {
                flooded = true;
                (self.r.set_ground)(tile, RailGroundType(13));
                (self.s.mark_dirty)(tile);
                crate::ground::hit(crate::ground::Witness::HalfRail);
            }
        } else {
            let foundation = (self.w.apply_foundation)(
                (self.r.foundation)(Slope(slope), TrackBits(tracks)),
                Slope(slope),
            );
            if foundation.z == 0
                && (foundation.slope.0 & 16 != 0 || three_corners(foundation.slope.0))
            {
                flooded = true;
                (self.r.set_ground)(tile, RailGroundType(13));
                (self.s.mark_dirty)(tile);
                crate::ground::hit(crate::ground::Witness::HalfRail);
            }
        }
        flooded
    }
    fn crash(&self, v: VehicleRef) {
        let victims = (self.w.crash)(v);
        (self.w.ai_event)(v, victims);
        (self.w.game_event)(v, victims);
        (self.w.news)(v, victims);
        (self.v.large_explosion)(v);
        if (self.s.disaster_sound)() {
            (self.v.play_sound)(v, 16);
        }
        crate::ground::hit(crate::ground::Witness::VehicleCrash);
    }
    fn victim(&self, v: VehicleRef, z: i32) {
        // Pose reads are separated only by pure map tests in the source. No
        // mutating callback intervenes, and no pose is reused after Crash.
        let inputs = (self.w.victim)(v);
        if inputs.status & 128 != 0 {
            crate::ground::hit(crate::ground::Witness::CrashedSkip);
            return;
        }
        match inputs.kind.0 {
            3 => {
                let pose = (self.w.aircraft_pose)(v);
                let tile = pose.tile;
                if !(self.w.airport_tile)(tile) || (self.s.tile_max_z)(tile) != 0 {
                    return;
                }
                if pose.subtype == 4 {
                    return;
                }
                if pose.z != (self.w.airport_z)(tile) {
                    return;
                }
                self.crash(v);
            }
            0 | 1 => {
                let pose = (self.w.ground_pose)(v);
                if pose.z <= z {
                    // SAFETY: Source dereferences First only after this z test.
                    // Vehicle construction/afterload establishes a nonnull head
                    // before hash insertion; the raw record adds no precondition
                    // to parts rejected by altitude. No callback intervenes.
                    self.crash(VehicleRef(unsafe {
                        std::ptr::NonNull::new_unchecked(pose.first)
                    }));
                }
            }
            _ => {}
        }
    }
    fn vehicles_on_tile(&self, tile: u32, z: i32) {
        let mut cursor = MaybeUninit::<TileVehicleCursor>::uninit();
        let cursor = cursor.as_mut_ptr();
        (self.w.iterator_begin)(cursor, tile);
        while let Some(v) = (self.w.iterator_current)(cursor) {
            self.victim(v, z);
            // Original iterator reads current->hash_tile_next AFTER Crash, both
            // event queues, news and explosion creation have all returned.
            (self.w.iterator_next)(cursor);
        }
        (self.w.iterator_end)(cursor);
    }
    fn flood_vehicles(&self, tile: u32, size_x: u32) {
        if (self.w.airport_tile)(tile) {
            let airport = (self.w.airport)(tile);
            for y in 0..u32::from(airport.height) {
                for x in 0..u32::from(airport.width) {
                    let target = airport.tile.wrapping_add(y * size_x).wrapping_add(x);
                    if (self.w.airport_member)(airport.station, target) {
                        crate::ground::hit(crate::ground::Witness::AirportMember);
                        self.vehicles_on_tile(target, 0);
                    }
                }
            }
            return;
        }
        if !(self.s.is_bridge)(tile) {
            self.vehicles_on_tile(tile, 0);
            return;
        }
        let end = (self.s.other_tunnel_end)(tile);
        let z = (self.w.bridge_height)(tile);
        crate::ground::hit(crate::ground::Witness::BridgeStart);
        self.vehicles_on_tile(tile, z);
        crate::ground::hit(crate::ground::Witness::BridgeEnd);
        self.vehicles_on_tile(end, z);
    }
    fn dirty_neighbours(&self, tile: u32, size_x: u32) {
        for direction in 0..8 {
            let target = offset(tile, direction, size_x);
            if (self.s.tile_valid)(target)
                && (self.s.tile_type)(target) == 6
                && (self.w.canal_or_river)(target)
            {
                (self.s.mark_dirty)(target);
            }
        }
    }
    fn flood(&self, tile: u32, size_x: u32, call: *mut Call) {
        let company = (self.s.current_company)();
        (self.w.set_company)(17);
        let slope = (self.s.tile_slope)(tile);
        let flooded = if slope != 0 {
            match (self.s.tile_type)(tile) {
                1 => {
                    if (self.s.is_plain_rail)(tile) {
                        self.flood_vehicles(tile, size_x);
                        self.half_flood(tile, call)
                    } else {
                        false
                    }
                }
                4 if !one_corner(slope) => {
                    (self.w.set_tree_ground)(tile, TreeGround(3), 3);
                    (self.s.mark_dirty)(tile);
                    true
                }
                0 | 4 => {
                    if self.clear(tile, call) {
                        (self.w.make_shore)(tile);
                        (self.s.mark_dirty)(tile);
                        true
                    } else {
                        false
                    }
                }
                _ => false,
            }
        } else {
            self.flood_vehicles(tile, size_x);
            if self.clear(tile, call) {
                (self.w.make_sea)(tile);
                (self.s.mark_dirty)(tile);
                true
            } else {
                false
            }
        };
        if flooded {
            self.dirty_neighbours(tile, size_x);
            (self.w.update_signals)();
            if (self.w.possible_docking)(tile) {
                (self.w.check_docking)(tile);
            }
            (self.w.invalidate_region)(tile);
            crate::ground::hit(crate::ground::Witness::FloodSuccess);
        } else {
            crate::ground::hit(crate::ground::Witness::FloodFail);
        }
        (self.w.set_company)(company);
    }
    fn dry(&self, tile: u32, call: *mut Call) {
        let company = (self.s.current_company)();
        (self.w.set_company)(17);
        match (self.s.tile_type)(tile) {
            1 => {
                let ground = match (self.w.tracks)(tile).0 {
                    4 => 10,
                    8 => 11,
                    16 => 8,
                    32 => 9,
                    _ => unreachable!("original drying rail track"),
                };
                (self.r.set_ground)(tile, RailGroundType(ground));
                (self.s.mark_dirty)(tile);
                crate::ground::hit(crate::ground::Witness::DryRail);
            }
            4 => {
                (self.w.set_tree_ground)(tile, TreeGround(0), 3);
                (self.s.mark_dirty)(tile);
                crate::ground::hit(crate::ground::Witness::DryTree);
            }
            6 => {
                if self.clear(tile, call) {
                    (self.w.make_clear)(tile, ClearGroundType(0), 3);
                    (self.s.mark_dirty)(tile);
                    crate::ground::hit(crate::ground::Witness::DryCoast);
                }
            }
            _ => unreachable!("original drying tile type"),
        }
        (self.w.set_company)(company);
    }
    fn tile_loop(&self, tile: u32, size_x: u32, size_y: u32, call: *mut Call) {
        match self.behaviour(tile) {
            1 => {
                let mut continue_flooding = false;
                for direction in 0..8 {
                    let target = wrap(tile, direction, size_x, size_y);
                    if !(self.s.tile_valid)(target) {
                        continue;
                    }
                    let kind = (self.s.tile_type)(target);
                    if kind == 6 {
                        continue;
                    }
                    if kind == 5 && (self.w.buoy_or_dock)(target) {
                        continue;
                    }
                    continue_flooding = true;
                    if kind == 4 && (self.w.tree_ground)(target).0 == 3 {
                        continue;
                    }
                    let foundation = (self.w.foundation)(target);
                    if foundation.z > 0 {
                        continue;
                    }
                    let mask = FLOOD_FROM[usize::from(foundation.slope.0 & 15)];
                    if mask & (1 << ((direction + 4) & 7)) == 0 {
                        continue;
                    }
                    self.flood(target, size_x, call);
                }
                if !continue_flooding && (self.s.tile_type)(tile) == 6 {
                    (self.w.set_non_flooding)(tile, true);
                }
            }
            3 => {
                let mask = FLOOD_FROM[usize::from((self.w.foundation)(tile).slope.0 & 15)];
                for direction in 0..8 {
                    if mask & (1 << direction) == 0 {
                        continue;
                    }
                    let target = wrap(tile, direction, size_x, size_y);
                    if target == u32::MAX {
                        continue;
                    }
                    if matches!(self.behaviour(target), 1 | 2) {
                        return;
                    }
                }
                self.dry(tile, call);
            }
            _ => {}
        }
    }
}
/// # Safety
/// Live immutable service tables and an independent native lazy-result context.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_water_tile_loop(
    tile: u32,
    size_x: u32,
    size_y: u32,
    ambient_callback: bool,
    call: *mut Call,
    w: *const WaterServices,
    s: *const Services,
) {
    // SAFETY: Same synchronous table contract as the in-crate scheduler entry.
    unsafe { water_tile_loop(tile, size_x, size_y, ambient_callback, call, w, s) };
}
#[allow(clippy::inline_always)]
#[inline(always)]
pub(crate) unsafe fn water_tile_loop(
    tile: u32,
    size_x: u32,
    size_y: u32,
    ambient_callback: bool,
    call: *mut Call,
    w: *const WaterServices,
    s: *const Services,
) {
    // SAFETY: Live immutable tables; no owner access spans ambient sound.
    let (water, shared) = unsafe { (&*w, &*s) };
    if (shared.tile_type)(tile) == 6 {
        if ambient_callback {
            (water.ambient)(tile);
        }
        if (water.non_flooding)(tile) {
            return;
        }
    }
    // SAFETY: Resolve only the tables reached after the original early return.
    unsafe { World::new(w, s) }.tile_loop(tile, size_x, size_y, call);
}
/// # Safety
/// Same table/context contract; original native plain-rail assertion is retained.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_flood_halftile(
    tile: u32,
    call: *mut Call,
    w: *const WaterServices,
    s: *const Services,
) -> bool {
    // SAFETY: Serialized native entry; no borrowed state crosses commands.
    unsafe { World::new(w, s) }.half_flood(tile, call)
}
/// # Safety
/// Live immutable service tables; valid original tile argument.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_flooding_behaviour(
    tile: u32,
    w: *const WaterServices,
    s: *const Services,
) -> FloodingBehaviour {
    // SAFETY: Serialized native entry, observations are copied.
    FloodingBehaviour(unsafe { World::new(w, s) }.behaviour(tile))
}
/// # Safety
/// Live immutable service tables for the synchronous no-unwind invocation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_dirty_water_neighbours(
    tile: u32,
    size_x: u32,
    w: *const WaterServices,
    s: *const Services,
) {
    // SAFETY: Serialized native entry; no map references are formed.
    unsafe { World::new(w, s) }.dirty_neighbours(tile, size_x);
}
/// # Safety
/// Live immutable service tables for the synchronous no-unwind invocation.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn openttd_rust_clear_neighbour_flooding(
    tile: u32,
    size_x: u32,
    w: *const WaterServices,
    s: *const Services,
) {
    // SAFETY: Serialized native entry; no map references are formed.
    let world = unsafe { World::new(w, s) };
    for direction in 0..8 {
        let target = offset(tile, direction, size_x);
        if (world.s.tile_valid)(target) && (world.s.tile_type)(target) == 6 {
            (world.w.set_non_flooding)(target, false);
            crate::ground::hit(crate::ground::Witness::Wakeup);
        }
    }
}
