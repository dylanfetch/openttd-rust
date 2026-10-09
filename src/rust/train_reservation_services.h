/*
	* This file is part of OpenTTD.
	* OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
	* OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	* See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
	*/

/** @file train_reservation_services.h Direct map/PBS/follower services. */

static uint64_t TrainReservationAllCompat(void *context, OpenTTDTrainHandle id) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	return GetAllCompatibleRailTypes(v->GetRailTypes()).base();
}
static uint64_t TrainReservationBackoff(void *context, OpenTTDTrainHandle) noexcept
{
	return _settings_game.pf.path_backoff_interval;
}
static uint64_t TrainReservationBitsTrack(void *context, OpenTTDTrainHandle, uint8_t a) noexcept
{
	return TrackBitsToTrack(static_cast<TrackBits>(a));
}
static uint64_t TrainReservationBlocking(void *context, OpenTTDTrainHandle, uint32_t a, uint8_t b) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return HasOnewaySignalBlockingTrackdir(t, static_cast<Trackdir>(b));
}
static uint64_t TrainReservationCheckReverse(void *context, OpenTTDTrainHandle id) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	return YapfTrainCheckReverse(v);
}
static uint64_t TrainReservationCompatStation(void *context, OpenTTDTrainHandle, uint32_t a, uint32_t b) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return IsCompatibleTrainStationTile(t, TileIndex(static_cast<uint32_t>(b)));
}
static uint64_t TrainReservationConditional(void *context, OpenTTDTrainHandle id, uint8_t a) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	return ProcessConditionalOrder(v->GetOrder(static_cast<VehicleOrderID>(a)), v);
}
static void TrainReservationCopyOrder(void *context, OpenTTDTrainHandle id, uint8_t a) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	v->current_order = *v->GetOrder(static_cast<VehicleOrderID>(a));
}
static uint64_t TrainReservationCrossDirs(void *context, OpenTTDTrainHandle, uint8_t a) noexcept
{
	return TrackdirCrossesTrackdirs(static_cast<Trackdir>(a));
}
static uint64_t TrainReservationCrossTracks(void *context, OpenTTDTrainHandle, uint8_t a) noexcept
{
	return TrackCrossesTracks(static_cast<Track>(a));
}
static uint64_t TrainReservationDepotDir(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return GetRailDepotDirection(t);
}
static uint64_t TrainReservationDepotReserved(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return HasDepotReservation(t);
}
static uint64_t TrainReservationDiagReachDirs(void *context, OpenTTDTrainHandle, uint8_t a) noexcept
{
	return DiagdirReachesTrackdirs(static_cast<DiagDirection>(a));
}
static uint64_t TrainReservationDiagTrack(void *context, OpenTTDTrainHandle, uint8_t a) noexcept
{
	return DiagDirToDiagTrack(static_cast<DiagDirection>(a));
}
static uint64_t TrainReservationEnterTd(void *context, OpenTTDTrainHandle, uint8_t a, uint8_t b) noexcept
{
	return TrackEnterdirToTrackdir(static_cast<Track>(a), static_cast<DiagDirection>(b));
}
static uint64_t TrainReservationExitDir(void *context, OpenTTDTrainHandle, uint8_t a) noexcept
{
	return TrackdirToExitdir(static_cast<Trackdir>(a));
}
static uint64_t TrainReservationFree(void *context, OpenTTDTrainHandle id, uint32_t a, uint8_t b) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	TileIndex t(static_cast<uint32_t>(a));
	return IsWaitingPositionFree(v, t, static_cast<Trackdir>(b), _settings_game.pf.forbid_90_deg);
}
static uint64_t TrainReservationGreen(void *context, OpenTTDTrainHandle, uint32_t a, uint8_t b) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return GetSignalStateByTrackdir(t, static_cast<Trackdir>(b)) == SIGNAL_STATE_GREEN;
}
static uint64_t TrainReservationHasPbs(void *context, OpenTTDTrainHandle, uint32_t a, uint8_t b) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return HasPbsSignalOnTrackdir(t, static_cast<Trackdir>(b));
}
static uint64_t TrainReservationHasReserved(void *context, OpenTTDTrainHandle, uint32_t a, uint8_t b) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return HasReservedTracks(t, static_cast<TrackBits>(b));
}
static uint64_t TrainReservationHasSignal(void *context, OpenTTDTrainHandle, uint32_t a, uint8_t b) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return HasSignalOnTrackdir(t, static_cast<Trackdir>(b));
}
static void TrainReservationIncrementOrder(void *context, OpenTTDTrainHandle id) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	v->IncrementRealOrderIndex();
}
static uint64_t TrainReservationIsBridge(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return IsBridge(t);
}
static uint64_t TrainReservationIsDepot(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return IsRailDepotTile(t);
}
static uint64_t TrainReservationIsPbs(void *context, OpenTTDTrainHandle, uint32_t a, uint8_t b) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return IsPbsSignal(GetSignalType(t, static_cast<Track>(b)));
}
static uint64_t TrainReservationIsPlain(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return IsPlainRail(t);
}
static uint64_t TrainReservationIsRailway(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return IsTileType(t, MP_RAILWAY);
}
static uint64_t TrainReservationIsStation(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return IsRailStationTile(t);
}
static uint64_t TrainReservationIsTunnel(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return IsTileType(t, MP_TUNNELBRIDGE);
}
static uint64_t TrainReservationIsWaypoint(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return IsRailWaypointTile(t);
}
static uint64_t TrainReservationLineReverse(void *context, OpenTTDTrainHandle) noexcept
{
	return _settings_game.difficulty.line_reverse_mode;
}
static void TrainReservationMarkBridge(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	MarkBridgeDirty(t);
}
static void TrainReservationMarkTile(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	MarkTileDirtyByTile(t);
}
static uint64_t TrainReservationNeedsService(void *context, OpenTTDTrainHandle id) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	return v->NeedsServicing();
}
static uint64_t TrainReservationOneway(void *context, OpenTTDTrainHandle, uint32_t a, uint8_t b) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return IsOnewaySignal(t, static_cast<Track>(b));
}
static uint64_t TrainReservationOrderService(void *context, OpenTTDTrainHandle id, uint8_t a) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	return v->GetOrder(static_cast<VehicleOrderID>(a))->GetDepotOrderType().Test(OrderDepotTypeFlag::Service);
}
static uint64_t TrainReservationOrderStop(void *context, OpenTTDTrainHandle id, uint16_t a) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	return v->current_order.ShouldStopAtStation(v, StationID(static_cast<uint16_t>(a)));
}
static uint64_t TrainReservationOrderType(void *context, OpenTTDTrainHandle id, uint8_t a) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	return v->GetOrder(static_cast<VehicleOrderID>(a))->GetType();
}
static uint64_t TrainReservationOtherEnd(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return GetOtherTunnelBridgeEnd(t).base();
}
static uint64_t TrainReservationOverlap(void *context, OpenTTDTrainHandle, uint8_t a) noexcept
{
	return TracksOverlap(static_cast<TrackBits>(a));
}
static void TrainReservationPathResult(void *context, OpenTTDTrainHandle id, uint8_t a) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	v->HandlePathfindingResult(a != 0);
}
static void TrainReservationProfile(void *context, OpenTTDTrainHandle, uint64_t a) noexcept
{
	if (_train_profile.enabled) ++_train_profile.counts[a];
}
static uint64_t TrainReservationRail90(void *context, OpenTTDTrainHandle, uint32_t a, uint32_t b) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return Rail90DegTurnDisallowed(GetTileRailType(t), GetTileRailType(TileIndex(static_cast<uint32_t>(b))));
}
static uint64_t TrainReservationReachDirs(void *context, OpenTTDTrainHandle, uint8_t a) noexcept
{
	return TrackdirReachesTrackdirs(static_cast<Trackdir>(a));
}
static uint64_t TrainReservationReachTracks(void *context, OpenTTDTrainHandle, uint8_t a) noexcept
{
	return DiagdirReachesTracks(static_cast<DiagDirection>(a));
}
static uint64_t TrainReservationReserved(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return GetReservedTrackbits(t);
}
static uint64_t TrainReservationReservePaths(void *context, OpenTTDTrainHandle) noexcept
{
	return _settings_game.pf.reserve_paths;
}
static void TrainReservationRestoreOrder(void *context, OpenTTDTrainHandle id) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	auto *ctx = static_cast<TrainReservationContext *>(context);
	v->current_order = *ctx->saved_order;
}
static uint64_t TrainReservationSafe(void *context, OpenTTDTrainHandle id, uint32_t a, uint8_t b) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	TileIndex t(static_cast<uint32_t>(a));
	return IsSafeWaitingPosition(v, t, static_cast<Trackdir>(b), true, _settings_game.pf.forbid_90_deg);
}
static void TrainReservationSaveOrder(void *context, OpenTTDTrainHandle id) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	auto *ctx = static_cast<TrainReservationContext *>(context);
	ctx->saved_order.emplace(v->current_order);
}
static void TrainReservationService(void *context, OpenTTDTrainHandle id) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	CheckIfTrainNeedsService(v);
}
static void TrainReservationSetDepot(void *context, OpenTTDTrainHandle, uint32_t a, uint8_t b) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	SetDepotReservation(t, b != 0);
}
static void TrainReservationSetDepotDest(void *context, OpenTTDTrainHandle id, uint32_t a) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	TileIndex t(static_cast<uint32_t>(a));
	v->current_order.SetDestination(GetDepotIndex(t));
}
static void TrainReservationSetPlatform(void *context, OpenTTDTrainHandle, uint32_t a, uint8_t b, uint8_t c) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	SetRailStationPlatformReservation(t, static_cast<DiagDirection>(b), c != 0);
}
static void TrainReservationSetSignal(void *context, OpenTTDTrainHandle, uint32_t a, uint8_t b, uint8_t c) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	SetSignalStateByTrackdir(t, static_cast<Trackdir>(b), c != 0 ? SIGNAL_STATE_GREEN : SIGNAL_STATE_RED);
}
static void TrainReservationSetTunnel(void *context, OpenTTDTrainHandle, uint32_t a, uint8_t b) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	SetTunnelBridgeReservation(t, b != 0);
}
static uint64_t TrainReservationShowRes(void *context, OpenTTDTrainHandle) noexcept
{
	return _settings_client.gui.show_track_reservation;
}
static void TrainReservationSignalBuffer(void *context, OpenTTDTrainHandle id, uint32_t a, uint8_t b) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	TileIndex t(static_cast<uint32_t>(a));
	AddSideToSignalBuffer(t, static_cast<DiagDirection>(b), v->owner);
}
static void TrainReservationStartStop(void *context, OpenTTDTrainHandle id) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP);
}
static uint64_t TrainReservationStation(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return GetStationIndex(t).base();
}
static uint64_t TrainReservationStationTrain(void *context, OpenTTDTrainHandle, uint16_t a) noexcept
{
	return Station::Get(StationID(static_cast<uint16_t>(a)))->facilities.Test(StationFacility::Train);
}
static uint64_t TrainReservationStationXy(void *context, OpenTTDTrainHandle, uint16_t a) noexcept
{
	return Station::Get(StationID(static_cast<uint16_t>(a)))->xy.base();
}
static void TrainReservationStuck(void *context, OpenTTDTrainHandle id) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	MarkTrainAsStuck(v);
}
static uint64_t TrainReservationTileAdd(void *context, OpenTTDTrainHandle, uint32_t a, uint8_t b) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return TileAddByDiagDir(t, static_cast<DiagDirection>(b)).base();
}
static uint64_t TrainReservationTileOffset(void *context, OpenTTDTrainHandle, uint8_t a) noexcept
{
	return static_cast<uint64_t>(static_cast<int64_t>(TileOffsByDiagDir(static_cast<DiagDirection>(a))));
}
static uint64_t TrainReservationTrackdir(void *context, OpenTTDTrainHandle id) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	return v->GetVehicleTrackdir();
}
static uint64_t TrainReservationTrackStatus(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return TrackStatusToTrackdirBits(GetTileTrackStatus(t, TRANSPORT_RAIL, 0));
}
static uint64_t TrainReservationTryTrack(void *context, OpenTTDTrainHandle, uint32_t a, uint8_t b) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return TryReserveRailTrack(t, static_cast<Track>(b));
}
static uint64_t TrainReservationTunnelDir(void *context, OpenTTDTrainHandle, uint32_t a) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	return GetTunnelBridgeDirection(t);
}
static uint64_t TrainReservationTunnelFree(void *context, OpenTTDTrainHandle id, uint32_t a, uint32_t b) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	TileIndex t(static_cast<uint32_t>(a));
	return TunnelBridgeIsFree(t, TileIndex(static_cast<uint32_t>(b)), v).Succeeded();
}
static void TrainReservationUnreserve(void *context, OpenTTDTrainHandle, uint32_t a, uint8_t b) noexcept
{
	TileIndex t(static_cast<uint32_t>(a));
	UnreserveRailTrack(t, static_cast<Track>(b));
}
static void TrainReservationUpdateBuffer(void *context, OpenTTDTrainHandle) noexcept
{
	UpdateSignalsInBuffer();
}
static void TrainReservationWriteDest(void *context, OpenTTDTrainHandle id, uint32_t a) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	TileIndex t(static_cast<uint32_t>(a));
	v->dest_tile = t;
}
static void TrainReservationWriteLast(void *context, OpenTTDTrainHandle id, uint16_t a) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	v->last_station_visited = StationID(static_cast<uint16_t>(a));
}
static void TrainReservationWriteSuppress(void *context, OpenTTDTrainHandle id, uint8_t a) noexcept
{
	Train *v = static_cast<Train *>(id.shell);
	AssignBit(v->gv_flags, GVF_SUPPRESS_IMPLICIT_ORDERS, a != 0);
}
static uint8_t TrainReservationFollow(OpenTTDTrainHandle id, uint64_t railtypes, OpenTTDTrainReservationFollow *out) noexcept
{
	CFollowTrackRail ft(static_cast<Train *>(id.shell), RailTypes(railtypes));
	ft.new_tile = TileIndex(out->new_tile); ft.new_td_bits = static_cast<TrackdirBits>(out->dirs);
	ft.exitdir = static_cast<DiagDirection>(out->exitdir); ft.is_tunnel = out->tunnel;
	ft.is_bridge = out->bridge; ft.is_station = out->station; ft.tiles_skipped = out->skipped;
	bool followed = ft.Follow(TileIndex(out->old_tile), static_cast<Trackdir>(out->old_td));
	*out = {ft.old_tile.base(), ft.new_tile.base(), ft.tiles_skipped, ft.new_td_bits,
	ft.old_td, ft.exitdir, static_cast<uint8_t>(ft.is_tunnel), static_cast<uint8_t>(ft.is_bridge),
	static_cast<uint8_t>(ft.is_station), ft.err};
	return followed;
}
static OpenTTDTrainReservationPbs TrainReservationOrigin(OpenTTDTrainHandle id, uint8_t check_other) noexcept
{
	Vehicle *other = nullptr;
	PBSTileInfo pbs = FollowTrainReservation(static_cast<Train *>(id.shell), check_other ? &other : nullptr);
	return {pbs.tile.base(), TrainHandle(other == nullptr ? nullptr : Train::From(other)), pbs.trackdir, static_cast<uint8_t>(pbs.okay)};
}

static OpenTTDTrainReservationSearch TrainReservationPathfind(OpenTTDTrainHandle id, uint32_t tile, uint8_t dir, uint8_t tracks, uint8_t reserve, uint8_t want_final) noexcept
{
	Train *v=static_cast<Train *>(id.shell); bool found=true; PBSTileInfo dest; TileIndex final_dest=INVALID_TILE;
	Track track=YapfTrainChooseTrack(v,TileIndex(tile),static_cast<DiagDirection>(dir),static_cast<TrackBits>(tracks),found,reserve != 0,&dest,want_final ? &final_dest:nullptr);
	OpenTTDTrainReservationSearch result{}; result.track=track;result.found=found;result.tile=dest.tile.base();result.td=dest.trackdir;result.okay=dest.okay;result.final_dest=final_dest.base();return result;
}
static uint8_t TrainReservationSafeTrack(OpenTTDTrainHandle id, uint32_t tile, uint8_t td, uint8_t override_types) noexcept
{return YapfTrainFindNearestSafeTile(static_cast<Train *>(id.shell),TileIndex(tile),static_cast<Trackdir>(td),override_types != 0);}
static uint8_t TrainReservationProcessOrders(OpenTTDTrainHandle id) noexcept
{return ProcessOrders(static_cast<Train *>(id.shell));}
static uint8_t TrainReservationUpdateOrderDest(OpenTTDTrainHandle id, uint8_t index) noexcept
{Train *v=static_cast<Train *>(id.shell);return UpdateOrderDest(v,v->GetOrder(static_cast<VehicleOrderID>(index)),0,true);}

static uint32_t TrainReservationReadTile(OpenTTDTrainHandle id) noexcept
{
	const Train *v=static_cast<Train *>(id.shell);
	return v->tile.base();
}
static OpenTTDTrainHandle TrainReservationReadNext(OpenTTDTrainHandle id) noexcept
{
	const Train *v=static_cast<Train *>(id.shell);
	return TrainHandle(v->Next());
}
static uint16_t TrainReservationReadLastStation(OpenTTDTrainHandle id) noexcept
{
	const Train *v=static_cast<Train *>(id.shell);
	return v->last_station_visited.base();
}
static uint8_t TrainReservationReadDirection(OpenTTDTrainHandle id) noexcept
{
	const Train *v=static_cast<Train *>(id.shell);
	return v->direction;
}
static uint8_t TrainReservationReadOrder(OpenTTDTrainHandle id) noexcept
{
	const Train *v=static_cast<Train *>(id.shell);
	return v->current_order.GetType();
}
static uint8_t TrainReservationReadNumOrders(OpenTTDTrainHandle id) noexcept
{
	const Train *v=static_cast<Train *>(id.shell);
	return v->GetNumOrders();
}
static uint8_t TrainReservationReadOrderIndex(OpenTTDTrainHandle id) noexcept
{
	const Train *v=static_cast<Train *>(id.shell);
	return v->cur_real_order_index;
}
static OpenTTDTrainReservationFreeRead TrainReservationReadFree(OpenTTDTrainHandle id) noexcept
{
	const Train *v=static_cast<Train *>(id.shell);
	return {v->tile.base()};
}
static OpenTTDTrainReservationFree1Read TrainReservationReadFree1(OpenTTDTrainHandle id) noexcept
{
	const Train *v=static_cast<Train *>(id.shell);
	return {v->tile.base(), TrainHandle(v->Next())};
}
static OpenTTDTrainReservationNewRead TrainReservationReadNew(OpenTTDTrainHandle id) noexcept
{
	const Train *v=static_cast<Train *>(id.shell);
	return {v->dest_tile.base(), v->last_station_visited.base(), v->cur_real_order_index, static_cast<uint8_t>(HasBit(v->gv_flags, GVF_SUPPRESS_IMPLICIT_ORDERS))};
}
static OpenTTDTrainReservationChooseRead TrainReservationReadChoose(OpenTTDTrainHandle id) noexcept
{
	const Train *v=static_cast<Train *>(id.shell);
	return {v->tile.base(), v->dest_tile.base(), v->current_order.GetDestination().base(), v->current_order.GetType()};
}
static OpenTTDTrainReservationChoose4Read TrainReservationReadChoose4(OpenTTDTrainHandle id) noexcept
{
	const Train *v=static_cast<Train *>(id.shell);
	return {v->current_order.GetType(), static_cast<uint8_t>(v->current_order.GetDepotActionType().Test(OrderDepotActionFlag::NearestDepot))};
}
static OpenTTDTrainReservationCheckNextRead TrainReservationReadCheckNext(OpenTTDTrainHandle id) noexcept
{
	const Train *v=static_cast<Train *>(id.shell);
	return {v->tile.base(), v->dest_tile.base(), v->current_order.GetDestination().base(), v->current_order.GetType(), v->GetNumOrders()};
}
