# OpenTTD-Rust near-term roadmap

Root owns this file and updates it when a phase completes or priorities change.
`AGENTS.md` and `docs/rust-migration.md` define the rules; this file decides what
to work on next. If an issue conflicts with this roadmap, follow the roadmap;
a subagent stops and reports the conflict in its hand-off to root.

## Where the fork stands (2026-10-04, `rust-migration` at `5991a6aacd`)

- Ported: one landscape kernel, StringConsumer/StringBuilder/UTF-8/byte-string
  utilities, history and spiral/alternating iterators, Script Admin JSON
  conversion, ScriptList storage and VM control (#70), the widget descriptor
  parser, authentication contexts, monocypher ChaCha20/Poly1305/AEAD/BLAKE2b/
  X25519, Packet, string validation, and station cargo-list reducers. Rust now
  also owns complete TGP terrain generation (#94) and link graph computation (#95).
- Much of the simulation remains in C++. The first ownership ports retired
  2,111 C++ lines with 219 lines of glue and 244 lines of tooling. Continue
  selecting game logic and tracking each port with `tools/port-metrics.py`.
- Quality: both ownership PRs passed required platform CI and independent review;
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

Selected evidence fix: **#97 freeze simulation runtime/provenance**. Long runs
currently hash executables and read HEAD at the end; a rebuild can mix candidate
executions and mislabel evidence. Root owns the isolated runtime and initial
identity capture; Astra medium reviews it. Run this alongside #96, without
changing semantic decoding or output masks.

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

## Phase 3: selected next task

**#96 Built-in town-name generation** (`src/townname.cpp`) is in progress. Implementation
owner: `/root/town_names` (Sol high); a separate Astra medium agent reviews it.
Rust will own all 21 generators, seed selection, private output and generator
constants. C++ keeps NewGRF routing, town-name retry/uniqueness checks and the
legacy loader's shared tables. The harness must compare rendered names through
observer AI logs as well as saved state, using identical prepared inputs for
explicit name seeds. This samples the 32-bit seed domain; it is not exhaustive.
PR #98 is in CI with source review and local evidence complete.

**#99 Tree generation, simulation and planting commands** is selected alongside
that review/CI. `/root/link_graph` (Sol high) owns implementation and
`/root/town_name_evidence` (Sol high) owns harness scenarios; Astra medium reviews.
Rust owns generation/placement, tree tile/tick loops and the private counter,
plus planting/clearing command control. Apply the world-state design to live map
reads and reentrant water/NewGRF/command services. The standalone editor forest
brush (`PlaceTreeGroupAroundTile`, InteractiveRandom and zone sweep) is an explicit
follow-up; it uses the migrated placement helper. Rendering and GUI stay in C++.

TGP and link graph favor coarse calls over copied inputs with private Rust state,
nonthrowing leaf callbacks and complete results. Apply that boundary to #96.
Remaining candidates, roughly in order of increasing coupling:

- Effect and disaster vehicles (`effectvehicle.cpp`, `disaster_vehicle.cpp`):
  the first vehicle-type ownership work.
- Ship pathfinding (`pathfinder/water_regions.cpp`, YAPF ship).
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
Harness regressions remain the first priority; finish #97 while #96 integrates
and #99 proceeds. Keep spare capacity on scenarios and independent review. Paused, deferred
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
