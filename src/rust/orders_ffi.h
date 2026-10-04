/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file orders_ffi.h Canonical order, shared-list and timetable owners. */
#ifndef RUST_ORDERS_FFI_H
#define RUST_ORDERS_FFI_H
#include <cstddef>
#include <cstdint>
struct OpenTTDOrderFields {
	uint8_t type, flags;
	uint16_t destination;
	uint8_t refit_cargo;
	uint16_t wait_time, travel_time, max_speed;
};
struct OpenTTDConsistState {
	int32_t current_order_time, lateness_counter;
	uint64_t timetable_start, last_departure, next_departure;
	int32_t round_trip_time;
	uint8_t real_index, implicit_index;
	uint16_t vehicle_flags;
};
struct OpenTTDVehicleOrderState {
	OpenTTDOrderFields current;
	void *orders, *next_shared, *previous_shared;
};
struct OpenTTDOrderListState {
	uint8_t manual;
	uint32_t vehicles;
	void *first_shared;
	int32_t timetable_duration, total_duration;
};
struct OpenTTDOrderBackupState {
	uint32_t user, tile;
	uint16_t group;
	const void *clone;
};
struct OpenTTDOrderVector;
struct OpenTTDOrdersLeaves {
	OpenTTDVehicleOrderState *(*vehicle)(void *) noexcept;
	OpenTTDConsistState *(*consist)(void *) noexcept;
	OpenTTDOrderListState *(*list)(void *) noexcept;
	OpenTTDOrderVector *(*vector)(void *) noexcept;
	OpenTTDOrderBackupState *(*backup)(void *) noexcept;
	OpenTTDOrderVector *(*backup_vector)(void *) noexcept;
	OpenTTDConsistState *(*backup_consist)(void *) noexcept;
	uint64_t (*query)(uint32_t, void *, uint64_t, uint64_t, uint64_t) noexcept;
	void (*write)(uint32_t, void *, uint64_t, uint64_t, uint64_t) noexcept;
};
struct OpenTTDOrdersClosest { uint32_t tile; uint16_t destination; uint8_t reverse, found; };
struct OpenTTDOrdersAction { uint32_t operation; void *context; uint64_t a, b, c; };
extern "C" {
void *openttd_rust_orders_create(uint32_t, void *, uint64_t, uint64_t, uint64_t, const void *, void *, const OpenTTDOrdersLeaves *);
OpenTTDOrdersAction openttd_rust_orders_advance(void *, uint64_t);
void openttd_rust_orders_task_delete(void *);
uint32_t openttd_rust_orders_vehicle(uint32_t, void *, uint32_t, const OpenTTDOrdersLeaves *);
uint32_t openttd_rust_order_scalar(uint32_t, void *, uint32_t, uint32_t, uint32_t, uint32_t, const void *);
uint32_t openttd_rust_orders_timetable(uint32_t, void *, uint8_t, uint64_t, uint64_t, uint64_t, void *, const OpenTTDOrdersLeaves *);
uint64_t openttd_rust_orders_start_tick(int32_t, int32_t, uint16_t, uint64_t);
int32_t openttd_rust_orders_start_date(uint64_t, int32_t, uint16_t, uint64_t);
uint32_t openttd_rust_orders_list(uint32_t, void *, void *, uint32_t, uint32_t, const void *, void *, const OpenTTDOrdersLeaves *);
OpenTTDConsistState *openttd_rust_consist_new();
void openttd_rust_consist_delete(OpenTTDConsistState *);
void openttd_rust_consist_copy(OpenTTDConsistState *, const OpenTTDConsistState *);
void openttd_rust_consist_reset(OpenTTDConsistState *);
OpenTTDVehicleOrderState *openttd_rust_vehicle_orders_new();
void openttd_rust_vehicle_orders_delete(OpenTTDVehicleOrderState *);
OpenTTDOrderListState *openttd_rust_orderlist_new();
void openttd_rust_orderlist_delete(OpenTTDOrderListState *);
OpenTTDOrderBackupState *openttd_rust_orderbackup_new();
void openttd_rust_orderbackup_delete(OpenTTDOrderBackupState *);
OpenTTDOrderVector *openttd_rust_order_vector_new(void (*)(void *, size_t) noexcept, void (*)(void *, size_t) noexcept);
void openttd_rust_order_vector_delete(OpenTTDOrderVector *);
size_t openttd_rust_order_vector_size(const OpenTTDOrderVector *);
OpenTTDOrderFields *openttd_rust_order_vector_data(const OpenTTDOrderVector *);
void openttd_rust_order_vector_resize(OpenTTDOrderVector *, size_t);
void openttd_rust_order_vector_insert(OpenTTDOrderVector *, size_t, const void *);
void openttd_rust_order_vector_erase(OpenTTDOrderVector *, size_t);
void openttd_rust_order_vector_move(OpenTTDOrderVector *, size_t, size_t);
void openttd_rust_order_vector_assign(OpenTTDOrderVector *, const void *, size_t);
/* C++ establishes Order/DestinationID lifetimes in Rust-allocated cells.
 * Trivial destruction precedes freeing/replacing their allocation. */
void openttd_orders_construct(void *, size_t) noexcept;
void openttd_orders_destroy(void *, size_t) noexcept;
}
const OpenTTDOrdersLeaves &GetRustOrdersLeaves();
uint64_t RunRustOrders(uint32_t, void *, uint64_t = 0, uint64_t = 0, uint64_t = 0, const void * = nullptr, void * = nullptr);
#endif /* RUST_ORDERS_FFI_H */
