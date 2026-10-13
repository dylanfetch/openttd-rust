/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file station_services.h Shared native station observations for Rust ports. */
#ifndef RUST_STATION_SERVICES_H
#define RUST_STATION_SERVICES_H
#include "../station_base.h"
#include "station_activity_ffi.h"

inline Owner OpenTTDStationOwner(const BaseStation *station) noexcept { return station->owner; }
inline OpenTTDStationActivity OpenTTDStationActivityOf(const Station *station) noexcept
{
	return openttd_rust_station_activity(station->rust_service.get());
}
#endif /* RUST_STATION_SERVICES_H */
