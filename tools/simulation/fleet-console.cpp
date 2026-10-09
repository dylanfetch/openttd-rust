/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fleet-console.cpp Identical command inputs for unchanged-reference fleet witnesses. */
#include "stdafx.h"
#include "console_internal.h"
#include "command_func.h"
#include "company_func.h"
#include "core/backup_type.hpp"
#include "core/random_func.hpp"
#include "group_cmd.h"
#include "autoreplace_cmd.h"
#include "autoreplace_func.h"
#include "vehicle_cmd.h"
#include "vehicle_func.h"
#include "roadveh.h"
#include "train.h"
#include "train_cmd.h"
#include "order_cmd.h"
#include "depot_base.h"
#include "station_base.h"
#include "engine_func.h"
#include "rail_map.h"
#include "road_map.h"
#include "water_map.h"
#include "settings_type.h"

[[noreturn]] static void FixtureAbort(int line) { fmt::print(stderr, "FLEET setup-failure {}\n", line); std::fflush(stderr); std::abort(); }

static void Cost(const char *name, const CommandCost &cost)
{
	fmt::print(stderr, "FLEET cost {} {} {} {}\n", name, cost.Succeeded(), cost.GetCost(), cost.GetErrorMessage());
}
static void Checked(const char *name, const CommandCost &cost)
{
	Cost(name, cost);
	if (cost.Failed()) FixtureAbort(__LINE__);
}
static std::string GroupName(const Group *g)
{
#ifdef WITH_RUST
	return g->GetName();
#else
	return g->name;
#endif
}
static Money &CompanyMoney(Company *c)
{
#ifdef WITH_RUST
	return c->Finances().money;
#else
	return c->money;
#endif
}
static Vehicle *BuildFixture(VehicleType type)
{
	Vehicle *source = nullptr;
	for (Vehicle *v : Vehicle::Iterate()) if (v->type == type && v->IsPrimaryVehicle() && v->GetNumOrders() >= 2) { source = v; break; }
	if (source == nullptr) FixtureAbort(__LINE__);
	_current_company = source->owner;
	/* Declared funding input isolates actual construction/replacement policy. */
	CompanyMoney(Company::Get(_current_company)) = Money(1000000000);
	TileIndex tile = INVALID_TILE;
	if (type == VEH_AIRCRAFT) {
		for (Station *st : Station::Iterate()) if (st->owner == _current_company && st->airport.GetNumHangars() != 0) { tile = st->airport.GetHangarTile(0); break; }
	} else {
		for (Depot *depot : Depot::Iterate()) {
			if (GetTileOwner(depot->xy) != _current_company) continue;
			if ((type == VEH_TRAIN && IsRailDepotTile(depot->xy)) || (type == VEH_SHIP && IsShipDepotTile(depot->xy))) { tile = depot->xy; break; }
		}
	}
	if (tile == INVALID_TILE) FixtureAbort(__LINE__);
	EngineID engine = EngineID::Invalid();
	for (const Engine *e : Engine::Iterate()) {
		if (!IsEngineBuildable(e->index, type, _current_company)) continue;
		if (type == VEH_TRAIN && RailVehInfo(e->index)->railveh_type == RAILVEH_WAGON) continue;
		if (std::get<0>(Command<CMD_BUILD_VEHICLE>::Do({}, tile, e->index, false, INVALID_CARGO, INVALID_CLIENT_ID)).Succeeded()) { engine = e->index; break; }
	}
	if (engine == EngineID::Invalid()) FixtureAbort(__LINE__);
	auto [cost, id, cap, mail, cargo] = Command<CMD_BUILD_VEHICLE>::Do(DoCommandFlag::Execute, tile, engine, false, INVALID_CARGO, INVALID_CLIENT_ID);
	Checked("build-fixture", cost);
	Vehicle *v = Vehicle::Get(id);
	Checked("share-fixture", Command<CMD_CLONE_ORDER>::Do(DoCommandFlag::Execute, CO_SHARE, v->index, source->index));
	if (type == VEH_TRAIN) {
		EngineID wagon = EngineID::Invalid();
		for (const Engine *e : Engine::Iterate()) {
			if (e->type != type || RailVehInfo(e->index)->railveh_type != RAILVEH_WAGON || !IsEngineBuildable(e->index, type, _current_company)) continue;
			if (std::get<0>(Command<CMD_BUILD_VEHICLE>::Do({}, tile, e->index, false, INVALID_CARGO, INVALID_CLIENT_ID)).Succeeded()) { wagon = e->index; break; }
		}
		if (wagon == EngineID::Invalid()) FixtureAbort(__LINE__);
		for (int i = 0; i < 6; ++i) {
			auto built = Command<CMD_BUILD_VEHICLE>::Do(DoCommandFlag::Execute, tile, wagon, false, INVALID_CARGO, INVALID_CLIENT_ID);
			Checked("build-wagon", std::get<0>(built));
			Checked("attach-wagon", Command<CMD_MOVE_RAIL_VEHICLE>::Do(DoCommandFlag::Execute, std::get<1>(built), v->index, false));
		}
	}
	return v;
}
static uint CargoCount(const Vehicle *v)
{
	uint count = 0;
	for (const Vehicle *u = v; u != nullptr; u = u->Next()) count += u->cargo.TotalCount();
	return count;
}
static bool SameRandom(const SavedRandomSeeds &a, const SavedRandomSeeds &b)
{
	return std::equal(std::begin(a.random.state), std::end(a.random.state), std::begin(b.random.state)) && std::equal(std::begin(a.interactive_random.state), std::end(a.interactive_random.state), std::begin(b.interactive_random.state));
}
static bool FleetScenario(std::span<std::string_view> args)
{
	if (args.size() != 2) return false;
	Backup<CompanyID> company(_current_company);
	if (args[1] == "inspect") {
		for (Group *g : Group::Iterate()) if (GroupName(g) == "Fleet snowman \xE2\x98\x83") {
			company.Change(g->owner);
			fmt::print(stderr, "FLEET reloaded {} {} {}\n", GroupIsInGroup(g->index, g->parent), GetGroupNumVehicle(g->owner, g->parent, g->vehicle_type), g->livery.colour1 == COLOUR_RED);
		}
		company.Restore();
		return true;
	}
	Vehicle *v = args[1] == "ship" ? BuildFixture(VEH_SHIP) : args[1] == "aircraft" ? BuildFixture(VEH_AIRCRAFT) : args[1].starts_with("train") ? BuildFixture(VEH_TRAIN) : Vehicle::Get(VehicleID(126));
	if (!v->IsInDepot()) FixtureAbort(__LINE__);
	company.Change(v->owner);
	if (args[1] == "groups" || args[1] == "groups-store") {
		auto [parent_cost, parent] = Command<CMD_CREATE_GROUP>::Do(DoCommandFlag::Execute, v->type, GroupID::Invalid());
		Checked("parent", parent_cost);
		auto [child_cost, child] = Command<CMD_CREATE_GROUP>::Do(DoCommandFlag::Execute, v->type, parent);
		Checked("child", child_cost);
		Checked("rename", Command<CMD_ALTER_GROUP>::Do(DoCommandFlag::Execute, AlterGroupMode::Rename, child, GroupID::Invalid(), "Fleet snowman \xE2\x98\x83"));
		Checked("membership", std::get<0>(Command<CMD_ADD_VEHICLE_GROUP>::Do(DoCommandFlag::Execute, child, v->index, false, VehicleListIdentifier{})));
		Checked("all-rule", Command<CMD_SET_AUTOREPLACE>::Do(DoCommandFlag::Execute, ALL_GROUP, v->engine_type, v->engine_type, false));
		bool old = false;
		EngineID replacement = EngineReplacementForCompany(Company::Get(v->owner), v->engine_type, child, &old);
		fmt::print(stderr, "FLEET rule all {} {}\n", replacement == v->engine_type, old);
		Checked("protection", Command<CMD_SET_GROUP_FLAG>::Do(DoCommandFlag::Execute, parent, GroupFlag::ReplaceProtection, true, true));
		replacement = EngineReplacementForCompany(Company::Get(v->owner), v->engine_type, child, &old);
		fmt::print(stderr, "FLEET rule protected {} {}\n", replacement == EngineID::Invalid(), old);
		Checked("parent-rule", Command<CMD_SET_AUTOREPLACE>::Do(DoCommandFlag::Execute, parent, v->engine_type, v->engine_type, false));
		replacement = EngineReplacementForCompany(Company::Get(v->owner), v->engine_type, child, &old);
		fmt::print(stderr, "FLEET rule inherited {} {}\n", replacement == v->engine_type, old);
		Cost("recursive-parent", Command<CMD_ALTER_GROUP>::Do(DoCommandFlag::Execute, AlterGroupMode::SetParent, parent, child, ""));
		Checked("livery", Command<CMD_SET_GROUP_LIVERY>::Do(DoCommandFlag::Execute, parent, true, COLOUR_RED));
		fmt::print(stderr, "FLEET groups {} {} {} {}\n", GroupIsInGroup(child, parent), GetGroupNumVehicle(v->owner, parent, v->type), GetGroupNumEngines(v->owner, parent, v->engine_type), Group::Get(child)->livery.colour1 == COLOUR_RED);
		if (args[1] == "groups") {
		Checked("delete", Command<CMD_DELETE_GROUP>::Do(DoCommandFlag::Execute, parent));
		fmt::print(stderr, "FLEET deleted {} {} {}\n", !Group::IsValidID(parent), !Group::IsValidID(child), v->group_id == DEFAULT_GROUP);
		}
	} else {
		Company *c = Company::Get(v->owner);
		/* Typed inputs: old-enough actual command-built vehicles, carrying cargo. */
		v->age = TimerGameCalendar::Date(v->max_age.base() + 366);
		uint injected = 0;
		StationID first = StationID::Invalid();
		for (const Order &order : v->Orders()) if (order.IsType(OT_GOTO_STATION)) { first = order.GetDestination().ToStationID(); break; }
		if (first == StationID::Invalid()) FixtureAbort(__LINE__);
		for (Vehicle *u = v; u != nullptr; u = u->Next()) {
			if (u->cargo_cap == 0) continue;
			uint16_t amount = std::min<uint16_t>(u->cargo_cap, 7);
			if (!CargoPacket::CanAllocateItem()) FixtureAbort(__LINE__);
			u->cargo.Append(new CargoPacket(amount, 3, first, u->tile, Money(133)));
			injected += amount;
		}
		if (injected == 0) FixtureAbort(__LINE__);
		Checked("renew-rule", Command<CMD_SET_AUTOREPLACE>::Do(DoCommandFlag::Execute, ALL_GROUP, v->engine_type, v->engine_type, true));
		VehicleID original = v->index;
		EngineID engine = v->engine_type;
		uint orders = v->GetNumOrders();
		uint old_count = CargoCount(v);
		SavedRandomSeeds before, after;
		SaveRandomSeeds(&before);
		Cost("test", Command<CMD_AUTOREPLACE_VEHICLE>::Do({}, original));
		SaveRandomSeeds(&after);
		fmt::print(stderr, "FLEET test-rng {} original {}\n", SameRandom(before, after), Vehicle::IsValidID(original));
		fmt::print(stderr, "FLEET test-cargo {}\n", CargoCount(v) == old_count);
		if (args[1] == "cash" || args[1] == "train-rollback") {
			Backup<Money> cash(CompanyMoney(c));
			Backup<uint8_t> max_length(_settings_game.vehicle.max_train_length);
			if (args[1] == "cash") cash.Change(Money(0));
			else max_length.Change(1);
			SaveRandomSeeds(&before);
			Cost("rollback", Command<CMD_AUTOREPLACE_VEHICLE>::Do(DoCommandFlag::Execute, original));
			SaveRandomSeeds(&after);
			fmt::print(stderr, "FLEET rollback {} {} {} {}\n", SameRandom(before, after), Vehicle::IsValidID(original), CargoCount(v) == old_count, v->GetNumOrders() == orders);
			max_length.Restore();
			cash.Restore();
		} else {
			Checked("execute", Command<CMD_AUTOREPLACE_VEHICLE>::Do(DoCommandFlag::Execute, original));
			fmt::print(stderr, "FLEET replaced {}\n", !Vehicle::IsValidID(original));
			bool transferred = false;
			for (Vehicle *u : Vehicle::Iterate()) if (u->IsPrimaryVehicle() && u->engine_type == engine && u->age == 0 && u->GetNumOrders() == orders && CargoCount(u) == old_count) transferred = true;
			fmt::print(stderr, "FLEET transferred {} {}\n", transferred, old_count > 0);
		}
	}
	company.Restore();
	return true;
}
extern "C" void __real__Z22IConsoleStdLibRegisterv();
extern "C" void __wrap__Z22IConsoleStdLibRegisterv()
{
	__real__Z22IConsoleStdLibRegisterv();
	IConsole::CmdRegister("fleet_scenario", FleetScenario);
}
