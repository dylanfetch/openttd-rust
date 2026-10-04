/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

//! Bounded cross-language layout metadata for the first native 32-bit ABI audit.

use std::mem::{align_of, offset_of, size_of};

macro_rules! layout {
    ($type:ty, $item:expr; $($field:ident),+) => {
        match $item {
            0 => size_of::<$type>(),
            1 => align_of::<$type>(),
            _ => [$(offset_of!($type, $field)),+]
                .get(usize::from($item) - 2)
                .copied()
                .unwrap_or(usize::MAX),
        }
    };
}

pub fn layout(type_id: u8, item: u8) -> usize {
    match type_id {
        0 => {
            layout!(crate::IntegerResult, item; value_bits, length, error_offset, error_length, error_kind)
        }
        1 => layout!(crate::Utf8Encoded, item; bytes, length),
        2 => layout!(crate::Utf8Decoded, item; length, codepoint),
        3 => layout!(crate::LittleEndian, item; bytes),
        4 => layout!(crate::FormattedInteger, item; bytes, length),
        5 => layout!(crate::AlternatingState, item; position, next_after, current_after),
        6 => layout!(crate::AlternatingStep, item; state, movement),
        7 => layout!(crate::ConsumerBound, item; length, position, shortfall),
        8 => layout!(crate::ConsumerByte, item; value_bits, length),
        9 => layout!(crate::ConsumerMatch, item; length, matched),
        10 => layout!(crate::ConsumerSeparator, item; result_length, consumed_length),
        11 => layout!(crate::EncodedDescriptor, item; value, bytes, length, kind),
        12 => {
            layout!(crate::EncodedView, item; bytes, length, diagnostic_count, assertion_offset, assertion_length, assertion_codepoint, apply, assertion_kind)
        }
        13 => layout!(crate::EncodedDiagnostic, item; offset, length, tail_length, kind),
        14 => {
            layout!(crate::SpiralState, item; max_radius, extent, cur_radius, position, x, y, direction)
        }
        15 => layout!(crate::byte_strings::TrimResult, item; offset, length),
        16 => {
            layout!(crate::HistoryDescriptor, item; child, periods, records, first, last, division, total_division, child_periods, child_division)
        }
        17 => layout!(crate::HistoryStep, item; kind, count, first, last, target, token, value),
        18 => {
            layout!(crate::station_cargo::Collector, item; amount, previous, last_key, other, origin, selector, finalized)
        }
        19..=21 => crate::crypto_primitives::abi_layout(type_id, item),
        22 => crate::blake2b::abi_layout(item),
        23 => layout!(crate::packet::State, item; limit, position),
        24 => layout!(crate::packet::Frame, item; message, payload),
        25 => crate::x25519::abi_layout(item),
        26 => layout!(crate::string_validation::Step, item; consumed, output, stopped),
        27 => layout!(crate::string_validation::Write, item; position, accepted),
        29 => layout!(crate::script_list::control::Input, item; a, b, flag, kind),
        30 => layout!(crate::script_list::control::Action, item; a, kind),
        34 => {
            layout!(crate::linkgraph::InputNode, item; supply, demand, station, x, y, edge_begin, edge_count)
        }
        35 => layout!(crate::linkgraph::InputEdge, item; capacity, travel_time, dest),
        36 => {
            layout!(crate::linkgraph::Settings, item; accuracy, demand_distance, demand_size, saturation, distribution, express, map_max_x, map_max_y, runtime)
        }
        37 => {
            layout!(crate::linkgraph::OutputShare, item; node, origin, via, cumulative, unrestricted, has_share)
        }
        38 => layout!(crate::trees::Action, item; kind, tile, a, b, cost),
        39 => {
            layout!(crate::effect::View, item; x, y, z, sprite, progress, spritenum, subtype, ambient)
        }
        40 => 0, // Retired effect invocation cursor.
        41 => {
            layout!(crate::effect::Leaves, item; observe, write, viewport, sound, animated)
        }
        42 => {
            layout!(crate::services::Services, item; context, random, observe_tile, write_tile, trig, industry)
        }
        44 => layout!(crate::disaster::State, item; image_override, target, state, flags),
        45 => layout!(crate::disaster::Action, item; kind, id, other, a, b, c, d),
        46 => layout!(crate::water_regions::Patch, item; x, y, label),
        47 => layout!(crate::water_regions::Snapshot, item; edges, labels, patches, aqueducts),
        48 => layout!(crate::water_regions::Leaves, item; tracks, follow, aqueduct, debug),
        90 => {
            layout!(crate::rail_yapf::Settings, item; max_nodes, firstred, firstred_exit, lastred, lastred_exit, station, slope, curve45, curve90, depot_reverse, crossing, lookahead, p0, p1, p2, pbs_cross, pbs_station, pbs_back, doubleslip, longer, longer_tile, shorter, shorter_tile, firstred_eol)
        }
        91 => {
            layout!(crate::rail_yapf::Tile, item; flags, other_end, station, railtype, tracks, reserved, station_track, tunnel_dir, uphill, flat_ramp, signal_along, signal_against, signal_green, signal_type, oneway)
        }
        92 => {
            layout!(crate::rail_yapf::Follow, item; tile, skipped, min_speed, max_speed, dirs, followed, error, station)
        }
        93 => {
            layout!(crate::rail_yapf::Leaves, item; train, tile, follow, safe, free, compatible_station, platform_length, closest_station, destination_dirs, origin, write, output, debug)
        }
        94 => {
            layout!(crate::rail_yapf::Input, item; context, settings, map_x, tile, max_cost, desync, kind, td, override_railtype, forbid90, reserve)
        }
        95 => {
            layout!(crate::rail_yapf::Step, item; tile, destination, target_tile, best_length, action, td, found, reverse, value, target_td, target_okay)
        }
        97 => {
            layout!(crate::rail_yapf::Train, item; compatible, all_compatible, tile, rear_tile, virtual_tile, rear_virtual_tile, dest_tile, length, speed, order_destination, td, rear_td, wormhole, rear_wormhole, order, nearest_depot, complex_waypoint)
        }
        _ => usize::MAX,
    }
}
