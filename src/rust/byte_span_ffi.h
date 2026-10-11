/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file byte_span_ffi.h Synchronous borrowed bytes, including embedded NULs. */
#ifndef RUST_BYTE_SPAN_FFI_H
#define RUST_BYTE_SPAN_FFI_H
#include <stddef.h>
#include <stdint.h>
/** The producer defines the lifetime. Consume synchronously before mutation,
 * destruction, or a callback that may mutate the owner; never retain the span.
 * Empty spans need no dereference and may have a null data pointer. */
typedef struct OpenTTDByteSpan { const uint8_t *data; size_t size; } OpenTTDByteSpan;
#endif /* RUST_BYTE_SPAN_FFI_H */
