# OpenTTD-Rust near-term roadmap

Root owns this file and updates it when a phase completes or priorities change.
`AGENTS.md` and `docs/rust-migration.md` define the rules; this file decides what
to work on next. If an issue conflicts with this roadmap, follow the roadmap;
a subagent stops and reports the conflict in its hand-off to root. Keep this
file forward-looking: completed work is one table row, and its evidence stays in
the PR (`AGENTS.md`, "Evidence budget").

## Where the fork stands (2026-10-04, integration `320711cccd`)

- Seven ownership ports retired 8,698 original C++ lines, about 2.3% of roughly
  384k non-vendored `src/` lines. Town names account for 3,848, mostly data;
  the other six account for 4,850 lines of game components.
- The last two integrations were disaster scheduling/control (968 retired) and
  the authorized aircraft fixture (347 tooling, no game retirement). Eight
  integrations since steering include two new owners; evidence and moved code
  do not count as simulation retirement.
- Landed overruns: effects originally added 965 glue/tooling for 548 retired;
  water adds 883 for 407, including the reusable ship corpus and visitor probes;
  disasters adds 1,101 for 968. Pending cargo adds 960 for 315. Each PR names its
  concrete evidence cost; do not expand generic tooling to improve a metric.
- Since this stocktake, cargo/ship/town/road batch #134 integrated 5,351 retired
  lines against 4,469 glue/tooling, including 1,475 road movement-table lines.
  Ship, town and road each retire more than their glue/tooling; cargo is an
  explicit overrun. Refresh the full totals after the next integration.
- Reuse current fixtures and component scenario modules. Preserve one authority
  for each owned component; shared packet/map/pool storage remains deliberate.
  Utility work and already accepted evidence are not substitutes for these loops.

## Steering review (2026-10-04)

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

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness maintenance

The harness is `python3 tools/migration.py simulate` (`docs/rust-migration.md`,
"Simulation comparison"); its default set runs in CI. Harness regressions are
always the first priority. Port differences go in `KNOWN_FAILURES` with an issue,
never in masks.

The #86 rail/aircraft fixture gate is complete. Controller ports still require
component-specific branch witnesses; extend the existing scenario modules and
reuse the supplied rail/ship save and authorized aircraft setup AI.

## Phase 3: current ownership work, in order

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

## Resume checkpoint (2026-10-04)

Root: `/root` (gpt-6-astra, ultra). Integration base `adbe063372`; nine
integrations since steering. Stocktake covers the eighth; update after the next
integration. All rows below are active, not integrated completion.
Worktrees are siblings of the main checkout unless a path says otherwise.

| Issue / owner | Branch and checkpoint commit | Worktree | Next step |
| --- | --- | --- | --- |
| #122 `/root/rail_yapf_ownership` (Sol high) | `rail-yapf-ownership-122` at `ef1b981101`, PR #133 | `openttd-rust-rail-yapf` | Verify97/120, six-year soak6/6, independent review and all 13 standalone CI checks pass. Included in #141; combined CI gates integration after #134. ABI 90-95/97. |
| #124 `/root/road_yapf_ownership` (Sol high), root coordinates | `road-yapf-ownership-124` at `453f249c5f`, PR #135 | `openttd-rust-road-yapf` | Same reviewer accepts two explicit uint8 test conversions after Windows annotations. Production and prior pair/self/soak unchanged; final combined verify passes. ABI100-109. |
| #125 `/root/station_service_ownership` (Sol high), root coordinates | `station-service-ownership-125` at `54dabb86d0`, PR #142 | `openttd-rust-station-service` | Final source/input fixes accepted at d277a54c10; clean actual #141 update adds accepted road test conversions. Exact final verify97/136 and prior focused self/pair11/11 pass. Required CI running. ABI110-129. |
| #129 `/root/industry_periodic_ownership` (Sol high), root coordinates | `industry-periodic-ownership-129` at `eaa7ff26e2`, PR #144 | `openttd-rust-industry-periodic` | Final owner and introducing-commit style correction accepted at4ff6a1a1ab; clean #141 update verified. Pair/self/soak17/17 and native4480 gap cases pass. Metrics1508/474/676/924 record overrun. Required CI running. ABI130-149. |
| #130 `/root/rail_vehicle_ownership` (Sol high), root coordinates | `rail-vehicle-ownership-130` at `dfd47a51db`, PR #143 | `openttd-rust-rail-vehicles` | Owner and actual #141 conflict resolution accepted at b94694ec9d; final clean dependency fixes pass exact verify97/133. Pair24/24, prior soak and native curve/reversal gap pass. Required CI running. ABI150-179. |
| #136 `/root/aircraft_controller_ownership` (Sol high), root coordinates | `aircraft-controller-ownership-136` at `1f176deed3`, PR #145 | `openttd-rust-aircraft-controller` | Same reviewer accepts actual #142 join, preserving loading-drain/aircraft-target cleanup order. Exact verify and combined aircraft/economy/stations pair20/20 (150 snapshots) pass. Required CI running; integrate after #142. ABI180-209. |
| #137 `/root/company_economy_ownership` (Sol high) | `company-economy-ownership-137` at `4165f204be`, draft PR #149 | `openttd-rust-company-economy` | Full owner includes actual #144/#141 dependencies. Verify, company pair/self12/12 and finance soak1/1 pass after original tile-query predicate ordering fix. Native command gaps covered. Independent review accepts; required CI running. Component1609/705/1012/1394 records glue/tooling overrun. ABI210-239. |
| #138 `/root/order_lifecycle_ownership` (Sol high) | `order-lifecycle-ownership-138` at `5ac057aa16`, uncommitted owner | `openttd-rust-order-lifecycle` | Actual #141 dependency included. Canonical vectors/current/shared/backups and full list operations implemented; complete commands/controllers/timetable next. Preserve typed lifetimes and separate unmasked #83 evidence. ABI 240-279. |
| #139 `/root/cargo_storage_movement_ownership` (Sol high) | `cargo-storage-movement-139` at `620d17d99e`, uncommitted owner | `openttd-rust-cargo-storage` | Packets/lists/actions and canonical flow/application helper implemented; adapters and complete validation underway. Coordinate shared ABI u16 widening82dca4a9f2 plus crypto-helper narrowing/lint correction. ABI280-319. |
| #140 `/root` (Astra ultra) | `integrate-reviewed-pathfinding` at `14d7ce95d5`, PR #141 | `openttd-rust-pathfinding-batch` | Actual #134/#135 final fixes merged cleanly; both component deltas independently accepted. Exact verify97/131 and focused stockpile pair pass; prior combined27/27 unchanged production. Required CI running; integrate after #134. |
| #146 `/root/ship_controller_ownership` (Sol high) | `ship-controller-ownership-146` at `b38b42f09d` | `openttd-rust-ship-controller` | Actual station/path dependency included; implement full private owner/controller/depot BFS and branch witnesses. Coordinate orders/cargo/ABI widening; ABI320-339 reserved. |
| #147 `/root/fleet_replacement_ownership` (Sol high) | `fleet-replacement-ownership-147`, worktree initialization | `openttd-rust-fleet-replacement` | Implement complete groups/rules/transactions/drain using accepted company/rail/aircraft ancestry; coordinate active order/cargo APIs. ABI340-379 reserved. |
| #148 fresh `/root/town_lifecycle_ownership` (Sol high) | Not started | To create `openttd-rust-town-lifecycle` | Selected after fleet; extend canonical town owner with complete remaining town/house lifecycle and authority. |
| #150 fresh `/root/industry_construction_ownership` (Sol high) | Not started | To create `openttd-rust-industry-construction` | Selected after town; extend industry owner with complete construction/tile/destruction control and canonical metadata. |

Preserved evidence branches: `evidence-disaster-vehicles` at `a775543162`
(`openttd-rust-disasters-evidence`) and `evidence-water-regions` at `c752070cde`
(`openttd-rust-water-evidence`). Their useful source inputs are incorporated;
do not reapply borrowed effect helper `73ccd511fb`. Build/test with `--jobs 2`;
required CI supplies default/all-comparison checks where local component evidence
already covers the change. Keep long-running checks in an active agent session:
ending and recycling a task thread has killed unfinished background processes.
Company PR #149 is independently accepted. Next review slots follow final order/cargo/ship/fleet PRs. Source fixes from
shared dependencies must reach later branches before their integration.

Fresh Astra high planning selected the current queue. Latest planner
`/root/plan_after_ship_and_fleet` supplied #148 then #150; root accepted both.
Company is in CI; orders, cargo, ship controller and fleet are active. Town
lifecycle and industry construction are unstarted. Replenish when fewer than two are
unstarted ahead of active work. Preserve complete owners, native-width behavior
and explicit reentry boundaries; CI/review runs concurrently with implementation.

## Choosing the next task

Take the first unblocked item above. Work in CI or review is not blocking: start
the next item while it runs. Keep spare capacity on scenarios (#86) and
independent review. Paused, deferred and out-of-scope issues are not fallbacks.
Root selects further ownership work here before implementation starts.

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
