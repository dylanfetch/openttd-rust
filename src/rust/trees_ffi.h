/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file trees_ffi.h Rust tree generation, simulation and command ownership. */
#ifndef TREES_FFI_H
#define TREES_FFI_H
#include <cstdint>
#include "services_ffi.h"

struct OpenTTDTreeAction {
	uint32_t kind, tile, a, b;
	int64_t cost;
};
extern "C" {
/* Entry kinds: 0 generate, 1 scatter, 2 place(tile,r,keep), 3 plant(tile,type,count,growth),
 * 5 tile loop, 6 tick, 8 command(end,start,type,diagonal), 9 clear. Original game
 * preconditions apply. Settings[12]: tick, build/clear Money bits, map size/x/y,
 * snowline bits, climate, placer, extra, height limit, editor/ambient/freeform/
 * execute/company-valid bits. Settings and leaf callbacks must be noexcept,
 * nonreentrant and borrow output only for the call. Shared table is copied.
 * Progress leaves return 1 only for cancellation, before progress effects; Rust
 * immediately propagates it, with no resumed loop, to action 12.
 * Component leaves: progress(1,2), clear nonflood flags(3), sound(6), square
 * clear(7), town rating(8), iterator start(9), company debit(10), iterator next(11).
 * Original bodies remain the portable fallback; map/pools stay canonical C++.
 * Panic/OOM/environmental failures abort. No exception unwinds through Rust. */
void *openttd_rust_trees_create(uint32_t kind, uint32_t tile, uint32_t a, uint32_t b, uint32_t c,
	void *context, void (*settings)(void *, uint64_t *) noexcept, const OpenTTDSharedServices *,
	uint64_t (*leaf)(void *, uint32_t, uint32_t, uint32_t, uint32_t) noexcept);
/* Only actions: 0 done (a result, b error, cost), 4 water flooding (may dispatch
 * nested clears), 5 NewGRF ambient callback (arbitrary code), 10 landscape clear
 * command (may reenter tree code), 12 world-generation abort (callback + throw).
 * Actions run after advance returns; fresh
 * settings/map reads follow them. Response/cost are nested clear failure/cost. */
OpenTTDTreeAction openttd_rust_trees_advance(void *, uint64_t response, int64_t cost);
/* Destroy once, including C++ exception cleanup; no callback/context access. */
void openttd_rust_trees_destroy(void *);
/* Stable Rust-owned byte; game-thread initialization/ticks and serial save/load access only.
 * C++ never retains a C++ reference across a Rust call. DATE LoadCheck omits this field.
 * The unchanged modern and TTD/TTO descriptors access it only at serialization boundaries. */
uint8_t *openttd_rust_tree_counter();
void openttd_rust_trees_initialize();
/* Ten copied observation words; used only by the deferred editor brush. */
uint8_t openttd_rust_trees_suitable(const uint32_t *, uint8_t allow_desert);
}
#endif /* TREES_FFI_H */
