# OpenTTD-Rust near-term roadmap

Root owns this file and updates it when a phase completes or priorities change.
`AGENTS.md` and `docs/rust-migration.md` define the rules; this file decides what
to work on next. If an issue conflicts with this roadmap, follow the roadmap;
a subagent stops and reports the conflict in its hand-off to root. Keep this
file forward-looking: completed work is one table row, and its evidence stays in
the PR (`AGENTS.md`, "Evidence budget").

## Where the fork stands (2026-10-07, integration `e16d01c869`)

- Eleven ownership ports retired 14,049 original C++ lines, about 3.7% of roughly
  384k non-vendored `src/` lines. Town names account for 3,848, mostly data, and
  road movement tables for another 1,475; the remaining retirement is 8,726 lines.
  This is still an early migration, not a mostly Rust simulation.
- Ten integrations since steering are complete. The latest ownership batch, #134, integrates
  cargo payment/delivery, ship YAPF, town growth and road controllers: 5,351
  retired lines against 2,689 glue and 1,780 tooling. Ship, town and road each
  retire more than their glue/tooling. This batch improves on the earlier small
  ports; do not count pending PRs or fleet's state-only checkpoint as completed.
- Landed overruns: effects added 965 glue/tooling for 548 retired; water adds
  883 for 407; disasters adds 1,101 for 968; cargo payment/delivery adds 988 for
  315. Pending industry periodic adds 1,150 for 924, company/economy 1,717 for
  1,394, and ship control 732 for 721. Each PR must explain its concrete cost;
  do not grow generic tooling to polish evidence or improve a metric.
- The tenth integration, #160, repairs deterministic harness endpoints: zero
  additional C++ retired or glue, 277 tooling lines added and 228 removed.
  Main post-merge checks are running. The separate nightly repair #159 passes
  its dispatched three-platform nightly; required PR checks still gate it.
- Preliminary old-policy play-save ratio is 2.453x; its source provenance limit
  and profile are recorded in #155. Road observation/task overhead leads the
  samples, so retain the planned road conversion; no raw map view is selected.
  Repeat the benchmark under the integrated deterministic execution policy.
- #156 now tracks the synchronous harness's excluded worker/abort interleavings.
  RNG/crossing/news coverage #161 and flooding #162 are reviewed or in review,
  not yet integrated. Eleven component branches plus one integration branch
  remain unintegrated. Start no new component until the cap is restored.
- Cargo #151 review caught quadratic list operations missed by the tiny packet
  corpus. Its fix restores near-linear scaling in the bounded probe and has
  source acceptance; final-base scenarios and CI remain. Drain the reviewed
  ownership queue after the CI repairs; the next stocktake is integration twelve.
- Reuse current fixtures and component scenario modules. Preserve one authority
  for each owned component; shared map/pool storage remains deliberate. Utility
  work and already accepted evidence are not substitutes for simulation loops.

## Second steering review (2026-10-04, evening)

A user-directed review (Claude Code, `claude-opus-5-5`, effort high) audited
road vehicles, town growth, cargo payment and pending rail YAPF #133 line by
line against the original bodies. All four are real ownership ports, and no
reachable crash or desync was found. The direction stands; keep porting core
simulation. The review found four problems to fix first:

1. **`rust-migration` is red** (#154). After #134, `compare` failed on a plain-run
   end moment. All 25 snapshots matched, but run length depends on wall time
   through link graph join pauses. Fix this before further integrations.
2. **The Rust build is 1.4-2.5x slower than the original** (#155). This is
   unmeasured and unbudgeted, and it compounds with each port. Road still uses
   the action protocol and per-call allocation for about 24 direct-eligible
   services, contrary to AGENTS.md.
3. **The harness reaches the common paths only** (#156). Accepted "evidence
   limits" pile up untracked. No save has a level crossing. The rail network
   closes 10 nodes, ship YAPF runs 2-3 ships, and Random-draw branches such as
   road's no-destination track choice are unreached. Keep one agent on this.
4. **Too much work in progress** (#157). Thirteen components were in flight,
   seven PRs conflict, and branches stack on unmerged branches. The cap is six
   unintegrated branches, and integration comes before new starts.

## First steering review (2026-10-04)

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
| #104 | Water-region cache and graph service | #114 | `9732d8bfa2` | 459 / 610 / 273 / 407 |
| #103 | Disaster scheduling, vehicles and event control | #118 | `d5edfcb4c5` | 1404 / 795 / 306 / 968 |
| #117 | Cargo payment and delivery | #123 via #134 | `adbe063372` | 396 / 618 / 370 / 315 |
| #119 | Ship YAPF and canonical path cache | #126 via #134 | `adbe063372` | 881 / 54 / 506 / 646 |
| #120 | Town growth and private state | #127 via #134 | `adbe063372` | 1430 / 668 / 625 / 1548 |
| #121 | Road controller/private state (includes 1475 movement-data lines) | #128 via #134 | `adbe063372` | 3159 / 471 / 1188 / 2842 |

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

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness maintenance

The harness is `python3 tools/migration.py simulate` (`docs/rust-migration.md`,
"Simulation comparison"); its default set runs in CI. Harness regressions are
always the first priority. Port differences go in `KNOWN_FAILURES` with an issue,
never in masks.

1. **#156 Coverage gaps, standing capacity.** One agent closes the tracked
   unexercised branches, Random-draw and crash branches first. Each port PR
   appends its gaps there.
2. **#155 Speed report and budget.** Add the per-scenario ratio and profile, then
   convert road's direct-eligible services. Port PRs report the play-save ratio.
   No individual port may worsen that ratio by more than 10% without a stated
   reason; review the trend at each stocktake. Record execution/thread policy
   with measurements and compare like-for-like before and after runs.
3. **#158 Existing MinGW i686 nightly repair.** Since steering, nightly main
   also fails before compilation on unavailable MSYS2 LZO/LLD packages. Restore
   dependencies and retain LZO support and the platform matrix. Land after
   the integrated #154, before component integrations; require a green main nightly run.

The #86 rail/aircraft fixture gate is complete. Controller ports still require
component-specific branch witnesses; extend the existing scenario modules and
reuse the supplied rail/ship save and authorized aircraft setup AI.

## Phase 3: current ownership work, in order

On resume: finish the additional main-CI repair #158, then drain the
reviewed queue (#141, then #142, #143, #144, #145, #149), then #155's road
conversion. Next finish #151, #152, #153 and
#147, then start #148 and #150 from integrated `rust-migration`. Never hold more
than six unintegrated component branches (#157).

Accepted search batch #140 combines component PRs #133/#135 using the actual
integrated #134 ancestry. It retains the component reviews, independent combined
review and all required CI before integration.

1. **#122 Complete rail YAPF search, caches and reservation.** Own all four
   searches, six specialization-specific global cache banks, rail-change
   invalidation, reservation traversal and rollback. Reuse the rail corpus and
   ship search machinery only where ordering matches; require branch witnesses.
2. **#124 Complete road YAPF search and path construction.** Own both track and
   depot searches, all node/queue/segment state and reconstruction. Replace
   #121's temporary result bridge using its canonical path cache; do not claim
   the cache twice. Reuse road scenarios and compatible search support.
3. **#125 Complete station cargo-service control and state.** Own loading order,
   service/cargo metadata, full loading/reservation/refit policy, acceptance,
   ratings/distribution and periodic service loops. Preserve #117's payment and
   production-flush owner and shared packet/flow/map storage. Reuse its fixtures.
4. **#129 Complete industry periodic production and builder ownership.** Own
   canonical cargo slots/histories, production and transport control, daily/monthly
   closure, farm/lumber loops, NewGRF production repeat/apply policy and weighted
   builder targets/retries/backoff. Reuse #117 and #125 boundaries and fixtures.
5. **#130 Complete rail vehicle control and private state.** Own consist/tick
   movement, reversal/crossings/crash, speed/service/day loops and controller
   reservation extension/rollback, with canonical train-private state and adapters.
   #122 search reservation does not cover controller rollback. Establish missing
   collision/crossing/reversal witnesses as part of the port before integration.
6. **#136 Complete aircraft control and airport movement ownership.** Own both
    controller passes, FTA traversal, terminal/helipad/block allocation, private
    aircraft state and canonical station airport masks, plus service/diversion/
    crash and save adapters. Reuse #132; add actual contention, dedicated helipad,
    closure/removal and crash witnesses. Coordinate #125's station lifetime.
7. **#137 Complete company financial and economy lifecycle ownership.** Own
    canonical finances/histories, bankruptcy/offer/acquisition/deletion control,
    world ownership-transfer traversal, periodic economy/prices and financial
    commands. Preserve real VM/deletion reentry and #129 ECMY fields. Require
    multiple-company recovery/transfer/deletion evidence, not only profitable play.
8. **#138 Complete orders, shared lists, backups and timetable ownership.** Own
    canonical vectors/current orders/shared links/backups, editing/validation/
    execution/destination resolution, timetable and depot-unbunching control.
    Preserve controller reentry, save formats and #83 unmasked timing evidence.
9. **#139 Complete cargo packet, list, flow and movement ownership.** Own packet
    contents, ordered station/vehicle containers, actions/caches/flows and the
    completed-link-job live-flow application. Preserve #117/#125/#74 policy
    boundaries, stable identity/iteration and shared pool-shell allocation.
10. **#146 Complete ship control and private-state ownership.** Own movement,
    locks, rotation/reversal, day/service/build policy and nearest-depot regional
    BFS with canonical state/rotation coordinates. Preserve #119's path owner;
    adapt every save/external writer and reuse the water corpus for real witnesses.
11. **#147 Complete fleet grouping and replacement lifecycle ownership.** Own
    group hierarchy/membership/statistics, ordered renewal rules, complete
    replacement transactions/rollback and the ordered tick-end replacement drain.
    Preserve #137 finances, #138 orders and #139 cargo at original mutation points.
12. **#148 Complete town lifecycle, authority and house simulation.** Extend
    #120's sole owner with remaining town state, founding/deletion/authority,
    monthly histories and house construction/cargo/rebuild/animation loops.
    Preserve generation cancellation and destructive house callback ordering.
13. **#150 Complete industry construction and tile lifecycle.** Extend #129's
    sole owner with construction/prospecting/generation, canonical metadata and
    ordered registry, destruction and complete tile/animation policy. Preserve
    oilrig/station/cargo cleanup, flooding rechecks and generation cancellation.
14. **Following selections:** remaining simulation components and commands that
    change them, guided by canonical state and complete control-loop ownership.

The accepted #108 decision keeps canonical map arrays in C++ with direct bundled
`noexcept` services. Counts establish crossing density, not a bottleneck; no raw
view or allocation transfer is selected. Storage transfers still require explicit
selection following `docs/design/world-state.md`.

## Resume checkpoint (2026-10-07, migration resumed)

Root: `/root` (gpt-6-astra, xhigh). The user resumed continuous migration.
Latest integration is `e16d01c869` (#160 / #154); ten integrations since
steering. Finish nightly failure #158, then follow
the queue below. Eleven unfinished components plus one integration branch were
inherited; start no further component until the six-branch cap is restored.
Maintenance work does not retire additional C++ simulation code.

Active work (all new branches start from integrated main):

| Issue / agent | Branch / worktree | Next step |
| --- | --- | --- |
| #156 first slice | `coverage-gaps-156`, `openttd-rust-coverage-gaps`, PR #161 at `f9da2921fd` | Independently accepted RNG/crossing/news coverage, 14/14 and three negative probes. CI pending; join actual #154 before integration. |
| #156 `/root/road_flooding_coverage_156` (Sol high) | `road-flooding-coverage-156`, `openttd-rust-road-flooding` | Legal canal/road reference prototype reproduces flood event and state. Root authorizes maintenance dependency on reviewed #161 to reuse its fixture helpers; no new component. Implement compact flood witnesses and negative probe next. |
| #155 `/root/speed_report_155_resume` (Sol high) | `speed-report-155`, `openttd-rust-speed-report`, local checkpoint `1afd74f92a` | Resumed to join integrated #154, run exact serial-policy benchmark/profile, then draft PR and fresh review. Coordinate an idle local timing window. |
| #158 `/root` (Astra xhigh) | `fix-mingw-nightly-158`, `openttd-rust-mingw-nightly`, PR #159 at `4a6c8fc152` | Reviewed YAML `c4cee973f1` plus clean #160 dependency. Nightly run 37570989513 passes all three builds/tests and annotations. Required final-head compare/annotations are pending; then integrate and dispatch main nightly. |
| #140 / #141 preparation | `integrate-reviewed-pathfinding`, local unpushed `17c3bae441` | Clean main update/F1 callback docs independently accepted. Include integrated main repairs, then push once for required CI. |
| #142 / #143 root preparation | Local unpushed `2a1df213c6` / `a29a142d22` in existing worktrees | Clean main updates change only guidance/docs; source acceptance carries forward. Join integrated #141 before final checks/push. |
| #144 industry warning repair | Local unpushed `4751809474` in `openttd-rust-industry-periodic` | Independent delta review, exact verify and 4480 native cases pass; join integrated main, focused pair and final CI. Metrics now 1508 / 474 / 678 / 932; eight extra retired lines are declaration/comment scope, not new game logic. |
| #149 root preparation | Local unpushed `1886b41784` in `openttd-rust-company-economy` | Clean main update plus identical industry warning fix; exact verify and independent delta review pass. Actual #144 ancestry, ratio and final CI remain. |
| #151 `/root/cargo_list_scaling_151` (Sol high) | `cargo-storage-movement-139`, `openttd-rust-cargo-storage`, local `ff912bcfcb` | F1 fixed and source accepted at `a25a7d41e2`; exact verify/probe pass. Clean actual #154 join plus obsolete rail-preparation lock removal; run final pair/self/soak, review resulting evidence and push once. Same reviewer `/root/review_cargo_storage_151` (Astra medium). |

The component table below preserves the last implementation checkpoints from
2026-10-04; its next steps remain applicable except where superseded above.

Worktrees are siblings of the main checkout. Owner shorthand below: Sol high is
`gpt-6.1-sol`, reasoning effort high; root is `gpt-6-astra`, ultra. Reviews are
separately attributed in each PR. Counts and CI status describe these commits,
not a promise that later dependency updates will pass.

| Issue / owner | Branch and checkpoint commit | Worktree | Next step |
| --- | --- | --- | --- |
| #122 `/root/rail_yapf_ownership` (Sol high) | `rail-yapf-ownership-122` at `ef1b981101`, PR #133 | `openttd-rust-rail-yapf` | Reviewed; standalone verify 97/120, six-year soak 6/6 and 13 CI checks passed. Integrate through #141 after its current-head CI. ABI 90-95/97. |
| #124 `/root/road_yapf_ownership` (Sol high), root coordinates | `road-yapf-ownership-124` at `453f249c5f`, PR #135 | `openttd-rust-road-yapf` | Same reviewer accepts final explicit uint8 test conversions. Production and prior pair/self/soak unchanged. Integrate through #141. ABI 100-109. |
| #140 `/root` (Astra ultra) | `integrate-reviewed-pathfinding` at `2eff182b18`, PR #141 | `openttd-rust-pathfinding-batch` | Independently reviewed search batch. Clean merge of actual main 5659611be9 changes only roadmap from the previously green 14d7ce95d5. Exact verify 97/131 passes; prior combined 27/27 (433 snapshots) and stockpile pair pass. Current head is mergeable; required CI rerunning. Integrate first when all 13 pass, then docs completion/stocktake. |
| #125 `/root/station_service_ownership` (Sol high), root coordinates | `station-service-ownership-125` at `54dabb86d0`, PR #142 | `openttd-rust-station-service` | Reviewed at d277a54c10; final clean dependency update verified 97/136, focused pair/self 11/11 passed. All 13 CI checks pass at this head. Refresh base after #141; GitHub currently reports conflicts. ABI 110-129. |
| #129 `/root/industry_periodic_ownership` (Sol high), root coordinates | `industry-periodic-ownership-129` at `eaa7ff26e2`, PR #144 | `openttd-rust-industry-periodic` | Reviewed at 4ff6a1a1ab; exact final verify passes, pair/self/soak 17/17 and native 4480 gap cases pass. Twelve required checks pass; Check Annotations fails on unused PERCENT_TRANSPORTED_60/80 in industry_cmd.cpp and last in industry_adapter.hpp. Fix introducing declarations, re-review changes, refresh base and rerun CI. Metrics 1508/474/676/924. ABI 130-149. |
| #130 `/root/rail_vehicle_ownership` (Sol high), root coordinates | `rail-vehicle-ownership-130` at `dfd47a51db`, PR #143 | `openttd-rust-rail-vehicles` | Reviewed source/dependency resolution at b94694ec9d; exact verify 97/133, pair 24/24, prior soak and native curve/reversal gap pass. All 13 CI checks pass at this head. Refresh base after #141; GitHub currently reports conflicts. ABI 150-179. |
| #136 `/root/aircraft_controller_ownership` (Sol high), root coordinates | `aircraft-controller-ownership-136` at `1f176deed3`, PR #145 | `openttd-rust-aircraft-controller` | Same reviewer accepts actual #142 join and loading-drain/target cleanup order. Exact verify and combined aircraft/economy/stations pair 20/20 (150 snapshots) pass. CI pending; refresh base and integrate after #142. ABI 180-209. |
| #137 `/root/company_economy_ownership` (Sol high) | `company-economy-ownership-137` at `4165f204be`, PR #149 | `openttd-rust-company-economy` | Independent review accepted. Verify97/135, company pair 12/12 (24 snapshots), finance soak 1/1, economy 3/3 and unchanged native financial-command 40320 cases pass. Refresh actual #144/#141 ancestry and propagate industry warning fixes before integration; current CI pending. Metrics 1609/705/1012/1394. ABI 210-239. |
| #138 `/root/order_lifecycle_ownership` (Sol high) | `order-lifecycle-ownership-138` at `ef58c967f7`, draft PR #153 | `openttd-rust-order-lifecycle` | Full canonical owner/control checkpoint. Verify97/134, Cargo/Ruff and paired regression/depot/reload 67 snapshots pass; 101 self/soak snapshots are reference-vs-reference, not candidate soak. Still needs focused backup/BKOR restore, timetable execution, conditional/implicit reload, shared-depot unbunching and unmasked initialized #83 evidence; then candidate soak, fresh review, base update and CI. Metrics 3636/10/882/2518. ABI 240-279. |
| #139 `/root/cargo_storage_movement_ownership` (Sol high) | `cargo-storage-movement-139` at `baace0f9a3`, draft PR #151 | `openttd-rust-cargo-storage` | Full packet/list/action/cache/flow and completed-job live application owner. Exact verify 97/137, Cargo/Ruff, pair 20/20 (174 snapshots), self 5/5, soak 5/5 and all 13 comparison tools pass. Actual station 54d and main 565 ancestry included. Fresh independent review and current-head CI remain required; integrate after #142. Metrics 2741/283/973/2445. ABI 280-319. |
| #146 `/root/ship_controller_ownership` (Sol high) | `ship-controller-ownership-146` at `7ec70a1768`, draft PR #152 | `openttd-rust-ship-controller` | Full private-state/control/depot-BFS checkpoint. Verify, Cargo/Ruff and 12 comparisons pass. Water structures 4/4 pass; both ferry cases match 40 snapshots each but fail plain end timing after 3 attempts. Rerun on idle host, then self/soak and actual build/sell/reuse/water-class witnesses; join final company/orders/cargo ancestry, obtain fresh review and CI. Metrics 1040/89/643/721. ABI 320-339. |
| #147 `/root/fleet_replacement_ownership` (Sol high) | `fleet-replacement-ownership-147` at `68d660adc9`, no PR | `openttd-rust-fleet-replacement` | State-only WIP, not a completed ownership port. Build/Cargo and 36 Rust tests pass; native Fleet test 45 assertions pass. Actual dependency base 344b6663b6 includes company 4165, rail dfd, aircraft 1f and cargo 18f5. Join final orders #153/cargo #151, finish group/rule/replacement/rollback/pending-drain control and compact fleet scenarios, then full evidence, draft PR, fresh review and CI. Metrics 235/0/300/66, chiefly declarations. ABI 340-379. |
| #148 fresh `/root/town_lifecycle_ownership` (Sol high) | Not started | To create `openttd-rust-town-lifecycle` | Selected after fleet; extend canonical town owner with complete remaining town/house lifecycle and authority. Wait for resume. |
| #150 fresh `/root/industry_construction_ownership` (Sol high) | Not started | To create `openttd-rust-industry-construction` | Selected after town; extend industry owner with complete construction/tile/destruction control and canonical metadata. Wait for resume. |

After main-CI repairs, resume component integration with #141, then refresh the dependent reviewed branches from
actual main. GitHub currently reports conflicting bases for #142/#143/#144/#145/
#149/#152/#153; do not bypass CI. A clean base update with no src/rust conflicts
needs no new review under AGENTS.md, but source conflict resolution or source
fixes require the same PR reviewer. All required checks must pass on the resulting
head. Industry warning fixes must reach company and later dependent branches.

Cargo #151 is locally validated but unreviewed. Orders #153 and ship #152 retain
explicit evidence gaps, and fleet #147 is only a storage checkpoint. No missing
review or evidence is waived by the wind-down. For ship, the next diagnostic is
`OPENTTD_SHIP_PROFILE=1 python3 tools/migration.py simulate water-ferry --jobs 2`
on an idle host. For orders, script APIs cannot exercise backup capture/restore
or timetable commands; a proposed narrow native tick-hook must preserve frozen
binary provenance and run the same instrumentation on unchanged reference bodies.
That hook has not been implemented or accepted as evidence.

The shared ABI registry was widened to u16 in cargo ancestry, with bounded u8
conversion fixes for crypto and company dispatch. Preserve these fixes when
joining branches. Build/test with `--jobs 2`; keep processes in an active task
session until they finish. No local build or simulation is left running.

Preserved evidence branches: `evidence-disaster-vehicles` at `a775543162`
(`openttd-rust-disasters-evidence`) and `evidence-water-regions` at `c752070cde`
(`openttd-rust-water-evidence`). Useful inputs are incorporated; do not reapply
borrowed effect helper `73ccd511fb`. Completed-work receipts remain archived;
checkable evidence and review dispositions are in the linked component PRs.

Fresh Astra high planner `/root/plan_after_ship_and_fleet` selected #148 then
#150, accepted by root. Both remain unstarted. After an explicit resume, replenish
when fewer than two unstarted selections remain ahead of active work; preserve
whole owners, native-width behavior and explicit ordinary-play reentry boundaries.

## Choosing the next task

The assignment is active. Take the first
unblocked item above. Fill idle capacity in this order: integration and review
of finished work, Phase 1 items, then a new component, but only while fewer
than six component branches are unintegrated (#157). Paused, deferred and
out-of-scope issues are not fallbacks. Root selects further ownership work here before implementation starts.

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
