/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file shared_services.hpp Valid unused company callbacks for isolated service fixtures. */
#ifndef TEST_SHARED_SERVICES_HPP
#define TEST_SHARED_SERVICES_HPP
#include <cstdint>
struct OpenTTDTownHandle;
struct OpenTTDTownState;
inline bool UnusedCompanyValid(uint8_t) noexcept { return false; }
inline uint8_t UnusedNextCompany(uint8_t) noexcept { return UINT8_MAX; }
inline const OpenTTDTownHandle *UnusedClosestTown(uint32_t, uint32_t) noexcept { return nullptr; }
inline OpenTTDTownState *UnusedTownOwner(const OpenTTDTownHandle *) noexcept { return nullptr; }
inline uint8_t UnusedCurrentCompany() noexcept { return UINT8_MAX; }
#endif
