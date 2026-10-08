/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */
/** @file company_adapter.h Company facade calls; portable bodies remain at callers. */
#ifndef RUST_COMPANY_ADAPTER_H
#define RUST_COMPANY_ADAPTER_H
#include "company_ffi.h"
OpenTTDCompanyAction RunRustCompany(uint32_t op, uint32_t id = 0, int64_t a = 0, int64_t b = 0, int64_t c = 0, int64_t d = 0);
CommandCost RustCompanyCost(const OpenTTDCompanyAction &);
#endif
