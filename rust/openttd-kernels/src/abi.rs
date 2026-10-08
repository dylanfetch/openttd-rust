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

// Keep ABI IDs in one registry so duplicate match arms remain compiler-checked.
#[allow(clippy::too_many_lines)]
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
        80 => layout!(crate::road::PathElement, item; trackdir, tile),
        81 => {
            layout!(crate::road::View, item; r#type, first, next, previous, tile, dest, x, y, z, direction, speed, tick, running, day, order_time, progress, status, owner, engine, last_station, order_destination, order_type, order_max_speed, breakdown, max_track_speed, length, total_length, roadtype, front, articulated, tram, bus, order_nonstop)
        }
        82 => layout!(crate::road::Leaves, item; observe, write, leaf, owner, nearby),
        83 => layout!(crate::road::Action, item; op, id, a, b, c),
        44 => layout!(crate::disaster::State, item; image_override, target, state, flags),
        45 => layout!(crate::disaster::Action, item; kind, id, other, a, b, c, d),
        46 => layout!(crate::water_regions::Patch, item; x, y, label),
        47 => layout!(crate::water_regions::Snapshot, item; edges, labels, patches, aqueducts),
        48 => layout!(crate::water_regions::Leaves, item; tracks, follow, aqueduct, debug),
        49 => {
            layout!(crate::cargo_payment::Spec, item; payment, valid, callback, periods1, periods2)
        }
        50 => {
            layout!(crate::cargo_payment::Fields, item; front, route_profit, visual_profit, visual_transfer)
        }
        51 => {
            layout!(crate::cargo_payment::Services, item; spec, callback, near, station_read, industry_read, industry_write, refuses, accept, statistics, monitor, subsidised, industry_effect, vehicle_read, settle, feeder, setting)
        }
        52 => {
            layout!(crate::ship_yapf::Input, item; map_x, map_y, tile, dest_tile, curve90, curve45, max_speed, dest_dirs, reverse_dirs, trackdir, ocean_frac, canal_frac, station, unit_number)
        }
        53 => {
            layout!(crate::ship_yapf::Leaves, item; destination, follow, tile, patch, visit_new, visit_next, visit_destroy, debug)
        }
        54 => layout!(crate::ship_yapf::Follow, item; tile, skipped, dirs, followed),
        55 => layout!(crate::ship_yapf::Tile, item; ships, docking, sea, lock_middle, destination),
        56 => layout!(crate::ship_yapf::Result, item; direction, found, origin, stats),
        60 => layout!(crate::town::Action, item; kind, town, tile, a, b, c, d, cost),
        61 => layout!(crate::town::Leaves, item; observe, leaf, state, stations),
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
        100 => {
            layout!(crate::road_yapf::Input, item; map_x, map_y, tile, dest_tile, max_nodes, slope, crossing, stop, occupied, bay, curve, display_speed, order_destination, order_speed, order_type, bus, articulated, trackdir)
        }
        101 => {
            layout!(crate::road_yapf::Tile, item; occupied, length, station, r#type, station_type, depot, depot_dir, crossing, waypoint, drive_through, continuation, busy_bays)
        }
        102 => {
            layout!(crate::road_yapf::Follow, item; tile, skipped, max_speed, min_speed, dirs, followed)
        }
        103 => {
            layout!(crate::road_yapf::Area, item; tile, width, height, valid, stop, drive_through, next)
        }
        104 => layout!(crate::road_yapf::Leaves, item; tile, follow, tracks, height, closest, area),
        105 => {
            layout!(crate::road_yapf::Result, item; tile, cost, direction, found, rounds, open, closed, calcs, distance)
        }
        110 => {
            layout!(crate::station_service::CargoFields, item; max_waiting_cargo, status, time_since_pickup, rating, last_speed, last_age, amount_fract)
        }
        111 => {
            layout!(crate::station_service::Fields, item; always_accepted, delete_ctr, time_since_load, time_since_unload, last_vehicle_type)
        }
        112 => {
            layout!(crate::station_service::World, item; get, owner, read, effect, rating_callback, tiles, tile_next, tile_destroy, accept_tile, truncate, truncate_next, truncate_destroy, random)
        }
        113 => {
            layout!(crate::station_service::Edge, item; destination, last_update, unrestricted, restricted, distance, node)
        }
        114 => {
            layout!(crate::station_service::Links, item; graph, read, edge, effect, order_list, order_read, order_vehicle, next_vehicle, vehicle_read, refresh, reroute)
        }
        115 => {
            layout!(crate::station_service::Loading, item; read, write, next, next_stations, next_stations_destroy, cargo, load_callback, payment, effect, refit)
        }

        160 => {
            layout!(crate::train_reservation::View, item; tile, dest, next, destination, last_station, direction, order, num_orders, order_index, suppress, nearest)
        }
        161 => {
            layout!(crate::train_reservation::Follow, item; old_tile, new_tile, skipped, dirs, old_td, exitdir, tunnel, bridge, station, error)
        }
        162 => layout!(crate::train_reservation::Pbs, item; tile, other, td, okay),
        163 => {
            layout!(crate::train_reservation::Step, item; value, action, id, tile, final_dest, td, dir, tracks, reserve, found, got, okay)
        }
        164 => {
            layout!(crate::train_reservation::Leaves, item; observe, leaf, owner, follow, origin)
        }
        151 => {
            layout!(crate::train::View, item; id,first,next,previous,next_unit,last,tile,dest,x,y,z,order_time,power,weight,length,total_length,max_speed,max_track_speed,speed,gv_flags,cargo_cap,refit_cap,engine,first_engine,order_destination,last_station,direction,status,tick,running,day,progress,subspeed,acceleration,order,nonstop,breakdown,front,free_wagon,articulated,engine_part,multiheaded,owner,vis_effect)
        }
        152 => layout!(crate::train::Leaves, item; observe,write,leaf,owner,nearby),
        153 => layout!(crate::train::Action, item; op,id,a,b,c),
        130 => {
            layout!(crate::industry::Fields, item; valid_history, last_prod_year, counter, prod_level, was_cargo_delivered, ctlflags)
        }
        131 => {
            layout!(crate::industry::BuildFields, item; probability, min_number, target_count, max_wait, wait_count)
        }
        132 => {
            layout!(crate::industry::BuilderFields, item; builddata, wanted_inds, daily_counter, daily_increment, sound_tile, sound_ctr)
        }
        133 => layout!(crate::industry::Slots, item; data, size),
        134 => layout!(crate::industry::Produced, item; cargo, waiting, rate, history),
        135 => {
            layout!(crate::industry::Accepted, item; cargo, waiting, accumulated_waiting, last_accepted, history)
        }
        136 => {
            layout!(crate::industry::Observation, item; owner, tile, behaviour, id, width, height, callbacks, sound_count, life, original, minimal_cargo, kind, up_text, down_text, closure_text)
        }
        137 => layout!(crate::industry::Services, item; observe, next, setting, world),
        138 => {
            layout!(crate::industry::ProductionResult, item; subtract, add, again, cargo_input, cargo_output, version, num_input, num_output, present)
        }

        180 => {
            layout!(crate::aircraft::State, item; cached_max_range_sqr, cached_max_range, cache_padding, crashed_counter, targetairport, pos, previous_pos, state, last_direction, number_consecutive_turns, turn_counter, flags)
        }
        181 => layout!(crate::aircraft::Action, item; kind, id, other, a, b, c, d),
        _ => usize::MAX,
    }
}
