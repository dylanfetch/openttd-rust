/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */
/** @file company_adapter.cpp Thin shared-world and genuine reentry services. */
#include "../stdafx.h"
#ifdef WITH_RUST
#include "company_ffi.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../company_cmd.h"
#include "../company_gui.h"
#include "../economy_func.h"
#include "../economy_base.h"
#include "../economy_cmd.h"
#include "../command_func.h"
#include "../vehicle_base.h"
#include "../vehicle_func.h"
#include "../vehicle_cmd.h"
#include "../aircraft.h"
#include "../station_base.h"
#include "../waypoint_base.h"
#include "../subsidy_base.h"
#include "../subsidy_func.h"
#include "../town.h"
#include "../signs_base.h"
#include "../goal_base.h"
#include "../story_base.h"
#include "../landscape.h"
#include "../rail.h"
#include "../road.h"
#include "../signal_func.h"
#include "../cargomonitor.h"
#include "../autoreplace_func.h"
#include "../newgrf.h"
#include "../network/network.h"
#include "../network/network_func.h"
#include "../network/network_admin.h"
#include "../ai/ai.hpp"
#include "../game/game.hpp"
#include "../script/api/script_object.hpp"
#include "../script/api/script_event_types.hpp"
#include "../news_func.h"
#include "../strings_func.h"
#include "../sound_func.h"
#include "../texteff.hpp"
#include "../timer/timer_game_tick.h"
#include "../timer/timer.h"
#include "../window_func.h"
#include "../object.h"
#include "../water.h"
#include "../core/backup_type.hpp"
#include "../table/strings.h"
#include <memory>
#include "../safeguards.h"

extern void CompanyAdminRemove(CompanyID, CompanyRemoveReason);
extern const PriceBaseSpec _price_base_specs[];
extern TimeoutTimer<TimerGameTick> _new_competitor_timeout;
extern int64_t RustCompanyIdentity(uint32_t op, uint32_t id, int64_t a, int64_t b);

/** Fetch the next occupied pool slot; traversal policy lives in Rust. */
template <typename T> static uint32_t CompanyNextPool(uint32_t from) noexcept
{
	for (const auto *item : T::Iterate(from)) return item->index.base();
	return UINT32_MAX;
}
static uint32_t OPENTTD_COMPANY_CALL CompanyNext(uint32_t kind, uint32_t from) noexcept
{
	switch (kind) {
		case 1: return CompanyNextPool<Company>(from);
		case 2: return CompanyNextPool<Vehicle>(from);
		case 3: return CompanyNextPool<Station>(from);
		case 4: return CompanyNextPool<Subsidy>(from);
		case 5: return CompanyNextPool<Town>(from);
		case 6: return CompanyNextPool<Group>(from);
		case 7: return CompanyNextPool<Waypoint>(from);
		case 8: return CompanyNextPool<Sign>(from);
		case 9: return CompanyNextPool<Goal>(from);
		case 10: return CompanyNextPool<StoryPage>(from);
		case 14: for (const CargoSpec *cs : CargoSpec::Iterate()) if (cs->Index() >= from) return cs->Index(); return UINT32_MAX;
		default: NOT_REACHED();
	}
}
static void *OPENTTD_COMPANY_CALL CompanyOwner(uint32_t id) noexcept
{
	Company *c = Company::GetIfValid(id);
	return c == nullptr ? nullptr : &c->Finances();
}
/** Copied observations, without retaining a world or owner reference. */
static void OPENTTD_COMPANY_CALL CompanyRead(uint32_t kind, uint32_t id, int64_t a, int64_t *v) noexcept
{
	std::fill_n(v, 32, 0);
	switch (kind) {
		case 0:
			if (a == 1) {
				v[0] = _settings_game.construction.terraform_per_64k_frames; v[1] = _settings_game.construction.terraform_frame_burst;
				v[2] = _settings_game.construction.clear_per_64k_frames; v[3] = _settings_game.construction.clear_frame_burst;
				v[4] = _settings_game.construction.tree_per_64k_frames; v[5] = _settings_game.construction.tree_frame_burst;
				v[6] = _settings_game.construction.build_object_per_64k_frames; v[7] = _settings_game.construction.build_object_frame_burst;
			} else if (a == 2) {
				v[0] = _settings_game.vehicle.max_trains; v[1] = _settings_game.vehicle.max_roadveh;
				v[2] = _settings_game.vehicle.max_ships; v[3] = _settings_game.vehicle.max_aircraft;
			} else if (a == 3) {
				v[0] = _settings_client.gui.show_finances; v[1] = _settings_client.sound.new_year;
			} else {
				v[0] = _game_mode == GM_EDITOR; v[1] = _game_mode == GM_MENU; v[2] = _networking; v[3] = _network_server;
				v[4] = _local_company.base(); v[5] = _current_company.base(); v[6] = _settings_game.difficulty.infinite_money;
				v[7] = TimerGameEconomy::month; v[8] = TimerGameCalendar::year.base(); v[9] = _settings_game.economy.inflation;
				v[10] = _settings_game.economy.infrastructure_maintenance; v[11] = _settings_game.economy.give_money;
				v[12] = _settings_game.difficulty.competitors_interval; v[13] = _settings_game.difficulty.max_no_competitors;
				v[14] = _settings_client.network.max_companies; v[15] = static_cast<int64_t>(Company::GetNumItems());
				v[16] = AI::CanStartNew(); v[17] = _new_competitor_timeout.HasFired(); v[18] = Ticks::DAY_TICKS;
				v[19] = Ticks::TICKS_PER_SECOND; v[20] = TimerGameEconomy::year.base();
				v[21] = CalendarTime::ORIGINAL_BASE_YEAR.base(); v[22] = CalendarTime::ORIGINAL_MAX_YEAR.base();
				v[23] = RAILTYPE_END; v[24] = ROADTYPE_END; v[25] = PR_END; v[26] = VEHICLE_PROFIT_MIN_AGE.base();
				v[27] = _settings_game.difficulty.max_loan; v[28] = _settings_game.difficulty.vehicle_costs;
				v[29] = _settings_game.difficulty.construction_cost; v[30] = _settings_game.difficulty.initial_interest;
				v[31] = Company::CanAllocateItem();
			}
			break;
		case 1: {
			const Company *c = Company::GetIfValid(id); if (c == nullptr) break;
			v[0] = 1; v[1] = c->is_ai; v[2] = c->name_1; v[3] = c->location_of_HQ.base();
			for (size_t i = 0; i < VEH_COMPANY_END; i++) v[4 + i] = c->group_all[i].num_vehicle;
			break;
		}
		case 2: {
			const Vehicle *x = Vehicle::Get(id);
			v[0] = x->owner.base(); v[1] = x->type; v[2] = x->IsPrimaryVehicle(); v[3] = x->IsEngineCountable();
			v[4] = x->Previous() == nullptr; v[5] = x->type == VEH_AIRCRAFT && Aircraft::From(x)->IsNormalAircraft();
			v[6] = x->value; v[7] = x->profit_last_year; v[8] = x->economy_age.base();
			v[9] = x->ServiceIntervalIsCustom(); v[10] = x->group_id.base();
			break;
		}
		case 3: { const Station *x = Station::Get(id); v[0] = x->owner.base(); v[1] = x->facilities.Count(); v[2] = x->time_since_load; v[3] = x->time_since_unload; break; }
		case 4: v[0] = Subsidy::Get(id)->awarded.base(); break;
		case 5: { const Town *x = Town::Get(id); CompanyID owner(static_cast<uint8_t>(a)); v[0] = x->ratings[owner]; v[1] = x->have_ratings.base(); v[2] = x->exclusive_counter; v[3] = x->exclusivity.base(); break; }
		case 6: v[0] = Group::Get(id)->owner.base(); break;
		case 7: v[0] = Waypoint::Get(id)->owner.base(); break;
		case 8: v[0] = Sign::Get(id)->owner.base(); break;
		case 9: v[0] = Goal::Get(id)->company.base(); break;
		case 10: v[0] = StoryPage::Get(id)->company.base(); break;
		case 11: {
			TileIndex tile(id); v[0] = Map::Size(); v[1] = IsTileType(tile, MP_RAILWAY); v[4] = IsLevelCrossingTile(tile);
			/* Original short-circuit predicates never read house/industry owners. */
			if (v[1] != 0 || v[4] != 0) v[2] = GetTileOwner(tile).base();
			v[3] = v[1] != 0 && HasSignals(tile);
			if (v[3] != 0) { v[5] = GetTrackBits(tile); if (a >= 0) v[6] = HasSignalOnTrack(tile, static_cast<Track>(a)); }
			break;
		}
		case 12: {
			const Company *c = Company::Get(id);
			if (a >= 0 && a < RAILTYPE_END) v[0] = c->infrastructure.rail[static_cast<size_t>(a)];
			else if (a >= 100 && a < 100 + ROADTYPE_END) { RoadType rt = static_cast<RoadType>(a - 100); v[0] = c->infrastructure.road[rt]; v[1] = RoadTypeIsRoad(rt); }
			v[2] = c->infrastructure.GetRailTotal(); v[3] = c->infrastructure.GetRoadTotal(); v[4] = c->infrastructure.GetTramTotal();
			v[5] = c->infrastructure.signal; v[6] = c->infrastructure.water; v[7] = c->infrastructure.station; v[8] = c->infrastructure.airport;
			break;
		}
		case 13: v[0] = _price_base_specs[id].start_price; v[1] = _price_base_specs[id].category; break;
		case 14: v[0] = CargoSpec::Get(static_cast<CargoType>(id))->initial_payment; break;
		default: NOT_REACHED();
	}
}
/** Nonthrowing scalar/world primitives; selected policy and traversal are Rust. */
static int64_t OPENTTD_COMPANY_CALL CompanyService(const OpenTTDCompanyAction *x) noexcept
{
	CompanyID id(static_cast<uint8_t>(x->id));
	switch (x->kind) {
		case RustCompanyLeafSharedRandom: return Random();
		case RustCompanyLeafSetCurrentCompany: _current_company = id; break;
		case RustCompanyLeafInvalidateCompanyWindows: InvalidateCompanyWindows(Company::Get(id)); break;
		case RustCompanyLeafPerformanceDirty: SetWindowDirty(WC_PERFORMANCE_DETAIL, 0); break;
		case RustCompanyLeafUpdateHeadquarters: UpdateCompanyHQ(TileIndex(static_cast<uint32_t>(x->a)), static_cast<int>(x->b)); break;
		case RustCompanyLeafAdminUpdate: CompanyAdminUpdate(Company::Get(id)); break;
		case RustCompanyLeafTroubleNews: {
			auto cni = std::make_unique<CompanyNewsInformation>(STR_NEWS_COMPANY_IN_TROUBLE_TITLE, Company::Get(id));
			EncodedString headline = GetEncodedString(STR_NEWS_COMPANY_IN_TROUBLE_DESCRIPTION, cni->company_name);
			AddCompanyNewsItem(std::move(headline), std::move(cni));
			AI::BroadcastNewEvent(new ScriptEventCompanyInTrouble(id)); Game::NewEvent(new ScriptEventCompanyInTrouble(id));
			break;
		}
		case RustCompanyLeafFinancialGraphsDirty:
			SetWindowDirty(WC_INCOME_GRAPH, 0); SetWindowDirty(WC_OPERATING_PROFIT, 0); SetWindowDirty(WC_DELIVERED_CARGO, 0);
			SetWindowDirty(WC_PERFORMANCE_HISTORY, 0); SetWindowDirty(WC_COMPANY_VALUE, 0); SetWindowDirty(WC_COMPANY_LEAGUE, 0); break;
		case RustCompanyLeafRailMaintenance: return RailMaintenanceCost(static_cast<RailType>(x->id), static_cast<uint32_t>(x->a), static_cast<uint32_t>(x->b));
		case RustCompanyLeafSignalMaintenance: return SignalMaintenanceCost(static_cast<uint32_t>(x->a));
		case RustCompanyLeafRoadMaintenance: return RoadMaintenanceCost(static_cast<RoadType>(x->id), static_cast<uint32_t>(x->a), static_cast<uint32_t>(x->b));
		case RustCompanyLeafCanalMaintenance: return CanalMaintenanceCost(static_cast<uint32_t>(x->a));
		case RustCompanyLeafStationMaintenance: return StationMaintenanceCost(static_cast<uint32_t>(x->a));
		case RustCompanyLeafAirportMaintenance: return AirportMaintenanceCost(id);
		case RustCompanyLeafRecessionNews: AddNewsItem(GetEncodedString(x->a != 0 ? STR_NEWS_BEGIN_OF_RECESSION : STR_NEWS_END_OF_RECESSION), NewsType::Economy, NewsStyle::Normal, {}); break;
		case RustCompanyLeafPriceWindowsDirty:
			SetWindowClassesDirty(WC_BUILD_VEHICLE); SetWindowClassesDirty(WC_REPLACE_VEHICLE); SetWindowClassesDirty(WC_VEHICLE_DETAILS);
			SetWindowClassesDirty(WC_COMPANY_INFRASTRUCTURE); InvalidateWindowData(WC_PAYMENT_RATES, 0); break;
		case RustCompanyLeafSetCargoPayment: CargoSpec::Get(static_cast<CargoType>(x->id))->current_payment = x->a; break;
		case RustCompanyLeafIndustryStartup: StartupIndustryDailyChanges(x->a != 0); break;
		case RustCompanyLeafClearMonitors:
			if (x->a != 0) { ClearCargoPickupMonitoring(id); ClearCargoDeliveryMonitoring(id); }
			else { ClearCargoPickupMonitoring(); ClearCargoDeliveryMonitoring(); } break;
		case RustCompanyLeafResetCompetitorTimer: _new_competitor_timeout.Reset({TimerGameTick::Priority::COMPETITOR_TIMEOUT, static_cast<uint>(x->a)}); break;
		case RustCompanyLeafAbortCompetitorTimer: _new_competitor_timeout.Abort(); break;
		case RustCompanyLeafCompanyIdentity: return RustCompanyIdentity(static_cast<uint32_t>(x->a), x->id, x->b, x->c);
		case RustCompanyLeafMergerNews: {
			Company *c = Company::Get(id);
			auto cni = std::make_unique<CompanyNewsInformation>(STR_NEWS_COMPANY_MERGER_TITLE, c, Company::Get(_current_company));
			EncodedString headline = x->a != 0 ? GetEncodedString(STR_NEWS_MERGER_TAKEOVER_TITLE, cni->company_name, cni->other_company_name) : GetEncodedString(STR_NEWS_COMPANY_MERGER_DESCRIPTION, cni->company_name, cni->other_company_name, c->Finances().bankrupt_value);
			AddCompanyNewsItem(std::move(headline), std::move(cni));
			AI::BroadcastNewEvent(new ScriptEventCompanyMerger(id, _current_company)); Game::NewEvent(new ScriptEventCompanyMerger(id, _current_company)); break;
		}
		case RustCompanyLeafAcquisitionWindows:
			CloseCompanyWindows(id); InvalidateWindowClassesData(WC_TRAINS_LIST, 0); InvalidateWindowClassesData(WC_SHIPS_LIST, 0);
			InvalidateWindowClassesData(WC_ROADVEH_LIST, 0); InvalidateWindowClassesData(WC_AIRCRAFT_LIST, 0); InvalidateWindowData(WC_CLIENT_LIST, 0); break;
		case RustCompanyLeafBankruptEvents:
			AI::BroadcastNewEvent(new ScriptEventCompanyBankrupt(id)); Game::NewEvent(new ScriptEventCompanyBankrupt(id));
			CompanyAdminRemove(id, static_cast<CompanyRemoveReason>(x->a));
			if (StoryPage::GetNumItems() == 0 || Goal::GetNumItems() == 0) InvalidateWindowData(WC_MAIN_TOOLBAR, 0);
			InvalidateWindowData(WC_CLIENT_LIST, 0); break;
		case RustCompanyLeafCompanyControlWindows:
			if (x->a == 0) InvalidateWindowData(WC_COMPANY_LEAGUE, 0, 0);
			else { InvalidateWindowClassesData(WC_GAME_OPTIONS); InvalidateWindowClassesData(WC_SCRIPT_SETTINGS); InvalidateWindowClassesData(WC_SCRIPT_LIST); } break;
		case RustCompanyLeafNetworkCompanyNew: return RustCompanyIdentity(7, x->id, x->a, 0);
		case RustCompanyLeafCloseNetworkProgress: CloseWindowById(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_JOIN); break;
		case RustCompanyLeafNetworkCreationFailed: return RustCompanyIdentity(8, x->id, x->a, 0);
		case RustCompanyLeafNetworkClientCreated: return RustCompanyIdentity(9, x->id, x->a, 0);
		case RustCompanyLeafCountGroupVehicle: {
			Vehicle *v = Vehicle::Get(x->id); if (x->b == 0) GroupStatistics::CountEngine(v, static_cast<int>(x->a)); else GroupStatistics::CountVehicle(v, static_cast<int>(x->a)); break;
		}
		case RustCompanyLeafClearReplacementRules: RemoveAllEngineReplacementForCompany(Company::Get(id)); break;
		case RustCompanyLeafTransferGroup: {
			Group *g = Group::Get(x->id); Company *c = Company::Get(static_cast<uint8_t>(x->a));
			g->owner = c->index; g->number = c->freegroups.UseID(c->freegroups.NextID()); break;
		}
		case RustCompanyLeafCopyServiceDefaults: {
			Company *old = Company::Get(id); const Company *next = Company::Get(static_cast<uint8_t>(x->a));
			old->settings.vehicle.servint_aircraft = next->settings.vehicle.servint_aircraft; old->settings.vehicle.servint_trains = next->settings.vehicle.servint_trains;
			old->settings.vehicle.servint_roadveh = next->settings.vehicle.servint_roadveh; old->settings.vehicle.servint_ships = next->settings.vehicle.servint_ships;
			old->settings.vehicle.servint_ispercent = next->settings.vehicle.servint_ispercent; break;
		}
		case RustCompanyLeafServiceInterval: return CompanyServiceInterval(Company::Get(id), static_cast<VehicleType>(x->a));
		case RustCompanyLeafTransferVehicleOwner: {
			Vehicle *v = Vehicle::Get(x->id); v->owner = CompanyID(static_cast<uint8_t>(x->a)); v->colourmap = PAL_NONE; v->InvalidateNewGRFCache(); break;
		}
		case RustCompanyLeafAssignUnitNumber: {
			Vehicle *v = Vehicle::Get(x->id); auto &gen = Company::Get(v->owner)->freeunits[v->type]; v->unitnumber = gen.UseID(gen.NextID()); break;
		}
		case RustCompanyLeafUpdateAutoreplace: GroupStatistics::UpdateAutoreplace(id); break;
		case RustCompanyLeafAddSignalTrack: AddTrackToSignalBuffer(TileIndex(x->id), static_cast<Track>(x->a), CompanyID(static_cast<uint8_t>(x->b))); break;
		case RustCompanyLeafUpdateCrossing: UpdateLevelCrossing(TileIndex(x->id)); break;
		case RustCompanyLeafFlushSignals: UpdateSignalsInBuffer(); break;
		case RustCompanyLeafTransferAirportCount: Company::Get(id)->infrastructure.airport += Company::Get(static_cast<uint8_t>(x->a))->infrastructure.airport; break;
		case RustCompanyLeafStationOwner: Station::Get(x->id)->owner = CompanyID(static_cast<uint8_t>(x->a)); break;
		case RustCompanyLeafTownRating: {
			Town *t = Town::Get(x->id); CompanyID owner(static_cast<uint8_t>(x->a)); t->ratings[owner] = static_cast<int16_t>(x->b);
			if (x->c != 0) t->have_ratings.Set(owner); else t->have_ratings.Reset(owner); break;
		}
		case RustCompanyLeafTownExclusivity: { Town *t = Town::Get(x->id); t->exclusivity = CompanyID(static_cast<uint8_t>(x->a)); t->exclusive_counter = static_cast<uint8_t>(x->b); break; }
		case RustCompanyLeafSubsidyOwner: Subsidy::Get(x->id)->awarded = CompanyID(static_cast<uint8_t>(x->a)); break;
		case RustCompanyLeafWaypointSignOwner: if (x->b == 7) Waypoint::Get(x->id)->owner = CompanyID(static_cast<uint8_t>(x->a)); else Sign::Get(x->id)->owner = CompanyID(static_cast<uint8_t>(x->a)); break;
		case RustCompanyLeafTransferWindowOwner: ChangeWindowOwner(id, CompanyID(static_cast<uint8_t>(x->a))); break;
		case RustCompanyLeafScreenDirty: MarkWholeScreenDirty(); break;
		case RustCompanyLeafSetLocalCompany: SetLocalCompany(id); break;
		case RustCompanyLeafClientsToSpectators: NetworkClientsToSpectators(id); break;
		case RustCompanyLeafGiveMoneyMessage: NetworkTextMessage(NETWORK_ACTION_GIVE_MONEY, GetDrawStringCompanyColour(_current_company), false, GetString(STR_COMPANY_NAME, _current_company), GetString(STR_COMPANY_NAME, id), x->a); break;
		case RustCompanyLeafMoneyAnimation: { TileIndex tile(x->id); ShowCostOrIncomeAnimation(TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE, GetTilePixelZ(tile), x->a); break; }
		case RustCompanyLeafFinancesDirty: InvalidateWindowData(WC_FINANCES, id); break;
		case RustCompanyLeafShowFinances: ShowCompanyFinances(id); break;
		case RustCompanyLeafNewYearSound: SndPlayFx(x->a != 0 ? SND_01_BAD_YEAR : SND_00_GOOD_YEAR); break;
		case RustCompanyLeafInteractiveCompany: return IsInteractiveCompany(id);
		case RustCompanyLeafShowTakeoverDialog: ShowBuyCompanyDialog(id, false); break;
		case RustCompanyLeafAskMergerEvent: AI::NewEvent(id, new ScriptEventCompanyAskMerger(CompanyID(static_cast<uint8_t>(x->a)), x->b)); break;
		case RustCompanyLeafScriptRandomNext: return ScriptObject::GetRandomizer(OWNER_NONE).Next(static_cast<uint32_t>(x->a));
		case RustCompanyLeafServicePercent: return Company::Get(id)->settings.vehicle.servint_ispercent;
		case RustCompanyLeafRebuildSubsidyCache: RebuildSubsidisedSourceAndDestinationCache(); break;
		case RustCompanyLeafBankruptNews: {
			auto cni = std::make_unique<CompanyNewsInformation>(STR_NEWS_COMPANY_BANKRUPT_TITLE, Company::Get(id));
			EncodedString headline = GetEncodedString(STR_NEWS_COMPANY_BANKRUPT_DESCRIPTION, cni->company_name);
			AddCompanyNewsItem(std::move(headline), std::move(cni)); break;
		}
		case RustCompanyLeafAssertNewAISlot: assert(id == CompanyID::Invalid() || !Company::IsValidID(id)); break;
		case RustCompanyLeafFluctuatingEconomy: return _settings_game.difficulty.economy;
		case RustCompanyLeafNewCompanyEvents: AI::BroadcastNewEvent(new ScriptEventCompanyNew(id), id); Game::NewEvent(new ScriptEventCompanyNew(id)); break;
		default: NOT_REACHED();
	}
	return 0;
}

/** Ordinary reentry runs here, after the Rust poll and all owner accesses ended. */
static int64_t CompanyReentry(const OpenTTDCompanyAction &x)
{
	CompanyID id(static_cast<uint8_t>(x.id));
	switch (x.kind) {
		case RustCompanyLeafPostCompanyControl: Command<CMD_COMPANY_CTRL>::Post(static_cast<CompanyCtrlAction>(x.a), id, static_cast<CompanyRemoveReason>(x.b), ClientID(static_cast<uint32_t>(x.c))); break;
		case RustCompanyLeafStartAI: AI::StartNew(id); break;
		case RustCompanyLeafStopAI: AI::Stop(id); break;
		case RustCompanyLeafDeleteCompany: delete Company::Get(id); break;
		case RustCompanyLeafChangeTileOwner: ChangeTileOwner(TileIndex(x.id), CompanyID(static_cast<uint8_t>(x.a)), CompanyID(static_cast<uint8_t>(x.b))); break;
		case RustCompanyLeafDeletePoolObject:
			switch (x.a) {
				case 2: delete Vehicle::Get(x.id); break;
				case 4: delete Subsidy::Get(x.id); break;
				case 6: delete Group::Get(x.id); break;
				case 9: delete Goal::Get(x.id); break;
				case 10: delete StoryPage::Get(x.id); break;
				default: NOT_REACHED();
			}
			break;
		case RustCompanyLeafChangeServiceInterval: Command<CMD_CHANGE_SERVICE_INT>::Do({DoCommandFlag::Execute, DoCommandFlag::Bankrupt}, VehicleID(x.id), static_cast<uint16_t>(x.a), false, x.b != 0); break;
		case RustCompanyLeafAllocateCompany: { Company *c = x.id == CompanyID::Invalid().base() ? new Company(STR_SV_UNNAMED, x.a != 0) : new (id) Company(STR_SV_UNNAMED, x.a != 0); return c->index.base(); }
		default: NOT_REACHED();
	}
	return 0;
}
static const OpenTTDCompanyLeaves _company_leaves = {CompanyNext, CompanyRead, CompanyOwner, CompanyService};
OpenTTDCompanyAction RunRustCompany(uint32_t op, uint32_t id, int64_t a, int64_t b, int64_t c, int64_t d)
{
	/* Construct process-lifetime C++ views before Rust accesses their storage. */
	(void)GetRustEconomy(); (void)GetRustPrices(); (void)GetRustCompanyScores();
	std::unique_ptr<Backup<CompanyID>> current;
	if (op == 26) current = std::make_unique<Backup<CompanyID>>(_current_company);
	std::unique_ptr<OpenTTDCompanyRun, decltype(&openttd_rust_company_destroy)> run(openttd_rust_company_create(op, id, a, b, c, d, &_company_leaves), openttd_rust_company_destroy);
	int64_t response = 0;
	for (;;) {
		OpenTTDCompanyAction action = openttd_rust_company_advance(run.get(), response);
		if (action.kind == 0) { if (current != nullptr) current->Trash(); return action; }
		response = CompanyReentry(action);
	}
}
CommandCost RustCompanyCost(const OpenTTDCompanyAction &x)
{
	switch (x.a) {
		case 0: return CommandCost(static_cast<ExpensesType>(x.c), Money(x.b));
		case 1: return CMD_ERROR;
		case 2: return CommandCostWithParam(STR_ERROR_MAXIMUM_PERMITTED_LOAN, Money(x.d));
		case 3: return CommandCost(STR_ERROR_LOAN_ALREADY_REPAID);
		case 4: return CommandCostWithParam(STR_ERROR_CURRENCY_REQUIRED, Money(x.d));
		case 5: return CommandCost(STR_ERROR_INSUFFICIENT_FUNDS);
		case 6: return CommandCost(STR_ERROR_TOO_MANY_VEHICLES_IN_GAME);
		default: NOT_REACHED();
	}
}
#endif /* WITH_RUST */
