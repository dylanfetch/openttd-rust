/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file services.cpp Direct shared-world leaves for Rust game owners. */
#include "../stdafx.h"
#include "services_ffi.h"
#include "command_services.h"
#ifdef WITH_RUST
#include "../industry_map.h"
#include "../bridge_map.h"
#include "../landscape.h"
#include "../viewport_func.h"
#include "../core/random_func.hpp"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_type.h"
#include "../command_func.h"
#include "../landscape_cmd.h"
#include "../town.h"
#include "../safeguards.h"

static CommandCost *NativeCost(OpenTTDCommandCost *cost) noexcept { return reinterpret_cast<CommandCost *>(cost); }
static const CommandCost *NativeCost(const OpenTTDCommandCost *cost) noexcept { return reinterpret_cast<const CommandCost *>(cost); }
static void CostZero(OpenTTDCommandCost *out) noexcept { *NativeCost(out) = CommandCost(); }
static void CostError(OpenTTDCommandCost *out, uint32_t message) noexcept { *NativeCost(out) = CommandCost(message); }
static void CostExpenseMoney(OpenTTDCommandCost *out, uint8_t expense, int64_t amount) noexcept { *NativeCost(out) = CommandCost(static_cast<ExpensesType>(expense), Money(amount)); }
static void CostErrorParam(OpenTTDCommandCost *out, uint32_t message, uint64_t parameter) noexcept { *NativeCost(out) = CommandCostWithParam(message, parameter); }
static void CostAdd(OpenTTDCommandCost *out, OpenTTDCommandCost *item) noexcept { NativeCost(out)->AddCost(std::move(*NativeCost(item))); }
static void CostMove(OpenTTDCommandCost *out, OpenTTDCommandCost *item) noexcept { *NativeCost(out) = std::move(*NativeCost(item)); }
static void CostAmount(OpenTTDCommandCost *out, int64_t amount) noexcept { NativeCost(out)->AddCost(Money(amount)); }
static bool CostSuccess(const OpenTTDCommandCost *cost) noexcept { return NativeCost(cost)->Succeeded(); }
static uint32_t CostErrorMessage(const OpenTTDCommandCost *cost) noexcept { return NativeCost(cost)->GetErrorMessage(); }
static int64_t CostMoney(const OpenTTDCommandCost *cost) noexcept { return NativeCost(cost)->GetCost(); }
const OpenTTDCommandCostServices &GetRustCommandCostServices() noexcept
{
	static const OpenTTDCommandCostServices services{
		.zero = CostZero, .error = CostError, .expense_money = CostExpenseMoney,
		.error_param = CostErrorParam, .add = CostAdd, .move = CostMove,
		.amount = CostAmount, .success = CostSuccess, .error_message = CostErrorMessage, .money = CostMoney,
	};
	return services;
}

/* Direct command execution and its tile callbacks do not run a script VM or
 * save/load code. They may reenter Rust owners, but ordinary play cannot unwind
 * these leaves; allocation/error-formatting failures terminate under noexcept.
 * The native returned object is moved whole into its caller's stack sink. */
static void LandscapeClear(OpenTTDCommandCost *out, uint32_t tile, uint16_t flags) noexcept
{
	*NativeCost(out) = Command<CMD_LANDSCAPE_CLEAR>::Do(DoCommandFlags(flags), TileIndex{tile});
}
const OpenTTDCommandServices &GetRustCommandServices() noexcept
{
	static const OpenTTDCommandServices services{.landscape_clear = LandscapeClear};
	return services;
}

static uint32_t SharedRandom(void *) noexcept { return Random(); }
static uint8_t SharedTileType(uint32_t tile) noexcept { return GetTileType(TileIndex{tile}); }
static bool SharedBridgeAbove(uint32_t tile) noexcept { return IsBridgeAbove(TileIndex{tile}); }
static uint8_t SharedTropicZone(uint32_t tile) noexcept { return GetTropicZone(TileIndex{tile}); }
static int32_t SharedTileZ(uint32_t tile) noexcept { return GetTileZ(TileIndex{tile}); }
static uint8_t SharedTileSlope(uint32_t tile) noexcept { return GetTileSlope(TileIndex{tile}); }
static void SharedMarkDirty(uint32_t tile) noexcept { MarkTileDirtyByTile(TileIndex{tile}); }
static void SharedSetTropicZone(uint32_t tile, uint8_t zone) noexcept { SetTropicZone(TileIndex{tile}, static_cast<TropicZone>(zone)); }

/** Pure map query; no allocation, reentry or C++ exception crosses into Rust. */
static uint32_t SharedIndustry(int32_t x, int32_t y, uint32_t *index) noexcept
{
	TileIndex tile = TileVirtXY(x, y);
	*index = tile.base();
	if (!IsTileType(tile, MP_INDUSTRY)) return 0;
	return GetIndustryGfx(tile) == GFX_BUBBLE_CATCHER ? 2 : 1;
}

static bool SharedCompanyValid(uint8_t company) noexcept { return Company::IsValidID(CompanyID(company)); }
static uint8_t SharedCurrentCompany() noexcept { return _current_company.base(); }
static uint8_t SharedNextCompany(uint8_t from) noexcept
{
	auto range = Company::Iterate(from);
	auto it = range.begin();
	return it == range.end() ? CompanyID::Invalid().base() : (*it)->index.base();
}

static const OpenTTDTownHandle *SharedClosestTown(uint32_t tile, uint32_t threshold) noexcept
{
	return reinterpret_cast<const OpenTTDTownHandle *>(ClosestTownFromTile(TileIndex{tile}, threshold));
}
static OpenTTDTownState *SharedTownOwner(const OpenTTDTownHandle *town) noexcept
{
	return reinterpret_cast<const Town *>(town)->growth_owner.get();
}

const OpenTTDSharedServices &GetRustSharedServices() noexcept
{
	static const OpenTTDSharedServices services{
		.context = nullptr,
		.random = SharedRandom,
		.industry = SharedIndustry,
		.tile_type = SharedTileType,
		.bridge_above = SharedBridgeAbove,
		.tropic_zone = SharedTropicZone,
		.tile_z = SharedTileZ,
		.tile_slope = SharedTileSlope,
		.mark_dirty = SharedMarkDirty,
		.set_tropic_zone = SharedSetTropicZone,
		.company_valid = SharedCompanyValid, .next_company = SharedNextCompany, .current_company = SharedCurrentCompany,
		.closest_town = SharedClosestTown, .town_owner = SharedTownOwner,
	};
	return services;
}
#endif /* WITH_RUST */
