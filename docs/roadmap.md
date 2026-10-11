# OpenTTD-Rust near-term roadmap

Root owns selection here; `AGENTS.md` and `docs/rust-migration.md` define process.
If an issue conflicts, follow this roadmap and report it to root. Keep this
forward-looking, about 200 lines; completed work is one row, with evidence in PRs.

## Where the fork stands (2026-10-11, `4b130309a5`)

- Twenty-two ownership ports record 31,359 retired C++ lines in the table below.
  These counts include town-name and movement tables, comments and blanks;
  they do not measure complete simulation coverage. Against the pinned original,
  the whole-fork metric is Rust 51,602 / tooling 26,279 / C++ glue 22,752 /
  original C++ retired 34,429. Recent conversions add no new owner. Town #148
  and industry construction #150 remain drafts; tile loops and base vehicle
  maintenance remain in C++, with #212/#213 selected to extend ownership.
- Latest independent idle-host timing in #215: Opus 1.279x, Grok 1.243x,
  Padhattan-1996 1.285x, Padhattan-2000 1.367x and TGP 1.174x. All five pass
  their ratchets; improved caps fell. Play saves meet 1.5x; TGP still misses
  1.15x. Road crossings meet both budgets; the planned instruction diagnostic
  misses. The prior 27% whole-glue wall-time profile was not remeasured (#199).
- Four unintegrated component branches remain. #156 tracks script exceptions,
  multiplayer and scenario gaps; startup cases exercise both company Post paths,
  with mandatory common witnesses still pending in #209.

## Seventh steering review (2026-10-09)

The review found three cited probes outside CI, colliding ABI IDs, an interrupted
review used as #208's only review, loose speed caps, and conversions crowding out
new ownership. Its corrections remain in force.

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

Earlier maintenance: #109/#112, #107/#113/#115, #86/#110/#132, #108/#116,
#154/#160, #158/#159, #155/#163, #156/#161/#162/#164/#167, #170/#171,
#173/#174, and #178/#185/#187 in #184; evidence remains in those PRs.
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
| #210 (default probes) | #211 | direct | `cbebf6e5fe` | 0 / 25 / 0 / 0 |
| #202 (#168 trees) | #208 | direct | `0e459aa81f` | 1604 / 91 / 347 / 53 |
| #199 (shared road services) | #215 | direct | `4b130309a5` | 2288 / 565 / 1331 / 1354 |

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness and speed maintenance

The default semantic harness runs in CI; regressions come first. Port differences
go in `KNOWN_FAILURES` with an issue, never in masks (`docs/rust-migration.md`).

1. **Speed ratchet (#155).** `SPEED_BUDGETS` in `tools/simulation/roads.py` is
   the authority for the five caps, run with `simulate <name> --benchmark 3
   --jobs 2` on an idle host. A PR may exceed a cap by at most 3%, unless root
   accepts a stated reason that is not a boundary-rule exception. Improvements
   lower the caps in the same PR (seventh review, item 4). Targets: play saves
   1.5x or less (met), generation 1.15x or less. The null driver redraws
   whenever 1 ms has elapsed, so a slower candidate also draws more frames.
2. **#156 coverage, standing capacity.** Random and crash branches first. The
   seventh review added fleet, industry, trees and company gaps. Default company
   startup cases set cap four above two fixture actors and exercise both Post paths.

## Phase 2: current work, in order

Integrate reviewed work first; at most six unintegrated branches (#157).
Independent items (#156, #210) may run in parallel.

1. **Integrate:** finish company #209 first. Shared road #215, trees #208 with
   lowered caps, default probes #211 and the company reset doc fix are integrated.
2. **#199 shared vehicle and map layer**, one PR per vehicle type: road #215
   integrated; next aircraft (about 150 crossings per tick), train, ship. Delete accessor copies
   and reports crossings per vehicle tick.
3. **#148 town lifecycle (#216)** owns state, custom-name/text bytes and control in the direct typed
   form. Reuse integrated #217 station activity and #209 company context before review.
4. **#168 station tick and loading (#214 / #217)**: consolidate typed handles and
   reuse integrated #215 vehicle/engine and #209 company-context services. Then disaster and the cold
   cargo/orders conversions, which never block new ownership work.
5. **New ownership:** finish #150 industry construction; then #212 landscape tile
   scheduling and clear/water/rail/road/tunnel-bridge loops, then #213 base
   vehicle tick and maintenance. #212/#213 start after integrated #208 and
   #199 road; retain canonical map/shared Vehicle fields behind the typed layer.
   Then selected #219 owns complete signal-block updates/worksets; #220 owns
   engine-design lifecycle/name state, including unpooled load prototypes.
   #220 also needs integrated #209/#148; prefer starting after #213.

The #108 decision keeps C++ map arrays and direct bundled `noexcept` services.
#199 selects a typed shared vehicle layer with per-operation reads.

## Resume checkpoint

Stopped at the user's request on 2026-10-11. #211/#208/#215 are integrated.
Four drafts are clean and pushed; all agents stopped. No new components, base updates or full-CI requests during wind-down.
Resume #209 first, then final #217, #216 and #218 in that dependency order.
Evidence and exact validation status stay on the linked PRs/issues.

| Issue / PR | Branch (worktree suffix), head | State and next step |
| --- | --- | --- |
| #192 / #209 | `company-direct-192`, `aaeafc9b69` | Draft; startup pair/self/soak and mutation pass. Add common Post witnesses, natural timer provenance and canonical company context, then fresh review/full CI. |
| #148 / #216 | `town-lifecycle-148`, `0e98404f21` | Draft; latest name retry batch passes Cargo/Ruff/preflight but native build was interrupted. Last fully verified `e0a1132aba`; finish full lifecycle/containers and integrated services, then review/full CI. |
| #214 / #217 | `station-loading-direct-214`, `d456668553` | Draft; typed station handles, map/timer reuse and 16-case pair pass. Consume integrated #215 and future #209; finish shared station contracts, final checks/review/full CI. |
| #150 / #218 | `industry-construction-150`, `8bf3a71c90` | Draft; construction/tile lifecycle and company-transfer scenarios pass; founder mutation fails as required. Consolidate integrated shared services/dimension captures, then final checks/review/full CI. |

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
added and original C++ retired (deleted or portable-only). Retire more C++ than
glue plus tooling, or explain why. Data tables inflate retirement; judge progress
by game logic owned and avoid utility-kernel extraction.
