/*
	* This file is part of OpenTTD.
	* OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
	* OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	* See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
	*/

/** @file train_reservation_services.h Direct map/PBS/follower services. */
struct TrainReservationContext { std::optional<Order> saved_order; };
static void TrainReservationObserve(uint32_t id, OpenTTDTrainReservationView *out) noexcept
{
	Train *v = Train::Get(VehicleID(id));
	*out = {v->tile.base(), v->dest_tile.base(), v->Next() == nullptr ? UINT32_MAX : v->Next()->index.base(),
	v->current_order.GetDestination().base(), v->last_station_visited.base(), v->direction,
	v->current_order.GetType(), v->GetNumOrders(), v->cur_real_order_index,
	static_cast<uint8_t>(HasBit(v->gv_flags, GVF_SUPPRESS_IMPLICIT_ORDERS)),
	static_cast<uint8_t>(v->current_order.GetDepotActionType().Test(OrderDepotActionFlag::NearestDepot))};
}
static uint64_t TrainReservationLeaf(void *context, uint32_t op, uint32_t id, uint64_t a, uint64_t b, uint64_t c) noexcept
{
	auto *ctx = static_cast<TrainReservationContext *>(context);
	Train *v = Train::Get(VehicleID(id));
	TileIndex t(static_cast<uint32_t>(a));
	switch (op) {
		case TR_BACKOFF: { return _settings_game.pf.path_backoff_interval; }
		case TR_RESERVE_PATHS: { return _settings_game.pf.reserve_paths; }
		case TR_FORBID90: { return _settings_game.pf.forbid_90_deg; }
		case TR_LINE_REVERSE: { return _settings_game.difficulty.line_reverse_mode; }
		case TR_SHOW_RES: { return _settings_client.gui.show_track_reservation; }
		case TR_IS_STATION: { return IsRailStationTile(t); }
		case TR_IS_WAYPOINT: { return IsRailWaypointTile(t); }
		case TR_IS_RAILWAY: { return IsTileType(t, MP_RAILWAY); }
		case TR_IS_TUNNEL: { return IsTileType(t, MP_TUNNELBRIDGE); }
		case TR_IS_DEPOT: { return IsRailDepotTile(t); }
		case TR_IS_PLAIN: { return IsPlainRail(t); }
		case TR_STATION: { return GetStationIndex(t).base(); }
		case TR_DEPOT_DIR: { return GetRailDepotDirection(t); }
		case TR_TUNNEL_DIR: { return GetTunnelBridgeDirection(t); }
		case TR_OTHER_END: { return GetOtherTunnelBridgeEnd(t).base(); }
		case TR_IS_BRIDGE: { return IsBridge(t); }
		case TR_TUNNEL_FREE: { return TunnelBridgeIsFree(t, TileIndex(static_cast<uint32_t>(b)), v).Succeeded(); }
		case TR_SET_TUNNEL: { SetTunnelBridgeReservation(t, b != 0); break; }
		case TR_MARK_BRIDGE: { MarkBridgeDirty(t); break; }
		case TR_MARK_TILE: { MarkTileDirtyByTile(t); break; }
		case TR_COMPAT_STATION: { return IsCompatibleTrainStationTile(t, TileIndex(static_cast<uint32_t>(b))); }
		case TR_SET_PLATFORM: { SetRailStationPlatformReservation(t, static_cast<DiagDirection>(b), c != 0); break; }
		case TR_UNRESERVE: { UnreserveRailTrack(t, static_cast<Track>(b)); break; }
		case TR_RESERVED: { return GetReservedTrackbits(t); }
		case TR_OVERLAP: { return TracksOverlap(static_cast<TrackBits>(a)); }
		case TR_HAS_RESERVED: { return HasReservedTracks(t, static_cast<TrackBits>(b)); }
		case TR_HAS_SIGNAL: { return HasSignalOnTrackdir(t, static_cast<Trackdir>(b)); }
		case TR_HAS_PBS: { return HasPbsSignalOnTrackdir(t, static_cast<Trackdir>(b)); }
		case TR_IS_PBS: { return IsPbsSignal(GetSignalType(t, static_cast<Track>(b))); }
		case TR_GREEN: { return GetSignalStateByTrackdir(t, static_cast<Trackdir>(b)) == SIGNAL_STATE_GREEN; }
		case TR_SET_SIGNAL: { SetSignalStateByTrackdir(t, static_cast<Trackdir>(b), c != 0 ? SIGNAL_STATE_GREEN : SIGNAL_STATE_RED); break; }
		case TR_ONEWAY: { return IsOnewaySignal(t, static_cast<Track>(b)); }
		case TR_BLOCKING: { return HasOnewaySignalBlockingTrackdir(t, static_cast<Trackdir>(b)); }
		case TR_SIGNAL_BUFFER: { AddSideToSignalBuffer(t, static_cast<DiagDirection>(b), v->owner); break; }
		case TR_UPDATE_BUFFER: { UpdateSignalsInBuffer(); break; }
		case TR_RAIL90: { return Rail90DegTurnDisallowed(GetTileRailType(t), GetTileRailType(TileIndex(static_cast<uint32_t>(b)))); }
		case TR_EXIT_DIR: { return TrackdirToExitdir(static_cast<Trackdir>(a)); }
		case TR_REACH_TRACKS: { return DiagdirReachesTracks(static_cast<DiagDirection>(a)); }
		case TR_REACH_DIRS: { return TrackdirReachesTrackdirs(static_cast<Trackdir>(a)); }
		case TR_CROSS_TRACKS: { return TrackCrossesTracks(static_cast<Track>(a)); }
		case TR_CROSS_DIRS: { return TrackdirCrossesTrackdirs(static_cast<Trackdir>(a)); }
		case TR_ENTER_TD: { return TrackEnterdirToTrackdir(static_cast<Track>(a), static_cast<DiagDirection>(b)); }
		case TR_TILE_ADD: { return TileAddByDiagDir(t, static_cast<DiagDirection>(b)).base(); }
		case TR_TILE_OFFSET: { return static_cast<uint64_t>(static_cast<int64_t>(TileOffsByDiagDir(static_cast<DiagDirection>(a)))); }
		case TR_SAFE: { return IsSafeWaitingPosition(v, t, static_cast<Trackdir>(b), true, _settings_game.pf.forbid_90_deg); }
		case TR_FREE: { return IsWaitingPositionFree(v, t, static_cast<Trackdir>(b), _settings_game.pf.forbid_90_deg); }
		case TR_TRY_TRACK: { return TryReserveRailTrack(t, static_cast<Track>(b)); }
		case TR_DEPOT_RESERVED: { return HasDepotReservation(t); }
		case TR_SET_DEPOT: { SetDepotReservation(t, b != 0); break; }
		case TR_TRACK_STATUS: { return TrackStatusToTrackdirBits(GetTileTrackStatus(t, TRANSPORT_RAIL, 0)); }
		case TR_DIAG_REACH_DIRS: { return DiagdirReachesTrackdirs(static_cast<DiagDirection>(a)); }
		case TR_TRACKDIR: { return v->GetVehicleTrackdir(); }
		case TR_ALL_COMPAT: { return GetAllCompatibleRailTypes(v->GetRailTypes()).base(); }
		case TR_ORDER_STOP: { return v->current_order.ShouldStopAtStation(v, StationID(static_cast<uint16_t>(a))); }
		case TR_STUCK: { MarkTrainAsStuck(v); break; }
		case TR_START_STOP: { SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP); break; }
		case TR_PATH_RESULT: { v->HandlePathfindingResult(a != 0); break; }
		case TR_SAVE_ORDER: { ctx->saved_order.emplace(v->current_order); break; }
		case TR_RESTORE_ORDER: { v->current_order = *ctx->saved_order; break; }
		case TR_WRITE_DEST: { v->dest_tile = t; break; }
		case TR_WRITE_LAST: { v->last_station_visited = StationID(static_cast<uint16_t>(a)); break; }
		case TR_WRITE_SUPPRESS: { AssignBit(v->gv_flags, GVF_SUPPRESS_IMPLICIT_ORDERS, a != 0); break; }
		case TR_ORDER_TYPE: { return v->GetOrder(static_cast<VehicleOrderID>(a))->GetType(); }
		case TR_ORDER_SERVICE: { return v->GetOrder(static_cast<VehicleOrderID>(a))->GetDepotOrderType().Test(OrderDepotTypeFlag::Service); }
		case TR_NEEDS_SERVICE: { return v->NeedsServicing(); }
		case TR_COPY_ORDER: { v->current_order = *v->GetOrder(static_cast<VehicleOrderID>(a)); break; }
		case TR_SET_DEPOT_DEST: { v->current_order.SetDestination(GetDepotIndex(t)); break; }
		case TR_STATION_TRAIN: { return Station::Get(StationID(static_cast<uint16_t>(a)))->facilities.Test(StationFacility::Train); }
		case TR_STATION_XY: { return Station::Get(StationID(static_cast<uint16_t>(a)))->xy.base(); }
		case TR_INCREMENT_ORDER: { v->IncrementRealOrderIndex(); break; }
		case TR_DIAG_TRACK: { return DiagDirToDiagTrack(static_cast<DiagDirection>(a)); }
		case TR_BITS_TRACK: { return TrackBitsToTrack(static_cast<TrackBits>(a)); }
		case TR_STATION_RAIL: { return IsTileType(t, MP_STATION) && HasStationRail(t); }
		case TR_CHECK_REVERSE: { return YapfTrainCheckReverse(v); }
		case TR_CONDITIONAL: { return ProcessConditionalOrder(v->GetOrder(static_cast<VehicleOrderID>(a)), v); }
		case TR_SERVICE: { CheckIfTrainNeedsService(v); break; }
		case TR_PROFILE: if (_train_profile.enabled) ++_train_profile.counts[a]; break;
		default: NOT_REACHED();
	}
	return 0;
}
static uint8_t TrainReservationFollow(uint32_t id, uint64_t railtypes, OpenTTDTrainReservationFollow *out) noexcept
{
	CFollowTrackRail ft(Train::Get(VehicleID(id)), RailTypes(railtypes));
	ft.new_tile = TileIndex(out->new_tile); ft.new_td_bits = static_cast<TrackdirBits>(out->dirs);
	ft.exitdir = static_cast<DiagDirection>(out->exitdir); ft.is_tunnel = out->tunnel;
	ft.is_bridge = out->bridge; ft.is_station = out->station; ft.tiles_skipped = out->skipped;
	bool followed = ft.Follow(TileIndex(out->old_tile), static_cast<Trackdir>(out->old_td));
	*out = {ft.old_tile.base(), ft.new_tile.base(), ft.tiles_skipped, ft.new_td_bits,
	ft.old_td, ft.exitdir, static_cast<uint8_t>(ft.is_tunnel), static_cast<uint8_t>(ft.is_bridge),
	static_cast<uint8_t>(ft.is_station), ft.err};
	return followed;
}
static OpenTTDTrainReservationPbs TrainReservationOrigin(uint32_t id, uint8_t check_other) noexcept
{
	Vehicle *other = nullptr;
	PBSTileInfo pbs = FollowTrainReservation(Train::Get(VehicleID(id)), check_other ? &other : nullptr);
	return {pbs.tile.base(), other == nullptr ? UINT32_MAX : other->index.base(), pbs.trackdir, static_cast<uint8_t>(pbs.okay)};
}
static OpenTTDTrainState *TrainReservationOwner(uint32_t id) noexcept { return Train::Get(VehicleID(id))->GetRustState(); }
