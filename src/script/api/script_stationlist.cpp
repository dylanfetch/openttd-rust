/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_stationlist.cpp Implementation of ScriptStationList and friends. */

#include "../../stdafx.h"
#include "script_stationlist.hpp"
#include "script_vehicle.hpp"
#include "script_cargo.hpp"
#include "../../station_base.h"
#include "../../vehicle_base.h"

#ifdef WITH_RUST
#include "../../rust/station_cargo_ffi.h"
#endif

#include "../../safeguards.h"

ScriptStationList::ScriptStationList(ScriptStation::StationType station_type)
{
	EnforceDeityOrCompanyModeValid_Void();
	bool is_deity = ScriptCompanyMode::IsDeity();
	::CompanyID owner = ScriptObject::GetCompany();
	ScriptList::FillList<Station>(this,
		[is_deity, owner, station_type](const Station *st) {
			return (is_deity || st->owner == owner) && st->facilities.Any(static_cast<StationFacilities>(station_type));
		}
	);
}

ScriptStationList_Vehicle::ScriptStationList_Vehicle(VehicleID vehicle_id)
{
	if (!ScriptVehicle::IsPrimaryVehicle(vehicle_id)) return;

	const Vehicle *v = ::Vehicle::Get(vehicle_id);

	for (const Order &o : v->Orders()) {
		if (o.IsType(OT_GOTO_STATION)) this->AddItem(o.GetDestination().ToStationID().base());
	}
}

ScriptStationList_Cargo::ScriptStationList_Cargo(ScriptStationList_Cargo::CargoMode mode,
		ScriptStationList_Cargo::CargoSelector selector, StationID station_id, CargoType cargo,
		StationID other_station)
{
	switch (mode) {
		case CM_WAITING:
			ScriptStationList_CargoWaiting(selector, station_id, cargo, other_station).SwapList(this);
			break;
		case CM_PLANNED:
			ScriptStationList_CargoPlanned(selector, station_id, cargo, other_station).SwapList(this);
			break;
		default:
			NOT_REACHED();
	}
}

ScriptStationList_CargoWaiting::ScriptStationList_CargoWaiting(
		ScriptStationList_Cargo::CargoSelector selector, StationID station_id, CargoType cargo,
		StationID other_station)
{
	switch (selector) {
		case CS_BY_FROM:
			ScriptStationList_CargoWaitingByFrom(station_id, cargo).SwapList(this);
			break;
		case CS_VIA_BY_FROM:
			ScriptStationList_CargoWaitingViaByFrom(station_id, cargo, other_station).SwapList(this);
			break;
		case CS_BY_VIA:
			ScriptStationList_CargoWaitingByVia(station_id, cargo).SwapList(this);
			break;
		case CS_FROM_BY_VIA:
			ScriptStationList_CargoWaitingFromByVia(station_id, cargo, other_station).SwapList(this);
			break;
		default:
			NOT_REACHED();
	}
}

ScriptStationList_CargoPlanned::ScriptStationList_CargoPlanned(
		ScriptStationList_Cargo::CargoSelector selector, StationID station_id, CargoType cargo,
		StationID other_station)
{
	switch (selector) {
		case CS_BY_FROM:
			ScriptStationList_CargoPlannedByFrom(station_id, cargo).SwapList(this);
			break;
		case CS_VIA_BY_FROM:
			ScriptStationList_CargoPlannedViaByFrom(station_id, cargo, other_station).SwapList(this);
			break;
		case CS_BY_VIA:
			ScriptStationList_CargoPlannedByVia(station_id, cargo).SwapList(this);
			break;
		case CS_FROM_BY_VIA:
			ScriptStationList_CargoPlannedFromByVia(station_id, cargo, other_station).SwapList(this);
			break;
		default:
			NOT_REACHED();
	}
}

#ifdef WITH_RUST

static_assert(sizeof(uint) == sizeof(uint32_t));
static_assert(sizeof(SQInteger) == sizeof(int64_t));
static_assert(sizeof(StationID::BaseType) == sizeof(uint16_t));
static_assert(ScriptStationList_Cargo::CM_WAITING == 0 && ScriptStationList_Cargo::CM_PLANNED == 1);
static_assert(ScriptStationList_Cargo::CS_BY_FROM == 0 && ScriptStationList_Cargo::CS_VIA_BY_FROM == 1);
static_assert(ScriptStationList_Cargo::CS_BY_VIA == 2 && ScriptStationList_Cargo::CS_FROM_BY_VIA == 3);
static_assert(sizeof(OpenTTDCargoCollector) == 16 && alignof(OpenTTDCargoCollector) == alignof(uint32_t));
static_assert(offsetof(OpenTTDCargoCollector, amount) == 0 && offsetof(OpenTTDCargoCollector, previous) == 4);
static_assert(offsetof(OpenTTDCargoCollector, last_key) == 8 && offsetof(OpenTTDCargoCollector, other) == 10);
static_assert(offsetof(OpenTTDCargoCollector, origin) == 12 && offsetof(OpenTTDCargoCollector, selector) == 14 && offsetof(OpenTTDCargoCollector, finalized) == 15);

/** No world/list pointer enters Rust except a synchronous destination borrow. */
class CargoCollector {
public:
	CargoCollector(ScriptStationList_Cargo *parent, StationID station_id, CargoType cargo, StationID other, ScriptStationList_Cargo::CargoSelector selector) : list(parent->GetRustListOwner())
	{
		openttd_rust_cargo_init(&this->state, selector, other.base());
		/* Preserve validation order, error state and null goods early returns. */
		if (!ScriptStation::IsValidStation(station_id)) return;
		if (!ScriptCargo::IsValidCargo(cargo)) return;
		this->ge = &(Station::Get(station_id)->goods[cargo]);
	}
	~CargoCollector() { openttd_rust_cargo_finish(&this->state, this->list); }
	CargoCollector(const CargoCollector &) = delete;
	CargoCollector &operator=(const CargoCollector &) = delete;
	const GoodsEntry *GE() const { return this->ge; }
	void Packet(StationID from, StationID via, uint amount) { openttd_rust_cargo_packet(&this->state, this->list, from.base(), via.base(), amount); }
	void Origin(StationID origin) { openttd_rust_cargo_origin(&this->state, origin.base()); }
	void Share(StationID via, uint32_t cumulative) { openttd_rust_cargo_share(&this->state, this->list, via.base(), cumulative); }
private:
	OpenTTDCargoCollector state;
	OpenTTDScriptList *list;
	const GoodsEntry *ge = nullptr;
};

void ScriptStationList_Cargo::AddCargo(CargoMode mode, CargoSelector selector, StationID station_id, CargoType cargo, StationID other_station)
{
	CargoCollector collector(this, station_id, cargo, other_station, selector);
	if (collector.GE() == nullptr) return;
	if (!collector.GE()->HasData()) return;

	/* Rust selects traversal/filter/grouping policy. C++ performs actual typed
	 * begin/end, equal_range and find; no STL or world layout crosses the ABI. */
	const auto plan = openttd_rust_cargo_plan(mode, selector);
	switch (plan) {
		case 0: [[fallthrough]];
		case 1: {
			const auto *packets = collector.GE()->GetData().cargo.Packets();
			const auto range = plan == 1 ? packets->equal_range(other_station) : std::pair{packets->begin(), packets->end()};
			for (auto iter = range.first; iter != range.second; ++iter) collector.Packet((*iter)->GetFirstStation(), iter.GetKey(), (*iter)->Count());
			break;
		}
		case 2: [[fallthrough]];
		case 3: {
			const auto &flows = collector.GE()->GetData().flows;
			auto feed_origin = [&collector](FlowStatMap::const_iterator iter) {
				collector.Origin(iter->first);
				const FlowStat::SharesMap *shares = iter->second.GetShares();
				for (auto share = shares->begin(); share != shares->end(); ++share) collector.Share(share->second, share->first);
			};
			if (plan == 3) {
				auto iter = flows.find(other_station);
				if (iter == flows.end()) return;
				feed_origin(iter);
			} else {
				for (auto iter = flows.begin(); iter != flows.end(); ++iter) feed_origin(iter);
			}
			break;
		}
		default: NOT_REACHED();
	}
}

ScriptStationList_CargoWaitingByFrom::ScriptStationList_CargoWaitingByFrom(StationID station_id, CargoType cargo)
{
	this->AddCargo(CM_WAITING, CS_BY_FROM, station_id, cargo);
}

ScriptStationList_CargoWaitingViaByFrom::ScriptStationList_CargoWaitingViaByFrom(StationID station_id, CargoType cargo, StationID other_station)
{
	this->AddCargo(CM_WAITING, CS_VIA_BY_FROM, station_id, cargo, other_station);
}

ScriptStationList_CargoWaitingByVia::ScriptStationList_CargoWaitingByVia(StationID station_id, CargoType cargo)
{
	this->AddCargo(CM_WAITING, CS_BY_VIA, station_id, cargo);
}

ScriptStationList_CargoWaitingFromByVia::ScriptStationList_CargoWaitingFromByVia(StationID station_id, CargoType cargo, StationID other_station)
{
	this->AddCargo(CM_WAITING, CS_FROM_BY_VIA, station_id, cargo, other_station);
}

ScriptStationList_CargoPlannedByFrom::ScriptStationList_CargoPlannedByFrom(StationID station_id, CargoType cargo)
{
	this->AddCargo(CM_PLANNED, CS_BY_FROM, station_id, cargo);
}

ScriptStationList_CargoPlannedViaByFrom::ScriptStationList_CargoPlannedViaByFrom(StationID station_id, CargoType cargo, StationID other_station)
{
	this->AddCargo(CM_PLANNED, CS_VIA_BY_FROM, station_id, cargo, other_station);
}

ScriptStationList_CargoPlannedByVia::ScriptStationList_CargoPlannedByVia(StationID station_id, CargoType cargo)
{
	this->AddCargo(CM_PLANNED, CS_BY_VIA, station_id, cargo);
}

ScriptStationList_CargoPlannedFromByVia::ScriptStationList_CargoPlannedFromByVia(StationID station_id, CargoType cargo, StationID other_station)
{
	this->AddCargo(CM_PLANNED, CS_FROM_BY_VIA, station_id, cargo, other_station);
}

#else

class CargoCollector {
public:
	CargoCollector(ScriptStationList_Cargo *parent, StationID station_id, CargoType cargo,
			StationID other);
	~CargoCollector() ;

	template <ScriptStationList_Cargo::CargoSelector Tselector>
	void Update(StationID from, StationID via, uint amount);
	const GoodsEntry *GE() const { return ge; }

private:
	void SetValue();

	ScriptStationList_Cargo *list;
	const GoodsEntry *ge;
	StationID other_station;

	StationID last_key;
	uint amount;
};

CargoCollector::CargoCollector(ScriptStationList_Cargo *parent,
		StationID station_id, CargoType cargo, StationID other) :
	list(parent), ge(nullptr), other_station(other), last_key(StationID::Invalid()), amount(0)
{
	if (!ScriptStation::IsValidStation(station_id)) return;
	if (!ScriptCargo::IsValidCargo(cargo)) return;
	this->ge = &(Station::Get(station_id)->goods[cargo]);
}

CargoCollector::~CargoCollector()
{
	this->SetValue();
}

void CargoCollector::SetValue()
{
	if (this->amount > 0) {
		if (this->list->HasItem(this->last_key.base())) {
			this->list->SetValue(this->last_key.base(),
					this->list->GetValue(this->last_key.base()) + this->amount);
		} else {
			this->list->AddItem(this->last_key.base(), this->amount);
		}
	}
}

template <ScriptStationList_Cargo::CargoSelector Tselector>
void CargoCollector::Update(StationID from, StationID via, uint amount)
{
	StationID key = StationID::Invalid();
	switch (Tselector) {
		case ScriptStationList_Cargo::CS_VIA_BY_FROM:
			if (via != this->other_station) return;
			[[fallthrough]];
		case ScriptStationList_Cargo::CS_BY_FROM:
			key = from;
			break;
		case ScriptStationList_Cargo::CS_FROM_BY_VIA:
			if (from != this->other_station) return;
			[[fallthrough]];
		case ScriptStationList_Cargo::CS_BY_VIA:
			key = via;
			break;
	}
	if (key == this->last_key) {
		this->amount += amount;
	} else {
		this->SetValue();
		this->amount = amount;
		this->last_key = key;
	}
}


template <ScriptStationList_Cargo::CargoSelector Tselector>
void ScriptStationList_CargoWaiting::Add(StationID station_id, CargoType cargo, StationID other_station)
{
	CargoCollector collector(this, station_id, cargo, other_station);
	if (collector.GE() == nullptr) return;
	if (!collector.GE()->HasData()) return;

	StationCargoList::ConstIterator iter = collector.GE()->GetData().cargo.Packets()->begin();
	StationCargoList::ConstIterator end = collector.GE()->GetData().cargo.Packets()->end();
	for (; iter != end; ++iter) {
		collector.Update<Tselector>((*iter)->GetFirstStation(), iter.GetKey(), (*iter)->Count());
	}
}


template <ScriptStationList_Cargo::CargoSelector Tselector>
void ScriptStationList_CargoPlanned::Add(StationID station_id, CargoType cargo, StationID other_station)
{
	CargoCollector collector(this, station_id, cargo, other_station);
	if (collector.GE() == nullptr) return;
	if (!collector.GE()->HasData()) return;

	FlowStatMap::const_iterator iter = collector.GE()->GetData().flows.begin();
	FlowStatMap::const_iterator end = collector.GE()->GetData().flows.end();
	for (; iter != end; ++iter) {
		const FlowStat::SharesMap *shares = iter->second.GetShares();
		uint prev = 0;
		for (FlowStat::SharesMap::const_iterator flow_iter = shares->begin();
				flow_iter != shares->end(); ++flow_iter) {
			collector.Update<Tselector>(iter->first, flow_iter->second, flow_iter->first - prev);
			prev = flow_iter->first;
		}
	}
}

ScriptStationList_CargoWaitingByFrom::ScriptStationList_CargoWaitingByFrom(StationID station_id,
		CargoType cargo)
{
	this->Add<CS_BY_FROM>(station_id, cargo);
}

ScriptStationList_CargoWaitingViaByFrom::ScriptStationList_CargoWaitingViaByFrom(
		StationID station_id, CargoType cargo, StationID via)
{
	CargoCollector collector(this, station_id, cargo, via);
	if (collector.GE() == nullptr) return;
	if (!collector.GE()->HasData()) return;

	std::pair<StationCargoList::ConstIterator, StationCargoList::ConstIterator> range =
			collector.GE()->GetData().cargo.Packets()->equal_range(via);
	for (StationCargoList::ConstIterator iter = range.first; iter != range.second; ++iter) {
		collector.Update<CS_VIA_BY_FROM>((*iter)->GetFirstStation(), iter.GetKey(), (*iter)->Count());
	}
}


ScriptStationList_CargoWaitingByVia::ScriptStationList_CargoWaitingByVia(StationID station_id,
		CargoType cargo)
{
	this->Add<CS_BY_VIA>(station_id, cargo);
}

ScriptStationList_CargoWaitingFromByVia::ScriptStationList_CargoWaitingFromByVia(
		StationID station_id, CargoType cargo, StationID from)
{
	this->Add<CS_FROM_BY_VIA>(station_id, cargo, from);
}

ScriptStationList_CargoPlannedByFrom::ScriptStationList_CargoPlannedByFrom(StationID station_id,
		CargoType cargo)
{
	this->Add<CS_BY_FROM>(station_id, cargo);
}

ScriptStationList_CargoPlannedViaByFrom::ScriptStationList_CargoPlannedViaByFrom(
		StationID station_id, CargoType cargo, StationID via)
{
	this->Add<CS_VIA_BY_FROM>(station_id, cargo, via);
}


ScriptStationList_CargoPlannedByVia::ScriptStationList_CargoPlannedByVia(StationID station_id,
		CargoType cargo)
{
	this->Add<CS_BY_VIA>(station_id, cargo);
}


ScriptStationList_CargoPlannedFromByVia::ScriptStationList_CargoPlannedFromByVia(
		StationID station_id, CargoType cargo, StationID from)
{
	CargoCollector collector(this, station_id, cargo, from);
	if (collector.GE() == nullptr) return;
	if (!collector.GE()->HasData()) return;

	FlowStatMap::const_iterator iter = collector.GE()->GetData().flows.find(from);
	if (iter == collector.GE()->GetData().flows.end()) return;
	const FlowStat::SharesMap *shares = iter->second.GetShares();
	uint prev = 0;
	for (FlowStat::SharesMap::const_iterator flow_iter = shares->begin();
			flow_iter != shares->end(); ++flow_iter) {
		collector.Update<CS_FROM_BY_VIA>(iter->first, flow_iter->second, flow_iter->first - prev);
		prev = flow_iter->first;
	}
}

#endif /* WITH_RUST */
