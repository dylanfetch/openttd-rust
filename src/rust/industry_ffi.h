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
#include <stdbool.h>
#include <stdint.h>
#include "byte_span_ffi.h"
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
typedef struct OpenTTDIndustryMetadata {
	uint32_t tile;
	uint16_t width, height;
	int32_t construction_date;
	uint16_t random;
	uint8_t type, owner, random_colour, founder, construction_type, selected_layout, exclusive_supplier, exclusive_consumer;
} OpenTTDIndustryMetadata;
OpenTTDIndustryMetadata *openttd_rust_industry_metadata(OpenTTDIndustry *owner);
/* Borrowed until text mutation or owner destruction; consume before any callback. */
OpenTTDByteSpan openttd_rust_industry_text(const OpenTTDIndustry *owner);
void openttd_rust_industry_set_text(OpenTTDIndustry *owner, const uint8_t *text, size_t length);
OpenTTDIndustrySlots openttd_rust_industry_registry_view(OpenTTDIndustryBuilder *owner, uint8_t type);
void openttd_rust_industry_registry_insert(OpenTTDIndustryBuilder *owner, uint8_t type, uint16_t id);
void openttd_rust_industry_registry_erase(OpenTTDIndustryBuilder *owner, uint8_t type, uint16_t id);
void openttd_rust_industry_registry_clear(OpenTTDIndustryBuilder *owner, uint8_t type);
OpenTTDIndustry *openttd_rust_industry_new(void);
void openttd_rust_industry_destroy(OpenTTDIndustry *owner);
OpenTTDIndustryFields *openttd_rust_industry_fields(OpenTTDIndustry *owner);
/* One entry per vector operation; any mutation invalidates outstanding views. */
OpenTTDIndustrySlots openttd_rust_industry_produced_view(OpenTTDIndustry *owner);
OpenTTDIndustrySlots openttd_rust_industry_accepted_view(OpenTTDIndustry *owner);
void openttd_rust_industry_produced_reserve(OpenTTDIndustry *owner, size_t count);
void openttd_rust_industry_accepted_reserve(OpenTTDIndustry *owner, size_t count);
void openttd_rust_industry_produced_resize(OpenTTDIndustry *owner, size_t count);
void openttd_rust_industry_accepted_resize(OpenTTDIndustry *owner, size_t count);
OpenTTDIndustrySlots openttd_rust_industry_produced_emplace_back(OpenTTDIndustry *owner);
OpenTTDIndustrySlots openttd_rust_industry_accepted_emplace_back(OpenTTDIndustry *owner);
void openttd_rust_industry_produced_shrink_to_fit(OpenTTDIndustry *owner);
void openttd_rust_industry_accepted_shrink_to_fit(OpenTTDIndustry *owner);
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
/** Map facts constant for one entry; landscape is the original enum value. */
typedef struct OpenTTDIndustryMap {
	uint32_t size_x, size_y;
	uint8_t landscape;
} OpenTTDIndustryMap;
/** Per-tick record read once by the facade: tick counter, map, inverse-scaled callback interval, ambient sound and editor mode. */
typedef struct OpenTTDIndustryTickRecord {
	uint64_t counter;
	OpenTTDIndustryMap map;
	uint32_t interval;
	uint8_t ambient, editor;
} OpenTTDIndustryTickRecord;
/** Next pool industry from an index; industry is null at the end. */
typedef struct OpenTTDIndustryEntry {
	void *industry;
	OpenTTDIndustry *owner;
	uint16_t id, callbacks;
} OpenTTDIndustryEntry;
typedef struct OpenTTDIndustryLocation {
	uint32_t tile;
	uint16_t width, height;
} OpenTTDIndustryLocation;
/** Snow and ground for clear tiles; ground and grown (at least TreeGrowthStage::Grown) for tree tiles; no height. */
typedef struct OpenTTDIndustryFarmTile {
	uint8_t type, snow, ground, grown;
} OpenTTDIndustryFarmTile;
typedef struct OpenTTDIndustryChange {
	uint16_t result;
	int32_t reg;
} OpenTTDIndustryChange;
typedef struct OpenTTDIndustryTypeInfo {
	uint32_t behaviour;
	bool enabled, layouts;
	uint8_t appear;
} OpenTTDIndustryTypeInfo;
#ifdef __cplusplus
#	define OPENTTD_INDUSTRY_NOEXCEPT noexcept
#else
#	define OPENTTD_INDUSTRY_NOEXCEPT
#endif
/** Direct typed services, one per original operation. No Rust borrow survives
 * any call; construction, clearing, distribution and destruction may reenter. */
typedef struct OpenTTDIndustryServices {
	/* Periodic tick. */
	OpenTTDIndustryEntry (*next_tick)(uint32_t from) OPENTTD_INDUSTRY_NOEXCEPT;
	uint16_t (*sound_count)(void *industry) OPENTTD_INDUSTRY_NOEXCEPT;
	uint32_t (*behaviour)(void *industry) OPENTTD_INDUSTRY_NOEXCEPT;
	OpenTTDIndustryLocation (*location)(void *industry) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*industry_sound)(void *industry, uint32_t index) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*play_sound)(uint16_t sound, uint32_t tile) OPENTTD_INDUSTRY_NOEXCEPT;
	uint16_t (*special_effect)(void *industry, uint32_t random, uint32_t parameter) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*tick_trigger)(void *industry) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*production_callback)(void *industry, uint8_t reason) OPENTTD_INDUSTRY_NOEXCEPT;
	uint32_t (*scale_cargo)(uint32_t amount) OPENTTD_INDUSTRY_NOEXCEPT;
	uint32_t (*random)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	uint32_t (*random_range)(uint32_t limit) OPENTTD_INDUSTRY_NOEXCEPT;
	uint32_t (*move_goods)(void *industry, uint32_t slot, uint32_t amount) OPENTTD_INDUSTRY_NOEXCEPT;
	/* Farm fields and lumber mills. */
	uint32_t (*tile_add_wrap)(uint32_t tile, int32_t x, int32_t y) OPENTTD_INDUSTRY_NOEXCEPT;
	OpenTTDIndustryFarmTile (*farm_tile)(uint32_t tile) OPENTTD_INDUSTRY_NOEXCEPT;
	int32_t (*tile_z)(uint32_t tile) OPENTTD_INDUSTRY_NOEXCEPT;
	uint8_t (*snow_line)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*make_field)(uint32_t tile, uint8_t field_type, uint8_t counter, uint16_t industry) OPENTTD_INDUSTRY_NOEXCEPT;
	bool (*fence_wanted)(uint32_t tile, uint8_t side) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*set_fence)(uint32_t tile, uint8_t side, uint8_t type) OPENTTD_INDUSTRY_NOEXCEPT;
	bool (*tile_completed)(void *industry, uint32_t tile) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*harvest)(uint32_t tile) OPENTTD_INDUSTRY_NOEXCEPT;
	/* Monthly, daily, builder and command entries. */
	void (*observe)(void *industry, OpenTTDIndustryObservation *out) OPENTTD_INDUSTRY_NOEXCEPT;
	uint8_t (*production_rate)(void *industry, uint32_t slot) OPENTTD_INDUSTRY_NOEXCEPT;
	OpenTTDIndustryChange (*change_callback)(void *industry, bool monthly, uint32_t random) OPENTTD_INDUSTRY_NOEXCEPT;
	uint32_t (*custom_text)(void *industry, uint16_t text) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*news)(void *industry, uint32_t text, bool close) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*rate_news)(void *industry, uint8_t cargo, int32_t percent) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*callback_error)(void *industry, bool invalid_cargo) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*set_dirty)(void *industry) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*destroy)(void *industry) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*advertise)(void *industry) OPENTTD_INDUSTRY_NOEXCEPT;
	void *(*create)(uint8_t type, uint8_t creation) OPENTTD_INDUSTRY_NOEXCEPT;
	void *(*random_industry)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	void *(*get)(uint16_t id) OPENTTD_INDUSTRY_NOEXCEPT;
	uint16_t (*type_count)(uint8_t type) OPENTTD_INDUSTRY_NOEXCEPT;
	uint32_t (*total)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	OpenTTDIndustryTypeInfo (*type_info)(uint8_t type) OPENTTD_INDUSTRY_NOEXCEPT;
	uint32_t (*probability_callback)(uint8_t type, uint8_t creation, uint32_t chance) OPENTTD_INDUSTRY_NOEXCEPT;
	uint32_t (*scale_by_map_size)(uint32_t n) OPENTTD_INDUSTRY_NOEXCEPT;
	uint8_t (*company_none)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*restore_company)(uint8_t company) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*directory_dirty)(uint8_t) OPENTTD_INDUSTRY_NOEXCEPT;
	bool (*recession)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	uint8_t (*economy_month)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	int32_t (*economy_year)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	uint32_t (*days_since_last_month)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	uint8_t (*landscape)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	uint8_t (*economy_type)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	uint8_t (*passengers)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	bool (*fund_only)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	int32_t (*calendar_year)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	bool (*deity)(void) OPENTTD_INDUSTRY_NOEXCEPT;
} OpenTTDIndustryServices;

/** Minimal destruction map facts, queried at the original observation point. */
typedef struct OpenTTDIndustryCleanupTile { uint16_t industry; uint8_t type; bool oilrig; } OpenTTDIndustryCleanupTile;
/** NewGRF resolver output. Sprite groups/resolvers remain native shared services. */
typedef struct OpenTTDIndustryReseed { uint32_t self, parent; uint8_t used; } OpenTTDIndustryReseed;
typedef struct OpenTTDIndustryRandomisationSpec { uint16_t gfx; bool enabled; } OpenTTDIndustryRandomisationSpec;
typedef struct OpenTTDIndustryAnimationSpec { const void *spec; uint8_t triggers; } OpenTTDIndustryAnimationSpec;
typedef struct OpenTTDIndustryLifecycleServices {
	bool (*neutral_station)(void *) OPENTTD_INDUSTRY_NOEXCEPT;
	OpenTTDIndustryCleanupTile (*cleanup_tile)(uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*delete_tile_inspect)(uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*make_water)(uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*delete_oilrig)(uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*clear_docking)(uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	bool (*industry_field)(uint32_t, uint16_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*disassociate_field)(uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*release_disasters)(uint16_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*delete_storage)(void *) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*delete_news)(uint16_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*close_view)(uint16_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*close_production)(uint16_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*delete_inspect)(uint16_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*delete_subsidies)(uint16_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*invalidate_cargo)(uint16_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void *(*next_near_station)(void *, void *) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*remove_delivery)(void *, void *) OPENTTD_INDUSTRY_NOEXCEPT;
	bool (*belongs)(uint32_t, uint16_t) OPENTTD_INDUSTRY_NOEXCEPT;
	OpenTTDIndustryRandomisationSpec (*randomisation_spec)(uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	uint8_t (*random_triggers)(uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*set_random_triggers)(uint32_t, uint8_t) OPENTTD_INDUSTRY_NOEXCEPT;
	OpenTTDIndustryReseed (*resolve_reseed)(uint32_t, uint16_t, void *, uint8_t) OPENTTD_INDUSTRY_NOEXCEPT;
	uint8_t (*random_bits)(uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*set_random_bits)(uint32_t, uint8_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*mark_dirty)(uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	OpenTTDIndustryAnimationSpec (*animation_spec)(uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*change_frame)(const void *, uint32_t, uint32_t, uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
} OpenTTDIndustryLifecycleServices;
void openttd_rust_industry_cleanup(void *, OpenTTDIndustry *, OpenTTDIndustryBuilder *, uint16_t, bool, OpenTTDIndustryMap, const OpenTTDIndustryServices *, const OpenTTDIndustryLifecycleServices *);
void openttd_rust_industry_randomise_tile(uint32_t, uint8_t, void *, OpenTTDIndustry *, const OpenTTDIndustryServices *, const OpenTTDIndustryLifecycleServices *);
void openttd_rust_industry_randomise(void *, OpenTTDIndustry *, uint16_t, uint8_t, uint32_t, const OpenTTDIndustryServices *, const OpenTTDIndustryLifecycleServices *);
bool openttd_rust_industry_animate_tile(uint32_t, uint8_t, uint32_t, const OpenTTDIndustryServices *, const OpenTTDIndustryLifecycleServices *);
bool openttd_rust_industry_animate(OpenTTDIndustry *, uint16_t, uint8_t, uint32_t, const OpenTTDIndustryServices *, const OpenTTDIndustryLifecycleServices *);
typedef struct OpenTTDIndustrySpecHandle OpenTTDIndustrySpecHandle;
typedef struct OpenTTDIndustryGenerationContext OpenTTDIndustryGenerationContext;
typedef struct OpenTTDIndustryGenerationInfo {
	uint8_t appear;
	bool enabled, layouts;
} OpenTTDIndustryGenerationInfo;
typedef struct OpenTTDIndustryGenerationServices {
	const OpenTTDIndustrySpecHandle *(*spec)(uint8_t) OPENTTD_INDUSTRY_NOEXCEPT;
	uint32_t (*spec_behaviour)(const OpenTTDIndustrySpecHandle *) OPENTTD_INDUSTRY_NOEXCEPT;
	OpenTTDIndustryGenerationInfo (*info)(const OpenTTDIndustrySpecHandle *) OPENTTD_INDUSTRY_NOEXCEPT;
	bool (*coastal)(const OpenTTDIndustrySpecHandle *) OPENTTD_INDUSTRY_NOEXCEPT;
	bool (*editor)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	uint8_t (*density)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	uint32_t (*custom_number)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	uint32_t (*scale_by_size_1d)(uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	uint32_t (*scale_by_land_proportion)(uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	bool (*set_progress)(OpenTTDIndustryGenerationContext *, bool, uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	bool (*increase_progress)(OpenTTDIndustryGenerationContext *, bool) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*missing_warning)(const OpenTTDIndustrySpecHandle *) OPENTTD_INDUSTRY_NOEXCEPT;
} OpenTTDIndustryGenerationServices;
bool openttd_rust_industry_generate(OpenTTDIndustryBuilder *, OpenTTDIndustryGenerationContext *, const OpenTTDIndustryServices *, const OpenTTDIndustryGenerationServices *);
void openttd_rust_industry_check(const OpenTTDIndustryServices *, const OpenTTDIndustryGenerationServices *);
typedef struct OpenTTDTownHandle OpenTTDTownHandle;
typedef struct OpenTTDIndustryLayoutHandle OpenTTDIndustryLayoutHandle;
typedef struct OpenTTDIndustryStationHandle OpenTTDIndustryStationHandle;
typedef struct OpenTTDIndustryCargoLists { uint8_t produced[16], accepted[16]; } OpenTTDIndustryCargoLists;
typedef struct OpenTTDIndustryLayoutTile { int16_t x, y; uint16_t gfx; } OpenTTDIndustryLayoutTile;
typedef struct OpenTTDIndustryConstructionRequest {
	const OpenTTDIndustryLayoutHandle *layout;
	const OpenTTDTownHandle *town;
	size_t layout_index;
	uint32_t tile, size_x;
	uint16_t id, random_bits;
	uint8_t type, founder;
} OpenTTDIndustryConstructionRequest;
typedef bool (*OpenTTDIndustryStationVisit)(void *, OpenTTDIndustryStationHandle *, uint32_t);
typedef struct OpenTTDIndustryConstructionServices {
	OpenTTDIndustryCargoLists (*cargo_lists)(const OpenTTDIndustrySpecHandle *) OPENTTD_INDUSTRY_NOEXCEPT;
	uint16_t (*callback_mask)(const OpenTTDIndustrySpecHandle *) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*set_town)(void *, const OpenTTDTownHandle *) OPENTTD_INDUSTRY_NOEXCEPT;
	int32_t (*calendar_date)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	bool (*generating)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	uint16_t (*callback)(void *, uint16_t, uint32_t, uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*callback_error)(const OpenTTDIndustrySpecHandle *, uint16_t, uint16_t) OPENTTD_INDUSTRY_NOEXCEPT;
	uint8_t (*grf_version)(const OpenTTDIndustrySpecHandle *) OPENTTD_INDUSTRY_NOEXCEPT;
	uint8_t (*cargo_translation)(const OpenTTDIndustrySpecHandle *, uint8_t) OPENTTD_INDUSTRY_NOEXCEPT;
	size_t (*layout_size)(const OpenTTDIndustryLayoutHandle *) OPENTTD_INDUSTRY_NOEXCEPT;
	OpenTTDIndustryLayoutTile (*layout_tile)(const OpenTTDIndustryLayoutHandle *, size_t) OPENTTD_INDUSTRY_NOEXCEPT;
	uint8_t (*water_class)(uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*clear_tile)(uint32_t, uint16_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*make_industry)(uint32_t, uint16_t, uint16_t, uint32_t, uint8_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*set_construction_counter)(uint32_t, uint8_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*set_construction_stage)(uint32_t, uint8_t) OPENTTD_INDUSTRY_NOEXCEPT;
	uint16_t (*translated_gfx)(uint16_t) OPENTTD_INDUSTRY_NOEXCEPT;
	bool (*animation_enabled)(uint16_t) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*add_animated)(uint32_t, bool) OPENTTD_INDUSTRY_NOEXCEPT;
	OpenTTDIndustryMap (*map)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*build_window_dirty)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	OpenTTDIndustryStationHandle *(*neutral_station)(void *) OPENTTD_INDUSTRY_NOEXCEPT;
	bool (*serve_neutral)(void) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*insert_near_station)(void *, OpenTTDIndustryStationHandle *) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*clear_near_industries)(OpenTTDIndustryStationHandle *) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*insert_neutral_industry)(OpenTTDIndustryStationHandle *, void *) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*stations_around)(void *, void *, OpenTTDIndustryStationVisit) OPENTTD_INDUSTRY_NOEXCEPT;
	void (*add_delivery)(OpenTTDIndustryStationHandle *, void *, uint32_t) OPENTTD_INDUSTRY_NOEXCEPT;
} OpenTTDIndustryConstructionServices;
void openttd_rust_industry_initialize(void *, OpenTTDIndustry *, OpenTTDIndustryBuilder *, const OpenTTDIndustryConstructionRequest *, const OpenTTDIndustryServices *, const OpenTTDIndustryGenerationServices *, const OpenTTDIndustryLifecycleServices *, const OpenTTDIndustryConstructionServices *);
void openttd_rust_industry_tick(OpenTTDIndustryBuilder *builder, const OpenTTDIndustryTickRecord *record, const OpenTTDIndustryServices *services);
bool openttd_rust_industry_transport(void *industry, OpenTTDIndustry *owner, uint8_t minimal_cargo, bool recession, const OpenTTDIndustryServices *services);
void openttd_rust_industry_farm(void *industry, uint16_t id, OpenTTDIndustryMap map, const OpenTTDIndustryServices *services);
void openttd_rust_industry_recompute(void *industry, OpenTTDIndustry *owner, const OpenTTDIndustryServices *services);
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
void openttd_rust_industry_production_callback(void *industry, OpenTTDIndustry *owner, void *context, uint32_t behaviour, uint8_t reason, OpenTTDIndustryResolve resolve, const OpenTTDIndustryServices *services);
#ifdef __cplusplus
}
#endif
#endif
