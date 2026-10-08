/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file road_services.h Typed synchronous road world services. */
static uint32_t OPENTTD_ROAD_CALL RoadAccModel() noexcept
{
	return _settings_game.vehicle.roadveh_acceleration_model;
}
static uint32_t OPENTTD_ROAD_CALL RoadRoadSide() noexcept
{
	return _settings_game.vehicle.road_side;
}
static uint32_t OPENTTD_ROAD_CALL RoadTileType(uint32_t tile) noexcept
{
	return GetTileType(TileIndex(tile));
}
static uint32_t OPENTTD_ROAD_CALL RoadHasRoad(uint32_t id, uint32_t tile) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return HasTileAnyRoadType(TileIndex(tile), v->compatible_roadtypes);
}
static uint32_t OPENTTD_ROAD_CALL RoadTrackStatus(uint32_t id, uint32_t tile) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return GetTileTrackStatus(TileIndex(tile), TRANSPORT_ROAD, GetRoadTramType(v->roadtype));
}
static uint32_t OPENTTD_ROAD_CALL RoadTileOwner(uint32_t tile) noexcept
{
	return GetTileOwner(TileIndex(tile)).base();
}
static uint32_t OPENTTD_ROAD_CALL RoadDepotDir(uint32_t tile) noexcept
{
	return GetRoadDepotDirection(TileIndex(tile));
}
static uint32_t OPENTTD_ROAD_CALL RoadBayDir(uint32_t tile) noexcept
{
	return GetBayRoadStopDir(TileIndex(tile));
}
static uint32_t OPENTTD_ROAD_CALL RoadIsDepot(uint32_t tile) noexcept
{
	return IsRoadDepotTile(TileIndex(tile));
}
static uint32_t OPENTTD_ROAD_CALL RoadNormalRoad(uint32_t tile) noexcept
{
	return IsNormalRoadTile(TileIndex(tile));
}
static uint32_t OPENTTD_ROAD_CALL RoadRoadWorks(uint32_t tile) noexcept
{
	return HasRoadWorks(TileIndex(tile));
}
static uint32_t OPENTTD_ROAD_CALL RoadDisallowed(uint32_t tile) noexcept
{
	return GetDisallowedRoadDirections(TileIndex(tile));
}
static uint32_t OPENTTD_ROAD_CALL RoadBayStop(uint32_t tile) noexcept
{
	return IsBayRoadStopTile(TileIndex(tile));
}
static uint32_t OPENTTD_ROAD_CALL RoadIsDtStop(uint32_t tile) noexcept
{
	return IsDriveThroughStopTile(TileIndex(tile));
}
static uint32_t OPENTTD_ROAD_CALL RoadStopType(uint32_t tile) noexcept
{
	return to_underlying(GetRoadStopType(TileIndex(tile)));
}
static uint32_t OPENTTD_ROAD_CALL RoadFreeBay(uint32_t tile) noexcept
{
	return RoadStop::GetByTile(TileIndex(tile), GetRoadStopType(TileIndex(tile)))->HasFreeBay();
}
static uint32_t OPENTTD_ROAD_CALL RoadAnyRoadBits(uint32_t id, uint32_t tile, bool straight_only) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return GetAnyRoadBits(TileIndex(tile), GetRoadTramType(v->roadtype), straight_only != 0);
}
static uint32_t OPENTTD_ROAD_CALL RoadRoadBits(uint32_t id, uint32_t tile) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return GetRoadBits(TileIndex(tile), GetRoadTramType(v->roadtype));
}
static uint32_t OPENTTD_ROAD_CALL RoadOffset(uint8_t direction) noexcept
{
	return static_cast<uint32_t>(TileOffsByDiagDir(static_cast<DiagDirection>(direction)));
}
static uint32_t OPENTTD_ROAD_CALL RoadTileX(uint32_t tile) noexcept
{
	return TileX(TileIndex(tile));
}
static uint32_t OPENTTD_ROAD_CALL RoadTileY(uint32_t tile) noexcept
{
	return TileY(TileIndex(tile));
}
static uint32_t OPENTTD_ROAD_CALL RoadStation(uint32_t tile) noexcept
{
	return GetStationIndex(TileIndex(tile)).base();
}
static uint32_t OPENTTD_ROAD_CALL RoadContinuation(uint32_t tile, uint32_t other_tile) noexcept
{
	return RoadStop::IsDriveThroughRoadStopContinuation(TileIndex(tile), TileIndex(other_tile));
}
static uint32_t OPENTTD_ROAD_CALL RoadBridgeSpeed(uint32_t tile) noexcept
{
	return GetBridgeSpec(GetBridgeType(TileIndex(tile)))->speed;
}
static uint32_t OPENTTD_ROAD_CALL RoadMaxPenalty() noexcept
{
	return _settings_game.pf.yapf.maximum_go_to_depot_penalty;
}
static uint32_t OPENTTD_ROAD_CALL RoadServint(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return Company::Get(v->owner)->settings.vehicle.servint_roadveh;
}
static uint32_t OPENTTD_ROAD_CALL RoadNeedsService(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return v->NeedsAutomaticServicing();
}
static uint32_t OPENTTD_ROAD_CALL RoadWaitUnbunch(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return v->IsWaitingForUnbunching();
}
static uint32_t OPENTTD_ROAD_CALL RoadOrderStop(uint32_t id, uint32_t tile) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return v->current_order.ShouldStopAtStation(v, GetStationIndex(TileIndex(tile)));
}
static uint32_t OPENTTD_ROAD_CALL RoadRoadType(uint32_t id, uint32_t tile) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return GetRoadType(TileIndex(tile), GetRoadTramType(v->roadtype));
}
static uint32_t OPENTTD_ROAD_CALL RoadQueue() noexcept
{
	return _settings_game.pf.roadveh_queue;
}
static uint32_t OPENTTD_ROAD_CALL RoadTunnelDir(uint32_t tile) noexcept
{
	return GetTunnelBridgeDirection(TileIndex(tile));
}
static int32_t OPENTTD_ROAD_CALL RoadAcceleration(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return v->GetAcceleration();
}
static int32_t OPENTTD_ROAD_CALL RoadUpdateSpeed(uint32_t id, uint32_t acceleration, int32_t min_speed, int32_t max_speed) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return v->RustDoUpdateSpeed(static_cast<uint32_t>(acceleration), static_cast<int32_t>(min_speed), static_cast<int32_t>(max_speed));
}
static uint32_t OPENTTD_ROAD_CALL RoadAdvance(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return v->GetAdvanceDistance();
}
static void OPENTTD_ROAD_CALL RoadPosition(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->UpdatePosition();
}
static void OPENTTD_ROAD_CALL RoadBaseViewport(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->Vehicle::UpdateViewport(true);
}
static void OPENTTD_ROAD_CALL RoadLastSpeed(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->SetLastSpeed();
}
static void OPENTTD_ROAD_CALL RoadRoadstopLeave(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	RoadStop::GetByTile(v->tile, GetRoadStopType(v->tile))->Leave(v);
}
static void OPENTTD_ROAD_CALL RoadEntranceSet(uint32_t id, bool busy) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	RoadStop::GetByTile(v->tile, GetRoadStopType(v->tile))->SetEntranceBusy(busy != 0);
}
static uint32_t OPENTTD_ROAD_CALL RoadEntranceBusy(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return RoadStop::GetByTile(v->tile, GetRoadStopType(v->tile))->IsEntranceBusy();
}
static void OPENTTD_ROAD_CALL RoadOrderFree(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->current_order.Free();
}
static void OPENTTD_ROAD_CALL RoadSetNext(uint32_t id, uint32_t next) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->SetNext(next == UINT32_MAX ? nullptr : Vehicle::Get(VehicleID(next)));
}
static void OPENTTD_ROAD_CALL RoadStartStopDirty(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP);
}
static void OPENTTD_ROAD_CALL RoadDepotDirty(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
}
static void OPENTTD_ROAD_CALL RoadDetailsDirty(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	SetWindowDirty(WC_VEHICLE_DETAILS, v->index); SetWindowClassesDirty(WC_ROADVEH_LIST);
}
static void OPENTTD_ROAD_CALL RoadService(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	VehicleServiceInDepot(v);
}
static void OPENTTD_ROAD_CALL RoadLeaveUnbunch(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->LeaveUnbunchingDepot();
}
static void OPENTTD_ROAD_CALL RoadResetUnbunch(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->ResetDepotUnbunching();
}
static void OPENTTD_ROAD_CALL RoadPathResult(uint32_t id, bool found) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->HandlePathfindingResult(found != 0);
}
static void OPENTTD_ROAD_CALL RoadOrderDummy(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->current_order.MakeDummy();
}
static void OPENTTD_ROAD_CALL RoadOrderDepot(uint32_t id, uint16_t depot) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->current_order.MakeGoToDepot(DepotID(depot), OrderDepotTypeFlag::Service);
}
static uint32_t OPENTTD_ROAD_CALL RoadDepotIndex(uint32_t tile) noexcept
{
	return GetDepotIndex(TileIndex(tile)).base();
}
static void OPENTTD_ROAD_CALL RoadDecreaseValue(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	DecreaseVehicleValue(v);
}
static void OPENTTD_ROAD_CALL RoadAge(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	AgeVehicle(v);
}
static void OPENTTD_ROAD_CALL RoadEconomyAge(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	EconomyAgeVehicle(v);
}
static void OPENTTD_ROAD_CALL RoadCheckBreakdown(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	CheckVehicleBreakdown(v);
}
static void OPENTTD_ROAD_CALL RoadCheckOrders(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	CheckOrders(v);
}
static void OPENTTD_ROAD_CALL RoadPayRunning(uint32_t id, int64_t amount) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	CommandCost cost(EXPENSES_ROADVEH_RUN, Money(amount)); v->profit_this_year -= cost.GetCost(); v->running_ticks = 0; SubtractMoneyFromCompanyFract(v->owner, cost);
}
static uint32_t OPENTTD_ROAD_CALL RoadCostClass(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return static_cast<uint32_t>(v->GetEngine()->VehInfo<RoadVehicleInfo>().running_cost_class);
}
static uint32_t OPENTTD_ROAD_CALL RoadCostFactor(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return v->GetEngine()->VehInfo<RoadVehicleInfo>().running_cost;
}
static int64_t OPENTTD_ROAD_CALL RoadGetPrice(uint32_t id, uint64_t factor) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return static_cast<int64_t>(GetPrice(v->GetEngine()->VehInfo<RoadVehicleInfo>().running_cost_class, factor, v->GetEngine()->GetGRF()));
}
static uint32_t OPENTTD_ROAD_CALL RoadGrfVersion(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return v->GetEngine()->GetGRF() == nullptr ? 0 : v->GetEngine()->GetGRF()->grf_version;
}
static uint32_t OPENTTD_ROAD_CALL RoadLengthDefault(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return v->GetEngine()->VehInfo<RoadVehicleInfo>().shorten_factor;
}
static uint32_t OPENTTD_ROAD_CALL RoadAgeDefault(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return EngInfo(v->engine_type)->cargo_age_period;
}
static uint32_t OPENTTD_ROAD_CALL RoadSpeedDefault(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return RoadVehInfo(v->engine_type)->max_speed;
}
static void OPENTTD_ROAD_CALL RoadLengthError(uint32_t id, uint32_t result) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	ErrorUnknownCallbackResult(v->GetEngine()->GetGRFID(), CBID_VEHICLE_LENGTH, result);
}
static void OPENTTD_ROAD_CALL RoadDisconnect() noexcept
{
	FatalError("Disconnecting road vehicle.");
}
static void OPENTTD_ROAD_CALL RoadExplosion(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	CreateEffectVehicleRel(v, 4, 4, 8, EV_EXPLOSION_LARGE);
}
static uint32_t OPENTTD_ROAD_CALL RoadSoundDefault(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return RoadVehInfo(v->engine_type)->sfx;
}
static void OPENTTD_ROAD_CALL RoadSound(uint32_t id, uint16_t sound) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	SndPlayVehicleFx(static_cast<SoundID>(sound), v);
}
static uint32_t OPENTTD_ROAD_CALL RoadSoundOld1() noexcept
{
	return SND_19_DEPARTURE_OLD_RV_1;
}
static uint32_t OPENTTD_ROAD_CALL RoadSoundOld2() noexcept
{
	return SND_1A_DEPARTURE_OLD_RV_2;
}
static uint32_t OPENTTD_ROAD_CALL RoadEngineInvalid() noexcept
{
	return EngineID::Invalid().base();
}
static uint32_t OPENTTD_ROAD_CALL RoadInvalidPrice() noexcept
{
	return static_cast<uint32_t>(INVALID_PRICE);
}
static uint32_t OPENTTD_ROAD_CALL RoadCostDivisor() noexcept
{
	return CalendarTime::DAYS_IN_YEAR * Ticks::DAY_TICKS;
}
static uint32_t OPENTTD_ROAD_CALL RoadIsCrossing(uint32_t tile) noexcept
{
	return IsLevelCrossingTile(TileIndex(tile));
}
static OpenTTDRoadPosition OPENTTD_ROAD_CALL RoadNewPosition(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	auto gp = GetNewVehiclePos(v); return {gp.x, gp.y};
}
static uint32_t OPENTTD_ROAD_CALL RoadVirtTile(int32_t x, int32_t y) noexcept
{
	return TileVirtXY(static_cast<int32_t>(x), static_cast<int32_t>(y)).base();
}
static uint32_t OPENTTD_ROAD_CALL RoadIsRoadStop(uint32_t tile) noexcept
{
	return IsStationRoadStop(TileIndex(tile));
}
static void OPENTTD_ROAD_CALL RoadSetDest(uint32_t id, uint32_t tile) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->dest_tile = TileIndex(static_cast<uint32_t>(tile));
}
static void OPENTTD_ROAD_CALL RoadCacheInvalidate(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->InvalidateNewGRFCacheOfChain();
}
static void OPENTTD_ROAD_CALL RoadArrival(uint32_t id, uint16_t station, uint32_t headline, bool local) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	static const StringID headlines[] = {STR_NEWS_FIRST_BUS_ARRIVAL, STR_NEWS_FIRST_PASSENGER_TRAM_ARRIVAL, STR_NEWS_FIRST_TRUCK_ARRIVAL, STR_NEWS_FIRST_CARGO_TRAM_ARRIVAL};
			AddVehicleNewsItem(GetEncodedString(headlines[headline], StationID(station)), local != 0 ? NewsType::ArrivalCompany : NewsType::ArrivalOther, v->index, StationID(station));
			AI::NewEvent(v->owner, new ScriptEventStationFirstVehicle(StationID(station), v->index));
			Game::NewEvent(new ScriptEventStationFirstVehicle(StationID(station), v->index));
}
static void OPENTTD_ROAD_CALL RoadCrashNews(uint32_t id, uint32_t victims) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	RoadCrashNews(v, static_cast<uint32_t>(victims));
}
static uint32_t OPENTTD_ROAD_CALL RoadStationVisits(uint16_t station) noexcept
{
	return Station::Get(StationID(station))->had_vehicle_of_type;
}
static void OPENTTD_ROAD_CALL RoadStationVisitSet(uint16_t station, uint32_t vehicle_type) noexcept
{
	Station::Get(StationID(station))->had_vehicle_of_type |= static_cast<StationHadVehicleOfType>(vehicle_type);
}
static uint32_t OPENTTD_ROAD_CALL RoadLocalCompany() noexcept
{
	return _local_company.base();
}
static uint32_t OPENTTD_ROAD_CALL RoadEnterTile(uint32_t id, uint32_t tile, int32_t x, int32_t y) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return VehicleEnterTile(v, TileIndex(tile), static_cast<int32_t>(x), static_cast<int32_t>(y)).base();
}
static void OPENTTD_ROAD_CALL RoadEnterDepot(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	VehicleEnterDepot(v);
}
static void OPENTTD_ROAD_CALL RoadProcessOrders(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	ProcessOrders(v);
}
static void OPENTTD_ROAD_CALL RoadLoading(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->HandleLoading();
}
static void OPENTTD_ROAD_CALL RoadBeginLoading(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->BeginLoading();
}
static uint32_t OPENTTD_ROAD_CALL RoadTramProbe(uint32_t id, uint32_t tile, uint8_t bits) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	Backup<CompanyID> current(_current_company, v->owner); CommandCost result = Command<CMD_BUILD_ROAD>::Do(DoCommandFlag::NoWater, TileIndex(tile), static_cast<RoadBits>(bits), v->roadtype, DRD_NONE, TownID::Invalid()); current.Restore(); return result.Succeeded();
}
static uint32_t OPENTTD_ROAD_CALL RoadProperty(uint32_t id, uint8_t property, uint32_t fallback) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return GetVehicleProperty(v, static_cast<PropertyID>(property), fallback);
}
static uint32_t OPENTTD_ROAD_CALL RoadLengthCallback(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return GetVehicleCallback(CBID_VEHICLE_LENGTH, 0, 0, v->engine_type, v);
}
static uint32_t OPENTTD_ROAD_CALL RoadPlaySound(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return PlayVehicleSound(v, VSE_START);
}
static void OPENTTD_ROAD_CALL RoadVisual(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->ShowVisualEffect();
}
static void OPENTTD_ROAD_CALL RoadUpdateVisual(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->UpdateVisualEffect();
}
static void OPENTTD_ROAD_CALL RoadCargoChanged(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->CargoChanged();
}
static void OPENTTD_ROAD_CALL RoadLengthChanged(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	VehicleLengthChanged(v);
}
static uint32_t OPENTTD_ROAD_CALL RoadBreakdown(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return v->HandleBreakdown();
}
static void OPENTTD_ROAD_CALL RoadDelete(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	delete v;
}
static uint32_t OPENTTD_ROAD_CALL RoadGroundCrash(uint32_t id, bool flooded) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return v->GroundVehicleBase::Crash(flooded != 0);
}
static void OPENTTD_ROAD_CALL RoadStopRandom(uint32_t id, uint16_t station) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	TriggerRoadStopRandomisation(Station::Get(StationID(station)), v->tile, StationRandomTrigger::VehicleArrives);
}
static void OPENTTD_ROAD_CALL RoadStopAnimation(uint32_t id, uint16_t station) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	TriggerRoadStopAnimation(Station::Get(StationID(station)), v->tile, StationAnimationTrigger::VehicleArrives);
}
static OpenTTDRoadTrackChoice OPENTTD_ROAD_CALL RoadYapf(uint32_t id, uint32_t tile, uint8_t entry_direction, uint16_t tracks) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	bool found = true; Trackdir dir = YapfRoadVehicleChooseTrack(v, TileIndex(tile), static_cast<DiagDirection>(entry_direction), static_cast<TrackdirBits>(tracks), found); return {static_cast<uint8_t>(dir), found};
}
static OpenTTDRoadDepotResult OPENTTD_ROAD_CALL RoadFindDepot(uint32_t id, int32_t max_distance) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	FindDepotData result = YapfRoadVehicleFindNearestDepot(v, static_cast<int32_t>(max_distance)); return {result.tile.base(), result.best_length};
}
static int32_t OPENTTD_ROAD_CALL RoadInclination(uint32_t id, bool new_tile, bool delta) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return v->UpdateInclination(new_tile, delta);
}
static void OPENTTD_ROAD_CALL RoadViewport(uint32_t id, bool force_update, bool update_delta) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->UpdateViewport(force_update != 0, update_delta != 0);
}
static void OPENTTD_ROAD_CALL RoadSetTile(uint32_t id, uint32_t tile) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->tile = TileIndex(static_cast<uint32_t>(tile));
}
static void OPENTTD_ROAD_CALL RoadSetX(uint32_t id, int32_t x) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->x_pos = static_cast<int32_t>(x);
}
static void OPENTTD_ROAD_CALL RoadSetY(uint32_t id, int32_t y) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->y_pos = static_cast<int32_t>(y);
}
static void OPENTTD_ROAD_CALL RoadSetDirection(uint32_t id, uint8_t direction) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->direction = static_cast<Direction>(direction);
}
static void OPENTTD_ROAD_CALL RoadSetSpeed(uint32_t id, uint16_t speed) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->cur_speed = static_cast<uint16_t>(speed);
}
static void OPENTTD_ROAD_CALL RoadSetTick(uint32_t id, uint8_t tick) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->tick_counter = static_cast<uint8_t>(tick);
}
static void OPENTTD_ROAD_CALL RoadSetRunning(uint32_t id, uint8_t running) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->running_ticks = static_cast<uint8_t>(running);
}
static void OPENTTD_ROAD_CALL RoadSetDay(uint32_t id, uint8_t day) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->day_counter = static_cast<uint8_t>(day);
}
static void OPENTTD_ROAD_CALL RoadSetOrderTime(uint32_t id, int32_t order_time) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->current_order_time = static_cast<int32_t>(order_time);
}
static void OPENTTD_ROAD_CALL RoadSetProgress(uint32_t id, uint8_t progress) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->progress = static_cast<uint8_t>(progress);
}
static void OPENTTD_ROAD_CALL RoadSetLastStation(uint32_t id, uint16_t station) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->last_station_visited = StationID(static_cast<uint16_t>(station));
}
static void OPENTTD_ROAD_CALL RoadSetHidden(uint32_t id, bool hidden) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->vehstatus.Set(VehState::Hidden, hidden != 0);
}
static void OPENTTD_ROAD_CALL RoadSetFirstEngine(uint32_t id, uint16_t engine) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->gcache.first_engine = EngineID(static_cast<uint16_t>(engine));
}
static void OPENTTD_ROAD_CALL RoadSetLength(uint32_t id, uint8_t length) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->gcache.cached_veh_length = static_cast<uint8_t>(length);
}
static void OPENTTD_ROAD_CALL RoadSetTotalLength(uint32_t id, uint16_t length) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->gcache.cached_total_length = static_cast<uint16_t>(length);
}
static void OPENTTD_ROAD_CALL RoadSetCargoAge(uint32_t id, uint16_t period) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->vcache.cached_cargo_age_period = static_cast<uint16_t>(period);
}
static void OPENTTD_ROAD_CALL RoadSetMaxSpeed(uint32_t id, uint16_t speed) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	v->vcache.cached_max_speed = static_cast<uint16_t>(speed);
}
static void OPENTTD_ROAD_CALL RoadSetSuppressImplicit(uint32_t id) noexcept
{
	RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	SetBit(v->gv_flags, GVF_SUPPRESS_IMPLICIT_ORDERS);
}
static uint32_t OPENTTD_ROAD_CALL RoadReadDay(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	return v->day_counter;
}
static uint32_t OPENTTD_ROAD_CALL RoadReadDest(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	return v->dest_tile.base();
}
static uint32_t OPENTTD_ROAD_CALL RoadReadDirection(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	return v->direction;
}
static uint32_t OPENTTD_ROAD_CALL RoadReadEngine(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	return v->engine_type.base();
}
static uint32_t OPENTTD_ROAD_CALL RoadReadFirst(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	return v->First()->index.base();
}
static uint32_t OPENTTD_ROAD_CALL RoadReadFront(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	const RoadVehicle *rv = v->type == VEH_ROAD ? RoadVehicle::From(v) : nullptr;
	return rv != nullptr && rv->IsFrontEngine();
}
static uint32_t OPENTTD_ROAD_CALL RoadReadLastStation(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	return v->last_station_visited.base();
}
static uint32_t OPENTTD_ROAD_CALL RoadReadLength(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	const RoadVehicle *rv = v->type == VEH_ROAD ? RoadVehicle::From(v) : nullptr;
	return rv == nullptr ? 0 : rv->gcache.cached_veh_length;
}
static uint32_t OPENTTD_ROAD_CALL RoadReadNext(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	return v->Next() == nullptr ? UINT32_MAX : v->Next()->index.base();
}
static uint32_t OPENTTD_ROAD_CALL RoadReadOrderType(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	return v->current_order.GetType();
}
static uint32_t OPENTTD_ROAD_CALL RoadReadPrevious(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	return v->Previous() == nullptr ? UINT32_MAX : v->Previous()->index.base();
}
static uint32_t OPENTTD_ROAD_CALL RoadReadProgress(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	return v->progress;
}
static uint32_t OPENTTD_ROAD_CALL RoadReadRunning(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	return v->running_ticks;
}
static uint32_t OPENTTD_ROAD_CALL RoadReadSpeed(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	return v->cur_speed;
}
static uint32_t OPENTTD_ROAD_CALL RoadReadStatus(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	return v->vehstatus.base();
}
static uint32_t OPENTTD_ROAD_CALL RoadReadTick(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	return v->tick_counter;
}
static uint32_t OPENTTD_ROAD_CALL RoadReadTile(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	return v->tile.base();
}
static uint32_t OPENTTD_ROAD_CALL RoadReadTotalLength(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	const RoadVehicle *rv = v->type == VEH_ROAD ? RoadVehicle::From(v) : nullptr;
	return rv == nullptr ? 0 : rv->gcache.cached_total_length;
}
static uint32_t OPENTTD_ROAD_CALL RoadReadTram(uint32_t id) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	const RoadVehicle *rv = v->type == VEH_ROAD ? RoadVehicle::From(v) : nullptr;
	return rv != nullptr && RoadTypeIsTram(rv->roadtype);
}
static void OPENTTD_ROAD_CALL RoadSpeedLimits(uint32_t id, OpenTTDRoadSpeedLimits *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	const RoadVehicle *rv = v->type == VEH_ROAD ? RoadVehicle::From(v) : nullptr;
	out->max_track_speed = rv == nullptr ? 0 : rv->gcache.cached_max_track_speed;
	out->order_max_speed = v->current_order.GetMaxSpeed();
}
static void OPENTTD_ROAD_CALL RoadConsistSpeed(uint32_t id, OpenTTDRoadConsistSpeed *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->direction = v->direction;
	out->next = v->Next() == nullptr ? UINT32_MAX : v->Next()->index.base();
	out->status = v->vehstatus.base();
	out->tile = v->tile.base();
}
static void OPENTTD_ROAD_CALL RoadCloseOrigin(uint32_t id, OpenTTDRoadCloseOrigin *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->first = v->First()->index.base();
	out->z = v->z_pos;
}
static void OPENTTD_ROAD_CALL RoadCloseCandidate(uint32_t id, OpenTTDRoadCloseCandidate *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->direction = v->direction;
	out->first = v->First()->index.base();
	out->x = v->x_pos;
	out->y = v->y_pos;
	out->z = v->z_pos;
}

static void OPENTTD_ROAD_CALL RoadOvertakeOrigin(uint32_t id, OpenTTDRoadOvertakeOrigin *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	const RoadVehicle *rv = v->type == VEH_ROAD ? RoadVehicle::From(v) : nullptr;
	out->articulated = rv != nullptr && rv->HasArticulatedPart();
	out->direction = v->direction;
	out->tile = v->tile.base();
	out->tram = rv != nullptr && RoadTypeIsTram(rv->roadtype);
}
static void OPENTTD_ROAD_CALL RoadOvertakeSpeed(uint32_t id, OpenTTDRoadOvertakeSpeed *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->direction = v->direction;
	out->speed = v->cur_speed;
	out->status = v->vehstatus.base();
	out->tile = v->tile.base();
}
static void OPENTTD_ROAD_CALL RoadSlidingPosition(uint32_t id, OpenTTDRoadSlidingPosition *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->direction = v->direction;
	out->x = v->x_pos;
	out->y = v->y_pos;
}
static void OPENTTD_ROAD_CALL RoadHeightSpeed(uint32_t id, OpenTTDRoadHeightSpeed *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	const RoadVehicle *rv = v->type == VEH_ROAD ? RoadVehicle::From(v) : nullptr;
	out->max_track_speed = rv == nullptr ? 0 : rv->gcache.cached_max_track_speed;
	out->speed = v->cur_speed;
	out->z = v->z_pos;
}
static void OPENTTD_ROAD_CALL RoadCollisionPart(uint32_t id, OpenTTDRoadCollisionPart *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->next = v->Next() == nullptr ? UINT32_MAX : v->Next()->index.base();
	out->tile = v->tile.base();
	out->z = v->z_pos;
}
static void OPENTTD_ROAD_CALL RoadCollisionOrigin(uint32_t id, OpenTTDRoadCollisionOrigin *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->x = v->x_pos;
	out->y = v->y_pos;
}

static void OPENTTD_ROAD_CALL RoadCrashDirection(uint32_t id, OpenTTDRoadCrashDirection *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->direction = v->direction;
}
static void OPENTTD_ROAD_CALL RoadPathVehicle(uint32_t id, OpenTTDRoadPathVehicle *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	const RoadVehicle *rv = v->type == VEH_ROAD ? RoadVehicle::From(v) : nullptr;
	out->articulated = rv != nullptr && rv->HasArticulatedPart();
	out->owner = v->owner.base();
	out->tile = v->tile.base();
	out->tram = rv != nullptr && RoadTypeIsTram(rv->roadtype);
}
static void OPENTTD_ROAD_CALL RoadDepotPart(uint32_t id, OpenTTDRoadDepotPart *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->next = v->Next() == nullptr ? UINT32_MAX : v->Next()->index.base();
	out->tile = v->tile.base();
}
static void OPENTTD_ROAD_CALL RoadDepotOrders(uint32_t id, OpenTTDRoadDepotOrders *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->dest = v->dest_tile.base();
	out->order_type = v->current_order.GetType();
}
static void OPENTTD_ROAD_CALL RoadVehicleTile(uint32_t id, OpenTTDRoadVehicleTile *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->tile = v->tile.base();
}
static void OPENTTD_ROAD_CALL RoadArrivalVehicle(uint32_t id, OpenTTDRoadArrivalVehicle *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	const RoadVehicle *rv = v->type == VEH_ROAD ? RoadVehicle::From(v) : nullptr;
	out->owner = v->owner.base();
	out->tram = rv != nullptr && RoadTypeIsTram(rv->roadtype);
}
static void OPENTTD_ROAD_CALL RoadTunnelVehicle(uint32_t id, OpenTTDRoadTunnelVehicle *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->direction = v->direction;
	const RoadVehicle *rv = v->type == VEH_ROAD ? RoadVehicle::From(v) : nullptr;
	out->front = rv != nullptr && rv->IsFrontEngine();
}
static void OPENTTD_ROAD_CALL RoadMoveVehicle(uint32_t id, OpenTTDRoadMoveVehicle *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	const RoadVehicle *rv = v->type == VEH_ROAD ? RoadVehicle::From(v) : nullptr;
	out->front = rv != nullptr && rv->IsFrontEngine();
	out->tile = v->tile.base();
	out->tram = rv != nullptr && RoadTypeIsTram(rv->roadtype);
}
static void OPENTTD_ROAD_CALL RoadMoveTransition(uint32_t id, OpenTTDRoadMoveTransition *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	const RoadVehicle *rv = v->type == VEH_ROAD ? RoadVehicle::From(v) : nullptr;
	out->length = rv == nullptr ? 0 : rv->gcache.cached_veh_length;
	out->next = v->Next() == nullptr ? UINT32_MAX : v->Next()->index.base();
	out->tile = v->tile.base();
}
static void OPENTTD_ROAD_CALL RoadMovePosition(uint32_t id, OpenTTDRoadMovePosition *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	const RoadVehicle *rv = v->type == VEH_ROAD ? RoadVehicle::From(v) : nullptr;
	out->order_type = v->current_order.GetType();
	out->owner = v->owner.base();
	out->speed = v->cur_speed;
	out->tile = v->tile.base();
}
static void OPENTTD_ROAD_CALL RoadBlockVehicle(uint32_t id, OpenTTDRoadBlockVehicle *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	const RoadVehicle *rv = v->type == VEH_ROAD ? RoadVehicle::From(v) : nullptr;
	out->direction = v->direction;
	out->front = rv != nullptr && rv->IsFrontEngine();
	out->owner = v->owner.base();
	out->tile = v->tile.base();
}
static void OPENTTD_ROAD_CALL RoadStopOrder(uint32_t id, OpenTTDRoadStopOrder *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->order_destination = v->current_order.GetDestination().base();
	out->order_type = v->current_order.GetType();
	out->tile = v->tile.base();
}
static void OPENTTD_ROAD_CALL RoadMoveStop(uint32_t id, OpenTTDRoadMoveStop *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->order_type = v->current_order.GetType();
	out->tile = v->tile.base();
}
static void OPENTTD_ROAD_CALL RoadOrderClock(uint32_t id, OpenTTDRoadOrderClock *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->order_time = v->current_order_time;
}
static void OPENTTD_ROAD_CALL RoadControllerPart(uint32_t id, OpenTTDRoadControllerPart *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->next = v->Next() == nullptr ? UINT32_MAX : v->Next()->index.base();
	out->status = v->vehstatus.base();
}
static void OPENTTD_ROAD_CALL RoadServiceOrigin(uint32_t id, OpenTTDRoadServiceOrigin *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->first = v->First()->index.base();
	out->speed = v->cur_speed;
	out->tile = v->tile.base();
}
static void OPENTTD_ROAD_CALL RoadServiceOrder(uint32_t id, OpenTTDRoadServiceOrder *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->order_nonstop = v->current_order.GetNonStopType().base();
	out->order_type = v->current_order.GetType();
}
static void OPENTTD_ROAD_CALL RoadTrackDirection(uint32_t id, OpenTTDRoadTrackDirection *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->direction = v->direction;
	out->status = v->vehstatus.base();
	out->tile = v->tile.base();
}
static void OPENTTD_ROAD_CALL RoadSlopeOrigin(uint32_t id, OpenTTDRoadSlopeOrigin *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->direction = v->direction;
	out->first = v->First()->index.base();
}
static void OPENTTD_ROAD_CALL RoadSlopePart(uint32_t id, OpenTTDRoadSlopePart *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->direction = v->direction;
	out->next = v->Next() == nullptr ? UINT32_MAX : v->Next()->index.base();
}
static void OPENTTD_ROAD_CALL RoadTurnVehicle(uint32_t id, OpenTTDRoadTurnVehicle *out) noexcept
{
	const Vehicle *v = Vehicle::Get(VehicleID(id));
	out->breakdown = v->breakdown_ctr;
	out->direction = v->direction;
	out->order_type = v->current_order.GetType();
	out->status = v->vehstatus.base();
	out->tile = v->tile.base();
}

static uint32_t OPENTTD_ROAD_CALL RoadReadType(uint32_t id) noexcept
{
	return Vehicle::Get(VehicleID(id))->type;
}

static uint32_t OPENTTD_ROAD_CALL RoadReadZ(uint32_t id) noexcept
{
	return Vehicle::Get(VehicleID(id))->z_pos;
}

static uint32_t OPENTTD_ROAD_CALL RoadReadBus(uint32_t id) noexcept
{
	const RoadVehicle *v = RoadVehicle::Get(VehicleID(id));
	return v->IsFrontEngine() && v->IsBus();
}
