/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file train_services.h Direct shared-world operations; no train policy. */
struct TrainProfile {
	bool enabled = std::getenv("OPENTTD_TRAIN_PROFILE") != nullptr;
	uint64_t counts[31]{};
	~TrainProfile()
	{
		if (!this->enabled) return;
		const char *personal = std::getenv("HOME");
		if (personal == nullptr) return;
		if (FILE *file = std::fopen(fmt::format("{}/train-profile.json", personal).c_str(), "w")) {
			constexpr const char *names[] = {"tick_first", "tick_second", "reversal", "wormhole_swap", "unequal_before", "unequal_after", "depot_start", "depot_service", "red_oneway", "red_twoway", "force_signal", "stuck_retry", "stuck_reverse", "cross_bar", "cross_unbar", "collision", "crash_delete", "free_wagon_delete", "articulated_move", "first_tile_rail", "wormhole_exit", "depot_reentry", "controller_extension", "extension_fail", "extension_rollback", "extension_opposing_red", "extension_restore", "free_path", "choose_track", "order_lookahead", "order_restore"};
			fmt::print(file, "{{");
			for (size_t i = 0; i < std::size(names); ++i) fmt::print(file, "{}\"{}\":{}", i == 0 ? "" : ",", names[i], this->counts[i]);
			fmt::print(file, "}}\n");
			std::fclose(file);
		}
	}
};
static TrainProfile _train_profile;
static void TrainObserve(uint32_t id, OpenTTDTrainView *out) noexcept
{
	const Train *v = Train::Get(VehicleID(id));
	out->id = v->index.base();
	out->first = v->First()->index.base();
	out->next = v->Next() == nullptr ? UINT32_MAX : v->Next()->index.base();
	out->previous = v->Previous() == nullptr ? UINT32_MAX : v->Previous()->index.base();
	out->next_unit = v->GetNextVehicle() == nullptr ? UINT32_MAX : v->GetNextVehicle()->index.base();
	out->last = v->Last()->index.base();
	out->tile = v->tile.base();
	out->dest = v->dest_tile.base();
	out->x = v->x_pos;
	out->y = v->y_pos;
	out->z = v->z_pos;
	out->order_time = v->current_order_time;
	out->power = v->gcache.cached_power;
	out->weight = v->gcache.cached_weight;
	out->length = v->gcache.cached_veh_length;
	out->total_length = v->gcache.cached_total_length;
	out->max_speed = v->vcache.cached_max_speed;
	out->max_track_speed = v->gcache.cached_max_track_speed;
	out->speed = v->cur_speed;
	out->gv_flags = v->gv_flags;
	out->cargo_cap = v->cargo_cap;
	out->refit_cap = v->refit_cap;
	out->engine = v->engine_type.base();
	out->first_engine = v->gcache.first_engine.base();
	out->order_destination = v->current_order.GetDestination().base();
	out->last_station = v->last_station_visited.base();
	out->direction = v->direction;
	out->status = v->vehstatus.base();
	out->tick = v->tick_counter;
	out->running = v->running_ticks;
	out->day = v->day_counter;
	out->progress = v->progress;
	out->subspeed = v->subspeed;
	out->acceleration = v->acceleration;
	out->order = v->current_order.GetType();
	out->nonstop = v->current_order.GetNonStopType().base();
	out->breakdown = v->breakdown_ctr;
	out->front = v->IsFrontEngine();
	out->free_wagon = v->IsFreeWagon();
	out->articulated = v->IsArticulatedPart();
	out->engine_part = v->IsEngine();
	out->multiheaded = v->IsMultiheaded();
	out->owner = v->owner.base();
	out->vis_effect = v->vcache.cached_vis_effect;
}
static void TrainWrite(uint32_t id, uint32_t field, uint64_t value) noexcept
{
	Train *v = Train::Get(VehicleID(id));
	switch (field) {
		case TRAIN_WRITE_TILE: v->tile = TileIndex(static_cast<uint32_t>(value)); break;
		case TRAIN_WRITE_DEST: v->dest_tile = TileIndex(static_cast<uint32_t>(value)); break;
		case TRAIN_WRITE_X: v->x_pos = static_cast<int32_t>(value); break;
		case TRAIN_WRITE_Y: v->y_pos = static_cast<int32_t>(value); break;
		case TRAIN_WRITE_Z: v->z_pos = static_cast<int32_t>(value); break;
		case TRAIN_WRITE_DIRECTION: v->direction = static_cast<Direction>(value); break;
		case TRAIN_WRITE_SPEED: v->cur_speed = static_cast<uint16_t>(value); break;
		case TRAIN_WRITE_TICK: v->tick_counter = static_cast<uint8_t>(value); break;
		case TRAIN_WRITE_RUNNING: v->running_ticks = static_cast<uint8_t>(value); break;
		case TRAIN_WRITE_DAY: v->day_counter = static_cast<uint8_t>(value); break;
		case TRAIN_WRITE_ORDER_TIME: v->current_order_time = static_cast<int32_t>(value); break;
		case TRAIN_WRITE_PROGRESS: v->progress = static_cast<uint8_t>(value); break;
		case TRAIN_WRITE_SUBSPEED: v->subspeed = static_cast<uint8_t>(value); break;
		case TRAIN_WRITE_GV_FLAGS: v->gv_flags = static_cast<uint16_t>(value); break;
		case TRAIN_WRITE_ACCELERATION: v->acceleration = static_cast<uint8_t>(value); break;
		case TRAIN_WRITE_LENGTH: v->gcache.cached_veh_length = static_cast<uint8_t>(value); break;
		case TRAIN_WRITE_TOTAL_LENGTH: v->gcache.cached_total_length = static_cast<uint16_t>(value); break;
		case TRAIN_WRITE_FIRST_ENGINE: v->gcache.first_engine = EngineID(static_cast<uint16_t>(value)); break;
		case TRAIN_WRITE_MAX_SPEED: v->vcache.cached_max_speed = static_cast<uint16_t>(value); break;
		case TRAIN_WRITE_CARGO_CAP: v->cargo_cap = static_cast<uint16_t>(value); break;
		case TRAIN_WRITE_REFIT_CAP: v->refit_cap = static_cast<uint16_t>(value); break;
		case TRAIN_WRITE_CARGO_AGE: v->vcache.cached_cargo_age_period = static_cast<uint16_t>(value); break;
		case TRAIN_WRITE_LAST_STATION: v->last_station_visited = StationID(static_cast<uint16_t>(value)); break;
		case TRAIN_WRITE_COLOURMAP: v->colourmap = static_cast<PaletteID>(value); break;
		case TRAIN_WRITE_STATUS: v->vehstatus = VehStates(static_cast<uint8_t>(value)); break;
		default: NOT_REACHED();
	}
}
static uint64_t TrainLeaf(uint32_t op, uint32_t id, uint64_t a, uint64_t b, uint64_t c) noexcept
{
	Train *v = id == UINT32_MAX ? nullptr : Train::Get(VehicleID(id));
	switch (op) {
		case TRAIN_OP_ACC_MODEL: { return _settings_game.vehicle.train_acceleration_model; }
		case TRAIN_OP_RAIL_TILT: { return v->GetEngine()->info.misc_flags.Test(EngineMiscFlag::RailTilts); }
		case TRAIN_OP_CURVE_MOD: { return static_cast<uint16_t>(GetVehicleProperty(v, PROP_TRAIN_CURVE_SPEED_MOD, RailVehInfo(v->engine_type)->curve_speed_mod, true)); }
		case TRAIN_OP_RAIL_TYPES: { return RailVehInfo(v->engine_type)->railtypes.base(); }
		case TRAIN_OP_USER_DEFAULT: { return RailVehInfo(v->engine_type)->user_def_data; }
		case TRAIN_OP_POW_WAG_POWER: { return RailVehInfo(v->engine_type)->pow_wag_power; }
		case TRAIN_OP_RAILVEH_WAGON: { return RailVehInfo(v->engine_type)->railveh_type == RAILVEH_WAGON; }
		case TRAIN_OP_ENGINE_POWER: { return RailVehInfo(v->engine_type)->power; }
		case TRAIN_OP_WAGON_OVERRIDE: { return UsesWagonOverride(v); }
		case TRAIN_OP_WAGON_SPEED_LIMITS: { return _settings_game.vehicle.wagon_speed_limits; }
		case TRAIN_OP_SPEED_DEFAULT: { return RailVehInfo(v->engine_type)->max_speed; }
		case TRAIN_OP_ALL_POWERED: { return GetAllPoweredRailTypes(RailTypes(a)).base(); }
		case TRAIN_OP_ALL_COMPATIBLE: { return GetAllCompatibleRailTypes(RailTypes(a)).base(); }
		case TRAIN_OP_CARGO_AGE_DEFAULT: { return v->GetEngine()->info.cargo_age_period; }
		case TRAIN_OP_GRF_VERSION: { return v->GetEngine()->GetGRF() == nullptr ? 0 : v->GetEngine()->GetGRF()->grf_version; }
		case TRAIN_OP_LENGTH_CALLBACK: { return v->GetEngine()->info.callback_mask.Test(VehicleCallbackMask::Length); }
		case TRAIN_OP_LENGTH_DEFAULT: { return RailVehInfo(v->engine_type)->shorten_factor; }
		case TRAIN_OP_INVALIDATE_GRF: { v->InvalidateNewGRFCache(); break; }
		case TRAIN_OP_CACHE_OVERRIDE: { v->tcache.cached_override = GetWagonOverrideSpriteSet(v->engine_type, v->cargo_type, v->gcache.first_engine); break; }
		case TRAIN_OP_VIS_EFFECT: { v->UpdateVisualEffect(a != 0); break; }
		case TRAIN_OP_PROPERTY: { return GetVehicleProperty(v, static_cast<PropertyID>(a), static_cast<uint32_t>(b)); }
		case TRAIN_OP_CAPACITY: { return v->GetEngine()->DetermineCapacity(v); }
		case TRAIN_OP_TRUNCATE_CARGO: { v->cargo.Truncate(static_cast<uint>(a)); break; }
		case TRAIN_OP_CAPACITY_ERROR: { ShowNewGrfVehicleError(v->engine_type, STR_NEWGRF_BROKEN, STR_NEWGRF_BROKEN_CAPACITY, GRFBug::VehCapacity, true); break; }
		case TRAIN_OP_LENGTH_ERROR: { ErrorUnknownCallbackResult(v->GetEngine()->GetGRFID(), CBID_VEHICLE_LENGTH, static_cast<uint16_t>(a)); break; }
		case TRAIN_OP_CALLBACK_LENGTH: { return GetVehicleCallback(CBID_VEHICLE_LENGTH, 0, 0, v->engine_type, v); }
		case TRAIN_OP_LENGTH_CHANGED: { VehicleLengthChanged(v); break; }
		case TRAIN_OP_CARGO_CHANGED: { v->CargoChanged(); break; }
		case TRAIN_OP_CONSIST_WINDOWS: { SetWindowDirty(WC_VEHICLE_DETAILS, v->index); InvalidateWindowData(WC_VEHICLE_REFIT, v->index, VIWD_CONSIST_CHANGED); InvalidateWindowData(WC_VEHICLE_ORDERS, v->index, VIWD_CONSIST_CHANGED); InvalidateNewGRFInspectWindow(GSF_TRAINS, v->index); InvalidateWindowData(WC_VEHICLE_VIEW, v->index, VIWD_CONSIST_CHANGED); break; }
		case TRAIN_OP_CURVE_ADVANTAGE: { return GetRailTypeInfo(GetRailType(v->tile))->curve_speed; }
		case TRAIN_OP_IS_STATION: { return IsRailStationTile(TileIndex(a)); }
		case TRAIN_OP_STATION: { return GetStationIndex(TileIndex(a)).base(); }
		case TRAIN_OP_ORDER_STOP: { return v->current_order.ShouldStopAtStation(v, StationID(a)); }
		case TRAIN_OP_PLATFORM_AHEAD: { return Station::Get(StationID(a))->GetPlatformLength(TileIndex(b), DirToDiagDir(v->direction)); }
		case TRAIN_OP_PLATFORM_LENGTH: { return Station::Get(StationID(a))->GetPlatformLength(TileIndex(b)); }
		case TRAIN_OP_STOP_LOCATION: { return to_underlying(v->current_order.GetStopLocation()); }
		case TRAIN_OP_BRIDGE_SPEED: { return GetBridgeSpec(GetBridgeType(TileIndex(a)))->speed; }
		case TRAIN_OP_ORDER_MAX_SPEED: { return v->current_order.GetMaxSpeed(); }
		case TRAIN_OP_ACCELERATION: { return static_cast<uint64_t>(v->GetAcceleration()); }
		case TRAIN_OP_UPDATE_SPEED: { return v->RustDoUpdateSpeed(static_cast<uint>(a), static_cast<int>(b), static_cast<int>(c)); }
		case TRAIN_OP_VIEWPORT: { v->UpdateViewport(a != 0, b != 0); break; }
		case TRAIN_OP_POSITION: { v->UpdatePosition(); break; }
		case TRAIN_OP_INCLINATION: { return static_cast<uint64_t>(v->UpdateInclination(a != 0, b != 0)); }
		case TRAIN_OP_AGE: { AgeVehicle(v); break; }
		case TRAIN_OP_ECONOMY_AGE: { EconomyAgeVehicle(v); break; }
		case TRAIN_OP_DECREASE_VALUE: { DecreaseVehicleValue(v); break; }
		case TRAIN_OP_CHECK_BREAKDOWN: { CheckVehicleBreakdown(v); break; }
		case TRAIN_OP_CHECK_ORDERS: { CheckOrders(v); break; }
		case TRAIN_OP_SERVINT: { return Company::Get(v->owner)->settings.vehicle.servint_trains; }
		case TRAIN_OP_NEEDS_SERVICE: { return v->NeedsAutomaticServicing(); }
		case TRAIN_OP_CHAIN_DEPOT: { return v->IsChainInDepot(); }
		case TRAIN_OP_SERVICE: { VehicleServiceInDepot(v); break; }
		case TRAIN_OP_MAX_DEPOT_PENALTY: { return _settings_game.pf.yapf.maximum_go_to_depot_penalty; }
		case TRAIN_OP_DEPOT_INDEX: { return GetDepotIndex(TileIndex(a)).base(); }
		case TRAIN_OP_ORDER_DUMMY: { v->current_order.MakeDummy(); break; }
		case TRAIN_OP_ORDER_DEPOT_SERVICE: { v->current_order.MakeGoToDepot(DepotID(a), OrderDepotTypeFlag::Service, OrderNonStopFlag::NoIntermediate, OrderDepotActionFlag::NearestDepot); break; }
		case TRAIN_OP_SUPPRESS_IMPLICIT: { SetBit(v->gv_flags, GVF_SUPPRESS_IMPLICIT_ORDERS); break; }
		case TRAIN_OP_START_STOP_DIRTY: { SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP); break; }
		case TRAIN_OP_STATION_DEST: { return Station::Get(v->current_order.GetDestination().ToStationID())->train_station.tile.base(); }
		case TRAIN_OP_COST_CLASS: { return static_cast<uint32_t>(v->GetEngine()->VehInfo<RailVehicleInfo>().running_cost_class); }
		case TRAIN_OP_COST_DEFAULT: { return v->GetEngine()->VehInfo<RailVehicleInfo>().running_cost; }
		case TRAIN_OP_PRICE: { return static_cast<uint64_t>(GetPrice(v->GetEngine()->VehInfo<RailVehicleInfo>().running_cost_class, static_cast<uint>(a), v->GetEngine()->GetGRF())); }
		case TRAIN_OP_PAY_RUNNING: { CommandCost cost(EXPENSES_TRAIN_RUN, Money(static_cast<int64_t>(a))); v->profit_this_year -= cost.GetCost(); v->running_ticks = 0; SubtractMoneyFromCompanyFract(v->owner, cost); break; }
		case TRAIN_OP_RUNNING_WINDOWS: { SetWindowDirty(WC_VEHICLE_DETAILS, v->index); SetWindowClassesDirty(WC_TRAINS_LIST); break; }
		case TRAIN_OP_COST_DIVISOR: { return CalendarTime::DAYS_IN_YEAR * Ticks::DAY_TICKS; }
		case TRAIN_OP_INVALID_PRICE: { return static_cast<uint32_t>(INVALID_PRICE); }
		case TRAIN_OP_IS_DEPOT: { return IsRailDepotTile(TileIndex(a)); }
		case TRAIN_OP_DEPOT_DIR: { return GetRailDepotDirection(TileIndex(a)); }
		case TRAIN_OP_TUNNEL_DIR: { return GetTunnelBridgeDirection(TileIndex(a)); }
		case TRAIN_OP_TRACK_DIRECTION: { return TrackDirectionToTrackdir(static_cast<Track>(a), static_cast<Direction>(b)); }
		case TRAIN_OP_DIAG_TRACKDIR: { return DiagDirToDiagTrackdir(static_cast<DiagDirection>(a)); }
		case TRAIN_OP_DIR_DIAG: { return DirToDiagDir(static_cast<Direction>(a)); }
		case TRAIN_OP_FIRST_TRACK: { return FindFirstTrack(static_cast<TrackBits>(a)); }
		case TRAIN_OP_PROP_TRAIN_USER_DATA: { return PROP_TRAIN_USER_DATA; }
		case TRAIN_OP_PROP_TRAIN_SPEED: { return PROP_TRAIN_SPEED; }
		case TRAIN_OP_PROP_TRAIN_CARGO_AGE_PERIOD: { return PROP_TRAIN_CARGO_AGE_PERIOD; }
		case TRAIN_OP_PROP_TRAIN_SHORTEN_FACTOR: { return PROP_TRAIN_SHORTEN_FACTOR; }
		case TRAIN_OP_PROP_TRAIN_RUNNING_COST_FACTOR: { return PROP_TRAIN_RUNNING_COST_FACTOR; }
		case TRAIN_OP_IS_TUNNELBRIDGE: { return IsTileType(TileIndex(a), MP_TUNNELBRIDGE); }
		case TRAIN_OP_IS_BRIDGE: { return IsBridgeTile(TileIndex(a)); }
		case TRAIN_OP_IS_RAILWAY: { return IsTileType(TileIndex(a), MP_RAILWAY); }
		case TRAIN_OP_IS_PLAIN_RAIL: { return IsPlainRailTile(TileIndex(a)); }
		case TRAIN_OP_IS_CROSSING: { return IsLevelCrossingTile(TileIndex(a)); }
		case TRAIN_OP_IS_WAYPOINT: { return IsRailWaypointTile(TileIndex(a)); }
		case TRAIN_OP_MAP_SIZE: { return Map::Size(); }
		case TRAIN_OP_VEH_EXIT_DIR: { return VehicleExitDir(static_cast<Direction>(a), static_cast<TrackBits>(b)); }
		case TRAIN_OP_TILE_ADD_DIAG: { return TileAddByDiagDir(TileIndex(a), static_cast<DiagDirection>(b)).base(); }
		case TRAIN_OP_TILE_OFFSET_DIAG: { return static_cast<uint32_t>(TileOffsByDiagDir(static_cast<DiagDirection>(a))); }
		case TRAIN_OP_TILE_VIRT: { return TileVirtXY(static_cast<int>(a), static_cast<int>(b)).base(); }
		case TRAIN_OP_TRACKDIR_EXIT: { return TrackdirToExitdir(static_cast<Trackdir>(a)); }
		case TRAIN_OP_DIAG_AXIS: { return DiagDirToAxis(static_cast<DiagDirection>(a)); }
		case TRAIN_OP_AXIS_DIAG: { return AxisToDiagDir(static_cast<Axis>(a)); }
		case TRAIN_OP_CROSSING_ROAD_AXIS: { return GetCrossingRoadAxis(TileIndex(a)); }
		case TRAIN_OP_CROSSING_RAIL_AXIS: { return GetCrossingRailAxis(TileIndex(a)); }
		case TRAIN_OP_CROSSING_RESERVED: { return HasCrossingReservation(TileIndex(a)); }
		case TRAIN_OP_CROSSING_BARRED: { return IsCrossingBarred(TileIndex(a)); }
		case TRAIN_OP_WRITE_CROSSING_RES: { SetCrossingReservation(TileIndex(a), b != 0); break; }
		case TRAIN_OP_WRITE_CROSSING_BAR: { SetCrossingBarred(TileIndex(a), b != 0); break; }
		case TRAIN_OP_DIRTY_TILE: { MarkTileDirtyByTile(TileIndex(a)); break; }
		case TRAIN_OP_CROSSING_SOUND: { SndPlayTileFx(SND_0E_LEVEL_CROSSING, TileIndex(a)); break; }
		case TRAIN_OP_AMBIENT_SOUND: { return _settings_client.sound.ambient; }
		case TRAIN_OP_COMPATIBLE_RAIL_OWNER: { return IsTileOwner(TileIndex(a), v->owner); }
		case TRAIN_OP_RAIL_TYPE: { return GetRailType(TileIndex(a)); }
		case TRAIN_OP_TILE_RAIL_TYPE: { return GetTileRailType(TileIndex(a)); }
		case TRAIN_OP_SIGNALS_UPDATE: { return UpdateSignalsOnSegment(TileIndex(a), static_cast<DiagDirection>(b), v->owner); }
		case TRAIN_OP_SIGNALS_UPDATE_OWNER: { return UpdateSignalsOnSegment(TileIndex(a), static_cast<DiagDirection>(b), Owner(c)); }
		case TRAIN_OP_RESERVE_PATHS: { return _settings_game.pf.reserve_paths; }
		case TRAIN_OP_NO_90: { return Rail90DegTurnDisallowed(static_cast<RailType>(a), static_cast<RailType>(b)); }
		case TRAIN_OP_TRACK_CROSSES: { return TrackCrossesTracks(static_cast<Track>(a)); }
		case TRAIN_OP_TRACK_BITS: { return GetTrackBits(TileIndex(a)); }
		case TRAIN_OP_TRACKDIR_REACHES: { return DiagdirReachesTrackdirs(static_cast<DiagDirection>(a)); }
		case TRAIN_OP_DIAG_REACHES_TRACKS: { return DiagdirReachesTracks(static_cast<DiagDirection>(a)); }
		case TRAIN_OP_TRACK_STATUS: { return GetTileTrackStatus(TileIndex(a), TRANSPORT_RAIL, 0, static_cast<DiagDirection>(b)); }
		case TRAIN_OP_DIAG_BETWEEN: { return DiagdirBetweenTiles(TileIndex(a), TileIndex(b)); }
		case TRAIN_OP_HAS_SIGNAL_TD: { return HasSignalOnTrackdir(TileIndex(a), static_cast<Trackdir>(b)); }
		case TRAIN_OP_HAS_SIGNAL: { return HasSignalOnTrack(TileIndex(a), static_cast<Track>(b)); }
		case TRAIN_OP_SIGNAL_TYPE: { return GetSignalType(TileIndex(a), static_cast<Track>(b)); }
		case TRAIN_OP_SIGNAL_PBS: { return IsPbsSignal(static_cast<SignalType>(a)); }
		case TRAIN_OP_SIGNAL_HAS_PBS: { return HasPbsSignalOnTrackdir(TileIndex(a), static_cast<Trackdir>(b)); }
		case TRAIN_OP_ONEWAY_BLOCKING: { return HasOnewaySignalBlockingTrackdir(TileIndex(a), static_cast<Trackdir>(b)); }
		case TRAIN_OP_HAS_SIGNALS: { return HasSignals(TileIndex(a)); }
		case TRAIN_OP_SET_SIGNAL_STATE: { SetSignalStateByTrackdir(TileIndex(a), static_cast<Trackdir>(b), static_cast<SignalState>(c)); break; }
		case TRAIN_OP_SHOW_RESERVATION: { return _settings_client.gui.show_track_reservation; }
		case TRAIN_OP_HAS_DEPOT_RES: { return HasDepotReservation(TileIndex(a)); }
		case TRAIN_OP_SET_DEPOT_RES: { SetDepotReservation(TileIndex(a), b != 0); break; }
		case TRAIN_OP_TRY_RESERVE: { return TryReserveRailTrack(TileIndex(a), static_cast<Track>(b), c != 0); }
		case TRAIN_OP_HAS_RESERVED: { return HasReservedTracks(TileIndex(a), static_cast<TrackBits>(b)); }
		case TRAIN_OP_UNRESERVE: { UnreserveRailTrack(TileIndex(a), static_cast<Track>(b)); break; }
		case TRAIN_OP_OTHER_END: { return GetOtherTunnelBridgeEnd(TileIndex(a)).base(); }
		case TRAIN_OP_SET_TUNNEL_RES: { SetTunnelBridgeReservation(TileIndex(a), b != 0); break; }
		case TRAIN_OP_SET_PLATFORM_RES: { SetRailStationPlatformReservation(TileIndex(a), static_cast<DiagDirection>(b), c != 0); break; }
		case TRAIN_OP_STATION_AXIS: { return GetRailStationAxis(TileIndex(a)); }
		case TRAIN_OP_STATION_COMPATIBLE: { return IsCompatibleTrainStationTile(TileIndex(a), TileIndex(b)); }
		case TRAIN_OP_SIGNALS_BOTH: { SetSignalsOnBothDir(TileIndex(a), static_cast<Track>(b), Owner(c)); break; }
		case TRAIN_OP_BACKOFF: { return _settings_game.pf.path_backoff_interval; }
		case TRAIN_OP_REVERSE_AT_SIGNALS: { return _settings_game.pf.reverse_at_signals; }
		case TRAIN_OP_WAIT_ONEWAY: { return _settings_game.pf.wait_oneway_signal; }
		case TRAIN_OP_WAIT_TWOWAY: { return _settings_game.pf.wait_twoway_signal; }
		case TRAIN_OP_WAIT_PBS: { return _settings_game.pf.wait_for_pbs_path; }
		case TRAIN_OP_DAY_TICKS: { return Ticks::DAY_TICKS; }
		case TRAIN_OP_SIGSEG_PBS: { return SIGSEG_PBS; }
		case TRAIN_OP_SIGSEG_FULL: { return SIGSEG_FULL; }
		case TRAIN_OP_ACC_TYPE: { return static_cast<uint8_t>(GetRailTypeInfo(GetRailType(v->tile))->acceleration_type); }
		case TRAIN_OP_WAIT_UNBUNCH: { return v->IsWaitingForUnbunching(); }
		case TRAIN_OP_LEAVE_UNBUNCH: { v->LeaveUnbunchingDepot(); break; }
		case TRAIN_OP_RESET_UNBUNCH: { v->ResetDepotUnbunching(); break; }
		case TRAIN_OP_LAST_SPEED: { v->SetLastSpeed(); break; }
		case TRAIN_OP_DEPOT_DIRTY: { InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile); break; }
		case TRAIN_OP_DEPOT_WINDOW: { SetWindowDirty(WC_VEHICLE_DEPOT, TileIndex(a)); break; }
		case TRAIN_OP_VIEW_WINDOW: { SetWindowDirty(WC_VEHICLE_VIEW, v->index); break; }
		case TRAIN_OP_TRAIN_LIST: { SetWindowClassesDirty(WC_TRAINS_LIST); break; }
		case TRAIN_OP_HIDE_FILL: { HideFillingPercent(&v->fill_percent_te_id); break; }
		case TRAIN_OP_COUNT_CHAIN: { return CountVehiclesInChain(v); }
		case TRAIN_OP_DEPOT_TRACK: { return GetRailDepotTrack(TileIndex(a)); }
		case TRAIN_OP_TICKS_LEAVE_DEPOT: { return TicksToLeaveDepot(v); }
		case TRAIN_OP_UPDATE_DELTA: { v->UpdateDeltaXY(); break; }
		case TRAIN_OP_BASE_VIEWPORT: { v->Vehicle::UpdateViewport(a != 0); break; }
		case TRAIN_OP_SHOW_EFFECT: { v->ShowVisualEffect(); break; }
		case TRAIN_OP_ADVANCE_DISTANCE: { return v->GetAdvanceDistance(); }
		case TRAIN_OP_ORDER_FREE: { v->current_order.Free(); break; }
		case TRAIN_OP_HANDLE_BREAKDOWN: { return v->HandleBreakdown(); }
		case TRAIN_OP_LOST_WARN: { return _settings_client.gui.lost_vehicle_warn; }
		case TRAIN_OP_LOCAL_COMPANY: { return _local_company.base(); }
		case TRAIN_OP_STUCK_NEWS: { AddVehicleAdviceNewsItem(AdviceType::TrainStuck, GetEncodedString(STR_NEWS_TRAIN_IS_STUCK, v->index), v->index); break; }
		case TRAIN_OP_SET_NEXT: { v->SetNext(a == UINT32_MAX ? nullptr : Train::Get(VehicleID(a))); break; }
		case TRAIN_OP_CRASH_GROUND: { return v->GroundVehicleBase::Crash(a != 0); }
		case TRAIN_OP_CRASH_EVENT: { AI::NewEvent(v->owner, new ScriptEventVehicleCrashed(v->index, v->tile, ScriptEventVehicleCrashed::CRASH_TRAIN, static_cast<uint>(a), v->owner)); Game::NewEvent(new ScriptEventVehicleCrashed(v->index, v->tile, ScriptEventVehicleCrashed::CRASH_TRAIN, static_cast<uint>(a), v->owner)); break; }
		case TRAIN_OP_CRASH_NEWS: { AddTileNewsItem(GetEncodedString(STR_NEWS_TRAIN_CRASH, static_cast<uint>(a)), NewsType::Accident, v->tile); break; }
		case TRAIN_OP_CRASH_RATING: { ModifyStationRatingAround(v->tile, v->owner, -160, 30); break; }
		case TRAIN_OP_DISASTER_SOUND: { return _settings_client.sound.disaster; }
		case TRAIN_OP_CRASH_SOUND: { SndPlayVehicleFx(SND_13_TRAIN_COLLISION, v); break; }
		case TRAIN_OP_LARGE_EXPLOSION: { CreateEffectVehicleRel(v, 4, 4, 8, EV_EXPLOSION_LARGE); break; }
		case TRAIN_OP_SMALL_EXPLOSION: { CreateEffectVehicleRel(v, static_cast<int>(a), static_cast<int>(b), static_cast<int>(c), EV_EXPLOSION_SMALL); break; }
		case TRAIN_OP_VISIT_TYPE: { return Station::Get(StationID(a))->had_vehicle_of_type; }
		case TRAIN_OP_WRITE_VISIT_TYPE: { Station::Get(StationID(a))->had_vehicle_of_type |= HVOT_TRAIN; break; }
		case TRAIN_OP_TRAIN_VISIT: { return HVOT_TRAIN; }
		case TRAIN_OP_ARRIVAL_NEWS: { Station *st = Station::Get(StationID(a)); AddVehicleNewsItem(GetEncodedString(STR_NEWS_FIRST_TRAIN_ARRIVAL, st->index), v->owner == _local_company ? NewsType::ArrivalCompany : NewsType::ArrivalOther, v->index, st->index); AI::NewEvent(v->owner, new ScriptEventStationFirstVehicle(st->index, v->index)); Game::NewEvent(new ScriptEventStationFirstVehicle(st->index, v->index)); break; }
		case TRAIN_OP_DISCONNECT: { FatalError("Disconnecting train"); break; }
		case TRAIN_OP_TILE_OWNER: { return GetTileOwner(TileIndex(a)).base(); }
		case TRAIN_OP_PBS_SIGNAL_TYPE: { return SIGTYPE_PBS; }
		case TRAIN_OP_TILE_OFFSET_AXIS: { return static_cast<uint32_t>(TileOffsByAxis(static_cast<Axis>(a))); }
		case TRAIN_OP_REVERSE_SINGLE_BLOCKED: { return EngInfo(v->engine_type)->callback_mask.Test(VehicleCallbackMask::ArticEngine); }
		case TRAIN_OP_STOPPED_IN_DEPOT: { return v->IsStoppedInDepot(); }
		case TRAIN_OP_REVERSE_WINDOWS: { SetWindowDirty(WC_VEHICLE_DEPOT, v->tile); SetWindowDirty(WC_VEHICLE_DETAILS, v->index); SetWindowDirty(WC_VEHICLE_VIEW, v->index); SetWindowClassesDirty(WC_TRAINS_LIST); break; }
		case TRAIN_OP_IS_STATION_ANY: return IsTileType(TileIndex(a), MP_STATION);
		case TRAIN_OP_PROFILE: if (_train_profile.enabled) ++_train_profile.counts[a]; break;
		default: NOT_REACHED();
	}
	return 0;
}
static OpenTTDTrainState *TrainOwner(uint32_t id) noexcept { return Train::Get(VehicleID(id))->GetRustState(); }
static size_t TrainNearby(uint32_t mode, uint32_t tile, int32_t x, int32_t y, uint32_t *out, size_t size) noexcept
{
	size_t count = 0;
	auto collect = [&](const Vehicle *v) {
		if (v->type != VEH_TRAIN) return;
		if (count < size) out[count] = v->index.base();
		++count;
	};
	if (mode == 0) {
		for (const Vehicle *v : VehiclesOnTile(TileIndex(tile))) collect(v);
	} else {
		for (const Vehicle *v : VehiclesNearTileXY(x, y, 7)) collect(v);
	}
	return count;
}
static uint64_t TrainAction(const OpenTTDTrainAction &action)
{
	Train *v = action.id == UINT32_MAX ? nullptr : Train::Get(VehicleID(action.id));
	uint64_t a = action.a, b = action.b, c = action.c;
	switch (action.op) {
		case TRAIN_OP_ENTER_TILE: { return VehicleEnterTile(v, TileIndex(a), static_cast<int>(b), static_cast<int>(c)).base(); }
		case TRAIN_OP_ENTER_DEPOT: { VehicleEnterDepot(v); break; }
		case TRAIN_OP_PROCESS_ORDERS: { return ProcessOrders(v); }
		case TRAIN_OP_LOADING: { v->HandleLoading(a != 0); break; }
		case TRAIN_OP_LEAVE_STATION: { v->LeaveStation(); break; }
		case TRAIN_OP_BEGIN_LOADING: { v->BeginLoading(); break; }
		case TRAIN_OP_ARRIVAL_TRIGGERS: { Station *st = Station::Get(StationID(a)); TriggerStationRandomisation(st, v->tile, StationRandomTrigger::VehicleArrives); TriggerStationAnimation(st, v->tile, StationAnimationTrigger::VehicleArrives); break; }
		case TRAIN_OP_LEAVE_SOUND: { v->PlayLeaveStationSound(); break; }
		case TRAIN_OP_DELETE_VEHICLE: { delete v; break; }
		case TRAIN_OP_CHOOSE_TRACK: { return ChooseTrainTrack(v, TileIndex(a), static_cast<DiagDirection>(b), static_cast<TrackBits>(c), false, nullptr, true); }
		case TRAIN_OP_CHECK_NEXT: { CheckNextTrainTile(v); break; }
		case TRAIN_OP_TRY_PATH: { return TryPathReserve(v, a != 0, b != 0); }
		case TRAIN_OP_FREE_RESERVATION: { FreeTrainTrackReservation(v); break; }
		case TRAIN_OP_CLEAR_RESERVATION: { ClearPathReservation(v, TileIndex(a), static_cast<Trackdir>(b)); break; }
		case TRAIN_OP_CHECK_REVERSE: { return CheckReverseTrain(v); }
		case TRAIN_OP_FIND_DEPOT_TILE: { return FindClosestTrainDepot(v, static_cast<int>(a)).tile.base(); }
		case TRAIN_OP_RESERVE_UNDER: { v->ReserveTrackUnderConsist(); break; }
		case TRAIN_OP_FIND_DEPOT: { auto result = FindClosestTrainDepot(v, static_cast<int>(a)); return result.tile.base() | (static_cast<uint64_t>(result.best_length) << 32); }
		case TRAIN_OP_RESERVE_TRACK: { return TryReserveRailTrack(TileIndex(a), static_cast<Track>(b), c != 0); }
		default: NOT_REACHED();
	}
	return 0;
}
static uint64_t TrainRun(uint32_t kind, const Train *v, uint64_t a, uint64_t b, uint64_t c)
{
	static const OpenTTDTrainServices leaves{TrainObserve, TrainWrite, TrainLeaf, TrainOwner, TrainNearby};
	std::unique_ptr<void, decltype(&openttd_rust_train_destroy)> task(openttd_rust_train_create(kind, v == nullptr ? UINT32_MAX : v->index.base(), a, b, c, &leaves, &GetRustSharedServices()), openttd_rust_train_destroy);
	uint64_t response = 0;
	for (;;) {
		auto action = openttd_rust_train_advance(task.get(), response);
		if (action.op == UINT32_MAX) return action.a;
		response = TrainAction(action);
	}
}
