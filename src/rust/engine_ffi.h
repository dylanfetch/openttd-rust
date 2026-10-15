/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file engine_ffi.h Engine lifecycle's synchronous typed boundary. */
#ifndef RUST_ENGINE_FFI_H
#define RUST_ENGINE_FFI_H
#include "services_ffi.h"
#include "engine_lifecycle.hpp"
#include "../engine_type.h"
#include "../vehicle_type.h"
struct OpenTTDEngine;
struct OpenTTDVehicle;
struct Company;
struct OpenTTDEngineRef { const OpenTTDEngine *shell; OpenTTDEngineLifecycle *state; uint16_t id; VehicleType type; };
struct OpenTTDEngineSyncVariant { uint16_t parent; bool sync; };
struct OpenTTDEngineDisplayVariant { uint16_t parent, display; };
struct OpenTTDEnginePreviewVariant { uint16_t parent; bool join; };
struct OpenTTDEngineReliability { int32_t base_life; int8_t retire_early; };
struct OpenTTDEngineStartup { int32_t base_intro; uint32_t grfid; int32_t base_life; uint8_t decay_speed; bool climate; };
struct OpenTTDEngineSeed { int32_t base_intro, lifelength; uint8_t decay_speed; int8_t retire_early; };
struct OpenTTDEngineYear { int32_t base_intro, lifelength; };
struct OpenTTDEngineYMD { int32_t year; uint8_t month, day; };
struct OpenTTDEngineStartSettings { uint32_t generation_seed; int32_t starting_year, date; };
struct OpenTTDEngineCompany { const Company *shell; uint8_t id; };
struct OpenTTDEngineCompanyPreview { uint8_t blocked; int32_t performance; };
struct OpenTTDEngineVehicle { OpenTTDVehicle *shell; uint32_t id; uint8_t owner; VehicleType type; };
struct OpenTTDEngineVehicleCargo { const OpenTTDEngine *engine; uint8_t cargo; };
/** Every callback is noexcept; script events only enqueue, no VM executes.
 * No owner borrow survives any callback, including native type availability,
 * articulated/NewGRF reads, windows, and recursively joined variants.
 * Pool traversal preserves IDs/order; prototypes arrive by shell, never ID. */
struct OpenTTDEngineServices {
	const OpenTTDSharedServices *shared;
	const OpenTTDRandomServices *rng;
	OpenTTDEngineRef (*next)(uint32_t) noexcept;
	OpenTTDEngineRef (*resolve)(uint16_t) noexcept;
	OpenTTDEngineSyncVariant (*sync_variant)(const OpenTTDEngine *) noexcept;
	OpenTTDEngineDisplayVariant (*display_variant)(const OpenTTDEngine *) noexcept;
	OpenTTDEnginePreviewVariant (*preview_variant)(const OpenTTDEngine *) noexcept;
	uint8_t (*extra_flags)(const OpenTTDEngine *) noexcept;
	uint16_t (*engine_id)(const OpenTTDEngine *) noexcept;
	OpenTTDEngineReliability (*reliability)(const OpenTTDEngine *) noexcept;
	OpenTTDEngineStartup (*startup)(const OpenTTDEngine *) noexcept;
	OpenTTDEngineSeed (*seed_config)(const OpenTTDEngine *) noexcept;
	int32_t (*base_intro)(const OpenTTDEngine *) noexcept;
	OpenTTDEngineYear (*aging_year)(const OpenTTDEngine *) noexcept;
	int32_t (*calendar_year)() noexcept;
	bool (*never_expire)() noexcept;
	OpenTTDEngineStartSettings (*start_settings)() noexcept;
	uint16_t (*train_limit)() noexcept;
	uint16_t (*road_limit)() noexcept;
	uint16_t (*ship_limit)() noexcept;
	uint16_t (*aircraft_limit)() noexcept;
	bool (*ai_train_disabled)() noexcept;
	bool (*ai_road_disabled)() noexcept;
	bool (*ai_ship_disabled)() noexcept;
	bool (*ai_aircraft_disabled)() noexcept;
	OpenTTDEngineYMD (*date_to_ymd)(int32_t) noexcept;
	int32_t (*ymd_to_date)(int32_t, uint8_t, uint8_t) noexcept;
	bool (*enabled)(const OpenTTDEngine *) noexcept;
	bool (*climate)(const OpenTTDEngine *) noexcept;
	RailVehicleTypes (*rail_kind)(const OpenTTDEngine *) noexcept;
	void (*clear_last_variant)(const OpenTTDEngine *) noexcept;
	void (*refresh_type)(VehicleType) noexcept;
	void (*refresh_lists)(bool) noexcept;
	OpenTTDEngineCompany (*company_next)(uint32_t) noexcept;
	OpenTTDEngineCompanyPreview (*company_preview)(const Company *) noexcept;
	OpenTTDEngineVehicle (*vehicle_next)(uint32_t) noexcept;
	OpenTTDEngineVehicleCargo (*vehicle_cargo)(OpenTTDVehicle *) noexcept;
	bool (*can_carry)(const OpenTTDEngine *) noexcept;
	uint64_t (*refit_mask)(const OpenTTDEngine *) noexcept;
	void (*update_railtypes)(uint8_t) noexcept;
	void (*update_roadtypes)(uint8_t) noexcept;
	void (*date_company_types)(uint8_t, int32_t) noexcept;
	bool (*company_valid)(uint8_t) noexcept;
	uint64_t (*compatible_railtypes)(const OpenTTDEngine *) noexcept;
	uint64_t (*powered_roadtypes)(const OpenTTDEngine *) noexcept;
	uint64_t (*company_railtypes)(uint8_t) noexcept;
	uint64_t (*company_roadtypes)(uint8_t) noexcept;
	uint (*company_engine_count)(uint8_t, uint16_t) noexcept;
	void (*block_preview)(uint8_t) noexcept;
	uint64_t (*introduces_railtypes)(const OpenTTDEngine *) noexcept;
	uint64_t (*introduces_roadtypes)(const OpenTTDEngine *) noexcept;
	void (*set_railtypes)(uint8_t, uint64_t, int32_t) noexcept;
	void (*set_roadtypes)(uint8_t, uint64_t, int32_t) noexcept;
	uint8_t (*current_company)() noexcept;
	uint8_t (*local_company)() noexcept;
	void (*toolbar_enable)(VehicleType) noexcept;
	void (*toolbar_release)(VehicleType) noexcept;
	void (*close_preview)(uint16_t) noexcept;
	void (*invalidate_preview)(uint16_t) noexcept;
	void (*offer_preview)(uint16_t, uint8_t, bool) noexcept;
	void (*available_event)(uint16_t) noexcept;
	void (*available_news)(uint16_t) noexcept;
	void (*no_engines)(int32_t) noexcept;
	void (*screen_dirty)() noexcept;
	size_t (*utf8_length)(const uint8_t *, size_t) noexcept;
};
extern "C" {
void openttd_rust_engine_reliability(const OpenTTDEngineServices *, OpenTTDEngineRef, bool);
void openttd_rust_engine_start_one(const OpenTTDEngineServices *, OpenTTDEngineRef, OpenTTDEngineYMD, uint32_t);
void openttd_rust_engine_aging_stops(const OpenTTDEngineServices *);
void openttd_rust_engine_start_all(const OpenTTDEngineServices *);
void openttd_rust_engine_daily(const OpenTTDEngineServices *);
void openttd_rust_engine_monthly(const OpenTTDEngineServices *);
void openttd_rust_engine_clear_hidden(const OpenTTDEngineServices *, uint8_t);
bool openttd_rust_engine_buildable(const OpenTTDEngineServices *, OpenTTDEngineRef, VehicleType, uint8_t);
bool openttd_rust_engine_variant_hidden(const OpenTTDEngineServices *, OpenTTDEngineRef, uint8_t);
uint8_t openttd_rust_engine_visibility(const OpenTTDEngineServices *, OpenTTDEngineRef, bool, bool);
uint8_t openttd_rust_engine_want_preview(const OpenTTDEngineServices *, OpenTTDEngineRef, bool);
uint8_t openttd_rust_engine_control(const OpenTTDEngineServices *, OpenTTDEngineRef, uint8_t, bool, bool);
uint8_t openttd_rust_engine_rename(const OpenTTDEngineServices *, OpenTTDEngineRef, OpenTTDByteSpan, bool);
void openttd_rust_engine_check(const OpenTTDEngineServices *);
}
#endif /* RUST_ENGINE_FFI_H */
