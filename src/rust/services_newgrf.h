/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file services_newgrf.h Shared NewGRF leaves for synchronous Rust services. */
#ifndef RUST_SERVICES_NEWGRF_H
#define RUST_SERVICES_NEWGRF_H
#include "../newgrf_commons.h"
#include "../newgrf_debug.h"

/* Borrowed GRF files remain native and live for the synchronous conversion.
 * No C++ exception crosses Rust; environmental failures terminate. */
inline bool OpenTTDConvertBooleanCallback(const GRFFile *grf, uint16_t callback, uint16_t result) noexcept { return ConvertBooleanCallback(grf, callback, result); }
inline void OpenTTDDeleteInspect(GrfSpecFeature feature, uint index) noexcept { DeleteNewGRFInspectWindow(feature, index); }
/* Typed adapters retain the existing per-component table signatures. */
inline void OpenTTDDeleteHouseInspect(uint32_t tile) noexcept { OpenTTDDeleteInspect(GSF_HOUSES, tile); }
inline void OpenTTDDeleteIndustryTileInspect(uint32_t tile) noexcept { OpenTTDDeleteInspect(GSF_INDUSTRYTILES, tile); }
inline void OpenTTDDeleteIndustryInspect(uint16_t industry) noexcept { OpenTTDDeleteInspect(GSF_INDUSTRIES, industry); }
#endif /* RUST_SERVICES_NEWGRF_H */
