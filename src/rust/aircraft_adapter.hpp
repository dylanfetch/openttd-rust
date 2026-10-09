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

/* Resolve each live shell/private owner once. Disaster helpers retain null State.
 * The record contains opaque pointers only; Rust never views C++ object layout. */
static OpenTTDAircraftVehicle AircraftVehicle(Vehicle *v) noexcept
{
	if (v == nullptr) return {nullptr, nullptr, VehicleID::Invalid().base()};
	return {v, v->type == VEH_AIRCRAFT ? Aircraft::From(v)->rust_state.get() : nullptr, v->index.base()};
}
static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_subtype(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->subtype;
}

static int32_t OPENTTD_AIRCRAFT_CALL Aircraft_x(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->x_pos;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_x(OpenTTDAircraftVehicle vehicle, int32_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->x_pos = value;
}

static int32_t OPENTTD_AIRCRAFT_CALL Aircraft_y(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->y_pos;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_y(OpenTTDAircraftVehicle vehicle, int32_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->y_pos = value;
}

static int32_t OPENTTD_AIRCRAFT_CALL Aircraft_z(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->z_pos;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_z(OpenTTDAircraftVehicle vehicle, int32_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->z_pos = value;
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_tile(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->tile.base();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_tile(OpenTTDAircraftVehicle vehicle, uint32_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->tile = TileIndex{value};
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_direction(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->direction;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_direction(OpenTTDAircraftVehicle vehicle, uint8_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->direction = static_cast<Direction>(value);
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_tick_counter(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->tick_counter;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_tick_counter(OpenTTDAircraftVehicle vehicle, uint8_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->tick_counter = value;
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_owner(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->owner.base();
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_vehicle_status(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->vehstatus.base();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_vehicle_status(OpenTTDAircraftVehicle vehicle, uint8_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->vehstatus = VehStates{value};
}

static uint16_t OPENTTD_AIRCRAFT_CALL Aircraft_current_speed(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->cur_speed;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_current_speed(OpenTTDAircraftVehicle vehicle, uint16_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->cur_speed = value;
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_subspeed(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->subspeed;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_subspeed(OpenTTDAircraftVehicle vehicle, uint8_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->subspeed = value;
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_progress(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->progress;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_progress(OpenTTDAircraftVehicle vehicle, uint8_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->progress = value;
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_acceleration(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->acceleration;
}

static uint16_t OPENTTD_AIRCRAFT_CALL Aircraft_maximum_speed(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->vcache.cached_max_speed;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_maximum_speed(OpenTTDAircraftVehicle vehicle, uint16_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->vcache.cached_max_speed = value;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_breakdown_counter(OpenTTDAircraftVehicle vehicle, uint8_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->breakdown_ctr = value;
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_order_type(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->current_order.GetType();
}

static uint16_t OPENTTD_AIRCRAFT_CALL Aircraft_order_destination(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->current_order.GetDestination().base();
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_running_ticks(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->running_ticks;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_running_ticks(OpenTTDAircraftVehicle vehicle, uint8_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->running_ticks = value;
}

static int32_t OPENTTD_AIRCRAFT_CALL Aircraft_order_time(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return Aircraft::From(v)->current_order_time;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_order_time(OpenTTDAircraftVehicle vehicle, int32_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	Aircraft::From(v)->current_order_time = value;
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_day_counter(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->day_counter;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_day_counter(OpenTTDAircraftVehicle vehicle, uint8_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->day_counter = value;
}

static int64_t OPENTTD_AIRCRAFT_CALL Aircraft_profit(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->profit_this_year;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_profit(OpenTTDAircraftVehicle vehicle, int64_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->profit_this_year = value;
}

static uint16_t OPENTTD_AIRCRAFT_CALL Aircraft_last_station(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->last_station_visited.base();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_last_station(OpenTTDAircraftVehicle vehicle, uint16_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->last_station_visited = StationID{value};
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_economy_service(OpenTTDAircraftVehicle vehicle, int32_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->date_of_last_service = TimerGameEconomy::Date{value};
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_calendar_service(OpenTTDAircraftVehicle vehicle, int32_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->date_of_last_service_newgrf = TimerGameCalendar::Date{value};
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_breakdowns(OpenTTDAircraftVehicle vehicle, uint8_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->breakdowns_since_last_service = value;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_reliability(OpenTTDAircraftVehicle vehicle, uint16_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->reliability = value;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_set_cargo_age(OpenTTDAircraftVehicle vehicle, uint16_t value) noexcept
{
	Vehicle *v = static_cast<Vehicle *>(vehicle.handle);
	v->vcache.cached_cargo_age_period = value;
}

static OpenTTDAircraftVehicle OPENTTD_AIRCRAFT_CALL Aircraft_next(OpenTTDAircraftVehicle vehicle) noexcept
{
	return AircraftVehicle(static_cast<Vehicle *>(vehicle.handle)->Next());
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_map_size_x() noexcept
{
	return Map::SizeX();
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_map_max_x() noexcept
{
	return Map::MaxX();
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_map_max_y() noexcept
{
	return Map::MaxY();
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_plane_speed() noexcept
{
	return _settings_game.vehicle.plane_speed;
}

static bool OPENTTD_AIRCRAFT_CALL Aircraft_no_jetcrash() noexcept
{
	return _cheats.no_jetcrash.value;
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_plane_crashes() noexcept
{
	return _settings_game.vehicle.plane_crashes;
}

static bool OPENTTD_AIRCRAFT_CALL Aircraft_service_at_helipad() noexcept
{
	return _settings_game.order.serviceathelipad;
}

static bool OPENTTD_AIRCRAFT_CALL Aircraft_disaster_sound() noexcept
{
	return _settings_client.sound.disaster;
}

static int32_t OPENTTD_AIRCRAFT_CALL Aircraft_economy_date() noexcept
{
	return TimerGameEconomy::date.base();
}

static int32_t OPENTTD_AIRCRAFT_CALL Aircraft_calendar_date() noexcept
{
	return TimerGameCalendar::date.base();
}

static const void * OPENTTD_AIRCRAFT_CALL Aircraft_station(uint16_t id) noexcept
{
	return Station::GetIfValid(id);
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_airport_tile(const void * s) noexcept
{
	const Station *st = static_cast<const Station *>(s);
	return st->airport.tile.base();
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_station_tile(const void * s) noexcept
{
	const Station *st = static_cast<const Station *>(s);
	return st->xy.base();
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_rotation(const void * s) noexcept
{
	const Station *st = static_cast<const Station *>(s);
	return st->airport.rotation;
}

static uint16_t OPENTTD_AIRCRAFT_CALL Aircraft_airport_width(const void * s) noexcept
{
	const Station *st = static_cast<const Station *>(s);
	return st->airport.w;
}

static uint16_t OPENTTD_AIRCRAFT_CALL Aircraft_airport_height(const void * s) noexcept
{
	const Station *st = static_cast<const Station *>(s);
	return st->airport.h;
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_airport_type(const void * s) noexcept
{
	const Station *st = static_cast<const Station *>(s);
	return st->airport.type;
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_station_owner(const void * s) noexcept
{
	const Station *st = static_cast<const Station *>(s);
	return st->owner.base();
}

static bool OPENTTD_AIRCRAFT_CALL Aircraft_has_hangar(const void * s) noexcept
{
	const Station *st = static_cast<const Station *>(s);
	return st->airport.HasHangar();
}

static bool OPENTTD_AIRCRAFT_CALL Aircraft_has_airport(const void * s) noexcept
{
	const Station *st = static_cast<const Station *>(s);
	return st->facilities.Test(StationFacility::Airport);
}

static const void * OPENTTD_AIRCRAFT_CALL Aircraft_airport_fta(const void * s) noexcept
{
	const Station *st = static_cast<const Station *>(s);
	return st->airport.GetFTA();
}

static uint64_t * OPENTTD_AIRCRAFT_CALL Aircraft_airport_blocks(const void * s) noexcept
{
	const Station *st = static_cast<const Station *>(s);
	return st->airport.rust_blocks.get();
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_had_vehicle(const void * s) noexcept
{
	const Station *st = static_cast<const Station *>(s);
	return st->had_vehicle_of_type;
}

static const void * OPENTTD_AIRCRAFT_CALL Aircraft_dummy_airport() noexcept
{
	return GetAirport(AT_DUMMY);
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_airport_elements(const void * airport) noexcept
{
	const auto *ap = static_cast<const AirportFTAClass *>(airport);
	return ap->nofelements;
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_helipads(const void * airport) noexcept
{
	const auto *ap = static_cast<const AirportFTAClass *>(airport);
	return ap->num_helipads;
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_airport_flags(const void * airport) noexcept
{
	const auto *ap = static_cast<const AirportFTAClass *>(airport);
	return ap->flags.base();
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_airport_delta_z(const void * airport) noexcept
{
	const auto *ap = static_cast<const AirportFTAClass *>(airport);
	return ap->delta_z;
}

static const void * OPENTTD_AIRCRAFT_CALL Aircraft_node(const void * airport, uint8_t pos) noexcept
{
	return &static_cast<const AirportFTAClass *>(airport)->layout[pos];
}

static OpenTTDAircraftNode OPENTTD_AIRCRAFT_CALL Aircraft_fta(const void * node) noexcept
{
	const auto *n = static_cast<const AirportFTA *>(node);
	return {n->next.get(), n->blocks.base(), n->position, n->next_position, n->heading};
}

static OpenTTDAircraftMoving OPENTTD_AIRCRAFT_CALL Aircraft_moving(const void * airport, uint8_t pos) noexcept
{
	const auto *m = static_cast<const AirportFTAClass *>(airport)->MovingData(pos);
	return {m->x, m->y, m->flags.base(), m->direction};
}

static uint16_t OPENTTD_AIRCRAFT_CALL Aircraft_engine_speed(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return AircraftVehInfo(v->engine_type)->max_speed;
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_engine_subtype(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return AircraftVehInfo(v->engine_type)->subtype;
}

static uint16_t OPENTTD_AIRCRAFT_CALL Aircraft_engine_sound(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return AircraftVehInfo(v->engine_type)->sfx;
}

static uint16_t OPENTTD_AIRCRAFT_CALL Aircraft_engine_reliability(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->GetEngine()->reliability;
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_vehicle_type(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->type;
}

static int32_t OPENTTD_AIRCRAFT_CALL Aircraft_slope(int32_t x, int32_t y) noexcept
{
	return GetSlopePixelZ(x, y);
}

static int32_t OPENTTD_AIRCRAFT_CALL Aircraft_tile_height(uint32_t tile) noexcept
{
	return TileHeight(TileIndex{tile}) * TILE_HEIGHT;
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_airport_entry(const void * ap, uint8_t direction) noexcept
{
	return static_cast<const AirportFTAClass *>(ap)->entry_points[direction];
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_direction_towards(OpenTTDAircraftVehicle vehicle, int32_t x, int32_t y) noexcept
{
	return GetDirectionTowards(static_cast<const Vehicle *>(vehicle.handle), x, y);
}

static OpenTTDAircraftPosition OPENTTD_AIRCRAFT_CALL Aircraft_new_position(OpenTTDAircraftVehicle vehicle) noexcept
{
	auto p = GetNewVehiclePos(static_cast<const Vehicle *>(vehicle.handle));
	return {p.x, p.y, p.new_tile.base()};
}

static int32_t OPENTTD_AIRCRAFT_CALL Aircraft_hangar_height(uint16_t station) noexcept
{
	return GetTileMaxPixelZ(Station::Get(station)->airport.GetHangarTile(0));
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_terminal_count(const void * ap, uint32_t index) noexcept
{
	return static_cast<const AirportFTAClass *>(ap)->terminals[index];
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_hangar_exit(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return Station::GetByTile(v->tile)->airport.GetHangarExitDirection(v->tile);
}

static bool OPENTTD_AIRCRAFT_CALL Aircraft_can_use_station(OpenTTDAircraftVehicle vehicle, uint16_t station) noexcept
{
	return CanVehicleUseStation(static_cast<const Vehicle *>(vehicle.handle), Station::Get(station));
}

static uint16_t OPENTTD_AIRCRAFT_CALL Aircraft_service_interval(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return Company::Get(v->owner)->settings.vehicle.servint_aircraft;
}

static bool OPENTTD_AIRCRAFT_CALL Aircraft_needs_service(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return Aircraft::From(v)->NeedsAutomaticServicing();
}

static bool OPENTTD_AIRCRAFT_CALL Aircraft_chain_in_depot(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return Aircraft::From(v)->IsChainInDepot();
}

static bool OPENTTD_AIRCRAFT_CALL Aircraft_waiting_unbunching(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return Aircraft::From(v)->IsWaitingForUnbunching();
}

static bool OPENTTD_AIRCRAFT_CALL Aircraft_nearest_depot_order(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->current_order.GetDepotActionType().Test(OrderDepotActionFlag::NearestDepot);
}

static bool OPENTTD_AIRCRAFT_CALL Aircraft_part_of_orders(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return v->current_order.GetDepotOrderType().Test(OrderDepotTypeFlag::PartOfOrders);
}

static uint16_t OPENTTD_AIRCRAFT_CALL Aircraft_next_station(uint16_t last) noexcept
{
	for (const Station *s : Station::Iterate(last == StationID::Invalid().base() ? 0 : last + 1)) return s->index.base();
	return StationID::Invalid().base();
}

static OpenTTDAircraftVehicle OPENTTD_AIRCRAFT_CALL Aircraft_next_aircraft(uint32_t last) noexcept
{
	for (Aircraft *v : Aircraft::Iterate(last == VehicleID::Invalid().base() ? 0 : last + 1)) return AircraftVehicle(v);
	return AircraftVehicle(nullptr);
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_random() noexcept
{
	return Random();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_update_position(OpenTTDAircraftVehicle vehicle) noexcept
{
	Aircraft *v = Aircraft::From(static_cast<Vehicle *>(vehicle.handle)); v->UpdatePosition(); v->UpdateViewport(true, false);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_rotor_image(OpenTTDAircraftVehicle vehicle) noexcept
{
	Aircraft *v = Aircraft::From(static_cast<Vehicle *>(vehicle.handle)); GetRotorImage(v, EIT_ON_MAP, &v->Next()->Next()->sprite_cache.sprite_seq);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_copy_sprite(OpenTTDAircraftVehicle vehicle, OpenTTDAircraftVehicle source) noexcept
{
	static_cast<Vehicle *>(vehicle.handle)->sprite_cache.sprite_seq.CopyWithoutPalette(static_cast<Vehicle *>(source.handle)->sprite_cache.sprite_seq);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_position_viewport(OpenTTDAircraftVehicle vehicle) noexcept
{
	static_cast<Vehicle *>(vehicle.handle)->UpdatePositionAndViewport();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_create_effect(OpenTTDAircraftVehicle vehicle, uint8_t kind, int32_t x, int32_t y, int32_t z) noexcept
{
	CreateEffectVehicleRel(static_cast<Vehicle *>(vehicle.handle), x, y, z, static_cast<EffectVehicleType>(kind));
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_dirty_start_stop(OpenTTDAircraftVehicle vehicle) noexcept
{
	SetWindowWidgetDirty(WC_VEHICLE_VIEW, vehicle.id, WID_VV_START_STOP);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_play_sound(OpenTTDAircraftVehicle vehicle, uint16_t sound) noexcept
{
	SndPlayVehicleFx(static_cast<SoundID>(sound), static_cast<Vehicle *>(vehicle.handle));
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_truncate_cargo(OpenTTDAircraftVehicle vehicle) noexcept
{
	static_cast<Vehicle *>(vehicle.handle)->cargo.Truncate();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_crash_news(OpenTTDAircraftVehicle vehicle, uint16_t station_id, uint32_t victims) noexcept
{
	Aircraft *v = Aircraft::From(static_cast<Vehicle *>(vehicle.handle)); StationID station{station_id};
	TileIndex vt = TileVirtXYClampedToMap(v->x_pos, v->y_pos); bool missing = station == StationID::Invalid();
	EncodedString headline = missing ? GetEncodedString(STR_NEWS_PLANE_CRASH_OUT_OF_FUEL, victims) : GetEncodedString(STR_NEWS_AIRCRAFT_CRASH, victims, station);
	AI::NewEvent(v->owner, new ScriptEventVehicleCrashed(v->index, vt, missing ? ScriptEventVehicleCrashed::CRASH_AIRCRAFT_NO_AIRPORT : ScriptEventVehicleCrashed::CRASH_PLANE_LANDING, victims, v->owner));
	Game::NewEvent(new ScriptEventVehicleCrashed(v->index, vt, missing ? ScriptEventVehicleCrashed::CRASH_AIRCRAFT_NO_AIRPORT : ScriptEventVehicleCrashed::CRASH_PLANE_LANDING, victims, v->owner));
	AddTileNewsItem(std::move(headline), v->owner == _local_company ? NewsType::Accident : NewsType::AccidentOther, vt, station);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_station_rating(uint32_t tile, uint8_t owner, int32_t amount, uint32_t radius) noexcept
{
	ModifyStationRatingAround(TileIndex{tile}, CompanyID{static_cast<uint8_t>(owner)}, amount, radius);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_landing_rating(uint16_t station_id, uint8_t cargo) noexcept
{
	GoodsEntry &ge = Station::Get(station_id)->goods[cargo]; ge.rating = 1; if (ge.HasData()) ge.GetData().cargo.Truncate();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_free_order(OpenTTDAircraftVehicle vehicle) noexcept
{
	Aircraft::From(static_cast<Vehicle *>(vehicle.handle))->current_order.Free();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_service_in_depot(OpenTTDAircraftVehicle vehicle) noexcept
{
	VehicleServiceInDepot(static_cast<Vehicle *>(vehicle.handle));
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_leave_unbunching(OpenTTDAircraftVehicle vehicle) noexcept
{
	Aircraft::From(static_cast<Vehicle *>(vehicle.handle))->LeaveUnbunchingDepot();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_dirty_depot(OpenTTDAircraftVehicle vehicle) noexcept
{
	InvalidateWindowData(WC_VEHICLE_DEPOT, static_cast<Vehicle *>(vehicle.handle)->tile); SetWindowClassesDirty(WC_AIRCRAFT_LIST);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_first_arrival(OpenTTDAircraftVehicle vehicle, uint16_t station) noexcept
{
	Aircraft *v = Aircraft::From(static_cast<Vehicle *>(vehicle.handle)); Station *st = Station::Get(station); st->had_vehicle_of_type |= HVOT_AIRCRAFT;
	AddVehicleNewsItem(GetEncodedString(STR_NEWS_FIRST_AIRCRAFT_ARRIVAL, st->index), v->owner == _local_company ? NewsType::ArrivalCompany : NewsType::ArrivalOther, v->index, st->index);
	AI::NewEvent(v->owner, new ScriptEventStationFirstVehicle(st->index, v->index)); Game::NewEvent(new ScriptEventStationFirstVehicle(st->index, v->index));
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_begin_loading(OpenTTDAircraftVehicle vehicle) noexcept
{
	static_cast<Vehicle *>(vehicle.handle)->BeginLoading();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_dirty_details(OpenTTDAircraftVehicle vehicle) noexcept
{
	SetWindowDirty(WC_VEHICLE_DETAILS, vehicle.id);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_update_delta(OpenTTDAircraftVehicle vehicle) noexcept
{
	static_cast<Vehicle *>(vehicle.handle)->UpdateDeltaXY();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_touchdown_animation(OpenTTDAircraftVehicle vehicle, uint16_t station) noexcept
{
	Aircraft *v = Aircraft::From(static_cast<Vehicle *>(vehicle.handle)); TriggerAirportTileAnimation(Station::Get(station), TileVirtXY(v->x_pos, v->y_pos), AirportAnimationTrigger::AirplaneTouchdown);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_destination_too_far(OpenTTDAircraftVehicle vehicle) noexcept
{
	Aircraft *v = Aircraft::From(static_cast<Vehicle *>(vehicle.handle)); AI::NewEvent(v->owner, new ScriptEventAircraftDestTooFar(v->index));
	if (v->owner == _local_company) AddVehicleAdviceNewsItem(AdviceType::AircraftDestinationTooFar, GetEncodedString(STR_NEWS_AIRCRAFT_DEST_TOO_FAR, v->index), v->index);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_delete_range_news(OpenTTDAircraftVehicle vehicle) noexcept
{
	DeleteVehicleNews(VehicleID{vehicle.id}, AdviceType::AircraftDestinationTooFar);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_handle_breakdown(OpenTTDAircraftVehicle vehicle) noexcept
{
	static_cast<Vehicle *>(vehicle.handle)->HandleBreakdown();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_handle_loading(OpenTTDAircraftVehicle vehicle, bool second) noexcept
{
	static_cast<Vehicle *>(vehicle.handle)->HandleLoading(second);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_service_order(OpenTTDAircraftVehicle vehicle, uint16_t station) noexcept
{
	Aircraft::From(static_cast<Vehicle *>(vehicle.handle))->current_order.MakeGoToDepot(StationID{static_cast<uint16_t>(station)}, OrderDepotTypeFlag::Service);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_dummy_order(OpenTTDAircraftVehicle vehicle) noexcept
{
	Aircraft::From(static_cast<Vehicle *>(vehicle.handle))->current_order.MakeDummy();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_age_vehicle(OpenTTDAircraftVehicle vehicle) noexcept
{
	AgeVehicle(static_cast<Vehicle *>(vehicle.handle));
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_economy_age(OpenTTDAircraftVehicle vehicle) noexcept
{
	EconomyAgeVehicle(static_cast<Vehicle *>(vehicle.handle));
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_decrease_value(OpenTTDAircraftVehicle vehicle) noexcept
{
	DecreaseVehicleValue(static_cast<Vehicle *>(vehicle.handle));
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_check_orders(OpenTTDAircraftVehicle vehicle) noexcept
{
	CheckOrders(static_cast<Vehicle *>(vehicle.handle));
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_check_breakdown(OpenTTDAircraftVehicle vehicle) noexcept
{
	CheckVehicleBreakdown(static_cast<Vehicle *>(vehicle.handle));
}

static int64_t OPENTTD_AIRCRAFT_CALL Aircraft_running_cost(OpenTTDAircraftVehicle vehicle) noexcept
{
	return Aircraft::From(static_cast<Vehicle *>(vehicle.handle))->GetRunningCost();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_subtract_cost(OpenTTDAircraftVehicle vehicle, int64_t cost) noexcept
{
	SubtractMoneyFromCompanyFract(static_cast<Vehicle *>(vehicle.handle)->owner, CommandCost(EXPENSES_AIRCRAFT_RUN, Money{cost}));
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_dirty_lists(OpenTTDAircraftVehicle vehicle) noexcept
{
	SetWindowDirty(WC_VEHICLE_DETAILS, vehicle.id); SetWindowClassesDirty(WC_AIRCRAFT_LIST);
}

static uint16_t OPENTTD_AIRCRAFT_CALL Aircraft_next_stopping_station(OpenTTDAircraftVehicle vehicle) noexcept
{
	std::vector<StationID> next_station; Aircraft::From(static_cast<Vehicle *>(vehicle.handle))->GetNextStoppingStation(next_station); return next_station.empty() ? StationID::Invalid().base() : next_station.back().base();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_remove_depot_orders(uint16_t station_id) noexcept
{
	RemoveOrderFromAllVehicles(OT_GOTO_DEPOT, StationID{static_cast<uint16_t>(station_id)}, true);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_assert_flying(OpenTTDAircraftVehicle vehicle) noexcept
{
	assert(Aircraft::From(static_cast<Vehicle *>(vehicle.handle))->state == FLYING);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_invalid_movement(OpenTTDAircraftVehicle vehicle) noexcept
{
	Debug(misc, 0, "[Ap] cannot move further on Airport! (pos {} state {}) for vehicle {}", Aircraft::From(static_cast<Vehicle *>(vehicle.handle))->pos, Aircraft::From(static_cast<Vehicle *>(vehicle.handle))->state, vehicle.id); NOT_REACHED();
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_invalid_position(OpenTTDAircraftVehicle vehicle, const void * ap) noexcept
{
	Debug(misc, 0, "[Ap] position {} is not valid for current airport. Max position is {}", Aircraft::From(static_cast<Vehicle *>(vehicle.handle))->pos, static_cast<const AirportFTAClass *>(ap)->nofelements - 1); assert(Aircraft::From(static_cast<Vehicle *>(vehicle.handle))->pos < static_cast<const AirportFTAClass *>(ap)->nofelements);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_invalid_scheme(OpenTTDAircraftVehicle) noexcept
{
	FatalError("OK, you shouldn't be here, check your Airport Scheme!");
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_unreachable(OpenTTDAircraftVehicle) noexcept
{
	NOT_REACHED();
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_speed_property(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return GetVehicleProperty(v, PROP_AIRCRAFT_SPEED, 0);
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_cargo_age_property(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return GetVehicleProperty(v, PROP_AIRCRAFT_CARGO_AGE_PERIOD, EngInfo(v->engine_type)->cargo_age_period);
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_range_property(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return GetVehicleProperty(v, PROP_AIRCRAFT_RANGE, AircraftVehInfo(v->engine_type)->max_range);
}

static bool OPENTTD_AIRCRAFT_CALL Aircraft_start_sound(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return PlayVehicleSound(v, VSE_START);
}

static bool OPENTTD_AIRCRAFT_CALL Aircraft_touchdown_sound(OpenTTDAircraftVehicle vehicle) noexcept
{
	const Vehicle *v = static_cast<const Vehicle *>(vehicle.handle);
	return PlayVehicleSound(v, VSE_TOUCHDOWN);
}

static bool OPENTTD_AIRCRAFT_CALL Aircraft_rotor_image_if_changed(OpenTTDAircraftVehicle vehicle) noexcept
{
	Aircraft *v = Aircraft::From(static_cast<Vehicle *>(vehicle.handle)); Aircraft *rotor = v->Next()->Next(); VehicleSpriteSeq seq;
	GetRotorImage(v, EIT_ON_MAP, &seq); if (rotor->sprite_cache.sprite_seq == seq) return true; rotor->sprite_cache.sprite_seq = seq; return false;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_update_rotor_image(OpenTTDAircraftVehicle vehicle) noexcept
{
	Aircraft *v = Aircraft::From(static_cast<Vehicle *>(vehicle.handle)); Aircraft *rotor = v->Next()->Next(); VehicleSpriteSeq seq;
	GetRotorImage(v, EIT_ON_MAP, &seq); rotor->sprite_cache.sprite_seq = seq;
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_process_orders(OpenTTDAircraftVehicle vehicle) noexcept
{
	ProcessOrders(Aircraft::From(static_cast<Vehicle *>(vehicle.handle)));
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_enter_depot(OpenTTDAircraftVehicle vehicle) noexcept
{
	VehicleEnterDepot(Aircraft::From(static_cast<Vehicle *>(vehicle.handle)));
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_vehicle_crash(OpenTTDAircraftVehicle vehicle, bool flooded) noexcept
{
	return static_cast<Vehicle *>(vehicle.handle)->Vehicle::Crash(flooded);
}

static void OPENTTD_AIRCRAFT_CALL Aircraft_delete_aircraft(OpenTTDAircraftVehicle vehicle) noexcept
{
	delete Aircraft::From(static_cast<Vehicle *>(vehicle.handle));
}

static bool OPENTTD_AIRCRAFT_CALL Aircraft_send_to_depot(OpenTTDAircraftVehicle vehicle, bool service) noexcept
{
	Backup<CompanyID> current(_current_company, static_cast<Vehicle *>(vehicle.handle)->owner);
	bool failed = Command<CMD_SEND_VEHICLE_TO_DEPOT>::Do(DoCommandFlag::Execute, VehicleID{vehicle.id}, service ? DepotCommandFlag::Service : DepotCommandFlags{}, {}).Failed();
	current.Restore();
	return failed;
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_sample_count() noexcept
{
	return ORIGINAL_SAMPLE_COUNT;
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_helicopter_sound() noexcept
{
	return SND_18_TAKEOFF_HELICOPTER;
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_explosion_sound() noexcept
{
	return SND_12_EXPLOSION;
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_skid_sound() noexcept
{
	return SND_17_SKID_PLANE;
}

static uint32_t OPENTTD_AIRCRAFT_CALL Aircraft_ticks_per_year() noexcept
{
	return CalendarTime::DAYS_IN_YEAR * Ticks::DAY_TICKS;
}

static uint64_t OPENTTD_AIRCRAFT_CALL Aircraft_fta_blocks(const void *node) noexcept
{
	return static_cast<const AirportFTA *>(node)->blocks.base();
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_fta_heading(const void *node) noexcept
{
	return static_cast<const AirportFTA *>(node)->heading;
}

static uint8_t OPENTTD_AIRCRAFT_CALL Aircraft_fta_next_position(const void *node) noexcept
{
	return static_cast<const AirportFTA *>(node)->next_position;
}

static const void * OPENTTD_AIRCRAFT_CALL Aircraft_fta_next(const void *node) noexcept
{
	return static_cast<const AirportFTA *>(node)->next.get();
}

static OpenTTDAircraftBlockNode OPENTTD_AIRCRAFT_CALL Aircraft_block_node(const void *node) noexcept
{
	const auto *n = static_cast<const AirportFTA *>(node);
	return {n->blocks.base(), n->position, n->next_position};
}

static OpenTTDAircraftRouteNode OPENTTD_AIRCRAFT_CALL Aircraft_route_node(const void *node) noexcept
{
	const auto *n = static_cast<const AirportFTA *>(node);
	return {n->next.get(), n->next_position, n->heading};
}

static OpenTTDAircraftBlockChoice OPENTTD_AIRCRAFT_CALL Aircraft_block_choice(const void *node) noexcept
{
	const auto *n = static_cast<const AirportFTA *>(node);
	return {n->next.get(), n->blocks.base(), n->heading};
}

static const OpenTTDAircraftLeaves _aircraft_leaves{
	Aircraft_subtype,
	Aircraft_x,
	Aircraft_set_x,
	Aircraft_y,
	Aircraft_set_y,
	Aircraft_z,
	Aircraft_set_z,
	Aircraft_tile,
	Aircraft_set_tile,
	Aircraft_direction,
	Aircraft_set_direction,
	Aircraft_tick_counter,
	Aircraft_set_tick_counter,
	Aircraft_owner,
	Aircraft_vehicle_status,
	Aircraft_set_vehicle_status,
	Aircraft_current_speed,
	Aircraft_set_current_speed,
	Aircraft_subspeed,
	Aircraft_set_subspeed,
	Aircraft_progress,
	Aircraft_set_progress,
	Aircraft_acceleration,
	Aircraft_maximum_speed,
	Aircraft_set_maximum_speed,
	Aircraft_set_breakdown_counter,
	Aircraft_order_type,
	Aircraft_order_destination,
	Aircraft_running_ticks,
	Aircraft_set_running_ticks,
	Aircraft_order_time,
	Aircraft_set_order_time,
	Aircraft_day_counter,
	Aircraft_set_day_counter,
	Aircraft_profit,
	Aircraft_set_profit,
	Aircraft_last_station,
	Aircraft_set_last_station,
	Aircraft_set_economy_service,
	Aircraft_set_calendar_service,
	Aircraft_set_breakdowns,
	Aircraft_set_reliability,
	Aircraft_set_cargo_age,
	Aircraft_next,
	Aircraft_map_size_x,
	Aircraft_map_max_x,
	Aircraft_map_max_y,
	Aircraft_plane_speed,
	Aircraft_no_jetcrash,
	Aircraft_plane_crashes,
	Aircraft_service_at_helipad,
	Aircraft_disaster_sound,
	Aircraft_economy_date,
	Aircraft_calendar_date,
	Aircraft_station,
	Aircraft_airport_tile,
	Aircraft_station_tile,
	Aircraft_rotation,
	Aircraft_airport_width,
	Aircraft_airport_height,
	Aircraft_airport_type,
	Aircraft_station_owner,
	Aircraft_has_hangar,
	Aircraft_has_airport,
	Aircraft_airport_fta,
	Aircraft_airport_blocks,
	Aircraft_had_vehicle,
	Aircraft_dummy_airport,
	Aircraft_airport_elements,
	Aircraft_helipads,
	Aircraft_airport_flags,
	Aircraft_airport_delta_z,
	Aircraft_node,
	Aircraft_fta,
	Aircraft_moving,
	Aircraft_engine_speed,
	Aircraft_engine_subtype,
	Aircraft_engine_sound,
	Aircraft_engine_reliability,
	Aircraft_vehicle_type,
	Aircraft_slope,
	Aircraft_tile_height,
	Aircraft_airport_entry,
	Aircraft_direction_towards,
	Aircraft_new_position,
	Aircraft_hangar_height,
	Aircraft_terminal_count,
	Aircraft_hangar_exit,
	Aircraft_can_use_station,
	Aircraft_service_interval,
	Aircraft_needs_service,
	Aircraft_chain_in_depot,
	Aircraft_waiting_unbunching,
	Aircraft_nearest_depot_order,
	Aircraft_part_of_orders,
	Aircraft_next_station,
	Aircraft_next_aircraft,
	Aircraft_random,
	Aircraft_update_position,
	Aircraft_rotor_image,
	Aircraft_copy_sprite,
	Aircraft_position_viewport,
	Aircraft_create_effect,
	Aircraft_dirty_start_stop,
	Aircraft_play_sound,
	Aircraft_truncate_cargo,
	Aircraft_crash_news,
	Aircraft_station_rating,
	Aircraft_landing_rating,
	Aircraft_free_order,
	Aircraft_service_in_depot,
	Aircraft_leave_unbunching,
	Aircraft_dirty_depot,
	Aircraft_first_arrival,
	Aircraft_begin_loading,
	Aircraft_dirty_details,
	Aircraft_update_delta,
	Aircraft_touchdown_animation,
	Aircraft_destination_too_far,
	Aircraft_delete_range_news,
	Aircraft_handle_breakdown,
	Aircraft_handle_loading,
	Aircraft_service_order,
	Aircraft_dummy_order,
	Aircraft_age_vehicle,
	Aircraft_economy_age,
	Aircraft_decrease_value,
	Aircraft_check_orders,
	Aircraft_check_breakdown,
	Aircraft_running_cost,
	Aircraft_subtract_cost,
	Aircraft_dirty_lists,
	Aircraft_next_stopping_station,
	Aircraft_remove_depot_orders,
	Aircraft_assert_flying,
	Aircraft_invalid_movement,
	Aircraft_invalid_position,
	Aircraft_invalid_scheme,
	Aircraft_unreachable,
	Aircraft_speed_property,
	Aircraft_cargo_age_property,
	Aircraft_range_property,
	Aircraft_start_sound,
	Aircraft_touchdown_sound,
	Aircraft_rotor_image_if_changed,
	Aircraft_update_rotor_image,
	Aircraft_process_orders,
	Aircraft_enter_depot,
	Aircraft_vehicle_crash,
	Aircraft_delete_aircraft,
	Aircraft_send_to_depot,
	Aircraft_sample_count,
	Aircraft_helicopter_sound,
	Aircraft_explosion_sound,
	Aircraft_skid_sound,
	Aircraft_ticks_per_year,
	Aircraft_fta_blocks,
	Aircraft_fta_heading,
	Aircraft_fta_next_position,
	Aircraft_fta_next,
	Aircraft_block_node,
	Aircraft_route_node,
	Aircraft_block_choice,
};
bool Aircraft::Tick()
{
	if (!this->IsNormalAircraft()) return true;
	PerformanceAccumulator framerate(PFE_GL_AIRCRAFT);
	return openttd_rust_aircraft_tick(&_aircraft_leaves, AircraftVehicle(this));
}
void Aircraft::OnNewCalendarDay() { openttd_rust_aircraft_calendar_day(&_aircraft_leaves, AircraftVehicle(this)); }
void Aircraft::OnNewEconomyDay() { openttd_rust_aircraft_economy_day(&_aircraft_leaves, AircraftVehicle(this)); }
void SetAircraftPosition(Aircraft *v, int x, int y, int z) { openttd_rust_aircraft_position(&_aircraft_leaves, AircraftVehicle(v), x, y, z); }
void HandleAircraftEnterHangar(Aircraft *v) { openttd_rust_aircraft_enter_hangar(&_aircraft_leaves, AircraftVehicle(v)); }
void UpdateAircraftCache(Aircraft *v, bool update_range) { openttd_rust_aircraft_cache(&_aircraft_leaves, AircraftVehicle(v), update_range); }
void GetAircraftFlightLevelBounds(const Vehicle *v, int *min, int *max) { openttd_rust_aircraft_flight_bounds(&_aircraft_leaves, AircraftVehicle(const_cast<Vehicle *>(v)), min, max); }
static uint8_t &GetFlightFlags(Aircraft *v) { return v->flags; }
static uint8_t &GetFlightFlags(DisasterVehicle *v) { return v->Flags(); }
template <class T> int GetAircraftFlightLevel(T *v, bool takeoff) { return openttd_rust_aircraft_flight_level(&_aircraft_leaves, AircraftVehicle(v), &GetFlightFlags(v), takeoff); }
template int GetAircraftFlightLevel(DisasterVehicle *v, bool takeoff);
template int GetAircraftFlightLevel(Aircraft *v, bool takeoff);
int GetTileHeightBelowAircraft(const Vehicle *v) { return openttd_rust_aircraft_height(&_aircraft_leaves, AircraftVehicle(const_cast<Vehicle *>(v))); }
int GetAircraftHoldMaxAltitude(const Aircraft *v) { return openttd_rust_aircraft_hold_altitude(&_aircraft_leaves, AircraftVehicle(const_cast<Aircraft *>(v))); }
void HandleMissingAircraftOrders(Aircraft *v) { openttd_rust_aircraft_missing_orders(&_aircraft_leaves, AircraftVehicle(v)); }
uint Aircraft::Crash(bool flooded) { return openttd_rust_aircraft_crash(&_aircraft_leaves, AircraftVehicle(this), flooded); }
void AircraftNextAirportPos_and_Order(Aircraft *v) { openttd_rust_aircraft_next_airport(&_aircraft_leaves, AircraftVehicle(v)); }
void AircraftLeaveHangar(Aircraft *v, Direction exit_dir) { openttd_rust_aircraft_leave_hangar(&_aircraft_leaves, AircraftVehicle(v), exit_dir); }
TileIndex Aircraft::GetOrderStationLocation(StationID) { openttd_rust_aircraft_order_location(&_aircraft_leaves, AircraftVehicle(this)); return TileIndex{}; }
void UpdateAirplanesOnNewStation(const Station *st) { openttd_rust_aircraft_replacement(&_aircraft_leaves, st->index.base()); }
ClosestDepot Aircraft::FindClosestDepot()
{
	StationID station{openttd_rust_aircraft_closest_depot(&_aircraft_leaves, AircraftVehicle(this))};
	return station == StationID::Invalid() ? ClosestDepot() : ClosestDepot(Station::Get(station)->xy, station);
}
Station *GetTargetAirportIfValid(const Aircraft *v)
{
	assert(v->type == VEH_AIRCRAFT);
	Station *st = Station::GetIfValid(v->targetairport);
	return st == nullptr || st->airport.tile == INVALID_TILE ? nullptr : st;
}
void ReleaseAircraftAirportBlocks(const Aircraft *v) { openttd_rust_aircraft_release_blocks(&_aircraft_leaves, AircraftVehicle(const_cast<Aircraft *>(v))); }
void InvalidateAircraftTargetStation(StationID station) { openttd_rust_aircraft_invalidate_target(&_aircraft_leaves, station.base()); }
