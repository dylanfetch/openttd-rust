/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file services_ffi.h Synchronous shared game services for Rust owners. */
#ifndef RUST_SERVICES_FFI_H
#define RUST_SERVICES_FFI_H
#include <cstdint>
#include <type_traits>

/* Immutable function table. All functions are noexcept and cannot reenter Rust.
 * Context may be null in the game; test fixtures supply their own world.
 * No Rust references into the map/pools survive calls. Output is call-scoped.
 * Map predicates take a TileIndex value and return the original accessor's
 * underlying type: TileType, TropicZone and Slope are uint8_t, GetTileZ is int.
 * mark_dirty is MarkTileDirtyByTile(tile) and set_tropic_zone is SetTropicZone.
 * Random logging still runs, but its debug source location names this wrapper.
 * Industry query returns 0 not industry,1 industry,2 bubble catcher and writes
 * TileVirtXY(x,y). It observes only type and industry graphics.
 * An environmental exception terminates rather than unwinding through Rust. */
struct OpenTTDSharedServices {
	void *context;
	uint32_t (*random)(void *) noexcept;
	uint32_t (*industry)(int32_t, int32_t, uint32_t *) noexcept;
	uint8_t (*tile_type)(uint32_t) noexcept;
	bool (*bridge_above)(uint32_t) noexcept;
	uint8_t (*tropic_zone)(uint32_t) noexcept;
	int32_t (*tile_z)(uint32_t) noexcept;
	uint8_t (*tile_slope)(uint32_t) noexcept;
	void (*mark_dirty)(uint32_t) noexcept;
	void (*set_tropic_zone)(uint32_t, uint8_t) noexcept;
	uint8_t (*tile_owner)(uint32_t) noexcept;
	uint16_t (*station_index)(uint32_t) noexcept;
	uint16_t (*depot_index)(uint32_t) noexcept;
	bool (*is_level_crossing)(uint32_t) noexcept;
	uint8_t (*tunnel_bridge_direction)(uint32_t) noexcept;
	uint16_t (*bridge_speed)(uint32_t) noexcept;
	bool (*is_water_tile)(uint32_t) noexcept;
};
const OpenTTDSharedServices &GetRustSharedServices() noexcept;

struct OpenTTDRailTraversal { bool depot; uint8_t tracks; };
struct OpenTTDRailSignalPresence { bool signals; bool track; uint8_t kind; bool forward; bool reverse; };
struct OpenTTDRailSignalUpdate { uint8_t kind; uint8_t reservations; };
struct OpenTTDRailStationAdmission { bool rail; uint8_t owner; uint8_t axis; bool blocked; };
struct OpenTTDRailCrossingAdmission { bool crossing; uint8_t owner; uint8_t axis; };
struct OpenTTDRailWormholeAdmission { uint8_t owner; uint8_t transport; uint8_t direction; };

/* Canonical rail/signal map leaves. A consuming owner calls each phase only at
 * the source observation point; signal writes are immediate. No map pointer,
 * whole tile record, opcode or allocation crosses this boundary. */
struct OpenTTDRailMapServices {
	OpenTTDRailTraversal (*traversal)(uint32_t) noexcept;
	OpenTTDRailSignalPresence (*signal_presence)(uint32_t, uint8_t) noexcept;
	OpenTTDRailSignalUpdate (*signal_update)(uint32_t, uint8_t) noexcept;
	OpenTTDRailStationAdmission (*station_admission)(uint32_t, uint8_t, uint8_t) noexcept;
	OpenTTDRailCrossingAdmission (*crossing_admission)(uint32_t, uint8_t) noexcept;
	OpenTTDRailWormholeAdmission (*wormhole_admission)(uint32_t, uint8_t) noexcept;

	bool (*is_rail_depot)(uint32_t) noexcept;
	uint8_t (*rail_depot_direction)(uint32_t) noexcept;
	uint8_t (*rail_tracks)(uint32_t) noexcept;
	bool (*has_signals)(uint32_t) noexcept;
	bool (*has_signal_track)(uint32_t, uint8_t) noexcept;
	uint8_t (*signal_type)(uint32_t, uint8_t) noexcept;
	bool (*has_signal_trackdir)(uint32_t, uint8_t) noexcept;
	uint8_t (*signal_state)(uint32_t, uint8_t) noexcept;
	void (*set_signal_state)(uint32_t, uint8_t, uint8_t) noexcept;
	uint32_t (*other_tunnel_bridge_end)(uint32_t) noexcept;
	uint32_t (*rail_track_status)(uint32_t, uint8_t) noexcept;
};
const OpenTTDRailMapServices &GetRustRailMapServices() noexcept;

/* Standalone fixtures without a game map: map predicates return zero and writes do nothing. */
template <typename Result, typename... Args>
Result OpenTTDFixtureMapService(Args...) noexcept
{
	if constexpr (!std::is_void_v<Result>) return Result{};
}
inline OpenTTDSharedServices OpenTTDFixtureSharedServices(void *context, uint32_t (*random)(void *) noexcept, uint32_t (*industry)(int32_t, int32_t, uint32_t *) noexcept) noexcept
{
	return {
		.context = context,
		.random = random,
		.industry = industry,
		.tile_type = OpenTTDFixtureMapService<uint8_t, uint32_t>,
		.bridge_above = OpenTTDFixtureMapService<bool, uint32_t>,
		.tropic_zone = OpenTTDFixtureMapService<uint8_t, uint32_t>,
		.tile_z = OpenTTDFixtureMapService<int32_t, uint32_t>,
		.tile_slope = OpenTTDFixtureMapService<uint8_t, uint32_t>,
		.mark_dirty = OpenTTDFixtureMapService<void, uint32_t>,
		.set_tropic_zone = OpenTTDFixtureMapService<void, uint32_t, uint8_t>,
		.tile_owner = OpenTTDFixtureMapService<uint8_t, uint32_t>,
		.station_index = OpenTTDFixtureMapService<uint16_t, uint32_t>,
		.depot_index = OpenTTDFixtureMapService<uint16_t, uint32_t>,
		.is_level_crossing = OpenTTDFixtureMapService<bool, uint32_t>,
		.tunnel_bridge_direction = OpenTTDFixtureMapService<uint8_t, uint32_t>,
		.bridge_speed = OpenTTDFixtureMapService<uint16_t, uint32_t>,
		.is_water_tile = OpenTTDFixtureMapService<bool, uint32_t>,
	};
}
#endif /* RUST_SERVICES_FFI_H */
