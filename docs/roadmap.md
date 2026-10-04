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
  against 400 glue/tooling; town growth and road control own substantially larger
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
  type. Rail/ship evidence is integrated in #110; #104 adds ship routing. Aircraft needs
  an owner-built input unless the owner permits a supplemental setup AI (#111).

## Phase 3: current ownership work, in order

1. **#103 Disaster scheduling, vehicles and event control, PR #118.**
   Final independent review accepts the owner and evidence; wait required CI.
   Actual #109/#113/#115 dependency ancestry is included; retain the real UFO/train,
   airport, industry, release/reload and submarine lifecycle witnesses.
2. **#117 Cargo payment and delivery ownership.** Selected after a fresh Astra
   high comparison with station ratings and industry production. Own CargoPayment
   state and lifetime, delivery acceptance/payment control, destination collection
   and the complete production-flush loop. Preserve Money saturation, native-width
   intermediates and CAPY lifecycle. Loading/reservation and shared industry state
   remain C++; this is not an income-formula extraction. Begin while earlier PRs
   are in review/CI, using the existing road scenarios and #107/#109 interfaces.
3. **#119 Complete ship YAPF and path cache ownership.** Own both search levels,
   their queues/arenas/corridor/retries and canonical `Ship::path`, including all
   controller and save adapters. Reuse #104's corpus; no separate comparison tool.
   Use integrated #104 and its real dependency ancestry.
4. **#120 Complete town-growth control and private state.** Own tick traversal,
   growth road walking/build choices, house selection and placement control,
   growth-rate/funding transitions and canonical counters/flags with CITY and
   legacy adapters. Keep unrelated town accounting and shared world state in C++.
   Add actual growth witnesses to the existing towns module.
5. **#121 Complete road vehicle control and private state.** Own consist/tick
   movement, blocking/overtaking/crash/servicing and day handling, canonical road
   counters and path cache, plus save/legacy/external mutation adapters. Existing
   road YAPF may return a temporary result for transfer to the empty Rust cache.
6. **#122 Complete rail YAPF search, caches and reservation.** Own all four
   searches, six specialization-specific global cache banks, rail-change
   invalidation, reservation traversal and rollback. Reuse the rail corpus and
   ship search machinery only where ordering matches; require branch witnesses.
7. **#124 Complete road YAPF search and path construction.** Own both track and
   depot searches, all node/queue/segment state and reconstruction. Replace
   #121's temporary result bridge using its canonical path cache; do not claim
   the cache twice. Reuse road scenarios and compatible search support.
8. **#125 Complete station cargo-service control and state.** Own loading order,
   service/cargo metadata, full loading/reservation/refit policy, acceptance,
   ratings/distribution and periodic service loops. Preserve #117's payment and
   production-flush owner and shared packet/flow/map storage. Reuse its fixtures.
9. **Following selections:** rail vehicle controller, complete industry periodic
   owner, company/economy loop, orders, cargo and their commands. Rail controller
   needs broader reversal/crossing/reservation evidence than the current two-train
   save; #122 should supply it. Aircraft stays gated on #111 and #86 input.

The accepted #108 decision keeps canonical map arrays in C++ with direct bundled
`noexcept` services. Counts establish crossing density, not a bottleneck; no raw
view or allocation transfer is selected. Storage transfers still require explicit
selection following `docs/design/world-state.md`.

## Resume checkpoint (2026-10-04)

Root: `/root` (gpt-6-astra, ultra). Integration base `9732d8bfa2`; six
integrations since steering, stocktake updated. Next stocktake after two more
integrations. All rows below are active, not integrated completion.
Worktrees are siblings of the main checkout unless a path says otherwise.

| Issue / owner | Branch and checkpoint commit | Worktree | Next step |
| --- | --- | --- | --- |
| #103 `/root/disaster_ownership_103` (Sol high) | `port-disaster-vehicles` at `390fff0025`, PR #118 | `openttd-rust-disasters` | Review accepts `9f9c265afc`; root base update changes docs/scenario registration only, src/rust identical. Combined smoke passes; required CI reruns. |
| #117 `/root/cargo_payment_delivery` (Sol high) | `cargo-payment-delivery-117` at local `bbc1925d4d`, PR #123 | `openttd-rust-cargo-payment` | Two findings fixed in447b5e5608, clean base merge bbc1925d4d. Pair/self pass; finish soak, push once, same reviewer checks final commit and new CI. ABI 49/50/51. |
| #119 `/root/ship_yapf_ownership` (Sol high) | `ship-yapf-ownership-119` at `f85a8a727f`, PR #126 | `openttd-rust-ship-yapf` | Full corpus pair6/6 passes, native heap/path and Cargo checks pass. Finish soak/self and counter-based branch audit; fresh reviewer next. Rare limit/retry/docking gaps need explicit disposition. ABI 52-56. |
| #120 `/root/town_growth_ownership` (Sol high) | `town-growth-ownership-120` at `350aec9e30` | `openttd-rust-town-growth` | Full owner builds and candidate pair6/6 passes; native ABI/growth tests pass. Finish verify/soak, bounded unchanged-reference tunnel gap check, then draft PR and fresh review. ABI 60+. |
| #121 `/root/road_vehicle_ownership` (Sol high) | `road-vehicle-ownership-121` at `aa14879225`, uncommitted implementation | `openttd-rust-road-vehicles` | Owner writes Rust/controller/FFI (ABI 80-83; 84-89 released). `/root/road_state_save_adapters` owns roadveh.h/external/save adapters in the same worktree; signatures pinned in road_ffi.h. |
| #122 `/root/rail_yapf_ownership` (Sol high) | Not started; branch from integration base | To create `openttd-rust-rail-yapf` | Selected next; own search/cache/reservation together and strengthen applicable rail witnesses. |
| #124 `/root/road_yapf_ownership` (Sol high) | Not started; use #121 path API | To create `openttd-rust-road-yapf` | Selected; full road search and reconstruction, no separate persistent cache. |
| #125 `/root/station_service_ownership` (Sol high) | Not started; depends on #117 owner | To create `openttd-rust-station-service` | Selected; complete station service state and periodic/loading control, reusing economic fixtures. |

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
until its coupled branches have suitable evidence. Replenish the queue before
fewer than two unstarted selections remain. Fresh Astra high planner `/root/plan_after_vehicle_pathfinding` selected #124
and #125. Three unstarted owners are queued; replenish when fewer than two
remain. A future industry selection must include histories, daily/monthly control,
closure and builder targets/backoff, not only the short production tick.

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
