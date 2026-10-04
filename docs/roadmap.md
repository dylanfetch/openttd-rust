# OpenTTD-Rust near-term roadmap

Root owns this file and updates it when a phase completes or priorities change.
`AGENTS.md` and `docs/rust-migration.md` define the rules; this file decides what
to work on next. If an issue conflicts with this roadmap, follow the roadmap;
a subagent stops and reports the conflict in its hand-off to root. Keep this
file forward-looking: completed work is one table row, and its evidence stays in
the PR (`AGENTS.md`, "Evidence budget").

## Where the fork stands (2026-10-04, `rust-migration` at `f6a0d66b63`)

- Five ownership ports landed after the course correction (table below), retiring
  about 7,300 C++ lines. That is under 2% of the ~384k non-vendored lines in
  `src/`; most of the simulation is still C++.
- Earlier work (utility kernels, string/UTF-8, crypto, Packet, ScriptList, widget
  parser) predates the course correction and is recorded in
  `docs/rust-migration.md`.
- Process works: each port had an issue, an isolated worktree, independent Astra
  review that caught real defects (#102's RNG exception path), green CI before
  merge, and no harness masks.

## Steering review (2026-10-04)

A user-directed review (Claude Code, `claude-opus-5-5`) found the direction right
and made these changes, now reflected in `AGENTS.md` and the phases below:

1. **Call shared services directly.** Trees, effects and the #103 WIP return to
   C++ before every `Random()` draw (task state machines or async/await), solely
   because `RANDOM_DEBUG` log I/O could throw. The user ruled environmental
   failures out of fidelity scope. Ports call `noexcept` service wrappers and
   mirror the original control flow (#107). This must land before vehicles,
   pathfinding and economy, where the state-machine style would not scale.
2. **Decide map access before tile-heavy ports** (#108), from measurements.
3. **Keep tooling proportionate.** `tools/simulate.py` grew to 2,247 lines, about
   550 per recent port; split it into per-component modules (#109). Effects added
   965 glue+tooling lines to retire 548.
4. **Move to core game logic.** The self-contained leaf components are done.
   After #103/#104, select economy, then vehicles; start #86's rail/aircraft
   scenarios now, since they gate vehicle controllers.
5. **Fresh agents per task and per PR review**, named for their task.

## Completed ownership ports

Metrics are `tools/port-metrics.py`: Rust / tooling / C++ glue / C++ retired.

| Issue | Component | PR | Commit | Metrics |
| --- | --- | --- | --- | --- |
| #73 | TGP terrain generation | #94 | `014968bd38` | 658 / 85 / 84 / 991 |
| #74 | Link graph job computation | #95 | `7a2858c469` | 1040 / 159 / 135 / 1120 |
| #96 | Built-in town names (retired count is mostly data tables) | #98 | `ff3648ba09` | 3296 / 241 / 122 / 3848 |
| #99 | Tree generation, simulation, planting | #102 | `602b5a60ff` | 1288 / 561 / 247 / 816 |
| #101 | Effect vehicles | #105 | `f6a0d66b63` | 494 / 553 / 412 / 548 |

Harness and process: #72 harness (#85), #84 play saves (#87), #97 provenance
freeze (#100), #88 Ruff (#92), #75 partial-pixel fidelity (#91), #90 world-state
design (`docs/design/world-state.md`).

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness maintenance

The harness is `python3 tools/migration.py simulate` (`docs/rust-migration.md`,
"Simulation comparison"); its default set runs in CI. Harness regressions are
always the first priority. Port differences go in `KNOWN_FAILURES` with an issue,
never in masks.

- **#109 Split `tools/simulate.py` into per-component scenario modules.**
  Behavior-preserving. Do it first: all agents are stopped, and the #103/#104
  evidence branches must rebase anyway, now into their own modules.
- **#86 Rail and aircraft scenarios.** The owner's hand-built save
  (`play-padhattan-ridge-1996-*`, `migration/saves/README.md`) runs two trains
  with signals, one ship and road vehicles by default. Aircraft remain open, and
  #104 still needs its canal/lock/aqueduct ship cases. Required before any rail,
  road-vehicle or aircraft controller or YAPF port.

## Phase 3: current ownership work, in order

1. **#107 Direct shared-service calls; rewrite trees, then effects.** Runs in
   parallel with #109. Its services module is the base for #103 and #104.
2. **#103 Disaster scheduling, vehicles and event control.** Resume from
   `port-disaster-vehicles` at `ccb9586fe3` (Cargo/verify and two smoke cases pass;
   not reviewed). Evidence: `evidence-disaster-vehicles` at `a775543162`; drop its
   borrowed effect-helper commit `73ccd511fb`. Rebase onto #105 and #107, replace
   async/await draws with direct calls, then turn the reference-built UFO/train and
   zeppelin/airport experiments into harness witnesses in a disasters module.
   Then review, comparisons and CI.
3. **#104 Water-region cache and graph service**, built on #107 from the start
   (`port-water-regions` has no source changes). Evidence: `evidence-water-regions`
   at `c752070cde`: two reference-built saves, setup AIs and five scenarios passing
   reference-self (164 snapshots). Rebase into a ships module after #109 and
   deduplicate `field_spans` from #105. Still needed: canal/lock/aqueduct, reload,
   warm-cache mutation, negative probes, soak and candidate evidence. Record
   crossing counts and timing for #108.
4. **#108 Map access decision** (Astra high). Measure on trees after #107 and on
   #104. Root records the decision here before any item in step 6 is selected.
5. **Economy** (next selection after #103/#104). Root has a fresh Astra high agent
   compare station rating updates, cargo payment and delivery, and industry
   production, and selects one whose periodic loop and state Rust can own
   whole. A formula extracted from a C++ loop is kernel extraction.
6. **After #108 and #86:** complete ship YAPF (both levels, after #104), town
   growth, road/rail vehicle controllers, YAPF rail/road.

Storage transfers (map arrays, pools) still require explicit selection here,
following `docs/design/world-state.md` as amended by #108.

## Choosing the next task

Take the first unblocked item above. Work in CI or review is not blocking: start
the next item while it runs. Keep spare capacity on scenarios (#86) and
independent review. Paused, deferred and out-of-scope issues are not fallbacks.
Root selects further ownership work here before implementation starts.

## Out of scope for the near term

More `src/3rdparty` ports (monocypher, squirrel, others). GUI rendering and
layout. Platform or toolchain expansion. Deferred behavior improvements stay in
`deferred-improvement` issues.

## Progress metric

Every port PR pastes the output of `python3 tools/port-metrics.py`: Rust added,
tooling added, C++ glue added (new `src/` lines compiled with `WITH_RUST`), and
C++ retired (original `src/` lines the candidate no longer compiles: deleted,
or moved under either `WITH_RUST` guard form into the portable fallback). A
healthy port retires more C++ than it adds as glue plus tooling; ports that
fail this state a reason in the PR. For scale, the history port (#32) measured
Rust 433, tooling 379, glue 203, retired 64: the kernel-extraction pattern to
avoid. Data tables inflate retired counts; judge progress by game logic owned.
