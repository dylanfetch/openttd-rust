/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_services.h Game-side handle conversion and GroundVehicle services for Rust ports. */
#ifndef RUST_VEHICLE_SERVICES_H
#define RUST_VEHICLE_SERVICES_H
#include "vehicle_ffi.h"
#include "../ground_vehicle.hpp"

/** The Rust handle of a live vehicle; null stays null. */
inline OpenTTDVehicle *RustVehicle(const Vehicle *v) noexcept { return reinterpret_cast<OpenTTDVehicle *>(const_cast<Vehicle *>(v)); }
/** The vehicle behind a Rust handle. */
inline Vehicle *VehicleOf(OpenTTDVehicle *v) noexcept { return reinterpret_cast<Vehicle *>(v); }

template <class T>
static OpenTTDGroundMoved OPENTTD_VEHICLE_CALL GroundSvcMoveIncline(OpenTTDVehicle *v, int32_t x, int32_t y, bool new_tile, bool update_delta) noexcept
{
	T *u = T::From(VehicleOf(v));
	u->x_pos = x;
	u->y_pos = y;
	u->UpdatePosition();
	int old_z = u->UpdateInclination(new_tile, update_delta);
	return {.old_z = old_z, .z = u->z_pos, .cur_speed = u->cur_speed, .max_track_speed = u->gcache.cached_max_track_speed};
}
template <class T>
static void OPENTTD_VEHICLE_CALL GroundSvcUpdateViewport(OpenTTDVehicle *v, bool force_update, bool update_delta) noexcept
{
	T::From(VehicleOf(v))->UpdateViewport(force_update, update_delta);
}
template <class T>
static void OPENTTD_VEHICLE_CALL GroundSvcTurn(OpenTTDVehicle *v, uint8_t direction) noexcept
{
	T *u = T::From(VehicleOf(v));
	u->direction = static_cast<Direction>(direction);
	u->UpdateViewport(true, true);
}
template <class T>
static OpenTTDGroundSpeed OPENTTD_VEHICLE_CALL GroundSvcDoUpdateSpeed(OpenTTDVehicle *v, uint32_t accel, int32_t min_speed, int32_t max_speed) noexcept
{
	T *u = T::From(VehicleOf(v));
	int distance = u->RustDoUpdateSpeed(accel, min_speed, max_speed);
	return {.distance = distance, .cur_speed = u->cur_speed};
}
template <class T>
static OpenTTDGroundLastSpeed OPENTTD_VEHICLE_CALL GroundSvcSetLastSpeed(OpenTTDVehicle *v) noexcept
{
	T *u = T::From(VehicleOf(v));
	u->SetLastSpeed();
	return {.status = u->vehstatus.base(), .progress = u->progress};
}
template <class T>
static int32_t OPENTTD_VEHICLE_CALL GroundSvcAcceleration(OpenTTDVehicle *v) noexcept
{
	return T::From(VehicleOf(v))->GetAcceleration();
}
template <class T>
static OpenTTDGroundCrash OPENTTD_VEHICLE_CALL GroundSvcCrash(OpenTTDVehicle *v, bool flooded) noexcept
{
	T *u = T::From(VehicleOf(v));
	uint victims = u->T::GroundVehicleBase::Crash(flooded);
	return {.victims = victims, .front = u->IsFrontEngine()};
}
template <class T>
static void OPENTTD_VEHICLE_CALL GroundSvcFirstCargoChanged(OpenTTDVehicle *v) noexcept
{
	T::From(VehicleOf(v))->First()->CargoChanged();
}
template <class T>
static void OPENTTD_VEHICLE_CALL GroundSvcGoToDepotService(OpenTTDVehicle *v, uint16_t depot) noexcept
{
	T *u = T::From(VehicleOf(v));
	SetBit(u->gv_flags, GVF_SUPPRESS_IMPLICIT_ORDERS);
	u->current_order.MakeGoToDepot(DepotID(depot), OrderDepotTypeFlag::Service);
}
template <class T>
static void OPENTTD_VEHICLE_CALL GroundSvcWriteFirstEngine(OpenTTDVehicle *v, uint16_t engine) noexcept
{
	T::From(VehicleOf(v))->gcache.first_engine = EngineID(engine);
}
template <class T>
static void OPENTTD_VEHICLE_CALL GroundSvcWriteLength(OpenTTDVehicle *part, uint8_t length, OpenTTDVehicle *front, uint16_t total_length) noexcept
{
	T *u = T::From(VehicleOf(part));
	u->gcache.cached_veh_length = length;
	T::From(VehicleOf(front))->gcache.cached_total_length = total_length;
	u->UpdateVisualEffect();
}

/** One GroundVehicle service table per vehicle type. */
template <class T>
const OpenTTDGroundServices &GetRustGroundServices() noexcept
{
	static const OpenTTDGroundServices services{
		.move_incline = GroundSvcMoveIncline<T>,
		.update_viewport = GroundSvcUpdateViewport<T>,
		.turn = GroundSvcTurn<T>,
		.do_update_speed = GroundSvcDoUpdateSpeed<T>,
		.set_last_speed = GroundSvcSetLastSpeed<T>,
		.acceleration = GroundSvcAcceleration<T>,
		.crash = GroundSvcCrash<T>,
		.first_cargo_changed = GroundSvcFirstCargoChanged<T>,
		.go_to_depot_service = GroundSvcGoToDepotService<T>,
		.write_first_engine = GroundSvcWriteFirstEngine<T>,
		.write_length = GroundSvcWriteLength<T>,
	};
	return services;
}
#endif /* RUST_VEHICLE_SERVICES_H */
