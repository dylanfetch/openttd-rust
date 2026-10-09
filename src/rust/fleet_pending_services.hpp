/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/* Typed tick-end services. Rust owns ordering and policy; native CommandCost
 * and presentation encoding stay on the native caller stack. */
static uint8_t FleetPending_current() noexcept { return _current_company.base(); }
static void FleetPending_set_current(uint8_t company) noexcept { _current_company = CompanyID(company); }
static void * FleetPending_vehicle(uint32_t id) noexcept { return Vehicle::Get(VehicleID(id)); }
static uint8_t FleetPending_owner(void *shell) noexcept { return static_cast<Vehicle *>(shell)->owner.base(); }
static void FleetPending_restart(void *shell) noexcept { static_cast<Vehicle *>(shell)->vehstatus.Reset(VehState::Stopped); }
static int32_t FleetPending_x(void *shell) noexcept { return static_cast<Vehicle *>(shell)->x_pos; }
static int32_t FleetPending_y(void *shell) noexcept { return static_cast<Vehicle *>(shell)->y_pos; }
static int32_t FleetPending_z(void *shell) noexcept { return static_cast<Vehicle *>(shell)->z_pos; }
static uint32_t FleetPending_reserve(uint8_t company) noexcept { return Company::Get(CompanyID(company))->settings.engine_renew_money; }
static void FleetPending_subtract(int64_t amount) noexcept { SubtractMoneyFromCompany(CommandCost(EXPENSES_NEW_VEHICLES, Money(amount))); }
static void FleetPending_command(void *out, uint32_t id) noexcept { *static_cast<CommandCost *>(out) = Command<CMD_AUTOREPLACE_VEHICLE>::Do(DoCommandFlag::Execute, VehicleID(id)); }
static bool FleetPending_local() noexcept { return IsLocalCompany(); }
static bool FleetPending_success(void *cost) noexcept { return static_cast<CommandCost *>(cost)->Succeeded(); }
static int64_t FleetPending_money(void *cost) noexcept { return static_cast<CommandCost *>(cost)->GetCost(); }
static uint32_t FleetPending_error(void *cost) noexcept { return static_cast<CommandCost *>(cost)->GetErrorMessage(); }
static void FleetPending_animation(int32_t x, int32_t y, int32_t z, int64_t amount) noexcept { ShowCostOrIncomeAnimation(x, y, z, Money(amount)); }
static void FleetPending_length_news(uint32_t id) noexcept { AddVehicleAdviceNewsItem(AdviceType::AutorenewFailed, GetEncodedString(STR_ERROR_TRAIN_TOO_LONG_AFTER_REPLACEMENT, VehicleID(id)), VehicleID(id)); }
static void FleetPending_failed_news(uint32_t id, uint32_t error) noexcept { AddVehicleAdviceNewsItem(AdviceType::AutorenewFailed, GetEncodedString(STR_NEWS_VEHICLE_AUTORENEW_FAILED, VehicleID(id), error, std::monostate{}), VehicleID(id)); }
static const OpenTTDFleetPendingServices _fleet_pending_services{
	FleetPending_current,
	FleetPending_set_current,
	FleetPending_vehicle,
	FleetPending_owner,
	FleetPending_restart,
	FleetPending_x,
	FleetPending_y,
	FleetPending_z,
	FleetPending_reserve,
	FleetPending_subtract,
	FleetPending_command,
	FleetPending_local,
	FleetPending_success,
	FleetPending_money,
	FleetPending_error,
	FleetPending_animation,
	FleetPending_length_news,
	FleetPending_failed_news,
	STR_ERROR_AUTOREPLACE_NOTHING_TO_DO,
	STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY,
	STR_ERROR_AUTOREPLACE_MONEY_LIMIT,
	STR_ERROR_TRAIN_TOO_LONG_AFTER_REPLACEMENT,
};
