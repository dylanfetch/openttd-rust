# OpenTTD-Rust near-term roadmap

Root owns this file and updates it when a phase completes or priorities change.
`AGENTS.md` and `docs/rust-migration.md` define the rules; this file decides what
to work on next. If an issue conflicts with this roadmap, follow the roadmap.

## Where the fork stands (2026-10-03, `rust-migration` at `2ebebd5b85`)

- Ported: one landscape kernel, StringConsumer/StringBuilder/UTF-8/byte-string
  utilities, history and spiral/alternating iterators, Script Admin JSON
  conversion, ScriptList storage, the widget descriptor parser, authentication
  contexts, monocypher ChaCha20/Poly1305/AEAD, and station cargo-list reducers.
- Size: about 5.8k lines of Rust, 5.6k lines of comparison tooling and about
  3.3k lines of new C++ glue, against roughly 382k lines of first-party C++.
  Almost all of the ported code is utility or vendored-library code.
- Quality: the original behavior has been preserved carefully, every platform
  build is green, and reviews are thorough.

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
| PR #62 (BLAKE2b, Packet, X25519, string validation) | Reviewed and green. Integrate. |
| PR #70 / issue #65 (ScriptList VM control) | Integrate once its review passes. Then close the ScriptList line. |
| PR #66 / issue #64 (curve family) | Integrate only if review passes without another implementation round. Otherwise label it `paused` and leave the draft open. |
| Issue #68 (SHA-512/HMAC/HKDF/Ed25519) | Paused. No new `src/3rdparty` work beyond finishing #62/#66 as stated. |
| Issue #69 (tile areas, bitmap, tile lists) | Paused. Revisit as an ownership port when station/industry work needs it. |
| Issue #75 (GetPartialPixelZ full-domain fidelity) | Do it. Small. |
| Issue #76 (worktree/branch cleanup) | Do it. Luna low. |

## Phase 1: simulation comparison harness (#72), the critical path

Run the reference and the candidate headlessly on identical scenarios, take
periodic uncompressed snapshots using the existing `-d desync=3` hook, decode
the save chunks, and compare them field by field. The scenarios are the
regression saves, generated maps across seeds, and one committed
transport-network save with cargodist enabled. It must pass reference vs
reference and reference vs candidate, detect a deliberate rule change, and run
in CI. Phase 2 waits for #72. Phase 0 items may continue in parallel.

## Phase 2: first game-logic ports with Rust-owned state

These two can run in parallel once #72 lands. Both are self-contained, game-visible
and deterministic, with a clean ownership boundary.

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

## Phase 3 candidates (root selects after phase 2)

Choose by what phase 2 teaches about callback-heavy boundaries. The candidates
are roughly in order of increasing coupling:

- Town name generation (`townname.cpp`): pure, and can be compared exhaustively.
- Tree tile loop and tree placement (`tree_cmd.cpp`): tile-loop callbacks and
  `Random`.
- Effect and disaster vehicles (`effectvehicle.cpp`, `disaster_vehicle.cpp`):
  the first vehicle-type ownership work.
- Ship pathfinding (`pathfinder/water_regions.cpp`, YAPF ship).
- Economy: cargo payment, inflation, station rating, industry production.
- After those: town growth, road/rail vehicle controllers, YAPF rail/road.

Map/tile storage and pool ownership will need a design note before any of
these crosses into shared world state. Root writes that note (or delegates it
to Astra high) during phase 2.

## Out of scope for the near term

More `src/3rdparty` ports (monocypher, squirrel, others). GUI rendering and
layout. Platform or toolchain expansion. Deferred behavior improvements stay in
`deferred-improvement` issues.

## Progress metric

Every port PR reports four line counts: Rust added, C++ retired from the
candidate build (moved under `#ifndef WITH_RUST` or deleted), new C++ glue, and
new tooling. A healthy port retires more C++ than it adds as glue plus tooling.
Ports that fail this need a stated reason in the PR.
