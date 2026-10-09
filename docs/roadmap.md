# OpenTTD-Rust near-term roadmap

Root owns selection here; `AGENTS.md` and `docs/rust-migration.md` define process.
If an issue conflicts, follow this roadmap and report it to root. Keep this
forward-looking, about 200 lines; completed work is one row, with evidence in PRs.

## Where the fork stands (2026-10-09, `5c7ab182b3`)

- Twenty-one ownership ports retire about 29.9k original C++ lines, about 7.8%
  of roughly 384k non-vendored `src/` lines. Untouched simulation remains large:
  `rail_cmd`, `road_cmd`, `water_cmd`, `tunnelbridge_cmd`, `clear_cmd`, most of
  `vehicle.cpp` and `vehicle_cmd.cpp`, and the tile loops they drive.
- An independent profile reproduces the integrated ratios: Opus 1.42x, Grok
  1.42x, Padhattan-1996 1.71x, Padhattan-2000 2.02x, TGP 1.44x. Glue services are
  52% of reference time on Padhattan-2000; Rust net of replaced C++ is 26%.
  Draft train #195 alone measures Padhattan-2000 1.77x. TGP's gap is trees,
  industry and town during the simulated years, not generation.
- The #168 conversions retire little and add glue: train #195 is net +1917 C++
  lines for 853 retired, aircraft #194 net +1053 for 247. Most of it is per-port
  copies of the same vehicle and map accessors (#199).
- Five unintegrated branches: fleet #197, train #195, aircraft #194, UTF-8
  #193 and breakdown coverage #196. Quick validation on `5c7ab182b3` is red.

## Sixth steering review (2026-10-09)

A Claude Code review (`claude-opus-5-5`) audited root's 10-08 session: four
independent audits (train #195, fleet #197, ship #152 with the orders/cargo
hot-path fix, aircraft #194 with UTF-8 #193) and a perf/callgrind profile. Root
applied the fifth review: exact timing, hot-path fixes, re-measured caps and a
clean #184 integration. Train, aircraft, ship and UTF-8 show no reachable
divergence. Fleet has one, and coverage claims rest on opt-in profiles.

Corrections, in order:
1. **Done:** red Quick validation fixed by #200/#201 (`bb302706c1`). Check the
   last push's runs before reporting the branch green.
2. **Enforce cited branch witnesses (#198)** before #195 integrates. Train and
   ship witnesses run only when a profile environment variable is set by hand,
   and CI never sets it. New AGENTS.md rule: cited witnesses run and fail by
   default.
3. **Fleet #197 must fix**, per its PR comment: the `NEW_GROUP` sentinel
   (`u32::MAX` versus `VehicleID::Invalid()` 0xFFFFF breaks "Create group" from a
   vehicle list), the per-Vehicle `Box<u16>` for `group_id`, positional
   callback tables, and dead wrappers. Add the list-based scenarios.
4. **Integrate train #195 and aircraft #194 as they stand** once their PR
   comments' final-head evidence passes. Do not rework them in flight. #193 is
   accepted as a support-code fast path (PR ruling), not a precedent.
5. **Then #199: one shared typed vehicle and map service layer** with a
   crossing budget per vehicle tick. New AGENTS.md rule: resolve once per entry,
   read only the used fields once, one shared definition per accessor, real
   return types and designated initializers. If Padhattan-2000 stays above 1.6x
   after it, plan a pinned-offset field view versus Rust-owned vehicle storage
   before more vehicle conversions.
6. **Reorder #168 by measured cost:** trees (with `ObserveTile`'s unconditional
   `GetTileZ` and per-call `Box`) and the industry tick's whole-record read come
   before company #192; station tick and loading come before cold cargo/orders.
   A fixed fast hasher for the lookup-only YAPF maps is a small independent PR.
7. **The next planner looks at untouched simulation:** the tile loops (clear,
   water flooding, rail, road, tunnel/bridge) and the vehicle base tick, age and
   breakdown in `vehicle.cpp`, after #148 and #150.

## Earlier steering, still in force

First review (10-04): call shared services directly (#107); measure before map
decisions (#108); keep tooling proportionate (#109); prioritize core simulation.
Second (10-04): harness end moments (#154, done); speed report (#155); coverage
gaps (#156); WIP cap of six branches (#157). Third (10-07): convert per-call
task/future/`Rc` boundaries, opcode dispatch and whole-record reads to direct
typed calls (#168); the speed budget is a ratchet; reviews use `gpt-6.1-sol` high
with a fresh reviewer each round who fixes its own findings; close finished agents.
Fourth (10-07): on-demand CI stays (#171). Keep `CI_ON_DEMAND=true`, require
exact-head full validation, never merge with `--admin`, and treat more process
tooling as no fallback. Fifth (10-08): root is `gpt-6.1-sol` xhigh with Astra
high planners; exact timing (#186, done); no boundary exceptions at integration.

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

Harness and process: #72 harness (#85), #84 play saves (#87), #97 provenance
freeze (#100), #88 Ruff (#92), #75 partial-pixel fidelity (#91), #90 world-state
design (`docs/design/world-state.md`).

Maintenance (issue, PR, tooling lines): #109 scenario modules #112 (2414, moved
code); #107 direct services #113, #115; #86 transport save #110 (141) and
aircraft fixture #132 (347); #108 map decision #116; #154 harness endpoints #160
(277); #158 MinGW nightly #159; #155 speed report #163 (212); #156 road witnesses
#161, #162, #164, #167 (581); #170 on-demand CI and validation tools #171 (3267,
no game logic); #173 validation repairs #174 (`cd0297938c`, net tooling -2).
Also in #184: #155 direct road #178, #183 unchanged 2006 save #185,
#186 exact timing/root config #187 (net tooling +78). Its component PRs and
attributed review reports retain evidence; batch totals are above.
| Issues | Maintenance PRs | Integration | Commit | Metrics |
| --- | --- | --- | --- | --- |
| #169, #156, #179 | #175, #177, #180 | #181 | `f604e30d50` | 4 / 193 / 0 / 0 |
| #200 | #201 | direct | `bb302706c1` | 0 / 0 / 0 / 0 |

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness and speed maintenance

The harness is `python3 tools/migration.py simulate` (`docs/rust-migration.md`,
"Simulation comparison"); its default set runs in CI. Harness regressions are
always the first priority. Port differences go in `KNOWN_FAILURES` with an issue,
never in masks.

1. **#198 witness enforcement** is a harness regression and comes first.
2. **Speed ratchet (#155).** `SPEED_BUDGETS` in `tools/simulation/roads.py` is
   the authority for the five caps, run with `simulate <name> --benchmark 3
   --jobs 2` on an idle host. A PR may exceed a cap by at most 3%, unless root
   accepts a stated reason that is not a boundary-rule exception. Improvements
   lower the caps. Targets: <=1.5x on play saves and <=1.15x on generation when
   #168/#199 close. The null driver redraws whenever 1 ms has elapsed, so a
   slower candidate also draws more frames; read drawing cost with that in mind.
3. **#156 coverage, standing capacity.** Random and crash branches first. The
   presence of PBS signals, locks or subsidies in a save does not show that every
   route or multiplier was exercised. #188/#196 aircraft breakdown is the current
   slice; the sixth review added fleet, train, reservation and ship gaps.

## Phase 2: current work, in order

Integration of reviewed work comes before new starts. Never hold more than six
unintegrated component branches (#157). Independent items (#156 slices) may run
in parallel with this list.

1. **Integrate the drafts:** UTF-8 #193, train #195 (after #198), aircraft
   #194, breakdown #196, then fleet #197 after its required fixes. Preserve
   fleet's full native CommandCost and its quirks.
2. **#199 shared vehicle and map layer**, one PR per vehicle type, starting once
   train and aircraft are integrated. Report crossings per vehicle tick.
3. **#168 remaining conversions, one PR per component:** trees, industry tick
   read, company #192 (StopAI direct; only proven startup/post VM exceptions use
   stack continuation records), station tick and loading, town, disaster, then
   cold cargo/orders. The YAPF hasher is a small PR at any point.
4. **New components** (#148 town lifecycle, then #150 industry construction)
   start once train #195 is integrated. When fewer than two unstarted selections
   remain, a fresh Astra high planner replenishes whole simulation owners in the
   #199 form, starting with the tile loops and the vehicle base tick.

The #108 decision keeps C++ map arrays and direct bundled `noexcept` services;
map crossings are 5-11% of reference time. #199 decides vehicle state access.

## Resume checkpoint

Apply the sixth steering review first: #198, then the drafts' PR comments.
#193 needs a fresh review of root fix `93dd49329c` (script-list tool compiled
reference VM copies) and a new full run; #196 is reviewed and in full CI.

| Issue / PR | Branch (worktree suffix), head | State and next step |
| --- | --- | --- |
| #147 / #197 | `fleet-replacement-ownership-147` (`fleet-replacement`), `32785b4631` | WIP; fix the steering PR comment's four items, then final audit, ABI/reentry/CommandCost fixtures, checks, fresh review, full CI. |
| #189 / #195 | `train-direct-189` (`train-direct-189`), `8aa1657008` | Cargo/native pass; final-head simulation with all play/rail-owner saves under #198, used-field and duplicate fixes, timing, review, full CI. |
| #190 / #194 | `aircraft-direct-190` (`aircraft-direct-190`), `29ff227562` | Cargo/native and 3 cases pass; final-head aircraft/disaster/default suites, timing, review, full CI. |
| #191 / #193 | `utf8-hot-191` (`utf8-hot-191`), `cf91511991` | Reviewed; full CI failed in script-list tool; root fix `93dd49329c` needs review and full CI. |
| #192 / #168 | none | Company direct conversion after trees and industry read. |
| #188 / #196 | `aircraft-breakdown-188` (`aircraft-breakdown-188`), `3fa6047b09` | Sensitivity/soak pass; reviewed (fix `e52b34c2ca` verified); full CI running. |

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
