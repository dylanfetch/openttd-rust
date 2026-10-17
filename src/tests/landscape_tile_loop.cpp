/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file landscape_tile_loop.cpp Actual native allocation/layout and live shared reads. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#ifdef WITH_RUST
#include "../rust/tile_type_access.hpp"
#include "../rust/abi_ffi.h"
#include "../tile_map.h"
#include "landscape_protocol.hpp"
#include "ground_protocol.hpp"
#endif
#include "../safeguards.h"

#ifdef WITH_RUST
TEST_CASE("Rust ground enum and consumed-record native calls")
{
	CHECK(GroundProbe::Run());
}

TEST_CASE("Rust landscape scalar native handler calls")
{
	LandscapeProbe::Run([](bool result) { CHECK(result); });
}

TEST_CASE("Rust shared tile-type allocation and all native byte values")
{
	using Base = OpenTTDNativeTileTypeAccess::BaseTile;
	const std::array<size_t, 9> layout{sizeof(Base), alignof(Base), offsetof(Base, type), offsetof(Base, height), offsetof(Base, m2), offsetof(Base, m1), offsetof(Base, m3), offsetof(Base, m4), offsetof(Base, m5)};
	for (uint8_t item = 0; item < layout.size(); item++) CHECK(openttd_rust_abi_layout(783, item) == layout[item]);
	CHECK(openttd_rust_abi_layout(783, layout.size()) == SIZE_MAX);
	using Extended = OpenTTDNativeTileTypeAccess::ExtendedTile;
	const std::array<size_t, 5> ext_layout{sizeof(Extended), alignof(Extended), offsetof(Extended, m6), offsetof(Extended, m7), offsetof(Extended, m8)};
	for (uint8_t item = 0; item < ext_layout.size(); item++) CHECK(openttd_rust_abi_layout(804, item) == ext_layout[item]);
	CHECK(openttd_rust_abi_layout(804, ext_layout.size()) == SIZE_MAX);
	const std::array<size_t, 5> view_layout{sizeof(OpenTTDMapReadView), alignof(OpenTTDMapReadView), offsetof(OpenTTDMapReadView, base), offsetof(OpenTTDMapReadView, count), offsetof(OpenTTDMapReadView, extended)};
	for (uint8_t item = 0; item < view_layout.size(); item++) CHECK(openttd_rust_abi_layout(782, item) == view_layout[item]);
	CHECK(openttd_rust_abi_layout(782, view_layout.size()) == SIZE_MAX);
	Map::Allocate(128, 64);
	const auto allocation = GetRustTileTypeAllocation();
	const auto &shared = GetRustSharedServices();
	CHECK(allocation.count == Map::Size());
	CHECK(allocation.base != nullptr);
	CHECK(allocation.extended == &Tile(TileIndex{0}).m6());
	for (const TileIndex tile : {TileIndex{0}, TileIndex{Map::Size() - 1}}) {
		for (uint byte = 0; byte < 256; byte++) {
			Tile(tile).type() = byte;
			CHECK(openttd_rust_shared_tile_type(allocation, shared.tile_type, tile.base()) == static_cast<uint8_t>(GetTileType(tile)));
			CHECK(openttd_rust_shared_tile_type({nullptr, 0, nullptr}, shared.tile_type, tile.base()) == static_cast<uint8_t>(GetTileType(tile)));
		}
	}
	/* Real native writes between accesses must be observed without a new snapshot. */
	SetTileType(TileIndex{1}, MP_ROAD);
	CHECK(openttd_rust_shared_tile_type(allocation, shared.tile_type, 1) == MP_ROAD);
	SetTileType(TileIndex{1}, MP_CLEAR);
	CHECK(openttd_rust_shared_tile_type(allocation, shared.tile_type, 1) == MP_CLEAR);
	/* Reset between reader entries; the next entry acquires a live allocation. */
	Map::Allocate(64, 64);
	const auto next = GetRustTileTypeAllocation();
	CHECK(next.count == Map::Size());
	CHECK(next.count != allocation.count);
	SetTileType(TileIndex{Map::Size() - 1}, MP_INDUSTRY);
	CHECK(openttd_rust_shared_tile_type(next, shared.tile_type, Map::Size() - 1) == MP_INDUSTRY);
	Map::Allocate(64, 64);
}

TEST_CASE("Rust shared tile-type out-of-range preserves callback policy")
{
	Map::Allocate(64, 64);
	const auto allocation = GetRustTileTypeAllocation();
	/* The original native accessor stays the production oracle. This fixture
	 * records fallback rather than executing source UB with assertions disabled. */
	static uint calls;
	static uint32_t observed;
	calls = 0;
	auto callback = [](uint32_t tile) noexcept -> uint8_t { calls++; observed = tile; return 231; };
	CHECK(openttd_rust_shared_tile_type(allocation, callback, allocation.count) == 231);
	CHECK(calls == 1);
	CHECK(observed == allocation.count);
	CHECK(openttd_rust_shared_tile_type(allocation, callback, UINT32_MAX) == 231);
	CHECK(calls == 2);
	CHECK(observed == UINT32_MAX);
	CHECK(openttd_rust_shared_tile_type({nullptr, 0, nullptr}, callback, 0) == 231);
	CHECK(calls == 3);
	CHECK(observed == 0);
}
#endif
