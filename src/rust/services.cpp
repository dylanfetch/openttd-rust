/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file services.cpp Direct shared-world leaves for Rust game owners. */
#include "../stdafx.h"
#include "services_ffi.h"
#ifdef WITH_RUST
#include "../clear_map.h"
#include "../tree_map.h"
#include "../industry_map.h"
#include "../water_map.h"
#include "../landscape.h"
#include "../viewport_func.h"
#include "../core/random_func.hpp"
#include "../safeguards.h"

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

const OpenTTDSharedServices &GetRustSharedServices() noexcept
{
	static const OpenTTDSharedServices services{nullptr, SharedRandom, ObserveTile, WriteTile, SharedTrig, SharedIndustry};
	return services;
}
#endif /* WITH_RUST */
