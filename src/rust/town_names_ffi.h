/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_names_ffi.h Synchronous name policy and stack uniqueness scratch. */
#ifndef RUST_TOWN_NAMES_FFI_H
#define RUST_TOWN_NAMES_FFI_H
#include "town_lifecycle_ffi.h"
#include "byte_span_ffi.h"
struct Randomizer;
using OpenTTDTownRandomizer = Randomizer;
/* The Rust set is initialized/destroyed exactly once in native stack storage.
 * Its owned nodes are algorithm scratch, not a heap-allocated FFI call bridge.
 * Only lookup/insert/clear are consumed, so set iteration order is unobservable. */
struct OpenTTDTownNamesScratch { uintptr_t words[4]; };
struct OpenTTDTownNameCursor { const OpenTTDTownHandle *town; uint16_t index; };
struct OpenTTDTownNameServices {
	OpenTTDTownNameParams (*params)() noexcept;
	uint32_t (*next_random)(OpenTTDTownRandomizer *) noexcept;
	OpenTTDTownNameCursor (*next_town)(uint32_t) noexcept;
	bool (*grf_valid)(uint32_t) noexcept;
	/* Renderer owns the temporary string until the synchronous consumer returns.
	 * Neither party retains the borrowed bytes or owner borrows across callbacks.
	 * Original rendering has no ordinary-play exceptions; environmental failures
	 * terminate at this noexcept boundary. */
	bool (*render)(OpenTTDTownNameParams, uint32_t, void *, bool (*)(void *, OpenTTDByteSpan)) noexcept;
};
const OpenTTDTownNameServices &GetRustTownNameServices() noexcept;
extern "C" {
void openttd_rust_town_names_init(OpenTTDTownNamesScratch *);
void openttd_rust_town_names_destroy(OpenTTDTownNamesScratch *);
void openttd_rust_town_names_insert(OpenTTDTownNamesScratch *, OpenTTDByteSpan);
void openttd_rust_town_names_clear(OpenTTDTownNamesScratch *);
bool openttd_rust_town_verify_name(const OpenTTDTownNameServices *, const OpenTTDSharedServices *, uint32_t, OpenTTDTownNameParams, OpenTTDTownNamesScratch *);
bool openttd_rust_town_generate_name(const OpenTTDTownNameServices *, const OpenTTDSharedServices *, OpenTTDTownRandomizer *, uint32_t *, OpenTTDTownNamesScratch *);
bool openttd_rust_town_unique_name(const OpenTTDTownNameServices *, const OpenTTDSharedServices *, OpenTTDByteSpan);
}
#endif /* RUST_TOWN_NAMES_FFI_H */
