/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file effect_ffi.h Rust effect owners and return-to-C++ action protocol. */
#ifndef OPENTTD_RUST_EFFECT_FFI_H
#define OPENTTD_RUST_EFFECT_FFI_H
#include <stdint.h>
#if defined(_MSC_VER)
#define OPENTTD_EFFECT_CALL __cdecl
#elif defined(__i386__)
#define OPENTTD_EFFECT_CALL __attribute__((cdecl))
#else
#define OPENTTD_EFFECT_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct OpenTTDEffectState OpenTTDEffectState;
typedef struct {
	int32_t x, y, z;
	uint32_t sprite;
	uint8_t progress, spritenum, subtype, ambient;
	uint32_t sprite_write;
} OpenTTDEffectView;
typedef struct { uint32_t phase, animation, tile, random; } OpenTTDEffectCursor;
typedef struct { uint32_t (OPENTTD_EFFECT_CALL *industry)(int32_t, int32_t, uint32_t *); } OpenTTDEffectLeaves;
/* One shell owns one zero-initialized state, including indexed loading. Destroy
 * once after its final use. get/set field0=uint16 animation, field1=uint8 substate.
 * Views/cursors are disjoint initialized scalar records valid/exclusive for a
 * synchronous call; no world/STL object is borrowed or retained. Cursor starts
 * zero per invocation and survives only that invocation's external actions.
 * The sole leaf is a nonthrowing/nonreentrant map query (0 not industry,1 industry,
 * 2 bubble catcher; also writes tile). All RNG, viewport, sound, animated-tile and
 * deletion actions run after Rust returns. Reacquire shared observations before
 * resuming; no Rust borrow survives external work/reentry/deletion. Original
 * subtype/table/coordinate preconditions apply. Panics/OOM abort; no unwinding.
 * Actions:0 done,1 viewport+done,2 viewport+resume,3 burst-sound+resume,
 * 4 success-sound+resume,5 animated-tile+resume,6 delete,7 RNG+resume.
 * sprite_write:0 unchanged,1 scalar increment,2 original sequence.Set. */
OpenTTDEffectState *OPENTTD_EFFECT_CALL openttd_rust_effect_new(void);
void OPENTTD_EFFECT_CALL openttd_rust_effect_destroy(OpenTTDEffectState *);
void OPENTTD_EFFECT_CALL openttd_rust_effect_set(OpenTTDEffectState *, uint8_t field, uint16_t value);
uint16_t OPENTTD_EFFECT_CALL openttd_rust_effect_get(const OpenTTDEffectState *, uint8_t field);
uint8_t OPENTTD_EFFECT_CALL openttd_rust_effect_step(OpenTTDEffectState *, OpenTTDEffectView *, OpenTTDEffectCursor *, const OpenTTDEffectLeaves *, uint8_t initialize);
#ifdef __cplusplus
}
#endif
#endif /* OPENTTD_RUST_EFFECT_FFI_H */
