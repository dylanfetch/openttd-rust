/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file signals-console.cpp Identical real-map inputs and pending-set inspection. */
/* signal.cpp is included by the scenario adapter builder before this file, so
 * portable work sets are visible without changing the pinned source checkout. */
#include "console_internal.h"
#include "console_func.h"
#include "command_func.h"
#include "company_func.h"
#include "rail_cmd.h"
#include "core/backup_type.hpp"
#include "core/random_func.hpp"
#include "depot_base.h"
#include "station_base.h"
#include "tunnelbridge_map.h"
#include "company_base.h"
#include "clear_map.h"
#include "../../../src/rust/signal_ffi.h"

#ifdef WITH_RUST
/* Measurement belongs only to this relinked fixture. The ordinary native query
 * and scalar accessor remain unchanged; count actual short-circuited reads. */
static uint64_t *_signal_occupancy_reads = nullptr;
extern "C" uint8_t __real_openttd_rust_train_state_get_track(const OpenTTDTrainState *);
extern "C" uint8_t __wrap_openttd_rust_train_state_get_track(const OpenTTDTrainState *state)
{
	if (_signal_occupancy_reads != nullptr) ++*_signal_occupancy_reads;
	return __real_openttd_rust_train_state_get_track(state);
}
static bool SignalCountTrainOnTile(uint32_t tile) noexcept
{
	uint64_t reads = 0;
	Backup<uint64_t *> counter(_signal_occupancy_reads, &reads);
	bool result = SignalTrainOnTile(tile);
	counter.Restore();
	openttd_rust_signal_count_occupancy(reads);
	return result;
}
static bool SignalCountTrainOnTracks(uint32_t tile, uint8_t tracks) noexcept
{
	uint64_t reads = 0;
	Backup<uint64_t *> counter(_signal_occupancy_reads, &reads);
	bool result = SignalTrainOnTracks(tile, tracks);
	counter.Restore();
	openttd_rust_signal_count_occupancy(reads);
	return result;
}
#endif

extern Company *DoStartupNewCompany(bool is_ai, CompanyID company);

[[noreturn]] static void SignalFixtureAbort(int line)
{
	fmt::print(stderr, "SIGNALS setup-failure {}\n", line);
	std::fflush(stderr);
	std::abort();
}
static void SignalCost(const char *name, const CommandCost &cost)
{
	fmt::print(stderr, "SIGNALS cost {} {} {} {}\n", name, cost.Succeeded(), cost.GetCost(), cost.GetErrorMessage());
	if (cost.Failed()) SignalFixtureAbort(__LINE__);
}
#ifndef WITH_RUST
template <typename Direction, uint N>
static void PrintPending(SmallSet<Direction, N> &set)
{
	std::array<std::pair<TileIndex, Direction>, N> buffer;
	uint count = set.Items();
	/* Inspect by drain/restore in the same translation unit. Add never dedups;
	 * the overflow flag is unchanged and restoration preserves the active order. */
	for (uint i = count; i > 0; --i) set.Get(&buffer[i - 1].first, &buffer[i - 1].second);
	for (uint i = 0; i < count; ++i) {
		fmt::print(stderr, " {}:{}", buffer[i].first.base(), static_cast<uint8_t>(buffer[i].second));
		set.Add(buffer[i].first, buffer[i].second);
	}
}
#endif
static void SignalPending(const char *name)
{
#ifdef WITH_RUST
	OpenTTDSignalSnapshot snapshot;
	openttd_rust_signal_inspect(&snapshot);
	fmt::print(stderr, "SIGNALS pending {} {} {} {} {} {} {} {}", name, snapshot.tbu_count, snapshot.tbd_count, snapshot.global_count, snapshot.overflow[0], snapshot.overflow[1], snapshot.overflow[2], snapshot.last_owner);
	for (auto [items, count] : {std::pair{snapshot.tbu, snapshot.tbu_count}, std::pair{snapshot.tbd, snapshot.tbd_count}, std::pair{snapshot.global, snapshot.global_count}}) {
		fmt::print(stderr, " |");
		for (uint i = 0; i < count; ++i) fmt::print(stderr, " {}:{}", items[i].tile, items[i].dir);
	}
#else
	fmt::print(stderr, "SIGNALS pending {} {} {} {} {} {} {} {} |", name, _tbuset.Items(), _tbdset.Items(), _globset.Items(), _tbuset.Overflowed(), _tbdset.Overflowed(), _globset.Overflowed(), _last_owner.base());
	PrintPending(_tbuset);
	fmt::print(stderr, " |");
	PrintPending(_tbdset);
	fmt::print(stderr, " |");
	PrintPending(_globset);
#endif
	fmt::print(stderr, "\n");
}
static std::vector<std::pair<TileIndex, Track>> SignalTracks(Owner owner)
{
	std::vector<std::pair<TileIndex, Track>> tracks;
	for (TileIndex tile(0); tile < Map::Size(); ++tile) {
		if (!IsTileType(tile, MP_RAILWAY) || GetTileOwner(tile) != owner || !HasSignals(tile)) continue;
		for (Track track = TRACK_BEGIN; track < TRACK_END; ++track) if (HasTrack(tile, track) && HasSignalOnTrack(tile, track)) tracks.emplace_back(tile, track);
	}
	return tracks;
}
static void SignalClassify(const std::vector<std::pair<TileIndex, Track>> &tracks, Owner owner, const char *name)
{
	uint states[3] = {};
	for (auto [tile, track] : tracks) ++states[UpdateSignalsOnSegment(tile, TrackdirToExitdir(TrackToTrackdir(track)), owner)];
	fmt::print(stderr, "SIGNALS {} {} {} {}\n", name, states[0], states[1], states[2]);
}
#ifndef WITH_RUST
template <typename Direction, uint N>
static OpenTTDSignalSetTrace SignalSetTrace(std::string_view name)
{
	SmallSet<Direction, N> set(name);
	Direction zero = static_cast<Direction>(0), invalid = static_cast<Direction>(255);
	set.Add(TileIndex(1), zero);
	set.Add(TileIndex(2), invalid);
	set.Add(TileIndex(1), zero);
	set.Add(TileIndex(1), static_cast<Direction>(2));
	set.Remove(TileIndex(1), zero);
	OpenTTDSignalSetTrace r{};
	r.duplicate_count = set.Items();
	TileIndex tile;
	Direction dir;
	for (auto &item : r.duplicate_pop) { set.Get(&tile, &dir); item = {tile.base(), static_cast<uint8_t>(dir)}; }
	set.Reset();
	for (uint i = 0; i < N; ++i) set.Add(TileIndex(i), static_cast<Direction>(i & 3));
	r.full_failed = !set.Add(TileIndex(UINT32_MAX), invalid);
	r.overflow_full = set.Overflowed();
	set.Remove(TileIndex(0), zero);
	/* Drain/restore exposes the first element without accessing private data. */
	std::array<std::pair<TileIndex, Direction>, N> buffer;
	uint count = set.Items();
	for (uint i = count; i > 0; --i) set.Get(&buffer[i - 1].first, &buffer[i - 1].second);
	r.first_after_remove = {buffer[0].first.base(), static_cast<uint8_t>(buffer[0].second)};
	for (uint i = 0; i < count; ++i) set.Add(buffer[i].first, buffer[i].second);
	r.refill_succeeded = set.Add(TileIndex(UINT32_MAX), invalid);
	set.Get(&tile, &dir);
	r.refill_pop = {tile.base(), static_cast<uint8_t>(dir)};
	while (set.Get(&tile, &dir)) { }
	r.drained_count = set.Items();
	r.overflow_drained = set.Overflowed();
	/* Sticky overflow must not reject insertion into the drained set. */
	r.refill_succeeded &= set.Add(TileIndex(UINT32_MAX), invalid);
	set.Reset();
	r.overflow_reset = set.Overflowed();
	r.refill_succeeded &= set.Items() == 0;
	return r;
}
#endif
static void SignalSetProbe()
{
	OpenTTDSignalSetTrace traces[3];
#ifdef WITH_RUST
	openttd_rust_signal_workset_probe(&_signal_services, traces);
#else
	traces[0] = SignalSetTrace<Trackdir, 64>("_tbuset");
	traces[1] = SignalSetTrace<DiagDirection, 256>("_tbdset");
	traces[2] = SignalSetTrace<DiagDirection, 128>("_globset");
#endif
	for (uint i = 0; i < 3; ++i) {
		auto &r = traces[i];
		fmt::print(stderr, "SIGNALS sets {} {}", std::array{64, 256, 128}[i], r.duplicate_count);
		for (auto &item : r.duplicate_pop) fmt::print(stderr, " {}:{}", item.tile, item.dir);
		fmt::print(stderr, " {} {} {}:{} {} {}:{} {} {} {}\n", r.full_failed, r.overflow_full, r.first_after_remove.tile, r.first_after_remove.dir, r.refill_succeeded, r.refill_pop.tile, r.refill_pop.dir, r.drained_count, r.overflow_drained, r.overflow_reset);
	}
}
static void SignalOverflowLayout(Owner owner)
{
	/* Declared terrain input: flatten an unused clear/tree rectangle. The rail
	 * and signal geometry is built through real commands, never raw rail bits. */
	constexpr uint N = 20;
	TileIndex origin = INVALID_TILE;
	for (uint y = 2; y + N + 2 < Map::SizeY() && origin == INVALID_TILE; ++y) {
		for (uint x = 2; x + N + 2 < Map::SizeX(); ++x) {
			bool clear = true;
			for (uint dy = 0; dy < N + 2 && clear; ++dy) for (uint dx = 0; dx < N + 2; ++dx) {
				TileType type = GetTileType(TileXY(x + dx, y + dy));
				if (type != MP_CLEAR && type != MP_TREES) { clear = false; break; }
			}
			if (clear) { origin = TileXY(x, y); break; }
		}
	}
	if (origin == INVALID_TILE) SignalFixtureAbort(__LINE__);
	for (uint y = 0; y < N + 2; ++y) for (uint x = 0; x < N + 2; ++x) {
		TileIndex tile = origin + TileDiffXY(x, y);
		SetTileHeight(tile, 1);
		MakeClear(tile, CLEAR_GRASS, 3);
	}
	Company *c = Company::Get(owner);
#ifdef WITH_RUST
	c->Finances().money = Money(1000000000);
#else
	c->money = Money(1000000000);
#endif
	std::vector<std::pair<TileIndex, Track>> boundary;
	for (uint y = 1; y <= N; ++y) for (uint x = 1; x <= N; ++x) {
		TileIndex tile = origin + TileDiffXY(x, y);
		bool edge_x = x == 1 || x == N, edge_y = y == 1 || y == N;
		if (edge_x && edge_y) continue;
		if (!edge_y) SignalCost("rail-x", Command<CMD_BUILD_SINGLE_RAIL>::Do(DoCommandFlag::Execute, tile, RAILTYPE_RAIL, TRACK_X, false));
		if (!edge_x) SignalCost("rail-y", Command<CMD_BUILD_SINGLE_RAIL>::Do(DoCommandFlag::Execute, tile, RAILTYPE_RAIL, TRACK_Y, false));
		if (edge_x || edge_y) boundary.emplace_back(tile, edge_x ? TRACK_X : TRACK_Y);
	}
	for (auto [tile, track] : boundary) SignalCost("boundary", Command<CMD_BUILD_SINGLE_SIGNAL>::Do(DoCommandFlag::Execute, tile, track, SIGTYPE_BLOCK, SIG_ELECTRIC, false, false, false, SIGTYPE_BLOCK, SIGTYPE_PBS_ONEWAY, 0, 0));
	UpdateSignalsInBuffer();
	TileIndex center = origin + TileDiffXY(2, 2);
	fmt::print(stderr, "SIGNALS overflow {}\n", static_cast<uint8_t>(UpdateSignalsOnSegment(center, DIAGDIR_NE, owner)));
	SignalPending("overflow-reset");
	for (uint i = 0; i < 12; ++i) SignalCost("open-boundary", Command<CMD_REMOVE_SINGLE_SIGNAL>::Do(DoCommandFlag::Execute, boundary[i].first, boundary[i].second));
	UpdateSignalsInBuffer();
	fmt::print(stderr, "SIGNALS follow-up {}\n", static_cast<uint8_t>(UpdateSignalsOnSegment(center, DIAGDIR_NE, owner)));
	SignalPending("overflow-follow-up");
	/* Command-built exit/combo geometry; declared live signal states isolate
	 * no-green and multiple-green exit rules before ordinary propagation. */
	for (uint i = 12; i < boundary.size(); ++i) {
		auto [tile, track] = boundary[i];
		SignalType kind = i == 12 ? SIGTYPE_COMBO : SIGTYPE_EXIT;
		SignalCost("exit-combo", Command<CMD_BUILD_SINGLE_SIGNAL>::Do(DoCommandFlag::Execute, tile, track, kind, SIG_ELECTRIC, true, false, false, SIGTYPE_BLOCK, SIGTYPE_PBS_ONEWAY, 0, SignalOnTrack(track)));
	}
	UpdateSignalsInBuffer();
	for (SignalState state : {SIGNAL_STATE_RED, SIGNAL_STATE_GREEN}) {
		for (uint i = 12; i < boundary.size(); ++i) {
			auto [tile, track] = boundary[i];
			Trackdir td = TrackToTrackdir(track);
			SetSignalStateByTrackdir(tile, td, state);
			SetSignalStateByTrackdir(tile, ReverseTrackdir(td), state);
		}
		fmt::print(stderr, "SIGNALS combo-input {} {}\n", state, static_cast<uint8_t>(UpdateSignalsOnSegment(center, DIAGDIR_NE, owner)));
	}
	CompanyID other = CompanyID::Invalid();
	for (const Company *company : Company::Iterate()) if (company->index != owner) { other = company->index; break; }
	if (other == CompanyID::Invalid()) {
		Company *company = DoStartupNewCompany(false, CompanyID::Invalid());
		if (company == nullptr) SignalFixtureAbort(__LINE__);
		other = company->index;
	}
	if (other == CompanyID::Invalid()) SignalFixtureAbort(__LINE__);
	TileIndex foreign = origin + TileDiffXY(0, 2);
	Backup<CompanyID> foreign_company(_current_company, other);
	SignalCost("foreign-rail", Command<CMD_BUILD_SINGLE_RAIL>::Do(DoCommandFlag::Execute, foreign, RAILTYPE_RAIL, TRACK_X, false));
	UpdateSignalsInBuffer();
	foreign_company.Restore();
	fmt::print(stderr, "SIGNALS owner-barrier {}\n", static_cast<uint8_t>(UpdateSignalsOnSegment(foreign, DIAGDIR_SW, owner)));
	/* Empty perimeter and its empty neighbour exercise the buffer continue. */
	AddSideToSignalBuffer(origin, DIAGDIR_SE, owner);
	UpdateSignalsInBuffer();
	SignalPending("empty-neighbour");

}
static bool SignalsScenario(std::span<std::string_view> args)
{
	if (args.size() != 2) return false;
	if (args[1] == "sets") { SignalSetProbe(); return true; }
	UpdateSignalsInBuffer();
	if (args[1] == "inspect") { SignalPending("reload"); return true; }
	Train *train = nullptr;
	for (Vehicle *v : Vehicle::Iterate()) if (v->type == VEH_TRAIN && Train::From(v)->IsFrontEngine()) { train = Train::From(v); break; }
	if (train == nullptr) SignalFixtureAbort(__LINE__);
	Owner owner = train->owner;
	Backup<CompanyID> company(_current_company, owner);
	if (args[1] == "overflow") { SignalOverflowLayout(owner); company.Restore(); return true; }
	auto tracks = SignalTracks(owner);
	if (args[1] == "inspect-live") { SignalClassify(tracks, owner, "live-states"); SignalPending("live"); company.Restore(); return true; }
	if (tracks.size() < 6) SignalFixtureAbort(__LINE__);
	auto [tile, track] = tracks.front();
	SavedRandomSeeds before, after;
	SaveRandomSeeds(&before);
	SignalPending("initial");
	AddSideToSignalBuffer(tile, DIAGDIR_NE, owner);
	AddSideToSignalBuffer(tile, DIAGDIR_NE, owner);
	AddSideToSignalBuffer(tile, DIAGDIR_SW, owner);
	SignalPending("duplicates");
	UpdateSignalsInBuffer();
	SignalPending("public-flush");
	for (uint i = 0; i < 31; ++i) AddTrackToSignalBuffer(tile, track, owner);
	SignalPending("below-threshold");
	AddTrackToSignalBuffer(tile, track, owner);
	SignalPending("forced-flush");
	SetSignalsOnBothDir(tile, track, owner);
	SignalPending("private-flush");
	SignalClassify(tracks, owner, "states");
	SaveRandomSeeds(&after);
	bool same_rng = before.random.state[0] == after.random.state[0] && before.random.state[1] == after.random.state[1] && before.interactive_random.state[0] == after.interactive_random.state[0] && before.interactive_random.state[1] == after.interactive_random.state[1];
	fmt::print(stderr, "SIGNALS rng {}\n", same_rng);
	if (!same_rng) SignalFixtureAbort(__LINE__);
	/* Real signal build/remove transactions on the existing rail. */
	SignalCost("remove", Command<CMD_REMOVE_SINGLE_SIGNAL>::Do(DoCommandFlag::Execute, tile, track));
	SignalCost("build", Command<CMD_BUILD_SINGLE_SIGNAL>::Do(DoCommandFlag::Execute, tile, track, SIGTYPE_BLOCK, SIG_ELECTRIC, false, false, false, SIGTYPE_BLOCK, SIGTYPE_PBS_ONEWAY, 0, 0));
	for (uint i = 0; i < 60 && i < tracks.size(); ++i) {
		auto [t, tr] = tracks[i];
		SignalType kind = static_cast<SignalType>(i % 6);
		SignalCost("convert", Command<CMD_BUILD_SINGLE_SIGNAL>::Do(DoCommandFlag::Execute, t, tr, kind, SIG_ELECTRIC, true, false, false, SIGTYPE_BLOCK, SIGTYPE_PBS_ONEWAY, 0, IsPbsSignal(kind) ? KillFirstBit(SignalOnTrack(tr)) : SignalOnTrack(tr)));
	}
	UpdateSignalsInBuffer();
	SignalPending("commands");
	SignalClassify(tracks, owner, "command-states");
	IConsoleCmdExec("schedule on-next-calendar-month signals-live.scr");
	company.Restore();
	return true;
}
extern "C" void __real__Z22IConsoleStdLibRegisterv();
extern "C" void __wrap__Z22IConsoleStdLibRegisterv()
{
	__real__Z22IConsoleStdLibRegisterv();
	IConsole::CmdRegister("signals_scenario", SignalsScenario);
}
