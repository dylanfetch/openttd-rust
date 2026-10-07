# OpenTTD-Rust near-term roadmap

Root owns this file and updates it when a phase completes or priorities change.
`AGENTS.md` and `docs/rust-migration.md` define the rules; this file decides what
to work on next. If an issue conflicts with this roadmap, follow the roadmap;
a subagent stops and reports the conflict in its hand-off to root. Keep this
file forward-looking and under about 200 lines: completed work is one table
row, and its evidence stays in the PR (`AGENTS.md`, "Evidence budget").

## Where the fork stands (2026-10-07, `e9cdc2842c`)

- Sixteen ownership ports retire 20,947 original C++ lines, about 5.5% of roughly
  384k non-vendored `src/` lines (15,624 excluding town-name and road-movement
  data). Main CI is green.
- The play saves run **2.54x** slower than the original (play-opus-55-167-002
  2.54x, play-grok-159-001 2.55x, padhattan 2.17x, generate-tgp-256-1 1.41x).
  Road is 87% of the gap, and most of it is boundary overhead, not game logic
  (#155, #168).
- Six component branches are unintegrated (#157 cap): #145, #149, #151, #152,
  #153, #147.

## Third steering review (2026-10-07)

A user-directed review (Claude Code, `claude-opus-5-5`) independently audited
aircraft #145, company #149, the integrated train controller #143, station #142
and industry #144 line by line against the original bodies, and profiled the
current build. Astra's work since the second review is on track. #154 is fixed,
19 integrations landed with green CI, and all five ports are faithful: Random
order, widths and saturation match. Reachable divergences are small: company
posts client ID `u32::MAX` instead of 0 (#149), and train crossings drop the
barrier sound (#169). Corrections:

1. **The boundary template is the speed problem** (#168). Road, train, town,
   disaster, aircraft, company, ship and orders allocate a task, future and `Rc`
   on every entry. They route non-throwing services through the action protocol,
   dispatch numbered opcodes and copy whole-record views on single-field reads.
   Station and cargo storage show the direct form works. `AGENTS.md` now forbids
   the template for new work, and reentry alone no longer justifies the protocol.
2. **The road plan missed its largest cost.** RoadObserve is the biggest single
   self cost (0.68 s with IsBus). The #155 conversion now includes narrowing it.
3. **The speed budget compounds.** "+10% per port" is replaced by a ratchet (Phase 1).
4. **The benchmark sees only road vehicles.** Both main play saves have no trains,
   ships or aircraft, so those ports' overhead is invisible. Train is 4.0x on
   padhattan, and its consist walk is O(n^2) per read.
5. **The roadmap became a log.** CI run IDs, hashes, seeds and per-PR plans now go
   in PRs and issues.

Phase 1 and Phase 2 below carry the resulting actions in order. Reviews now use
`gpt-6.1-sol` high, and the reviewer fixes its own findings (`AGENTS.md`). Existing
acceptances stand; the next review round on each open PR moves to the new rule.

## Earlier steering (2026-10-04), still in force

First review: call shared services directly (#107); measure before map-access
decisions (#108); keep tooling proportionate (#109); prioritize core simulation;
use fresh agents per task and per PR review. Second review: harness end moments
independent of wall time (#154, done); speed report (#155); track coverage gaps
(#156); cap work in progress at six branches (#157).

## Completed ownership ports

Metrics are `tools/port-metrics.py`: Rust / tooling / C++ glue / C++ retired.

| Issue | Component | PR | Commit | Metrics |
| --- | --- | --- | --- | --- |
| #73 | TGP terrain generation | #94 | `014968bd38` | 658 / 85 / 84 / 991 |
| #74 | Link graph job computation | #95 | `7a2858c469` | 1040 / 159 / 135 / 1120 |
| #96 | Built-in town names (retired count is mostly data tables) | #98 | `ff3648ba09` | 3296 / 241 / 122 / 3848 |
| #99 | Tree generation, simulation, planting | #102 | `602b5a60ff` | 1288 / 561 / 247 / 816 |
| #101 | Effect vehicles | #105 | `f6a0d66b63` | 494 / 553 / 412 / 548 |
| #104 | Water-region cache and graph service | #114 | `9732d8bfa2` | 459 / 610 / 273 / 407 |
| #103 | Disaster scheduling, vehicles and event control | #118 | `d5edfcb4c5` | 1404 / 795 / 306 / 968 |
| #117 | Cargo payment and delivery | #123 via #134 | `adbe063372` | 396 / 618 / 370 / 315 |
| #119 | Ship YAPF and canonical path cache | #126 via #134 | `adbe063372` | 881 / 54 / 506 / 646 |
| #120 | Town growth and private state | #127 via #134 | `adbe063372` | 1430 / 668 / 625 / 1548 |
| #121 | Road controller/private state (includes 1475 movement-data lines) | #128 via #134 | `adbe063372` | 3159 / 471 / 1188 / 2842 |
| #122 | Rail YAPF search, cache and reservation | #133 via #141 | `ea80a746a3` | 1704 / 144 / 595 / 1517 |
| #124 | Road YAPF search and path construction | #135 via #141 | `ea80a746a3` | 663 / 55 / 329 / 532 |
| #125 | Station cargo-service control and state | #142 | `ba920555ca` | 1516 / 241 / 1022 / 1332 |
| #130 | Rail controller, reservation and private state | #143 via #166 | `dace87c9b1` | 4087 / 613 / 1660 / 2585 |
| #129 | Industry production, histories and builder state | #144 via #166 | `dace87c9b1` | 1508 / 474 / 678 / 932 |

Harness and process: #72 harness (#85), #84 play saves (#87), #97 provenance
freeze (#100), #88 Ruff (#92), #75 partial-pixel fidelity (#91), #90 world-state
design (`docs/design/world-state.md`).

| Issue | Completed maintenance | PR | Commit | Metrics (Rust / tooling / glue / retired) |
| --- | --- | --- | --- | --- |
| #109 | Component scenario modules | #112 | `9e1e5d175d` | 0 / 2414 / 0 / 0; moved code, net +190 lines |
| #107 (trees) | Direct shared services | #113 | `c00402350b` | 640 / 22 / 149 / 22; old glue retired, no new game logic |
| #107 (effects) | Direct shared services | #115 | `e702c4724e` | 241 / 4 / 155 / 86; old glue retired, net +65 lines |
| #86 (rail/ship slice) | Owner-provided transport save | #110 | `350aec9e30` | 0 / 141 / 0 / 0 |
| #108 | Map measurements and direct-service decision | #116 | `15bef07bde` | 0 / 65 / 81 / 0 |
| #86 (aircraft slice) | Authorized reference-built aircraft fixture | #132 | `320711cccd` | 0 / 347 / 0 / 0 |
| #131 | CI-capacity integration of the four owners above | #134 | `adbe063372` | aggregate 5866 / 1780 / 2689 / 5351; not additional retirement |
| #154 | Deterministic semantic harness endpoints | #160 | `e16d01c869` | 0 / 277 / 0 / 0; tooling net +49 lines |
| #158 | Restore existing MinGW i686 nightly dependencies | #159 | `55ad3a84ad` | 0 / 0 / 0 / 0; workflow-only |
| #155 (measurement) | Semantic-valid speed report and benchmark isolation | #163 | `7fc597da60` | 0 / 212 / 0 / 0; road conversion remains open |
| #140 | CI-capacity integration of rail/road search owners | #141 | `ea80a746a3` | aggregate 2369 / 197 / 927 / 2049; not additional retirement |
| #156 (routing/crash) | Road RNG, crossing crash and event witnesses | #161 | `f8b5034ed3` | 0 / 306 / 0 / 0 |
| #156 (flooding) | Ordinary road flooding crash witness | #162 | `0b2503cb30` | 0 / 155 / 0 / 0 |
| #156 (service RNG) | Both road service routing random outcomes | #164 | `ee6fcdd44b` | 0 / 64 / 0 / 0 |
| #165 | CI-capacity integration of rail/industry owners | #166 | `dace87c9b1` | aggregate 5595 / 1079 / 2336 / 3517; not additional retirement |
| #156 (expiry) | Ordinary single-head crash cleanup boundary | #167 | `0c2a4bf37e` | 0 / 56 / 0 / 0 |

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness and speed maintenance

The harness is `python3 tools/migration.py simulate` (`docs/rust-migration.md`,
"Simulation comparison"); its default set runs in CI. Harness regressions are
always the first priority. Port differences go in `KNOWN_FAILURES` with an issue,
never in masks.

1. **Speed ratchet (#155).** Commit a per-scenario budget of best-known
   candidate/reference ratios, with today's figures above as the starting values.
   Benchmark both road play saves, padhattan and generate-tgp-256-1 with
   `simulate <name> --benchmark 3 --jobs 2` on an idle host. A PR may not exceed
   the budget by more than 3% without a stated reason accepted by root, and
   improvements lower the budget. Targets: <=1.9x on the road play saves after
   road conversion; <=1.5x on play saves and <=1.15x on generation when #168 closes.
2. **#156 coverage, standing capacity.** Random and crash branches first. A mixed
   save with trains (PBS junctions, crossings), ships, aircraft and subsidies
   closes many gaps and gives the speed budget a non-road benchmark. Ask the user
   for one through the play-save pipeline before building more per-branch fixtures.
3. **CI capacity.** About a third of recent runs were cancelled. Push drafts only
   when CI is needed, batch commits, and avoid merges whose only purpose is to
   join dependency ancestry. If capacity still blocks integration, propose gating
   the platform matrix to ready-for-review PRs.

## Phase 2: current work, in order

Integration of reviewed work comes before new starts. Never hold more than six
unintegrated component branches (#157). Independent items (#169, #156 slices)
may run in parallel with this list.

1. **Integrate #145 aircraft.** Resolve the `abi.rs` conflict (the reviewer checks
   it), restore the `PerformanceAccumulator` order, re-measure metrics and
   ratio, then pass CI.
2. **Integrate #149 company.** Fix client ID 0 and the u16 shift, get re-review,
   update the base (conflict only in `tools/`), then pass CI.
3. **#155 road conversion.** Remove the Task/Future/Rc protocol for all 22
   services and 14 entries, and narrow RoadObserve: field getters or a hot
   record, no `IsBus` in the view, type-filter in `close`, single-pass `nearby`,
   owner resolved once per entry, and nested `GetCurrentMaxSpeed`. Add the ratchet.
   The expected result is about 1.7-1.9x.
4. **Integrate #151 cargo storage** (already direct) on the fresh baseline.
5. **#168 train and train-reservation conversion**, including the O(n^2) consist
   walk and the per-step `nearby` Vec; then aircraft and company; then trees,
   town and disaster. One PR per component.
6. **Finish #153 orders, then #152 ship, in the direct form before review.** Ship
   names orders as an ancestry dependency. Then #147 fleet replacement.
7. **New components** (#148 town lifecycle, then #150 industry construction)
   start only once the road play saves are at or below 2.0x and #168's train
   slice is integrated. When fewer than two unstarted selections remain, a fresh
   Astra high planner replenishes whole simulation owners, which are planned in
   the direct form.

The accepted #108 decision keeps canonical map arrays in C++ with direct bundled
`noexcept` services. The 2026-10-07 profile attributes the overhead to entry and
record-copy costs, not map access; revisit #108 only if a post-#168 profile shows
map or pool crossings dominating.

## Resume checkpoint

| Issue / PR | Branch (worktree suffix), head | State and next step |
| --- | --- | --- |
| #136 / #145 | `aircraft-controller-ownership-136` (`aircraft-controller`), `dd0f2f0475` | Source accepted; base conflict in `abi.rs`; Phase 2 item 1. |
| #137 / #149 | `company-economy-ownership-137` (`company-economy`), `5b9a005064` | Source accepted; two audit fixes need re-review; item 2. |
| #139 / #151 | `cargo-storage-movement-139` (`cargo-storage`), `ff912bcfcb` | Accepted at `a25a7d41e2` (reviewer `/root/review_cargo_storage_151`); final base and CI after item 3. |
| #138 / #153 | `order-lifecycle-ownership-138` (`order-lifecycle`), `ef58c967f7` | Draft checkpoint; evidence plan and #168 conversion in PR comments. |
| #146 / #152 | `ship-controller-ownership-146` (`ship-controller`), `7ec70a1768` | Draft; join #154 launcher, convert per #168, follow #153. |
| #147 | `fleet-replacement-ownership-147` (`fleet-replacement`), `68d660adc9` | State-only WIP, no PR. |

Standing #156 work: aircraft landing RNG after #145 (plan in #156), then the
2026-10-07 audit list there. Preserve the pinned reference, paused curve
worktrees and evidence branches `evidence-disaster-vehicles` (`a775543162`) and
`evidence-water-regions` (`c752070cde`); do not reapply effect helper `73ccd511fb`.
Build and test with `--jobs 2`. The speed tool's clone-wide game lock must also be
initialized by standalone fixture entry points.

## Choosing the next task

Take the first unblocked item above. Fill idle capacity in this order: review and
integration of finished work, Phase 1 items, then Phase 2 in order. Paused,
deferred and out-of-scope issues are not fallbacks. Root selects further
ownership work here before implementation starts.

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
