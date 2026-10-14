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
		for (Track track = TRACK_BEGIN; track < TRACK_END; ++track) if (HasSignalOnTrack(tile, track)) tracks.emplace_back(tile, track);
	}
	return tracks;
}
static bool SignalsScenario([[maybe_unused]] uint8_t argc, std::span<std::string_view> args)
{
	if (argc != 2) return false;
	UpdateSignalsInBuffer();
	if (args[1] == "inspect") { SignalPending("reload"); return true; }
	Train *train = nullptr;
	for (Vehicle *v : Vehicle::Iterate()) if (v->type == VEH_TRAIN && Train::From(v)->IsFrontEngine()) { train = Train::From(v); break; }
	if (train == nullptr) SignalFixtureAbort(__LINE__);
	Owner owner = train->owner;
	Backup<CompanyID> company(_current_company, owner);
	auto tracks = SignalTracks(owner);
	if (tracks.size() < 12) SignalFixtureAbort(__LINE__);
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
	uint states[3] = {};
	for (auto [signal_tile, signal_track] : tracks) {
		DiagDirection side = TrackdirToExitdir(TrackToTrackdir(signal_track));
		SigSegState state = UpdateSignalsOnSegment(signal_tile, side, owner);
		++states[state];
	}
	/* Original state classification is observable independently of saved map bits. */
	fmt::print(stderr, "SIGNALS states {} {} {}\n", states[0], states[1], states[2]);
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
	company.Restore();
	return true;
}
extern "C" void __real__Z22IConsoleStdLibRegisterv();
extern "C" void __wrap__Z22IConsoleStdLibRegisterv()
{
	__real__Z22IConsoleStdLibRegisterv();
	IConsole::CmdRegister("signals_scenario", SignalsScenario);
}
