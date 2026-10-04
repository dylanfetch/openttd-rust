/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */
/** @file aircraft_adapter.hpp Canonical pool observations and direct shared leaves. */
#include "aircraft_ffi.h"
static_assert(AIR_FAST == 2 && AIR_CTOL == 1 && ROTOR_Z_OFFSET == 5);
static_assert(HRS_ROTOR_STOPPED == 0 && HRS_ROTOR_MOVING_3 == 3);
static_assert(VehicleID::Invalid().base() == 1048575 && StationID::Invalid().base() == 65535);
static_assert(EV_BREAKDOWN_SMOKE_AIRCRAFT == 10 && EV_EXPLOSION_LARGE == 5 && EV_EXPLOSION_SMALL == 7);
static_assert(HVOT_AIRCRAFT == 16);
static_assert(OWNER_NONE.base() == 16);

/* These callbacks expose records and single shared operations, never FTA policy.
 * Record pointers are opaque, stable airport descriptions; Rust never interprets
 * their C++ layout. All world state stays in the existing pools. */
static void OPENTTD_AIRCRAFT_CALL AircraftRead(void *, uint32_t kind, uint32_t id, int64_t a, int64_t b, int64_t *out) noexcept
{
	switch (kind) {
		case 0:
			out[0] = Map::SizeX(); out[1] = Map::SizeY(); out[2] = Map::MaxX(); out[3] = Map::MaxY();
			out[4] = _settings_game.vehicle.plane_speed; out[5] = _cheats.no_jetcrash.value; out[6] = _settings_game.vehicle.plane_crashes;
			out[7] = _settings_game.order.serviceathelipad; out[8] = _settings_client.sound.disaster; out[9] = _local_company.base();
			out[10] = ORIGINAL_SAMPLE_COUNT; out[11] = SND_18_TAKEOFF_HELICOPTER; out[12] = SND_12_EXPLOSION;
			out[13] = TimerGameEconomy::date.base(); out[14] = TimerGameCalendar::date.base(); out[15] = SND_17_SKID_PLANE;
			out[16] = CalendarTime::DAYS_IN_YEAR * Ticks::DAY_TICKS;
			break;
		case 1: {
			const Vehicle *v = Vehicle::Get(id);
			out[0] = v->subtype; out[1] = v->x_pos; out[2] = v->y_pos; out[3] = v->z_pos; out[4] = v->tile.base();
			out[5] = v->direction; out[6] = v->tick_counter; out[7] = v->owner.base(); out[8] = v->vehstatus.base();
			out[9] = v->Next() == nullptr ? VehicleID::Invalid().base() : v->Next()->index.base();
			out[10] = v->cur_speed; out[11] = v->subspeed; out[12] = v->progress; out[13] = v->acceleration; out[14] = v->vcache.cached_max_speed;
			out[15] = v->breakdown_ctr; out[34] = v->type;
			if (v->type != VEH_AIRCRAFT || !Aircraft::From(v)->IsNormalAircraft()) break;
			const Aircraft *av = Aircraft::From(v);
			out[16] = v->current_order.GetType(); out[17] = v->current_order.GetDestination().base();
			out[18] = v->running_ticks; out[19] = av->current_order_time; out[20] = v->day_counter; out[21] = v->profit_this_year;
			out[22] = v->last_station_visited.base(); out[23] = v->date_of_last_service.base(); out[24] = v->date_of_last_service_newgrf.base();
			out[25] = v->breakdowns_since_last_service; out[26] = v->reliability; out[27] = v->vcache.cached_cargo_age_period; out[28] = v->engine_type.base();
			break;
		}
		case 2: {
			const Station *st = Station::GetIfValid(id); if (st == nullptr) break;
			out[0] = 1; out[1] = st->airport.tile.base(); out[2] = st->xy.base(); out[3] = st->airport.rotation;
			out[4] = st->airport.w; out[5] = st->airport.h; out[6] = st->airport.type; out[7] = st->owner.base();
			out[8] = st->airport.HasHangar(); out[9] = st->facilities.Test(StationFacility::Airport);
			out[10] = reinterpret_cast<intptr_t>(st->airport.GetFTA()); out[11] = reinterpret_cast<intptr_t>(st->airport.rust_blocks.get()); out[12] = st->had_vehicle_of_type;
			break;
		}
		case 3: {
			const AirportFTAClass *ap = a == 0 ? GetAirport(AT_DUMMY) : reinterpret_cast<const AirportFTAClass *>(static_cast<intptr_t>(a));
			out[0] = reinterpret_cast<intptr_t>(ap); out[1] = ap->nofelements; out[2] = ap->num_helipads; out[3] = ap->flags.base(); out[4] = ap->delta_z;
			break;
		}
		case 4: out[0] = reinterpret_cast<intptr_t>(&reinterpret_cast<const AirportFTAClass *>(static_cast<intptr_t>(a))->layout[static_cast<uint8_t>(b)]); break;
		case 5: {
			const AirportFTA *node = reinterpret_cast<const AirportFTA *>(static_cast<intptr_t>(a));
			out[0] = reinterpret_cast<intptr_t>(node->next.get()); out[1] = node->position; out[2] = node->next_position; out[3] = std::bit_cast<int64_t>(node->blocks.base()); out[4] = node->heading;
			break;
		}
		case 6: {
			const AirportMovingData *amd = reinterpret_cast<const AirportFTAClass *>(static_cast<intptr_t>(a))->MovingData(b);
			out[0] = amd->x; out[1] = amd->y; out[2] = amd->flags.base(); out[3] = amd->direction; break;
		}
		case 8: out[0] = reinterpret_cast<intptr_t>(Aircraft::Get(id)->rust_state.get()); break;
		case 9: out[0] = GetSlopePixelZ(a, b); break;
		case 10: out[0] = TileHeight(TileIndex{id}) * TILE_HEIGHT; break;
		case 11: {
			const Aircraft *v = Aircraft::Get(id); const auto *avi = AircraftVehInfo(v->engine_type);
			out[0] = avi->max_speed; out[1] = avi->subtype; out[2] = avi->max_range; out[3] = avi->sfx; out[4] = v->GetEngine()->reliability; break;
		}
		case 12: out[0] = reinterpret_cast<const AirportFTAClass *>(static_cast<intptr_t>(a))->entry_points[static_cast<uint8_t>(b)]; break;
		case 13: out[0] = GetDirectionTowards(Vehicle::Get(id), a, b); break;
		case 14: { auto gp = GetNewVehiclePos(Vehicle::Get(id)); out[0] = gp.x; out[1] = gp.y; out[2] = gp.new_tile.base(); break; }
		case 15: out[0] = GetTileMaxPixelZ(Station::Get(id)->airport.GetHangarTile(0)); break;
		case 16: out[0] = reinterpret_cast<const AirportFTAClass *>(static_cast<intptr_t>(a))->terminals[id]; break;
		case 17: { const Vehicle *v = Vehicle::Get(id); out[0] = Station::GetByTile(v->tile)->airport.GetHangarExitDirection(v->tile); break; }
		case 18: out[0] = CanVehicleUseStation(Vehicle::Get(id), Station::Get(static_cast<uint32_t>(a))); break;
		case 19: out[0] = Company::Get(Vehicle::Get(id)->owner)->settings.vehicle.servint_aircraft; break;
		case 20:
			if (id == 0) { for (const Station *st : Station::Iterate(static_cast<uint32_t>(a) == 65535 ? 0 : a + 1)) { out[0] = st->index.base(); return; } out[0] = 65535; }
			else { for (const Aircraft *v : Aircraft::Iterate(static_cast<uint32_t>(a) == 1048575 ? 0 : a + 1)) { out[0] = v->index.base(); return; } out[0] = 1048575; }
			break;
		case 21: out[0] = Aircraft::Get(id)->NeedsAutomaticServicing(); break;
		case 22: out[0] = Aircraft::Get(id)->IsChainInDepot(); break;
		case 23: out[0] = Aircraft::Get(id)->IsWaitingForUnbunching(); break;
		case 24: out[0] = Aircraft::Get(id)->current_order.GetDepotActionType().Test(OrderDepotActionFlag::NearestDepot); break;
		case 25: out[0] = Aircraft::Get(id)->current_order.GetDepotOrderType().Test(OrderDepotTypeFlag::PartOfOrders); break;
	}
}

static void OPENTTD_AIRCRAFT_CALL AircraftWrite(void *, uint32_t field, uint32_t id, int64_t value) noexcept
{
	Vehicle *v = Vehicle::Get(id);
	switch (field) {
		case 0: v->subtype = value; break;
		case 1: v->x_pos = value; break;
		case 2: v->y_pos = value; break;
		case 3: v->z_pos = value; break;
		case 4: v->tile = TileIndex{static_cast<uint32_t>(value)}; break;
		case 5: v->direction = static_cast<Direction>(value); break;
		case 6: v->tick_counter = value; break;
		case 8: v->vehstatus = VehStates{static_cast<uint8_t>(value)}; break;
		case 10: v->cur_speed = value; break;
		case 11: v->subspeed = value; break;
		case 12: v->progress = value; break;
		case 14: v->vcache.cached_max_speed = value; break;
		case 15: v->breakdown_ctr = value; break;
		case 18: v->running_ticks = value; break;
		case 19: Aircraft::From(v)->current_order_time = value; break;
		case 20: v->day_counter = value; break;
		case 21: v->profit_this_year = value; break;
		case 22: v->last_station_visited = StationID{static_cast<uint16_t>(value)}; break;
		case 23: v->date_of_last_service = TimerGameEconomy::Date{static_cast<int32_t>(value)}; break;
		case 24: v->date_of_last_service_newgrf = TimerGameCalendar::Date{static_cast<int32_t>(value)}; break;
		case 25: v->breakdowns_since_last_service = value; break;
		case 26: v->reliability = value; break;
		case 27: v->vcache.cached_cargo_age_period = value; break;
	}
}

static int64_t OPENTTD_AIRCRAFT_CALL AircraftService(void *, const OpenTTDAircraftAction *request) noexcept
{
	const auto &s = *request;
	switch (s.kind) {
		case 1: { Aircraft *v = Aircraft::Get(s.id); v->UpdatePosition(); v->UpdateViewport(true, false); break; }
		case 2: { Aircraft *v = Aircraft::Get(s.id); GetRotorImage(v, EIT_ON_MAP, &v->Next()->Next()->sprite_cache.sprite_seq); break; }
		case 3: Vehicle::Get(s.id)->sprite_cache.sprite_seq.CopyWithoutPalette(Vehicle::Get(s.other)->sprite_cache.sprite_seq); break;
		case 4: Vehicle::Get(s.id)->UpdatePositionAndViewport(); break;
		case 5: {
			const Vehicle *v = Vehicle::Get(s.id);
			switch (s.other) {
				case 0: return GetVehicleProperty(v, PROP_AIRCRAFT_SPEED, 0);
				case 1: return GetVehicleProperty(v, PROP_AIRCRAFT_CARGO_AGE_PERIOD, EngInfo(v->engine_type)->cargo_age_period);
				case 2: return GetVehicleProperty(v, PROP_AIRCRAFT_RANGE, AircraftVehInfo(v->engine_type)->max_range);
			}
			break;
		}
		case 6: {
			Aircraft *v = Aircraft::Get(s.id); Aircraft *rotor = v->Next()->Next(); VehicleSpriteSeq seq;
			GetRotorImage(v, EIT_ON_MAP, &seq); if (s.other == 0 && rotor->sprite_cache.sprite_seq == seq) return 1;
			rotor->sprite_cache.sprite_seq = seq; break;
		}
		case 7: CreateEffectVehicleRel(Vehicle::Get(s.id), s.a, s.b, s.c, static_cast<EffectVehicleType>(s.other)); break;
		case 8: SetWindowWidgetDirty(WC_VEHICLE_VIEW, s.id, WID_VV_START_STOP); break;
		case 9: return PlayVehicleSound(Vehicle::Get(s.id), s.other == 0 ? VSE_START : VSE_TOUCHDOWN);
		case 10: SndPlayVehicleFx(static_cast<SoundID>(s.a), Vehicle::Get(s.id)); break;
		case 11: Vehicle::Get(s.id)->cargo.Truncate(); break;
		case 12: {
			Aircraft *v = Aircraft::Get(s.id); StationID station{static_cast<uint16_t>(s.other)};
			TileIndex vt = TileVirtXYClampedToMap(v->x_pos, v->y_pos); bool missing = station == StationID::Invalid();
			EncodedString headline = missing ? GetEncodedString(STR_NEWS_PLANE_CRASH_OUT_OF_FUEL, s.a) : GetEncodedString(STR_NEWS_AIRCRAFT_CRASH, s.a, station);
			AI::NewEvent(v->owner, new ScriptEventVehicleCrashed(v->index, vt, missing ? ScriptEventVehicleCrashed::CRASH_AIRCRAFT_NO_AIRPORT : ScriptEventVehicleCrashed::CRASH_PLANE_LANDING, s.a, v->owner));
			Game::NewEvent(new ScriptEventVehicleCrashed(v->index, vt, missing ? ScriptEventVehicleCrashed::CRASH_AIRCRAFT_NO_AIRPORT : ScriptEventVehicleCrashed::CRASH_PLANE_LANDING, s.a, v->owner));
			AddTileNewsItem(std::move(headline), v->owner == _local_company ? NewsType::Accident : NewsType::AccidentOther, vt, station); break;
		}
		case 13: ModifyStationRatingAround(TileIndex{s.id}, CompanyID{static_cast<uint8_t>(s.other)}, s.a, s.b); break;
		case 14: { GoodsEntry &ge = Station::Get(s.id)->goods[s.other]; ge.rating = 1; if (ge.HasData()) ge.GetData().cargo.Truncate(); break; }
		case 15: Aircraft::Get(s.id)->current_order.Free(); break;
		case 16: VehicleServiceInDepot(Vehicle::Get(s.id)); break;
		case 17: Aircraft::Get(s.id)->LeaveUnbunchingDepot(); break;
		case 18: InvalidateWindowData(WC_VEHICLE_DEPOT, Vehicle::Get(s.id)->tile); SetWindowClassesDirty(WC_AIRCRAFT_LIST); break;
		case 19: {
			Aircraft *v = Aircraft::Get(s.id); Station *st = Station::Get(s.other); st->had_vehicle_of_type |= HVOT_AIRCRAFT;
			AddVehicleNewsItem(GetEncodedString(STR_NEWS_FIRST_AIRCRAFT_ARRIVAL, st->index), v->owner == _local_company ? NewsType::ArrivalCompany : NewsType::ArrivalOther, v->index, st->index);
			AI::NewEvent(v->owner, new ScriptEventStationFirstVehicle(st->index, v->index)); Game::NewEvent(new ScriptEventStationFirstVehicle(st->index, v->index)); break;
		}
		case 20: Vehicle::Get(s.id)->BeginLoading(); break;
		case 21: SetWindowDirty(WC_VEHICLE_DETAILS, s.id); break;
		case 22: Vehicle::Get(s.id)->UpdateDeltaXY(); break;
		case 23: { Aircraft *v = Aircraft::Get(s.id); TriggerAirportTileAnimation(Station::Get(s.other), TileVirtXY(v->x_pos, v->y_pos), AirportAnimationTrigger::AirplaneTouchdown); break; }
		case 24: {
			Aircraft *v = Aircraft::Get(s.id); AI::NewEvent(v->owner, new ScriptEventAircraftDestTooFar(v->index));
			if (v->owner == _local_company) AddVehicleAdviceNewsItem(AdviceType::AircraftDestinationTooFar, GetEncodedString(STR_NEWS_AIRCRAFT_DEST_TOO_FAR, v->index), v->index);
			break;
		}
		case 25: DeleteVehicleNews(VehicleID{s.id}, AdviceType::AircraftDestinationTooFar); break;
		case 26: Vehicle::Get(s.id)->HandleBreakdown(); break;
		case 27: Vehicle::Get(s.id)->HandleLoading(s.other != 0); break;
		case 28: Aircraft::Get(s.id)->current_order.MakeGoToDepot(StationID{static_cast<uint16_t>(s.other)}, OrderDepotTypeFlag::Service); break;
		case 29: Aircraft::Get(s.id)->current_order.MakeDummy(); break;
		case 30: AgeVehicle(Vehicle::Get(s.id)); break;
		case 31: EconomyAgeVehicle(Vehicle::Get(s.id)); break;
		case 32: DecreaseVehicleValue(Vehicle::Get(s.id)); break;
		case 33: CheckOrders(Vehicle::Get(s.id)); break;
		case 34: CheckVehicleBreakdown(Vehicle::Get(s.id)); break;
		case 35: return Aircraft::Get(s.id)->GetRunningCost();
		case 38: SubtractMoneyFromCompanyFract(Vehicle::Get(s.id)->owner, CommandCost(EXPENSES_AIRCRAFT_RUN, Money{s.a})); break;
		case 39: SetWindowDirty(WC_VEHICLE_DETAILS, s.id); SetWindowClassesDirty(WC_AIRCRAFT_LIST); break;
		case 40: { std::vector<StationID> next_station; Aircraft::Get(s.id)->GetNextStoppingStation(next_station); return next_station.empty() ? StationID::Invalid().base() : next_station.back().base(); }
		case 41: RemoveOrderFromAllVehicles(OT_GOTO_DEPOT, StationID{static_cast<uint16_t>(s.id)}, true); break;
		case 44: assert(Aircraft::Get(s.id)->state == FLYING); break;
		case 45: Debug(misc, 0, "[Ap] cannot move further on Airport! (pos {} state {}) for vehicle {}", Aircraft::Get(s.id)->pos, Aircraft::Get(s.id)->state, s.id); NOT_REACHED();
		case 46: Debug(misc, 0, "[Ap] position {} is not valid for current airport. Max position is {}", Aircraft::Get(s.id)->pos, reinterpret_cast<const AirportFTAClass *>(static_cast<intptr_t>(s.a))->nofelements - 1); assert(Aircraft::Get(s.id)->pos < reinterpret_cast<const AirportFTAClass *>(static_cast<intptr_t>(s.a))->nofelements); break;
		case 47: FatalError("OK, you shouldn't be here, check your Airport Scheme!");
		case 48: NOT_REACHED();
	}
	return 0;
}

static int64_t RunAircraft(uint32_t operation, uint32_t id, int64_t a = 0, int64_t b = 0, int64_t c = 0, int64_t d = 0)
{
	std::unique_ptr<OpenTTDAircraftRun, decltype(&openttd_rust_aircraft_destroy)> run{openttd_rust_aircraft_create(operation, id, a, b, c, d, nullptr, AircraftRead, AircraftWrite, &GetRustSharedServices(), AircraftService), openttd_rust_aircraft_destroy};
	int64_t response = 0;
	for (;;) {
		auto s = openttd_rust_aircraft_advance(run.get(), response); response = 0;
		switch (s.kind) {
			case 0: return s.a;
			case 100: ProcessOrders(Aircraft::Get(s.id)); break; // UpdateOrderDest may reenter aircraft methods.
			case 102: VehicleEnterDepot(Aircraft::Get(s.id)); break; // HandleAircraftEnterHangar and refit/cache reentry.
			case 103: response = Aircraft::Get(s.id)->Vehicle::Crash(s.other != 0); break;
			case 104: delete Aircraft::Get(s.id); break;
			case 105: {
				Backup<CompanyID> cur_company(_current_company, Aircraft::Get(s.id)->owner);
				response = Command<CMD_SEND_VEHICLE_TO_DEPOT>::Do(DoCommandFlag::Execute, VehicleID{s.id}, s.other == 0 ? DepotCommandFlags{} : DepotCommandFlag::Service, {}).Failed();
				cur_company.Restore(); break;
			}
		}
	}
}

bool Aircraft::Tick() { PerformanceAccumulator framerate(PFE_GL_AIRCRAFT); return RunAircraft(0, this->index.base()) != 0; }
void Aircraft::OnNewCalendarDay() { RunAircraft(1, this->index.base()); }
void Aircraft::OnNewEconomyDay() { RunAircraft(2, this->index.base()); }
void SetAircraftPosition(Aircraft *v, int x, int y, int z) { RunAircraft(3, v->index.base(), x, y, z); }
void HandleAircraftEnterHangar(Aircraft *v) { RunAircraft(4, v->index.base()); }
void UpdateAircraftCache(Aircraft *v, bool update_range) { RunAircraft(5, v->index.base(), update_range); }
void GetAircraftFlightLevelBounds(const Vehicle *v, int *min, int *max) { if (min != nullptr) *min = RunAircraft(6, v->index.base()); if (max != nullptr) *max = RunAircraft(6, v->index.base(), 1); }
static uint8_t &GetFlightFlags(Aircraft *v) { return v->flags; }
static uint8_t &GetFlightFlags(DisasterVehicle *v) { return v->Flags(); }
template <class T> int GetAircraftFlightLevel(T *v, bool takeoff) { return RunAircraft(7, v->index.base(), takeoff, reinterpret_cast<intptr_t>(&GetFlightFlags(v))); }
template int GetAircraftFlightLevel(DisasterVehicle *v, bool takeoff);
template int GetAircraftFlightLevel(Aircraft *v, bool takeoff);
int GetTileHeightBelowAircraft(const Vehicle *v) { return RunAircraft(16, v->index.base()); }
int GetAircraftHoldMaxAltitude(const Aircraft *v) { return RunAircraft(17, v->index.base()); }
void HandleMissingAircraftOrders(Aircraft *v) { RunAircraft(8, v->index.base()); }
uint Aircraft::Crash(bool flooded) { return RunAircraft(9, this->index.base(), flooded); }
void AircraftNextAirportPos_and_Order(Aircraft *v) { RunAircraft(10, v->index.base()); }
void AircraftLeaveHangar(Aircraft *v, Direction exit_dir) { RunAircraft(11, v->index.base(), exit_dir); }
TileIndex Aircraft::GetOrderStationLocation(StationID) { RunAircraft(12, this->index.base()); return TileIndex{}; }
void UpdateAirplanesOnNewStation(const Station *st) { RunAircraft(13, st->index.base()); }
ClosestDepot Aircraft::FindClosestDepot()
{
	StationID station{static_cast<uint16_t>(RunAircraft(15, this->index.base()))};
	return station == StationID::Invalid() ? ClosestDepot() : ClosestDepot(Station::Get(station)->xy, station);
}
Station *GetTargetAirportIfValid(const Aircraft *v)
{
	assert(v->type == VEH_AIRCRAFT);
	Station *st = Station::GetIfValid(v->targetairport);
	return st == nullptr || st->airport.tile == INVALID_TILE ? nullptr : st;
}

void ReleaseAircraftAirportBlocks(const Aircraft *v) { RunAircraft(19, v->index.base()); }
void InvalidateAircraftTargetStation(StationID station) { RunAircraft(20, station.base()); }
