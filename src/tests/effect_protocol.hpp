/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file effect_protocol.hpp Native ABI checks for transient services absent from saves. */
#ifndef EFFECT_PROTOCOL_HPP
#define EFFECT_PROTOCOL_HPP
#include "../rust/effect_ffi.h"
#include <array>
#include <memory>

static inline uint32_t effect_industry_calls, effect_industry_result;
static inline uint32_t OPENTTD_EFFECT_CALL EffectTestIndustry(int32_t, int32_t, uint32_t *tile) noexcept
{
	effect_industry_calls++;
	*tile = 123;
	return effect_industry_result;
}

template <typename Check> void CheckEffectProtocol(Check check)
{
	/* Values and service ordering below are fixtures from pinned effectvehicle.cpp.
	 * Semantic simulation covers persistent state; these exercise the native action
	 * boundary, cdecl leaf, short-circuit RNG, reentry, throw and owner destruction. */
	const OpenTTDEffectLeaves leaves{EffectTestIndustry};
	auto owner = std::unique_ptr<OpenTTDEffectState, decltype(&openttd_rust_effect_destroy)>(openttd_rust_effect_new(), openttd_rust_effect_destroy);
	check(openttd_rust_effect_get(owner.get(), 0) == 0 && openttd_rust_effect_get(owner.get(), 1) == 0);
	openttd_rust_effect_set(owner.get(), 1, 511);
	check(openttd_rust_effect_get(owner.get(), 1) == 255);
	std::array<uint32_t, 12> sprites{3708,3079,3073,3084,2040,3709,3737,3725,1416,4751,2040,2040};
	std::array<uint8_t, 12> progress{7,12,0,1,12,0,0,0,0,0,12,12};
	OpenTTDEffectView v{};
	OpenTTDEffectCursor c{};
	auto step = [&](uint8_t init = 0) { v.sprite_write = 0; return openttd_rust_effect_step(owner.get(), &v, &c, &leaves, init); };
	for (uint8_t subtype = 0; subtype < 12; subtype++) {
		v = {13,-9,21,0,0,99,subtype,1,0}; c = {};
		if (subtype == 0) {
			check(step(1) == 7 && v.sprite_write == 0);
			/* C++ can inspect/mutate an owner while RNG is outside Rust. */
			openttd_rust_effect_set(owner.get(), 0, 65535);
			c.random = UINT32_MAX;
		}
		check(step(1) == 0 && v.sprite == sprites[subtype] && v.progress == progress[subtype] && v.sprite_write == 2);
		check(v.x == 13 && v.y == -9 && v.z == 21);
		check(openttd_rust_effect_get(owner.get(), 0) == (subtype < 8 ? 65535 : 0));
		check(openttd_rust_effect_get(owner.get(), 1) == (subtype < 8 ? 255 : 0));
		check(v.spritenum == (subtype == 9 ? 0 : 99));
	}
	/* Breakdown emits viewport before countdown, including zero-word wrapping. */
	for (uint16_t duration : {uint16_t{0}, uint16_t{1}, uint16_t{65535}}) {
		v = {0,0,0,3740,255,0,6,0,0}; c = {};
		openttd_rust_effect_set(owner.get(), 0, duration);
		check(step() == 2 && v.progress == 0 && v.sprite == 3737 && v.sprite_write == 2);
		check(openttd_rust_effect_get(owner.get(), 0) == duration);
		check(step() == (duration == 1 ? 6 : 0));
		check(openttd_rust_effect_get(owner.get(), 0) == static_cast<uint16_t>(duration - 1));
	}
	/* Historical != terminal and scalar sprite increment retain high-bit wrapping. */
	v = {0,0,0,UINT32_MAX,3,0,5,0,0}; c = {};
	check(step() == 1 && v.sprite == 0 && v.sprite_write == 1);
	v = {0,0,0,3724,3,0,5,0,0}; c = {};
	check(step() == 6 && v.sprite_write == 0);
	/* Bubble bursting changes spritenum before sound, with private state/movement
	 * still pending. z > 180 skips RNG, otherwise Chance16I's rounded threshold. */
	for (uint32_t random : {0U, 682U, 683U, 65536U}) {
		v = {10,20,180,4748,3,1,9,1,0}; c = {};
		openttd_rust_effect_set(owner.get(), 0, 3);
		check(step() == 7 && v.spritenum == 1 && v.z == 180);
		c.random = random;
		const bool burst = random != 683;
		check(step() == (burst ? 3 : 1));
		if (burst) {
			check(v.spritenum == 5 && v.z == 180 && openttd_rust_effect_get(owner.get(), 0) == 3);
			check(step() == 1);
		}
		check(v.z == 181 && openttd_rust_effect_get(owner.get(), 0) == 0);
	}
	v = {10,20,181,4748,3,1,9,1,0}; c = {};
	openttd_rust_effect_set(owner.get(), 0, 3);
	check(step() == 3 && v.spritenum == 5 && v.z == 181);
	/* Throwing C++ sound happens after return: no owner borrow is retained. */
	try { throw 1; } catch (int) { check(openttd_rust_effect_get(owner.get(), 0) == 3); }
	check(step() == 1 && v.z == 182);
	/* Absorption sound precedes map query, then animated-tile precedes private
	 * animation write and movement. Catcher/noncatcher have the same movement. */
	for (uint32_t industry : {0U, 1U, 2U}) {
		v = {10,20,100,4748,3,6,9,1,0}; c = {};
		openttd_rust_effect_set(owner.get(), 0, 77);
		effect_industry_calls = 0; effect_industry_result = industry;
		check(step() == 4 && effect_industry_calls == 0 && v.z == 100);
		const auto action = step();
		check(effect_industry_calls == 1);
		if (industry == 2) {
			check(action == 5 && c.tile == 123 && v.sprite == 4748 && openttd_rust_effect_get(owner.get(), 0) == 77);
			/* Reentry between AddAnimatedTile and continuation is safe; the original
			 * local animation subsequently overwrites this mutation. */
			openttd_rust_effect_set(owner.get(), 0, 9);
			check(step() == 1);
		} else { check(action == 1); }
		check(openttd_rust_effect_get(owner.get(), 0) == 79 && v.sprite == 4758 && v.x == 10 && v.y == 20 && v.z == 100);
	}
	/* Expiry returns deletion without viewport or post-delete owner access. */
	v = {0,0,0,4757,3,5,9,0,0}; c = {};
	openttd_rust_effect_set(owner.get(), 0, 3);
	check(step() == 6 && v.sprite_write == 0);
	owner.reset();
}
#endif /* EFFECT_PROTOCOL_HPP */
