# OpenTTD-Rust near-term roadmap

Root owns this file and updates it when a phase completes or priorities change.
`AGENTS.md` and `docs/rust-migration.md` define the rules; this file decides what
to work on next. If an issue conflicts with this roadmap, follow the roadmap;
a subagent stops and reports the conflict in its hand-off to root. Keep this
file forward-looking: completed work is one table row, and its evidence stays in
the PR (`AGENTS.md`, "Evidence budget").

## Where the fork stands (2026-10-04, integration `c00402350b`)

- Five ownership ports retired 7,323 original C++ lines, under 2% of roughly
  384k non-vendored `src/` lines. Of that count, 3,848 is the town-name port,
  mostly data tables; the other four account for 3,475 lines of game components.
- The first two integrations after steering (#112/#113) retire no additional
  game logic. The harness split adds 190 net tooling lines; the direct-call tree
  rewrite removes 35 net lines across Rust, C++ and docs. Gross metric additions
  include moved code and must not be mistaken for new simulation ownership.
- Effects remains the landed overrun: 965 glue/tooling lines for 548 retired.
  Pending water and disaster ports also exceed retired C++ by 475 and 133 lines,
  respectively; their PRs explain shared ship fixtures and lifecycle witnesses.
  Reuse that evidence infrastructure in subsequent ports.
- Course after these two infrastructure integrations: finish the required direct
  services/map decision, integrate the reviewed ownership work, and move on to
  cargo delivery and full ship search. Do not expand generic harness tooling or
  polish evidence already accepted by review to keep agents occupied.
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

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness maintenance

The harness is `python3 tools/migration.py simulate` (`docs/rust-migration.md`,
"Simulation comparison"); its default set runs in CI. Harness regressions are
always the first priority. Port differences go in `KNOWN_FAILURES` with an issue,
never in masks.

- **#86 Rail and aircraft scenarios** (the ship slice is part of #104). Run on spare
  capacity starting now; each controller/YAPF port needs evidence for its vehicle
  type. PR #110 supplies the owner's rail/ship save with input normalization and
  unchanged output masks; #104 adds the ship-routing corpus. Aircraft needs
  an owner-built input unless the owner permits a supplemental setup AI (#111).

## Phase 3: current ownership work, in order

1. **#107 Direct shared-service calls; effects PR #115 remains.**
   Trees are integrated. Effects has accepted review and green required CI; root
   integrates it next. Its industry query extends the common service table, ABI 42.
2. **#103 Disaster scheduling, vehicles and event control, PR #118.**
   Complete independent review of the direct-call owner and its final evidence.
   Actual #109/#113/#115 dependency ancestry is included; retain the real UFO/train,
   airport, industry, release/reload and submarine lifecycle witnesses.
3. **#104 Water-region cache and graph service, PR #114.** The visitor alias
   correction is accepted. Update from the integration base, check any source
   conflict resolution, then green CI and integration. Reuse its
   ships corpus for full ship YAPF; do not start another ship comparison tool.
4. **#108 Map access decision accepted; measurement PR #116 remains.**
   Keep canonical map arrays in C++ and direct bundled `noexcept` services, as
   recorded in `docs/design/world-state.md`. Counts establish crossing density,
   not a bottleneck; timings cannot resolve small overhead. No raw shared view or
   allocation transfer is selected. Integrate reviewed measurement code after
   its base update and required CI.
5. **#117 Cargo payment and delivery ownership.** Selected after a fresh Astra
   high comparison with station ratings and industry production. Own CargoPayment
   state and lifetime, delivery acceptance/payment control, destination collection
   and the complete production-flush loop. Preserve Money saturation, native-width
   intermediates and CAPY lifecycle. Loading/reservation and shared industry state
   remain C++; this is not an income-formula extraction. Begin while earlier PRs
   are in review/CI, using the existing road scenarios and #107/#109 interfaces.
6. **#119 Complete ship YAPF and path cache ownership.** Own both search levels,
   their queues/arenas/corridor/retries and canonical `Ship::path`, including all
   controller and save adapters. Reuse #104's corpus; no separate comparison tool.
   Begin while #104 finishes integration, using its real dependency ancestry.
7. **Next selection:** complete town growth (fresh scope planning underway),
   then road/rail vehicle controllers and YAPF rail/road. Aircraft stays gated
   on #111 and the remaining #86 fixture.

Storage transfers (map arrays, pools) still require explicit selection here,
following `docs/design/world-state.md` as amended by #108.

## Resume checkpoint (2026-10-04)

Root: `/root` (gpt-6-astra, ultra). Integration base `c00402350b`; two
integrations since steering, stocktake above updated. Next stocktake after two
more integrations. All rows below are active, not integrated completion.
Worktrees are siblings of the main checkout unless a path says otherwise.

| Issue / owner | Branch and checkpoint commit | Worktree | Next step |
| --- | --- | --- | --- |
| #107 effects `/root/effects_direct_services_107` (Sol high) | `effects-direct-services-107` at `ef7187239f`, PR #115 | `openttd-rust-effects-direct` | `/root/review_effects_pr115` accepts; CI green. Root integrates and corrects common Random/map wording and RANDOM_DEBUG source-location limit in docs. |
| #103 `/root/disaster_ownership_103` (Sol high) | `port-disaster-vehicles` at `9f9c265afc`, PR #118 | `openttd-rust-disasters` | `/root/review_disasters_pr118` (Astra medium) reviewing final owner and evidence; required CI running. |
| #104 `/root/water_regions_104` (Sol high) | `port-water-regions` at `048502be36`, PR #114 | `openttd-rust-water-regions` | `/root/review_water_pr114` accepts final alias fix; update base without force-push, recheck source conflicts if any, then CI and integration. |
| #86 `/root/rail_air_scenarios_86` (Sol high) | `harness-multimodal-save` at `5a99f2efe6`, PR #110 | `openttd-rust-saves` | `/root/review_rail_pr110` accepts, CI green; root integrates. Aircraft decision remains #111. |
| #108 `/root/map_access_decision_108` (Astra high) | `map-access-measurements-108` at `bdb5313a50`, PR #116 | `openttd-rust-map-access` | `/root/review_measurements_pr116` accepts; CI green but base conflicts. Root records accepted decision here; update base and CI before code integration. |
| #117 `/root/cargo_payment_delivery` (Sol high) | `cargo-payment-delivery-117` from `9f5f580078`, uncommitted owner | `openttd-rust-cargo-payment` | Build and initial road pair pass; payment/reload/acceptance witnesses underway. `/root/cargo_multidestination_scenario` supports real multi-industry fixture. ABI 49/50/51 reserved. |
| #119 `/root/ship_yapf_ownership` (Sol high) | To branch from current integration base, merge actual #104 dependency | To create `openttd-rust-ship-yapf` | Selected; start complete search/cache owner, reuse ships scenarios, keep shared storage in C++. |

Preserved evidence branches: `evidence-disaster-vehicles` at `a775543162`
(`openttd-rust-disasters-evidence`) and `evidence-water-regions` at `c752070cde`
(`openttd-rust-water-evidence`). Their useful source inputs are incorporated;
do not reapply borrowed effect helper `73ccd511fb`. Build/test with `--jobs 2`;
required CI supplies default/all-comparison checks where local component evidence
already covers the change. Keep long-running checks in an active agent session:
ending and recycling a task thread has killed unfinished background processes.

Fresh planner `/root/plan_next_ownership_selections` (Astra high) selected the
cargo/ship sequence. `/root/plan_town_growth_ownership` (Astra high) is refining
whole growth-loop and house-placement scope for the next issue; root must select
it here before implementation. Continue per-PR reviews while CI runs.

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
