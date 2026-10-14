/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file services.cpp Direct shared-world leaves for Rust game owners. */
#include "../stdafx.h"
#include "services_ffi.h"
#ifdef WITH_RUST
#include "../industry_map.h"
#include "../bridge_map.h"
#include "../landscape.h"
#include "../viewport_func.h"
#include "../core/random_func.hpp"
#include "../station_map.h"
#include "../depot_map.h"
#include "../rail_map.h"
#include "../tunnelbridge_map.h"
#include "../bridge.h"
#include "../water_map.h"
#include "../pbs.h"
#include "../safeguards.h"

static uint32_t SharedRandom(void *) noexcept { return Random(); }
static uint8_t SharedTileType(uint32_t tile) noexcept { return GetTileType(TileIndex{tile}); }
static bool SharedBridgeAbove(uint32_t tile) noexcept { return IsBridgeAbove(TileIndex{tile}); }
static uint8_t SharedTropicZone(uint32_t tile) noexcept { return GetTropicZone(TileIndex{tile}); }
static int32_t SharedTileZ(uint32_t tile) noexcept { return GetTileZ(TileIndex{tile}); }
static uint8_t SharedTileSlope(uint32_t tile) noexcept { return GetTileSlope(TileIndex{tile}); }
static void SharedMarkDirty(uint32_t tile) noexcept { MarkTileDirtyByTile(TileIndex{tile}); }
static void SharedSetTropicZone(uint32_t tile, uint8_t zone) noexcept { SetTropicZone(TileIndex{tile}, static_cast<TropicZone>(zone)); }

/** Pure map query; no allocation, reentry or C++ exception crosses into Rust. */
static uint32_t SharedIndustry(int32_t x, int32_t y, uint32_t *index) noexcept
{
	TileIndex tile = TileVirtXY(x, y);
	*index = tile.base();
	if (!IsTileType(tile, MP_INDUSTRY)) return 0;
	return GetIndustryGfx(tile) == GFX_BUBBLE_CATCHER ? 2 : 1;
}

static uint8_t MapSvcTileOwner(uint32_t tile) noexcept { return GetTileOwner(TileIndex(tile)).base(); }
static uint16_t MapSvcStationIndex(uint32_t tile) noexcept { return GetStationIndex(TileIndex(tile)).base(); }
static uint16_t MapSvcDepotIndex(uint32_t tile) noexcept { return GetDepotIndex(TileIndex(tile)).base(); }
static bool MapSvcRoadIsLevelCrossing(uint32_t tile) noexcept { return IsLevelCrossing(TileIndex(tile)); }
static bool MapSvcIsLevelCrossing(uint32_t tile) noexcept { return SharedTileType(tile) == MP_ROAD && MapSvcRoadIsLevelCrossing(tile); }
static uint8_t MapSvcTunnelBridgeDirection(uint32_t tile) noexcept { return GetTunnelBridgeDirection(TileIndex(tile)); }
static uint16_t MapSvcBridgeSpeed(uint32_t tile) noexcept { return GetBridgeSpec(GetBridgeType(TileIndex(tile)))->speed; }
static bool MapSvcIsWaterTile(uint32_t tile) noexcept { return IsWaterTile(TileIndex(tile)); }

const OpenTTDSharedServices &GetRustSharedServices() noexcept
{
	static const OpenTTDSharedServices services{
		.context = nullptr,
		.random = SharedRandom,
		.industry = SharedIndustry,
		.tile_type = SharedTileType,
		.bridge_above = SharedBridgeAbove,
		.tropic_zone = SharedTropicZone,
		.tile_z = SharedTileZ,
		.tile_slope = SharedTileSlope,
		.mark_dirty = SharedMarkDirty,
		.set_tropic_zone = SharedSetTropicZone,
		.tile_owner = MapSvcTileOwner,
		.station_index = MapSvcStationIndex,
		.depot_index = MapSvcDepotIndex,
		.is_level_crossing = MapSvcIsLevelCrossing,
		.tunnel_bridge_direction = MapSvcTunnelBridgeDirection,
		.bridge_speed = MapSvcBridgeSpeed,
		.is_water_tile = MapSvcIsWaterTile,
	};
	return services;
}

static bool SharedRail_is_rail_depot(uint32_t t) noexcept { return IsRailDepot(TileIndex(t)); }
static uint8_t SharedRail_rail_depot_direction(uint32_t t) noexcept { return GetRailDepotDirection(TileIndex(t)); }
static uint8_t SharedRail_rail_tracks(uint32_t t) noexcept { return GetTrackBits(TileIndex(t)); }
static bool SharedRail_has_signals(uint32_t t) noexcept { return HasSignals(TileIndex(t)); }
static uint32_t SharedRailPresentSignals(uint32_t tile) noexcept { return GetPresentSignals(TileIndex(tile)); }
static bool SharedRail_has_signal_track(uint32_t tile, uint8_t track) noexcept
{
	assert(IsValidTrack(static_cast<Track>(track)));
	return SharedRail_has_signals(tile) && (SharedRailPresentSignals(tile) & SignalOnTrack(static_cast<Track>(track))) != 0;
}
static uint8_t SharedRail_signal_type(uint32_t t, uint8_t a) noexcept { return GetSignalType(TileIndex(t), static_cast<Track>(a)); }
static bool SharedRail_has_signal_trackdir(uint32_t tile, uint8_t td) noexcept
{
	assert(IsValidTrackdir(static_cast<Trackdir>(td)));
	return SharedRail_has_signals(tile) && (SharedRailPresentSignals(tile) & SignalAlongTrackdir(static_cast<Trackdir>(td))) != 0;
}
static uint8_t SharedRail_signal_state(uint32_t t, uint8_t a) noexcept { return GetSignalStateByTrackdir(TileIndex(t), static_cast<Trackdir>(a)); }
static void SharedRail_set_signal_state(uint32_t t, uint8_t a, uint8_t b) noexcept { SetSignalStateByTrackdir(TileIndex(t), static_cast<Trackdir>(a), static_cast<SignalState>(b)); }
static uint32_t SharedRail_other_tunnel_bridge_end(uint32_t t) noexcept { return GetOtherTunnelBridgeEnd(TileIndex(t)).base(); }
static uint32_t SharedRail_rail_track_status(uint32_t t, uint8_t a) noexcept { return GetTileTrackStatus(TileIndex(t), TRANSPORT_RAIL, 0, static_cast<DiagDirection>(a)); }


static OpenTTDRailTraversal SharedRail_traversal(uint32_t tile) noexcept
{
	OpenTTDRailTraversal r{.depot = SharedRail_is_rail_depot(tile), .tracks = 0};
	if (!r.depot) r.tracks = SharedRail_rail_tracks(tile);
	return r;
}
static OpenTTDRailSignalPresence SharedRail_signal_presence(uint32_t tile, uint8_t masked_tracks, uint8_t td) noexcept
{
	Trackdir dir = static_cast<Trackdir>(td);
	OpenTTDRailSignalPresence r{.signals = SharedRail_has_signals(tile), .track = false, .kind = 0, .forward = false, .reverse = false};
	if (r.signals) {
		Track track = TrackBitsToTrack(static_cast<TrackBits>(masked_tracks));
		uint present = SharedRailPresentSignals(tile);
		r.track = (present & SignalOnTrack(track)) != 0;
		if (r.track) {
			r.kind = SharedRail_signal_type(tile, track);
			r.forward = (present & SignalAlongTrackdir(dir)) != 0;
			r.reverse = (present & SignalAlongTrackdir(ReverseTrackdir(dir))) != 0;
		}
	}
	return r;
}
static OpenTTDRailSignalUpdate SharedRail_signal_update(uint32_t tile, uint8_t td) noexcept
{
	TileIndex t(tile);
	assert(HasSignalOnTrackdir(t, static_cast<Trackdir>(td)));
	SignalType kind = static_cast<SignalType>(SharedRail_signal_type(tile, TrackdirToTrack(static_cast<Trackdir>(td))));
	OpenTTDRailSignalUpdate r{.kind = kind, .reservations = 0};
	if (IsPbsSignal(kind)) r.reservations = GetRailReservationTrackBits(t);
	return r;
}
static OpenTTDRailStationAdmission SharedRail_station_admission(uint32_t tile, uint8_t owner, uint8_t dir) noexcept
{
	TileIndex t(tile);
	OpenTTDRailStationAdmission r{.rail = HasStationRail(t), .owner = INVALID_OWNER.base(), .axis = 0, .blocked = false};
	if (r.rail) {
		r.owner = MapSvcTileOwner(tile);
		if (r.owner == owner) {
			r.axis = GetRailStationAxis(t);
			if (DiagDirToAxis(static_cast<DiagDirection>(dir)) == r.axis) r.blocked = IsStationTileBlocked(t);
		}
	}
	return r;
}
static OpenTTDRailCrossingAdmission SharedRail_crossing_admission(uint32_t tile, uint8_t owner) noexcept
{
	TileIndex t(tile);
	OpenTTDRailCrossingAdmission r{.crossing = MapSvcRoadIsLevelCrossing(tile), .owner = INVALID_OWNER.base(), .axis = 0};
	if (r.crossing) {
		r.owner = MapSvcTileOwner(tile);
		if (r.owner == owner) r.axis = GetCrossingRoadAxis(t);
	}
	return r;
}
static OpenTTDRailWormholeAdmission SharedRail_wormhole_admission(uint32_t tile, uint8_t owner) noexcept
{
	TileIndex t(tile);
	OpenTTDRailWormholeAdmission r{.owner = MapSvcTileOwner(tile), .transport = 0, .direction = 0};
	if (r.owner == owner) {
		r.transport = GetTunnelBridgeTransportType(t);
		if (r.transport == TRANSPORT_RAIL) r.direction = MapSvcTunnelBridgeDirection(tile);
	}
	return r;
}

const OpenTTDRailMapServices &GetRustRailMapServices() noexcept
{
	static const OpenTTDRailMapServices services{
		.traversal = SharedRail_traversal,
		.signal_presence = SharedRail_signal_presence,
		.signal_update = SharedRail_signal_update,
		.station_admission = SharedRail_station_admission,
		.crossing_admission = SharedRail_crossing_admission,
		.wormhole_admission = SharedRail_wormhole_admission,

		.is_rail_depot = SharedRail_is_rail_depot,
		.rail_depot_direction = SharedRail_rail_depot_direction,
		.rail_tracks = SharedRail_rail_tracks,
		.has_signals = SharedRail_has_signals,
		.has_signal_track = SharedRail_has_signal_track,
		.signal_type = SharedRail_signal_type,
		.has_signal_trackdir = SharedRail_has_signal_trackdir,
		.signal_state = SharedRail_signal_state,
		.set_signal_state = SharedRail_set_signal_state,
		.other_tunnel_bridge_end = SharedRail_other_tunnel_bridge_end,
		.rail_track_status = SharedRail_rail_track_status,
	};
	return services;
}
#endif /* WITH_RUST */
