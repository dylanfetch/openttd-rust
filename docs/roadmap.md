# OpenTTD-Rust near-term roadmap

Root owns selection here; `AGENTS.md` and `docs/rust-migration.md` define process.
If an issue conflicts, follow this roadmap and report it to root. Keep this
forward-looking, about 200 lines; completed work is one row, with evidence in PRs.

## Where the fork stands (2026-10-08, `f604e30d50`)

- Eighteen ownership ports retire 24,221 original C++ lines, about 6.3% of roughly
  384k non-vendored `src/` lines (18,898 excluding town-name and road-movement
  data). The last two integrations retire 1,394 lines for 1,052 glue + 895 net
  tooling. Company's canonical save/writer adapters explain the cost; its
  allocating boundary still needs #168. Company's post-merge checks passed.
- Reviewed road conversion lowers idle play-opus from **2.601x to 1.415x**, Grok
  2.503x to 1.405x, Padhattan 1996 2.264x to 1.678x, and mixed 2000 2.401x to
  1.911x. Ratchet caps are 1.415444x, 1.405295x, 1.678295x and 1.910615x.
  Generation remains about 1.55x; its cap stays **1.41x**. Root accepts only
  company's temporary allocating-boundary exception (#149/#168), with 50 ms
  process-wait resolution limiting attribution. The separate road profile
  identifies remaining pathfinding, typed service and original clock costs.
- CI is on demand (#171). An ordinary PR push costs about 2 job-minutes instead
  of 80. A full run costs about 104 job-minutes and 30 minutes wall time, once per
  final head.
- Five component branches are unintegrated (#157 cap): #151, #152, #176, #147
  and #178's road conversion. #176 replaces closed #153. Road, cargo, orders and
  ship have accepted source reviews; #182 selects their capacity integration.

## Fourth steering review (2026-10-07)

A user-directed review (Claude Code, `claude-opus-5-5`) audited #170/#171, which
a user-directed `gpt-6.1-sol` root session selected and integrated. Independent
audits covered the CI workflows and local tools. There were no port audits and
no profile, because no game code or branch head changed since the third review.

The CI change stays; harness and benchmark timing are unchanged. #173/#174
repaired actual-merge validation, protected-push cancellation, warning baselines,
orphaned builds and future archive exclusions with net non-positive tooling.
Live full requests passed. Keep `CI_ON_DEMAND=true`, enforce exact-head full
validation, and never merge with `--admin`. More process tooling is not a
fallback; resume the selected simulation queue. Earlier 103 GB archives remain.

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
| Issues | Maintenance PRs | Integration | Commit | Metrics |
| --- | --- | --- | --- | --- |
| #169, #156, #179 | #175, #177, #180 | #181 | `f604e30d50` | 4 / 193 / 0 / 0 |

CI-capacity batches #134, #141 and #166 integrated owners above.

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness and speed maintenance

The harness is `python3 tools/migration.py simulate` (`docs/rust-migration.md`,
"Simulation comparison"); its default set runs in CI. Harness regressions are
always the first priority. Port differences go in `KNOWN_FAILURES` with an issue,
never in masks.

1. **Speed ratchet (#155).** Commit a per-scenario budget of best-known
   candidate/reference ratios, with today's figures above as the starting values.
   Benchmark both road play saves, both Padhattan saves and generate-tgp-256-1 with
   `simulate <name> --benchmark 3 --jobs 2` on an idle host. A PR may not exceed
   the budget by more than 3% without a stated reason accepted by root, and
   improvements lower the budget. Targets: <=1.9x on the road play saves after
   road conversion; <=1.5x on play saves and <=1.15x on generation when #168 closes.
2. **#156 coverage, standing capacity.** Random and crash branches first. A mixed
   save with trains (PBS junctions, crossings), ships, aircraft and subsidies
   closes many gaps and gives the speed budget a non-road benchmark. The user
   supplied the later Padhattan save (#179/#180), integrated via #181. Defaults,
   six-year soaks and idle three-pair comparisons pass. Presence of PBS signals,
   buoys and subsidies does not establish every route, traversal or multiplier;
   remaining branches stay in #156.
   **Selected next input (#183):** the owner's 2006-09-21 Padhattan save adds
   monorail, bridges, tunnels, canal and locks. Import unchanged through existing
   rail/play scenarios, inventory presence separately from witnessed traversal,
   and run self/pair/soak checks. Source review and root fix verification passed
   in #185; select its join after the four owners in #184's final capacity batch.
   Preserve the established five benchmark cases.

## Phase 2: current work, in order

Integration of reviewed work comes before new starts. Never hold more than six
unintegrated component branches (#157). Independent items (#169, #156 slices)
may run in parallel with this list.
**#184 is blocked by the speed ratchet:** final Opus/Grok/mixed-2000 timings
exceed cap plus 3%. Isolate the regression using its retained joins before full
CI or another component start; keep the budgets unchanged.

1. **#155 road conversion (#178).** Remove the Task/Future/Rc protocol for all 22
   services and 14 entries, and narrow RoadObserve: field getters or a hot
   record, no `IsBus` in the view, type-filter in `close`, single-pass `nearby`,
   owner resolved once per entry, and nested `GetCurrentMaxSpeed`. Add the ratchet.
   Accepted source and final native checks pass; whole harness 233/233, road
   soaks 58/58 and five idle benchmarks pass. Ratchet is committed in #178.
2. **Integrate #151 cargo storage** (already direct) on the fresh baseline.
3. **Integrate reviewed #176 orders, then #152 ship.** Both are direct; ship
   names orders as an ancestry dependency. Finish their final-base checks before
   starting another component. Then finish #147 fleet replacement.
   **Selected capacity batch (#182):** join #178, #151, #176 and #152 in that
   order from integrated #181, then join reviewed fixture #185. One concrete
   integration PR costs about 104 rather than 520 full-CI job-minutes. Preserve
   each component's source review;
   a fresh Sol high reviewer fixes ABI/module conflict resolutions. Widened audit
   selectors retain company registrations and orders move to free IDs 260-266.
   Replace cargo capacity's residual pointer selector with three typed reads;
   preserve call order and include the source delta in the fresh review.
   Run combined native/Cargo, default, affected soaks/comparisons and five idle
   three-pair benchmarks before full CI. Bisect retained joins on regressions;
   component metrics use successive joins, road retirement is old boundary glue.
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

Active checks finished after the user's wind-down request.
Resume with reviewed #184's speed regression before CI/integration or new starts.
Its source review is accepted; a later source fix needs fresh review. Merge the
latest base before full CI; conflict-free docs refreshes need no new source review.

| Issue / PR | Branch (worktree suffix), head | State and next step |
| --- | --- | --- |
| #155 / #178 | `road-direct-155` (`road-direct-155`), `636be8104f` | Reviewed source and ratchet joined into #184. |
| #139 / #151 | `cargo-storage-movement-139` (`cargo-storage`), `ff912bcfcb` | Accepted source joined #184; its typed capacity delta has fresh review there. |
| #138 / #176 | `orders-direct-138` (`order-lifecycle`), `3ef37bc4fc` | Reviewed fixes verified; joined #184. |
| #146 / #152 | `ship-controller-ownership-146` (`ship-controller`), `df7a1d4d2e` | Reviewed with accepted orders; joined #184, including natural lock checks. |
| #147 | `fleet-replacement-ownership-147` (`fleet-replacement`), `68d660adc9` | State-only WIP, no PR. |
| #182 / #184 | `reviewed-owner-batch-182` (`reviewed-owners-182`), `fa0baea911` | Review/native/affected semantics pass; timing blocked on Opus 1.479898x, Grok 1.448534x and mixed 2.052632x. Full CI not requested. |
| #183 / #185 | `import/padhattan-ridge-2006` (`padhattan-2006`), `d287c8e882` | Fresh review and root doc-fix verification pass; unchanged save joined #184. |

The first dispatched full (#145), #174 bootstrap, #149 and #181 actual merges passed.
`CI_ON_DEMAND=true` is restored; both local timing and remote push holds are released.
Standing #156 work follows the remaining 2026-10-07 audit list there; landing
RNG and the mixed save are integrated. Preserve the pinned reference, paused curve
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
