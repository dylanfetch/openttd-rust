/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry-lifecycle-gap.cpp Unavailable NewGRF tile/parent trigger policy. */
#include "stdafx.h"
#include "core/bitmath_func.hpp"
#include "core/enum_type.hpp"
#include "rust/industry_ffi.h"
using TileIndex = uint32_t;
using IndustryGfx = uint16_t;
enum class IndustryRandomTrigger : uint8_t { TileLoop, IndustryTick, CargoReceived };
enum class IndustryAnimationTrigger : uint8_t { ConstructionStageChanged, TileLoop, IndustryTick, CargoReceived, CargoDistributed };
struct Bits {
	uint8_t value = 0;
	void Set(IndustryRandomTrigger trigger) { this->value |= 1U << static_cast<uint8_t>(trigger); }
	void Reset(Bits used) { this->value &= ~used.value; }
	bool Test(IndustryAnimationTrigger trigger) const { return (this->value & (1U << static_cast<uint8_t>(trigger))) != 0; }
};
using IndustryRandomTriggers = Bits;
constexpr int MP_INDUSTRY = 8, CBID_RANDOM_TRIGGER = 1, CBID_INDTILE_ANIMATION_TRIGGER = 0x25, VSG_SCOPE_SELF = 0, VSG_SCOPE_PARENT = 1;
struct Industry;
static Industry *active;
struct Tile { uint8_t triggers, bits; bool belongs; };
struct IndustryTileSpec {
	struct { bool enabled; bool HasSpriteGroups() const { return enabled; } } grf_prop;
	struct { Bits triggers; } animation;
};
static std::array<Tile, 6> tiles;
static std::array<IndustryTileSpec, 6> specs;
static std::vector<std::array<uint32_t, 4>> trace;
static uint32_t rng, self_mask, parent_mask;
static bool IsValidTile(TileIndex tile) { return tile < tiles.size(); }
static bool IsTileType(TileIndex tile, int) { return tiles[tile].belongs; }
static IndustryGfx GetIndustryGfx(TileIndex tile) { return tile; }
static const IndustryTileSpec *GetIndustryTileSpec(IndustryGfx gfx) { return &specs[gfx]; }
static uint32_t Random() { rng = rng * 1664525U + 1013904223U; trace.push_back({0, rng, 0, 0}); return rng; }
static Bits GetIndustryRandomTriggers(TileIndex tile) { return {tiles[tile].triggers}; }
static void SetIndustryRandomTriggers(TileIndex tile, Bits bits) { tiles[tile].triggers = bits.value; trace.push_back({1, tile, bits.value, 0}); }
static uint8_t GetIndustryRandomBits(TileIndex tile) { return tiles[tile].bits; }
static void SetIndustryRandomBits(TileIndex tile, uint8_t bits) { tiles[tile].bits = bits; trace.push_back({2, tile, bits, 0}); }
static void MarkTileDirtyByTile(TileIndex tile) { trace.push_back({3, tile, 0, 0}); }
struct Industry {
	std::array<TileIndex, 6> location{0, 1, 2, 3, 4, 5};
	uint16_t random;
	bool TileBelongsToIndustry(TileIndex tile) const { return tiles[tile].belongs; }
	static Industry *GetByTile(TileIndex) { return active; }
};
struct IndustryTileResolverObject {
	TileIndex tile;
	uint32_t reseed[2]{};
	Bits waiting;
	IndustryTileResolverObject(IndustryGfx, TileIndex tile, Industry *, int) : tile(tile) {}
	void SetWaitingRandomTriggers(Bits bits) { waiting = bits; }
	void ResolveRerandomisation() {
		trace.push_back({4, tile, waiting.value, tiles[tile].triggers});
		reseed[0] = self_mask ^ (tile * 0x0101U);
		reseed[1] = parent_mask;
		/* Resolver may affect live tile state before the source reads it. */
		tiles[tile].bits ^= 0x35;
	}
	Bits GetUsedRandomTriggers() const { return {static_cast<uint8_t>(waiting.value & (1U << (tile % 3)))}; }
};
struct IndustryAnimationBase {
	static void ChangeAnimationFrame(int, const IndustryTileSpec *, Industry *, TileIndex tile, uint32_t random, uint32_t parameter) { trace.push_back({5, tile, random, parameter}); }
};
#include "industry-lifecycle-reference.inc"

static OpenTTDIndustryLifecycleServices Lifecycle()
{
	OpenTTDIndustryLifecycleServices result{};
	result.belongs = [](uint32_t tile, uint16_t) noexcept -> bool { return tiles[tile].belongs; };
	result.randomisation_spec = [](uint32_t tile) noexcept -> OpenTTDIndustryRandomisationSpec { return {static_cast<uint16_t>(tile), specs[tile].grf_prop.enabled}; };
	result.random_triggers = [](uint32_t tile) noexcept -> uint8_t { return tiles[tile].triggers; };
	result.set_random_triggers = [](uint32_t tile, uint8_t bits) noexcept { SetIndustryRandomTriggers(tile, {bits}); };
	result.resolve_reseed = [](uint32_t tile, uint16_t gfx, void *industry, uint8_t waiting) noexcept -> OpenTTDIndustryReseed {
		IndustryTileResolverObject object(gfx, tile, static_cast<Industry *>(industry), 1);
		object.SetWaitingRandomTriggers({waiting});
		object.ResolveRerandomisation();
		return {object.reseed[0], object.reseed[1], object.GetUsedRandomTriggers().value};
	};
	result.random_bits = [](uint32_t tile) noexcept -> uint8_t { return GetIndustryRandomBits(tile); };
	result.set_random_bits = [](uint32_t tile, uint8_t bits) noexcept { SetIndustryRandomBits(tile, bits); };
	result.mark_dirty = [](uint32_t tile) noexcept { MarkTileDirtyByTile(tile); };
	result.animation_spec = [](uint32_t tile) noexcept -> OpenTTDIndustryAnimationSpec { return {&specs[tile], specs[tile].animation.triggers.value}; };
	result.change_frame = [](const void *spec, uint32_t tile, uint32_t random, uint32_t parameter) noexcept { IndustryAnimationBase::ChangeAnimationFrame(0, static_cast<const IndustryTileSpec *>(spec), active, tile, random, parameter); };
	return result;
}
static void Require(bool condition) { if (!condition) std::abort(); }
int main()
{
	const auto lifecycle = Lifecycle();
	OpenTTDIndustryServices services{};
	services.random = []() noexcept -> uint32_t { return Random(); };
	uint cases = 0;
	for (uint32_t seed : {0U, 1U, 0x12345678U, UINT32_MAX}) for (uint32_t mask : {0U, 1U, 0x55U, 0xFFFFU, 0x10000U, 0x12345678U, UINT32_MAX}) {
		for (uint8_t variant = 0; variant < 8; variant++) for (uint8_t trigger = 0; trigger < 5; trigger++) {
			auto setup = [&] {
				rng = seed; self_mask = mask; parent_mask = mask; trace.clear();
				for (uint tile = 0; tile < tiles.size(); tile++) {
					tiles[tile] = {static_cast<uint8_t>(tile * 7 + variant), static_cast<uint8_t>(tile * 23 + variant), tile != 1 && (variant != 7 || tile != 3)};
					specs[tile] = {{((variant >> (tile % 3)) & 1) != 0}, {{static_cast<uint8_t>(tile * 13 + variant)}}};
				}
			};
			Industry industry{}; active = &industry;
			OpenTTDIndustry *owner = openttd_rust_industry_new();
			auto *metadata = openttd_rust_industry_metadata(owner);
			metadata->tile = 0; metadata->width = 3; metadata->height = 2;
			for (uint8_t operation = 0; operation < 4; operation++) {
				if (operation != 2 && trigger >= 3) continue;
				setup(); industry.random = static_cast<uint16_t>(seed); metadata->random = industry.random;
				bool reference_result = true;
				if (operation == 0) TriggerIndustryRandomisation(&industry, static_cast<IndustryRandomTrigger>(trigger));
				if (operation == 1) TriggerIndustryTileRandomisation(2, static_cast<IndustryRandomTrigger>(trigger));
				if (operation == 3) reference_result = trigger == 0 ? TriggerIndustryTileAnimation_ConstructionStageChanged(2, variant & 1) : TriggerIndustryTileAnimation(2, static_cast<IndustryAnimationTrigger>(trigger));
				if (operation == 2) reference_result = TriggerIndustryAnimation(&industry, static_cast<IndustryAnimationTrigger>(trigger));
				const auto reference_trace = trace;
				const auto reference_tiles = tiles;
				const auto reference_rng = rng;
				const auto reference_random = industry.random;
				setup(); metadata->random = static_cast<uint16_t>(seed);
				bool candidate_result = true;
				if (operation == 0) openttd_rust_industry_randomise(&industry, owner, 0, trigger, 3, &services, &lifecycle);
				if (operation == 1) openttd_rust_industry_randomise_tile(2, trigger, &industry, owner, &services, &lifecycle);
				if (operation == 3) candidate_result = openttd_rust_industry_animate_tile(2, trigger, trigger == 0 && (variant & 1) ? 0x100 : 0, &services, &lifecycle);
				if (operation == 2) candidate_result = openttd_rust_industry_animate(owner, 0, trigger, 3, &services, &lifecycle);
				if (trace != reference_trace || rng != reference_rng || metadata->random != reference_random || candidate_result != reference_result) {
					std::fprintf(stderr, "seed %u mask %u variant %u trigger %u operation %u: RNG %u/%u, random %u/%u, result %d/%d, traces %zu/%zu\n", seed, mask, variant, trigger, operation, rng, reference_rng, metadata->random, reference_random, candidate_result, reference_result, trace.size(), reference_trace.size());
					std::abort();
				}
				for (uint tile = 0; tile < tiles.size(); tile++) Require(tiles[tile].triggers == reference_tiles[tile].triggers && tiles[tile].bits == reference_tiles[tile].bits);
				cases++;
			}
			openttd_rust_industry_destroy(owner);
		}
	}
	std::printf("Industry lifecycle absent-NewGRF reference cases: %u\n", cases);
}
