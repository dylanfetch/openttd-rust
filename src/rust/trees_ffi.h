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

struct OpenTTDTreeAction {
	uint32_t kind, tile, a, b;
	int64_t cost;
};
extern "C" {
/* Entry kinds: 0 generate, 1 scatter, 2 place(tile,r,keep), 3 plant(tile,type,count,growth),
 * 5 tile loop, 6 tick, 8 command(end,start,type,diagonal), 9 clear. Valid game inputs only;
 * placement callers supply suitable tiles and valid nonrandom species to plant.
 * Copied settings[12]: tick, build/clear Money bits, map size/x/y, snowline bits,
 * climate, placer, extra placement, height limit, editor/ambient/freeform/execute/company-valid bits.
 * Copied tile[10]: type, bridge, zone, height, ground, density, species, count, growth,
 * snow/coast/one-raised-corner bits. Helpers read only fields valid for the tile type.
 * Leaf operations: make tree, ground/density, add count, add growth, set growth, dirty,
 * make clear, make shore, make snow, set zone, company debit, iterator read/advance.
 * Callbacks must be nonthrowing, cannot reenter Rust and borrow output only for their call.
 * Trigonometry uses the same native sinf/cosf as the original grove generator. */
void *openttd_rust_trees_create(uint32_t kind, uint32_t tile, uint32_t a, uint32_t b, uint32_t c,
	void *context, void (*settings)(void *, uint64_t *), void (*observe)(void *, uint32_t, uint32_t *),
	uint64_t (*write)(void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t),
	uint32_t (*random)(), float (*trig)(uint32_t, float));
/* Actions execute AFTER advance returns: 0 done (a result, b error, cost), 1 progress,
 * 2 set progress(a), 3 clear neighbour nonflood flags, 4 water loop, 5 ambient,
 * 6 sound(a), 7 clear square, 8 town rating(a=up), 9 start iterator(tile,a=start,b=diagonal),
 * 10 landscape clear. Response is iterator limit or clear failure; cost is nested clear cost.
 * A per-invocation owner is exclusive during advance only. Reentrant actions create separate
 * owners; all world reads following actions are fresh. Panic/OOM abort; no unwinding across ABI. */
OpenTTDTreeAction openttd_rust_trees_advance(void *, uint64_t response, int64_t cost);
/* Destroy exactly once, including C++ exceptions; no callback/context access on destroy. */
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
