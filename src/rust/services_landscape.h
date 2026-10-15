/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file services_landscape.h Canonical typed terrain leaves shared by game owners. */
#ifndef RUST_SERVICES_LANDSCAPE_H
#define RUST_SERVICES_LANDSCAPE_H
#include "ground_ffi.h"
#include "../clear_map.h"
#include "services_rail.h"
#include "../road_internal.h"
#include "../tree_map.h"
#include "../water_map.h"
#include "../landscape.h"
#include "../water.h"
#include "../newgrf_generic.h"
#include "../sound_func.h"
#include "../signal_func.h"

inline OpenTTDFoundationSlope RustFoundationSlope(uint32_t tile) noexcept { auto [slope, z] = GetFoundationSlope(TileIndex{tile}); return {.z = z, .slope = slope}; }
inline RoadBits RustCleanRoadBits(TileIndex tile, RoadBits bits) noexcept { return CleanUpRoadBits(tile, bits); }
inline uint8_t RustTileOwnerValue(uint32_t tile) noexcept { return GetTileOwner(TileIndex{tile}).base(); }
inline bool RustPlainRailTile(uint32_t tile) noexcept { return IsPlainRailTile(TileIndex{tile}); }
inline ClearGround RustClearGround(uint32_t tile) noexcept { return GetClearGround(TileIndex{tile}); }
inline uint32_t RustClearDensity(uint32_t tile) noexcept { return GetClearDensity(TileIndex{tile}); }
inline bool RustSnowTile(uint32_t tile) noexcept { return IsSnowTile(TileIndex{tile}); }
inline uint16_t RustFieldIndustryValue(uint32_t tile) noexcept { return GetIndustryIndexOfField(TileIndex{tile}).base(); }
inline bool RustHasWaterClass(uint32_t tile) noexcept { return HasTileWaterClass(TileIndex{tile}); }
inline WaterClass RustWaterClass(uint32_t tile) noexcept { return GetWaterClass(TileIndex{tile}); }
inline TreeGround RustTreeGround(uint32_t tile) noexcept { return GetTreeGround(TileIndex{tile}); }
inline bool RustIsCoast(uint32_t tile) noexcept { return IsCoast(TileIndex{tile}); }
inline void RustMakeClear(uint32_t tile, ClearGround ground, uint32_t density) noexcept { MakeClear(TileIndex{tile}, ground, density); }
inline void RustMakeClearValue(uint32_t tile, uint8_t ground, uint32_t density) noexcept { RustMakeClear(tile, static_cast<ClearGround>(ground), density); }
inline void RustMakeShore(uint32_t tile) noexcept { MakeShore(TileIndex{tile}); }
inline void RustMakeSnow(uint32_t tile, uint32_t density) noexcept { MakeSnow(TileIndex{tile}, density); }
inline void RustSetTreeGroundDensity(uint32_t tile, TreeGround ground, uint32_t density) noexcept { SetTreeGroundDensity(TileIndex{tile}, ground, density); }
inline void RustSetTreeGroundDensityValue(uint32_t tile, uint8_t ground, uint32_t density) noexcept { RustSetTreeGroundDensity(tile, static_cast<TreeGround>(ground), density); }
inline void RustClearNeighbourFlooding(uint32_t tile) noexcept { ClearNeighbourNonFloodingStates(TileIndex{tile}); }
inline void RustTileLoopWater(uint32_t tile) noexcept { TileLoop_Water(tile); }
inline bool RustAmbientSound(uint32_t tile) noexcept { AmbientSoundEffect(TileIndex{tile}); return HasGrfMiscBit(GrfMiscBit::AmbientSoundCallback); }
inline void RustPlayTileSound(uint32_t tile, uint16_t sound) noexcept { SndPlayTileFx(sound, TileIndex{tile}); }
inline void RustPlayTileSoundBySound(uint16_t sound, uint32_t tile) noexcept { RustPlayTileSound(tile, sound); }
inline void RustClearSquare(uint32_t tile) noexcept { DoClearSquare(TileIndex{tile}); }
inline void RustUpdateSignals() noexcept { UpdateSignalsInBuffer(); }
/* Necessary legacy signatures: context handles are unused and scalar return
 * widths are retained. All native work remains in the canonical typed leaves. */
inline uint32_t RustFoundationSlopeValue(uint32_t tile) noexcept { return RustFoundationSlope(tile).slope; }
template <typename Context>
inline void RustCrossingSound(Context, uint32_t tile) noexcept { RustPlayTileSound(tile, SND_0E_LEVEL_CROSSING); }
#endif /* RUST_SERVICES_LANDSCAPE_H */
