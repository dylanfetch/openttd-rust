/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file landscape_services.h Canonical typed terrain leaves shared by game owners. */
#ifndef RUST_LANDSCAPE_SERVICES_H
#define RUST_LANDSCAPE_SERVICES_H
#include "../clear_map.h"
#include "../tree_map.h"
#include "../water_map.h"
#include "../landscape.h"
#include "../water.h"
#include "../newgrf_generic.h"
#include "../sound_func.h"
#include "../signal_func.h"

inline void RustMakeClear(uint32_t tile, uint8_t ground, uint32_t density) noexcept { MakeClear(TileIndex{tile}, static_cast<ClearGround>(ground), density); }
inline void RustMakeShore(uint32_t tile) noexcept { MakeShore(TileIndex{tile}); }
inline void RustMakeSnow(uint32_t tile, uint32_t density) noexcept { MakeSnow(TileIndex{tile}, density); }
inline void RustSetTreeGroundDensity(uint32_t tile, uint8_t ground, uint32_t density) noexcept { SetTreeGroundDensity(TileIndex{tile}, static_cast<TreeGround>(ground), density); }
inline void RustClearNeighbourFlooding(uint32_t tile) noexcept { ClearNeighbourNonFloodingStates(TileIndex{tile}); }
inline void RustTileLoopWater(uint32_t tile) noexcept { TileLoop_Water(tile); }
inline bool RustAmbientSound(uint32_t tile) noexcept { AmbientSoundEffect(TileIndex{tile}); return HasGrfMiscBit(GrfMiscBit::AmbientSoundCallback); }
inline void RustPlayTileSound(uint32_t tile, uint16_t sound) noexcept { SndPlayTileFx(sound, TileIndex{tile}); }
inline void RustClearSquare(uint32_t tile) noexcept { DoClearSquare(TileIndex{tile}); }
inline void RustUpdateSignals() noexcept { UpdateSignalsInBuffer(); }
#endif /* RUST_LANDSCAPE_SERVICES_H */
