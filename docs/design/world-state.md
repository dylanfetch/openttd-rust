# Incremental world-state ownership

Design: [#90](https://github.com/dylanfetch/openttd-rust/issues/90). Implementation
tasks require [roadmap](../roadmap.md) selection; this note moves no code.

## Constraints in the current game
- `src/map_func.h` exposes mutable tile-field references into C++-owned base and
  extended arrays; `Map::Allocate` replaces both (`src/map.cpp`). Tile indices are
  uint32, and type-dependent fields share bits. Storage cannot move independently
  of these accessors and their outstanding references.
- `src/core/pool_type.hpp` stores pointers indexed by typed IDs. Its companion
  `pool_func.hpp` chooses the first free slot, supports indexed load allocation,
  reuses IDs, optionally caches allocations, and invokes destruction hooks.
  Lifetimes, iteration and allocation order are behavior; an ID is not a
  generation-checked lifetime guarantee.
- `StateGameLoop` orders timers, tiles, vehicles and landscape; `RunTileLoop`
  uses an LFSR order plus special handling for tile zero. Parallelizing or sorting
  those updates would change immediate observations and shared random draws.
- Link graph jobs already copy their graph and settings (`linkgraphjob.h`). TGP's
  height map is temporary generator state. These are useful ownership boundaries;
  a general live-world snapshot would introduce different observation semantics.

## Options and decision
1. Mirror and reconcile each tick: two authorities, copying and delayed mutations
   make fidelity fragile.
2. Transfer map/pools together: requires changing tile references, polymorphic
   objects, save adapters and many callers.
3. Move component owners now; keep one shared-world store behind narrow operations
   until a selected port can transfer it. **Choose this.**

Rust owns component-private state and complete control flow. C++ initially owns
map arrays, live pools and shared services. Its facade supplies operations on tile
indices and typed pool IDs, with copied scalar records where useful; it retains
neither the migrated algorithm nor a second authoritative copy.
Original bodies remain compiled only without `WITH_RUST`. #73 owns the temporary
height map; #74 owns its computation snapshot, with scheduling/save/load/join in
C++. Neither requires moving the global map or station pool.

For a live component, invoke shared operations at the original observation and
mutation points. Preserve partial effects, traversal order, ID widths/sentinels,
integer narrowing and random draws. Do not add lifetime checks that reject source
behavior; design short access scopes that uphold the original preconditions.

## Boundary safety and cost
Use explicit scalar FFI and opaque Rust owners with one destruction path. A C++
object pointer may be an opaque context, never a Rust view of an STL/pool object.
No Rust reference into world storage survives an operation that can mutate,
reallocate or destroy it. Resolve IDs when needed under the source's lifetime
rules; reacquiring an ID alone does not make reuse safe. Document callback reentry
for each component and release affected owner borrows before it can reenter.
Shared services (`Random`, map and pool accessors) are `noexcept` C++ wrappers
that Rust calls directly, so ports keep the original control flow. Environmental
failures (allocation, debug/log I/O, developer-only defines such as `RANDOM_DEBUG`)
terminate inside the wrapper; they are not simulation behavior. Script VMs and
save/load errors use a return-to-C++ action protocol where an ordinary-play C++
exception can unwind. Reentry or mutation alone does not require that protocol:
end every affected Rust borrow before calling a typed service directly.
Neither C++ exceptions nor Rust panics unwind
across FFI; for ordinary-play failures preserve source failure order.

Per-field callbacks inside tile or vehicle loops may dominate runtime. Prefer
copied records for fields observed together and batches only where the original
has no intervening observable operation. Do not batch RNG or defer writes across
callbacks. A later bounded raw map view needs a proved exclusive access interval
and explicit layout/lifetime rules; it is not the default interface. Measure
crossings per update, copied bytes and elapsed time on the same harness scenarios
before enlarging the boundary. Keep source computation out of callback bodies.

## Map access decision (#108, accepted 2026-10-04)

Keep canonical map arrays in C++ and use direct `noexcept` operations for the
next selected tile-heavy owners. Prefer existing semantic operations and copied
records over one callback per bitfield; trees already copy ten words per read,
and water-region traversal calls track/follower services rather than individual
map fields. The measurements in [PR #116](https://github.com/dylanfetch/openttd-rust/pull/116)
and [PR #114](https://github.com/dylanfetch/openttd-rust/pull/114) establish crossing density,
not a map-access bottleneck. No shared-storage implementation is selected now.

For the 256x512 tree soak, generation uses 191,315 (temperate) / 321,489 (tropic)
map-service calls. Tree updates average 204--245 map calls per 512-tile batch
(including generation warm-up where applicable), around one third of tree-facing
FFI calls; copied tree tile-loop observation/settings records total 321--389 MB over a run.
Water plain soak uses 9,302--169,534 map-service calls and 18--88 region rebuilds:
281--1,995 track/follower calls per 16x16-region rebuild; aqueduct-neighbour
queries are counted separately from flood fill.
Plain tree reference/candidate process times were 0.214--0.315 / 0.264--0.365 s;
counted, counter-disabled and uninstrumented-parent times were indistinguishable
within the subprocess wait granularity (up to 50 ms). This does not establish
zero profiling overhead or performance equivalence. Elapsed time includes
startup, other ports, save I/O and profiling overhead;
these runs do not compare raw-array alternatives or establish a whole-game gain.

A future raw view must retain one canonical allocation, pin both C++ and Rust
size/alignment/offsets, use field-sized raw `ptr::read`/`write` with no Rust
references/slices over shared arrays, and refresh pointers at each component
entry. End its access scope before callbacks that may reenter, replace the map,
or run arbitrary code. Prove a serial access interval excluding concurrent map reset or mutation.
C++ field references may alias those pointers; absence of Rust references does
not permit concurrent conflicting access. Preserve original
observation/write order rather than copying a whole tile back after a callback.

Moving allocation to Rust adds reset, destruction, allocator and C++ object-
lifetime obligations without itself removing calls or aliases. Do not couple
that move to the next simulation port. `Map::Allocate` covers new-game, modern
and legacy load, and tests. Existing field-wise save adapters remain unchanged;
serialization reads live fields before the asynchronous compression thread starts.
Link-graph workers use private graph copies and map dimensions, not tile arrays;
retain the existing job join/reset ordering. Portable builds keep the C++ owner.

## Transferring shared storage later
Root selects a storage port when measured coupling warrants it. Move its canonical
owner, accessors, allocation/reset and destruction together;
retain C++ facades for remaining callers. Map fields keep their encodings. Pools
keep IDs, first-free selection, iteration and hook order; changing to a compacting
arena or generational IDs is not a compatible shortcut. Any retained C++ object
shell owns only the fields not yet migrated, never a synchronized second copy.

## Saves and evidence
Keep chunk IDs, versions, field widths and reference numbering unchanged.
`src/saveload/map_sl.cpp` serializes tile fields rather than struct bytes;
`saveload.cpp` maps current object references to index + 1 (zero is null), with
legacy load rules and pointer fixups. Keep those C++ adapters initially; when an
owner moves, adapt its reads/writes and load lifecycle without changing the format.
Rust-owned persistent state must be visible to saving before the call returns.

Extend the component module under `tools/simulation/` for save/load and
mutation/reuse paths. Run `python3 tools/migration.py simulate` and `--soak`/`--self`
as needed. Keep desync snapshots and plain runs: cache rebuilding can conceal
faults. The existing semantic decoder remains unchanged. Port divergences belong in `KNOWN_FAILURES` with an issue, never new
`MASKS`; #83's masked `round_trip_time` needs separate evidence if touched. Saves
do not expose every transient state: add a narrow direct check only for a named
unreachable gap. Vehicle ports need applicable #86/#104 evidence; the aircraft fixture is now
integrated in #132. Each controller still needs its own branch witnesses. Retain
existing tests.
