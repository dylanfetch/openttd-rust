/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file engine_adapter.hpp Native pool/configuration/display services only. */
#include "engine_ffi.h"
#include "vehicle_services.h"
#include "company_services.h"
#include "settings_services.h"
static_assert(MAX_COMPANIES == 15 && OWNER_DEITY.base() == 18 && MAX_LENGTH_ENGINE_NAME_CHARS == 32);
static OpenTTDEngineRef EngineRef(const Engine *e) noexcept
{
	if (e == nullptr) return {.shell = nullptr, .state = nullptr, .id = EngineID::Invalid().base(), .type = VEH_INVALID};
	return {.shell = RustEngine(e), .state = e->rust_lifecycle.state, .id = e->index.base(), .type = e->type};
}
static OpenTTDEngineRef Engine_next(uint32_t from) noexcept
{
	for (Engine *e : Engine::Iterate(from)) return EngineRef(e);
	return EngineRef(nullptr);
}
static OpenTTDEngineRef Engine_resolve(uint16_t id) noexcept { return EngineRef(Engine::GetIfValid(id)); }
static OpenTTDEngineRef EnginePrototype(const Engine *e) noexcept
{
	/* Unpooled prototypes may not initialize PoolItem::index. Read that field
	 * only at the original startup-seed/retirement/new-month branches. */
	return {.shell = RustEngine(e), .state = e->rust_lifecycle.state, .id = 0, .type = e->type};
}
static OpenTTDEngineSyncVariant Engine_sync_variant(const OpenTTDEngine *e) noexcept
{
	const auto &info = EngineOf(e)->info;
	return {.parent = info.variant_id.base(), .sync = info.extra_flags.Test(ExtraEngineFlag::SyncReliability)};
}
static OpenTTDEngineDisplayVariant Engine_display_variant(const OpenTTDEngine *shell) noexcept
{
	const Engine *e = EngineOf(shell);
	return {.parent = e->info.variant_id.base(), .display = e->display_last_variant.base()};
}
static OpenTTDEnginePreviewVariant Engine_preview_variant(const OpenTTDEngine *e) noexcept
{
	const auto &info = EngineOf(e)->info;
	return {.parent = info.variant_id.base(), .join = info.extra_flags.Test(ExtraEngineFlag::JoinPreview)};
}
static uint8_t Engine_extra_flags(const OpenTTDEngine *e) noexcept { return EngineOf(e)->info.extra_flags.base(); }
static uint16_t Engine_engine_id(const OpenTTDEngine *e) noexcept { return EngineOf(e)->index.base(); }
static OpenTTDEngineReliability Engine_reliability(const OpenTTDEngine *shell) noexcept
{
	const auto &info = EngineOf(shell)->info;
	return {.base_life = static_cast<uint8_t>(info.base_life.base()), .retire_early = info.retire_early};
}
static OpenTTDEngineStartup Engine_startup(const OpenTTDEngine *shell) noexcept
{
	const Engine *e = EngineOf(shell); const auto &info = e->info;
	return {.base_intro = info.base_intro.base(), .grfid = e->GetGRFID(), .decay_speed = info.decay_speed, .base_life = static_cast<uint8_t>(info.base_life.base()), .climate = info.climates.Test(_settings_game.game_creation.landscape)};
}
static OpenTTDEngineSeed Engine_seed_config(const OpenTTDEngine *e) noexcept
{
	const auto &info = EngineOf(e)->info;
	return {.base_intro = info.base_intro.base(), .lifelength = info.lifelength.base(), .decay_speed = info.decay_speed, .retire_early = info.retire_early};
}
static int32_t Engine_base_intro(const OpenTTDEngine *e) noexcept { return EngineOf(e)->info.base_intro.base(); }
static OpenTTDEngineYear Engine_aging_year(const OpenTTDEngine *shell) noexcept
{
	const Engine *e = EngineOf(shell);
	return {.base_intro = e->info.base_intro.base(), .lifelength = e->info.lifelength.base()};
}

static OpenTTDEngineStartSettings Engine_start_settings() noexcept { return {.generation_seed = _settings_game.game_creation.generation_seed, .starting_year = _settings_game.game_creation.starting_year.base(), .date = TimerGameCalendar::date.base()}; }

static OpenTTDEngineYMD Engine_date_to_ymd(int32_t date) noexcept
{
	auto ymd = TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::Date{date});
	return {.year = ymd.year.base(), .month = ymd.month, .day = ymd.day};
}
static int32_t Engine_ymd_to_date(int32_t year, uint8_t month, uint8_t day) noexcept { return TimerGameCalendar::ConvertYMDToDate(TimerGameCalendar::Year{year}, month, day).base(); }
static bool Engine_enabled(const OpenTTDEngine *e) noexcept { return EngineOf(e)->IsEnabled(); }

static void Engine_clear_variant(const OpenTTDEngine *e) noexcept { const_cast<Engine *>(EngineOf(e))->display_last_variant = EngineID::Invalid(); }
static void Engine_refresh_type(uint8_t type) noexcept { AddRemoveEngineFromAutoreplaceAndBuildWindows(static_cast<VehicleType>(type)); }
static void Engine_refresh_lists(bool dirty) noexcept
{
	InvalidateWindowClassesData(WC_BUILD_VEHICLE);
	if (dirty) { SetWindowClassesDirty(WC_BUILD_VEHICLE); SetWindowClassesDirty(WC_REPLACE_VEHICLE); }
}
static OpenTTDEngineCompany Engine_company_next(uint32_t from) noexcept
{
	for (const Company *c : Company::Iterate(from)) return {.shell = c, .id = c->index.base()};
	return {.shell = nullptr, .id = CompanyID::Invalid().base()};
}
static OpenTTDEngineCompanyPreview Engine_company_preview(const void *shell) noexcept
{
	const Company *c = static_cast<const Company *>(shell);
	return {.blocked = c->Finances().block_preview, .performance = c->Finances().old_economy[0].performance_history};
}
static OpenTTDEngineVehicle Engine_vehicle_next(uint32_t from) noexcept
{
	for (const Vehicle *v : Vehicle::Iterate(from)) return {.shell = RustVehicle(v), .id = v->index.base(), .owner = RustVehicleOwner(v), .type = RustVehicleVehicleType(v)};
	return {.shell = nullptr, .id = UINT32_MAX, .owner = 0, .type = VEH_INVALID};
}
static OpenTTDEngineVehicleCargo Engine_vehicle_cargo(OpenTTDVehicle *handle) noexcept
{
	const Vehicle *v = VehicleOf(handle);
	return {.engine = RustEngine(v->GetEngine()), .cargo = RustVehicleCargoType(handle)};
}
static bool Engine_can_carry(const OpenTTDEngine *e) noexcept { return EngineOf(e)->CanCarryCargo(); }
static uint64_t Engine_refit_mask(const OpenTTDEngine *e) noexcept { return GetUnionOfArticulatedRefitMasks(EngineOf(e)->index, true); }

static void Engine_date_company_types(uint8_t company, int32_t date) noexcept
{
	Company *c = Company::Get(CompanyID{company});
	c->avail_railtypes = AddDateIntroducedRailTypes(c->avail_railtypes, TimerGameCalendar::Date{date});
	c->avail_roadtypes = AddDateIntroducedRoadTypes(c->avail_roadtypes, TimerGameCalendar::Date{date});
}
); }

static uint16_t Engine_company_engine_count(uint8_t company, uint16_t engine) noexcept { return GetGroupNumEngines(CompanyID{company}, ALL_GROUP, EngineID{engine}); }
static void Engine_block_preview(uint8_t company) noexcept { Company::Get(CompanyID{company})->Finances().block_preview = 20; }



static void Engine_toolbar_release(uint8_t type) noexcept
{
	if (type == VEH_ROAD) InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_ROAD);
	if (type == VEH_SHIP) InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_WATER);
	if (type == VEH_AIRCRAFT) InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_AIR);
}
static void Engine_toolbar_enable(uint8_t type) noexcept { InvalidateWindowData(WC_MAIN_TOOLBAR, 0); Engine_toolbar_release(type); }
static void Engine_close_preview(uint16_t engine) noexcept { CloseWindowById(WC_ENGINE_PREVIEW, engine); }
static void Engine_invalidate_preview(uint16_t engine) noexcept { InvalidateWindowData(WC_ENGINE_PREVIEW, engine); }
static void Engine_offer_preview(uint16_t engine, uint8_t company, bool ai) noexcept
{
	if (ai) AI::NewEvent(CompanyID{company}, new ScriptEventEnginePreview(EngineID{engine}));
	if (IsInteractiveCompany(CompanyID{company})) ShowEnginePreviewWindow(EngineID{engine});
}
static void Engine_available_event(uint16_t engine) noexcept { AI::BroadcastNewEvent(new ScriptEventEngineAvailable(EngineID{engine})); }
static void Engine_available_news(uint16_t engine) noexcept
{
	EngineID index{engine};
	AddNewsItem(GetEncodedString(STR_NEWS_NEW_VEHICLE_NOW_AVAILABLE_WITH_TYPE, GetEngineCategoryName(index), PackEngineNameDParam(index, EngineNameContext::PreviewNews)), NewsType::NewVehicles, NewsStyle::Vehicle, {}, index);
}
static void Engine_no_engines(int32_t date) noexcept
{
	if (date < INT32_MAX) ShowErrorMessage(GetEncodedString(STR_ERROR_NO_VEHICLES_AVAILABLE_YET), GetEncodedString(STR_ERROR_NO_VEHICLES_AVAILABLE_YET_EXPLANATION, TimerGameCalendar::Date{date}), WL_WARNING);
	else ShowErrorMessage(GetEncodedString(STR_ERROR_NO_VEHICLES_AVAILABLE_AT_ALL), GetEncodedString(STR_ERROR_NO_VEHICLES_AVAILABLE_AT_ALL_EXPLANATION), WL_WARNING);
}
static void Engine_screen_dirty() noexcept { MarkWholeScreenDirty(); }
static uint32_t Engine_utf8_length(OpenTTDByteSpan name) noexcept { return Utf8StringLength(std::string_view{reinterpret_cast<const char *>(name.data), name.size}); }
static bool Engine_climate(const OpenTTDEngine *e) noexcept { return EngineOf(e)->info.climates.Test(_settings_game.game_creation.landscape); }
static uint8_t Engine_rail_kind(const OpenTTDEngine *e) noexcept { return EngineOf(e)->VehInfo<RailVehicleInfo>().railveh_type; }
static void Engine_update_railtypes(uint8_t id) noexcept { Company::Get(CompanyID{id})->avail_railtypes = GetCompanyRailTypes(CompanyID{id}); }
static void Engine_update_roadtypes(uint8_t id) noexcept { Company::Get(CompanyID{id})->avail_roadtypes = GetCompanyRoadTypes(CompanyID{id}); }
static uint64_t Engine_compatible_railtypes(const OpenTTDEngine *e) noexcept { return GetAllCompatibleRailTypes(EngineOf(e)->VehInfo<RailVehicleInfo>().railtypes).base(); }
static uint64_t Engine_powered_roadtypes(const OpenTTDEngine *e) noexcept { return GetRoadTypeInfo(EngineOf(e)->VehInfo<RoadVehicleInfo>().roadtype)->powered_roadtypes.base(); }
static uint64_t Engine_company_railtypes(uint8_t id) noexcept { return Company::Get(CompanyID{id})->avail_railtypes.base(); }
static uint64_t Engine_company_roadtypes(uint8_t id) noexcept { return Company::Get(CompanyID{id})->avail_roadtypes.base(); }
static uint64_t Engine_introduces_railtypes(const OpenTTDEngine *e) noexcept
{
	RailTypes types = EngineOf(e)->VehInfo<RailVehicleInfo>().railtypes;
	assert(types != RailTypes{});
	return GetAllIntroducesRailTypes(types).base();
}
static uint64_t Engine_introduces_roadtypes(const OpenTTDEngine *e) noexcept
{
	RoadType type = EngineOf(e)->VehInfo<RoadVehicleInfo>().roadtype;
	assert(type < ROADTYPE_END);
	return GetRoadTypeInfo(type)->introduces_roadtypes.base();
}
static void Engine_set_railtypes(uint8_t id, uint64_t types, int32_t date) noexcept { Company::Get(CompanyID{id})->avail_railtypes = AddDateIntroducedRailTypes(RailTypes{types}, TimerGameCalendar::Date{date}); }
static void Engine_set_roadtypes(uint8_t id, uint64_t types, int32_t date) noexcept { Company::Get(CompanyID{id})->avail_roadtypes = AddDateIntroducedRoadTypes(RoadTypes{types}, TimerGameCalendar::Date{date}); }
static const OpenTTDEngineServices &EngineServices() noexcept
{
	static const OpenTTDEngineServices services{
		.shared = &GetRustSharedServices(),
		.rng = &GetRustRandomServices(),
		.next = Engine_next,
		.resolve = Engine_resolve,
		.sync_variant = Engine_sync_variant,
		.display_variant = Engine_display_variant,
		.preview_variant = Engine_preview_variant,
		.extra_flags = Engine_extra_flags,
		.engine_id = Engine_engine_id,
		.reliability = Engine_reliability,
		.startup = Engine_startup,
		.seed_config = Engine_seed_config,
		.base_intro = Engine_base_intro,
		.aging_year = Engine_aging_year,
		.calendar_year = RustCalendarYear,
		.never_expire = RustNeverExpireVehicles,
		.start_settings = Engine_start_settings,
		.train_limit = RustTrainLimit,
		.road_limit = RustRoadVehicleLimit,
		.ship_limit = RustShipLimit,
		.aircraft_limit = RustAircraftLimit,
		.ai_train_disabled = RustAITrainDisabled,
		.ai_road_disabled = RustAIRoadDisabled,
		.ai_ship_disabled = RustAIShipDisabled,
		.ai_aircraft_disabled = RustAIAircraftDisabled,
		.date_to_ymd = Engine_date_to_ymd,
		.ymd_to_date = Engine_ymd_to_date,
		.enabled = Engine_enabled,
		.climate = Engine_climate,
		.rail_kind = Engine_rail_kind,
		.clear_variant = Engine_clear_variant,
		.refresh_type = Engine_refresh_type,
		.refresh_lists = Engine_refresh_lists,
		.company_next = Engine_company_next,
		.company_preview = Engine_company_preview,
		.vehicle_next = Engine_vehicle_next,
		.vehicle_cargo = Engine_vehicle_cargo,
		.can_carry = Engine_can_carry,
		.refit_mask = Engine_refit_mask,
		.update_railtypes = Engine_update_railtypes,
		.update_roadtypes = Engine_update_roadtypes,
		.date_company_types = Engine_date_company_types,
		.company_valid = RustCompanyValid,
		.compatible_railtypes = Engine_compatible_railtypes,
		.powered_roadtypes = Engine_powered_roadtypes,
		.company_railtypes = Engine_company_railtypes,
		.company_roadtypes = Engine_company_roadtypes,
		.company_engine_count = Engine_company_engine_count,
		.block_preview = Engine_block_preview,
		.introduces_railtypes = Engine_introduces_railtypes,
		.introduces_roadtypes = Engine_introduces_roadtypes,
		.set_railtypes = Engine_set_railtypes,
		.set_roadtypes = Engine_set_roadtypes,
		.current_company = RustCurrentCompanyID,
		.local_company = RustLocalCompanyID,
		.toolbar_enable = Engine_toolbar_enable,
		.toolbar_release = Engine_toolbar_release,
		.close_preview = Engine_close_preview,
		.invalidate_preview = Engine_invalidate_preview,
		.offer_preview = Engine_offer_preview,
		.available_event = Engine_available_event,
		.available_news = Engine_available_news,
		.no_engines = Engine_no_engines,
		.screen_dirty = Engine_screen_dirty,
		.utf8_length = Engine_utf8_length,
	};
	return services;
}
