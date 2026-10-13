/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file company_services.h Shared current-company context for synchronous Rust callbacks. */
#ifndef RUST_COMPANY_SERVICES_H
#define RUST_COMPANY_SERVICES_H
#include "../company_type.h"
#include "../company_func.h"

inline CompanyID OpenTTDCurrentCompany() noexcept { return _current_company; }
inline void OpenTTDSetCurrentCompany(CompanyID company) noexcept { _current_company = company; }
#endif /* RUST_COMPANY_SERVICES_H */
