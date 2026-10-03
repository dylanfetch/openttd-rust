/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file history_adapter.hpp Typed executor for Rust's history instruction stream. */
#ifndef RUST_HISTORY_ADAPTER_HPP
#define RUST_HISTORY_ADAPTER_HPP
#include "history_ffi.h"
#include <memory>

namespace RustHistory {

static_assert(sizeof(uint) == sizeof(uint32_t) && sizeof(uintptr_t) == sizeof(size_t));
static_assert(sizeof(ValidHistoryMask) == sizeof(uint64_t));

using Owner = std::unique_ptr<OpenTTDRustHistoryEngine, decltype(&openttd_rust_history_destroy)>;

inline Owner Create(uint8_t mode, const HistoryRange &hr, ValidHistoryMask mask, uint age, uint month)
{
	return Owner(openttd_rust_history_create(mode, reinterpret_cast<uintptr_t>(&hr), mask, age, month), &openttd_rust_history_destroy);
}

/** Marshal immutable objects at Rust's request; tokens are identity, never layouts. */
inline void Describe(OpenTTDRustHistoryEngine *engine, uintptr_t token)
{
	const auto &hr = *reinterpret_cast<const HistoryRange *>(token);
	OpenTTDRustHistoryDescriptor descriptor{};
	descriptor.child = reinterpret_cast<uintptr_t>(hr.hr);
	descriptor.periods = hr.periods;
	descriptor.records = hr.records;
	descriptor.first = hr.first;
	descriptor.last = hr.last;
	descriptor.division = hr.division;
	descriptor.total_division = hr.total_division;
	if (hr.hr != nullptr) {
		descriptor.child_periods = hr.hr->periods;
		descriptor.child_division = hr.hr->division;
	}
	openttd_rust_history_describe(engine, descriptor);
}

inline uint64_t Scalar(uint8_t mode, const HistoryRange &hr, ValidHistoryMask mask, uint age, uint month)
{
	auto owner = Create(mode, hr, mask, age, month);
	for (;;) {
		auto step = openttd_rust_history_next(owner.get());
		switch (step.kind) {
			case 0: return step.value;
			case 1: Describe(owner.get(), step.token); break;
			default: NOT_REACHED();
		}
	}
}

template <typename T>
void Rotate(HistoryData<T> &history, ValidHistoryMask mask, const HistoryRange &hr, uint month)
{
	auto owner = Create(2, hr, mask, 0, month);
	for (;;) {
		auto step = openttd_rust_history_next(owner.get());
		switch (step.kind) {
			case 0: return;
			case 1: Describe(owner.get(), step.token); break;
			case 2:
				std::move_backward(std::next(std::begin(history), step.first), std::next(std::begin(history), step.last - 1), std::next(std::begin(history), step.last));
				break;
			case 3: history[step.target] = history[step.first]; break;
			case 4: history.front() = {}; break;
			case 5: {
				auto first = std::next(std::begin(history), step.first);
				auto last = std::next(first, step.count);
				history[step.target] = SumHistory<T>(std::span{first, last});
				break;
			}
			default: NOT_REACHED();
		}
	}
}

/**
 * Recursive scopes retain original typed scratch addresses and unwind order.
 * Rust selects all children/slots and owns OR validity; C++ executes its stream.
 */
template <typename T>
bool Query(OpenTTDRustHistoryEngine *engine, const HistoryData<T> &history, T &result)
{
	for (;;) {
		auto step = openttd_rust_history_next(engine);
		switch (step.kind) {
			case 1: Describe(engine, step.token); break;
			case 9:
				result = history[step.first];
				return openttd_rust_history_complete(engine) != 0;
			case 6: {
				std::array<T, HISTORY_MAX_DIVISION> tmp_result; // Preserve full default construction, including unused elements.
				openttd_rust_history_phase(engine, TimerGameEconomy::month);
				for (;;) {
					auto child = openttd_rust_history_next(engine);
					switch (child.kind) {
						case 7: Query(engine, history, tmp_result[child.target]); break;
						case 8:
							result = SumHistory<T>(std::span{std::begin(tmp_result), child.count});
							return openttd_rust_history_complete(engine) != 0;
						default: NOT_REACHED();
					}
				}
			}
			case 10: NOT_REACHED();
			default: NOT_REACHED();
		}
	}
}

template <typename T>
bool Get(const HistoryData<T> &history, ValidHistoryMask mask, const HistoryRange &hr, uint age, T &result)
{
	auto owner = Create(3, hr, mask, age, 0);
	return Query(owner.get(), history, result);
}

} // namespace RustHistory
#endif /* RUST_HISTORY_ADAPTER_HPP */
