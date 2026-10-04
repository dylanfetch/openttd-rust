# OpenTTD-Rust near-term roadmap

Root owns this file and updates it when a phase completes or priorities change.
`AGENTS.md` and `docs/rust-migration.md` define the rules; this file decides what
to work on next. If an issue conflicts with this roadmap, follow the roadmap;
a subagent stops and reports the conflict in its hand-off to root. Keep this
file forward-looking: completed work is one table row, and its evidence stays in
the PR (`AGENTS.md`, "Evidence budget").

## Where the fork stands (2026-10-04, integration `350aec9e30`)

- Five ownership ports retired 7,323 original C++ lines, under 2% of roughly
  384k non-vendored `src/` lines. Of that count, 3,848 is the town-name port,
  mostly data tables; the other four account for 3,475 lines of game components.
- Four integrations after steering (#112/#113/#115/#110) retire no additional
  game logic. The harness split adds 190 net tooling lines; trees remove 35 net
  lines, effects add 65, and rail/ship evidence adds 141 tooling lines. Gross
  additions count moved code; tree/effect retirement here is old glue.
- Effects remains the landed overrun: 965 glue/tooling lines for 548 retired.
  Pending water and disaster ports also exceed retired C++ by 475 and 133 lines,
  respectively; their PRs explain shared ship fixtures and lifecycle witnesses.
  Reuse that evidence infrastructure in subsequent ports.
- Course correction is now active: cargo delivery, full ship search and whole
  town growth have implementation owners. The direct services are complete and
  map access is decided; water/disasters are in final integration. Do not expand
  generic harness tooling or polish evidence already accepted by review. The next
  selected ports must reuse current fixtures and retire complete game loops.
- Earlier utility, parser and crypto work is recorded in `docs/rust-migration.md`.

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

Harness and process: #72 harness (#85), #84 play saves (#87), #97 provenance
freeze (#100), #88 Ruff (#92), #75 partial-pixel fidelity (#91), #90 world-state
design (`docs/design/world-state.md`).

| Issue | Completed maintenance | PR | Commit | Metrics (Rust / tooling / glue / retired) |
| --- | --- | --- | --- | --- |
| #109 | Component scenario modules | #112 | `9e1e5d175d` | 0 / 2414 / 0 / 0; moved code, net +190 lines |
| #107 (trees) | Direct shared services | #113 | `c00402350b` | 640 / 22 / 149 / 22; old glue retired, no new game logic |
| #107 (effects) | Direct shared services | #115 | `e702c4724e` | 241 / 4 / 155 / 86; old glue retired, net +65 lines |
| #86 (rail/ship slice) | Owner-provided transport save | #110 | `350aec9e30` | 0 / 141 / 0 / 0 |

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
2. **#104 Water-region cache and graph service, PR #114.** The visitor alias
   correction is accepted. Update from the integration base, check any source
   conflict resolution, then green CI and integration. Reuse its
   ships corpus for full ship YAPF; do not start another ship comparison tool.
3. **#108 Map access decision accepted; measurement PR #116 remains.**
   Keep canonical map arrays in C++ and direct bundled `noexcept` services, as
   recorded in `docs/design/world-state.md`. Counts establish crossing density,
   not a bottleneck; timings cannot resolve small overhead. No raw shared view or
   allocation transfer is selected. Integrate reviewed measurement code after
   its base update and required CI.
4. **#117 Cargo payment and delivery ownership.** Selected after a fresh Astra
   high comparison with station ratings and industry production. Own CargoPayment
   state and lifetime, delivery acceptance/payment control, destination collection
   and the complete production-flush loop. Preserve Money saturation, native-width
   intermediates and CAPY lifecycle. Loading/reservation and shared industry state
   remain C++; this is not an income-formula extraction. Begin while earlier PRs
   are in review/CI, using the existing road scenarios and #107/#109 interfaces.
5. **#119 Complete ship YAPF and path cache ownership.** Own both search levels,
   their queues/arenas/corridor/retries and canonical `Ship::path`, including all
   controller and save adapters. Reuse #104's corpus; no separate comparison tool.
   Begin while #104 finishes integration, using its real dependency ancestry.
6. **#120 Complete town-growth control and private state.** Own tick traversal,
   growth road walking/build choices, house selection and placement control,
   growth-rate/funding transitions and canonical counters/flags with CITY and
   legacy adapters. Keep unrelated town accounting and shared world state in C++.
   Add actual growth witnesses to the existing towns module.
7. **#121 Complete road vehicle control and private state.** Own consist/tick
   movement, blocking/overtaking/crash/servicing and day handling, canonical road
   counters and path cache, plus save/legacy/external mutation adapters. Existing
   road YAPF may return a temporary result for transfer to the empty Rust cache.
8. **#122 Complete rail YAPF search, caches and reservation.** Own all four
   searches, six specialization-specific global cache banks, rail-change
   invalidation, reservation traversal and rollback. Reuse the rail corpus and
   ship search machinery only where ordering matches; require branch witnesses.
9. **Following selections:** rail vehicle controller, road YAPF, then stations,
   industries, company/economy loop, orders, cargo and their commands. Rail
   controller needs broader reversal/crossing/reservation evidence than the
   current two-train save. Aircraft stays gated on #111 and the remaining #86 input.

Storage transfers (map arrays, pools) still require explicit selection here,
following `docs/design/world-state.md` as amended by #108.

## Resume checkpoint (2026-10-04)

Root: `/root` (gpt-6-astra, ultra). Integration base `350aec9e30`; four
integrations since steering, stocktake updated. Next stocktake after two more
integrations. All rows below are active, not integrated completion.
Worktrees are siblings of the main checkout unless a path says otherwise.

| Issue / owner | Branch and checkpoint commit | Worktree | Next step |
| --- | --- | --- | --- |
| #103 `/root/disaster_ownership_103` (Sol high) | `port-disaster-vehicles` at `390fff0025`, PR #118 | `openttd-rust-disasters` | Review accepts `9f9c265afc`; root base update changes docs/scenario registration only, src/rust identical. Combined smoke passes; required CI reruns. |
| #104 `/root/water_regions_104` (Sol high) | `port-water-regions` at `b3eea95239`, PR #114 | `openttd-rust-water-regions` | `/root/review_water_pr114` accepts final head after ABI resolution re-review. Verify, Ruff/provenance pass; required CI remains. |
| #108 `/root/map_access_decision_108` (Astra high) | `map-access-measurements-108` at `3c3ffa09b4`, PR #116 | `openttd-rust-map-access` | Review accepts original code; conflict-free base merge, verify and focused smoke pass. Required CI rerunning; root integrates when green. |
| #117 `/root/cargo_payment_delivery` (Sol high) | `cargo-payment-delivery-117` at `c301dccb87`, PR #123 | `openttd-rust-cargo-payment` | `/root/review_cargo_pr123` (Astra medium) reviews full owner and evidence. Verify, self/soak, native gap checks pass; reference job drain is documented. CI running; ABI 49/50/51. |
| #119 `/root/ship_yapf_ownership` (Sol high) | `ship-yapf-ownership-119` at `ea5a304f1c`, actual #104 dependency | `openttd-rust-ship-yapf` | Full owner builds; first structures pair passes. Audit source ordering, complete existing corpus and narrow native heap gap check. ABI 52-56. |
| #120 `/root/town_growth_ownership` (Sol high) | `town-growth-ownership-120` at `350aec9e30` | `openttd-rust-town-growth` | Implement full owner/adapters (ABI 60+). Scenario delegate completed towns.py: self default 6/6, soak 10/10. Candidate pairs next; tunnel and command/lifetime gaps stated. |
| #121 `/root/road_vehicle_ownership` (Sol high) | `road-vehicle-ownership-121` at `aa14879225`, uncommitted implementation | `openttd-rust-road-vehicles` | Owner writes Rust/controller/FFI (ABI 80-89 reserved). Delegate roadveh.h and external/save adapters when a slot opens; exact interfaces pinned in road_ffi.h. |
| #122 `/root/rail_yapf_ownership` (Sol high) | Not started; branch from integration base | To create `openttd-rust-rail-yapf` | Selected next; own search/cache/reservation together and strengthen applicable rail witnesses. |

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
fewer than two unstarted selections remain. Only #122 is now unstarted; the next
freed planning slot must select further whole owners from the following list.

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
