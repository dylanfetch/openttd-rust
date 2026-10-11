# OpenTTD-Rust near-term roadmap

Root owns selection here; `AGENTS.md` and `docs/rust-migration.md` define process.
If an issue conflicts, follow this roadmap and report it to root. Keep this
forward-looking, about 200 lines; completed work is one row, with evidence in PRs.

## Where the fork stands (2026-10-09, `271fdfea2e`)

- Twenty-two ownership ports retire about 32.5k original C++ lines, about 8.5%
  of roughly 384k non-vendored `src/` lines. Over the last day, the #168
  conversions added about 5.7k C++ glue lines and retired 2.6k. Untouched
  simulation remains large: `rail_cmd`, `road_cmd`, `water_cmd`,
  `tunnelbridge_cmd`, `clear_cmd`, most of `vehicle.cpp`/`vehicle_cmd.cpp`, and
  the tile loops they drive.
- Speed (independent idle-host run, `271fdfea2e`): Opus 1.39x, Grok 1.39x,
  Padhattan-1996 1.44x, Padhattan-2000 1.49x (was 2.02x), TGP 1.38x. The
  play-save target of 1.5x or less is met. Glue is about 27% of reference time,
  down from 52%. Trees #208 brings TGP to 1.20x, and #199 road takes road
  crossings from 44 to 11 per vehicle tick.
- Three unintegrated branches: trees #208, company #209 and #199 road (WIP, no
  PR yet).

## Seventh steering review (2026-10-09)

The two root sessions since the sixth review ran in Claude Code
(`claude-opus-5-5`) rather than Codex, and both ended at a usage limit. Root
followed the process: train #195, aircraft #194, UTF-8 #193, industry #205,
fleet #197 and hasher #207 each integrated with an attributed review, an
exact-head full CI run and paired timing. Five independent audits (trees #208,
company #209, the #199 WIP, the integrated deltas, and a perf/callgrind
profile) found no reachable divergence.

The findings are systemic:
- three cited probes never run in CI;
- hand-allocated ABI IDs collide between branches;
- an interrupted review was left as the only review of #208;
- the speed caps lag far behind the measured ratios;
- conversion work is crowding out new ownership.

Corrections, in order:
1. **Cited checks run in CI (#210, AGENTS.md).** The rule now covers reference
   probes as well as witnesses. Resolve the `companies`, `industries` and
   `cargo_storage` probes; the company probe gates #209.
2. **Interrupted work (AGENTS.md).** Agents push at every passing milestone,
   each task records its state on its issue or PR when it starts or stops, and a
   review with no findings or disposition counts as no review. #208 needs a
   fresh reviewer; the audit on #208 can serve as its comparison.
3. **Integrate #208, then #199 road PR1, then #209** (PR comments on #208 and
   #209, and the #199 issue comment). Before PR1, #199 fixes the 390-398 ABI-ID
   collision with fleet, reserves one ID block per component, and keeps one
   shared accessor table rather than a second `OpenTTDMapServices`.
4. **Ratchet the caps now.** Lower `SPEED_BUDGETS` to an idle-host
   re-measurement, for example in #208's PR: about Opus 1.39, Grok 1.40,
   Padhattan-1996 1.44, Padhattan-2000 1.49 and TGP 1.38, or 1.20 with #208.
   Every later PR holds them.
5. **Return to ownership.** Speed no longer gates new ports. Start #148 town
   lifecycle now: it delivers town in the direct typed form and replaces the
   `RustTownRun` operation dispatch rather than converting it twice. After the
   #199 road and aircraft PRs and station/loading, the remaining #168 tail
   (disaster, cold cargo/orders) does not block new ownership work. The next
   planner selects the tile loops and the vehicle base tick (see #168 comment).

## Earlier steering, still in force

First review (10-04): call shared services directly (#107); measure before map
decisions (#108); keep tooling proportionate (#109); prioritize core simulation.
Second (10-04): harness end moments (#154); speed report (#155); coverage gaps
(#156); a WIP cap of six branches (#157). Third (10-07): convert per-call
task/future/`Rc` boundaries, opcode dispatch and whole-record reads to direct
typed calls (#168); the speed budget is a ratchet; each review round gets a
fresh reviewer, who fixes its own findings; close finished agents. Fourth
(10-07): on-demand CI stays (#171); exact-head full validation; never merge with
`--admin`. Fifth (10-08): root is `gpt-6.1-sol` xhigh in Codex; exact timing
(#186); no boundary exceptions at integration. Sixth (10-09): cited witnesses
run by default (#198, #204); one shared typed vehicle and map layer with a
crossing budget (#199); trees and the industry tick before company; a fixed
hasher for the lookup-only YAPF maps.

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
| #136 | Aircraft controller and airport blocks | #145 | `f0e0b3d712` | 1773 / 764 / 380 / 1880 |
| #137 | Company finance, economy and lifecycle | #149 | `dbce82a309` | 1607 / 702 / 1052 / 1394 |
| #139 | Cargo storage and movement | #151 via #184 | `2a76d5f287` | 2838 / 460 / 973 / 2445 |
| #138 | Orders lifecycle and commands | #176 via #184 | `2a76d5f287` | 4554 / 519 / 1752 / 2518 |
| #146 | Ship controller and private state | #152 via #184 | `2a76d5f287` | 1101 / 270 / 893 / 721 |
| #147 | Fleet groups and autoreplace | #197 | `a7a6e6650a` | 2686 / 577 / 1199 / 1454 |

Harness and process: #72 harness (#85), #84 play saves (#87), #97 provenance
freeze (#100), #88 Ruff (#92), #75 partial-pixel fidelity (#91), #90 world-state
design (`docs/design/world-state.md`).

Earlier maintenance (issue, PR, tooling lines): #109/#112 (2414, moved), #107
(#113, #115), #86 (#110 141, #132 347), #108 (#116), #154 (#160 277), #158
(#159), #155 (#163 212), #156 (#161, #162, #164, #167: 581), #170 (#171 3267),
#173 (#174 -2), and in #184: #178, #185, #187 (+78). PR reviews keep the evidence.
| Issues | Maintenance PRs | Integration | Commit | Metrics |
| --- | --- | --- | --- | --- |
| #169, #156, #179 | #175, #177, #180 | #181 | `f604e30d50` | 4 / 193 / 0 / 0 |
| #200 | #201 | direct | `bb302706c1` | 0 / 10 / 0 / 0 |
| #188 | #196 | direct | `05f94f56d5` | 0 / 182 / 0 / 0 |
| #191 | #193 | direct | `b127ffaa88` | 0 / 44 / 73 / 14 |
| #198 (enforcement) | #204 | direct | `c0c67a3dd4` | 0 / 147 / 0 / 0 |
| #190 (#168 aircraft) | #194 | direct | `d8aed8754f` | 1279 / 352 / 1300 / 247 |
| #189 (#168 train) | #195 | direct | `3a2e8f6a10` | 4263 / 91 / 2812 / 853 |
| #203 (#168 industry) | #205 | direct | `9025f623e0` | 450 / 8 / 278 / 16 |
| #206 (YAPF hasher) | #207 | direct | `65ff465080` | 95 / 0 / 0 / 0 |

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness and speed maintenance

The harness is `python3 tools/migration.py simulate` (`docs/rust-migration.md`,
"Simulation comparison"); its default set runs in CI. Harness regressions are
always the first priority. Port differences go in `KNOWN_FAILURES` with an issue,
never in masks.

1. **#210 cited probes** run in default CI or lose their citation.
2. **Speed ratchet (#155).** `SPEED_BUDGETS` in `tools/simulation/roads.py` is
   the authority for the five caps, run with `simulate <name> --benchmark 3
   --jobs 2` on an idle host. A PR may exceed a cap by at most 3%, unless root
   accepts a stated reason that is not a boundary-rule exception. Improvements
   lower the caps in the same PR (seventh review, item 4). Targets: play saves
   1.5x or less (met), generation 1.15x or less. The null driver redraws
   whenever 1 ms has elapsed, so a slower candidate also draws more frames.
3. **#156 coverage, standing capacity.** Random and crash branches first. The
   seventh review added fleet, industry, trees and company gaps. A company
   scenario with `max_no_competitors >= 1` reaches the AI-start Post loop.

## Phase 2: current work, in order

Integrate reviewed work before starting anything new. Never hold more than six
unintegrated component branches (#157). Independent items (#156, #210) may run
in parallel.

1. **Integrate:** trees #208 (fresh reviewer, caps lowered), then company #209
   (after #210 and its doc fix).
2. **#199 shared vehicle and map layer**, one PR per vehicle type: road PR1 from
   `shared-layer-road-199` (fixes listed on #199), then aircraft (about 150
   crossings per tick), train, ship. Each PR deletes that port's accessor copies
   and reports crossings per vehicle tick.
3. **#148 town lifecycle starts now**, in the direct typed form; it retires the
   town operation dispatch.
4. **#168 station tick and loading**: field-number reads (`RustStationRead`,
   `RustLoadingRead`) become typed narrow reads. Then disaster and the cold
   cargo/orders conversions, which never block new ownership work.
5. **New ownership:** #150 industry construction, then the planner's selection.
   When fewer than two unstarted selections remain, a fresh high-effort planner
   selects whole simulation owners in the #199 form, starting with the tile loops
   (clear, water flooding, rail, road, tunnel/bridge) and the vehicle base tick,
   age and breakdown in `vehicle.cpp`.

The #108 decision keeps C++ map arrays and direct bundled `noexcept` services.
#199 decides vehicle state access: a typed shared layer with per-operation reads,
not a pinned-offset field view.

## Resume checkpoint

No agents or background runs are active. Apply the seventh steering review first.

| Issue / PR | Branch (worktree suffix), head | State and next step |
| --- | --- | --- |
| #202 / #208 | `trees-direct-202`, `fb06f22ddd` | Interrupted review is no review; audit on PR. Fresh reviewer, base merge, lower caps, full CI. |
| #192 / #209 | `company-direct-192`, `06a219d447` | Reviewed, fix verified. Blocked on #210 (company probe) and the `InitializeCompanies` doc line; #156 entry posted by steering. |
| #199 PR1 | `shared-layer-road-199`, `04711fa568` | WIP saved. Fixes listed on #199 (ABI IDs, one table), rebase, full suite, crossings, draft PR. |

Preserve the pinned reference, paused curve worktrees and evidence branches
`evidence-disaster-vehicles` (`a775543162`) and `evidence-water-regions`
(`c752070cde`); do not reapply effect helper `73ccd511fb`. Build and test with
`--jobs 2`; standalone fixture games share the benchmark lock.

## Choosing the next task

Take the first unblocked item above: review/integration, Phase 1, then Phase 2.
Paused, deferred and out-of-scope issues are not fallbacks. Root selects further
ownership work here before implementation starts.

## Out of scope for the near term

More `src/3rdparty` ports (monocypher, squirrel, others). GUI rendering and
layout. Platform or toolchain expansion. Deferred behavior improvements stay in
`deferred-improvement` issues.

## Progress metric

Every port PR pastes `python3 tools/port-metrics.py`: Rust added, tooling added,
C++ glue added (new Rust-enabled `src/` lines), and C++ retired (original lines
deleted or excluded into portable fallback). Retire more C++ than glue plus
tooling, or state why in the PR. Avoid utility-kernel extraction; data tables
inflate retirement. Judge progress by game logic owned.
