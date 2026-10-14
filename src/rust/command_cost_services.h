/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file command_cost_services.h Borrow native command result storage without copying. */
#ifndef RUST_COMMAND_COST_SERVICES_H
#define RUST_COMMAND_COST_SERVICES_H
#include "command_cost_ffi.h"
class CommandCost;
inline OpenTTDCommandCost *RustCommandCost(CommandCost *cost) noexcept { return reinterpret_cast<OpenTTDCommandCost *>(cost); }
inline const OpenTTDCommandCost *RustCommandCost(const CommandCost *cost) noexcept { return reinterpret_cast<const OpenTTDCommandCost *>(cost); }
#endif /* RUST_COMMAND_COST_SERVICES_H */
