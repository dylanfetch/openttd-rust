# OpenTTD-Rust near-term roadmap

Root owns selection here; `AGENTS.md` and `docs/rust-migration.md` define process.
If an issue conflicts, follow this roadmap and report it to root. Keep this
forward-looking, about 200 lines; completed work is one row, with evidence in PRs.

## Where the fork stands (2026-10-08, `2a76d5f287`)

- Twenty-one ownership ports retire about 29.9k original C++ lines, about 7.8%
  of roughly 384k non-vendored `src/` lines (24.6k excluding town-name and road
  data). #184 totals: Rust 10166 / tooling 1918 / glue 4985 / retired 6073,
  including direct-road/timing/save maintenance; 386 retired lines are old road
  boundary code. Glue plus tooling exceeds retirement; next owners stay whole.
- Exact three-pair ratios in #184: Opus 1.435x, Grok 1.419x, Padhattan-1996
  1.704x, Padhattan-2000 2.018x, generation 1.452x. All pass current caps +3%;
  no cap rises. The full semantic suite passes 259 cases / 1,922 snapshots.
- Last mixed-save profile before #184's hot-path fixes, as extra candidate time:
  aircraft +27%, train +25%, window drawing and string formatting +18%, road +11%, trees +8%.
- #184 passed exact-head full validation and post-merge platform checks; native
  comparisons are running.
  A full run costs about 104 job-minutes; ordinary PR pushes stay cheap.
- Four unintegrated branches: fleet #147, train #189, aircraft #190 and UTF-8
  #191. The host accepted three fresh workers before its thread limit; root
  implements the bounded UTF-8 fix. Coverage #188 waits for a fresh slot.

## Fifth steering review (2026-10-08)

A Claude Code review (`claude-opus-5-5`) audited the first `gpt-6.1-sol` xhigh
root session. It ran four independent port audits (aircraft, company, cargo,
orders) and a perf bisection and profile of #184. No reachable divergence was
found. Root integrated #145, #149, #174 and #181 cleanly and correctly held #184
at the ratchet. The user keeps Sol xhigh as root and Astra high for planning.

Corrections, in order:
1. **Root is `gpt-6.1-sol` xhigh** (AGENTS.md, `/start-development`). Attribute
   root artifacts with the effort actually used: several xhigh root comments
   said "medium". Change `.codex/config.toml` root to `gpt-6.1-sol` in a small
   PR; it may share #186's full run.
2. **Fix the timer before trusting the ratchet (#186).** `subprocess.run(timeout=)`
   rounds each game up to 50 ms. That is about 7.5% on a 0.65 s case, against a
   3% tolerance. Root had noted this limit and still blocked on it. Re-measure
   all caps once, exactly, then judge #184 against them.
3. **#184 fixes its hot paths before integrating**, per its PR comment. Orders
   must look up each record once per entry, not through about 17 indirect calls
   per vehicle per tick. Cargo needs typed single-field getters, no `CargoNext`
   vector, and no `Packets()` copy. Then a fresh review, the Opus/Grok caps, and
   full CI.
4. **No boundary exceptions at integration** (new AGENTS.md rule). Company's
   accepted Task/Future exception also covered a 66-service switch, positional
   `[i64; 32]` reads and reentry-only action services. All of these are now
   listed in #168.
5. **#168 order is confirmed**: train or aircraft first, then company, trees and
   station/town. #168 also lists the per-character `DecodeUtf8`/string-consumer
   crossings (+9% on Padhattan) as a separate small PR.
6. **Coverage gaps**: new aircraft-breakdown, cargo forced-transfer/GetVia,
   conditional-order and loan-arm gaps go to #156.

## Earlier steering, still in force

First review (10-04): call shared services directly (#107); measure before map
decisions (#108); keep tooling proportionate (#109); prioritize core simulation.
Second (10-04): harness end moments (#154, done); speed report (#155); coverage
gaps (#156); WIP cap of six branches (#157). Third (10-07): convert per-call
task/future/`Rc` boundaries, opcode dispatch and whole-record reads to direct
typed calls (#168); the speed budget is a ratchet; reviews use `gpt-6.1-sol` high
with a fresh reviewer each round who fixes its own findings; close finished agents.
Fourth (10-07): on-demand CI stays (#171, gaps closed by #173/#174). Keep
`CI_ON_DEMAND=true`, require exact-head full validation, never merge with
`--admin`, and treat more process tooling as no fallback.

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

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness and speed maintenance

The harness is `python3 tools/migration.py simulate` (`docs/rust-migration.md`,
"Simulation comparison"); its default set runs in CI. Harness regressions are
always the first priority. Port differences go in `KNOWN_FAILURES` with an issue,
never in masks.

1. **Speed ratchet (#155); exact timing #186/#187 is integrated.** The budget
   covers both road play saves, both Padhattan saves and generate-tgp-256-1, run
   with `simulate <name> --benchmark 3 --jobs 2` on an idle host. A PR may exceed
   a cap by at most 3%, unless root accepts a stated reason that is not a
   boundary-rule exception. Improvements lower the caps. Targets: <=1.5x on play
   saves and <=1.15x on generation when #168 closes.
2. **#156 coverage, standing capacity.** Random and crash branches first. The
   2000 and 2006 Padhattan saves are imported (#181, #185 via #184). The presence
   of PBS signals, locks or subsidies in a save does not show that every route or
   multiplier was exercised. Next slice: #188 aircraft breakdown witnesses.
   Cargo-routing, conditional-order and company source plans are in #156;
   their branches remain open until executed witnesses and sensitivity pass.

Exact caps (three-pair medians; [samples and provenance](https://github.com/dylanfetch/openttd-rust/pull/187#issuecomment-6072318657)):

| Scenario | Cap |
| --- | ---: |
| Opus manual | 1.422193877551 |
| Grok manual | 1.405807365439 |
| Padhattan 1996 manual | 1.662674650699 |
| Padhattan 2000 manual | 1.982905982906 |
| TGP 256 | 1.440443213296 |

## Phase 2: current work, in order

Integration of reviewed work comes before new starts. Never hold more than six
unintegrated component branches (#157). Independent items (#156 slices) may run
in parallel with this list.

1. **Finish #147 fleet replacement** in the direct form; refresh its state-only
   WIP from integrated main. Preserve full native CommandCost and its quirks.
2. **#168 conversions, one PR per component:** train/reservation #189 first
   (O(n^2) consist walk, per-step `nearby` Vec), aircraft #190, then company #192
   with widened scope, then trees, town and disaster, then cold cargo/orders.
   UTF-8/string-consumer #191 is a separate selected small PR at any point.
   #192 keeps StopAI direct; only proven startup/post VM exceptions use stack
   continuation records. No per-call heap task, generic dispatch or hot record copy.
3. **New components** (#148 town lifecycle, then #150 industry construction)
   start only once the road play saves are at or below 2.0x (they are) and #168's
   train slice is integrated. When fewer than two unstarted selections remain, a
   fresh Astra high planner replenishes whole simulation owners, planned in the
   direct form.

The #108 decision keeps C++ map arrays and direct bundled `noexcept` services.
Revisit it only if a post-#168 profile shows map/pool crossings dominating.

## Resume checkpoint

| Issue / PR | Branch (worktree suffix), head | State and next step |
| --- | --- | --- |
| #147 | `fleet-replacement-ownership-147` (`fleet-replacement`), `9d81c9a0e0` | Fresh Sol high owner completing controller; integrated main refreshed. |
| #189 / #168 | `train-direct-189` (`train-direct-189`), `c28136c093` | Fresh Sol high owner converting train and reservation services. |
| #190 / #168 | `aircraft-direct-190` (`aircraft-direct-190`), `c28136c093` | Fresh Sol high owner converting aircraft services and existing native adapters. |
| #191 / #193 | `utf8-hot-191` (`utf8-hot-191`), `9ae81d9b25` | Root implementation; validation running, fresh Sol high review next. |
| #192 / #168 | none | Company direct conversion after train/aircraft; exception disposition in issue. |
| #188 / #156 | none | Aircraft breakdown slice queued for a fresh slot; other plans stay in #156. |

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
