/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file settings_services.h Shared native setting definitions consumed by game ports. */
#ifndef RUST_SETTINGS_SERVICES_H
#define RUST_SETTINGS_SERVICES_H
#include "../settings_type.h"

inline uint8_t RustRoadAccelerationModel() noexcept { return _settings_game.vehicle.roadveh_acceleration_model; }
inline uint8_t RustRoadSide() noexcept { return _settings_game.vehicle.road_side; }
inline bool RustRoadQueue() noexcept { return _settings_game.pf.roadveh_queue; }
inline uint32_t RustMaximumDepotPenalty() noexcept { return _settings_game.pf.yapf.maximum_go_to_depot_penalty; }
inline uint8_t RustPlaneSpeed() noexcept { return _settings_game.vehicle.plane_speed; }
inline uint8_t RustPlaneCrashes() noexcept { return _settings_game.vehicle.plane_crashes; }
inline bool RustServiceAtHelipad() noexcept { return _settings_game.order.serviceathelipad; }
inline bool RustDisasterSound() noexcept { return _settings_client.sound.disaster; }
#endif /* RUST_SETTINGS_SERVICES_H */
