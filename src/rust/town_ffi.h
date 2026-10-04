/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_ffi.h Town growth owner and command/NewGRF action boundaries. */
#ifndef RUST_TOWN_FFI_H
#define RUST_TOWN_FFI_H
#include "services_ffi.h"
#include <cstdint>

struct OpenTTDTownState;
struct OpenTTDTownTask;
struct OpenTTDTownAction {
	uint32_t kind, town, tile, a, b, c, d;
	int64_t cost;
};
struct OpenTTDTownLeaves {
	void (*observe)(uint32_t, uint32_t, uint32_t *) noexcept;
	uint64_t (*leaf)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) noexcept;
	OpenTTDTownState *(*state)(uint32_t) noexcept;
	void (*stations)(uint32_t, uint32_t, void *, void (*)(void *, uint32_t, uint32_t, uint32_t)) noexcept;
};
/* A shell owns one state from construction (including indexed load) through
 * member destruction, even when Town::~Town returns during pool cleanup. Scalar
 * proxies contain only the owner pointer, never a second authoritative value.
 * Fields:0 grow_counter,1 growth_rate,2 funding months,3 road months,4 entire flags.
 * All table leaves are direct noexcept operations on canonical shared storage;
 * outputs are 32 copied words, type-valid fields only. Tile field24 is plain
 * IsWaterTile, distinct from the MP_WATER type which also includes coast.
 * They cannot reenter Rust,
 * throw, destroy the town, retain output pointers or execute commands/NewGRF.
 * Task operation:0 grow,1 try-house,2 build-house,3 tick all,4 rate,5 growth,
 * 6 expand,7 monthly counters,8 custom rate,9 funding,10 shared town road type;
 * 11 invokes the complete tunnel helper for the named bounded evidence gap (a=dir).
 * step yields only ordinary command/NewGRF boundaries; result/cost are copied
 * after C++ executes the action. No owner borrow survives a yield or shared leaf.
 * Tasks retain invocation-local candidate probabilities; reentrant invocations
 * have separate scratch. RAII drops tasks on exceptions, restores the caller's
 * company, and preserves all already-written state. Panics/environmental failure
 * abort. Save/load never runs growth: C++ staging commits even on partial loads. */
extern "C" {
OpenTTDTownState *openttd_rust_town_new();
void openttd_rust_town_destroy(OpenTTDTownState *);
uint16_t openttd_rust_town_get(const OpenTTDTownState *, uint8_t);
void openttd_rust_town_set(OpenTTDTownState *, uint8_t, uint16_t);
OpenTTDTownTask *openttd_rust_town_begin(const OpenTTDTownLeaves *, const OpenTTDSharedServices *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t openttd_rust_town_step(OpenTTDTownTask *, uint64_t, int64_t, OpenTTDTownAction *);
void openttd_rust_town_task_destroy(OpenTTDTownTask *);
}
#endif /* RUST_TOWN_FFI_H */
