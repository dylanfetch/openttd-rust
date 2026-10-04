# OpenTTD-Rust near-term roadmap

Root owns this file and updates it when a phase completes or priorities change.
`AGENTS.md` and `docs/rust-migration.md` define the rules; this file decides what
to work on next. If an issue conflicts with this roadmap, follow the roadmap;
a subagent stops and reports the conflict in its hand-off to root. Keep this
file forward-looking: completed work is one table row, and its evidence stays in
the PR (`AGENTS.md`, "Evidence budget").

## Where the fork stands (2026-10-04, integration `9732d8bfa2`)

- Six ownership ports retired 7,730 original C++ lines, about 2% of roughly
  384k non-vendored `src/` lines. Town names account for 3,848, mostly data;
  the other five account for 3,882 lines of game components.
- The last two integrations added map measurements (65 tooling/81 glue, no game
  retirement) and the whole water-region cache/graph owner (407 retired). Work
  since steering now includes one new owner after five prerequisite/evidence
  integrations; moved code and old glue must not inflate simulation progress.
- Landed overruns: effects originally added 965 glue/tooling for 548 retired;
  water adds 883 for 407, including the first reusable ship corpus and native
  visitor probes. Pending disasters is 1,101 for 968 and cargo payment is 960
  for 315 after review fixes. Each PR records its concrete evidence cost.
- Larger owners are now producing usable code: full ship search retires 646
  against 547 glue/tooling; town growth and road control own substantially larger
  complete loops. Reuse the current fixtures and native checks. Do not expand
  generic harness tools or polish evidence already accepted by review.
- The direct-service and map prerequisites are complete. Prioritize integration
  of these full owners and remove duplicate C++ authority; shared packet/map/pool
  storage remains deliberate. Earlier utility work stays in rust-migration.md.

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

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness maintenance

The harness is `python3 tools/migration.py simulate` (`docs/rust-migration.md`,
"Simulation comparison"); its default set runs in CI. Harness regressions are
always the first priority. Port differences go in `KNOWN_FAILURES` with an issue,
never in masks.

- **#86 Rail and aircraft scenarios** (the ship slice is part of #104). Run on spare
  capacity starting now; each controller/YAPF port needs evidence for its vehicle
  type. Rail/ship evidence is integrated in #110; #104 adds ship routing. The owner
  authorized a separate reference-generated aircraft fixture on 2026-10-04
  (#111, decision recorded on #86). Commit the deterministic setup AI, preserve
  the supplied save, and require movement/delivery, self/pair and negative-probe
  witnesses. `/root/aircraft_fixture_86` owns this at the next available slot.

## Phase 3: current ownership work, in order

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
10. **Following selections:** company/economy lifecycle, orders, further cargo and
    their commands. Aircraft controller selection still needs #86 evidence, now
    authorized; fixture availability alone does not select a controller port.

The accepted #108 decision keeps canonical map arrays in C++ with direct bundled
`noexcept` services. Counts establish crossing density, not a bottleneck; no raw
view or allocation transfer is selected. Storage transfers still require explicit
selection following `docs/design/world-state.md`.

## Resume checkpoint (2026-10-04)

Root: `/root` (gpt-6-astra, ultra). Integration base `d5edfcb4c5`; seven
integrations since steering. Stocktake covers the sixth; update it after the
next integration. All rows below are active, not integrated completion.
Worktrees are siblings of the main checkout unless a path says otherwise.

| Issue / owner | Branch and checkpoint commit | Worktree | Next step |
| --- | --- | --- | --- |
| #117 `/root/cargo_payment_delivery` (Sol high) | `cargo-payment-delivery-117` at `2abf956bdf`, PR #123 | `openttd-rust-cargo-payment` | Same reviewer accepts explicit MSVC conversions and disaster-base update; verify and combined pair4/4 pass. Required CI reruns. ABI 49-51. |
| #119 `/root/ship_yapf_ownership` (Sol high) | `ship-yapf-ownership-119` at `465a8376a9`, PR #126 | `openttd-rust-ship-yapf` | Explicit native-fixture MSVC conversion and disaster-base update pass verify and combined pair2/2. Same reviewer accepts small delta; required CI reruns. ABI 52-56. |
| #120 `/root/town_growth_ownership` (Sol high) | `town-growth-ownership-120` at `9fbeaa00d1`, PR #127 | `openttd-rust-town-growth` | `/root/review_town_growth_pr127` (Astra medium) accepts final owner and evidence. Verify and unchanged soak10/10 pass after coast predicate and live callback-mask fixes. Required CI remains; base conflicts need an ordinary update before integration. ABI 60/61. |
| #121 `/root/road_vehicle_ownership` (Sol high) | `road-vehicle-ownership-121` at `a8a2525557`, PR #128 | `openttd-rust-road-vehicles` | Actual disaster dependency included. Final verify, roads8/8 and soak10/10 pass. `/root/review_road_control_pr128` (Astra medium) accepts the restored current-depot shortcut and native regression; exact-head verify and pair10/10 pass. Required CI remains. Short road inputs explicitly postpone five LGRJ joins, leaving vehicle/map fields unchanged. ABI 80-83. |
| #122 `/root/rail_yapf_ownership` (Sol high) | `rail-yapf-ownership-122` at `72ddedf780`, uncommitted owner | `openttd-rust-rail-yapf` | Candidate soak6/6 proves search, depot/reversal, safe search, reservation success/busy/rollback and cache reuse. `/root/rail_yapf_limit_probe` supplies bounded limit/heap/invalidation/signal-order checks. Finish diagnostic fidelity, base update, verify and draft PR. ABI 90-95/97. |
| #124 `/root/road_yapf_ownership` (Sol high) | `road-yapf-ownership-124` at `2f59e67086` | `openttd-rust-road-yapf` | Whole search builds and roads8/8 passes after origin-mask fix. Finish native limit gap and valid penalty witnesses; merge latest #128 a8a2525557 dependency, then verify/draft PR. ABI 100-109 reserved. |
| #125 `/root/station_service_ownership` (Sol high) | `station-service-ownership-125` at `0db650e1bd` | `openttd-rust-station-service` | Actual #123 dependency merged. Implement canonical service/metadata and complete loading/periodic control; audit stable scalar references and named refit reentry. Queue/save adapters are a bounded helper task when capacity permits. ABI 110-129 reserved. |
| #129 `/root/industry_periodic_ownership` (Sol high) | Not started | To create `openttd-rust-industry-periodic` | Selected next after station service; whole production/state/history/builder owner. |
| #130 `/root/rail_vehicle_ownership` (Sol high) | Not started; uses #122 search owner | To create `openttd-rust-rail-vehicles` | Selected after industry; include separate controller reservation and missing branch evidence. |
| #86 `/root/aircraft_fixture_86` (Sol high) | `aircraft-evidence-86` at `a8aabad40a`, uncommitted fixture | `openttd-rust-aircraft-evidence` | Authorized reference-built 258KB save has two airports, plane and helicopter. Self/pair and four-year soak pass movement/delivery witnesses. Finish negative-probe restoration, verify and draft PR for fresh review. |

Preserved evidence branches: `evidence-disaster-vehicles` at `a775543162`
(`openttd-rust-disasters-evidence`) and `evidence-water-regions` at `c752070cde`
(`openttd-rust-water-evidence`). Their useful source inputs are incorporated;
do not reapply borrowed effect helper `73ccd511fb`. Build/test with `--jobs 2`;
required CI supplies default/all-comparison checks where local component evidence
already covers the change. Keep long-running checks in an active agent session:
ending and recycling a task thread has killed unfinished background processes.

Fresh planner `/root/plan_next_ownership_selections` (Astra high) selected the
cargo/ship sequence. `/root/plan_town_growth_ownership` (Astra high) supplied the
whole growth-loop and house-placement scope now selected as #120. Continue
per-PR reviews while CI runs. `/root/plan_next_vehicle_owners` (Astra high)
selected road control then rail YAPF as #121/#122; reserve complete rail control
until its coupled branches have suitable evidence. Fresh Astra high planner `/root/plan_after_vehicle_pathfinding` selected #124
and #125. Fresh Astra high planner `/root/plan_next_simulation_owners` selected
#129 then #130; root accepted both. Two unstarted owners remain while station
service and the authorized aircraft evidence task are active. Replenish when fewer than two remain. Industry
includes histories, complete periodic control and builder backoff; rail includes
its own reservation rollback, distinct from #122.

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
