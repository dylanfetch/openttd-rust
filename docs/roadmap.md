# OpenTTD-Rust near-term roadmap

Root owns this file and updates it when a phase completes or priorities change.
`AGENTS.md` and `docs/rust-migration.md` define the rules; this file decides what
to work on next. If an issue conflicts with this roadmap, follow the roadmap;
a subagent stops and reports the conflict in its hand-off to root. Keep this
file forward-looking: completed work is one table row, and its evidence stays in
the PR (`AGENTS.md`, "Evidence budget").

## Where the fork stands (2026-10-04, `rust-migration` at `f6a0d66b63`)

- Five ownership ports landed after the course correction (table below), retiring
  about 7,300 C++ lines. That is under 2% of the ~384k non-vendored lines in
  `src/`; most of the simulation is still C++.
- Earlier work (utility kernels, string/UTF-8, crypto, Packet, ScriptList, widget
  parser) predates the course correction and is recorded in
  `docs/rust-migration.md`.
- Process works: each port had an issue, an isolated worktree, independent Astra
  review that caught real defects (#102's RNG exception path), green CI before
  merge, and no harness masks.

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

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness maintenance

The harness is `python3 tools/migration.py simulate` (`docs/rust-migration.md`,
"Simulation comparison"); its default set runs in CI. Harness regressions are
always the first priority. Port differences go in `KNOWN_FAILURES` with an issue,
never in masks.

- **#109 Split `tools/simulate.py` into per-component scenario modules.**
  Behavior-preserving. In progress first; #103/#104 and #86 evidence must use
  the new component modules before integration.
- **#86 Rail and aircraft scenarios** (the ship slice is part of #104). Run on spare
  capacity starting now; required before any rail, road-vehicle or aircraft
  controller or YAPF port. PR #110 supplies the owner's rail/ship save; remove its
  new GLOG output mask in favor of existing input normalization. Aircraft needs
  an owner-built input unless the owner permits a supplemental setup AI (#111).

## Phase 3: current ownership work, in order

1. **#107 Direct shared-service calls; trees PR #113, effects PR #115.**
   Integrate separately after final evidence, independent review and CI. Progress
   cancellation is an ordinary callback/throw boundary, so Rust returns an abort
   action; ordinary draws and map operations remain direct. #115 adds the existing
   effect industry query to the common service table; consumers must match ABI 42.
2. **#103 Disaster scheduling, vehicles and event control.** Complete candidate
   witnesses and review on the direct-call implementation and disasters module.
   Actual #109/#113/#115 dependency ancestry is included; retain the real UFO/train,
   airport, industry, release/reload and submarine lifecycle witnesses.
3. **#104 Water-region cache and graph service, PR #114.** Resolve the defined
   visitor alias case: a callback can mutate the caller's patch descriptor, and
   later sides must observe it while the initial region origin remains fixed.
   Re-review the fix and final evidence, then green CI and integration. Reuse its
   ships corpus for full ship YAPF; do not start another ship comparison tool.
4. **#108 Map access decision** (Astra high). Small opt-in measurement PR #116
   supplies tree crossing counts, copied bytes and timings; #114 supplies water
   profiles. Measure on the same scenarios against the unchanged original and
   distinguish profiler/host overhead. Root records the decision here before any
   item in step 6 is selected; no map storage transfer is selected yet.
5. **#117 Cargo payment and delivery ownership.** Selected after a fresh Astra
   high comparison with station ratings and industry production. Own CargoPayment
   state and lifetime, delivery acceptance/payment control, destination collection
   and the complete production-flush loop. Preserve Money saturation, native-width
   intermediates and CAPY lifecycle. Loading/reservation and shared industry state
   remain C++; this is not an income-formula extraction. Begin while earlier PRs
   are in review/CI, using the existing road scenarios and #107/#109 interfaces.
6. **After #108 and #86:** complete ship YAPF (both levels, after #104), town
   growth, road/rail vehicle controllers, YAPF rail/road.

Storage transfers (map arrays, pools) still require explicit selection here,
following `docs/design/world-state.md` as amended by #108.

## Resume checkpoint (2026-10-04)

Root: `/root` (gpt-6-astra, ultra). Integration base `ce6ecea068`; no integration
since the steering review. All rows below are active, not reviewed completion.
Worktrees are siblings of the main checkout unless a path says otherwise.

| Issue / owner | Branch and checkpoint commit | Worktree | Next step |
| --- | --- | --- | --- |
| #109 `/root/harness_modules_109` (Sol medium) | `harness-modules-109` at `6617573eca`, PR #112 | `openttd-rust-harness-modules` | CI green; `/root/review_harness_pr112` found no code issues. All four full before/after runs pass 119 scenarios; root is isolating timing-dependent snapshot-count differences, then final evidence review and merge. |
| #107 trees `/root/trees_direct_services_107` (Sol high) | `trees-direct-services-107` at `cfcc04907b`, PR #113 | `openttd-rust-trees-direct` | `/root/review_trees_pr113` found no blocking code issues; root finishes final paired/soak evidence. All comparisons passed. Await green CI and integration. |
| #107 effects `/root/effects_direct_services_107` (Sol high) | `effects-direct-services-107` at `ef7187239f`, PR #115 | `openttd-rust-effects-direct` | Finish focused soak, assign fresh reviewer, then CI. Integration docs must name common Random/map services and accepted RANDOM_DEBUG wrapper source locations. |
| #103 `/root/disaster_ownership_103` (Sol high) | `port-disaster-vehicles` at `5ce6534589` | `openttd-rust-disasters` | Final build/verify and candidate witness matrix, then component PR and fresh reviewer. Includes real #109/#113/#115 ancestry; no duplicate prerequisite patches. |
| #104 `/root/water_regions_104` (Sol high) | `port-water-regions` at `8281876e72`, PR #114 | `openttd-rust-water-regions` | Owner fixes descriptor alias; `/root/review_water_pr114` re-reviews resulting commit/regression. Existing six-case soak, negative probe and verify pass; update base normally, never force-push. |
| #86 `/root/rail_air_scenarios_86` (Sol high) | `harness-multimodal-save` at `5a99f2efe6`, PR #110 | `openttd-rust-saves` | `/root/review_rail_pr110` accepts source/evidence (independent 75-snapshot run). Merge after #112 and green CI. Aircraft input decision remains #111. |
| #108 `/root/map_access_decision_108` (Astra high) | `map-access-measurements-108` at `bdb5313a50`, PR #116 | `openttd-rust-map-access` | Finish isolated timing trials, fresh review/CI of measurement code, and short design recommendation; root accepts decision and selects gated work. |
| #117 `/root/cargo_payment_delivery` (Sol high) | New branch from current `rust-migration` | New isolated worktree | Start selected complete payment/delivery owner; reuse road evidence, open one component PR. |

Preserved evidence branches: `evidence-disaster-vehicles` at `a775543162`
(`openttd-rust-disasters-evidence`) and `evidence-water-regions` at `c752070cde`
(`openttd-rust-water-evidence`). Their useful source inputs are incorporated;
do not reapply borrowed effect helper `73ccd511fb`. Build/test with `--jobs 2`;
required CI supplies default/all-comparison checks where local component evidence
already covers the change. Keep long-running checks in an active agent session:
ending and recycling a task thread has killed unfinished background processes.

Fresh planner `/root/plan_next_ownership_selections` (Astra high) recommends full
ship YAPF (both levels and canonical `Ship::path`) after #117, then complete town
growth. These are not selected before #108; root creates their issues and records
the accepted access decision before implementation. Their evidence should reuse
#104 and the existing town corpus. Continue per-PR reviews while CI runs.

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
