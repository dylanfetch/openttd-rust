/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/* Named shared services and full native CommandCost storage. All callbacks are
 * noexcept, retain original move/AddCost behavior, and permit ordinary reentry.
 * No Rust state access spans them; environmental failures terminate. */
struct NativeFleetCosts {
	CommandCost result, replace, build, copy, temporary;
	SavedRandomSeeds seeds;
	OpenTTDFleetCosts View() { return {&result, &replace, &build, &copy, &temporary, &seeds}; }
};
static void FleetTransaction_cost_zero(void *out) noexcept { *static_cast<CommandCost *>(out) = CommandCost(); }
static void FleetTransaction_cost_vehicles(void *out) noexcept { *static_cast<CommandCost *>(out) = CommandCost(EXPENSES_NEW_VEHICLES, Money(0)); }
static void FleetTransaction_cost_error(void *out, uint32_t error) noexcept { *static_cast<CommandCost *>(out) = CommandCost(error); }
static void FleetTransaction_cost_add(void *out, void *item) noexcept { static_cast<CommandCost *>(out)->AddCost(std::move(*static_cast<CommandCost *>(item))); }
static void FleetTransaction_cost_move(void *out, void *item) noexcept { *static_cast<CommandCost *>(out) = std::move(*static_cast<CommandCost *>(item)); }
static void FleetTransaction_cost_amount(void *out, int64_t amount) noexcept { static_cast<CommandCost *>(out)->AddCost(Money(amount)); }
static bool FleetTransaction_success(void *out) noexcept { return static_cast<CommandCost *>(out)->Succeeded(); }
static uint32_t FleetTransaction_error(void *out) noexcept { return static_cast<CommandCost *>(out)->GetErrorMessage(); }
static int64_t FleetTransaction_money(void *out) noexcept { return static_cast<CommandCost *>(out)->GetCost(); }
static void FleetTransaction_ownership(void *out, uint8_t owner) noexcept { *static_cast<CommandCost *>(out) = CheckOwnership(CompanyID(owner)); }
static bool FleetTransaction_rear(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return Train::From(v)->IsRearDualheaded(); }
static bool FleetTransaction_articulated(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return v->IsArticulatedPart(); }
static bool FleetTransaction_crashed(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return v->vehstatus.Test(VehState::Crashed); }
static bool FleetTransaction_stopped(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return v->vehstatus.Test(VehState::Stopped); }
static bool FleetTransaction_chain_depot(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return v->IsChainInDepot(); }
static void * FleetTransaction_first(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return v->First(); }
static void * FleetTransaction_next_unit(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return Train::From(v)->GetNextUnit(); }
static void * FleetTransaction_prev_unit(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return Train::From(v)->GetPrevUnit(); }
static uint16_t FleetTransaction_length(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return Train::From(v)->gcache.cached_total_length; }
static bool FleetTransaction_flipped(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return Train::From(v)->GetTrainFlags().Test(VehicleRailFlag::Flipped); }
static uint8_t FleetTransaction_cargo_type(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return v->cargo_type; }
static bool FleetTransaction_can_carry(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return v->GetEngine()->CanCarryCargo(); }
static int32_t FleetTransaction_x(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return v->x_pos; }
static int32_t FleetTransaction_y(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return v->y_pos; }
static int32_t FleetTransaction_z(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return v->z_pos; }
static bool FleetTransaction_needs_renew(void *shell, bool settings) noexcept { return static_cast<Vehicle *>(shell)->NeedsAutorenewing(Company::Get(_current_company), settings); }
static bool FleetTransaction_engine_valid(uint16_t id) noexcept { return Engine::IsValidID(EngineID(id)); }
static bool FleetTransaction_company_valid(uint8_t id) noexcept { return Company::IsValidID(CompanyID(id)); }
static bool FleetTransaction_engine_buildable(uint16_t id, uint8_t type, uint8_t company) noexcept { return IsEngineBuildable(EngineID(id), static_cast<VehicleType>(type), CompanyID(company)); }
static uint64_t FleetTransaction_rail_compatible(uint16_t id) noexcept { return GetAllCompatibleRailTypes(RailVehInfo(EngineID(id))->railtypes).base(); }
static uint64_t FleetTransaction_road_powered(uint16_t id) noexcept { return GetRoadTypeInfo(RoadVehInfo(EngineID(id))->roadtype)->powered_roadtypes.base(); }
static bool FleetTransaction_wagon(uint16_t id) noexcept { return RailVehInfo(EngineID(id))->railveh_type == RAILVEH_WAGON; }
static bool FleetTransaction_tram(uint16_t id) noexcept { return Engine::Get(EngineID(id))->info.misc_flags.Test(EngineMiscFlag::RoadIsTram); }
static uint8_t FleetTransaction_plane(uint16_t id) noexcept { return AircraftVehInfo(EngineID(id))->subtype & AIR_CTOL; }
static uint64_t FleetTransaction_refit_mask(uint16_t id, bool initial) noexcept { return GetUnionOfArticulatedRefitMasks(EngineID(id), initial); }
static void FleetTransaction_refit_masks(uint16_t id, uint64_t *union_mask, uint64_t *available) noexcept { GetArticulatedRefitMasks(EngineID(id), true, union_mask, available); }
static uint64_t FleetTransaction_vehicle_cargo(void *shell, uint8_t *cargo) noexcept { return GetCargoTypesOfArticulatedVehicle(static_cast<Vehicle *>(shell), cargo); }
static uint64_t FleetTransaction_default_cargo(uint16_t id) noexcept { return GetCargoTypesOfArticulatedParts(EngineID(id)); }
static void * FleetTransaction_orders(void *shell) noexcept { return static_cast<Vehicle *>(shell)->orders; }
static size_t FleetTransaction_order_count(void *orders) noexcept { return static_cast<OrderList *>(orders)->GetOrders().size(); }
static uint8_t FleetTransaction_order_count_id(void *orders) noexcept { return static_cast<OrderList *>(orders)->GetNumOrders(); }
static void * FleetTransaction_order_at(void *orders, size_t index) noexcept { return &static_cast<OrderList *>(orders)->GetOrders()[index]; }
static bool FleetTransaction_order_refit(void *order) noexcept { return static_cast<Order *>(order)->IsRefit(); }
static bool FleetTransaction_order_auto(void *order) noexcept { return static_cast<Order *>(order)->IsAutoRefit(); }
static uint8_t FleetTransaction_order_cargo(void *order) noexcept { return static_cast<Order *>(order)->GetRefitCargo(); }
static bool FleetTransaction_local() noexcept { return IsLocalCompany(); }
static void FleetTransaction_refit_news(void *shell, int32_t order) noexcept { auto *v = static_cast<Vehicle *>(shell); VehicleID id = (v->type == VEH_TRAIN) ? v->First()->index : v->index; EncodedString headline = order != -1 ? GetEncodedString(STR_NEWS_VEHICLE_AUTORENEW_FAILED, id, STR_ERROR_AUTOREPLACE_INCOMPATIBLE_REFIT, order + 1) : GetEncodedString(STR_NEWS_VEHICLE_AUTORENEW_FAILED, id, STR_ERROR_AUTOREPLACE_INCOMPATIBLE_CARGO, CargoSpec::Get(v->cargo_type)->name); AddVehicleAdviceNewsItem(AdviceType::AutorenewFailed, std::move(headline), id); }
static void * FleetTransaction_build(void *out, void *shell, uint16_t engine) noexcept { auto *v = static_cast<Vehicle *>(shell); VehicleID id; std::tie(*static_cast<CommandCost *>(out), id, std::ignore, std::ignore, std::ignore) = Command<CMD_BUILD_VEHICLE>::Do({DoCommandFlag::Execute, DoCommandFlag::AutoReplace}, v->tile, EngineID(engine), true, INVALID_CARGO, INVALID_CLIENT_ID); return static_cast<CommandCost *>(out)->Failed() ? nullptr : Vehicle::Get(id); }
static void FleetTransaction_refit(void *out, void *shell, uint8_t cargo, uint8_t subtype) noexcept { *static_cast<CommandCost *>(out) = std::get<0>(Command<CMD_REFIT_VEHICLE>::Do(DoCommandFlag::Execute, static_cast<Vehicle *>(shell)->index, cargo, subtype, false, false, 0)); }
static uint8_t FleetTransaction_subtype(void *old, void *replacement, uint8_t cargo) noexcept { return GetBestFittingSubType(static_cast<Vehicle *>(old), static_cast<Vehicle *>(replacement), cargo); }
static bool FleetTransaction_reverse_probability(void *shell) noexcept { auto *v = static_cast<Vehicle *>(shell); return TestVehicleBuildProbability(v, v->engine_type, BuildProbabilityType::Reversed).has_value(); }
static void FleetTransaction_reverse(void *shell) noexcept { Command<CMD_REVERSE_TRAIN_DIRECTION>::Do(DoCommandFlag::Execute, static_cast<Vehicle *>(shell)->index, true); }
static void FleetTransaction_start_stop(void *out, void *shell, bool evaluate) noexcept { *static_cast<CommandCost *>(out) = Command<CMD_START_STOP_VEHICLE>::Do({DoCommandFlag::Execute, DoCommandFlag::AutoReplace}, static_cast<Vehicle *>(shell)->index, evaluate); }
static void FleetTransaction_move(void *out, void *shell, void *after, uint32_t flags, bool whole) noexcept { *static_cast<CommandCost *>(out) = Command<CMD_MOVE_RAIL_VEHICLE>::Do(DoCommandFlags(static_cast<uint16_t>(flags)).Set(DoCommandFlag::NoCargoCapacityCheck), static_cast<Vehicle *>(shell)->index, after == nullptr ? VehicleID::Invalid() : static_cast<Vehicle *>(after)->index, whole); }
static void FleetTransaction_sell(void *out, void *shell, uint32_t flags) noexcept { *static_cast<CommandCost *>(out) = Command<CMD_SELL_VEHICLE>::Do(DoCommandFlags(static_cast<uint16_t>(flags)), static_cast<Vehicle *>(shell)->index, false, false, INVALID_CLIENT_ID); }
static void FleetTransaction_clone_order(void *out, void *old, void *replacement) noexcept { *static_cast<CommandCost *>(out) = Command<CMD_CLONE_ORDER>::Do(DoCommandFlag::Execute, CO_SHARE, static_cast<Vehicle *>(replacement)->index, static_cast<Vehicle *>(old)->index); }
static void FleetTransaction_copy_group(void *out, void *old, void *replacement) noexcept { *static_cast<CommandCost *>(out) = std::get<0>(Command<CMD_ADD_VEHICLE_GROUP>::Do(DoCommandFlag::Execute, static_cast<Vehicle *>(old)->group_id, static_cast<Vehicle *>(replacement)->index, false, VehicleListIdentifier{})); }
static void FleetTransaction_copy_configuration(void *old, void *replacement) noexcept { static_cast<Vehicle *>(replacement)->CopyVehicleConfigAndStatistics(static_cast<Vehicle *>(old)); }
static void FleetTransaction_viewports(void *old, void *replacement) noexcept { ChangeVehicleViewports(static_cast<Vehicle *>(old)->index, static_cast<Vehicle *>(replacement)->index); }
static void FleetTransaction_view_window(void *old, void *replacement) noexcept { ChangeVehicleViewWindow(static_cast<Vehicle *>(old)->index, static_cast<Vehicle *>(replacement)->index); }
static void FleetTransaction_news(void *old, void *replacement) noexcept { ChangeVehicleNews(static_cast<Vehicle *>(old)->index, static_cast<Vehicle *>(replacement)->index); }
static void FleetTransaction_transfer_cargo(void *old, void *replacement, bool chain) noexcept { TransferCargo(static_cast<Vehicle *>(old), static_cast<Vehicle *>(replacement), chain); }
static void FleetTransaction_capacity(void *shell) noexcept { CheckCargoCapacity(static_cast<Vehicle *>(shell)); }
static void FleetTransaction_event(void *old, void *replacement) noexcept { auto *v = static_cast<Vehicle *>(old); AI::NewEvent(v->owner, new ScriptEventVehicleAutoReplaced(v->index, static_cast<Vehicle *>(replacement)->index)); }
static void FleetTransaction_save_rng(void *seeds) noexcept { SaveRandomSeeds(static_cast<SavedRandomSeeds *>(seeds)); }
static void FleetTransaction_restore_rng(void *seeds) noexcept { RestoreRandomSeeds(*static_cast<SavedRandomSeeds *>(seeds)); }
static void FleetTransaction_rule_window(uint16_t engine, uint16_t group) noexcept { InvalidateAutoreplaceWindow(EngineID(engine), GroupID(group)); }
static const OpenTTDFleetTransactionServices _fleet_transaction_services{
	FleetTransaction_cost_zero,
	FleetTransaction_cost_vehicles,
	FleetTransaction_cost_error,
	FleetTransaction_cost_add,
	FleetTransaction_cost_move,
	FleetTransaction_cost_amount,
	FleetTransaction_success,
	FleetTransaction_error,
	FleetTransaction_money,
	FleetTransaction_ownership,
	FleetTransaction_rear,
	FleetTransaction_articulated,
	FleetTransaction_crashed,
	FleetTransaction_stopped,
	FleetTransaction_chain_depot,
	FleetTransaction_first,
	FleetTransaction_next_unit,
	FleetTransaction_prev_unit,
	FleetTransaction_length,
	FleetTransaction_flipped,
	FleetTransaction_cargo_type,
	FleetTransaction_can_carry,
	FleetTransaction_x,
	FleetTransaction_y,
	FleetTransaction_z,
	FleetTransaction_needs_renew,
	FleetTransaction_engine_valid,
	FleetTransaction_company_valid,
	FleetTransaction_engine_buildable,
	FleetTransaction_rail_compatible,
	FleetTransaction_road_powered,
	FleetTransaction_wagon,
	FleetTransaction_tram,
	FleetTransaction_plane,
	FleetTransaction_refit_mask,
	FleetTransaction_refit_masks,
	FleetTransaction_vehicle_cargo,
	FleetTransaction_default_cargo,
	FleetTransaction_orders,
	FleetTransaction_order_count,
	FleetTransaction_order_count_id,
	FleetTransaction_order_at,
	FleetTransaction_order_refit,
	FleetTransaction_order_auto,
	FleetTransaction_order_cargo,
	FleetTransaction_local,
	FleetTransaction_refit_news,
	FleetTransaction_build,
	FleetTransaction_refit,
	FleetTransaction_subtype,
	FleetTransaction_reverse_probability,
	FleetTransaction_reverse,
	FleetTransaction_start_stop,
	FleetTransaction_move,
	FleetTransaction_sell,
	FleetTransaction_clone_order,
	FleetTransaction_copy_group,
	FleetTransaction_copy_configuration,
	FleetTransaction_viewports,
	FleetTransaction_view_window,
	FleetTransaction_news,
	FleetTransaction_transfer_cargo,
	FleetTransaction_capacity,
	FleetTransaction_event,
	FleetTransaction_save_rng,
	FleetTransaction_restore_rng,
	FleetTransaction_rule_window,
	STR_ERROR_RAIL_VEHICLE_NOT_AVAILABLE,
	STR_ERROR_TRAIN_TOO_LONG,
	STR_ERROR_TRAIN_TOO_LONG_AFTER_REPLACEMENT,
	STR_ERROR_AUTOREPLACE_NOTHING_TO_DO,
};
