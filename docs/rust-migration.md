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

## Build and verification

The current verification setup targets native Linux and needs a C++20 compiler,
CMake, Ninja, Python 3.11 or newer, SDL2 development files, and the normal OpenTTD
libraries described in `COMPILING.md`. OpenGFX supplies free graphics for regression
games; commercial game assets are unnecessary. The verification driver requires
the pinned Rust toolchain and always configures the candidate with `OPTION_RUST=ON`.
Ordinary CMake builds default to `OPTION_RUST=OFF`, preserving the original portable
C++ path. Rust linkage supports native GNU/Linux (64-bit x86 or ARM) and native macOS arm64.
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
Windows CRT policy and Emscripten host/target builds remain separate tasks.

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
explicit `rust` input. Debug enables `OPTION_USE_ASSERTS`; release disables it.
Both run the four targeted Cargo checks, nonempty CTest inventories and scripted
regressions, then build and execute fresh native tools. The evidence artifact
retains JUnit, compiler/SDK/target metadata, compile commands, link scripts, native
libraries, Mach-O archive/symbol checks and fresh generated files. These checks
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
