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
terminate inside the wrapper; they are not simulation behavior. Script VMs,
save/load errors and callbacks that run arbitrary code or mutate borrowed state
use a return-to-C++ action protocol. Neither C++ exceptions nor Rust panics unwind
across FFI; for ordinary-play failures preserve source failure order.

Per-field callbacks inside tile or vehicle loops may dominate runtime. Prefer
copied records for fields observed together and batches only where the original
has no intervening observable operation. Do not batch RNG or defer writes across
callbacks. A later bounded raw map view needs a proved exclusive access interval
and explicit layout/lifetime rules; it is not the default interface. Measure
crossings per update, copied bytes and elapsed time on the same harness scenarios
before enlarging the boundary. Keep source computation out of callback bodies.

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

Extend `scenario_list()` in `tools/simulate.py` for component save/load and
mutation/reuse paths. Run `python3 tools/migration.py simulate` and `--soak`/`--self`
as needed. Keep desync snapshots and plain runs: cache rebuilding can conceal
faults. The existing semantic decoder remains unchanged. Port divergences belong in `KNOWN_FAILURES` with an issue, never new
`MASKS`; #83's masked `round_trip_time` needs separate evidence if touched. Saves
do not expose every transient state: add a narrow direct check only for a named
unreachable gap. Rail/ship/aircraft work first needs #86; retain existing tests.
