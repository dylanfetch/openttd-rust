/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file services.cpp Direct shared-world leaves for Rust game owners. */
#include "../stdafx.h"
#include "services_ffi.h"
#include "command_cost_services.h"
#ifdef WITH_RUST
#include "../clear_map.h"
#include "../tree_map.h"
#include "../industry_map.h"
#include "../water_map.h"
#include "../landscape.h"
#include "../viewport_func.h"
#include "../core/random_func.hpp"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_type.h"
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

static uint32_t SharedRandom(void *) noexcept { return Random(); }
static void ObserveTile(void *, uint32_t index, uint32_t *v) noexcept
{
	TileIndex tile{index};
	std::fill_n(v, 10, 0);
	v[0] = GetTileType(tile); v[1] = IsBridgeAbove(tile); v[2] = GetTropicZone(tile); v[3] = GetTileZ(tile);
	if (v[0] == MP_CLEAR) {
		v[4] = GetClearGround(tile); v[5] = GetClearDensity(tile); v[9] = IsSnowTile(tile);
	} else if (v[0] == MP_TREES) {
		v[4] = GetTreeGround(tile); v[5] = GetTreeDensity(tile); v[6] = GetTreeType(tile);
		v[7] = GetTreeCount(tile); v[8] = to_underlying(GetTreeGrowth(tile));
	} else if (v[0] == MP_WATER) {
		v[9] = (static_cast<uint32_t>(IsCoast(tile)) << 1) | (static_cast<uint32_t>(IsSlopeWithOneCornerRaised(GetTileSlope(tile))) << 2);
	}
}

static void WriteTile(void *, uint32_t op, uint32_t index, uint32_t a, uint32_t b, uint32_t c, uint32_t d) noexcept
{
	TileIndex tile{index};
	switch (op) {
		case 0: MakeTree(tile, static_cast<TreeType>(a), b, static_cast<TreeGrowthStage>(c), static_cast<TreeGround>(d & 255), d >> 8); break;
		case 1: SetTreeGroundDensity(tile, static_cast<TreeGround>(a), b); break;
		case 2: AddTreeCount(tile, static_cast<int>(a)); break;
		case 3: AddTreeGrowth(tile, a); break;
		case 4: SetTreeGrowth(tile, static_cast<TreeGrowthStage>(a)); break;
		case 5: MarkTileDirtyByTile(tile); break;
		case 6: MakeClear(tile, static_cast<ClearGround>(a), b); break;
		case 7: MakeShore(tile); break;
		case 8: MakeSnow(tile, a); break;
		case 9: SetTropicZone(tile, static_cast<TropicZone>(a)); break;
	}
}
static float SharedTrig(uint32_t kind, float value) noexcept { return kind == 0 ? sinf(value) : cosf(value); }

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
		.context = nullptr, .random = SharedRandom, .observe_tile = ObserveTile,
		.write_tile = WriteTile, .trig = SharedTrig, .industry = SharedIndustry,
		.company_valid = SharedCompanyValid, .next_company = SharedNextCompany, .current_company = SharedCurrentCompany,
		.closest_town = SharedClosestTown, .town_owner = SharedTownOwner,
	};
	return services;
}
#endif /* WITH_RUST */
