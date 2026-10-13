# OpenTTD-Rust near-term roadmap

Root owns selection here; `AGENTS.md` and `docs/rust-migration.md` define process.
If an issue conflicts, follow this roadmap and report it to root. Keep this
forward-looking, about 200 lines; completed work is one row, with evidence in PRs.

## Where the fork stands (2026-10-13, `648d44e940`)

- Twenty-three ownership ports. Whole fork against the pinned original: Rust
  55,079 / tooling 26,610 / C++ glue 24,570 / original C++ retired 36,162 (7.68%
  of all `src/` sources); simulation-source share 28,215 / 102,615 (27.50%, #226).
- #218 industry construction/lifecycle is integrated; four reviewed PRs wait
  for CI. #228/#232 are integrated; platform CI reuses versioned OpenGFX archives.
- Tenth-review benchmark at `46f725b426`, median of three `--benchmark 3`
  invocations: Opus 1.265, Grok 1.262, Padhattan-1996 1.275, Padhattan-2000
  1.355, Padhattan-2016 1.436, TGP 1.158; all within 3% of their caps, TGP above
  its 1.15 target. Per-field, opcode and task boundaries in aircraft, station,
  loading, town and orders cost about 0.31 of Padhattan-2016's ratio.
- Full validation takes about 43 minutes and serializes PRs (#233).
- Open #156 gaps include threaded execution, NewGRF/refit callbacks, positive
  gradual loading and station acceptance polling. No whole-game fidelity claim.

## Tenth steering review (2026-10-13, CI)

One root session since the eighth review integrated #209, #225, #228, #232 and
#218 with the process followed and CI green, then paused with #216, #217, #226
and #230 reviewed and waiting. CI now limits throughput more than agents do: a
full run takes about 43 minutes, and any push to `rust-migration` fails the
in-flight runs of other PRs. Audits found a reachable divergence in #218
(#234), none in #216 or #217, per-port accessor copies in all three, and caps
in #230 combined below the noise.

Corrections, in order:
1. **CI throughput first (#233, AGENTS.md).** Assign #233 to an implementation
   agent at session start. Integrate the four paused PRs by readiness around
   it, holding roadmap commits while a full run is in flight until #233 lands.
   Runner size and account plan are the user's decision; #233 keeps them a
   repository setting.
2. **Fix #234** (industry water-check tiles accept ship depots and locks) in its
   own small PR.
3. **Fix before integration, per the PR comments.** #217: one crossing before
   the per-station empty-queue check, shared accessors, no `capacity_remaining`.
   #216: road table straight to the shared functions, Rust-owned building
   counts read directly, shared accessors. #230: recalibrate at its new base
   rather than taking the lower of two calibrations. #218's departures are on
   #168.
4. **Shared accessors in review (AGENTS.md).** #216, #217 and #218 each added
   copies of company, vehicle or map accessors despite the rule. Review reports
   now name any new callback that duplicates a shared accessor.
5. **Aircraft conversion next (#199).** Aircraft costs 0.13 of Padhattan-2016's
   ratio (per-field calls, `src/rust/aircraft_adapter.hpp:31-60`), the largest
   recoverable item. It is unblocked and starts when a branch slot frees.
6. **Budgets for #212 and #213.** TGP has about 11 ms of slack. #212's scope is
   about 177 of TGP's 352 reference ms over 14.4M tile visits: at most about
   0.8 ns extra per visit, and no Rust-to-C++-to-Rust crossing for tile types
   Rust owns. #213 has about 6 ns per vehicle-tick on Opus. Prototype each hot
   path under `perf` against its budget before building the port.
7. **New components** still wait until two of the four drafts integrate.

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
#148 and select the tile loops and vehicle base tick. Eighth (10-12): integrate
by readiness, no draft waiting on another's services; Rust state at the
original's scope; the original-retirement metric (#223); caps from the median of
three invocations; ownership before more conversion. Ninth (10-12): remove
merged worktrees with plain git and archive nothing.

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
| #150 | Industry construction and lifecycle | #218 | `648d44e940` | 3066 / 701 / 1433 / 1736 |

Harness and process: #72 harness (#85), #84 play saves (#87), #97 provenance
freeze (#100), #88 Ruff (#92), #75 partial-pixel fidelity (#91), #90 world-state
design (`docs/design/world-state.md`).

Earlier maintenance: #109/#112, #107/#113/#115, #86/#110/#132, #108/#116,
#154/#160, #158/#159, #155/#163, #156/#161/#162/#164/#167, #170/#171,
#173/#174, #178/#185/#187 in #184, #169/#175/#177/#180 in #181, #200/#201,
#188/#196, #191/#193, #198/#204, and the #168 conversions #190/#194,
#189/#195, #203/#205, #202/#208 with #206/#207 and #210/#211; evidence
remains in those PRs. Rows through
#215 use legacy retirement including deleted glue; new rows use original
retirement, with net glue in the PR (#223).
| Issues | Maintenance PRs | Integration | Commit | Metrics |
| --- | --- | --- | --- | --- |
| #199 (shared road services) | #215 | direct | `4b130309a5` | 2288 / 565 / 1331 / 1354 |
| #192 (company direct services) | #209 | direct | `8fe6f5ed56` | 1495 / 159 / 954 / -3; net glue +424 |
| #224 (disk hygiene) | #225 | direct | `898eb33c17` | 0 / 149 / 0 / 0; net tooling -546 |
| #227 (loading context fidelity) | #228 | direct | `9ee89f69ad` | 3 / 47 / 0 / 0 |
| #231 (OpenGFX cache) | #232 | direct | `21e2c9ea60` | 0 / 0 / 0 / 0; workflow net -6 |

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness and speed maintenance

The default semantic harness runs in CI; regressions come first. Port differences
go in `KNOWN_FAILURES` with an issue, never in masks (`docs/rust-migration.md`).

1. **CI throughput (#233).** Parallel native jobs and simulation shards,
   concurrent PR validation, no re-validation of an already-validated tree,
   caches under the limit. Takes CI capacity ahead of other work.
2. **Speed ratchet (#155).** `SPEED_BUDGETS` in `tools/simulation/roads.py`
   holds the caps: the median of three `simulate <name> --benchmark 3 --jobs 2`
   invocations on an idle host. A PR may exceed a cap by 3% unless root accepts
   a stated reason that is not a boundary exception. A PR lowers a cap only when
   its median of three beats it by more than the invocation spread (about 2%).
   Targets: play saves 1.5x, generation 1.15x. The null driver redraws whenever
   1 ms has elapsed, so a slower candidate draws more frames.
3. **#156 coverage, standing capacity.** Random and crash branches first. The
   tenth review added station stale-link restriction, town station ratings,
   exclusivity and unwanted months, and #218's NewGRF and game-script paths.
4. **#234 fidelity fix**, then **#223 metric (#226)** and **#229 caps (#230,
   recalibrated at its new base)**.

## Phase 2: current work, in order

Integrate reviewed work first, in order of readiness; at most six unintegrated
branches (#157). Independent items (#156, #223, caps) may run in parallel.

1. **#217 station/loading**, with the changes in its PR comment.
2. **#216 town, narrowed**, with the changes in its PR comment; then town
   founding/generation, the house tile loop, and town commands, authority
   actions and deletion as their own ports.
3. **#199 aircraft conversion** when a branch slot frees (tenth review item 5).
4. **New ownership** after two drafts integrate: #212 landscape tile loops and
   #213 base vehicle tick within the tenth review's budgets; then #219 signal
   blocks and #220 engine lifecycle (#220 after #213 and #148).
5. **Conversions:** train and ship after #213; the #168 tail (orders, effect,
   disaster, #218's accessors) never blocks ownership.

The #108 decision keeps C++ map arrays and direct bundled `noexcept` services.
#199 selects a typed shared vehicle layer with per-operation reads.

## Resume checkpoint

Resume after the user's pause. #218 is integrated at `648d44e940` with green
post-merge CI. Four PRs wait at their reviewed heads for a base update, the
tenth review's PR-comment changes and exact-head full CI. #233 and #234 are
new and unstarted.

| Issue / PR | Branch (worktree suffix), head | State and next step |
| --- | --- | --- |
| #214 / #217 | `station-loading-direct-214`, `f53ea99f6d` | Reviewed, including #228 context conflict; waiting on base update and exact-head full CI. Preserve the per-vehicle loading context; apply the tenth-review comment. |
| #148 / #216 | `town-lifecycle-148`, `079350b3a7` | Narrowed/state/guard and company-base fixes reviewed; waiting on base update adopting #218's native cost helpers, the tenth-review comment, narrow review and exact-head full CI. |
| #223 / #226 | `metric-original-223`, `e20519eb78` | Identity/inventory fixes reviewed; waiting on base update, roadmap/template reconciliation and exact-head full CI. |
| #229 / #230 | `speed-caps-229`, `d7949a970a` | Recalibration reviewed; waiting on base update, recalibration at the new base (tenth-review comment), cap-data review and exact-head full CI. |

Selected but unstarted: #212/#213, then #219/#220. ABI blocks: train through 385,
tree additions 386-389, fleet 390-399, company 400-429, town 430-459, road 460-499,
station/loading 500-559, industry 560-619 (572 consumed shared ByteSpan in #218), CommandCost 620-629,
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
