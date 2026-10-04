/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fleet_ffi.h WIP canonical fleet state access. */
#ifndef RUST_FLEET_FFI_H
#define RUST_FLEET_FFI_H
#include <cstdint>
#include <cstddef>
/* Serial game-thread calls only. Owners are stable Rust allocations with C++
 * scalar object lifetimes in their prefix. C++ views alias canonical storage.
 * Rust access scopes end before returning; no reference into a shell spans calls.
 * GRPS/ERNW/PLYR/VEHS adapters retain widths, references and indexed pool identity.
 * Name/children/engine maps are Rust containers. Exports are call-local copies.
 * WIP: command/replacement control and tick-end drain remain C++. No Rust
 * reference spans a caller or reentry. Panics/allocation failure abort.
 * Prefix layouts are checked in ABI340-342. */
extern "C" {
void *openttd_rust_fleet_state_create(uint32_t);
void openttd_rust_fleet_state_destroy(uint32_t, void *);
void openttd_rust_fleet_state_copy(uint32_t, void *, const void *);
size_t openttd_rust_fleet_name(const void *, uint8_t *, size_t);
void openttd_rust_fleet_set_name(void *, const uint8_t *, size_t);
uint32_t openttd_rust_fleet_child(const void *, uint32_t);
void openttd_rust_fleet_child_change(void *, uint32_t, bool);
uint16_t openttd_rust_fleet_engine_count(const void *, uint16_t);
void openttd_rust_fleet_engine_change(void *, uint16_t, int32_t);
void openttd_rust_fleet_stats_clear(void *, uint32_t);
}
#endif /* RUST_FLEET_FFI_H */
