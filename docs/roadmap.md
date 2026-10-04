# OpenTTD-Rust near-term roadmap

Root owns this file and updates it when a phase completes or priorities change.
`AGENTS.md` and `docs/rust-migration.md` define the rules; this file decides what
to work on next. If an issue conflicts with this roadmap, follow the roadmap;
a subagent stops and reports the conflict in its hand-off to root.

## Where the fork stands (2026-10-04, `rust-migration` at `1fecd819d8`)

- Ported: one landscape kernel, StringConsumer/StringBuilder/UTF-8/byte-string
  utilities, history and spiral/alternating iterators, Script Admin JSON
  conversion, ScriptList storage and VM control (#70), the widget descriptor
  parser, authentication contexts, monocypher ChaCha20/Poly1305/AEAD/BLAKE2b/
  X25519, Packet, string validation, and station cargo-list reducers. Rust now
  also owns complete TGP terrain generation (#94), link graph computation (#95)
  and all 21 built-in town-name generators (#98).
- Much of the simulation remains in C++. The first ownership ports retired
  2,111 C++ lines with 219 lines of glue and 244 lines of tooling. Continue
  selecting game logic and tracking each port with `tools/port-metrics.py`.
- Quality: these ownership PRs passed required platform CI and independent review;
  combined-branch evidence is recorded below.
- Workflow (#80): worktrees share one reference build and a compiler cache, a
  fresh-worktree `verify` takes about a minute, comparisons run in parallel,
  and merges no longer force other PRs to be up to date.

## Course correction

Three habits held back progress. The rules in `AGENTS.md` now prevent them.

1. **Picking components by test coverage alone.** Upstream unit tests cover
   utilities, so test coverage kept pointing at utilities and at vendored crypto,
   never at the game. Fix: build a semantic simulation harness (phase 1). That
   makes game logic testable, and game logic becomes the priority.
2. **Kernel extraction.** Many ports move one algorithmic fragment into Rust
   while C++ keeps the state, iteration and control flow. Each such port adds a
   new FFI surface, ABI-layout checks and a dedicated comparison tool, but
   retires little C++. Fix: prefer ports where Rust owns the component's state
   and control flow, so that its C++ body exists only in the portable build.
3. **Evidence volume.** Issues, PRs, docs and review reports re-derive every
   detail in prose, which costs more than the code. Fix: a size budget for each
   artifact (see `AGENTS.md`), and the harness replaces most bespoke fixtures.

## Phase 0: active close-out items complete

| Item | Disposition |
| --- | --- |
| Issue #88 (ruff for `tools/`) | Complete in #92 (`584aa2bd82`): pinned lint/format gates, separate formatting commit, independent review and required CI passed. |
| PR #70 / issue #65 (ScriptList VM control) | Integrated at `8edca751f7` after final Astra medium review and green required CI; #65 closed. |
| PR #66 / issue #64 (curve family) | Paused: its macOS Release check fails, which needs another implementation round. Leave the draft open; no further work. |
| Issue #68 (SHA-512/HMAC/HKDF/Ed25519) | Paused. No new `src/3rdparty` work beyond finishing #62/#66 as stated. |
| Issue #69 (tile areas, bitmap, tile lists) | Paused. Revisit as an ownership port when station/industry work needs it. |
| Issue #75 (GetPartialPixelZ full-domain fidelity) | Complete in #91 (`498bcb8f42`): original coordinate domain and valid UINT32_MAX heights restored; review, comparisons, simulation and required CI passed. |
| Issue #76 (worktree/branch cleanup) | Complete: 37 merged-PR remote branches deleted; open PR and unrelated branches retained. #76 closed. |

## Phase 1: simulation comparison harness (#72), the critical path

Status: the harness is `python3 tools/migration.py simulate` (see
`docs/rust-migration.md`, "Simulation comparison"); its default set runs in CI.
The `play-*` scenarios cover road networks under manual distribution and
cargodist (#84). Reload testing for #74 exposed build-history differences even
without that port (#93), fixed in #95 by resetting only prepared-input history
and verifying all other chunks unchanged. No output masks were added.
Port differences still require `KNOWN_FAILURES`, never masks. Rail, ship and
aircraft scenarios (#86) come before ports of those vehicle types.

**#97 runtime/provenance freeze is complete** in #100 (`1c04143c36`). Both
executables and runtime assets are copied before scenarios, with initial checkout
identity and executed-file hashes. Three controlled rebuild tests cover default,
custom and reference-self runs. Independent review and required CI passed; semantic
decoding and output masks are unchanged.

Original scope: run the reference and the candidate headlessly on identical
scenarios, take periodic uncompressed snapshots using the existing `-d desync=3`
hook, decode the save chunks, and compare them field by field. The scenarios are
the regression saves, generated maps across seeds, and one committed
transport-network save with cargodist enabled. It must pass reference vs
reference and reference vs candidate, detect a deliberate rule change, and run
in CI. The tool is `tools/simulate.py` behind `python3 tools/migration.py
simulate`; it must not match the `tools/*-comparison.py` glob, which would run
it a second time inside the comparisons step.

Reference vs reference must be clean before #72 lands. A reference-vs-candidate
divergence caused by an existing port does not block #72: file it as a bug and
record it as a known failure by its exact first divergence (scenario, chunk,
field, the issue), then land. "Clean" means no divergence outside that list,
and CI fails on any other. The list is emptied by fixing the ports, never by
masking fields. Those bug fixes are phase 1 items and take priority over new
phase 2 or 3 work.

## Phase 2: first game-logic ownership ports complete

Both ports integrated after separate Astra medium review and green required CI:

- **#73 TGP terrain generator**, PR #94 (`014968bd38`). Rust owns the height map
  and every generation stage. C++ keeps settings, shared RNG, progress dispatch
  and tile writes. Metrics: Rust 658, tooling 85, glue 84, C++ retired 991.
- **#74 Link graph job**, PR #95 (`7a2858c469`). Rust owns demands, both MCF passes,
  paths/cycles and flow mapping over copied job inputs. C++ keeps scheduling,
  threads, save/load and station joins. Metrics: Rust 1040, tooling 159, glue 135,
  C++ retired 1120. Both original implementations compile only in portable builds.

Local evidence includes 42 TGP soak cases (1,800 snapshots), 14 road soak cases
(994), reference-self settings/reload runs and final-head default runs. PRs retain
commands, receipts and limits. Root verified the combined `7a2858c469` branch: all four
Cargo checks, 97 reference/111 candidate CTests, native generators, all 12 existing
comparisons, Ruff and the default harness (20/20 scenarios, 401 snapshots, no differences) passed.

## Phase 3: current ownership work

**#96 Built-in town-name generation is complete** in #98 (`ff3648ba09`). Rust
owns all 21 generators, seed selection, private output and generator constants.
C++ keeps NewGRF routing, retry/uniqueness and four legacy-loader tables. Independent
Astra medium review and required CI passed. The harness compares rendered names
and saved state: 42 generated/prepared cases with 106 explicit seed witnesses;
default 62/62 scenarios (566 snapshots), town-name self/soak 42/42 each. This samples
the 32-bit seed domain. Metrics: Rust 3296, tooling 241, glue 122, C++ retired 3848.

Root checked the combined `ff3648ba09` branch: Cargo's four checks, 97 reference/
112 candidate CTests, three provenance regression tests and Ruff passed. The
default harness matched 62/62 scenarios, 562 snapshots in 140.6 seconds; receipts
are `.local/verification/20261004T050321.116498Z/report.json` and
`.local/simulation/20261004T050424Z-1034522/report.json`. No new differences or masks.

**#99 Tree generation, simulation and planting commands** is in implementation
and review. `/root/link_graph` (Sol high) owns implementation and
`/root/town_name_evidence` (Sol high) owns harness scenarios; Astra medium reviews.
Rust owns generation/placement, tree tile/tick loops and the private counter,
plus planting/clearing command control. Apply the world-state design to live map
reads and reentrant water/NewGRF/command services. The standalone editor forest
brush (`PlaceTreeGroupAroundTile`, InteractiveRandom and zone sweep) is an explicit
follow-up; it uses the migrated placement helper. Rendering and GUI stay in C++.
PR #102 has a reviewed correction: live tree RNG draws return to C++ because
`RANDOM_DEBUG` logging can throw. Corrected-head local default passed 102/102
(729 snapshots); tree soak and reference-self soak passed 40/40 each (288 snapshots).
Required corrected-head CI remains the integration gate.

**#101 Effect-vehicle controllers and private state** is selected alongside #99
review. `/root/town_names` (Sol high) owns implementation; Astra medium reviews.
Move all 12 subtype controllers and private animation bytes into Rust; shared
Vehicle fields, pools, viewport updates and rendering stay canonical in C++.
Preserve caller setters and modern/legacy save adapters without a persistent
state mirror. Short-horizon harness checkpoints must observe these brief-lived
effects, plus natural creation and breakdown/bubble caller paths. Ordinary
rail/ship/aircraft controllers remain untouched, so #86 is not a prerequisite.

**#103 Disaster scheduling, vehicles and event control** is selected next while
#99/#101 validate. `/root/link_graph` (Sol high) owns implementation after its tree
handoff; Astra medium reviews. Move the whole disaster family, target-release hooks
and private state/delay; keep shared Vehicle fields, pools and services canonical
in C++. Astra high reference experiments established genuine targets using the
stationlist save's train and airport: two reference-built rail tiles plus declared
company/AIPL edits let a big UFO select the human train, and a zeppelin block the
airport. Turn these into reproducible harness witnesses with actual event outcomes.
This covers disaster interactions; #86's ordinary rail/ship/aircraft route corpus
remains needed before ports of those controllers. No shared pool storage transfer.

**#104 Water-region cache and graph service** is selected as the complete first
component before ship YAPF. `/root/town_names` (Sol high) implements after the effect
handoff; root owns ship harness evidence and Astra medium reviews. Rust owns cache
validity, labels, edge/aqueduct data, flood traversal, lazy rebuild, invalidation and
ordered neighbour visits. Both YAPF levels and nearest-depot search remain external
clients of this service. Arbitrary visitors return to C++ without a live cache borrow.
The ship slice of #86 is now active: a reference-built ferry has demonstrated real
loading and paid delivery. Commit reproducible setup/save evidence and require
manual/cargodist routes, reload, canal/lock/aqueduct and warm-cache mutation cases
before integration. Ordinary rail/aircraft coverage in #86 remains open.

TGP and link graph favor coarse calls over copied inputs with private Rust state,
nonthrowing leaf callbacks and complete results. Trees and effects apply the
world-state design where shared services can reenter or throw.
Remaining candidates, roughly in order of increasing coupling:

- Complete ship YAPF searches, after #104 (both region and tile search levels).
- Economy: cargo payment, inflation, station rating, industry production.
- After those: town growth, road/rail vehicle controllers, YAPF rail/road.

Map/tile storage and pool ownership will need a design note before any of
these crosses into shared world state. The design is
[`docs/design/world-state.md`](design/world-state.md) (#90), authored by Astra high
and accepted by root under the docs-only direct-commit rule. It chooses component
owners with one canonical world store and specifies callback, lifetime and save
constraints; storage transfers still require explicit roadmap selection.

## Choosing the next task

Take the first unblocked item from the earliest phase that has one. Work
already in CI or review is not blocking: start the next item while it runs.
Harness regressions remain the first priority; close #102's corrected-head gates
while #101 proceeds. #103 implementation/evidence are active; use effect handoff
capacity for #104, with its #86 ship evidence developed in parallel.
Keep spare capacity on scenarios and independent review. Paused, deferred
and out-of-scope issues are not fallbacks. Root selects further ownership work
here before implementation starts.

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
fail this state a reason in the PR. For
scale, the history port (#32) measured Rust 433, tooling 379, glue 203,
retired 64: the kernel-extraction pattern phase 2 must avoid.
