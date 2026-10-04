/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file ship_control_services.h Copied shared-world services and named reentry. */
#include <cstdio>
#include <cstdlib>
static_assert(MP_INDUSTRY == 8 && MP_TUNNELBRIDGE == 9 && TILE_HEIGHT == 8 && TILE_SIZE == 16);
static_assert(TRACK_BIT_DEPOT == 128 && TRACK_BIT_WORMHOLE == 64 && HVOT_SHIP == 32);
static_assert(to_underlying(WaterClass::Sea) == 0 && DIRDIFF_REVERSE == 4);
static_assert(static_cast<uint8_t>(VehicleEnterTileState::EnteredWormhole) == 1 && static_cast<uint8_t>(VehicleEnterTileState::CannotEnter) == 2);

static void ShipObserve(uint32_t id, OpenTTDShipView *out) noexcept
{
	const Ship *v = Ship::Get(VehicleID(id));
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
	out->acceleration = v->acceleration;
	out->max_speed = v->vcache.cached_max_speed;
}
static void ShipWrite(uint32_t id, uint32_t field, uint64_t value) noexcept
{
	Ship *v = Ship::Get(VehicleID(id));
	switch (field) {
		case SHIP_WRITE_TILE: v->tile = TileIndex(static_cast<uint32_t>(value)); break;
		case SHIP_WRITE_X: v->x_pos = static_cast<int32_t>(value); break;
		case SHIP_WRITE_Y: v->y_pos = static_cast<int32_t>(value); break;
		case SHIP_WRITE_Z: v->z_pos = static_cast<int32_t>(value); break;
		case SHIP_WRITE_DIRECTION: v->direction = static_cast<Direction>(value); break;
		case SHIP_WRITE_SPEED: v->cur_speed = static_cast<uint16_t>(value); break;
		case SHIP_WRITE_TICK: v->tick_counter = static_cast<uint8_t>(value); break;
		case SHIP_WRITE_RUNNING: v->running_ticks = static_cast<uint8_t>(value); break;
		case SHIP_WRITE_DAY: v->day_counter = static_cast<uint8_t>(value); break;
		case SHIP_WRITE_ORDER_TIME: v->current_order_time = static_cast<int32_t>(value); break;
		case SHIP_WRITE_PROGRESS: v->progress = static_cast<uint8_t>(value); break;
		case SHIP_WRITE_LAST_STATION: v->last_station_visited = StationID(static_cast<uint16_t>(value)); break;
		case SHIP_WRITE_HIDDEN: v->vehstatus.Set(VehState::Hidden, value != 0); break;
		case SHIP_WRITE_MAX_SPEED: v->vcache.cached_max_speed = static_cast<uint16_t>(value); break;
		case SHIP_WRITE_CARGO_AGE: v->vcache.cached_cargo_age_period = static_cast<uint16_t>(value); break;
		case SHIP_WRITE_DEST: v->dest_tile = TileIndex(static_cast<uint32_t>(value)); break;
		default: NOT_REACHED();
	}
}
static uint64_t ShipLeaf(uint32_t op, uint32_t id, uint64_t a, uint64_t b, uint64_t c) noexcept
{
	Ship *v = id == UINT32_MAX ? nullptr : Ship::Get(VehicleID(id));
	(void)c;
	switch (op) {
		case SHIP_OP_DEPOT_DIR: { return GetShipDepotDirection(TileIndex(a)); }
		case SHIP_OP_DEPOT_AXIS: { return GetShipDepotAxis(TileIndex(a)); }
		case SHIP_OP_IS_DEPOT: { return IsShipDepotTile(TileIndex(a)); }
		case SHIP_OP_DEPOT_INDEX: { return GetDepotIndex(TileIndex(a)).base(); }
		case SHIP_OP_WAIT_UNBUNCH: { return v->IsWaitingForUnbunching(); }
		case SHIP_OP_CHAIN_DEPOT: { return v->IsChainInDepot(); }
		case SHIP_OP_SERVINT: { return Company::Get(v->owner)->settings.vehicle.servint_ships; }
		case SHIP_OP_NEEDS_SERVICE: { return v->NeedsAutomaticServicing(); }
		case SHIP_OP_MAX_DISTANCE: { return _settings_game.pf.yapf.maximum_go_to_depot_penalty / YAPF_TILE_LENGTH; }
		case SHIP_OP_TILE_VALID: { return IsValidTile(TileIndex(a)); }
		case SHIP_OP_TILE_TYPE: { return GetTileType(TileIndex(a)); }
		case SHIP_OP_WATER_CLASS: { return to_underlying(GetEffectiveWaterClass(TileIndex(a))); }
		case SHIP_OP_LOCK_MIDDLE: { return IsTileType(TileIndex(a), MP_WATER) && IsLock(TileIndex(a)) && GetLockPart(TileIndex(a)) == LockPart::Middle; }
		case SHIP_OP_LOCK_DIR: { return GetInclinedSlopeDirection(GetTileSlope(TileIndex(a))); }
		case SHIP_OP_TILE_MIN_Z: { return GetTileZ(TileIndex(a)); }
		case SHIP_OP_TILE_MAX_Z: { return GetTileMaxZ(TileIndex(a)); }
		case SHIP_OP_TRACK_STATUS: { return TrackStatusToTrackBits(GetTileTrackStatus(TileIndex(a), TRANSPORT_WATER, 0, static_cast<DiagDirection>(b))); }
		case SHIP_OP_OFFSET: { return static_cast<uint32_t>(TileOffsByDiagDir(static_cast<DiagDirection>(a))); }
		case SHIP_OP_DIAG_BETWEEN: { return DiagdirBetweenTiles(TileIndex(a), TileIndex(b)); }
		case SHIP_OP_DIST_SQUARE: { return DistanceSquare(TileIndex(a), TileIndex(b)); }
		case SHIP_OP_DIST_MANHATTAN: { return DistanceManhattan(TileIndex(a), TileIndex(b)); }
		case SHIP_OP_DOCKING: { return IsDockingTile(TileIndex(a)); }
		case SHIP_OP_DOCK: { return IsDockTile(TileIndex(a)); }
		case SHIP_OP_DOCK_WATER: { return IsDockWaterPart(TileIndex(a)); }
		case SHIP_OP_STATION: { return GetStationIndex(TileIndex(a)).base(); }
		case SHIP_OP_INDUSTRY_STATION: { const Industry *i = Industry::GetByTile(TileIndex(a)); return i->neutral_station == nullptr ? UINT32_MAX : i->neutral_station->index.base(); }
		case SHIP_OP_OILRIG: { return IsTileType(TileIndex(a), MP_STATION) && IsOilRig(TileIndex(a)); }
		case SHIP_OP_STATION_USE: { return CanVehicleUseStation(v, Station::Get(StationID(a))); }
		case SHIP_OP_STATION_XY: { return Station::Get(StationID(a))->xy.base(); }
		case SHIP_OP_STATION_CONTAINS: { return Station::Get(StationID(a))->docking_station.Contains(TileIndex(b)); }
		case SHIP_OP_STATION_DOCK: { return Station::Get(StationID(a))->facilities.Test(StationFacility::Dock); }
		case SHIP_OP_STATION_VISITS: { return Station::Get(StationID(a))->had_vehicle_of_type; }
		case SHIP_OP_VISIT_SET: { Station::Get(StationID(a))->had_vehicle_of_type |= static_cast<StationHadVehicleOfType>(b); break; }
		case SHIP_OP_ARRIVAL: { AddVehicleNewsItem(GetEncodedString(STR_NEWS_FIRST_SHIP_ARRIVAL, StationID(a)), v->owner == _local_company ? NewsType::ArrivalCompany : NewsType::ArrivalOther, v->index, StationID(a)); AI::NewEvent(v->owner, new ScriptEventStationFirstVehicle(StationID(a), v->index)); Game::NewEvent(new ScriptEventStationFirstVehicle(StationID(a), v->index)); break; }
		case SHIP_OP_SERVICE: { VehicleServiceInDepot(v); break; }
		case SHIP_OP_LEAVE_UNBUNCH: { v->LeaveUnbunchingDepot(); break; }
		case SHIP_OP_PATH_RESULT: { v->HandlePathfindingResult(a != 0); break; }
		case SHIP_OP_ORDER_FREE: { v->current_order.Free(); break; }
		case SHIP_OP_ORDER_DUMMY: { v->current_order.MakeDummy(); break; }
		case SHIP_OP_ORDER_DEPOT: { v->current_order.MakeGoToDepot(DepotID(a), OrderDepotTypeFlag::Service); break; }
		case SHIP_OP_ORDER_LEAVE: { v->current_order.MakeLeaveStation(); break; }
		case SHIP_OP_ORDER_INCREMENT: { v->IncrementRealOrderIndex(); break; }
		case SHIP_OP_TIMETABLE: { UpdateVehicleTimetable(v, true); break; }
		case SHIP_OP_POSITION: { v->UpdatePosition(); break; }
		case SHIP_OP_START_DIRTY: { SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP); break; }
		case SHIP_OP_DEPOT_DIRTY: { SetWindowDirty(WC_VEHICLE_DEPOT, v->tile); break; }
		case SHIP_OP_DEPOT_INVALIDATE: { InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile); break; }
		case SHIP_OP_SHIPS_DIRTY: { SetWindowClassesDirty(WC_SHIPS_LIST); break; }
		case SHIP_OP_DETAILS_DIRTY: { SetWindowDirty(WC_VEHICLE_DETAILS, v->index); break; }
		case SHIP_OP_AGE: { AgeVehicle(v); break; }
		case SHIP_OP_ECONOMY_AGE: { EconomyAgeVehicle(v); break; }
		case SHIP_OP_DECREASE_VALUE: { DecreaseVehicleValue(v); break; }
		case SHIP_OP_CHECK_BREAKDOWN: { CheckVehicleBreakdown(v); break; }
		case SHIP_OP_CHECK_ORDERS: { CheckOrders(v); break; }
		case SHIP_OP_RUNNING_COST: { return static_cast<uint64_t>(v->GetRunningCost()); }
		case SHIP_OP_COST_DIVISOR: { return CalendarTime::DAYS_IN_YEAR * Ticks::DAY_TICKS; }
		case SHIP_OP_PAY_RUNNING: { CommandCost cost(EXPENSES_SHIP_RUN, Money(static_cast<int64_t>(a))); v->profit_this_year -= cost.GetCost(); v->running_ticks = 0; SubtractMoneyFromCompanyFract(v->owner, cost); break; }
		case SHIP_OP_SPEED_DEFAULT: { return ShipVehInfo(v->engine_type)->max_speed; }
		case SHIP_OP_AGE_DEFAULT: { return EngInfo(v->engine_type)->cargo_age_period; }
		case SHIP_OP_SPEED_FRAC: { return a != 0 ? ShipVehInfo(v->engine_type)->ocean_speed_frac : ShipVehInfo(v->engine_type)->canal_speed_frac; }
		case SHIP_OP_PROPERTY: { return GetVehicleProperty(v, a == 0 ? PROP_SHIP_SPEED : PROP_SHIP_CARGO_AGE_PERIOD, b); }
		case SHIP_OP_UPDATE_VISUAL: { v->UpdateVisualEffect(); break; }
		case SHIP_OP_CACHE_INVALIDATE: { v->InvalidateNewGRFCacheOfChain(); break; }
		case SHIP_OP_CAPACITY: { return v->GetEngine()->DetermineCapacity(v); }
		case SHIP_OP_BUILD_SHARED: {
			const Engine *e = Engine::Get(EngineID(b));
			const auto &svi = e->VehInfo<ShipVehicleInfo>();
			switch (a) {
				case 0: v->owner = _current_company; break;
				case 1: v->z_pos = GetSlopePixelZ(v->x_pos, v->y_pos); break;
				case 2:
					v->vehstatus = {VehState::Hidden, VehState::Stopped, VehState::DefaultPalette};
					v->spritenum = svi.image_index;
					v->cargo_type = e->GetDefaultCargoType();
					assert(IsValidCargoType(v->cargo_type));
					v->cargo_cap = svi.capacity;
					v->refit_cap = 0;
					v->last_station_visited = StationID::Invalid();
					v->last_loading_station = StationID::Invalid();
					v->engine_type = e->index;
					v->reliability = e->reliability;
					v->reliability_spd_dec = e->reliability_spd_dec;
					v->max_age = e->GetLifeLengthInDays();
					break;
				case 3:
					v->SetServiceInterval(Company::Get(_current_company)->settings.vehicle.servint_ships);
					v->date_of_last_service = TimerGameEconomy::date;
					v->date_of_last_service_newgrf = TimerGameCalendar::date;
					v->build_year = TimerGameCalendar::year;
					v->sprite_cache.sprite_seq.Set(SPR_IMG_QUERY);
					break;
				case 4: v->acceleration = svi.acceleration; break;
				case 5: v->vehicle_flags.Set(VehicleFlag::BuiltAsPrototype); break;
				case 6: v->SetServiceIntervalIsPercent(Company::Get(_current_company)->settings.vehicle.servint_ispercent); break;
				case 7: v->cargo_cap = static_cast<uint16_t>(c); break;
				default: NOT_REACHED();
			}
			break;
		}
		case SHIP_OP_SPRITE_DIRECTION: { v->sprite_cache.last_direction = INVALID_DIR; break; }
		case SHIP_OP_TILE_X: return TileX(TileIndex(a));
		case SHIP_OP_TILE_Y: return TileY(TileIndex(a));
		case SHIP_OP_BUILD_FLAG: return Engine::Get(EngineID(a))->flags.Test(EngineFlag::ExclusivePreview);
		case SHIP_OP_BUILD_RANDOM: v->random_bits = static_cast<uint8_t>(a); break;
		case SHIP_OP_ADVANCE: { return v->GetAdvanceDistance(); }
		case SHIP_OP_NEW_POSITION: { auto gp = GetNewVehiclePos(v); return static_cast<uint32_t>(gp.x) | (static_cast<uint64_t>(static_cast<uint32_t>(gp.y)) << 32); }
		case SHIP_OP_VIRT_TILE: { return TileVirtXY(static_cast<int32_t>(a), static_cast<int32_t>(b)).base(); }
		case SHIP_OP_EXIT_DIR: { return VehicleExitDir(static_cast<Direction>(a), static_cast<TrackBits>(b)); }
		case SHIP_OP_TRACK_DIRECTION: { return TrackDirectionToTrackdir(static_cast<Track>(a), static_cast<Direction>(b)); }
		case SHIP_OP_TRACKS_REACH: { return DiagdirReachesTracks(static_cast<DiagDirection>(a)); }
		case SHIP_OP_BUSY_TILE: { return HasVehicleOnTile(v->tile, [](const Vehicle *u) { return u->type == VEH_SHIP && u->cur_speed != 0; }); }
		case SHIP_OP_PATH_SIZE: { return v->path.size(); }
		case SHIP_OP_PATH_BACK: { return v->path.back().trackdir; }
		case SHIP_OP_PATH_POP: { v->path.pop_back(); break; }
		case SHIP_OP_PATH_CLEAR: { v->path.clear(); break; }
		default: NOT_REACHED();
	}
	return 0;
}
static uint64_t ShipAction(const OpenTTDShipAction &action)
{
	Ship *v = Ship::Get(VehicleID(action.id));
	uint64_t a = action.a, b = action.b, c = action.c;
	switch (action.op) {
		case SHIP_OP_ENTER_TILE: { return VehicleEnterTile(v, TileIndex(a), static_cast<int32_t>(b), static_cast<int32_t>(c)).base(); }
		case SHIP_OP_ENTER_DEPOT: { VehicleEnterDepot(v); break; }
		case SHIP_OP_PROCESS_ORDERS: { return ProcessOrders(v); }
		case SHIP_OP_LOADING: { v->HandleLoading(); break; }
		case SHIP_OP_BEGIN_LOADING: { v->BeginLoading(); break; }
		case SHIP_OP_BREAKDOWN: { return v->HandleBreakdown(); }
		case SHIP_OP_VIEWPORT: { v->UpdateViewport(a != 0, b != 0); break; }
		case SHIP_OP_BASE_VIEWPORT: { v->Vehicle::UpdateViewport(true); break; }
		case SHIP_OP_VISUAL: { v->ShowVisualEffect(); break; }
		case SHIP_OP_CACHE: { v->UpdateCache(); break; }
		case SHIP_OP_PLAY_SOUND: { v->PlayLeaveStationSound(); break; }
		case SHIP_OP_YAPF_REVERSE: { Trackdir dir = INVALID_TRACKDIR; bool reverse = YapfShipCheckReverse(v, a != 0 ? &dir : nullptr); return static_cast<uint64_t>(reverse) | (static_cast<uint64_t>(dir) << 8); }
		case SHIP_OP_YAPF_CHOOSE: { bool found = true; Track track = YapfShipChooseTrack(v, TileIndex(a), found, v->path); return track | (static_cast<uint64_t>(found) << 8); }
		case SHIP_OP_UPDATE_DELTA: { v->UpdateDeltaXY(); break; }
		default: NOT_REACHED();
	}
	return 0;
}
static OpenTTDShipState *ShipOwner(uint32_t id) noexcept { return Ship::Get(VehicleID(id))->GetRustState(); }
static OpenTTDWaterPatch ShipPatch(uint32_t tile) noexcept
{
	auto p = GetWaterRegionPatchInfo(TileIndex(tile));
	return {p.x, p.y, p.label.base()};
}
static size_t ShipNeighbours(OpenTTDWaterPatch patch, OpenTTDWaterPatch *out) noexcept
{
	size_t count = 0;
	VisitWaterRegionPatchCallback visitor = [&](const WaterRegionPatchDesc &p) { out[count++] = {p.x, p.y, p.label.base()}; };
	VisitWaterRegionPatchNeighbours({patch.x, patch.y, WaterRegionPatchLabel(patch.label)}, visitor);
	return count;
}
static size_t ShipDepots(OpenTTDShipDepot *out, size_t capacity) noexcept
{
	size_t count = 0;
	for (const Depot *d : Depot::Iterate()) {
		if (out != nullptr && count < capacity) out[count] = {d->index.base(), d->xy.base(), IsShipDepotTile(d->xy) ? GetTileOwner(d->xy).base() : UINT32_MAX, static_cast<uint32_t>(IsShipDepotTile(d->xy))};
		++count;
	}
	return count;
}
static uint64_t RunRustShip(uint32_t kind, const Ship *v, uint64_t a, uint64_t b, uint64_t c)
{
	static const OpenTTDShipLeaves leaves{ShipObserve, ShipWrite, ShipLeaf, ShipOwner, ShipPatch, ShipNeighbours, ShipDepots};
	std::unique_ptr<void, decltype(&openttd_rust_ship_control_destroy)> task(openttd_rust_ship_control_create(kind, v == nullptr ? UINT32_MAX : v->index.base(), a, b, c, &leaves, &GetRustSharedServices()), openttd_rust_ship_control_destroy);
	uint64_t response = 0;
	for (;;) {
		auto action = openttd_rust_ship_control_advance(task.get(), response);
		if (action.op == 0) return action.a;
		response = ShipAction(action);
	}
}

/* Opt-in branch receipts are checked by ships.py alongside semantic saves. */
struct ShipControlProfile {
	bool enabled = std::getenv("OPENTTD_SHIP_PROFILE") != nullptr;
	ShipControlProfile() { if (this->enabled) openttd_rust_ship_control_profile_enable(); }
	~ShipControlProfile()
	{
		if (!this->enabled) return;
		const char *personal = std::getenv("HOME");
		if (personal == nullptr) return;
		if (FILE *file = std::fopen(fmt::format("{}/ship-control-profile.json", personal).c_str(), "w")) {
			constexpr const char *names[] = {"lock_up", "lock_down", "reverse", "rotate", "depot_leave", "auto_service", "buoy", "loading", "water_change", "aqueduct", "rotation_reload", "depot_search", "build", "economy_day", "path_cache", "depot_enter"};
			fmt::print(file, "{{");
			for (uint8_t i = 0; i < 16; ++i) fmt::print(file, "{}\"{}\":{}", i == 0 ? "" : ",", names[i], openttd_rust_ship_control_profile(i));
			fmt::print(file, "}}\n");
			std::fclose(file);
		}
	}
};
static ShipControlProfile _ship_control_profile;
