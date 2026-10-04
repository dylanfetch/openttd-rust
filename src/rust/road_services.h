/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file road_services.h Shared-world road leaves and named reentry dispatch. */
static void OPENTTD_ROAD_CALL RoadObserve(uint32_t id, OpenTTDRoadView *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	const RoadVehicle *rv = v->type == VEH_ROAD ? RoadVehicle::From(v) : nullptr;
	out->type = v->type;
	out->first = v->First()->index.base();
	out->next = v->Next() == nullptr ? UINT32_MAX : v->Next()->index.base();
	out->previous = v->Previous() == nullptr ? UINT32_MAX : v->Previous()->index.base();
	out->tile = v->tile.base();
	out->dest = v->dest_tile.base();
	out->x = v->x_pos;
	out->y = v->y_pos;
	out->z = v->z_pos;
	out->direction = v->direction;
	out->speed = v->cur_speed;
	out->tick = v->tick_counter;
	out->running = v->running_ticks;
	out->day = v->day_counter;
	out->order_time = v->current_order_time;
	out->progress = v->progress;
	out->status = v->vehstatus.base();
	out->owner = v->owner.base();
	out->engine = v->engine_type.base();
	out->last_station = v->last_station_visited.base();
	out->order_destination = v->current_order.GetDestination().base();
	out->order_type = v->current_order.GetType();
	out->order_max_speed = v->current_order.GetMaxSpeed();
	out->breakdown = v->breakdown_ctr;
	out->max_track_speed = rv == nullptr ? 0 : rv->gcache.cached_max_track_speed;
	out->length = rv == nullptr ? 0 : rv->gcache.cached_veh_length;
	out->total_length = rv == nullptr ? 0 : rv->gcache.cached_total_length;
	out->roadtype = rv == nullptr ? 0 : rv->roadtype;
	out->front = rv != nullptr && rv->IsFrontEngine();
	out->articulated = rv != nullptr && rv->HasArticulatedPart();
	out->tram = rv != nullptr && RoadTypeIsTram(rv->roadtype);
	out->bus = rv != nullptr && rv->IsFrontEngine() && rv->IsBus();
	out->order_nonstop = v->current_order.GetNonStopType().base();
}
static void OPENTTD_ROAD_CALL RoadWrite(uint32_t id, uint32_t field, uint64_t value) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	switch (field) {
		case ROAD_WRITE_TILE: v->tile = TileIndex(static_cast<uint32_t>(value)); break;
		case ROAD_WRITE_X: v->x_pos = static_cast<int32_t>(value); break;
		case ROAD_WRITE_Y: v->y_pos = static_cast<int32_t>(value); break;
		case ROAD_WRITE_DIRECTION: v->direction = static_cast<Direction>(value); break;
		case ROAD_WRITE_SPEED: v->cur_speed = static_cast<uint16_t>(value); break;
		case ROAD_WRITE_TICK: v->tick_counter = static_cast<uint8_t>(value); break;
		case ROAD_WRITE_RUNNING: v->running_ticks = static_cast<uint8_t>(value); break;
		case ROAD_WRITE_DAY: v->day_counter = static_cast<uint8_t>(value); break;
		case ROAD_WRITE_ORDER_TIME: v->current_order_time = static_cast<int32_t>(value); break;
		case ROAD_WRITE_PROGRESS: v->progress = static_cast<uint8_t>(value); break;
		case ROAD_WRITE_LAST_STATION: v->last_station_visited = StationID(static_cast<uint16_t>(value)); break;
		case ROAD_WRITE_HIDDEN: v->vehstatus.Set(VehState::Hidden, value != 0); break;
		case ROAD_WRITE_FIRST_ENGINE: v->gcache.first_engine = EngineID(static_cast<uint16_t>(value)); break;
		case ROAD_WRITE_LENGTH: v->gcache.cached_veh_length = static_cast<uint8_t>(value); break;
		case ROAD_WRITE_TOTAL_LENGTH: v->gcache.cached_total_length = static_cast<uint16_t>(value); break;
		case ROAD_WRITE_CARGO_AGE: v->vcache.cached_cargo_age_period = static_cast<uint16_t>(value); break;
		case ROAD_WRITE_MAX_SPEED: v->vcache.cached_max_speed = static_cast<uint16_t>(value); break;
		case ROAD_WRITE_SUPPRESS_IMPLICIT: SetBit(v->gv_flags, GVF_SUPPRESS_IMPLICIT_ORDERS); break;
		default: NOT_REACHED();
	}
}

static uint64_t OPENTTD_ROAD_CALL RoadLeaf(uint32_t op, uint32_t id, uint64_t a, uint64_t b, uint64_t c) noexcept
{
	RoadVehicle *v = id == UINT32_MAX ? nullptr : RoadVehicle::Get(VehicleID(id));
	switch (op) {
		case ROAD_OP_ACC_MODEL: { return _settings_game.vehicle.roadveh_acceleration_model; }
		case ROAD_OP_ROAD_SIDE: { return _settings_game.vehicle.road_side; }
		case ROAD_OP_TILE_TYPE: { return GetTileType(TileIndex(a)); }
		case ROAD_OP_HAS_ROAD: { return HasTileAnyRoadType(TileIndex(a), v->compatible_roadtypes); }
		case ROAD_OP_TRACK_STATUS: { return GetTileTrackStatus(TileIndex(a), TRANSPORT_ROAD, GetRoadTramType(v->roadtype)); }
		case ROAD_OP_TILE_OWNER: { return GetTileOwner(TileIndex(a)).base(); }
		case ROAD_OP_DEPOT_DIR: { return GetRoadDepotDirection(TileIndex(a)); }
		case ROAD_OP_BAY_DIR: { return GetBayRoadStopDir(TileIndex(a)); }
		case ROAD_OP_IS_DEPOT: { return IsRoadDepotTile(TileIndex(a)); }
		case ROAD_OP_NORMAL_ROAD: { return IsNormalRoadTile(TileIndex(a)); }
		case ROAD_OP_ROAD_WORKS: { return HasRoadWorks(TileIndex(a)); }
		case ROAD_OP_DISALLOWED: { return GetDisallowedRoadDirections(TileIndex(a)); }
		case ROAD_OP_BAY_STOP: { return IsBayRoadStopTile(TileIndex(a)); }
		case ROAD_OP_IS_DT_STOP: { return IsDriveThroughStopTile(TileIndex(a)); }
		case ROAD_OP_STOP_TYPE: { return to_underlying(GetRoadStopType(TileIndex(a))); }
		case ROAD_OP_FREE_BAY: { return RoadStop::GetByTile(TileIndex(a), GetRoadStopType(TileIndex(a)))->HasFreeBay(); }
		case ROAD_OP_ANY_ROAD_BITS: { return GetAnyRoadBits(TileIndex(a), GetRoadTramType(v->roadtype), b != 0); }
		case ROAD_OP_ROAD_BITS: { return GetRoadBits(TileIndex(a), GetRoadTramType(v->roadtype)); }
		case ROAD_OP_OFFSET: { return static_cast<uint32_t>(TileOffsByDiagDir(static_cast<DiagDirection>(a))); }
		case ROAD_OP_TILE_X: { return TileX(TileIndex(a)); }
		case ROAD_OP_TILE_Y: { return TileY(TileIndex(a)); }
		case ROAD_OP_STATION: { return GetStationIndex(TileIndex(a)).base(); }
		case ROAD_OP_CONTINUATION: { return RoadStop::IsDriveThroughRoadStopContinuation(TileIndex(a), TileIndex(b)); }
		case ROAD_OP_BRIDGE_SPEED: { return GetBridgeSpec(GetBridgeType(TileIndex(a)))->speed; }
		case ROAD_OP_MAX_PENALTY: { return _settings_game.pf.yapf.maximum_go_to_depot_penalty; }
		case ROAD_OP_SERVINT: { return Company::Get(v->owner)->settings.vehicle.servint_roadveh; }
		case ROAD_OP_NEEDS_SERVICE: { return v->NeedsAutomaticServicing(); }
		case ROAD_OP_WAIT_UNBUNCH: { return v->IsWaitingForUnbunching(); }
		case ROAD_OP_ORDER_STOP: { return v->current_order.ShouldStopAtStation(v, GetStationIndex(TileIndex(a))); }
		case ROAD_OP_ROAD_TYPE: { return GetRoadType(TileIndex(a), GetRoadTramType(v->roadtype)); }
		case ROAD_OP_QUEUE: { return _settings_game.pf.roadveh_queue; }
		case ROAD_OP_TUNNEL_DIR: { return GetTunnelBridgeDirection(TileIndex(a)); }
		case ROAD_OP_ACCELERATION: { return static_cast<uint64_t>(v->GetAcceleration()); }
		case ROAD_OP_UPDATE_SPEED: { return v->RustDoUpdateSpeed(static_cast<uint32_t>(a), static_cast<int32_t>(b), static_cast<int32_t>(c)); }
		case ROAD_OP_ADVANCE: { return v->GetAdvanceDistance(); }
		case ROAD_OP_POSITION: { v->UpdatePosition(); break; }
		case ROAD_OP_BASE_VIEWPORT: { v->Vehicle::UpdateViewport(true); break; }
		case ROAD_OP_LAST_SPEED: { v->SetLastSpeed(); break; }
		case ROAD_OP_ROADSTOP_LEAVE: { RoadStop::GetByTile(v->tile, GetRoadStopType(v->tile))->Leave(v); break; }
		case ROAD_OP_ENTRANCE_SET: { RoadStop::GetByTile(v->tile, GetRoadStopType(v->tile))->SetEntranceBusy(a != 0); break; }
		case ROAD_OP_ENTRANCE_BUSY: { return RoadStop::GetByTile(v->tile, GetRoadStopType(v->tile))->IsEntranceBusy(); }
		case ROAD_OP_ORDER_FREE: { v->current_order.Free(); break; }
		case ROAD_OP_SET_NEXT: { v->SetNext(a == UINT32_MAX ? nullptr : Vehicle::Get(VehicleID(a))); break; }
		case ROAD_OP_START_STOP_DIRTY: { SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP); break; }
		case ROAD_OP_DEPOT_DIRTY: { InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile); break; }
		case ROAD_OP_DETAILS_DIRTY: { SetWindowDirty(WC_VEHICLE_DETAILS, v->index); SetWindowClassesDirty(WC_ROADVEH_LIST); break; }
		case ROAD_OP_SERVICE: { VehicleServiceInDepot(v); break; }
		case ROAD_OP_LEAVE_UNBUNCH: { v->LeaveUnbunchingDepot(); break; }
		case ROAD_OP_RESET_UNBUNCH: { v->ResetDepotUnbunching(); break; }
		case ROAD_OP_PATH_RESULT: { v->HandlePathfindingResult(a != 0); break; }
		case ROAD_OP_ORDER_DUMMY: { v->current_order.MakeDummy(); break; }
		case ROAD_OP_ORDER_DEPOT: { v->current_order.MakeGoToDepot(DepotID(a), OrderDepotTypeFlag::Service); break; }
		case ROAD_OP_DEPOT_INDEX: { return GetDepotIndex(TileIndex(a)).base(); }
		case ROAD_OP_DECREASE_VALUE: { DecreaseVehicleValue(v); break; }
		case ROAD_OP_AGE: { AgeVehicle(v); break; }
		case ROAD_OP_ECONOMY_AGE: { EconomyAgeVehicle(v); break; }
		case ROAD_OP_CHECK_BREAKDOWN: { CheckVehicleBreakdown(v); break; }
		case ROAD_OP_CHECK_ORDERS: { CheckOrders(v); break; }
		case ROAD_OP_PAY_RUNNING: { CommandCost cost(EXPENSES_ROADVEH_RUN, Money(static_cast<int64_t>(a))); v->profit_this_year -= cost.GetCost(); v->running_ticks = 0; SubtractMoneyFromCompanyFract(v->owner, cost); break; }
		case ROAD_OP_COST_CLASS: { return static_cast<uint32_t>(v->GetEngine()->VehInfo<RoadVehicleInfo>().running_cost_class); }
		case ROAD_OP_COST_FACTOR: { return v->GetEngine()->VehInfo<RoadVehicleInfo>().running_cost; }
		case ROAD_OP_GET_PRICE: { return static_cast<uint64_t>(GetPrice(v->GetEngine()->VehInfo<RoadVehicleInfo>().running_cost_class, a, v->GetEngine()->GetGRF())); }
		case ROAD_OP_GRF_VERSION: { return v->GetEngine()->GetGRF() == nullptr ? 0 : v->GetEngine()->GetGRF()->grf_version; }
		case ROAD_OP_LENGTH_DEFAULT: { return v->GetEngine()->VehInfo<RoadVehicleInfo>().shorten_factor; }
		case ROAD_OP_AGE_DEFAULT: { return EngInfo(v->engine_type)->cargo_age_period; }
		case ROAD_OP_SPEED_DEFAULT: { return RoadVehInfo(v->engine_type)->max_speed; }
		case ROAD_OP_LENGTH_ERROR: { ErrorUnknownCallbackResult(v->GetEngine()->GetGRFID(), CBID_VEHICLE_LENGTH, a); break; }
		case ROAD_OP_DISCONNECT: { FatalError("Disconnecting road vehicle."); }
		case ROAD_OP_EXPLOSION: { CreateEffectVehicleRel(v, 4, 4, 8, EV_EXPLOSION_LARGE); break; }
		case ROAD_OP_SOUND_DEFAULT: { return RoadVehInfo(v->engine_type)->sfx; }
		case ROAD_OP_SOUND: { SndPlayVehicleFx(static_cast<SoundID>(a), v); break; }
		case ROAD_OP_SOUND_OLD1: { return SND_19_DEPARTURE_OLD_RV_1; }
		case ROAD_OP_SOUND_OLD2: { return SND_1A_DEPARTURE_OLD_RV_2; }
		case ROAD_OP_ENGINE_INVALID: { return EngineID::Invalid().base(); }
		case ROAD_OP_INVALID_PRICE: { return static_cast<uint32_t>(INVALID_PRICE); }
		case ROAD_OP_COST_DIVISOR: { return CalendarTime::DAYS_IN_YEAR * Ticks::DAY_TICKS; }
		case ROAD_OP_IS_CROSSING: return IsLevelCrossingTile(TileIndex(a));
		case ROAD_OP_NEW_POSITION: { auto gp = GetNewVehiclePos(v); return static_cast<uint32_t>(gp.x) | (static_cast<uint64_t>(static_cast<uint32_t>(gp.y)) << 32); }
		case ROAD_OP_VIRT_TILE: return TileVirtXY(static_cast<int32_t>(a), static_cast<int32_t>(b)).base();
		case ROAD_OP_IS_ROAD_STOP: return IsStationRoadStop(TileIndex(a));
		case ROAD_OP_SET_DEST: v->dest_tile = TileIndex(static_cast<uint32_t>(a)); break;
		case ROAD_OP_CACHE_INVALIDATE: { v->InvalidateNewGRFCacheOfChain(); break; }
		case ROAD_OP_ARRIVAL: { RoadVehArrivesAt(v, Station::Get(StationID(a))); break; }
		case ROAD_OP_CRASH_NEWS: { RoadCrashNews(v, static_cast<uint32_t>(a)); break; }
		default: NOT_REACHED();
	}
	return 0;
}

static uint64_t RoadAction(const OpenTTDRoadAction &action)
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(action.id));
	uint64_t a = action.a, b = action.b, c = action.c;
	switch (action.op) {
		case ROAD_OP_ENTER_TILE: { return VehicleEnterTile(v, TileIndex(a), static_cast<int32_t>(b), static_cast<int32_t>(c)).base(); }
		case ROAD_OP_ENTER_DEPOT: { VehicleEnterDepot(v); break; }
		case ROAD_OP_PROCESS_ORDERS: { ProcessOrders(v); break; }
		case ROAD_OP_LOADING: { v->HandleLoading(); break; }
		case ROAD_OP_BEGIN_LOADING: { v->BeginLoading(); break; }
		case ROAD_OP_TRAM_PROBE: { Backup<CompanyID> current(_current_company, v->owner); CommandCost result = Command<CMD_BUILD_ROAD>::Do(DoCommandFlag::NoWater, TileIndex(a), static_cast<RoadBits>(b), v->roadtype, DRD_NONE, TownID::Invalid()); current.Restore(); return result.Succeeded(); }
		case ROAD_OP_PROPERTY: { return GetVehicleProperty(v, static_cast<PropertyID>(a), b); }
		case ROAD_OP_LENGTH_CALLBACK: { return GetVehicleCallback(CBID_VEHICLE_LENGTH, 0, 0, v->engine_type, v); }
		case ROAD_OP_PLAY_SOUND: { return PlayVehicleSound(v, VSE_START); }
		case ROAD_OP_VISUAL: { v->ShowVisualEffect(); break; }
		case ROAD_OP_UPDATE_VISUAL: { v->UpdateVisualEffect(); break; }
		case ROAD_OP_CARGO_CHANGED: { v->CargoChanged(); break; }
		case ROAD_OP_LENGTH_CHANGED: { VehicleLengthChanged(v); break; }
		case ROAD_OP_BREAKDOWN: { return v->HandleBreakdown(); }
		case ROAD_OP_DELETE: { delete v; break; }
		case ROAD_OP_GROUND_CRASH: { return v->GroundVehicleBase::Crash(a != 0); }
		case ROAD_OP_STOP_RANDOM: { TriggerRoadStopRandomisation(Station::Get(StationID(a)), v->tile, StationRandomTrigger::VehicleArrives); break; }
		case ROAD_OP_STOP_ANIMATION: { TriggerRoadStopAnimation(Station::Get(StationID(a)), v->tile, StationAnimationTrigger::VehicleArrives); break; }
		case ROAD_OP_YAPF: { RoadVehPathCache path; bool found = true; Trackdir dir = YapfRoadVehicleChooseTrack(v, TileIndex(a), static_cast<DiagDirection>(b), static_cast<TrackdirBits>(c), found, path); v->ReplacePath(path); return dir | (static_cast<uint64_t>(found) << 8); }
		case ROAD_OP_FIND_DEPOT: { FindDepotData result = YapfRoadVehicleFindNearestDepot(v, static_cast<int32_t>(a)); return result.tile.base() | (static_cast<uint64_t>(result.best_length) << 32); }
		case ROAD_OP_INCLINATION: { return static_cast<uint64_t>(v->UpdateInclination(a != 0, b != 0)); }
		case ROAD_OP_VIEWPORT: { v->UpdateViewport(a != 0, b != 0); break; }
		default: NOT_REACHED();
	}
	return 0;
}
