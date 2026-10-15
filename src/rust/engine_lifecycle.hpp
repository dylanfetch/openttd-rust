/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file engine_lifecycle.hpp Canonical engine lifecycle storage and shell lifetime. */
#ifndef RUST_ENGINE_LIFECYCLE_HPP
#define RUST_ENGINE_LIFECYCLE_HPP
#include "byte_span_ffi.h"
#include <utility>
/** Scalar prefix of one Rust-owned allocation; native references alias only named
 * fields. No native field duplicates live state. Game-state mutex serializes all
 * access; references end before callbacks. Strings stay opaque Rust byte vectors. */
struct OpenTTDEngineLifecycle {
	CompanyMask company_avail{}; ///< Bit for each company whether the engine is available for that company.
	CompanyMask company_hidden{}; ///< Bit for each company whether the engine is normally hidden in the build gui for that company.
	CompanyMask preview_asked{}; ///< Bit for each company which has already been offered a preview.


	TimerGameCalendar::Date intro_date{}; ///< Date of introduction of the engine.
	int32_t age = 0; ///< Age of the engine in months.

	uint16_t reliability = 0; ///< Current reliability of the engine.
	uint16_t reliability_spd_dec = 0; ///< Speed of reliability decay between services (per day).
	uint16_t reliability_start = 0; ///< Initial reliability of the engine.
	uint16_t reliability_max = 0; ///< Maximal reliability of the engine.
	uint16_t reliability_final = 0; ///< Final reliability of the engine.
	uint16_t duration_phase_1 = 0; ///< First reliability phase in months, increasing reliability from #reliability_start to #reliability_max.
	uint16_t duration_phase_2 = 0; ///< Second reliability phase in months, keeping #reliability_max.
	uint16_t duration_phase_3 = 0; ///< Third reliability phase in months, decaying to #reliability_final.
	EngineFlags flags{}; ///< Flags of the engine. @see EngineFlags

	CompanyID preview_company = CompanyID::Invalid();  ///< Company which is currently being offered a preview \c CompanyID::Invalid() means no company.
	uint8_t preview_wait = 0; ///< Daily countdown timer for timeout of offering the engine to the #preview_company company.
};
extern "C" {
OpenTTDEngineLifecycle *openttd_rust_engine_new();
void openttd_rust_engine_destroy(OpenTTDEngineLifecycle *);
OpenTTDByteSpan openttd_rust_engine_name(const OpenTTDEngineLifecycle *);
void openttd_rust_engine_set_name(OpenTTDEngineLifecycle *, OpenTTDByteSpan);
}
/** Vector relocation transfers the allocation. Copies and assignment cannot
 * silently share ownership or invalidate native scalar aliases. */
struct EngineLifecycleOwner {
	OpenTTDEngineLifecycle *state;
	EngineLifecycleOwner() : state(openttd_rust_engine_new()) { new (this->state) OpenTTDEngineLifecycle(); }
	EngineLifecycleOwner(EngineLifecycleOwner &&other) noexcept : state(std::exchange(other.state, nullptr)) {}
	EngineLifecycleOwner(const EngineLifecycleOwner &) = delete;
	EngineLifecycleOwner &operator=(const EngineLifecycleOwner &) = delete;
	~EngineLifecycleOwner() { if (this->state != nullptr) openttd_rust_engine_destroy(this->state); }
};
struct EngineName {
	OpenTTDEngineLifecycle *state;
	explicit EngineName(OpenTTDEngineLifecycle *state) noexcept : state(state) {}
	EngineName(const EngineName &) noexcept = default;
	std::string_view View() const
	{
		auto bytes = openttd_rust_engine_name(this->state);
		return {reinterpret_cast<const char *>(bytes.data), bytes.size};
	}
	bool empty() const { return this->View().empty(); }
	void clear() { openttd_rust_engine_set_name(this->state, {nullptr, 0}); }
	EngineName &operator=(std::string_view text)
	{
		openttd_rust_engine_set_name(this->state, {reinterpret_cast<const uint8_t *>(text.data()), text.size()});
		return *this;
	}
	EngineName &operator=(const EngineName &other) { return *this = other.View(); }
	bool operator==(std::string_view text) const { return this->View() == text; }
};
#endif /* RUST_ENGINE_LIFECYCLE_HPP */
