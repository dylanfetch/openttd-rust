# OpenTTD-Rust near-term roadmap

Root owns selection here; `AGENTS.md` and `docs/rust-migration.md` define process.
If an issue conflicts, follow this roadmap and report it to root. Keep this
forward-looking, about 200 lines; completed work is one row, with evidence in PRs.

## Where the fork stands (2026-10-12, `04b32068f6`)

- Twenty-two ownership ports. Whole fork against the pinned original: Rust
  51,602 / tooling 26,308 / C++ glue 22,752 / original C++ retired 34,429 (7.3%
  of 470,612 `src/` lines), up 11 since the seventh review (#223).
- Four drafts: #218 industry construction (real ownership, nearest), #209
  company (reviewed conversion), #217 station/loading (conversion), #216 town
  (mostly conversion, with a reachable crash).
- Idle-host timing, nine pairs each: Opus 1.273, Grok 1.300 (cap 1.243),
  Padhattan-1996 1.305, Padhattan-2000 1.386, TGP 1.185 (target 1.15),
  Padhattan-2016 1.518 (no cap). Game loop alone: Padhattan-2000 1.55x.

## Eighth steering review (2026-10-12)

One root session since the seventh review integrated #211, #208 and #215,
started #216-#218 and selected #212, #213, #219 and #220, with CI green and the
process followed. Audits of #215-#218 found a reachable crash in #216, drafts
waiting on each other for shared accessors, a metric that counts deleted glue,
and speed caps inside the noise.

Corrections, in order:
1. **Integrate by readiness (AGENTS.md).** No draft waits for another draft's
   services, and a reviewed PR gains no scope before it integrates. The first
   PR to integrate adds a missing shared accessor in final typed form; others
   adopt it at their next base update. #218 first with #209 alongside, then
   #217, then a narrowed #216; each PR comment lists what must change.
2. **State keeps the original's scope (AGENTS.md).** #216's global house counts
   are `thread_local!`; GUI builds run commands on the main thread and the game
   loop on `ottd:game`, so a NewGRF house set crashes. Fix it in #216 with a
   default check against `thread_local!` in non-test kernel code.
3. **Narrow #216** to direct calls replacing `RustTownRun`, Rust storage with
   save staging, monthly/yearly loops, ratings, authority checks, population and
   radius. Placement, names and house stages, now fragments under C++ loops,
   move with the founding/generation and house tile-loop ports.
4. **Honest metric (#223):** original C++ retired apart from removed glue, plus
   a simulation share for "percent complete".
5. **Caps from the median of three `--benchmark 3` invocations**, plus a
   Padhattan-2016 cap (about 1.52); one measurement plus 3% is inside the noise.
6. **Ownership before more conversion.** No speed item gates ownership. #199
   aircraft (0.05-0.09, Padhattan only) may follow once #218 and #209
   integrate; train and ship wait for #213, which moves the shared base-vehicle
   slots. Orders (hot, positional) and effect (opcode) join the #168 tail.
7. **Design #212 and #213 for the caps.** `TileLoop_Clear` and `TileLoop_Water`
   are 126 of TGP's 364 reference ms; ported trees-style (2.6x the original's
   per-tile instructions) they would take TGP to about 1.5x. Prototype and
   measure the per-tile and per-vehicle-tick path against the original first.
8. **Start a new component only after two of the four drafts integrate.**

## Ninth steering review (2026-10-12, disk)

Deleted 231 GB of merged-work cruft (`tools/worktrees.py` archives, `simulate`
runtime copies, CI artifacts). Merged worktrees are now removed with plain git
and nothing archived (AGENTS.md); land #224 (Phase 1 item 4).

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
run by default (#198, #204); one shared typed vehicle and map layer (#199);
trees and the industry tick before company; a fixed hasher for the lookup-only
YAPF maps. Seventh (10-09): cited reference probes run in default CI (#210,
#211); milestone pushes and recorded task state, and an unfinished review counts
as none; improvements lower the caps; one ABI block per component; start town
#148 and select the tile loops and vehicle base tick.

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

Earlier maintenance: #109/#112, #107/#113/#115, #86/#110/#132, #108/#116,
#154/#160, #158/#159, #155/#163, #156/#161/#162/#164/#167, #170/#171,
#173/#174, #178/#185/#187 in #184, #169/#175/#177/#180 in #181, #200/#201,
#188/#196, #191/#193 and #198/#204; evidence remains in those PRs. Conversion
rows below count removed glue as retired; their original retirement is near
zero (#223).
| Issues | Maintenance PRs | Integration | Commit | Metrics |
| --- | --- | --- | --- | --- |
| #190 (#168 aircraft) | #194 | direct | `d8aed8754f` | 1279 / 352 / 1300 / 247 |
| #189 (#168 train) | #195 | direct | `3a2e8f6a10` | 4263 / 91 / 2812 / 853 |
| #203 (#168 industry) | #205 | direct | `9025f623e0` | 450 / 8 / 278 / 16 |
| #206 (YAPF hasher) | #207 | direct | `65ff465080` | 95 / 0 / 0 / 0 |
| #210 (default probes) | #211 | direct | `cbebf6e5fe` | 0 / 25 / 0 / 0 |
| #202 (#168 trees) | #208 | direct | `0e459aa81f` | 1604 / 91 / 347 / 53 |
| #199 (shared road services) | #215 | direct | `4b130309a5` | 2288 / 565 / 1331 / 1354 |

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness and speed maintenance

The default semantic harness runs in CI; regressions come first. Port differences
go in `KNOWN_FAILURES` with an issue, never in masks (`docs/rust-migration.md`).

1. **Speed ratchet (#155).** `SPEED_BUDGETS` in `tools/simulation/roads.py`
   holds the caps: the median of three `simulate <name> --benchmark 3 --jobs 2`
   invocations on an idle host. A PR may exceed a cap by 3% unless root accepts
   a stated reason that is not a boundary exception; improvements lower it in
   the same PR. Targets: play saves 1.5x, generation 1.15x. The null driver
   redraws whenever 1 ms has elapsed, so a slower candidate draws more frames.
2. **#156 coverage, standing capacity.** Random and crash branches first. The
   eighth review added the threaded game loop, multi-part road vehicles,
   station refit and loading callbacks, industry NewGRF construction callbacks
   and candidate-side town generation.
3. **#223 metric.** Original C++ retired apart from removed glue; small.
4. **#224 disk hygiene.** Delete `tools/worktrees.py`; `simulate` drops its
   runtime copies and keeps the newest 10 runs. Small; does not wait on ports.

## Phase 2: current work, in order

Integrate reviewed work first, in order of readiness; at most six unintegrated
branches (#157). Independent items (#156, #223, caps) may run in parallel.

1. **Integrate #218 and #209.** #218 updates from base, adopts #215's shared map
   services and drops dead slots; #209 moves its three pending steps to a #156
   follow-up. Each gets a fresh review and exact-head full CI.
2. **#217 station/loading**, with the changes in its PR comment.
3. **#216 town, narrowed**, then town founding/generation, the house tile loop,
   and town commands, authority actions and deletion as their own ports.
4. **New ownership** after two drafts integrate: #212 landscape tile loops and
   #213 base vehicle tick, designed for the caps first; then #219 signal blocks
   and #220 engine lifecycle (#220 after #213, #209 and #148).
5. **Conversions:** #199 aircraft after item 1; train and ship after #213; the
   #168 tail (orders, effect, disaster) never blocks ownership.

The #108 decision keeps C++ map arrays and direct bundled `noexcept` services.
#199 selects a typed shared vehicle layer with per-operation reads.

## Resume checkpoint

Paused on 2026-10-11 at the user's wind-down; resume with the work order above.
#211, #208 and #215 are integrated. Four drafts are clean and pushed. Evidence
and exact validation status stay on the linked PRs and issues; the eighth-review
PR comments list what each draft must change.

| Issue / PR | Branch (worktree suffix), head | State and next step |
| --- | --- | --- |
| #150 / #218 | `industry-construction-150`, `8bf3a71c90` | Draft; real ownership, audited clean. Update from base, adopt shared map services, drop dead slots, then fresh review and full CI. |
| #192 / #209 | `company-direct-192`, `aaeafc9b69` | Draft; reviewed, plus startup scenarios. No new steps: update from base, fresh review of new commits, full CI. |
| #214 / #217 | `station-loading-direct-214`, `d456668553` | Draft; fidelity clean. Remove its profile facility, revert portable-body edits, adopt #215 accessors, group reads. |
| #148 / #216 | `town-lifecycle-148`, `0e98404f21` | Draft; last native-verified `e0a1132aba`. Fix the `thread_local!` crash, narrow scope, adopt #215 accessors. |

Selected but unstarted: #212/#213, then #219/#220. ABI blocks: train through 385,
tree additions 386-389, fleet 390-399, company 400-429, town 430-459, road 460-499,
station/loading 500-559, industry 560-619 (572 shared ByteSpan lands with town), CommandCost 620-629,
and additional town lifecycle 630-669. Allocate only consumed layouts.

Preserve the pinned reference, paused curve worktrees and evidence branches
`evidence-disaster-vehicles` (`a775543162`) and `evidence-water-regions`
(`c752070cde`); do not reapply effect helper `73ccd511fb`. Build and test with
`--jobs 2`; standalone fixture games share the benchmark lock.

## Out of scope for the near term

More `src/3rdparty` ports (monocypher, squirrel, others). GUI rendering and
layout. Platform or toolchain expansion. Deferred behavior improvements stay in
`deferred-improvement` issues.

## Progress metric

Every port PR pastes `python3 tools/port-metrics.py`: Rust / tooling / C++ glue
added and original C++ retired (deleted or portable-only). Until #223 lands,
deleted glue also counts as retired; quote the whole-fork delta against the
pinned original for conversions. Retire more original C++ than glue plus
tooling, or explain why. Data tables inflate retirement; judge progress
by game logic owned and avoid utility-kernel extraction.
