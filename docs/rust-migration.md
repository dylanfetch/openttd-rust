# OpenTTD-Rust migration

OpenTTD-Rust is an independent experiment in incremental C++ to Rust migration.
The original game's observable behavior is the specification, including quirks
retained for historical fidelity. Record possible improvements in fork GitHub
issues, deferred until near-full Rust reproduction. Component priorities come
from dependencies and existing test coverage.

## Repository and reference

The fork is https://github.com/dylanfetch/openttd-rust. `origin` points to the fork;
`upstream` points to https://github.com/OpenTTD/OpenTTD.git. The migration starts
from OpenTTD 15.3, commit `14ec60f248547d4d062a1160f0fc26d742319888`, pinned in
`migration/baseline.json`.

`rust-migration` is the integration branch. Create isolated task branches/worktrees
from it and target it with fork PRs; the pinned original revision remains the
behavioral reference throughout migration.

The verification driver creates `.local/reference/openttd` as a detached Git
worktree at that revision. It rejects a changed reference revision or dirty
reference source. Builds go into `build-reference` and `build-rust`; neither
changes reference source. Updating the baseline is a separate deliberate task.
Never change candidate behavior and expected results together to make checks pass.

The premature Rust integer-square-root implementation was removed before component
selection. The first selected replacement is `GetPartialPixelZ`, the scalar
landscape height kernel, implemented in `rust/openttd-kernels` behind its original
C++ interface. The shared crate also implements StringConsumer's integer and
remaining byte algorithms, UTF-8 codec/iteration, alternating-iterator traversal,
and StringBuilder's numeric byte encoders. Native generators share the Rust
archive; actual call-site coverage is described below. These replacements do not
complete their containing subsystems. Preserve the complete game,
including networking, saves, NewGRF mods, graphics, and shared random-number behavior.

### Paired Script Admin conversion

`ScriptAdminMakeJSON` and `ScriptEventAdminPort::GetObject` keep their public C++
interfaces. With Rust enabled, an Admin-specific owner selects types, walks both
conversion directions, propagates results, and schedules the original VM and JSON
operations. An opaque per-invocation handle returns scalar actions; C++ executes
each action after the Rust call returns. The adapter retains the bundled Squirrel
VM, nlohmann parser and JSON objects, script logger, and network send/framing.
Portable builds retain the original recursive bodies.

Outgoing traversal checks `depth == 25` before reading the VM type or changing
JSON, including calls with an explicit initial depth. It preserves live Squirrel
iteration, key stringification and byte copying before child conversion, duplicate
stringified-key overwrite order, `index - 1`, and `depth + 1` for the original
defined arithmetic domain. A failed child leaves completed root children intact:
the original two VM pops and iterator pop occur before failed temporary cleanup.
Arrays copy their completed temporary; tables move the copied key and value.
Incoming traversal accepts only an object root, rejects floats, and has no depth
25 limit. Ordinary failure restores the saved stack top before logging and pushing
null. Parsing malformed input still supplies the original root diagnostic.

No JSON tree or byte string crosses the ABI. Stable C++ heap frames retain actual
JSON temporaries, iterators, and copied keys; Rust retains only traversal state and
scalar frame slots. Raw Squirrel and nlohmann type encodings are pinned with C++
assertions. `SQInteger` and `SQRESULT` remain signed 64-bit, `SQBool` unsigned
64-bit, and outgoing depth signed 32-bit, including the original VM widths on
i686. The incoming unsigned JSON conversion still calls `get<int64_t>()`.
Arbitrary NUL and non-UTF-8 bytes retain the original byte-string operations.

Typed allocation errors, `Script_FatalError`, nlohmann exceptions, and reentrant
key metamethods occur entirely between Rust calls. RAII destroys control owners and
typed temporaries on C++ unwinding, without executing pending VM pops, rollback,
logging, or null pushes. Nested conversions own independent engines. The matching
Rust destroy function owns deallocation; no C++ exception crosses a live Rust
frame. Rust panic and allocation exhaustion abort. Additional control/frame
allocations change resource-exhaustion timing; this port does not claim identical
failure timing for all memory limits or arbitrary allocation positions.

The two unchanged `test_script_admin.cpp` cases retain their 15 outgoing and 27
incoming checks. `python3 tools/admin-conversion-comparison.py` adds only the
demonstrated coverage gaps, extracting unchanged pinned conversion bodies and
`ScriptAllocator`, and using the actual bundled VM and candidate entry points.
It compares both mixed original/candidate directions and directly inspects VM
values, stack state, partial JSON, diagnostic entry text/order, and owner cleanup
at O0 and O2. The scoped fixture includes depth 25/26, incoming depth 40, integer
extrema and unsigned overflow, byte strings/keys, completed siblings before
failure, colliding key stringification, a reentrant `_tostring`, typed string/key
copy allocation failures, and a real script allocation limit. The GNU link-wrap
cleanup checks currently run on native Linux; game-log storage, every allocation
failure, arbitrary recursion depths, network simulation, and complete script/game
equivalence remain outside this evidence. Native platform builds exercise the
unchanged game tests through their actual Rust/C++ ABI.

### Nested widget descriptor parser

With Rust enabled, `MakeNWidgets` and `MakeWindowNWidgetTree` use a widget-specific
pull parser. Rust owns the descriptor cursor, contiguous attribute traversal,
declared/produced container decisions, recursive tree control, end-marker handling,
complete-consumption policy, and first/root/body/shade composition. C++ retains
the unchanged single-part factory, attribute operations, real widget classes,
RTTI observations, `unique_ptr` owners, Add/GetWidgetOfType operations and generator
callbacks. Each typed operation executes after the Rust call returns; generators
may safely invoke an independent nested parser. Portable builds retain the original
recursive control bodies. The constexpr descriptor builders and public signatures
remain C++.

The scalar ABI carries unsigned 64-bit descriptor offsets and stable owner slots,
raw uint8 widget tags and capability observations. No C++ union, virtual object,
RTTI layout, function pointer or owner enters Rust. Native valid span/iterator
domains apply; no descriptor pointer survives completion. Enum widths, attribute
range markers, exact container tags and action field offsets are asserted in C++.
Push-button bits are not masked when classifying containers. A function-produced
subtree is complete and never acquires following descriptor nodes as children.

EOF inside a declared container remains accepted. Null function results retain
their original unconsumed cursor and end-marker assertion; release behavior is
preserved only for originally defined cases. Ordinary assertions remain separate
from the `WITH_ASSERT`-only trailing-parts exception. Window construction clears
the shade output on entry, recognizes actual horizontal subclasses, queries
caption then shade only when there is a remaining body, and writes the new shade
pointer before constructing that body. The inserted stacked wrapper retains
INVALID_WIDGET and its original vertical body container.

The initial `unique_ptr&&` remains a C++ reference until successful return.
Constructor, attribute, generator and Add exceptions retain already-committed
children in a caller-supplied container. Stable C++ slots hold unattached objects
and typed temporaries; RAII destroys them in reverse construction order without
executing pending parser actions. The shade output is not reset on an exception
and may be unusable after failure, as originally. Rust owns only control allocations
and its matching destroy function; exceptions never unwind through a Rust frame.
Panic and Rust allocation exhaustion abort. Additional control/slot allocations
change resource-exhaustion timing; identical failure timing for arbitrary allocation
positions is not claimed.

All four unchanged `test_window_desc.cpp` bodies remain primary evidence, including
constructing/destroying every registered WindowDesc through the production parser.
The source inventory has 156 static WindowDesc declarations and 34 NWidgetFunction
callsites; these counts do not establish platform registration or identical shape.
`python3 tools/widget-parser-comparison.py` records the actual native registered
count (163 in the recorded native build) and compares a small semantic/ownership
fixture against unchanged pinned
parser bodies with the same real construction primitives and MockEnvironment.
It inspects type/order/index, selected explicit attributes, shade membership,
partial caller ownership, callback/cleanup order and exception messages. Cases
cover nested background attributes, shade body/no-body, function-produced and
reentrant subtrees, permissive EOF, trailing end markers, throwing attributes and
generators, and shade output timing. Null-generator failures run separately under
custom assertions, standard assertions and release policy.

This production-object fixture currently runs on native Linux. It varies assertion
policy in widget.cpp/oracle/fixture; the other real production objects retain their
native build flags. Fatal probes compare termination category and callback entry,
not changed assertion expression/source-location text. Its background shapes use
the unchanged default vertical child. Rendering, layout, events, complete widget
ownership migration, arbitrary malformed descriptors, all allocation failures and
full GUI equivalence remain outside this evidence. Existing generator/comparison
checks remain required; the shared Rust archive imports no widget-library callbacks.

## Build and verification

The current verification setup targets native Linux and needs a C++20 compiler,
CMake, Ninja, Python 3.11 or newer, SDL2 development files, and the normal OpenTTD
libraries described in `COMPILING.md`. OpenGFX supplies free graphics for regression
games; commercial game assets are unnecessary. The verification driver requires
the pinned Rust toolchain and always configures the candidate with `OPTION_RUST=ON`.
Ordinary CMake builds default to `OPTION_RUST=OFF`, preserving the original portable
C++ path. Rust linkage supports native GNU/Linux (64-bit x86 or ARM) and native macOS arm64.
The scoped native Windows MSVC mode is described below.
Other platforms, cross compilation, and macOS universal/Intel configurations reject
an enabled Rust option; their portable fallback remains migration work.

On Ubuntu with administrator access:

```sh
sudo apt-get install build-essential cmake ninja-build python3 pkg-config \
  libsdl2-dev liblzma-dev libpng-dev libcurl4-openssl-dev libfreetype-dev \
  libfontconfig-dev libharfbuzz-dev libicu-dev liblzo2-dev openttd-opengfx
```

For this host, Ubuntu 26.04 on x86-64, `tools/bootstrap-local.py` offers setup without
administrator access:

```sh
python3 tools/bootstrap-local.py
python3 tools/migration.py verify
```

The bootstrap downloads Ubuntu packages into `.local/downloads`, extracts them into
`.local/deps`, and installs Rust into `.local/cargo` and `.local/rustup`. The bootstrap
changes no shell profiles or system packages. It relies on installed compiler/runtime
libraries and host-specific package names; it is not a portable installer.
`rust-toolchain.toml` pins Cargo compiler, formatting, and lint tools. CI installs
that same toolchain before verification. The crate has no external dependencies.

The driver's `verify` action requires all four Cargo checks below, builds both graphical executables,
runs both CTest suites (upstream unit
and scripted game tests), and requires the candidate to retain all reference test
names. Empty test inventories fail verification. CI runs this native verification
and uploads its evidence. The candidate Rust option is checked in the CMake cache
and recorded in the report; a C++ fallback cannot pass as a migrated candidate.

The inherited platform CI also remains in place. Windows CI selects Windows 2022
and Visual Studio 2022 because the pinned breakpad dependency uses
`stdext::checked_array_iterator`, removed by the newer Visual Studio runner.
It builds dependencies without the inherited GitHub Packages cache and retains
dependency/configuration failure logs. The dependency manifest and game behavior
are unchanged by this runner correction.

```sh
python3 tools/migration.py build --jobs 6
python3 tools/migration.py verify --jobs 6
```

Each invocation retains command logs, test reports, executable hashes, source revision,
and local changes under `.local/verification/<timestamp>/`. Passing establishes only
covered behavior. The driver does not itself compare every game state or prove full
game equivalence. PRs record exact checks and limits for each reviewed commit.

The fork executable is `build-rust/openttd-rust`; Cargo artifacts reside in the
ignored `build-rust/cargo` directory. CMake tracks the Rust sources, manifests,
lockfile, and toolchain file to rebuild the archive and affected executables.
The reference executable is
`build-reference/openttd`. Most in-game branding remains original. When using this
host's extracted dependencies, launch with their library path:

```sh
LD_LIBRARY_PATH="$PWD/.local/deps/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  ./build-rust/openttd-rust -X
```

`-X` avoids global game folders. Use separate development configuration and saves.

## Selecting and validating components

`GetPartialPixelZ` is tested through its unchanged C++ entry point by 32 upstream
cases, including fixed expected grids at all 256 tile positions, addition
properties, ordinary and steep slopes, and half-tile foundations. Small Rust tests
cover the direct flat/elevated gap and unusual half-tile/invalid-input handling.
These tests establish only this kernel's covered behavior, not whole-game equivalence.

The ABI takes two `int32_t` coordinates and one `uint8_t` slope, returning `uint32_t`.
There are no pointers, allocations, shared state, or ownership transfers. The C++
adapter asserts integer widths, tile dimensions, and slope/corner encodings. For
documented coordinates 0 through 15, arithmetic stays within 0 through 32 and
heights within 0 through 16. The Rust kernel preserves half-tile returns before
base-slope validation, clears all upper slope bits, and retains asymmetric rounding.
The reserved `UINT32_MAX` result invokes the existing C++ `NOT_REACHED` fatal handler
for an unsupported base slope or an out-of-contract coordinate. The coordinate
guard is new: the original may compute a height for some out-of-range inputs,
whereas Rust rejects them. Equivalence is limited to the original documented
coordinate range; callers inspected for this port use 0 through 15. Both Rust build profiles
abort on panic; the non-unwinding C ABI prevents unwinding into C++. Unsafe code is
denied except for the scoped export-symbol attribute; the implementation is safe Rust.

StringConsumer's integer parser and lexical skipper retain their C++ public
templates, optional/pair/string-view adapters, cursor updates, and formatted
logging. Rust owns the complete base selection, digit scanning, width-aware
conversion, overflow/clamping, and independent lexical-skip algorithms. The
inspected integer types are 8/16/32/64-bit signed and unsigned types; native `int`,
`uint`, `size_t`, and used enum-underlying aliases fall within those widths.
Unsigned 64-bit values do not pass through a signed intermediate. Negative automatic
hexadecimal parsing first converts/clamps to the matching unsigned width, then
negates/narrows and checks the resulting signed value, matching integer promotions
and the original modular conversion. Signed `0x-1` still parses length four but
lexically skips only two bytes; free ParseInteger rejects its remaining suffix.

The integer ABI borrows arbitrary bytes for one call, including NUL/non-UTF8 bytes.
For nonempty input, the caller supplies one readable allocation with length at most
`PTRDIFF_MAX`, without concurrent mutation; empty views may supply null. No pointers
are retained and no allocation ownership crosses the boundary. `repr(C)` metadata
returns zero-extended value bits, matched length, and diagnostic kind/byte spans.
C++ formats the original prefix-relative messages and four-byte previews before
advancing the cursor. Errors remain diagnostic in the game and fatal in generators.
Unsafe slice construction/export attributes have scoped exceptions to the unsafe
lint; panic still aborts and the C ABI never unwinds.

The shared Rust target initializes before the tools-only return and propagates
`WITH_RUST` to game, tests, strgen, and settingsgen, including inline parser users.
Game, tests, and strgen instantiate the integer parser. Settingsgen currently has
no integer-template call; its shared StringConsumer source compiles with
`WITH_RUST` and links the Rust archive, including lexical skipping. Fresh settings
output comparison verifies generator integration without claiming a parser call.
Each native build directory owns one archive. Imported `HOST_BINARY_DIR` tools
remain independent already-built executables; this does not add cross compilation.
To reproduce a fresh tools-only build with the local bootstrap toolchain:

```sh
export CARGO_HOME="$PWD/.local/cargo"
export RUSTUP_HOME="$PWD/.local/rustup"
export PATH="$CARGO_HOME/bin:$PATH"
cmake -S . -B .local/build-tools-rust -G Ninja -DOPTION_TOOLS_ONLY=ON -DOPTION_RUST=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .local/build-tools-rust --target tools --parallel 6
python3 tools/compare-integers.py
```

Run full verification before the comparison so the reference generators exist.
The comparison compiles the same small probe against unchanged pinned headers/source
and the candidate. It covers 8/16-bit extrema, modular negative-hex boundaries,
recursive/invalid prefixes, empty/NUL input, long overflow runs, both clamp settings,
peek/read/try/skip and free ParseInteger, and byte-encoded messages/cursors. It also
compares fatal logging adapters, real malformed strgen diagnostics, and fresh
settings/string headers plus English/French language output on unchanged reference
inputs. Evidence and source hashes live under `.local/integer-comparison/`; CI runs
the fresh Rust tools build and comparisons. The eleven unchanged upstream
StringConsumer cases remain the primary existing parser tests. In-place ownership,
other builder algorithms and C++ adapters remain migration work.

Alternating-iterator traversal also uses the shared Rust archive. Rust owns initial
position/selectors, logical advancement, side selection, end transitions, and
position comparison. C++ retains the typed iterators, range identity assertions,
dereferencing, and container lifetimes. Each increment recomputes the live range
distance, Rust requests a typed move, C++ applies that move and queries only the
requested live boundary, then Rust completes the next-side state. This preserves
the original operation order without caching size or requiring random access;
stable noncontiguous iterators remain supported after insertion.

The scalar ABI in `src/rust/alternating_ffi.h` maps size_t to Rust usize and explicit
uint8 selectors (0 before, 1 after). Nonnegative distances retain the original
size_t conversion; there is no additional range cap. State copies are independent;
no pointer, allocation, container element, or ownership crosses the ABI. Valid
range/iterator preconditions remain; panic aborts and never unwinds into C++.
Logical end skips movement/completion, preserving the last selected Base iterator,
while separately constructed end retains middle. They compare equal by position.

The fifteen original fixed sequences remain unchanged. Small additional public
interface tests cover empty/singleton, independent copies/postfix/prefix identity,
position ordering, distinct end Base values, stable-list insertion, and typed
operation counts. The original C++ algorithm remains under the explicit fallback;
other generic iterator/container and text-file owner code remains C++.

The remaining StringConsumer byte algorithms live in `consumer.rs`: exact unsigned
little-endian assembly; bounded read/skip lengths and shortfall/cursor decisions;
byte-prefix matching and conditional consumption; substring/character-set search
and membership; and complete separator result/consumption decisions. C++ retains
typed optional/default conversions, borrowed std::string_view construction,
diagnostic formatting/dispatch, cursor commit, and trivial accessors. It preserves
empty view pointers through the original source substring at the current offset.
No separate Rust call was added for trivial getters.

`src/rust/consumer_ffi.h` returns scalar repr(C) metadata by value. Read-only spans
may overlap and include NUL/non-UTF8 bytes; each nonempty span addresses initialized
bytes in one live allocation, length <=PTRDIFF_MAX, immutable for the call. Empty
spans allow null. Rust retains no pointer/slice, allocates no C++ storage, and calls
no C++ logger while borrowing. C++ logs a shortfall before applying the returned
position; fatal generator logging therefore leaves the cursor unchanged. Normal
game diagnostics keep the original text and consume the remaining bytes.

Nonempty search/set/separator assertions remain valid-input preconditions; release
behavior outside those contracts is not claimed equivalent (including original
empty-separator loops). Prefix matching still accepts empty patterns at end. npos
maps to SIZE_MAX/usize::MAX and means all remaining or not-found as appropriate.
Bounds use remaining-length subtraction before clamping, avoiding overflowing
position+requested arithmetic. Separator policies return and consume different
lengths for SKIP modes, repeat whole separators without overlapping matches, and
retain default KEEP for unknown values. Panic aborts; the ABI never unwinds.

The eleven original consumer cases remain unchanged. Four additional public cases
cover audited gaps: empty-prefix/zero-length borrowed offsets; partial-width TryRead
cursor preservation; all policies for multi-byte separators with nonzero offsets,
overlap and unknown-value default; and byte sets/read-only overlapping patterns.
The existing `tools/compare-integers.py` script invokes the probe's additive
`--consumer` mode:
60 bounded byte/offset/shortfall cases and six fatal timing checks compare against
unchanged pinned C++ source, retaining diagnostic bytes and cursor-before-log.
The inherited integer/generator comparisons and separate UTF8 checks remain in CI.
The integer comparison report records `consumer_bytes` separately.

The in-place pair and owning string/container code remain C++. In particular,
SQFile erases/resizes its owning buffer and explicitly reconstructs the consumer;
no Rust borrow survives that boundary. No allocator, in-place memory copy/rebinding,
escape-parser caller, or encoded-string transformation moved with this group.

Spiral tile traversal also uses the shared archive. Rust owns both square/hole
initializations, position initialization, per-direction movement, shell jumps,
outside-map skipping, end detection, and coordinate-only equality. C++ keeps the
public typed iterator/sequence facade, TileXY dereference conversion, copies and
postfix wrappers, and map storage/allocation. Map dimensions are supplied afresh
at each constructor and prefix increment; no map size is cached in Rust state.
Orthogonal and diagonal tile-area algorithms remain C++.

`src/rust/spiral_ffi.h` exposes five pointer-free by-value operations and a copyable
40-byte state (nine uint32_t fields counting the four extents, then a uint8_t
direction at byte 36; alignment 4). C++ and Rust assert that layout; C++ also
asserts original uint width and direction encodings. Directions are NE(-1,0),
SE(0,1), SW(1,0), NW(0,-1), with west shell jumps(+1,-1). Coordinate, extent,
position and radius calculations explicitly wrap at 32 bits, including temporary
outside-map coordinates. There is no new clamp or extent/diameter cap. Positive
diameter/radius and increment-before-end remain original preconditions. End is
exactly radius equality with a non-invalid direction; iterator equality uses x,y
only, including terminal coordinates. No pointer, tile storage, allocator,
callback, borrow or random state crosses this ABI; panic aborts without unwinding.

The five original ordered spiral tests remain unchanged (217 assertions). Four
focused public cases cover three complete clipped sequences on 128x64/64x128
maps, live map dimensions, copies/postfix/coordinate-only equality and sentinel
state, and UINT32_MAX hole-initialization wrapping. Fixtures were captured from
pinned unchanged C++ functions. The same original and new cases are compiled
against pinned reference algorithms, Rust facades and portable C++ fallbacks.
The huge inherited perimeter after a wrapped extent is not traversed by the
constructor test; no runtime limit is introduced. These checks do not establish
full world-generation or map-subsystem equivalence. Native generators do not use
spiral traversal and are not claimed as its runtime coverage.

Start with dependency and test inventories. Prefer bounded, heavily tested modules
whose unchanged upstream tests can exercise replacements through their existing
interfaces. Assess ownership, data representation, conversion/overflow behavior,
and linkage before choosing a component or coupled group.

Reuse existing checks and identify specific uncovered behavior. Add narrow reference
comparisons only where those gaps justify them, retaining the pinned original as the
independent oracle. There is no required initial rail harness or universal new
comparison test for each port. When simulation migration eventually needs state
comparisons, compare semantic state and retain the first divergence and its inputs;
screenshots and compressed-save byte equality are insufficient.

Integrate verified replacements incrementally while retaining the playable game and
reference checks. Report validation limits explicitly. Improvements to original game
behavior remain deferred issues rather than migration changes.

## StringBuilder numeric byte encoders

With `OPTION_RUST=ON`, Rust extracts the bytes for `PutUint8`, `PutUint16LE`,
`PutUint32LE`, and `PutUint64LE`, and formats integral `PutIntegerBase` values in
bases 2 through 36. The signed binary wrappers keep their original modulo-width
unsigned casts. Text uses lowercase digits, a leading minus for signed negatives,
no base prefix, and one digit for zero. The signed magnitude uses unsigned
negation, including `INT64_MIN`, without signed overflow.

The original formatting scratch buffer holds exactly 32 bytes, including the minus
sign. Values that need more bytes produce no `PutBuffer` call; exactly 32 bytes
produce one call. For example, `UINT32_MAX` in base 2 succeeds, whereas `INT32_MIN`
in base 2 produces no call. This behavior is retained; #12 records a possible later
capacity improvement. `PutUtf8` reuses the migrated codec and still makes one
zero-length sink call for an invalid codepoint, distinct from formatting failure.

The two functions in `src/rust/builder_ffi.h` accept scalars and return `repr(C)`
byte arrays by value. Rust neither borrows caller storage nor invokes a C++ sink.
The C++ adapter synchronously passes a span of the returned local array to its
original virtual `PutBuffer`. That span is valid during the call; sinks must not
retain it. Allocation, sink exceptions, string ownership, raw `Put`, InPlaceBuilder
copy/overlap handling, and cursor updates remain in C++. There are no Rust
allocations, ownership transfers, retained pointers, or additional unsafe blocks.
Overflow checks and abort-on-panic remain enabled. Invalid bases are outside the
same 2..36 precondition as the original `std::to_chars` API.

The adapter covers standard integral types other than bool, up to 64 bits, and
checks widths at compile time. A C++ overload adapter preserves
the original integer overload selection, including unscoped-enum promotions and
implicit user-defined conversions; bool remains rejected. Real text-format call sites use 32/64-bit values in
`strings.cpp`, save repair, and script text encoding. No wider compiler integer
extension is used there. Strgen uses binary byte/16-bit encoders and UTF-8, but has
no `PutIntegerBase` call. Settingsgen has no numeric builder call; compiling the
shared source and linking the archive does not establish runtime use there.
`OPTION_RUST=OFF` retains the original portable bodies, tracked under #3 until
platform support and the eventual facade removal are resolved.

The three unchanged StringBuilder cases, InPlaceReplacement, encoded-string tests,
and full verification exercise the existing interfaces. `tools/compare-integers.py`
also compares the existing narrow probe's `--builder` mode against unchanged pinned
C++ source and both Rust and portable candidate bodies. Its 1,624 formats cover all
bases, signed/unsigned widths, extrema, zero, and the 32/33-byte boundary. Six alias
cases, three unscoped-enum and three implicit-conversion cases, and an ordered counting sink preserve byte lengths, order, synchronous calls,
zero-length UTF-8 calls, and absent formatting calls. Parser and fatal diagnostic
comparisons remain intact, as do freshly generated string/settings headers,
English/French output, and malformed strgen diagnostics. These bounded checks do
not establish complete text formatting or whole-game equivalence.

## Encoded-string compatibility and parameter rewriting

With `OPTION_RUST=ON`, Rust owns `FixSCCEncoded`, `FixSCCEncodedNegative`,
`EncodedString::ReplaceParam`, and the shared `GetEncodedStringWithArgs`
serialization algorithm. The save-version dispatch and its ordering remain C++:
legacy encoding before version 350 (old markers before 169), negative repair before
353, then sanitation under the original control-code policy. General decoding,
rendering, ScriptText encoding, and sanitation remain separate migration work.

Legacy conversion remains permissive: old E028/E02A normalize only with fix_code;
markers are recognized even inside quotes; quotes toggle/disappear; quoted colons
remain bytes; numeric text is not validated. A valid nonmarker first character
leaves the original string untouched. Invalid first UTF-8 produces empty output,
and invalid UTF-8 after a recognized prefix truncates output. Negative repair only
accepts SCC_ENCODED, tries unsigned hex before signed hex, retains signed modulo
bits, and canonicalizes successful positive values as well. Failed signed reads
log the original diagnostic, default to zero, and perform the original lexical
skip, preserving suffix bytes for copying.

Replacement requires the internal marker and uint32 hexadecimal ID. Empty interior
records and unknown types become monostate. A final separator does not create a
final empty record. Out-of-range replacement returns empty after the original
parsing/diagnostic/assertion work. String parameters remain arbitrary bytes,
including NUL and interior record separators. Public StringParameter construction
still converts negative integers to uint64 before the descriptor boundary.

`src/rust/encoded_ffi.h` passes explicit tags (0 monostate, 1 uint64, 2 byte span),
never C++ string/vector/variant layouts. Static checks pin StringID width and the
RS/E000/E001/E002/E003 token contract. Nonempty spans are initialized readable bytes
in one live allocation, length <=PTRDIFF_MAX; descriptor arrays are aligned and
have total byte size <=PTRDIFF_MAX. Empty spans/counts allow null, and read-only
spans may overlap. All input borrows end before Rust returns; no C++ pointer is
stored in the result.

Rust returns an opaque Box owning output and diagnostic vectors. Getters provide
immutable views and by-value metadata without mutating/reallocating storage. C++
keeps a unique_ptr with the Rust destroy function as deleter, copies output into
its own std::string, and returns all intermediate allocations only to Rust, even
if C++ copying or logging throws. No view survives destruction. Each output append
checks addition and pointer-sized length; Vec checks capacity. Rust allocation
failure/panic abort, overflow checks stay enabled, and the ABI never unwinds. This
boundary does not claim equivalent resource-exhaustion timing.

Diagnostic offsets always identify the complete operation input. A record's
integer error span and following preview (at most four bytes) are bounded within
that original record before translating to full input offsets; negative repair's
preview uses the whole remaining input. C++ retains that input while formatting
and replaying ordered messages. A single Rust scan avoids duplicate diagnostics
from a sizing pass. It stops at the first enabled assertion, then C++ replays
preceding logs and uses the original assertion expressions before any output
commit. Numeric remainder assertions follow !NDEBUG || WITH_ASSERT, matching
stdafx's release assertion handler. The serializer's forbidden string-prefix
check follows the separate WITH_ASSERT guard. These distinct policies are retained.

The four original FixSCCEncoded/Negative and ReplaceParam positive/negative tests
remain unchanged and run through the production adapters. Bounded gaps use:

```sh
python3 tools/encoded-comparison.py
```

The script extracts the four complete reference functions verbatim from pristine
pinned files into ignored compilation fixtures, preserving source/function hashes.
It compiles the same public-API fixture against reference and candidate headers
and both Rust/portable candidate bodies. Malformed EncodedString cases use the
existing EndianBufferReader; output uses EndianBufferWriter. No raw-string public
constructor is added. Four NDEBUG/WITH_ASSERT combinations compare output bytes,
ordered diagnostic bytes, assertion expressions and status; only libc assertion
file/function locations are normalized. The corpus includes both old markers,
malformed UTF-8, permissive quotes, extrema and invalid numerics, suffixes,
interior/trailing empty records, NUL/RS string payloads, default/cleared strings,
and replacement bounds. Address/undefined/leak sanitizer runs cover the C++
fixture/adapters and intercepted allocations, including logger and output-copy
exceptions that must destroy the Rust owner. The Rust archive itself is not
sanitizer-instrumented. Evidence resides in `.local/encoded-comparison/` and is
retained by CI. No generator runtime coverage is claimed for these algorithms.
Portable bodies and ownership facades remain transitional under #3.

## Byte-string utility boundary

The issue #21 byte-string port moves case-insensitive compare/equal/prefix/suffix/
contains, lowercase conversion, uppercase hex encoding, sequential hex decoding,
and byte-set trim scanning into `byte_strings.rs`. C++ retains std::string owners,
public views, erases, and the installed standard library's exact equal-prefix
length comparison policy. The length adapter compares a valid suffix of the
longer original view against a default empty view; Rust scans/maps bytes and
returns that supplied scalar only when the shared prefix is equal. All operations
remain length-delimited, including embedded NUL.

Rust calls native C `toupper` with the original C++ char promotion (signedness is
an explicit scalar supplied by C++) and native `tolower` with unsigned-byte
promotion. This retains process-locale behavior and does not replace it with ASCII
or Unicode rules. Negative-char uppercase inputs other than EOF remain outside
portable C's specified domain; compatibility is only the observed native behavior
and call shape, not a claim of defined behavior. Locale must not change concurrently.
Issue #26 records the deferred signed-byte ctype improvement.

The ABI borrows initialized bytes in live allocations, lengths <= PTRDIFF_MAX;
empty spans allow null, read-only spans may overlap, no pointer is retained, and
ownership never transfers. Lowercase holds an exclusive mutable span and requires
an offset at most size. Hex encode uses disjoint input and caller-owned initialized
output. Hex decode deliberately forms no Rust slices or references: sequential raw
reads of both nibbles precede each raw write, allowing legal input/output overlap
and preserving earlier writes on later invalid pairs. Decode destinations may be
uninitialized except where their bytes also belong to initialized readable input.
Rejected lengths write
nothing. Rust trim returns offsets; C++ returns a default null-data string_view for
all-trimmed/empty inputs and otherwise takes the original substring. In-place trim
keeps the original newline-preserving whitespace set and erase order. Panic aborts
and the C ABI never unwinds. Equivalent resource-exhaustion timing is not claimed.

Only the portable fallback of natural contains routes through this helper. ICU,
Windows/macOS search/collation, sanitation/character validation, in-place replacement,
and StringIterator backends remain separate work; these utilities do not complete
string.cpp or the string subsystem. The twelve original utility tests are unchanged.

The bounded `python3 tools/byte-strings-comparison.py` probe compiles full unchanged
pinned string.cpp/core string-consumer sources against the public interfaces, and
compares Rust and portable C++ outputs. It uses the shared validated CMake archive
locator and native static-library flags. Its 3,699 records cover all single-byte
case/lowercase mappings and nibble positions, bounded/overlapping/NUL contains,
changed flags and lowercase offsets, uppercase hex, sentinel destinations, invalid
lengths and late invalid pairs, legal same/forward/backward decode overlap with
full backing-byte checks, custom trim sets and null/offset results. Real readable
zero-filled mmap storage above INT_MAX tests exact native length-result saturation
in both directions without fabricating invalid views. On this libstdc++ host the
results are INT_MAX and INT_MIN respectively. Both native char promotion and
-funsigned-char builds match the pristine reference.

Available host locales are C, C.utf8, and POSIX; their mappings are identical for
this corpus, so alternate locale mapping behavior remains untested. The report
records available locales and whether their results differ from C rather than
claiming non-C coverage from a locale name alone. ASan/UBSan instrument the C++
fixture/facades (including complete original dependencies needed by UBSan RTTI);
the release Rust archive's accesses remain uninstrumented. The bounded sanitizer
run checks output equality and intercepted allocator leaks, not Rust memory access
instrumentation or defined behavior of historical negative-char ctype calls.
Evidence and source hashes are retained under `.local/byte-strings-comparison/`;
CI runs the probe after native verification. Existing upstream tests remain the
primary covered behavior evidence, with this probe limited to the listed gaps.


## UTF-8 codec and byte positions

With `OPTION_RUST=ON`, `EncodeUtf8`, `DecodeUtf8`, `IsUtf8Part`, forward/backward
iterator stepping, and `GetIterAtByte` normalization call the Rust byte algorithms.
The C++ view owns only its borrowed `std::string_view` and iterator facade; pair
adapters, comparison assertions, postfix copying, and invalid-data `?` dereference
remain there. Native generators use the same archive and codec through issue #5's
shared target. `OPTION_RUST=OFF` retains the original portable algorithms.

This codec deliberately accepts surrogate values, rejects overlong and out-of-range
first sequences, ignores malformed trailing data after a valid first sequence, and
zeros all unused encoding bytes. View movement scans continuation runs rather than
advancing by decoded lengths. The unchanged `StringConsumer` read/skip methods still
advance one byte on decode failure; `TryReadUtf8` still leaves its position unchanged.

`src/rust/utf8_ffi.h` uses 32-bit codepoints, byte pointers and `size_t` lengths/offsets,
and returns `repr(C)` data by value. There are no allocations, output-pointer aliases,
ownership transfers or retained pointers. Nonempty spans must be readable initialized
bytes in one allocation, immutable for the call, and representable by `ptrdiff_t`;
empty/null views bypass raw-slice construction. Read-only overlapping spans are valid.
The C++ facade retains the original position assertions. Valid positions bound each
increment/decrement; codepoint-to-byte conversions are explicitly masked or bounded.
Both Rust profiles check overflow and abort on panic; the C ABI never unwinds into C++.

The three unchanged upstream UTF-8 view tests and existing consumer/builder tests
exercise the production adapters. A bounded comparison compiles the unchanged pinned
reference codec and consumer alongside the same fixture used for the candidate:

```sh
python3 tools/utf8-comparison.py
```

It records commands, reference source hashes and exact outputs under
`.local/utf8-comparison/`, comparing assertion-enabled and `NDEBUG` builds. The corpus
covers encoding boundaries, surrogates, all four buffer bytes, invalid and truncated
prefixes, invalid continuation positions, valid prefixes with invalid trailing bytes,
all byte classifications, every position in selected malformed runs, embedded NUL,
empty/null views, and consumer-versus-view movement. Under `NDEBUG` it also compares
the original `offset >= size` end branch, including `SIZE_MAX`. The standalone adapter
aborts if its bounded consumer corpus reaches error logging; game logging behavior is
outside this comparison. These checks establish this bounded byte behavior, not full
Unicode/text rendering or whole-game equivalence.

The C++ algorithm fallback remains transitional until Rust linkage and the existing
checks pass on maintained target platforms. Removing it and making Rust the normal
production path is a distinct remaining obligation under #3. As consumer groups move
to Rust, they should use the internal byte algorithms directly; after the last consumer
moves, remove the C++ view/pair facade and files. Neither this slice nor its reference
comparison completes the entire string subsystem.

## Rounded square root and runtime integer saturation

With `OPTION_RUST=ON`, `IntSqrt(uint32_t)` and runtime `ClampTo` / `SoftClamp`
call the shared Rust archive through `src/rust/math_ffi.h`. `IntSqrt` retains
nearest-integer rounding, including 65536 for UINT32_MAX. `DivideApprox` remains
C++: its potentially overflowing signed intermediates require separate work.

The public saturation templates retain their original C++ bodies for constant
evaluation, dispatched with `std::is_constant_evaluated()`. The StrongType and
OverflowSafeInt overloads retain their original unwrap-and-forward behavior.
Portable builds retain the original complete bodies. Accepted wider extension
sources/signed destinations and SoftClamp also retain the original runtime bodies.
These are explicit remaining
C++ implementations; this slice does not complete all math migration.

Source inventory finds ClampTo destinations uint8/uint16/uint32/int32, the int32
widget size_type and TimerGameTick::Ticks aliases, and history element types.
Sources include promoted 8/16-bit expressions, native int/uint, 32/64-bit integers,
size_t/ptrdiff_t, date/year StrongTypes, and OverflowSafeInt money results. All
four production SoftClamp calls in misc_gui.cpp use native int. No production
bool saturation, explicit narrow SoftClamp instantiation, or wider compiler
integer extension appears in these call sites. The standard integral public
contracts also include signed/unsigned char, wchar_t and char8/16/32_t aliases.
The Rust adapter explicitly checks eight-bit bytes, a 32-bit int promotion model
for SoftClamp, and routes standard widths through uint64_t to Rust. An accepted wider unsigned
destination uses Rust's unsigned64 saturation followed by C++ widening. Wider
ClampTo sources/signed destinations and wider SoftClamp types retain the original
C++ runtime body when accepted by the compiler/library. No new source/destination
width precondition is imposed. Non-builtin integer-like destinations accepted by
`numeric_limits` retain the original body and constructor/conversion selection.
Strict GCC/libstdc++ originally rejects signed
sources for unsigned128 destinations, signed128 destinations and 128-bit sources
through its traits; libc++ supports a broader extension domain. The bounded
extension comparison below checks each library's actual accepted domain rather
than assuming GCC defines every supported platform's public API.

The ABI is pointer-free and scalar-only: modulo-2^64 value bits, explicit widths
and signedness, and result bits reconstructed by C++20 integral conversion.
ClampTo accepts 1-bit bool descriptors alongside 8/16/32/64-bit integer widths.
Unsigned uint64 values never pass through signed int64; Rust uses a bounded i128
comparison domain and explicitly reconstructs modulo bits. The original template
still determines which bool-source instantiations are well-formed: for example,
its make_unsigned<bool> means bool-to-int8 remains ill-formed. Existing valid
bool conversions and destination truth values are preserved.

SoftClamp accepts the original non-bool standard integral types. Reversed signed
8/16-bit intervals first convert min to the matching unsigned type, then promote
both subtraction operands to int. Rust preserves this unusual behavior: e.g.
SoftClamp<int8_t>(0, -1, -3) returns 126. Reversed 32/64-bit signed intervals use
unsigned subtraction/division and modular result conversion; unsigned intervals
retain the original rounding toward min. Ordinary/equal intervals preserve the
<= and >= decisions. No allocation, ownership, random state, callback, pointer,
or exception crosses the ABI. Safe Rust uses explicitly bounded or wrapping
operations; both profiles abort on panic and the C ABI cannot unwind into C++.

The unchanged IntSqrtTest - Zero/FindSqRt, ClampTo, and SoftClamp cases remain
primary. Bounded coverage gaps are compared with:

```sh
python3 tools/math-comparison.py
```

The script verifies the pristine pinned oracle and compiles one public-API fixture
against its unchanged math source/header and both Rust and portable candidate
bodies, in assertion-enabled and NDEBUG modes. It compares exact ordered results
for every uint32 integer-root square and adjacent rounding transition, UINT32_MAX,
8/16/32/64-bit signedness/width extrema, bool and standard character/size aliases,
StrongType/OverflowSafeInt adapters, accepted unsigned 128-bit destinations
(including both result words and runtime routing), normal/equal/reversed SoftClamp intervals,
and all reversed signed 8-bit pairs. Static assertions retain constexpr evidence.
GNU linker wrappers count actual calls to each Rust symbol, including initial
literal calls at -O2; retained nm output supplies symbol evidence. Source hashes,
commands, outputs, and routing counts reside in `.local/math-comparison/` and CI
retains them. The probe is a native GNU/Linux comparison, not a new simulation
framework or a proof over all math inputs, platform ABIs, or whole-game behavior.

`python3 tools/math-extension-comparison.py` additionally compiles the same small
wide-template fixture against pinned, portable and Rust candidate headers on the
native compiler/library. The probe uses the configured C++ compiler and pointer
width, preserving the original macOS pointer guard; native macOS commands also
carry that build's arm64 architecture, SDK and deployment minimum for compilation
and linkage. It covers the GCC accepted unsigned128 widening path;
on libc++ it also checks accepted signed/unsigned128 sources, signed destinations,
saturation beyond uint64 limits, negative values and wide SoftClamp intervals.
Both result words and constexpr assertions are checked. A tiny integer-like
numeric_limits destination also checks the original constructor selection. Native macOS Rust CI runs
this comparison with `--build build` and retains it in the macOS evidence bundle.
Library-dependent extension coverage is reported by each fixture output; wider
C++ fallback behavior is explicitly remaining migration work.

## Generic history structural engine

The issue #29 history port moves descriptor-driven validity, rotation scheduling,
and query traversal into `history.rs`. HistoryRange constexpr construction/layout,
HistoryData typed ownership, every SumHistory specialization, graph fillers, and
GetAndResetAccumulatedAverage remain in C++. Production averaging keeps its exact
literal-0 int accumulators and nested reduction grouping; this port does not
complete history/economy statistics or change saves.

Rust owns an arbitrary-depth scalar frame stack. Its staged operation stream asks
C++ to describe immutable range objects by value, then selects child/parent order,
slots, move-backward/copy/reset/reduce scheduling, query scratch boundaries, and
validity. Opaque uintptr identity tokens round-trip to pointers only in C++; Rust
never dereferences them, mirrors the C++ layout, or borrows typed history storage.
The acyclic descriptor chain must remain live/immutable until engine destruction;
original valid-index/divisor/bit-shift preconditions apply, without a three-level
or built-in-range restriction. Unsigned index arithmetic wraps at native uint32,
and GB's uint32 extraction truncation remains even for uint64 validity masks.

C++ executes every typed operation after Rust returns. Query aggregates retain the
original full std::array scratch default construction at stable stack addresses,
then supply the live global month to Rust. Nested reductions/assignments and
scratch destruction finish before subsequent child scheduling and phase reads.
Generic constructors, copies/moves, reducers and destructors may therefore affect
phase or throw without crossing an extern-C stack. C++ RAII returns the opaque
engine to Rust exactly once during normal return or typed unwinding; scalar frame
reallocation moves no typed elements. Rust allocation/OOM/panic aborts; equivalent
resource-exhaustion timing is not claimed. No C++ allocator or layout ownership
crosses the boundary.

Update and rotation independently use explicit cur_month; queries use the live
TimerGameEconomy::month. Children update/rotate first even for saturated/skipped
parents, no-prerequisite higher rotations still shift, query validity ORs all
children while IsValidHistory chooses only the first, invalid children still
contribute data, and invalid query ages retain the original C++ fatal dispatch
rather than validity's false return. Result/history aliasing and partial typed
writes follow the original operation order.

The unchanged 288-month upstream test remains primary evidence. Its standalone
reference/Rust/fallback runs each pass 86 assertions. `python3 tools/history-comparison.py` compiles the pristine
history source and a common public-interface fixture independently for the
reference, Rust facade, and portable C++ facade at O0 and O2. Its 11,133 records
cover all twelve phases, prerequisite/unrelated/saturated masks, wide GB windows,
arbitrary valid descriptor chains, wrapped/out-of-range ages, and the independent
explicit rotation versus global query phases. It records typed constructor,
copy/move/assignment, reduction and destruction order, exception partial state,
result aliases, live phase mutations, and both ordered graph fillers with present
and absent histories. The three actual production reducers are extracted verbatim
from the pristine source and checked unchanged in the candidate: a nested-year
rounding fixture produces 0 while an intentionally flattened comparison produces
1, and Town fields retain the historical int accumulator conversions.

The fixture records IsValidHistory/GetHistory disagreement rather than treating
it as a bug. Fatal stubs compare original dispatch and C++ unwinding; they do not
compare game fatal text or source locations. ASan/UBSan instruments the C++ fixture
and adapters; the release Rust archive is uninstrumented, while allocator leak
checking covers opaque engine destruction, including typed exceptions. The shared
migration archive locator validates the configured native target and archive.
Full native checks and linked Industry/Town call-site evidence are recorded in the
issue/PR; this bounded corpus is not complete game/economy equivalence.

## Team process and engineering standards

`AGENTS.md` defines durable agent instructions. Root orchestrates, delegates heavily,
selects boundaries, checks returned evidence, and integrates changes. Target nine
agents total (root plus eight); respect any lower running-session limit. Explicitly
select model and effort for every spawn: `gpt-6.1-sol` high by default for most
implementation/analysis, `gpt-6-luna` low/medium for bounded mechanical tasks,
`gpt-6-astra` high for all delegated planning, and `gpt-6-astra` medium for all
independent review. Root uses xhigh effort; every Astra subagent must use strictly
lower effort than root. Lower Sol effort requires a clearly bounded task.
`.codex/config.toml` sets root to Astra xhigh and spawned agents to Sol high by
default, with a limit of eight spawned threads. Explicit spawn settings choose the
required role; a running host may impose a lower limit.

For substantive work, create a fork issue specifying scope, existing test evidence,
behavior gaps, and acceptance criteria. Assign an owner and an isolated branch/worktree.
The implementation agent opens a draft PR targeting `rust-migration` and linking
the issue, with reproducible checks and limits. A separate reviewer examines the
final commit and validation evidence;
resolve findings and review changed commits before root integrates.

Every agent-authored GitHub issue, PR, comment, and review report must identify the
agent, exact model, and reasoning effort, including root's artifacts. Use a footer
such as `Agent: /root/implementation | Model: gpt-6.1-sol | Reasoning effort: high`.
When agents share GitHub credentials, use an attributed review report identifying the
agent, exact model, reasoning effort, reviewed commit, findings, and disposition.
Do not self-approve or present shared credentials as independent accounts. GitHub
platform approval needs separate
reviewer credentials. Normal fork issues, PRs, and review reports are authorized;
upstream contact and submissions are outside this experiment.

Current automation enforces native build/tests, nonempty test inventories, reference
test-name preservation, and these Cargo checks for formatting, compiler checking,
Clippy with warnings denied, and tests:

```sh
cargo fmt --all -- --check
cargo check --workspace --all-targets --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo test --workspace --locked
```

Keep unsafe/FFI code narrow and document safety, ownership, lifetimes, and panic
behavior. Define overflow and integer conversions explicitly to preserve behavior.
Avoid unrelated blanket C++ warning changes. Server-side protection on `rust-migration`
requires all platform matrix, native comparison, commit, and annotation checks, a
branch current with its base, and resolved conversations. Force pushes and branch
deletion are disallowed, including for administrators. The approving-review count
is zero because agents share credentials; the separate, attributed independent
review report remains a process gate before root integrates. Repository controls
are enforced separately from these documents.

Preserve OpenTTD copyright notices, credits, and GPLv2. Agent-generated work is welcome
in this fork; upstream submission policies govern contributions to OpenTTD itself.

## Native macOS arm64 Rust linkage

CMake verifies the pinned `rustc -vV` host against the actual C++ platform,
architecture and 64-bit pointer width, then passes an explicit Cargo `--target`.
On macOS it requires exactly `CMAKE_OSX_ARCHITECTURES=arm64` and a deployment
minimum of 11.0 or newer, with the same resolved SDK and minimum supplied to Rust
through `SDKROOT` and `MACOSX_DEPLOYMENT_TARGET`. Rust's [Darwin target documentation](https://doc.rust-lang.org/rustc/platform-support/apple-darwin.html)
specifies the supported minimum and these environment inputs. Intel packaging,
Additional Windows CRT modes and Emscripten host/target builds remain separate tasks.

Archives live at `<build>/cargo/<validated-target>/release/libopenttd_kernels.a`.
`tools/migration.py` exposes `rust_configuration(build)` and `rust_archive(build)`;
all comparison consumers use this cache-validated lookup. Reconfigure existing
build directories when adopting this layout. Imported `HOST_BINARY_DIR` tools
remain previously built executables and do not consume the target archive.

The pinned compiler's `--print=native-static-libs` output supplies final link
flags instead of applying Linux libraries to Darwin. Configuration retains
`rust-toolchain.txt` and `rust-native-libs.log`; archive builds retain
`rust-build.log`, including the actual crate's native-library output. A
content-stable `rust-build-configuration.txt` dependency records the compiler,
target, SDK, minimum and relevant build flags. A changed configuration invalidates
only that target's release crate before Cargo rebuilds; unchanged reconfiguration
preserves the archive. Native CI checks minimum changes and restoration explicitly. Rust's
[static-library linkage documentation](https://doc.rust-lang.org/reference/linkage.html#linkstaticlib)
explains why final C++ links require these system dependencies. The crate retains
its release abort-on-panic profile and explicit overflow checks in both C++ modes.

The existing required macOS ARM jobs activate Rust through the reusable workflow's
explicit `rust` input. Debug enables `OPTION_USE_ASSERTS`; release disables it. The original CMake
ordering gives game/tests `WITH_ASSERT` in Debug, while generators retain ordinary
C++ assertions with neither `WITH_ASSERT` nor `NDEBUG`. Release defines `NDEBUG`
for every consumer. Evidence verifies these roles without changing their policies.
Both run the four targeted Cargo checks, nonempty CTest inventories and scripted
regressions, then build and execute fresh native tools. The evidence artifact
retains JUnit, compiler/SDK/target metadata, compile commands, link scripts, native
libraries, archive architecture, final Mach-O symbols and fresh generated files.
Whole-archive Apple `nm` inspection is excluded: its LLVM21 reader cannot parse
LLVM23 bitcode embedded by the pinned Rust compiler. Architecture, exact archive
linkage and final executable symbol checks remain mandatory; failures of final
binary `nm` inspection are not suppressed. These checks
validate native linkage and covered behavior; Linux results alone do not establish
Darwin support. Actual macOS CI evidence and independent review are required before
integration.

## Conservative compiler-cache trial

Compiler caching is opt-in. `python3 tools/migration.py tools --ccache` builds
native Rust generators; `python3 tools/migration.py verify --ccache` retains all
Cargo checks, original/candidate builds, test inventories, and tests. Ordinary
commands clear stale compiler launchers and restore the normal PCH policy.
`--ccache-bypass` requires `--ccache` and keeps identical no-PCH flags while
disabling artifact reuse. Both modes record effective settings, per-role counter
deltas, build settings, and total duration in the verification report.

The policy in `migration/ccache.conf` requires compiler-content validation,
preprocessor mode, empty sloppiness, unchanged paths, and local storage. Ambient
`CCACHE_*` policy overrides are removed. Original and candidate artifacts occupy
separate directories; PCH is disabled rather than enabling timestamp or PCH
sloppiness. Existing build dates remain observable and can change the revision
object and executable bytes across otherwise equivalent builds.

The Linux migration workflow can run the trial with the manual `use_ccache`
input. Automatic use requires the repository variable `MIGRATION_CCACHE=true`
after measured benefit and a successful protected-branch seed. Cache compatibility
includes OS, architecture, pinned original revision, policy/workflow/driver hashes,
compiler/tool versions, and installed dependency versions. Ccache validates source
and header contents for reuse. Pull requests restore only; successful protected
`rust-migration` push/manual runs save only compilation artifacts after every
existing comparison succeeds. Reports, expected results, build directories, and
executables are never restored.

Measure fresh ordinary-PCH, cold cache, warm cache, and cache-bypassed no-PCH runs
with the same source, compiler, jobs, and private build paths. Delete only the
trial's own build outputs between runs while retaining the warm cache. Record
actual hits and total elapsed time, retain all verification evidence, and explain
object or executable differences before adoption. A warm Ninja no-op is not a
cache benefit measurement.

The first Linux trial at `07c713445146ea38ad9ce82f2cdd8208aeeab1a0` used
five jobs, GCC 15.2.0, ccache 4.12.3, CMake 4.3.4, Ninja 1.13.2, and pinned Rust
1.99.0. Each row recreated the same private game/reference/tools build paths, ran
`tools`, `verify`, and all four focused comparison scripts. Only the warm cache
retained compilation entries. All rows passed four Cargo gates, 97 original and
110 candidate tests, all focused suites, and fresh generator comparisons.

| Mode | Native verification (seconds) | Complete validation (seconds) | Original PP hits/misses | Candidate PP hits/misses |
| --- | ---: | ---: | ---: | ---: |
| Ordinary PCH | 494.141 | 646.790 | disabled | disabled |
| Cold strict cache | 582.175 | 735.064 | 8 / 538 | 8 / 541 |
| Warm recreated outputs | 120.275 | 270.413 | 545 / 1 | 548 / 1 |
| Bypassed, same no-PCH flags | 566.154 | 720.521 | 0 / 0 | 0 / 0 |

Native hit/miss deltas exclude the separately recorded tools step; direct hits
were zero. Bypass effective configuration reported `disable=true`. Complete
duration includes tools, Cargo/native/tests, every focused suite, and snapshots.
Warm validation was 58.2% shorter than ordinary PCH and 62.5% shorter than the
equivalent bypass control; cold validation was 13.6% slower than ordinary PCH.
These are single local runs, without GitHub cache upload/download time. Keep
automatic adoption disabled until a protected-branch manual seed passes all gates.

Object differences were retained and investigated, without rewriting flags or
build dates. Cold/warm production C++ objects matched except two revision
objects whose differing bytes were solely their original build-date strings.
Against bypass, all 1,113 non-revision production objects had identical allocated
code/data and runtime relocations. GCC DWARF producer text records ccache's
reordered `-finput-charset=utf-8`; two Unix entry objects additionally contain
different internal LTO identifiers. An identical configure-only IPO command
compiled twice reproduced different GCC LTO identifiers. Configure-only probes
and fresh Cargo debug incremental paths are separate from cached game objects.
All four original/candidate game/test binaries have 27 file-backed allocated sections; only
the original date bytes in `.rodata` and the resulting build IDs changed between
cold and bypass. Code and mutable data agree. The trial therefore establishes
covered behavior and material local reuse benefit, not byte-identical binaries
or a guaranteed CI speedup.

Evidence remains in the implementation worktree's `.local/cache-trial/`: each
mode retains driver reports, all comparison logs, object SHA/size inventories,
original binaries, build IDs, and section/date/producer/LTO analysis. The ignored
`.local/cache-trial.py` reproduces the guarded private-path run sequence; the
comparison commands are `tools/compare-integers.py`, `tools/utf8-comparison.py`,
`tools/encoded-comparison.py`, and `tools/byte-strings-comparison.py`. No expected
results or binaries from a prior run supplied validation inputs.

## Native Windows MSVC Rust linkage

Issue #33 adds one native Windows mode: VS 2022 MSVC, x86 or x64, single-config
Ninja, `RelWithDebInfo`, `OPTION_USE_ASSERTS=ON`, and the static release CRT.
The C++ compiler's architecture macros and pointer width select
`i686-pc-windows-msvc` or `x86_64-pc-windows-msvc`; the pinned Rust host is recorded
separately. An x64 Rust host therefore does not choose the game architecture.
The protected architecture jobs install the exact target standard library and run
the four Cargo gates with an explicit target for target-dependent commands.

`WindowsRust.cmake` sets `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` before creating
any game, test or generator target, including the tools-only path. CMP0091 is NEW
before the first `project()` call, as required by the
[CMake runtime property](https://cmake.org/cmake/help/latest/prop_tgt/MSVC_RUNTIME_LIBRARY.html).
Both the direct rustc native-library query and Cargo use
`-C target-feature=+crt-static`. The resulting archive is
`<build>/cargo/<validated-target>/release/openttd_kernels.lib`. The shared cache
locator validates target, pointer width, CRT, flags and the target-specific path.
The configuration stamp includes these effective settings and captured profile
inputs; a code-generation setting change and restoration must rebuild the actual
archive. Native library ordering and quoted arguments are retained. Rust's
[CRT documentation](https://doc.rust-lang.org/reference/linkage.html#static-and-dynamic-c-runtimes)
explains why the feature must apply to the selected target and its native query.

Debug/debug CRT, dynamic CRT, other build types, assertions disabled, non-MSVC,
multi-config/non-Ninja, ARM/UWP/MinGW, cross-OS and external `HOST_BINARY_DIR`
configurations remain unsupported for Windows Rust. Conflicting C++ runtime flags,
Rust target overrides or mismatch suppression fail configuration. Missing target
std also fails; C++ fallback is never selected implicitly. Future modes remain
tracked under issue #3. `OPTION_RUST=OFF` retains its existing configuration.

The unchanged CMake ordering gives Windows RelWithDebInfo game/tests both
`NDEBUG` and `WITH_ASSERT`, while generators have `NDEBUG` alone. Evidence checks
actual role-specific compile commands rather than adding generator definitions.
The x86 RelWithDebInfo build exposed six existing narrowing assignments in station
expansion, snow-line calculation, map-height selection and old-save station loading.
Explicit casts to their existing unsigned destinations retain the original modulo
conversion after the complete expression, without changing arithmetic or ordering.
The ABI fixture compares all current C++ struct sizes, alignments and field offsets
against Rust, and executes high-bit scalars, by-value returns, pointer-sized
sentinels, null/empty inputs and Rust allocation/view/destroy paths. Its deliberately
unaligned descriptor array closes the documented
[MSVC i686 alignment gap](https://doc.rust-lang.org/rustc/platform-support.html):
Rust copies foreign encoded descriptors with raw `read_unaligned` before taking
any references, without changing their declared layout. All exports retain
`extern "C"`; the [MSVC target ABI](https://doc.rust-lang.org/rustc/platform-support/windows-msvc.html)
uses cdecl on i686. Borrowed byte spans retain `isize::MAX` limits and release
panics abort. Rust owns and frees its allocations.

The accepted history engine is included in this audit: descriptor/step layout and
by-value calls round-trip live C++ HistoryRange identities, high-bit masks and all
staged operation modes. Only opaque Rust-allocated engine handles enter Rust by
pointer; typed C++ storage and exception execution remain outside Rust.

`windows-rust-evidence.py` checks PE machine headers, retained Ninja response files,
exact archive linkage, MSVC maps and static CRT imports. It executes a shared
per-thread non-C locale check through native C++ and Rust lowercase calls; signed
negative compare arguments remain outside the defined C library domain (issue #26).
Fresh tools-only builds execute current-source string and settings generators after
deleting previous outputs. Windows artifacts retain compiler/host/target metadata,
CRT and assertion commands, maps, ABI execution, refusal/freshness reports, nonempty
test inventories, JUnit and fresh generated files. Parser unit checks and Linux
ABI/Cargo checks are preliminary evidence; actual x86/x64 Windows artifacts and
existing macOS checks are required before claiming platform support or integration.

## Authentication and streaming owners

The X25519 session and encryption-owner migration (#35) uses a versioned,
primitive-only host function table. The bundled Monocypher algorithms remain
unchanged. Rust owns stable key/session allocations and vendor-context storage;
C++ supplies each vendor context's actual size/alignment and starts its trivial
object lifetime before initialization or copying. This avoids a Rust mirror of
platform-dependent vendor structs and adds no vendor-symbol dependency to other
Rust archive consumers. Packet, RNG, policy and logging calls happen after each
Rust call returns, so application exceptions cannot unwind through Rust.

Secret fields are initialized directly in their final allocation. Deep copies
copy heap to heap; assignment overwrites existing fixed storage as the original
C++ member assignment did. Rvalue facade copies preserve the original source
state. Destruction invokes bundled volatile wiping before Rust deallocation,
including the original reverse session-field order. Hash finalization performs
its original context wipe. Temporary shared secrets have independently wiped
stable storage. Opaque streaming contexts retain the bundled successful-rekey
behavior; their counter does not advance, and failed authentication leaves the
context and output unchanged. Allocation failure/panics abort as for the other
Rust kernels. Register spills, caller-held input copies and the vendor
algorithms' internal temporaries remain outside this owner-storage guarantee;
this does not claim complete deallocation security.

Borrowed fixed-width views retain their address across completed mutation and
assignment, until owner destruction. Callers serialize access and never read a
view during a mutating call. Exchange extra payload may alias existing derived
key bytes because all input hashing precedes key replacement. Callers provide
initialized readable/writable buffers and byte lengths
no greater than `PTRDIFF_MAX`, and keep MAC/message regions disjoint. Encryption
is in place through raw primitive pointers; Rust never creates overlapping
shared and mutable message slices. Empty variable spans may use null pointers.
The existing short nonempty `Packet::Recv_bytes` path is undefined because its
callback takes an unchecked subspan; deferred fork issue #41 records this
separately. Zero-length, full-length and trailing-data paths are defined and
remain within #35's reproduction contract.

`python3 tools/auth-comparison.py` compiles the actual pinned and candidate
session/Packet/vendor sources into separate endpoints. Both mixed directions,
Rust/Rust and portable C++ are compared with original/original transcripts:
request/response/enable bytes, derived halves, results/cursor/diagnostic bytes,
prescribed RNG traces, failure/retry state and independent stream keys/counters.
The corpus includes empty and block-boundary messages, wrong/empty/NUL payload,
low-order peers, exact-size errors, tampering, copy/assignment/self/rvalue copies,
self-key-payload aliasing, span stability, and cleanup after first/second RNG,
packet/output allocation and logger exceptions. Primitive observers invoke the
real bundled algorithms, use fixed-capacity nonthrowing records, and check
outer wiping order and hash-final zero bytes. Fixture observations retain only
known test values. They do not establish constant-time execution or whole-system
secret erasure. The C++ fixture/vendor/Packet/adapter sanitizer run also observes
Rust allocator leaks, but does not instrument Rust memory accesses.

Clean core `f308ac2cda` passed all four Cargo gates and full 97-reference /
110-candidate tests, including the five unchanged network cases. Fresh native
Rust/portable tools and all inherited comparison suites passed. The focused
corpus passed 1,800 mixed endpoint records plus both sanitizer directions before
its final evidence commit; exact final-head evidence and platform review remain
required. Original source hashes, commands and transcripts are retained under
`.local/auth-comparison/`; `.local/auth-linkage.json` records actual Rust calls in
both game/test binaries. The migration workflow runs the authentication
comparison unconditionally and retains its evidence.

## ScriptList storage and iteration

The ScriptList owner migration (#44) moves both deterministic item/value indexes,
all four sort modes, live pending-cursor/end state, mutation accounting, filters
and list algebra into Rust. C++ retains script identity/bindings, pool enumeration,
VM/error/operation charging and save/load adapters. Valuation and serialization
read copied ascending-item scalars; every Rust borrow ends before a VM operation
can reenter the list. The mutation token is validated after the original callback
return-type check; SetValue occurs before the original pop and five-operation
charge. Earlier commits and callback side effects remain on failure.

The scalar/pointer ABI avoids aggregate-return layout differences on 32-bit
hosts; items/values are explicitly signed 64-bit and modification tokens signed
32-bit. Two-list operations recognize self-aliasing before creating references.
Rust allocation/panics abort. Defined-input reproduction excludes original signed
modification-counter overflow, nonempty rank decrement overflow and overflowing
Count()-count, and callbacks that leave original iterators invalid while evading
its modification check. Resource-exhaustion exception behavior is not promised
identical. Clone content uses the original target sort/initialization flow;
saving traverses item order without resetting public iteration. The original
mixed-type load validation remains unchanged. The unchanged full-game regressions
`regression_regression` and `regression_stationlist` pass alongside all reference
tests. `python3 tools/script-list-comparison.py` compares only the identified gaps
against the actual pinned C++ implementation, with separate original, Rust-enabled
and portable binaries at O0/O2. It covers active/ended cursor swaps and insertions,
self operations, pending removals/value changes, no-op mutations, empty-list
union, strict/reversed/equal filters and zero/negative ranks. The actual bundled
Squirrel VM and allocator exercise callback failure/error precedence, partial
commits, operation charges and command-scope restoration. Valid List and TileList
save/load representations and independent clones are compared without resetting
the source cursor. The fixture substitutes only a command-permission bool for the
full game instance and extracts unchanged TileList persistence bodies without
simulating world population. This evidence does not establish arbitrary VM,
savegame or allocation-failure equivalence. CI retains these comparisons along
with the existing checks.

## ChaCha20, Poly1305 and AEAD primitives

The bundled Monocypher 4.0.2 ChaCha20/Poly1305/AEAD family (#48) keeps its public
C interfaces and caller-owned context types. Rust owns cipher rounds, MAC
arithmetic, incremental buffering, authentication padding and composition. C++
starts actual trivial context lifetimes for one-shot calls and passes compiler
size/alignment/field-offset descriptors. Raw field access reads only initialized
Poly1305 fields/chunk bytes, preserves unwritten chunk/padding bytes at init, and
wipes the actual complete caller context at finalization. Counter access is raw
and unaligned-capable, avoiding an i686 Rust/C++ uint64 alignment assumption.

The original nonthrowing wipe and constant-time verify16 leaves are borrowed
through a two-function explicit-cdecl table; Rust has no direct vendor imports or
global callback registration. Every callback returns synchronously. Cipher input
and output are disjoint or exactly in-place; key/nonce loads precede output,
including unchanged Elligator key generation's overlapping key. Unsigned
arithmetic wraps as before, failed reads preserve output/context, successful
stream operations rekey without incrementing the context counter. Fixed-size
secret temporaries use stable local storage and the original wipe points; this
does not promise erasure of every compiler copy/spill or cryptographic
certification. Other Monocypher algorithms and the exact portable family remain
in C++. The clean core passes the four Cargo gates and full native reference/candidate
verification, including the five unchanged network cases and both unchanged
scripted regressions. The unchanged authentication corpus first passes all 1,800
transcript records, including both mixed endpoint directions. Its existing
`python3 tools/auth-comparison.py` tool now also invokes the bounded `--primitives`
mode against actual pinned vendor functions, matching 484 direct records through
Rust and portable C++: variant outputs/carries, null keystream, disjoint/in-place
text, key/nonce and Elligator overlap, split Poly1305 updates with prefilled
untouched bytes and final wipe, unaligned associated-data padding, three
initializers, multi-chunk rekey, and failed output/context preservation with retry.
The existing C++ ASan/UBSan runs exercise both modes; Rust accesses are not
instrumented. Passing does not certify cryptography or arbitrary overlap/inputs.

The native ABI executable adds all 15 actual facade/FFI calls and caller-context
checks while retaining every existing layout/math check. Metadata IDs 19..21
cover the two-leaf table and two field-layout descriptors; 18 covers the
station cargo owner. The combined audit has all 22 IDs from 0 through 21. Only the
ABI executable adds a vendor object for its primitive calls; fresh strgen/settingsgen continue linking
the shared Rust archive with no vendor object/import dependency. Actual final
Linux/macOS/Windows x86/x64 CI and independent final-head review remain mandatory.

## Station cargo-list queries

The station cargo-list port (#51) reuses the Rust ScriptList owner. Rust owns all
four selector/filter rules, pending run keys and unsigned 32-bit totals, positive
flush decisions, existing-item add-versus-set merges, per-origin cumulative-share
decoding, and selection of waiting/all versus equal_range and planned/all versus
find query plans. C++ retains actual station/cargo validation, pool/GoodsEntry
access, HasData checks, packet/flow iterators and scalar extraction. All eight
specialized constructors and the generic mode/selector temporary/SwapList facades
use the same reducer. Non-cargo station lists, routing, packet/flow ownership and
other station APIs remain C++. Portable builds retain the original cargo bodies.

A caller-owned 16-byte scalar collector uses unsigned 32-bit amount/previous,
unsigned 16-bit station IDs (including 0xFFFF), and byte selector/finalized fields.
Explicit width/offset/alignment assertions and ABI layout entry 18 check the C/Rust
boundary. Windows x86 uses the established explicit cdecl convention. No collector
allocation is added; Rust borrows the existing opaque destination and disjoint
scalar state only during feed/finalize calls. No world pointer, STL/VM layout,
iterator, callback or C++ exception enters or survives a Rust call. A C++ RAII
finalizer flushes the last positive run on normal exit, early return or unwinding;
the destination outlives the collector. Finalization is idempotent, with no
transactional rollback or extra list insertion on empty/zero runs.

Filtering precedes pending-key changes. Selected equal-key totals and cumulative
share differences wrap modulo 2^32. Each origin resets previous to zero; every
visited share advances it, including filtered and restricted entries from GetShares.
Repeated noncontiguous keys merge using the accepted list add/set methods, retaining
modification tokens, live pending cursors and final value/tie ordering. Original
signed result/modification-counter overflow remains outside the defined domain.
List allocation/panic limits are unchanged: Rust aborts on panic/allocation failure;
recoverable resource-exhaustion equivalence is not claimed.

The unchanged stationlist script already exercises 24 cargo-list constructions
and checks 41 cargo item/value output rows. Both unchanged scripted suites and the
full upstream unit inventory remain primary evidence. The existing
`python3 tools/script-list-comparison.py` machinery adds only bounded cargo gaps
at O0/O2, comparing exact pinned original Update/SetValue/destructor and planned
all/find loop bodies, portable candidate and Rust feeds. Actual FlowStat maps
exercise origin reset, filtered intermediate and restricted shares. Traces include
ordered items, mutation tokens and live cursor/end behavior, unsigned wrapping,
zero/invalid-sentinel keys, noncontiguous repeats, empty/absent origins, reentry and
a C++ exception between completed feeds. The original private mutation count is
read through a narrowly scoped explicit-instantiation member pointer without
changing production headers or behavior.

The direct reducer fixture bypasses world lookup. It checks exact preservation of
the typed station-before-cargo validation and null-goods/HasData guard bodies/order;
failed station/cargo/company policy and missing-data supplemental probes are not
executed. Existing saved-world regressions cover their exercised real query paths,
not every invalid world state. This evidence does not establish cargo routing,
station-storage ownership, arbitrary allocation failures or full game equivalence.

### Packet framing, binary serialization and transfer state

The Rust Packet kernel owns the native-width limit, persistently narrowed uint16
cursor, binary encoding/decoding, length-prefix sequencing, framing offsets and
transfer planning/commit decisions. C++ retains vector/string storage, each
original allocation operation, direct spans, socket policy, assertions and
external encryption/transfer callbacks. Packet copies/assignments copy scalar
state and the existing C++ vector. No Rust borrow survives a C++ allocation or
callback, and no C++ exception crosses Rust; kernel panics abort.

The facade preserves the original per-byte append order and advances each
received result byte before its potentially throwing C++ push. Parsing commits
position two only after resize succeeds. Encryption header writes occur before
handler queries; send reset occurs after normal encryption return and before
shrink. Normal false decryption still skips its MAC; throwing decryption does
not. Transfers commit positive results to the live post-callback cursor, using
uint16 narrowing; native unsigned arithmetic wraps as in the original. The
historical GetPacketType send-handler offset remains intact. C++ fallback bodies
remain available when Rust is disabled.

The unchanged five network/authentication unit cases and existing mixed-endpoint
1,800-record authentication comparison remain primary evidence, together with
both scripted regressions and the full test inventory. Run
`python3 tools/packet-comparison.py` for the independent bounded companion at O0
and O2: actual pinned, portable candidate and Rust candidate Packet sources are
compared for binary/buffer bytes, suffix identity, copy independence, TCP/UDP
framing, partial/zero/negative/throwing transfers, reentrant live cursor changes,
close policy, controlled encryption callbacks and selected C++ allocation
failures. It also checks the original uint16 per-byte wrap with larger native
buffers and prefix narrowing. Commands, source hashes and full observations are
retained in `.local/packet-comparison/`. ABI IDs 23 and 24 describe scalar Packet
state/framing outputs; native ABI smoke calls exercise their actual exports.

Send_string and Recv_string collection/sanitation remain C++, as does the exact
Recv_bytes callback. The nonempty-short Recv_bytes domain remains undefined in
the pinned original and is excluded per issue #41; zero-source and sufficiently
large-source cases retain original behavior. These checks do not establish real
socket delivery, all allocator failure modes, full protocol equivalence or full
Packet ownership migration. No upstream test or expected output changes.
