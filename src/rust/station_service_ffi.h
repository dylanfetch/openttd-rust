/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file station_service_ffi.h Canonical Rust station service state and direct world leaves. */
#ifndef RUST_STATION_SERVICE_FFI_H
#define RUST_STATION_SERVICE_FFI_H
#include <cstdint>
#include <cstddef>
struct OpenTTDStationService;
struct OpenTTDStationCargo {
	uint32_t max_waiting_cargo;
	uint8_t status, time_since_pickup, rating, last_speed, last_age, amount_fract;
};
struct OpenTTDStationFields {
	uint64_t always_accepted;
	uint8_t delete_ctr, time_since_load, time_since_unload, last_vehicle_type;
};
/* Each BaseStation owns one stable allocation, created for ordinary and indexed
 * allocation and destroyed after its derived shell and cargo storage. Scalar
 * addresses stay valid until owner destruction; native facade references alias
 * this allocation rather than mirror it. All calls are serial. Rust must use
 * raw field-sized access, ending access before a world service; C++ may read or
 * mutate any metadata through callbacks. No reference/slice into state survives
 * a world call. Saving and legacy loading address the same canonical scalars.
 * Cargo status starts a trivially-copyable native EnumBitSet<uint8_t> lifetime
 * at shell construction; the ABI pins size/alignment and Rust accesses only its
 * byte representation through raw pointers. No C++ reference object is saved.
 * Stable queue nodes have std::list identity/erase semantics; first/next/value
 * require live entries, and erase_node requires an entry from that owner.
 * The native save facade alone uses a call-local std::list for partial loading
 * and pointer fixups, committing on native error unwind outside Rust frames.
 * Refitting returns CommandCost/capacities without ordinary exceptions. Its
 * capacity/NewGRF cache updates and command-scope window invalidations may read
 * canonical metadata; no reference to canonical owner fields, cargo metadata or queue spans the call.
 * Cargo Stage/Unload can reenter #117 payment/delivery and later industry owners;
 * copied observations and raw field accesses end before each operation. NewGRF
 * load/house/cargo/vehicle/station/airport/road resolvers execute sprite-group
 * programs, never script VMs or arbitrary game commands. Command-scope window
 * invalidation may read the owner; unrestricted GUI-scope runs later in C++.
 * Opaque world handles obey the original pool lifetimes. Panics and environmental
 * service failures abort. Packet, flow, geometry and map storage remain C++.
 */
/* Station read: 0 pool bound,1 tick counter(low32),2 mode editor,
 * 3 facilities,4 in-use,5 is-real-station,6 owner,7 local-owner,
 * 8 rectangle-empty,9 town statue,10 selectgoods,11 rating cheat,
 * 12 available,13 packet-destination-count,14 cargo valid,
 * 15 cargo rating callback,16 passenger class,17 exclusivity counter,
 * 18 exclusivity owner,19 cargo packet allocation available,20 house tile,
 * 21 tick counter high32.
 * Effects:0 rating redraw,1 demand,2 acceptance news,3 acceptance redraw,
 * 4 delete shell,5 periodic animation,6 watched house animation,
 * 7 append incoming packet,8 update link supply,9 station-list invalidation,
 * 10 new-cargo animation/randomisation,11 waiting redraw.
 * Temporary native iterator handles contain shared geometry/map/packet storage
 * operations only; the Rust caller owns traversal and releases them on exit.
 */
struct OpenTTDStationServices {
	void *(*get)(uint32_t) noexcept;
	OpenTTDStationService *(*owner)(void *) noexcept;
	uint32_t (*read)(void *, uint8_t, uint32_t) noexcept;
	void (*effect)(void *, uint8_t, uint32_t, uint32_t, uint32_t) noexcept;
	uint16_t (*rating_callback)(void *, uint8_t, uint32_t, uint32_t) noexcept;
	void *(*tiles)(void *) noexcept;
	uint32_t (*tile_next)(void *, uint8_t) noexcept;
	void (*tile_destroy)(void *) noexcept;
	void (*accept_tile)(uint32_t, uint32_t *, uint64_t *) noexcept;
	void *(*truncate)(void *, uint8_t, uint32_t) noexcept;
	uint32_t (*truncate_next)(void *, uint32_t *) noexcept;
	void (*truncate_destroy)(void *) noexcept;
	uint32_t (*random)() noexcept;
};
struct OpenTTDStationEdge {
	void *destination;
	int32_t last_update, unrestricted, restricted;
	uint32_t distance;
	uint16_t node;
};
struct OpenTTDStationLinks {
	void *(*graph)(void *, uint8_t) noexcept;
	uint32_t (*read)(void *, uint8_t, uint32_t, uint32_t) noexcept;
	void (*edge)(void *, uint16_t, uint32_t, OpenTTDStationEdge *) noexcept;
	void (*effect)(void *, uint8_t, uint16_t, uint16_t) noexcept;
	void *(*order_list)(uint32_t) noexcept;
	size_t (*order_read)(void *, uint8_t, size_t) noexcept;
	void *(*order_vehicle)(void *) noexcept;
	void *(*next_vehicle)(void *, uint8_t) noexcept;
	uint32_t (*vehicle_read)(void *, uint8_t) noexcept;
	void (*refresh)(void *) noexcept;
	void (*reroute)(void *, void *, uint8_t, uint16_t, uint16_t) noexcept;
};
struct OpenTTDStationLoading {
	int64_t (*read)(void *, uint8_t) noexcept;
	void (*write)(void *, uint8_t, int64_t) noexcept;
	void *(*next)(void *, uint8_t) noexcept;
	void *(*next_stations)(void *) noexcept;
	void (*next_stations_destroy)(void *) noexcept;
	uint32_t (*cargo)(void *, void *, uint8_t, uint32_t, void *, void *) noexcept;
	uint16_t (*load_callback)(void *, uint8_t) noexcept;
	void *(*payment)(void *, uint8_t) noexcept;
	void (*effect)(void *, void *, uint8_t, uint8_t) noexcept;
	uint64_t (*refit)(void *, uint8_t, uint8_t, int64_t *) noexcept;
};
const OpenTTDStationServices &GetRustStationServices() noexcept;
extern "C" {
OpenTTDStationService *openttd_rust_station_service_new();
void openttd_rust_station_service_destroy(OpenTTDStationService *);
OpenTTDStationFields *openttd_rust_station_service_fields(OpenTTDStationService *);
OpenTTDStationCargo *openttd_rust_station_service_cargo(OpenTTDStationService *, uint8_t);
size_t openttd_rust_station_queue_size(const OpenTTDStationService *);
void *openttd_rust_station_queue_get(const OpenTTDStationService *, size_t);
void openttd_rust_station_queue_push(OpenTTDStationService *, void *);
void openttd_rust_station_queue_erase(OpenTTDStationService *, size_t);
void openttd_rust_station_queue_remove(OpenTTDStationService *, void *);
void openttd_rust_station_queue_clear(OpenTTDStationService *);
void *openttd_rust_station_queue_first(const OpenTTDStationService *);
void *openttd_rust_station_queue_next(const OpenTTDStationService *, const void *);
void *openttd_rust_station_queue_value(const void *);
void *openttd_rust_station_queue_erase_node(OpenTTDStationService *, void *);
void openttd_rust_station_prepare(void *, const OpenTTDStationServices *, const OpenTTDStationLoading *);
void openttd_rust_station_load(void *, const OpenTTDStationServices *, const OpenTTDStationLoading *);
void openttd_rust_station_acceptance(void *, const OpenTTDStationServices *, uint8_t);
void openttd_rust_station_rating(void *, const OpenTTDStationServices *);
void openttd_rust_station_watched(void *, const OpenTTDStationServices *);
void openttd_rust_station_tick(const OpenTTDStationServices *, const OpenTTDStationLinks *);
void openttd_rust_station_stale(void *, const OpenTTDStationServices *, const OpenTTDStationLinks *);
void openttd_rust_station_reroute(void *, uint8_t, uint16_t, uint16_t, const OpenTTDStationServices *, const OpenTTDStationLinks *);
void openttd_rust_station_monthly(const OpenTTDStationServices *);
void openttd_rust_station_modify_rating(OpenTTDStationService *, int32_t);
uint32_t openttd_rust_station_distribute(uint8_t, uint32_t, uint32_t, void *const *, size_t, uint8_t, const OpenTTDStationServices *);

}
#endif
