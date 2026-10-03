# OpenTTD-Rust near-term roadmap

Root owns this file and updates it when a phase completes or priorities change.
`AGENTS.md` and `docs/rust-migration.md` define the rules; this file decides what
to work on next. If an issue conflicts with this roadmap, follow the roadmap;
a subagent stops and reports the conflict in its hand-off to root.

## Where the fork stands (2026-10-03, `rust-migration` at `f40b0a73f8`)

- Ported: one landscape kernel, StringConsumer/StringBuilder/UTF-8/byte-string
  utilities, history and spiral/alternating iterators, Script Admin JSON
  conversion, ScriptList storage, the widget descriptor parser, authentication
  contexts, monocypher ChaCha20/Poly1305/AEAD, and station cargo-list reducers.
- Size: about 5.8k lines of Rust, 5.6k lines of comparison tooling and about
  3.3k lines of new C++ glue, against roughly 382k lines of first-party C++.
  Almost all of the ported code is utility or vendored-library code.
- Quality: the original behavior has been preserved carefully, every platform
  build is green, and reviews are thorough.
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

## Phase 0: close out in-flight work (now)

| Item | Disposition |
| --- | --- |
| PR #62 (BLAKE2b, Packet, X25519, string validation) | Reviewed. Base-updated after #80; integrate when its CI is green. Then close the superseded component drafts #57, #60, #61, #63. |
| PR #70 / issue #65 (ScriptList VM control) | Stacked on #62. After #62 lands, update it from base (take the base workflow file), finish its review, integrate, and close #65. |
| PR #66 / issue #64 (curve family) | Paused: its macOS Release check fails, which needs another implementation round. Leave the draft open; no further work. |
| Issue #68 (SHA-512/HMAC/HKDF/Ed25519) | Paused. No new `src/3rdparty` work beyond finishing #62/#66 as stated. |
| Issue #69 (tile areas, bitmap, tile lists) | Paused. Revisit as an ownership port when station/industry work needs it. |
| Issue #75 (GetPartialPixelZ full-domain fidelity) | Do it. Small. |
| Issue #76 (worktree/branch cleanup) | Local worktrees done 2026-10-03. Remaining: delete remote branches of merged or closed PRs, then close. Luna low. |

## Phase 1: simulation comparison harness (#72), the critical path

Run the reference and the candidate headlessly on identical scenarios, take
periodic uncompressed snapshots using the existing `-d desync=3` hook, decode
the save chunks, and compare them field by field. The scenarios are the
regression saves, generated maps across seeds, and one committed
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

## Phase 2: first game-logic ports with Rust-owned state

Implementation of both starts now, in parallel with #72: the Rust code, its
unit tests, and the C++ facade, on their own branches. Integration waits for
#72 and a clean harness run that includes scenarios exercising the component
(add them to the harness's scenario list). Both are self-contained,
game-visible and deterministic, with a clean ownership boundary.

- **#73 TGP terrain generator** (`src/tgp.cpp`). Rust owns the height map and
  every generation stage. C++ keeps a facade plus callbacks for `Random` and
  progress. The random-draw order and the float/double arithmetic must match
  exactly.
- **#74 Link graph job** (`src/linkgraph/demands.cpp`, `mcf.cpp`,
  `flowmapper.cpp`). The job already runs on its own thread over a copy of the
  graph. Rust takes a snapshot of that copy, computes the flows and returns them.
  C++ keeps scheduling, save/load and the join.

Both issues set a budget for C++ glue, and both require the C++ body to be
compiled only in the portable build.

## Phase 3 candidates

Start these after #73 or #74 integrates, choosing by what it taught about
callback-heavy boundaries. The candidates are roughly in order of increasing
coupling:

- Town name generation (`townname.cpp`): pure, and can be compared exhaustively.
- Tree tile loop and tree placement (`tree_cmd.cpp`): tile-loop callbacks and
  `Random`.
- Effect and disaster vehicles (`effectvehicle.cpp`, `disaster_vehicle.cpp`):
  the first vehicle-type ownership work.
- Ship pathfinding (`pathfinder/water_regions.cpp`, YAPF ship).
- Economy: cargo payment, inflation, station rating, industry production.
- After those: town growth, road/rail vehicle controllers, YAPF rail/road.

Map/tile storage and pool ownership will need a design note before any of
these crosses into shared world state. Astra high writes it now, as
`docs/design/world-state.md` (about 80 lines: options, the chosen boundary,
callback costs, and how saves and the harness stay unchanged), and root
accepts it by merging it like any other PR.

## Choosing the next task

Take the first unblocked item from the earliest phase that has one. Work
already in CI or review is not blocking: start the next item while it runs.
Phases 1 and 2 and the design note run concurrently. At most two ports may sit
implemented but unintegrated while they wait for #72; put spare capacity into
#72, its harness scenarios and the bugs it finds. Paused, deferred and
out-of-scope issues are not fallbacks. Once #73 or #74 integrates, root picks
the next phase 3 candidate and adds its issue here.

## Out of scope for the near term

More `src/3rdparty` ports (monocypher, squirrel, others). GUI rendering and
layout. Platform or toolchain expansion. Deferred behavior improvements stay in
`deferred-improvement` issues.

## Progress metric

Every port PR pastes the output of `python3 tools/port-metrics.py`: Rust added,
tooling added, C++ glue added (new `src/` lines compiled with `WITH_RUST`), and
C++ retired (net new `src/` lines compiled only without `WITH_RUST`, under
either guard form, plus deleted files). A healthy port retires more C++ than it
adds as glue plus tooling; ports that fail this state a reason in the PR. For
scale, the history port (#32) measured Rust 433, tooling 379, glue 203,
retired 64: the kernel-extraction pattern phase 2 must avoid.
