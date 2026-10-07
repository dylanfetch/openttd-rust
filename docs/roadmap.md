# OpenTTD-Rust near-term roadmap

Root owns this file and updates it when a phase completes or priorities change.
`AGENTS.md` and `docs/rust-migration.md` define the rules; this file decides what
to work on next. If an issue conflicts with this roadmap, follow the roadmap;
a subagent stops and reports the conflict in its hand-off to root. Keep this
file forward-looking: completed work is one table row, and its evidence stays in
the PR (`AGENTS.md`, "Evidence budget").

## Where the fork stands (2026-10-07, integration `dace87c9b1`)

- Sixteen ownership ports retire 20,947 original C++ lines, about 5.5% of roughly
  384k non-vendored `src/` lines. Town names account for 3,848, mostly data, and
  road movement tables for another 1,475; the remaining retirement is 15,624 lines.
- Eighteen integrations since steering are complete. The last two, service RNG
  #164 and the rail/industry batch #166, retire 3,517 lines against 2,336 glue
  and 1,143 tooling. Ownership retirement is advancing again; finish aircraft
  #145 and company/economy #149 before the selected road performance conversion.
- Landed glue/tooling overruns remain effects 965/548 retired, water 883/407,
  disasters 1,101/968, cargo payment 988/315 and industry 1,152/932. Industry's
  canonical views/save adapters span about twenty consumers; its PR records that
  cost. Keep additional evidence in existing component modules.
- Fresh same-window Opus manual timing is 2.437x before the batch, 2.574x after
  rail (+5.61%) and 2.568x after industry (-0.22%; +5.38% overall). All nine plain
  pairs match state/logs; both increments meet the 10% budget. The absolute cost
  still calls for #155: RoadObserve (14.11% of samples) and the road future
  (9.03%) are major costs. Keep direct conversion after the reviewed queue;
  no raw map view is selected.
- Main's deterministic harness and repaired nightly are green. Post-#164 main
  platform and semantic checks pass; watch post-#166 checks and fix any new
  main failure before another integration. #166 has independent final acceptance,
  all required CI, default 212/212 and focused candidate pair/soak 39/39 each.
- #156 retains worker/abort interleavings and component-specific gaps. Road
  no-destination/service RNG, actual crossing/flooding and early crash draws are
  covered. Single-head expiry #167 has independent acceptance and all required
  checks; integrate it next. Articulated cleanup and landing-crash RNG remain
  open. A bounded original-only plan found a reusable one-tick landing boundary
  in #145's existing setup; it is not candidate coverage yet.
- Six component branches remain unintegrated, restoring #157's cap. No new
  component starts ahead of this reviewed queue. Cargo #151's quadratic-list fix
  is accepted; final base/CI follow road conversion. Next stocktake: integration
  twenty. Merged rail, industry and batch worktrees are removed with ignored
  evidence preserved, alongside earlier completed worktrees.

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
| #122 | Rail YAPF search, cache and reservation | #133 via #141 | `ea80a746a3` | 1704 / 144 / 595 / 1517 |
| #124 | Road YAPF search and path construction | #135 via #141 | `ea80a746a3` | 663 / 55 / 329 / 532 |
| #125 | Station cargo-service control and state | #142 | `ba920555ca` | 1516 / 241 / 1022 / 1332 |
| #130 | Rail controller, reservation and private state | #143 via #166 | `dace87c9b1` | 4087 / 613 / 1660 / 2585 |
| #129 | Industry production, histories and builder state | #144 via #166 | `dace87c9b1` | 1508 / 474 / 678 / 932 |

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
| #158 | Restore existing MinGW i686 nightly dependencies | #159 | `55ad3a84ad` | 0 / 0 / 0 / 0; workflow-only |
| #155 (measurement) | Semantic-valid speed report and benchmark isolation | #163 | `7fc597da60` | 0 / 212 / 0 / 0; road conversion remains open |
| #140 | CI-capacity integration of rail/road search owners | #141 | `ea80a746a3` | aggregate 2369 / 197 / 927 / 2049; not additional retirement |
| #156 (routing/crash) | Road RNG, crossing crash and event witnesses | #161 | `f8b5034ed3` | 0 / 306 / 0 / 0 |
| #156 (flooding) | Ordinary road flooding crash witness | #162 | `0b2503cb30` | 0 / 155 / 0 / 0 |
| #156 (service RNG) | Both road service routing random outcomes | #164 | `ee6fcdd44b` | 0 / 64 / 0 / 0 |
| #165 | CI-capacity integration of rail/industry owners | #166 | `dace87c9b1` | aggregate 5595 / 1079 / 2336 / 3517; not additional retirement |

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

The #86 rail/aircraft fixture gate is complete. Controller ports still require
component-specific branch witnesses; extend the existing scenario modules and
reuse the supplied rail/ship save and authorized aircraft setup AI.

## Phase 3: current ownership work, in order

On resume: finish coverage #167, integrate #145 and #149, then #155's road
conversion. Next finish #151, #152, #153 and #147, then start #148 and #150
from integrated `rust-migration`. Never hold more than six unintegrated component
branches (#157). Ship #146 names orders #138 as an actual ancestry dependency;
prepare ship evidence in the listed phase, but its final integration must follow
completed #153. No unreviewed orders checkpoint substitutes for that dependency.

1. **#136 Complete aircraft control and airport movement ownership.** Own both
    controller passes, FTA traversal, terminal/helipad/block allocation, private
    aircraft state and canonical station airport masks, plus service/diversion/
    crash and save adapters. Reuse #132; add actual contention, dedicated helipad,
    closure/removal and crash witnesses. Coordinate #125's station lifetime.
2. **#137 Complete company financial and economy lifecycle ownership.** Own
    canonical finances/histories, bankruptcy/offer/acquisition/deletion control,
    world ownership-transfer traversal, periodic economy/prices and financial
    commands. Preserve real VM/deletion reentry and #129 ECMY fields. Require
    multiple-company recovery/transfer/deletion evidence, not only profitable play.
3. **#138 Complete orders, shared lists, backups and timetable ownership.** Own
    canonical vectors/current orders/shared links/backups, editing/validation/
    execution/destination resolution, timetable and depot-unbunching control.
    Preserve controller reentry, save formats and #83 unmasked timing evidence.
4. **#139 Complete cargo packet, list, flow and movement ownership.** Own packet
    contents, ordered station/vehicle containers, actions/caches/flows and the
    completed-link-job live-flow application. Preserve #117/#125/#74 policy
    boundaries, stable identity/iteration and shared pool-shell allocation.
5. **#146 Complete ship control and private-state ownership.** Own movement,
    locks, rotation/reversal, day/service/build policy and nearest-depot regional
    BFS with canonical state/rotation coordinates. Preserve #119's path owner;
    adapt every save/external writer and reuse the water corpus for real witnesses.
6. **#147 Complete fleet grouping and replacement lifecycle ownership.** Own
    group hierarchy/membership/statistics, ordered renewal rules, complete
    replacement transactions/rollback and the ordered tick-end replacement drain.
    Preserve #137 finances, #138 orders and #139 cargo at original mutation points.
7. **#148 Complete town lifecycle, authority and house simulation.** Extend
    #120's sole owner with remaining town state, founding/deletion/authority,
    monthly histories and house construction/cargo/rebuild/animation loops.
    Preserve generation cancellation and destructive house callback ordering.
8. **#150 Complete industry construction and tile lifecycle.** Extend #129's
    sole owner with construction/prospecting/generation, canonical metadata and
    ordered registry, destruction and complete tile/animation policy. Preserve
    oilrig/station/cargo cleanup, flooding rechecks and generation cancellation.
9. **Following selections:** remaining simulation components and commands that
    change them, guided by canonical state and complete control-loop ownership.

The accepted #108 decision keeps canonical map arrays in C++ with direct bundled
`noexcept` services. Counts establish crossing density, not a bottleneck; no raw
view or allocation transfer is selected. Storage transfers still require explicit
selection following `docs/design/world-state.md`.

## Resume checkpoint (2026-10-07, migration active)

Root: `/root` (gpt-6-astra, xhigh). Latest integration is `dace87c9b1` (#166);
**eighteen integrations since steering**. Main #164 validation 37582451591 and
platform 37582452027 pass. Watch post-#166 main checks; fix any new failure before
integration. All merge guards from earlier tasks are finished; do not restart
old monitors. Completed worktrees, including rail, industry and their batch, are
removed after preserving ignored artifacts and branch references.

The user set a weekly usage wind-down threshold of 50%. The latest host reading
is 48% at 2026-10-07 07:21 UTC. Monitor the active session's seven-day usage
window. At 50%, stop starting new work except what is needed for a clean handoff;
finish active validation, reviews, necessary fixes and cleanup without abruptly
cancelling jobs, update this checkpoint, then wrap up. Do not mark the migration
complete.

Integrate reviewed/green #167 next, then refresh #145 and #149 from actual main.
Follow with #155 road conversion, then finish #151/#152/#153/#147 with the ship
ancestry dependency above. Six unfinished component branches remain; integrate
reviewed work before any new component. All worktrees below are siblings of the
main checkout; source reviews and checkable evidence remain in their PRs.

| Issue / PR | Branch, worktree suffix, current head | Next step |
| --- | --- | --- |
| #136 / #145 | `aircraft-controller-ownership-136`, `aircraft-controller`, `dd0f2f0475` | Source accepted; exact verify and clean-head pair/self 21/21 pass. Historical ratio 2.567x. Owner `/root/aircraft_final_integration_145` (Sol high) completed read-only preparation; resume on actual main after #167 for fresh base/final timing, necessary conflict review and final CI. |
| #137 / #149 | `company-economy-ownership-137`, `company-economy`, `5b9a005064` | Accepted industry warning delta; exact verify, company/reload 13/13 and changed industry preparation 2/2 pass. Ratio 2.543x. Refresh after #145; final CI remains. |
| #156 / #167 | `road-crash-expiry-156`, `road-crash-expiry`, `20404d4a30` | Root-authored ordinary collision cleanup endpoints. Exact verify 97/136/Cargo checks, roads 20/20, focused reference self/candidate soak 2/2 and Ruff pass; mutation delaying deletion fails. Independent review and all required CI pass; combined run against #166 binary also passes. Ready for root integration. |
| #139 / #151 | `cargo-storage-movement-139`, `cargo-storage`, `ff912bcfcb` | F1 quadratic list fix accepted at `a25a7d41e2`; final verify, pair 20/20, self/soak 5/5 each, reload and scaling pass. Ratio 2.588x. Integrate after #155 conversion; same reviewer `/root/review_cargo_storage_151` (Astra medium). |
| #146 / #152 | `ship-controller-ownership-146`, `ship-controller`, `7ec70a1768` | Full owner checkpoint; verify/Cargo/Ruff and 12 comparisons pass. Old ferry failures matched 40 snapshots but differed at wall-time plain exit: join #154 deterministic launcher before rerunning. Still needs reference self/candidate soak, actual build/sell/ID reuse/water-class witnesses, final company/orders/cargo ancestry, fresh review and CI. |
| #138 / #153 | `order-lifecycle-ownership-138`, `order-lifecycle`, `ef58c967f7` | Full owner checkpoint; verify 97/134 and paired regression/depot/reload 67 snapshots pass. Prior 101 self/soak snapshots are reference-vs-reference. Add conditional/implicit active reload, native timetable/backup commands, shared-depot unbunching and initialized unmasked #83 evidence, then candidate soak, fresh review and CI. |
| #147 | `fleet-replacement-ownership-147`, `fleet-replacement`, `68d660adc9` | State-only WIP, no PR; not ownership completion. Join final orders/cargo, finish group/rule/replacement/rollback/pending-drain control and compact scenarios, then full evidence, draft PR, fresh review and CI. |

CI scheduling: #143/#144/#145/#149/#151 heavy workflows were deliberately
cancelled to free repair/integration runners. No cancelled check permits merging.
The host limits fresh tasks despite completed agent entries, but an existing
reviewer can resume concurrently. Continue within actual capacity; preserve
same-PR reviewers when still live.
Batch #166 is integrated; owner and reviewer tasks finished. Source joins and
combined acceptance name final `1230641e58` in #143/#144/#166. PR #167 source
acceptance names `20404d4a30`; its additional two-case pair against #166's verified
binary passes both modes. No new base update is needed unless GitHub reports a
conflict; normal merge must still require its exact accepted head and checks.

The corrected #155 plan in its issue converts all 22 road services directly and
removes the entire Task/Future/Rc/action protocol for all 14 entry kinds. No live
owner borrow crosses the audited reentrant services. Recheck integrated callees,
short borrow scopes and deletion order before implementation. Benchmark a fresh
post-queue baseline; the historical 2.398x is not that before measurement.

Standing #156 work: the unchanged crossing already covers three crash-rotation
Random draws, confirmed by a negative probe in both modes. #167 establishes
actual-collision expiry at 2238/2239 ticks against the unchanged reference;
root owns it and `/root/review_road_crash_expiry_167` (Astra medium) accepts it.
Articulated deletion,
surviving-head behavior and road-stop cleanup remain open. Next selected RNG slice
is aircraft landing after integrated #145, reusing its eight-plane 411-tick reload
setup. Original-only probes found plane 2 at state 16/pos 34, speed 293, target 0.
At setting 1 the third draw's low 22 bits equal 21 (crash) or 22 (survival);
DATE seeds `(2443390976,1012692424)` / `(2443382784,1011643848)` exercise these.
Disabled setting 0 draws twice less; `(2443390968,1012691400)` tests high-bit
masking. One tick witnesses cargo removal/counter 3; a passive 16-tick observer
sees landing event `2 10064 3 10`. Active station ratings end at 0 after surrounding
penalties, not 1. Preserve fresh preparation/typed-edit provenance; do not commit
scratch saves. Candidate mutations, event desync mode and required PR process
remain; no implementation branch exists. Short-strip jets/cheat/setting 2 remain gaps.

Fresh Astra high planning for #153 found BKOR entries are saved only by a network
server and cleared on offline/server load. Ordinary reload must witness clearing;
client-load fixups and restoration need a narrow identical native boundary check.
Keep the original source unchanged, hash the actually relinked executable/runtime
and adapter inputs, and leave network transport explicitly uncovered. Use one
orders scenario module; do not build a general multiplayer harness for this gap.

Shared ABI identifiers widen to u16 in cargo ancestry; preserve bounded crypto
and company conversions when joining branches. Build/test with `--jobs 2`.
Retain each process in an active task session and coordinate idle timing windows.
The speed tool now supplies a clone-wide shared/exclusive game lock; standalone
fixture entrypoints must initialize it too.

Preserve the pinned reference, paused curve worktrees and explicit evidence
branches `evidence-disaster-vehicles` (`a775543162`) and `evidence-water-regions`
(`c752070cde`). Useful inputs are incorporated; do not reapply effect helper
`73ccd511fb`. Completed-work receipts are archived, while checkable evidence and
review dispositions remain in the linked PRs.

#148 town/house lifecycle and #150 industry construction/tile lifecycle remain
selected and unstarted. Begin them only in the specified order from integrated
main after the existing queue. When fewer than two unstarted selections remain,
use a fresh Astra high planner to replenish whole simulation owners.

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
