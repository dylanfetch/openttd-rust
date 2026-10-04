/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file linkgraph_ffi.h Owned computation of a copied link graph job. */
#ifndef OPENTTD_RUST_LINKGRAPH_FFI_H
#define OPENTTD_RUST_LINKGRAPH_FFI_H
#include <stdint.h>
#include <stddef.h>
#if defined(_MSC_VER)
#define OPENTTD_LINKGRAPH_CALL __cdecl
#elif defined(__i386__)
#define OPENTTD_LINKGRAPH_CALL __attribute__((cdecl))
#else
#define OPENTTD_LINKGRAPH_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
	uint32_t supply, demand, station, x, y, edge_begin, edge_count;
} OpenTTDLinkGraphNode;
typedef struct {
	uint32_t capacity, travel_time, dest;
} OpenTTDLinkGraphEdge;
typedef struct {
	uint32_t accuracy, demand_distance, demand_size, saturation, distribution, express, map_max_x, map_max_y, runtime;
} OpenTTDLinkGraphSettings;
typedef struct {
	uint32_t node, origin, via, cumulative, unrestricted, has_share;
} OpenTTDLinkGraphShare;
typedef struct OpenTTDLinkGraphResult OpenTTDLinkGraphResult;
/* Synchronous call copies plain input arrays into Rust-owned state. Inputs are
 * aligned/readable/live for the call, extents <= PTRDIFF_MAX; empty permits null.
 * Original graph invariants/settings apply. No world pointer is retained. Only
 * the nonthrowing abort callback reads the job's atomic flag; no game globals,
 * C++ allocations, reentry or exceptions occur from inside Rust. Panics abort.
 * Result arrays are immutable borrows of the opaque Rust owner; C++ copies them
 * before destruction, including on allocation exceptions. Destroy exactly once. */
OpenTTDLinkGraphResult *OPENTTD_LINKGRAPH_CALL openttd_rust_linkgraph_run(const OpenTTDLinkGraphNode *, size_t, const OpenTTDLinkGraphEdge *, size_t, const OpenTTDLinkGraphSettings *, const void *, uint8_t (OPENTTD_LINKGRAPH_CALL *)(const void *));
const OpenTTDLinkGraphShare *OPENTTD_LINKGRAPH_CALL openttd_rust_linkgraph_shares(const OpenTTDLinkGraphResult *, size_t *);
const uint32_t *OPENTTD_LINKGRAPH_CALL openttd_rust_linkgraph_edges(const OpenTTDLinkGraphResult *);
void OPENTTD_LINKGRAPH_CALL openttd_rust_linkgraph_destroy(OpenTTDLinkGraphResult *);
#ifdef __cplusplus
}
#endif
#endif /* OPENTTD_RUST_LINKGRAPH_FFI_H */
