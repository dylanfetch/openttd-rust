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
struct OpenTTDStation;
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
	uint8_t (*map_log_x)() noexcept;
	uint32_t (*map_max_x)() noexcept;
	uint32_t (*map_max_y)() noexcept;
	uint8_t (*plane_speed)() noexcept;
	bool (*no_jetcrash)() noexcept;
	uint8_t (*plane_crashes)() noexcept;
	bool (*service_at_helipad)() noexcept;
	bool (*disaster_sound)() noexcept;
	int32_t (*economy_date)() noexcept;
	int32_t (*calendar_date)() noexcept;
	int32_t (*slope)(int32_t, int32_t) noexcept;
	int32_t (*tile_height)(uint32_t) noexcept;
};
const OpenTTDSharedServices &GetRustSharedServices() noexcept;

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
		.map_log_x = OpenTTDFixtureMapService<uint8_t>,
		.map_max_x = OpenTTDFixtureMapService<uint32_t>,
		.map_max_y = OpenTTDFixtureMapService<uint32_t>,
		.plane_speed = OpenTTDFixtureMapService<uint8_t>,
		.no_jetcrash = OpenTTDFixtureMapService<bool>,
		.plane_crashes = OpenTTDFixtureMapService<uint8_t>,
		.service_at_helipad = OpenTTDFixtureMapService<bool>,
		.disaster_sound = OpenTTDFixtureMapService<bool>,
		.economy_date = OpenTTDFixtureMapService<int32_t>,
		.calendar_date = OpenTTDFixtureMapService<int32_t>,
		.slope = OpenTTDFixtureMapService<int32_t, int32_t, int32_t>,
		.tile_height = OpenTTDFixtureMapService<int32_t, uint32_t>,
	};
}
#endif /* RUST_SERVICES_FFI_H */
