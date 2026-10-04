/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rail_yapf.cpp Finite-limit and heap ordering against unchanged YAPF. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#ifdef WITH_RUST
#include "rail_yapf_protocol.hpp"
#include "../rust/abi_ffi.h"
#include "../train.h"
#include "../pathfinder/yapf/yapf_base.hpp"
#include "../pathfinder/yapf/yapf_node.hpp"
#include "../pathfinder/yapf/nodelist.hpp"
namespace RailYapfReference {
struct Key {
	uint32_t tile;
	uint8_t td;
	int CalcHash() const { return (this->tile << 4) | this->td; }
	bool operator==(const Key &other) const { return this->tile == other.tile && this->td == other.td; }
};
struct Node : CYapfNodeT<Key, Node> {
	void Set(uint32_t tile, uint8_t td, Node *parent)
	{
		this->key = {tile, td}; this->hash_next = nullptr; this->parent = parent;
		this->cost = this->estimate = 0;
	}
};
struct Follower {};
class Search;
struct Types {
	using Tpf = Search;
	using TrackFollower = Follower;
	using NodeList = ::NodeList<Node, 6, 6>;
	using VehicleType = Train;
};
class Search : public CYapfBaseT<Types> {
public:
	uint32_t goal, curve;
	Search(uint32_t limit, uint32_t goal, uint32_t curve = 29) : goal(goal), curve(curve)
	{
		this->max_search_nodes = limit;
		Node &node = this->CreateNewNode(); node.Set(0, 0, nullptr); this->AddStartupNode(node);
	}
	bool PfNodeCacheFetch(Node &) { return false; }
	void PfFollowNode(Node &parent)
	{
		RailYapfProbe::Follow(nullptr, parent.key.tile, parent.key.td, 1, 0, 0);
		for (uint8_t td : {0, 2}) {
			Node &node = this->CreateNewNode(); node.Set(parent.key.tile + 1, td, &parent);
			this->AddNewNode(node, Follower{});
		}
	}
	bool PfCalcCost(Node &node, const Follower *)
	{
		const uint8_t td = node.key.td;
		node.cost = node.parent->cost + (td == 0 ? 100 : 71);
		if (td != 0 || node.parent->key.td != 0) node.cost += this->curve;
		if (td == 0 && RailYapfProbe::hill && node.key.tile == 2) node.cost += 50;
		RailYapfProbe::Follow(nullptr, node.key.tile, td, 1, 0, 0);
		return true;
	}
	bool PfCalcEstimate(Node &node) { node.estimate = node.cost + 100 * (this->goal - node.key.tile); return true; }
	bool PfDetectDestination(Node &node) { return node.key.tile == this->goal; }
	char TransportTypeChar() const { return 'r'; }
	uint32_t Calcs() const { return this->stats_cost_calcs; }
	uint8_t Direction()
	{
		Node *node = this->GetBestNode(), *next = nullptr;
		while (node != nullptr && node->parent != nullptr) { next = node; node = node->parent; }
		return next == nullptr ? UINT8_MAX : next->key.td;
	}
};
}
TEST_CASE("Rail YAPF finite limit and heap ordering against unchanged reference")
{
	/* Loaded games clamp max_search_nodes to 500; the rail corpus closes at most
	 * ten nodes. Forced choices isolate one-tile segments and equal priorities.
	 * This compares the common C++ base/heap, not full C++ rail cost semantics,
	 * physical tracks, train callbacks or reservation in a reachable world. */
	for (uint32_t goal : {2, 5, 9}) for (uint32_t limit : {0, 1, 2, 7}) {
		RailYapfProbe::goal = goal;
		const auto result = RailYapfProbe::Run(limit);
		const auto trace = RailYapfProbe::follows;
		RailYapfProbe::follows.clear();
		RailYapfReference::Search reference{limit, goal};
		const bool found = reference.FindPath(nullptr);
		CHECK(result.found == static_cast<uint8_t>(found));
		CHECK(result.td == reference.Direction());
		CHECK(trace == RailYapfProbe::follows);
		CHECK(RailYapfProbe::stats[0] == static_cast<uint32_t>(reference.num_steps));
		CHECK(RailYapfProbe::stats[1] == static_cast<uint32_t>(reference.nodes.OpenCount()));
		CHECK(RailYapfProbe::stats[2] == static_cast<uint32_t>(reference.nodes.ClosedCount()));
		CHECK(RailYapfProbe::stats[4] == reference.Calcs());
		CHECK(RailYapfProbe::limits == static_cast<uint32_t>(!found && limit != 0));
		if (found) CHECK(RailYapfProbe::stats[6] == static_cast<uint32_t>(reference.GetBestNode()->cost));
	}
}
TEST_CASE("Rail YAPF live global cache invalidation after tile mutation")
{
	RailYapfProbe::goal = 3;
	openttd_rust_rail_invalidate();
	CHECK(RailYapfProbe::Run(0, 100, true).found == 1);
	CHECK(RailYapfProbe::Run(0, 100, true).found == 1);
	CHECK(RailYapfProbe::stats[3] > 0);
	CHECK(RailYapfProbe::stats[4] == 0);
	/* Same bank/settings, but the bundled tile service now reports an uphill
	 * tile. Notify invalidation must discard costs from before the mutation. */
	RailYapfProbe::hill = true;
	openttd_rust_rail_invalidate();
	CHECK(RailYapfProbe::Run(0, 100, true).found == 1);
	CHECK(RailYapfProbe::flushes == 1);
	CHECK(RailYapfProbe::stats[4] > 0);
	RailYapfReference::Search reference{0, 3, 100};
	CHECK(reference.FindPath(nullptr));
	CHECK(RailYapfProbe::stats[6] == static_cast<uint32_t>(reference.GetBestNode()->cost));
	CHECK(RailYapfProbe::stats[6] == 350);
	RailYapfProbe::hill = false;
	openttd_rust_rail_invalidate();
}
TEST_CASE("Rail YAPF opposing PBS signals restored in insertion order on rollback")
{
	/* The real rail corpus reaches rollback but no opposing green PBS signal.
	 * Reuse the scalar graph to expose ordered red/unreserve/green writes. */
	RailYapfProbe::goal = 3; RailYapfProbe::rollback = true;
	openttd_rust_rail_invalidate();
	CHECK(RailYapfProbe::Run(0, 100, true, true).found == 1);
	const std::vector<uint32_t> expected{0x3001, 0x3085, 0x2001, 0x2085, 0x1001, 0x3002, 0x2002, 0x3086, 0x2086};
	CHECK(RailYapfProbe::writes == expected);
	CHECK(RailYapfProbe::red == 2);
	CHECK(RailYapfProbe::restored == 2);
	RailYapfProbe::rollback = false;
	openttd_rust_rail_invalidate();
}
TEST_CASE("Rail YAPF Rust ABI layouts")
{
	const auto Layout = [](uint8_t kind, const char *, std::initializer_list<size_t> fields) {
		uint8_t index = 0;
		for (size_t field : fields) CHECK(openttd_rust_abi_layout(kind, index++) == field);
	};
	Layout(90, "OpenTTDRailSettings", {sizeof(OpenTTDRailSettings), alignof(OpenTTDRailSettings), offsetof(OpenTTDRailSettings, max_nodes), offsetof(OpenTTDRailSettings, firstred), offsetof(OpenTTDRailSettings, firstred_exit), offsetof(OpenTTDRailSettings, lastred), offsetof(OpenTTDRailSettings, lastred_exit), offsetof(OpenTTDRailSettings, station), offsetof(OpenTTDRailSettings, slope), offsetof(OpenTTDRailSettings, curve45), offsetof(OpenTTDRailSettings, curve90), offsetof(OpenTTDRailSettings, depot_reverse), offsetof(OpenTTDRailSettings, crossing), offsetof(OpenTTDRailSettings, lookahead), offsetof(OpenTTDRailSettings, p0), offsetof(OpenTTDRailSettings, p1), offsetof(OpenTTDRailSettings, p2), offsetof(OpenTTDRailSettings, pbs_cross), offsetof(OpenTTDRailSettings, pbs_station), offsetof(OpenTTDRailSettings, pbs_back), offsetof(OpenTTDRailSettings, doubleslip), offsetof(OpenTTDRailSettings, longer), offsetof(OpenTTDRailSettings, longer_tile), offsetof(OpenTTDRailSettings, shorter), offsetof(OpenTTDRailSettings, shorter_tile), offsetof(OpenTTDRailSettings, firstred_eol)});
	Layout(91, "OpenTTDRailTile", {sizeof(OpenTTDRailTile), alignof(OpenTTDRailTile), offsetof(OpenTTDRailTile, flags), offsetof(OpenTTDRailTile, other_end), offsetof(OpenTTDRailTile, station), offsetof(OpenTTDRailTile, railtype), offsetof(OpenTTDRailTile, tracks), offsetof(OpenTTDRailTile, reserved), offsetof(OpenTTDRailTile, station_track), offsetof(OpenTTDRailTile, tunnel_dir), offsetof(OpenTTDRailTile, uphill), offsetof(OpenTTDRailTile, flat_ramp), offsetof(OpenTTDRailTile, signal_along), offsetof(OpenTTDRailTile, signal_against), offsetof(OpenTTDRailTile, signal_green), offsetof(OpenTTDRailTile, signal_type), offsetof(OpenTTDRailTile, oneway)});
	Layout(92, "OpenTTDRailFollow", {sizeof(OpenTTDRailFollow), alignof(OpenTTDRailFollow), offsetof(OpenTTDRailFollow, tile), offsetof(OpenTTDRailFollow, skipped), offsetof(OpenTTDRailFollow, min_speed), offsetof(OpenTTDRailFollow, max_speed), offsetof(OpenTTDRailFollow, dirs), offsetof(OpenTTDRailFollow, followed), offsetof(OpenTTDRailFollow, error), offsetof(OpenTTDRailFollow, station)});
	Layout(93, "OpenTTDRailLeaves", {sizeof(OpenTTDRailLeaves), alignof(OpenTTDRailLeaves), offsetof(OpenTTDRailLeaves, train), offsetof(OpenTTDRailLeaves, tile), offsetof(OpenTTDRailLeaves, follow), offsetof(OpenTTDRailLeaves, safe), offsetof(OpenTTDRailLeaves, free), offsetof(OpenTTDRailLeaves, compatible_station), offsetof(OpenTTDRailLeaves, platform_length), offsetof(OpenTTDRailLeaves, closest_station), offsetof(OpenTTDRailLeaves, destination_dirs), offsetof(OpenTTDRailLeaves, origin), offsetof(OpenTTDRailLeaves, write), offsetof(OpenTTDRailLeaves, output), offsetof(OpenTTDRailLeaves, debug)});
	Layout(94, "OpenTTDRailInput", {sizeof(OpenTTDRailInput), alignof(OpenTTDRailInput), offsetof(OpenTTDRailInput, context), offsetof(OpenTTDRailInput, settings), offsetof(OpenTTDRailInput, map_x), offsetof(OpenTTDRailInput, tile), offsetof(OpenTTDRailInput, max_cost), offsetof(OpenTTDRailInput, desync), offsetof(OpenTTDRailInput, kind), offsetof(OpenTTDRailInput, td), offsetof(OpenTTDRailInput, override_railtype), offsetof(OpenTTDRailInput, forbid90), offsetof(OpenTTDRailInput, reserve)});
	Layout(95, "OpenTTDRailStep", {sizeof(OpenTTDRailStep), alignof(OpenTTDRailStep), offsetof(OpenTTDRailStep, tile), offsetof(OpenTTDRailStep, destination), offsetof(OpenTTDRailStep, target_tile), offsetof(OpenTTDRailStep, best_length), offsetof(OpenTTDRailStep, action), offsetof(OpenTTDRailStep, td), offsetof(OpenTTDRailStep, found), offsetof(OpenTTDRailStep, reverse), offsetof(OpenTTDRailStep, value), offsetof(OpenTTDRailStep, target_td), offsetof(OpenTTDRailStep, target_okay)});
	Layout(97, "OpenTTDRailTrain", {sizeof(OpenTTDRailTrain), alignof(OpenTTDRailTrain), offsetof(OpenTTDRailTrain, compatible), offsetof(OpenTTDRailTrain, all_compatible), offsetof(OpenTTDRailTrain, tile), offsetof(OpenTTDRailTrain, rear_tile), offsetof(OpenTTDRailTrain, virtual_tile), offsetof(OpenTTDRailTrain, rear_virtual_tile), offsetof(OpenTTDRailTrain, dest_tile), offsetof(OpenTTDRailTrain, length), offsetof(OpenTTDRailTrain, speed), offsetof(OpenTTDRailTrain, order_destination), offsetof(OpenTTDRailTrain, td), offsetof(OpenTTDRailTrain, rear_td), offsetof(OpenTTDRailTrain, wormhole), offsetof(OpenTTDRailTrain, rear_wormhole), offsetof(OpenTTDRailTrain, order), offsetof(OpenTTDRailTrain, nearest_depot), offsetof(OpenTTDRailTrain, complex_waypoint)});
}
#endif
