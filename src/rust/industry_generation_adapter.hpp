/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry_generation_adapter.hpp Named generation abort handoff. */
#ifndef RUST_INDUSTRY_GENERATION_ADAPTER_HPP
#define RUST_INDUSTRY_GENERATION_ADAPTER_HPP
#include "generation_services.hpp"
#include <exception>

/** Invocation-local exception shell. The exception never enters Rust. */
struct OpenTTDIndustryGenerationContext { std::exception_ptr cancellation; };

#endif
