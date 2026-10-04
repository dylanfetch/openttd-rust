/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */
/** @file disaster_ffi.h Disaster owners and return-to-C++ shared-service protocol. */
#ifndef OPENTTD_RUST_DISASTER_FFI_H
#define OPENTTD_RUST_DISASTER_FFI_H
#include <stdint.h>
#if defined(_MSC_VER)
#define OPENTTD_DISASTER_CALL __cdecl
#elif defined(__i386__)
#define OPENTTD_DISASTER_CALL __attribute__((cdecl))
#else
#define OPENTTD_DISASTER_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint32_t image_override, target; uint16_t state; uint8_t flags; } OpenTTDDisasterState;
typedef struct { uint32_t kind, id, other; int64_t a, b, c, d; } OpenTTDDisasterAction;
typedef struct OpenTTDDisasterRun OpenTTDDisasterRun;
/* State allocation belongs to Rust and follows the C++ shell's lifetime. Raw
 * scalar field addresses may be used by serialization/flight helpers only when
 * no Rust call is active. They remain stable until destruction. No live mirror.
 * A run owns its continuation and IDs, never world references. Read/write leaf
 * callbacks copy records or update canonical scalar fields; they cannot throw,
 * reenter, allocate or delete objects. Every other service returns as an action.
 * C++ resumes only after completing that action; no Rust borrow spans it. C++
 * owns cleanup on exceptions. All calls are game-thread-only; panic aborts. */
OpenTTDDisasterState *OPENTTD_DISASTER_CALL openttd_rust_disaster_state_create(uint32_t);
void OPENTTD_DISASTER_CALL openttd_rust_disaster_state_destroy(OpenTTDDisasterState *);
uint16_t *OPENTTD_DISASTER_CALL openttd_rust_disaster_delay(void);
OpenTTDDisasterRun *OPENTTD_DISASTER_CALL openttd_rust_disaster_create(uint32_t, uint32_t, int64_t, int64_t, int64_t, int64_t, void *, void (OPENTTD_DISASTER_CALL *)(void *, uint32_t, uint32_t, int64_t, int64_t, int64_t *), void (OPENTTD_DISASTER_CALL *)(void *, uint32_t, uint32_t, int64_t));
OpenTTDDisasterAction OPENTTD_DISASTER_CALL openttd_rust_disaster_advance(OpenTTDDisasterRun *, int64_t);
void OPENTTD_DISASTER_CALL openttd_rust_disaster_destroy(OpenTTDDisasterRun *);
#ifdef __cplusplus
}
#endif
#endif /* OPENTTD_RUST_DISASTER_FFI_H */
