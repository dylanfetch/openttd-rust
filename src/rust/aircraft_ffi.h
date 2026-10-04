/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */
/** @file aircraft_ffi.h Aircraft-private ownership and copied shared services. */
#ifndef OPENTTD_RUST_AIRCRAFT_FFI_H
#define OPENTTD_RUST_AIRCRAFT_FFI_H
#include "services_ffi.h"
#if defined(_MSC_VER)
#define OPENTTD_AIRCRAFT_CALL __cdecl
#elif defined(__i386__)
#define OPENTTD_AIRCRAFT_CALL __attribute__((cdecl))
#else
#define OPENTTD_AIRCRAFT_CALL
#endif
extern "C" {
struct OpenTTDAircraftState {
	uint32_t cached_max_range_sqr;
	uint16_t cached_max_range, cache_padding, crashed_counter, targetairport;
	uint8_t pos, previous_pos, state, last_direction, number_consecutive_turns, turn_counter, flags;
};
struct OpenTTDAircraftAction { uint32_t kind, id, other; int64_t a, b, c, d; };
struct OpenTTDAircraftRun;
/* Both owners use fixed allocations with field-sized raw access on the game
 * thread. C++ references alias only their named scalar. No owner reference may
 * span a callback. Aircraft deletion runs PreDestructor before releasing state;
 * airport ownership lasts through the enclosing Station's destructor. Modern
 * and legacy descriptors take the live scalar address and retain file widths.
 * Leaf callbacks copy records and are noexcept. Named returned services are
 * ProcessOrders, UpdateOrderDest, VehicleEnterDepot, Vehicle::Crash, depot
 * commands and deletion; these finish on the C++ stack before Rust resumes.
 * NewGRF resolvers are bounded direct calls; AI/Game insertion only queues.
 * Save/load errors never cross Rust. Rust panic/allocation failures abort. */
OpenTTDAircraftState *OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_state_new();
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_state_destroy(OpenTTDAircraftState *);
uint64_t *OPENTTD_AIRCRAFT_CALL openttd_rust_airport_blocks_new();
void OPENTTD_AIRCRAFT_CALL openttd_rust_airport_blocks_destroy(uint64_t *);
OpenTTDAircraftRun *OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_create(uint32_t, uint32_t, int64_t, int64_t, int64_t, int64_t, void *, void (OPENTTD_AIRCRAFT_CALL *)(void *, uint32_t, uint32_t, int64_t, int64_t, int64_t *), void (OPENTTD_AIRCRAFT_CALL *)(void *, uint32_t, uint32_t, int64_t), const OpenTTDSharedServices *, int64_t (OPENTTD_AIRCRAFT_CALL *)(void *, const OpenTTDAircraftAction *));
OpenTTDAircraftAction OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_advance(OpenTTDAircraftRun *, int64_t);
void OPENTTD_AIRCRAFT_CALL openttd_rust_aircraft_destroy(OpenTTDAircraftRun *);
}
#endif /* OPENTTD_RUST_AIRCRAFT_FFI_H */
