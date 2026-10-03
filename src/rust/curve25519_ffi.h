// Monocypher version 4.0.2
//
// This file is dual-licensed.  Choose whichever licence you want from
// the two licences listed below.
//
// The first licence is a regular 2-clause BSD licence.  The second licence
// is the CC-0 from Creative Commons. It is intended to release Monocypher
// to the public domain.  The BSD licence serves as a fallback option.
//
// SPDX-License-Identifier: BSD-2-Clause OR CC0-1.0
//
// ------------------------------------------------------------------------
//
// Copyright (c) 2017-2020, Loup Vaillant
// All rights reserved.
//
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the
//    distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// ------------------------------------------------------------------------
//
// Written in 2017-2020 by Loup Vaillant
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related neighboring rights to this software to the public domain
// worldwide.  This software is distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along
// with this software.  If not, see
// <https://creativecommons.org/publicdomain/zero/1.0/>

/** @file curve25519_ffi.h Coarse coupled scalar, Edwards and Elligator facades. */
#ifndef OPENTTD_CURVE25519_FFI_H
#define OPENTTD_CURVE25519_FFI_H
#include "x25519_ffi.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Same synchronous nonthrowing wipe/verify32 table (ABI ID 25). No field,
 * point, scalar/context layout, retained pointer, heap or vendor import crosses
 * FFI. Buffers retain original defined extents and sequential alias semantics;
 * key-pair seed/output writes retain source order. Message sizes are native
 * size_t. Original variable-time public verification and Elligator retries
 * remain distinct from secret table selection. Rust panics abort. */
void OPENTTD_CRYPTO_CALL openttd_rust_eddsa_reduce(const struct OpenTTDX25519Leaves *, uint8_t reduced[32], const uint8_t expanded[64]);
void OPENTTD_CRYPTO_CALL openttd_rust_eddsa_mul_add(const struct OpenTTDX25519Leaves *, uint8_t r[32], const uint8_t a[32], const uint8_t b[32], const uint8_t c[32]);
void OPENTTD_CRYPTO_CALL openttd_rust_eddsa_scalarbase(const struct OpenTTDX25519Leaves *, uint8_t point[32], const uint8_t scalar[32]);
int32_t OPENTTD_CRYPTO_CALL openttd_rust_eddsa_check_equation(const struct OpenTTDX25519Leaves *, const uint8_t signature[64], const uint8_t public_key[32], const uint8_t h[32]);
void OPENTTD_CRYPTO_CALL openttd_rust_eddsa_key_pair(const struct OpenTTDX25519Leaves *, uint8_t secret_key[64], uint8_t public_key[32], uint8_t seed[32]);
void OPENTTD_CRYPTO_CALL openttd_rust_eddsa_sign(const struct OpenTTDX25519Leaves *, uint8_t signature [64], const uint8_t secret_key[64], const uint8_t *message, size_t message_size);
int32_t OPENTTD_CRYPTO_CALL openttd_rust_eddsa_check(const struct OpenTTDX25519Leaves *, const uint8_t signature[64], const uint8_t public_key[32], const uint8_t *message, size_t message_size);
void OPENTTD_CRYPTO_CALL openttd_rust_eddsa_to_x25519(const struct OpenTTDX25519Leaves *, uint8_t x25519[32], const uint8_t eddsa[32]);
void OPENTTD_CRYPTO_CALL openttd_rust_x25519_to_eddsa(const struct OpenTTDX25519Leaves *, uint8_t eddsa[32], const uint8_t x25519[32]);
void OPENTTD_CRYPTO_CALL openttd_rust_x25519_dirty_small(const struct OpenTTDX25519Leaves *, uint8_t public_key[32], const uint8_t secret_key[32]);
void OPENTTD_CRYPTO_CALL openttd_rust_x25519_dirty_fast(const struct OpenTTDX25519Leaves *, uint8_t public_key[32], const uint8_t secret_key[32]);
void OPENTTD_CRYPTO_CALL openttd_rust_elligator_map(const struct OpenTTDX25519Leaves *, uint8_t curve[32], const uint8_t hidden[32]);
int32_t OPENTTD_CRYPTO_CALL openttd_rust_elligator_rev(const struct OpenTTDX25519Leaves *, uint8_t hidden[32], const uint8_t public_key[32], uint8_t tweak);
void OPENTTD_CRYPTO_CALL openttd_rust_elligator_key_pair(const struct OpenTTDX25519Leaves *, uint8_t hidden[32], uint8_t secret_key[32], uint8_t seed[32]);
void OPENTTD_CRYPTO_CALL openttd_rust_x25519_inverse(const struct OpenTTDX25519Leaves *, uint8_t blind_salt [32], const uint8_t private_key[32], const uint8_t curve_point[32]);
#ifdef __cplusplus
}
#endif
#endif /* OPENTTD_CURVE25519_FFI_H */
