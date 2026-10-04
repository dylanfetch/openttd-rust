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
- The accepted cargo/ship/town/road batch retires 5,351 lines against 4,470
  glue/tooling, including 1,475 road movement table lines. These remain pending
  until the combined PR passes CI. Ship, town and road each retire more than
  their glue/tooling; cargo is an explicit overrun. Larger owners are now ready
  for integration, while station, industry and vehicle controllers are active.
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

CI capacity requires one integration batch (#131) for accepted component PRs
#123/#126/#127/#128. Keep their component reviews/evidence; require independent
review of the combined resolutions and all required CI on the integration PR.

1. **#117 Cargo payment and delivery ownership.** Selected after a fresh Astra
   high comparison with station ratings and industry production. Own CargoPayment
   state and lifetime, delivery acceptance/payment control, destination collection
   and the complete production-flush loop. Preserve Money saturation, native-width
   intermediates and CAPY lifecycle. Loading/reservation and shared industry state
   remain C++; this is not an income-formula extraction. Begin while earlier PRs
   are in review/CI, using the existing road scenarios and #107/#109 interfaces.
2. **#119 Complete ship YAPF and path cache ownership.** Own both search levels,
   their queues/arenas/corridor/retries and canonical `Ship::path`, including all
   controller and save adapters. Reuse #104's corpus; no separate comparison tool.
   Use integrated #104 and its real dependency ancestry.
3. **#120 Complete town-growth control and private state.** Own tick traversal,
   growth road walking/build choices, house selection and placement control,
   growth-rate/funding transitions and canonical counters/flags with CITY and
   legacy adapters. Keep unrelated town accounting and shared world state in C++.
   Add actual growth witnesses to the existing towns module.
4. **#121 Complete road vehicle control and private state.** Own consist/tick
   movement, blocking/overtaking/crash/servicing and day handling, canonical road
   counters and path cache, plus save/legacy/external mutation adapters. Existing
   road YAPF may return a temporary result for transfer to the empty Rust cache.
5. **#122 Complete rail YAPF search, caches and reservation.** Own all four
   searches, six specialization-specific global cache banks, rail-change
   invalidation, reservation traversal and rollback. Reuse the rail corpus and
   ship search machinery only where ordering matches; require branch witnesses.
6. **#124 Complete road YAPF search and path construction.** Own both track and
   depot searches, all node/queue/segment state and reconstruction. Replace
   #121's temporary result bridge using its canonical path cache; do not claim
   the cache twice. Reuse road scenarios and compatible search support.
7. **#125 Complete station cargo-service control and state.** Own loading order,
   service/cargo metadata, full loading/reservation/refit policy, acceptance,
   ratings/distribution and periodic service loops. Preserve #117's payment and
   production-flush owner and shared packet/flow/map storage. Reuse its fixtures.
8. **#129 Complete industry periodic production and builder ownership.** Own
   canonical cargo slots/histories, production and transport control, daily/monthly
   closure, farm/lumber loops, NewGRF production repeat/apply policy and weighted
   builder targets/retries/backoff. Reuse #117 and #125 boundaries and fixtures.
9. **#130 Complete rail vehicle control and private state.** Own consist/tick
   movement, reversal/crossings/crash, speed/service/day loops and controller
   reservation extension/rollback, with canonical train-private state and adapters.
   #122 search reservation does not cover controller rollback. Establish missing
   collision/crossing/reversal witnesses as part of the port before integration.
10. **#136 Complete aircraft control and airport movement ownership.** Own both
    controller passes, FTA traversal, terminal/helipad/block allocation, private
    aircraft state and canonical station airport masks, plus service/diversion/
    crash and save adapters. Reuse #132; add actual contention, dedicated helipad,
    closure/removal and crash witnesses. Coordinate #125's station lifetime.
11. **#137 Complete company financial and economy lifecycle ownership.** Own
    canonical finances/histories, bankruptcy/offer/acquisition/deletion control,
    world ownership-transfer traversal, periodic economy/prices and financial
    commands. Preserve real VM/deletion reentry and #129 ECMY fields. Require
    multiple-company recovery/transfer/deletion evidence, not only profitable play.
12. **Following selections:** complete orders/shared-list/backup ownership, further
    cargo and commands. Orders must include editing/sharing/current-order and
    timetable adapters, not a ProcessOrders-only extraction.

The accepted #108 decision keeps canonical map arrays in C++ with direct bundled
`noexcept` services. Counts establish crossing density, not a bottleneck; no raw
view or allocation transfer is selected. Storage transfers still require explicit
selection following `docs/design/world-state.md`.

## Resume checkpoint (2026-10-04)

Root: `/root` (gpt-6-astra, ultra). Integration base `320711cccd`; eight
integrations since steering. Stocktake covers the eighth; update after two more
integrations. All rows below are active, not integrated completion.
Worktrees are siblings of the main checkout unless a path says otherwise.

| Issue / owner | Branch and checkpoint commit | Worktree | Next step |
| --- | --- | --- | --- |
| #131 `/root` (Astra ultra) | `integrate-reviewed-owners` at `3c4e9da0b0`, PR #134 | `openttd-rust-owner-batch` | Combined verify97/124 and focused24/24 (261 snapshots) pass. `/root/review_owner_batch_pr134` (Astra medium) accepts original joins and test-only conversion fix. Include integrated aircraft fixture base, check registration, then final required CI. |
| #117 `/root/cargo_payment_delivery` (Sol high) | `cargo-payment-delivery-117` at `2abf956bdf`, PR #123 | `openttd-rust-cargo-payment` | Same reviewer accepts explicit MSVC conversions and disaster-base update; verify and combined pair4/4 pass. Accepted head is included in #131; combined CI gates integration. ABI 49-51. |
| #119 `/root/ship_yapf_ownership` (Sol high) | `ship-yapf-ownership-119` at `465a8376a9`, PR #126 | `openttd-rust-ship-yapf` | Explicit native-fixture MSVC conversion and disaster-base update pass verify and combined pair2/2. Same reviewer accepts small delta; included in #131. ABI 52-56. |
| #120 `/root/town_growth_ownership` (Sol high) | `town-growth-ownership-120` at `9fbeaa00d1`, PR #127 | `openttd-rust-town-growth` | `/root/review_town_growth_pr127` (Astra medium) accepts final owner and evidence. Verify and unchanged soak10/10 pass after coast predicate and live callback-mask fixes. Accepted head and resolved base conflicts are included in #131. ABI 60/61. |
| #121 `/root/road_vehicle_ownership` (Sol high) | `road-vehicle-ownership-121` at `682a8151f3`, PR #128 | `openttd-rust-road-vehicles` | Actual disaster dependency included. Final verify, roads8/8 and soak10/10 pass. `/root/review_road_control_pr128` (Astra medium) accepts the restored current-depot shortcut and native regression; exact-head verify and pair10/10 pass. Explicit native test conversion fixes the Windows annotation; same reviewer accepts, exact verify passes and fix is included in #131. Short road inputs explicitly postpone five LGRJ joins, leaving vehicle/map fields unchanged. ABI 80-83. |
| #122 `/root/rail_yapf_ownership` (Sol high) | `rail-yapf-ownership-122` at `ef1b981101`, PR #133 | `openttd-rust-rail-yapf` | Merged main base; verify/Cargo and all comparisons pass. Actual soak branches plus native limit/heap/invalidation/signal-order tests pass; original dump format retained through temporary views. Final soak6/6 (426 snapshots) and `/root/review_rail_yapf_pr133` (Astra medium) acceptance complete. Native test conversion fix passes exact verify and same-reviewer acceptance; fresh CI running. ABI 90-95/97. |
| #124 `/root/road_yapf_ownership` (Sol high) | `road-yapf-ownership-124` at `ab09ce9306`, PR #135 | `openttd-rust-road-yapf` | Final verify97/121, roads9/9 pair/self and six-year12/12 soak pass. `/root/review_road_yapf_pr135` (Astra medium) accepts final source/evidence. Clean dependency update includes accepted #128 test conversion fix; exact verify passes. Root repeats clean-head soak to remove precommit provenance ambiguity; required CI gates integration. ABI 100-109 reserved. |
| #125 `/root/station_service_ownership` (Sol high) | `station-service-ownership-125` at `0db650e1bd` | `openttd-rust-station-service` | Actual #123 dependency merged. Complete service/loading/refit/reservation and periodic policy implemented; native build and semantic evidence next. `/root/station_loading_queue_adapters` finished stable-node queue and stack-only save/fixup staging; focused tests await native build. Audit field-sized raw access and actual refit reentry. ABI 110-129 reserved. |
| #129 `/root/industry_periodic_ownership` (Sol high) | `industry-periodic-ownership-129` at `6b17fad17e` | `openttd-rust-industry-periodic` | Actual #123 dependency merged. Implement canonical slots/histories, complete production/periodic and builder owner; coordinate #125. ABI 130-149 reserved. |
| #130 `/root/rail_vehicle_ownership` (Sol high) | `rail-vehicle-ownership-130` at `73d18db`, actual #133 dependency | `openttd-rust-rail-vehicles` | Whole controller audit/implementation; `/root/rail_vehicle_ownership/train_state_adapters` (Sol high) owns canonical state/external/save adapters. Include separate controller rollback and branch evidence. ABI 150-179 reserved. |
| #136 `/root/aircraft_controller_ownership` (Sol high) | `aircraft-controller-ownership-136` from `320711cccd` | `openttd-rust-aircraft-controller` | Whole aircraft/airport block owner and branch evidence now active; coordinate #125 station lifetime. ABI 180-209 reserved. |
| #137 fresh `/root/company_economy_ownership` (Sol high) | Not started | To create `openttd-rust-company-economy` | Selected after aircraft; whole company/economy state, periodic lifecycle and transfer/financial commands. |

Preserved evidence branches: `evidence-disaster-vehicles` at `a775543162`
(`openttd-rust-disasters-evidence`) and `evidence-water-regions` at `c752070cde`
(`openttd-rust-water-evidence`). Their useful source inputs are incorporated;
do not reapply borrowed effect helper `73ccd511fb`. Build/test with `--jobs 2`;
required CI supplies default/all-comparison checks where local component evidence
already covers the change. Keep long-running checks in an active agent session:
ending and recycling a task thread has killed unfinished background processes.

Fresh Astra high planning selected the current ownership queue. Latest planner
`/root/plan_after_industry_and_rail` selected #136 then #137; root accepted both.
Aircraft has a usable reference-built input gate, with new airport/control branch
witnesses still required. Company needs complete transfer/lifecycle orchestration;
orders remain a later whole-owner selection. Only #137 remains unstarted. Fresh planning must replenish
the queue before the next implementation slot opens; keep per-PR review and CI concurrent.

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
