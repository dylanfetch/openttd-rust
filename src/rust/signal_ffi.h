/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file signal_ffi.h Rail signal-block owner boundary (ABI 820-859). */
#ifndef RUST_SIGNAL_FFI_H
#define RUST_SIGNAL_FFI_H
#include "services_ffi.h"

/* Signal work sets have process lifetime. The game-state mutex serializes all
 * entries, including calls from main and game threads. Each native service is
 * synchronous and noexcept. Rust releases every state borrow before a callback;
 * map, occupancy and dirtying cannot unwind ordinary-play exceptions. */
struct OpenTTDSignalServices {
	bool (*train_on_tile)(uint32_t) noexcept;
	bool (*train_on_tracks)(uint32_t, uint8_t) noexcept;
	void (*overflow_tbu)() noexcept;
	void (*overflow_tbd)() noexcept;
	void (*overflow_global)() noexcept;
	void (*assert_company)(uint8_t) noexcept;
};

extern "C" {
void openttd_rust_signal_flush(const OpenTTDSharedServices *, const OpenTTDSignalServices *, uint8_t);
void openttd_rust_signal_add_track(const OpenTTDSharedServices *, const OpenTTDSignalServices *, uint8_t, uint32_t, uint8_t, uint8_t);
void openttd_rust_signal_add_side(const OpenTTDSharedServices *, const OpenTTDSignalServices *, uint8_t, uint32_t, uint8_t, uint8_t);
uint8_t openttd_rust_signal_segment(const OpenTTDSharedServices *, const OpenTTDSignalServices *, uint8_t, uint32_t, uint8_t, uint8_t);
void openttd_rust_signal_both(const OpenTTDSharedServices *, const OpenTTDSignalServices *, uint8_t, uint32_t, uint8_t, uint8_t);
}
#endif /* RUST_SIGNAL_FFI_H */
