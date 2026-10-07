/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry_ffi.h Canonical industry storage and periodic control. */
#ifndef RUST_INDUSTRY_FFI_H
#define RUST_INDUSTRY_FFI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct OpenTTDIndustry OpenTTDIndustry;
typedef struct OpenTTDIndustryBuilder OpenTTDIndustryBuilder;
typedef struct OpenTTDIndustryFields {
	uint64_t valid_history;
	int32_t last_prod_year;
	uint16_t counter;
	uint8_t prod_level, was_cargo_delivered, ctlflags;
} OpenTTDIndustryFields;
typedef struct OpenTTDIndustryBuildFields {
	uint32_t probability;
	uint8_t min_number;
	uint16_t target_count, max_wait, wait_count;
} OpenTTDIndustryBuildFields;
typedef struct OpenTTDIndustryBuilderFields {
	OpenTTDIndustryBuildFields builddata[240];
	uint32_t wanted_inds, daily_counter, daily_increment, sound_tile;
	uint8_t sound_ctr;
} OpenTTDIndustryBuilderFields;
typedef struct OpenTTDIndustrySlots { void *data; size_t size; } OpenTTDIndustrySlots;
OpenTTDIndustry *openttd_rust_industry_new(void);
void openttd_rust_industry_destroy(OpenTTDIndustry *owner);
OpenTTDIndustryFields *openttd_rust_industry_fields(OpenTTDIndustry *owner);
OpenTTDIndustrySlots openttd_rust_industry_slots(OpenTTDIndustry *owner, uint8_t produced, uint8_t operation, size_t count);
void *openttd_rust_industry_history(void *accepted);
OpenTTDIndustryBuilder *openttd_rust_industry_builder_new(void);
void openttd_rust_industry_builder_destroy(OpenTTDIndustryBuilder *owner);
OpenTTDIndustryBuilderFields *openttd_rust_industry_builder_fields(OpenTTDIndustryBuilder *owner);

typedef struct OpenTTDIndustryObservation {
	OpenTTDIndustry *owner;
	uint32_t tile, behaviour;
	uint16_t id, width, height, callbacks, sound_count;
	uint8_t life, original, minimal_cargo, type;
	uint32_t up_text, down_text, closure_text;
} OpenTTDIndustryObservation;
/** Direct services: raw records only, no borrow survives any call. */
typedef struct OpenTTDIndustryServices {
	void (*observe)(void *industry, OpenTTDIndustryObservation *out);
	void *(*next)(uint32_t from);
	uint32_t (*setting)(uint8_t field);
	uint64_t (*world)(uint8_t operation, void *industry, uint32_t a, uint32_t b, uint32_t c);
} OpenTTDIndustryServices;
void openttd_rust_industry_tick(OpenTTDIndustryBuilder *builder, const OpenTTDIndustryServices *services);
uint8_t openttd_rust_industry_transport(void *industry, const OpenTTDIndustryServices *services);
void openttd_rust_industry_farm(void *industry, const OpenTTDIndustryServices *services);
void openttd_rust_industry_recompute(void *industry, const OpenTTDIndustryServices *services);
void openttd_rust_industry_change(void *industry, uint8_t monthly, const OpenTTDIndustryServices *services);
void openttd_rust_industry_daily_start(OpenTTDIndustryBuilder *builder, uint32_t map_bits, uint8_t init_counter);
void openttd_rust_industry_daily(OpenTTDIndustryBuilder *builder, const OpenTTDIndustryServices *services);
void openttd_rust_industry_monthly(OpenTTDIndustryBuilder *builder, const OpenTTDIndustryServices *services);
void openttd_rust_industry_build_type_reset(OpenTTDIndustryBuildFields *fields);
uint8_t openttd_rust_industry_command(uint16_t id, uint8_t production, uint8_t execute, uint8_t value, const OpenTTDIndustryServices *services);
void openttd_rust_industry_build_reset(OpenTTDIndustryBuilder *builder, const OpenTTDIndustryServices *services);
void openttd_rust_industry_build_monthly(OpenTTDIndustryBuilder *builder, const OpenTTDIndustryServices *services);
void openttd_rust_industry_build_targets(OpenTTDIndustryBuilder *builder, const OpenTTDIndustryServices *services);
void openttd_rust_industry_build_try(OpenTTDIndustryBuilder *builder, const OpenTTDIndustryServices *services);
uint8_t openttd_rust_industry_build_type(OpenTTDIndustryBuildFields *fields, uint8_t type, const OpenTTDIndustryServices *services);
void *openttd_rust_industry_place(uint8_t type, uint8_t creation, uint8_t hard, const OpenTTDIndustryServices *services);
void openttd_rust_industry_trim(OpenTTDIndustry *industry);
void openttd_rust_industry_set_production(void *industry, uint8_t level, const OpenTTDIndustryServices *services);
typedef struct OpenTTDIndustryProductionResult {
	int32_t subtract[256], add[256], again;
	uint8_t cargo_input[256], cargo_output[256], version, num_input, num_output, present;
} OpenTTDIndustryProductionResult;
typedef void (*OpenTTDIndustryResolve)(void *context, uint32_t random, uint32_t parameter, OpenTTDIndustryProductionResult *out);
void openttd_rust_industry_production_callback(void *industry, void *context, uint32_t behaviour, uint8_t reason, OpenTTDIndustryResolve resolve, const OpenTTDIndustryServices *services);
#ifdef __cplusplus
}
#endif
#endif
