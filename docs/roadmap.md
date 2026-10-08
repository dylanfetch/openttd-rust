# OpenTTD-Rust near-term roadmap

Root owns selection here; `AGENTS.md` and `docs/rust-migration.md` define process.
If an issue conflicts, follow this roadmap and report it to root. Keep this
forward-looking, about 200 lines; completed work is one row, with evidence in PRs.

## Where the fork stands (2026-10-08, `dbce82a309`)

- Eighteen ownership ports retire 24,221 original C++ lines, about 6.3% of roughly
  384k non-vendored `src/` lines (18,898 excluding town-name and road-movement
  data). The last two integrations retire 1,394 lines for 1,052 glue + 700 net
  tooling. Company's canonical save/writer adapters explain the cost; its
  allocating boundary still needs #168. Post-merge CI is running.
- Latest aircraft-base play-opus is **2.626x**; company is 2.624x on the same host.
  Generation is 1.411x / 1.552x; root accepts company's temporary allocating
  boundary cost, with 50 ms process-wait resolution limiting attribution (#149).
  Best-known budgets remain play-opus-55-167-002 2.54x,
  play-grok-159-001 2.55x, padhattan 2.17x, generate-tgp-256-1 1.41x.
  Road is 87% of the gap, and most of it is boundary overhead, not game logic
  (#155, #168).
- CI is on demand (#171). An ordinary PR push costs about 2 job-minutes instead
  of 80. A full run costs about 104 job-minutes and 30 minutes wall time, once per
  final head.
- Five component branches are unintegrated (#157 cap): #151, #152, #176, #147
  and #178's road conversion. #176 replaces closed #153. Orders and ship have
  joined aircraft; road is joining company; cargo and fleet need an update.

## Fourth steering review (2026-10-07)

A user-directed review (Claude Code, `claude-opus-5-5`) audited #170/#171, which
a user-directed `gpt-6.1-sol` root session selected and integrated. Independent
audits covered the CI workflows and local tools. There were no port audits and
no profile, because no game code or branch head changed since the third review.

The CI change is sound and stays. Its gate refuses partial, skipped, cancelled
and stale runs, and the harness and benchmark timing are unchanged. But #171 is
the fork's largest tooling addition (0 / 3267 / 0 / 0), and #173 now tracks its
gaps: full validation tests the head, not the merge; a docs-only push can cancel
a merged PR's post-merge run; dispatched `full` has never run live; preflight
fails every full build (the original alone gives 124 GCC 15 warnings); a killed
driver orphans its builds; and archives hold 103 GB. Actions:

1. **Resume Phase 2 at item 1 (#145).** The CI maintenance is finished. More
   process tooling is not a fallback; only #173 is selected.
2. **#173 runs alongside, with one agent and net non-positive lines** in `tools/`
   and `.github/`. Until its merge-ref fix lands, merge the base into a branch
   immediately before requesting that branch's final full run.
3. **The first dispatched `full` run is a live test.** If it fails for a workflow
   reason, fix that under #173 before other integrations. A workflow-changing PR
   may unset `CI_ON_DEMAND` to use the `ci:full` label path. Restore it to `true`
   after merge and record both changes in the PR. Never use `--admin`.
4. **Until #173 lands**, use preflight for the commit checker only, without
   cleaning up the pre-existing warnings. After a killed or timed-out driver,
   stop leftover cmake, cargo or openttd processes in that worktree before
   rebuilding.

## Earlier steering, still in force

First review (10-04): call shared services directly (#107); measure before map
decisions (#108); keep tooling proportionate (#109); prioritize core simulation.
Second (10-04): harness end moments (#154, done); speed report (#155); coverage
gaps (#156); WIP cap of six branches (#157). Third (10-07):
- per-call task/future/`Rc` boundaries, opcode dispatch and whole-record reads
  are the speed problem; convert to direct typed calls (#168);
- the road conversion narrows RoadObserve (#155);
- the speed budget is a ratchet;
- the benchmark needs a non-road save (#156);
- reviews use `gpt-6.1-sol` high, with a fresh reviewer each round that fixes its
  own findings; close finished agents.

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

Harness and process: #72 harness (#85), #84 play saves (#87), #97 provenance
freeze (#100), #88 Ruff (#92), #75 partial-pixel fidelity (#91), #90 world-state
design (`docs/design/world-state.md`).

Maintenance (issue, PR, tooling lines): #109 scenario modules #112 (2414, moved
code); #107 direct services #113, #115; #86 transport save #110 (141) and
aircraft fixture #132 (347); #108 map decision #116; #154 harness endpoints #160
(277); #158 MinGW nightly #159; #155 speed report #163 (212); #156 road witnesses
#161, #162, #164, #167 (581); #170 on-demand CI and validation tools #171 (3267,
no game logic); #173 validation repairs #174 (`cd0297938c`, net tooling -2).
CI-capacity batches #134, #141 and #166 integrated owners above.

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
   closes many gaps and gives the speed budget a non-road benchmark. The user
   supplied the later Padhattan save (#179/#180); defaults and six-year soaks
   pass. Establish its idle three-pair baseline without raising old budgets.
3. **Reviewed maintenance batch (#175/#177/#180).** One concrete integration PR
   validates crossing sound, landing RNG evidence and the later save together:
   about 104 rather than 312 full-CI job-minutes. Independent component reviews
   remain the source gate; no individual PR bypasses CI.

## Phase 2: current work, in order

Integration of reviewed work comes before new starts. Never hold more than six
unintegrated component branches (#157). Independent items (#169, #156 slices)
may run in parallel with this list.

1. **#155 road conversion (#178).** Remove the Task/Future/Rc protocol for all 22
   services and 14 entries, and narrow RoadObserve: field getters or a hot
   record, no `IsBus` in the view, type-filter in `close`, single-pass `nearby`,
   owner resolved once per entry, and nested `GetCurrentMaxSpeed`. Add the ratchet.
   Join integrated company before final evidence. The expected result is about
   1.7-1.9x; use the later-save benchmark once its baseline is established.
2. **Integrate #151 cargo storage** (already direct) on the fresh baseline.
3. **Integrate reviewed #176 orders, then #152 ship.** Both are direct; ship
   names orders as an ancestry dependency. Finish their final-base checks before
   starting another component. Then finish #147 fleet replacement.
4. **#168 train and train-reservation conversion**, including the O(n^2) consist
   walk and the per-step `nearby` Vec; then aircraft and company; then trees,
   town and disaster. One PR per component.
5. **New components** (#148 town lifecycle, then #150 industry construction)
   start only once the road play saves are at or below 2.0x and #168's train
   slice is integrated. When fewer than two unstarted selections remain, a fresh
   Astra high planner replenishes whole simulation owners, which are planned in
   the direct form.

The #108 decision keeps C++ map arrays and direct bundled `noexcept` services.
Revisit it only if a post-#168 profile shows map/pool crossings dominating;
the current profile instead identifies entry and record-copy overhead.

## Resume checkpoint

Every branch below must merge the base before its next push; the primary
checkout's `tools/ci.py` can request CI for any fork PR.

| Issue / PR | Branch (worktree suffix), head | State and next step |
| --- | --- | --- |
| #155 / #178 | `road-direct-155` (`road-direct-155`), `624ff3d704` | Direct conversion and ratchet published; join company, resolve ABI test-ID overlap, finish final checks, review and idle timing. |
| #139 / #151 | `cargo-storage-movement-139` (`cargo-storage`), `ff912bcfcb` | Accepted at `a25a7d41e2`; final base and CI after item 2. |
| #138 / #176 | `orders-direct-138` (`order-lifecycle`), `3ef37bc4fc` | Reviewed fixes verified by root; final native/pair/self pass; refresh and full CI remain. |
| #146 / #152 | `ship-controller-ownership-146` (`ship-controller`), `df7a1d4d2e` | Joins accepted orders; fresh reviewer found no issue so far, final checks running. |
| #147 | `fleet-replacement-ownership-147` (`fleet-replacement`), `68d660adc9` | State-only WIP, no PR. |
| #169 / #175 | `crossing-sound-169` (`crossing-sound-169`), `9b20e639d8` local | Reviewed; native/Cargo and default 214 pass; selected maintenance batch. |
| #156 / #177 | `aircraft-landing-rng-156` (`aircraft-landing-156`), `bc07a5c7dc` | Reviewed RNG wording fix verified; paired/self landing 5/5 pass; selected maintenance batch. |
| #179 / #180 | `padhattan-2000-179` (`padhattan-2000-179`), `51d3541f86` | Reviewed unchanged user save; native/default/soak/old rails pass; batch plus new idle benchmark remain. |

The first live dispatched full (#145), #174 bootstrap and #149 actual merge passed.
`CI_ON_DEMAND=true` is restored; both local timing and remote push holds are released.
Standing #156 work: aircraft landing RNG (plan in #156), then the
2026-10-07 audit list there. Preserve the pinned reference, paused curve
worktrees and evidence branches `evidence-disaster-vehicles` (`a775543162`) and
`evidence-water-regions` (`c752070cde`); do not reapply effect helper `73ccd511fb`.
Build and test with `--jobs 2`; standalone fixture games share the benchmark lock.

## Choosing the next task

Take the first unblocked item above: review/integration, Phase 1, then Phase 2.
Paused, deferred and out-of-scope issues are not fallbacks. Root selects further
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
