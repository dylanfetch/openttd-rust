# OpenTTD-Rust migration

OpenTTD-Rust is an independent experiment in incremental C++ to Rust migration.
The original game's observable behavior is the specification, including quirks
retained for historical fidelity. Record possible improvements in fork GitHub
issues, deferred until near-full Rust reproduction. Game-simulation code takes
priority, with Rust owning component state; `docs/roadmap.md` sets current work
and `AGENTS.md` states the selection rules.

## Repository and reference

The fork is https://github.com/dylanfetch/openttd-rust. `origin` points to the fork;
`upstream` points to https://github.com/OpenTTD/OpenTTD.git. The migration starts
from OpenTTD 15.3, commit `14ec60f248547d4d062a1160f0fc26d742319888`, pinned in
`migration/baseline.json`.

`rust-migration` is the integration branch. Create isolated task branches/worktrees
from it and target it with fork PRs; the pinned original revision remains the
behavioral reference throughout migration.

The verification driver creates `.local/reference/openttd` in the main checkout
as a detached Git worktree at that revision, shared by every worktree of the
clone. It rejects a changed reference revision or dirty reference source. The
shared reference builds into the main checkout's `.local/build-reference`; each
worktree builds its candidate into its own `build-rust`. Neither changes
reference source. Updating the baseline is a separate deliberate task.
Never change candidate behavior and expected results together to make checks pass.

Migrated code lives in the shared `rust/openttd-kernels` crate, linked into the
game, the unit tests and the native generators (strgen, settingsgen). Each port
keeps its original C++ interface; see "Ported components" below. None of these
ports completes its containing subsystem. Preserve the complete game, including
networking, saves, NewGRF mods, graphics, and shared random-number behavior.

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
an enabled Rust option; their portable fallback remains migration work (#3).

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

The driver's `verify` action requires all four Cargo checks below, builds both
graphical executables, runs both CTest suites (upstream unit and scripted game
tests), and requires the candidate to retain all reference test names. Empty test
inventories fail verification. The candidate Rust option is checked in the CMake
cache and recorded in the report; a C++ fallback cannot pass as a migrated candidate.
The `tools` action builds the native Rust generators alone into
`.local/build-tools-rust` (`OPTION_TOOLS_ONLY=ON`, `OPTION_RUST=ON`), using the
bootstrapped toolchain when present. `tools/run-comparisons.py` then runs every
reference comparison tool in parallel; CI runs all three steps.

```sh
python3 tools/migration.py build --jobs 6
python3 tools/migration.py verify --jobs 6
python3 tools/migration.py tools
python3 tools/run-comparisons.py
```

Each driver invocation retains command logs, test reports, executable hashes,
source revision, and local changes under `.local/verification/<timestamp>/`.
Passing establishes only covered behavior. The driver does not itself compare
every game state or prove full game equivalence.

The inherited platform CI also remains in place. Windows CI uses Windows 2022 and
Visual Studio 2022 because the pinned breakpad dependency uses
`stdext::checked_array_iterator`, removed by newer Visual Studio. It builds
dependencies without the inherited GitHub Packages cache and retains
dependency/configuration failure logs; the dependency manifest and game behavior
are unchanged.

The fork executable is `build-rust/openttd-rust`; Cargo artifacts reside in the
ignored `build-rust/cargo` directory. CMake tracks the Rust sources, manifests,
lockfile, and toolchain file to rebuild the archive and affected executables.
The reference executable is `.local/build-reference/openttd` in the main
checkout. Most in-game branding remains original. When using this host's
extracted dependencies, launch with their library path:

```sh
LD_LIBRARY_PATH="$PWD/.local/deps/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  ./build-rust/openttd-rust -X
```

`-X` avoids global game folders. Use separate development configuration and saves.

## Compiler cache, shared reference and CI time

Every worktree of a clone shares the main checkout's pinned reference checkout
(`.local/reference/openttd`), reference build (`.local/build-reference`), bootstrapped
toolchain/dependencies, and per-role ccache stores (`.local/compiler-cache/`).
A lock serializes reference configure/build/test across concurrent runs, so the
original is compiled once per clone rather than once per worktree. New worktrees
need no symlinks. The driver skips an explicit CMake configure when the existing
cache already holds every requested `-D` value; Ninja still reconfigures on any
CMake input change. Configure-time environment changes (for example a new
dependency prefix) need a fresh build directory or one manual configure.

The driver uses ccache whenever it is installed (`tools/bootstrap-local.py`
installs it locally). `migration/ccache.conf` keeps compiler-content checks and
empty sloppiness, and enables direct mode. The driver sets `base_dir` to each
role's root, so paths are relative and worktrees share entries. PCH is disabled
under ccache; `--no-ccache` gives an ordinary PCH build, and `--ccache-bypass`
keeps the no-PCH flags without reuse for measurement. The first local trial
(`07c7134451`) found warm-cache validation 58% shorter than ordinary PCH builds,
with no code or data differences in the objects.

CI restores both cache roles keyed only by OS, architecture, baseline, compiler
versions and `ccache.conf`. ccache itself hashes compiler content, arguments and
every included file. Workflow and driver edits do not change the key (#79).
Successful `rust-migration` pushes save the cache, and PRs restore it. Windows and
macOS keep vcpkg binaries in an actions/cache files store keyed by runner image;
vcpkg's ABI hashes make stale entries miss. Their Rust jobs also compile through
ccache 4.14.1 (checksummed download) with PCH off; MSVC uses embedded `/Z7` debug
info (CMP0141), which ccache requires. Platform CI also runs on `rust-migration`
pushes, which checks each merge.

## Native macOS arm64 Rust linkage

CMake verifies the pinned `rustc -vV` host against the actual C++ platform,
architecture and 64-bit pointer width, then passes an explicit Cargo `--target`.
On macOS it requires exactly `CMAKE_OSX_ARCHITECTURES=arm64` and a deployment
minimum of 11.0 or newer, with the same resolved SDK and minimum supplied to Rust
through `SDKROOT` and `MACOSX_DEPLOYMENT_TARGET` (see Rust's
[Darwin target documentation](https://doc.rust-lang.org/rustc/platform-support/apple-darwin.html)).
Intel packaging, additional Windows CRT modes and Emscripten host/target builds
remain separate tasks.

Archives live at `<build>/cargo/<validated-target>/release/libopenttd_kernels.a`.
`tools/migration.py` exposes `rust_configuration(build)` and `rust_archive(build)`;
all comparison consumers use this cache-validated lookup. Imported `HOST_BINARY_DIR`
tools remain previously built executables and do not consume the target archive.

The pinned compiler's `--print=native-static-libs` output supplies final link flags
(see [static-library linkage](https://doc.rust-lang.org/reference/linkage.html#linkstaticlib)).
Configuration retains `rust-toolchain.txt` and `rust-native-libs.log`; archive builds
retain `rust-build.log`. A content-stable `rust-build-configuration.txt` dependency
records the compiler, target, SDK, minimum and relevant build flags. A changed
configuration invalidates only that target's release crate; unchanged
reconfiguration preserves the archive. Native CI checks minimum changes and
restoration explicitly.

The required macOS ARM jobs activate Rust through the reusable workflow's `rust`
input. Debug enables `OPTION_USE_ASSERTS`: game/tests get `WITH_ASSERT`, while
generators keep ordinary C++ assertions with neither `WITH_ASSERT` nor `NDEBUG`.
Release defines `NDEBUG` for every consumer. Both run the four Cargo checks,
nonempty CTest inventories and scripted regressions, then build and execute fresh
native tools. The evidence artifact retains JUnit, compiler/SDK/target metadata,
compile commands, link scripts, native libraries, archive architecture, final
Mach-O symbols and fresh generated files. Whole-archive Apple `nm` inspection is
excluded: its LLVM21 reader cannot parse LLVM23 bitcode embedded by the pinned Rust
compiler. Architecture, exact archive linkage and final executable symbol checks
remain mandatory. Linux results alone do not establish Darwin support.

## Native Windows MSVC Rust linkage

Issue #33 adds one native Windows mode: VS 2022 MSVC, x86 or x64, single-config
Ninja, `RelWithDebInfo`, `OPTION_USE_ASSERTS=ON`, and the static release CRT.
The C++ compiler's architecture macros and pointer width select
`i686-pc-windows-msvc` or `x86_64-pc-windows-msvc`; the pinned Rust host is recorded
separately, so an x64 Rust host does not choose the game architecture. The
architecture jobs install the exact target standard library and run the four Cargo
checks with an explicit target for target-dependent commands.

`WindowsRust.cmake` sets `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` before creating
any game, test or generator target, including the tools-only path; CMP0091 is NEW
before the first `project()` call (see the
[CMake runtime property](https://cmake.org/cmake/help/latest/prop_tgt/MSVC_RUNTIME_LIBRARY.html)).
The rustc native-library query and Cargo both use `-C target-feature=+crt-static`
(see Rust's [CRT documentation](https://doc.rust-lang.org/reference/linkage.html#static-and-dynamic-c-runtimes)).
The archive is `<build>/cargo/<validated-target>/release/openttd_kernels.lib`; the
shared locator validates target, pointer width, CRT, flags and path. The
configuration stamp includes these settings, so a code-generation change and its
restoration rebuild the archive.

Debug/debug CRT, dynamic CRT, other build types, assertions disabled, non-MSVC,
multi-config/non-Ninja, ARM/UWP/MinGW, cross-OS and external `HOST_BINARY_DIR`
configurations remain unsupported for Windows Rust (#3). Conflicting runtime flags,
Rust target overrides, mismatch suppression or missing target std fail
configuration; the C++ fallback is never selected implicitly.

The unchanged CMake ordering gives Windows RelWithDebInfo game/tests both `NDEBUG`
and `WITH_ASSERT`, while generators have `NDEBUG` alone. The x86 build exposed six
existing narrowing assignments (station expansion, snow-line calculation,
map-height selection, old-save station loading); explicit casts to their existing
unsigned destinations keep the original modulo conversion. All exports keep
`extern "C"`, which is cdecl on i686
([MSVC target ABI](https://doc.rust-lang.org/rustc/platform-support/windows-msvc.html)).
Rust copies foreign encoded descriptors with `read_unaligned` before taking
references, closing the [MSVC i686 alignment gap](https://doc.rust-lang.org/rustc/platform-support.html).

`windows-rust-evidence.py` checks PE machine headers, Ninja response files, exact
archive linkage, MSVC maps and static CRT imports, and runs the native ABI
executable: struct sizes, alignments and field offsets for every `abi.rs` layout
ID, high-bit scalars, by-value returns, sentinels, null/empty inputs, Rust
allocation/view/destroy paths, history-engine staging and a per-thread non-C
locale lowercase check (signed negative compare arguments stay outside the defined
C domain, #26). Fresh tools-only builds run the current string and settings
generators after deleting previous outputs.

## Team process and engineering standards

`AGENTS.md` is authoritative for agent roles, models and reasoning effort,
attribution, the issue/PR/review flow, and the evidence budget. This section adds
only repository facts. `.codex/config.toml` sets root to `gpt-6-astra` xhigh and
spawned agents to `gpt-6.1-sol` high by default, with at most five spawned threads;
explicit spawn settings select the required role, and a running host may impose a
lower limit.

The driver and CI enforce native build/tests, nonempty test inventories, reference
test-name preservation, and these Cargo checks:

```sh
cargo fmt --all -- --check
cargo check --workspace --all-targets --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo test --workspace --locked
```

Server-side protection on `rust-migration` requires the platform matrix, native
comparison, commit, and annotation checks, plus resolved conversations. Required
checks must pass on the PR head; branches need not be up to date with the base.
Platform CI also runs after each merge into `rust-migration`, and a post-merge
failure is fixed forward first. Force pushes and branch deletion are disallowed,
including for administrators. The approving-review count is zero because agents
share credentials; the attributed independent review report is the process gate
before root integrates. Repository controls are enforced separately from these
documents.

Preserve OpenTTD copyright notices, credits, and GPLv2. Agent-generated work is welcome
in this fork; upstream submission policies govern contributions to OpenTTD itself.

## Selecting components

Selection follows `AGENTS.md` and `docs/roadmap.md`: game code first, ownership
ports over fragment extraction, and the semantic simulation harness (#72) as the
default evidence for game logic. Before choosing, inventory dependencies, the
state the component owns, shared services it calls (`Random`, pools, map
access), and floating-point or overflow behavior. Retain the pinned original as
the independent oracle. When the harness finds a divergence, keep the first
differing snapshot and its inputs. Improvements to original behavior remain
deferred issues.

## Ported components

The entries below record the components ported so far. The earlier ones were
selected under the previous coverage-first rule and are mostly utility kernels.

Unless an entry says otherwise, these properties hold for every port:

- With `OPTION_RUST=ON` (`WITH_RUST`) the C++ facade calls Rust; with it off, the
  original C++ bodies compile unchanged. Rust on further platforms is #3.
- Rust panics and Rust allocation failure abort (`panic = "abort"` in both
  profiles); the C ABI never unwinds, and no C++ exception crosses a Rust frame.
  Identical resource-exhaustion timing is not claimed.
- Borrowed byte spans are initialized, readable bytes in one live allocation with
  length at most `PTRDIFF_MAX`, immutable during the call; empty spans may be
  null; read-only spans may overlap. Rust retains no pointer after returning.
- Evidence is the unchanged upstream tests first; the named comparison tool covers
  only listed gaps, and nothing here establishes whole-game equivalence.

### Landscape partial-pixel height

`GetPartialPixelZ`, the scalar landscape height kernel, runs in Rust behind its
original C++ interface. The ABI takes two `int32_t` coordinates and one `uint8_t`
slope and returns `uint32_t`; no pointers, allocations, shared state or ownership
cross it. The C++ adapter asserts integer widths, tile dimensions and slope/corner
encodings. For coordinates 0 through 15, arithmetic stays within 0 through 32 and
heights within 0 through 16. Rust preserves half-tile returns before base-slope
validation, clears all upper slope bits and retains asymmetric rounding. Unsafe
code is denied except for the scoped export-symbol attribute.

Known divergence (#75): the reserved `UINT32_MAX` result invokes the existing C++
`NOT_REACHED` handler for an unsupported base slope or an out-of-contract
coordinate. The coordinate guard is new; the original may compute a height for
some out-of-range inputs. Equivalence is limited to the documented coordinate
range; inspected callers use 0 through 15.

Evidence: 32 unchanged upstream cases through the C++ entry point (fixed grids at
all 256 tile positions, addition properties, ordinary and steep slopes, half-tile
foundations), plus small Rust tests for the flat/elevated gap and unusual
half-tile/invalid-input handling.

### StringConsumer integer parsing

C++ keeps the public templates, optional/pair/string-view adapters, cursor updates
and formatted logging. Rust owns base selection, digit scanning, width-aware
conversion, overflow/clamping and the independent lexical skip. Inspected types
are 8/16/32/64-bit signed and unsigned; native `int`, `uint`, `size_t` and used
enum aliases fall within those widths. Unsigned 64-bit values never pass through a
signed intermediate. Negative automatic hex converts/clamps to the matching
unsigned width, then negates/narrows and checks the signed value (the original
modular conversion). Signed `0x-1` still parses length four but lexically skips
two bytes; free ParseInteger rejects its remaining suffix.

The ABI borrows arbitrary bytes (including NUL/non-UTF-8) and returns `repr(C)`
metadata: zero-extended value bits, matched length and diagnostic kind/byte spans.
C++ formats the original prefix-relative messages and four-byte previews before
advancing the cursor; errors stay diagnostic in the game and fatal in generators.
`WITH_RUST` reaches game, tests, strgen and settingsgen through issue #5's shared
target. Settingsgen has no integer-template call; it links the archive and uses
only lexical skipping. Imported `HOST_BINARY_DIR` tools are already-built
executables; no cross compilation is added.

Evidence: the eleven unchanged StringConsumer cases, then (after `build` or
`verify`, so the shared reference generators exist):

```sh
python3 tools/migration.py tools
python3 tools/compare-integers.py
```

The probe compiles against unchanged pinned sources and the candidate: 8/16-bit
extrema, modular negative-hex boundaries, recursive/invalid prefixes, empty/NUL
input, long overflow runs, both clamp settings, peek/read/try/skip and free
ParseInteger, fatal logging adapters, malformed strgen diagnostics, and fresh
settings/string headers plus English/French output. Evidence and source hashes:
`.local/integer-comparison/`.

### Alternating-iterator traversal

Rust owns initial position/selectors, logical advancement, side selection, end
transitions and position comparison. C++ keeps the typed iterators, range identity
assertions, dereferencing and container lifetimes. Each increment recomputes the
live range distance; Rust requests a typed move, C++ applies it and queries only
the requested live boundary, then Rust completes the next-side state. Size is not
cached and random access is not required; stable noncontiguous iterators remain
supported after insertion.

`src/rust/alternating_ffi.h` maps size_t to usize and explicit uint8 selectors (0
before, 1 after). Nonnegative distances keep the original size_t conversion with no
added range cap. No pointer, allocation, element or ownership crosses the ABI.
Logical end skips movement/completion and keeps the last selected Base iterator,
while a separately constructed end keeps middle; they compare equal by position.

Evidence: the fifteen unchanged fixed sequences, plus public-interface tests for
empty/singleton, independent copies, postfix/prefix identity, position ordering,
distinct end Base values, stable-list insertion and typed operation counts.

### StringConsumer byte algorithms

`consumer.rs` owns exact unsigned little-endian assembly; bounded read/skip lengths
and shortfall/cursor decisions; byte-prefix matching and conditional consumption;
substring/character-set search and membership; and separator result/consumption
decisions. C++ keeps typed optional/default conversions, string_view construction,
diagnostic formatting, cursor commit and trivial accessors, and preserves empty
view pointers through the original substring at the current offset.

`src/rust/consumer_ffi.h` returns scalar `repr(C)` metadata by value. Rust calls no
C++ logger while borrowing. C++ logs a shortfall before applying the returned
position, so fatal generator logging leaves the cursor unchanged. Nonempty
search/set/separator assertions remain preconditions; release behavior outside
them (including the original empty-separator loops) is not claimed. Empty prefixes
still match at end. npos maps to SIZE_MAX/usize::MAX. Bounds subtract remaining
length before clamping, avoiding position+requested overflow. SKIP separator
policies return and consume different lengths, repeat whole non-overlapping
separators, and unknown values default to KEEP.

The in-place pair, owning string/container code, allocators, escape parsing and
encoded-string transformation remain C++. SQFile resizes its own buffer and
rebuilds the consumer; no Rust borrow survives that boundary.

Evidence: the eleven unchanged consumer cases plus four public cases (empty-prefix
offsets, partial-width TryRead cursor preservation, multi-byte separators with
offsets/overlap/unknown policy, overlapping byte sets). The `--consumer` mode of
`python3 tools/compare-integers.py` compares 60 byte/offset/shortfall cases and six
fatal-timing checks against pinned C++; the report records `consumer_bytes`.

### Spiral tile traversal

Rust owns square/hole initialization, position initialization, per-direction
movement, shell jumps, outside-map skipping, end detection and coordinate-only
equality. C++ keeps the typed iterator/sequence facade, TileXY dereference,
copies/postfix wrappers and map storage. Map dimensions are passed at each
constructor and prefix increment; Rust caches none. Orthogonal and diagonal
tile-area algorithms remain C++.

`src/rust/spiral_ffi.h` has five pointer-free by-value operations and a copyable
40-byte state (nine uint32_t fields counting the four extents, then a uint8_t
direction at byte 36; alignment 4), asserted on both sides. Directions are
NE(-1,0), SE(0,1), SW(1,0), NW(0,-1), with west shell jumps (+1,-1). Coordinate,
extent, position and radius arithmetic wraps explicitly at 32 bits, including
temporary outside-map coordinates; there is no new clamp or diameter cap. Positive
diameter/radius and increment-before-end remain original preconditions. End is
radius equality with a non-invalid direction; equality compares x,y only.

Evidence: the five unchanged ordered tests (217 assertions) plus four public cases
(clipped sequences on 128x64/64x128 maps, live map dimensions, copy/postfix/equality
and sentinel state, UINT32_MAX hole-initialization wrapping), compiled against
pinned reference algorithms, Rust facades and portable C++ bodies. The huge
inherited perimeter after a wrapped extent is not traversed. Native generators do
not use spiral traversal.

### StringBuilder numeric byte encoders

Rust extracts the bytes for `PutUint8` and `PutUint16LE`/`32LE`/`64LE`, and formats
integral `PutIntegerBase` values in bases 2 through 36: lowercase digits, a leading
minus for signed negatives, no base prefix, one digit for zero. Signed binary
wrappers keep their modulo-width unsigned casts; the signed magnitude uses unsigned
negation, including `INT64_MIN`. The original 32-byte scratch buffer (including the
minus sign) is kept: values needing more produce no `PutBuffer` call (for example
`INT32_MIN` in base 2), exactly 32 bytes produce one; #12 records a possible
capacity improvement. `PutUtf8` uses the migrated codec and still makes one
zero-length sink call for an invalid codepoint.

`src/rust/builder_ffi.h` takes scalars and returns `repr(C)` byte arrays by value;
C++ passes a span of the returned local array synchronously to its virtual
`PutBuffer`, and sinks must not retain it. Allocation, sink exceptions, raw `Put`,
InPlaceBuilder copy/overlap handling and cursor updates remain C++. A C++ overload
adapter preserves integer overload selection, unscoped-enum promotions and implicit
user-defined conversions; bool stays rejected and widths are checked at compile
time. Invalid bases are outside the original 2..36 `std::to_chars` precondition.
Strgen uses the binary encoders and UTF-8 but not `PutIntegerBase`; settingsgen
has no numeric builder call.

Evidence: the three unchanged StringBuilder cases, InPlaceReplacement and
encoded-string tests, plus the `--builder` mode of `python3
tools/compare-integers.py`: 1,624 formats (all bases, widths, extrema, zero, the
32/33-byte boundary), six alias, three unscoped-enum and three implicit-conversion
cases, and an ordered counting sink checking lengths, order and absent calls.

### Encoded-string compatibility and parameter rewriting

Rust owns `FixSCCEncoded`, `FixSCCEncodedNegative`, `EncodedString::ReplaceParam`
and the shared `GetEncodedStringWithArgs` serialization. C++ keeps the save-version
dispatch and its order (legacy encoding before version 350, old markers before
169, negative repair before 353, then sanitation), general decoding, rendering,
ScriptText encoding and sanitation.

Retained quirks: legacy conversion is permissive (old E028/E02A normalize only with
fix_code; markers are recognized inside quotes; quotes toggle and disappear; quoted
colons stay bytes; numerics are not validated). A valid nonmarker first character
leaves the string untouched; invalid first UTF-8 yields empty output and later
invalid UTF-8 truncates. Negative repair accepts only SCC_ENCODED, tries unsigned
before signed hex, keeps signed modulo bits, canonicalizes positives too, and on a
failed read logs, defaults to zero and lexically skips. ReplaceParam requires the
internal marker and uint32 hex ID; empty records and unknown types become
monostate; a final separator adds no record; out-of-range replacement returns empty
after the original parsing/diagnostic/assertion work. String parameters stay
arbitrary bytes (NUL and RS included); public StringParameter construction still
converts negative integers to uint64 before the boundary.

`src/rust/encoded_ffi.h` passes explicit tags (0 monostate, 1 uint64, 2 byte span),
never C++ string/vector/variant layouts; StringID width and the RS/E000..E003 token
contract are asserted. Rust returns an opaque Box owning output and diagnostics;
C++ holds it in a unique_ptr with the Rust destroy deleter, even if copying or
logging throws. Diagnostic offsets refer to the complete input; one scan stops at
the first enabled assertion, and C++ replays earlier logs before asserting.
Numeric assertions follow `!NDEBUG || WITH_ASSERT`; the serializer's string-prefix
check follows `WITH_ASSERT` alone.

Evidence: the four unchanged FixSCCEncoded/Negative and ReplaceParam tests, plus
`python3 tools/encoded-comparison.py` (verbatim pinned functions, Rust and portable
bodies, four NDEBUG/WITH_ASSERT combinations, ASan/UBSan/LSan on the C++ side; the
Rust archive is uninstrumented). Evidence: `.local/encoded-comparison/`. No
generator runtime coverage is claimed.

### Byte-string utilities

Issue #21 moved case-insensitive compare/equal/prefix/suffix/contains, lowercase
conversion, uppercase hex encoding, sequential hex decoding and byte-set trim
scanning into `byte_strings.rs`. C++ keeps std::string owners, views, erases and the
installed standard library's equal-prefix length-comparison policy; Rust returns
that C++-supplied scalar only when the shared prefix is equal. All operations are
length-delimited, including embedded NUL.

Rust calls native C `toupper` with the original char promotion (signedness supplied
by C++) and `tolower` with unsigned-byte promotion, keeping process-locale
behavior. Negative-char uppercase inputs other than EOF are outside portable C's
domain; only observed native behavior is matched (#26). Locale must not change
concurrently.

Lowercase takes an exclusive mutable span with offset at most size. Hex encode
writes disjoint caller-owned output. Hex decode forms no Rust slices: raw reads of
both nibbles precede each raw write, so legal input/output overlap works and earlier
writes survive a later invalid pair; rejected lengths write nothing. Trim returns
offsets; C++ returns a default null-data view when everything trims, and in-place
trim keeps the newline-preserving whitespace set. Only the portable natural-contains
fallback uses these helpers; ICU, Windows/macOS collation, validation, in-place
replacement and StringIterator backends remain C++.

Evidence: the twelve unchanged utility tests, plus `python3
tools/byte-strings-comparison.py` (3,699 records against full pinned string.cpp,
length-result saturation above INT_MAX via mmap, -funsigned-char builds). Host
locales are C, C.utf8 and POSIX only, so non-C mappings are untested. ASan/UBSan
cover the C++ side only. Evidence: `.local/byte-strings-comparison/`.

### UTF-8 codec and byte positions

`EncodeUtf8`, `DecodeUtf8`, `IsUtf8Part`, forward/backward iterator stepping and
`GetIterAtByte` normalization run in Rust. The C++ view keeps its borrowed
string_view and iterator facade, pair adapters, comparison assertions, postfix
copying and invalid-data `?` dereference. Native generators use the same codec
through issue #5's shared target.

Retained behavior: surrogates are accepted; overlong and out-of-range first
sequences are rejected; malformed trailing data after a valid first sequence is
ignored; unused encoding bytes are zeroed. View movement scans continuation runs
rather than decoded lengths. StringConsumer read/skip still advance one byte on
decode failure; `TryReadUtf8` leaves its position unchanged.

`src/rust/utf8_ffi.h` uses 32-bit codepoints and size_t lengths/offsets and returns
`repr(C)` data by value, with no allocations or output aliases. The C++ facade keeps
the original position assertions; valid positions bound each step, and
codepoint/byte conversions are explicitly masked or bounded.

Evidence: the three unchanged UTF-8 view tests and consumer/builder tests, plus
`python3 tools/utf8-comparison.py` (assertion and NDEBUG builds; encoding
boundaries, malformed runs, embedded NUL, empty views, consumer-versus-view
movement, and the `offset >= size` end branch including SIZE_MAX). Evidence:
`.local/utf8-comparison/`. Game logging and Unicode rendering are outside it.

### Rounded square root and runtime integer saturation

`IntSqrt(uint32_t)` and runtime `ClampTo`/`SoftClamp` call `math.rs` through
`src/rust/math_ffi.h`. `IntSqrt` keeps nearest-integer rounding, including 65536
for UINT32_MAX. `DivideApprox` remains C++: its potentially overflowing signed
intermediates need separate work. Constant evaluation keeps the original bodies via
`std::is_constant_evaluated()`; StrongType and OverflowSafeInt overloads keep their
unwrap-and-forward behavior.

The ABI is scalar-only: modulo-2^64 value bits plus explicit width and signedness;
C++20 integral conversion rebuilds the result. ClampTo also accepts 1-bit bool
descriptors; the original template still decides which bool instantiations are
well-formed (bool-to-int8 stays ill-formed). Rust compares in a bounded i128 domain
and never passes uint64 through int64. Accepted wider unsigned destinations use
Rust's uint64 saturation then C++ widening. Wider sources, wide signed
destinations, wide SoftClamp and non-builtin integer-like destinations keep the
original C++ runtime body; strict GCC/libstdc++ rejects some 128-bit cases that
libc++ accepts. No new width precondition is imposed. The adapter checks 8-bit
bytes and the 32-bit int promotion model.

Reversed signed 8/16-bit SoftClamp intervals convert min to unsigned and then
promote to int, so `SoftClamp<int8_t>(0, -1, -3)` returns 126. Reversed 32/64-bit
signed intervals use unsigned subtraction/division and modular conversion; unsigned
intervals round toward min.

Evidence: the unchanged IntSqrtTest Zero/FindSqRt, ClampTo and SoftClamp cases,
plus `python3 tools/math-comparison.py` (every uint32 root square and rounding
transition, width/signedness extrema, bool and character/size aliases, adapters,
unsigned 128-bit destinations, SoftClamp intervals, GNU link-wrap call counts;
`.local/math-comparison/`) and `python3 tools/math-extension-comparison.py` (wide
templates on the native library; macOS CI runs it with `--build build`). Neither
covers all inputs or platform ABIs.

### Generic history structural engine

Issue #29 moved descriptor-driven validity, rotation scheduling and query traversal
into `history.rs`. HistoryRange constexpr construction/layout, typed HistoryData
storage, every SumHistory specialization, graph fillers and
GetAndResetAccumulatedAverage remain C++; production averaging keeps its literal-0
int accumulators and nested reduction grouping. Saves are unchanged.

Rust keeps an arbitrary-depth scalar frame stack and streams staged operations; C++
describes immutable ranges by value and executes every typed operation after Rust
returns. Opaque uintptr identity tokens become pointers only in C++. The acyclic
descriptor chain must stay live and immutable until engine destruction. Unsigned
index arithmetic wraps at uint32, and GB's uint32 truncation remains for uint64
validity masks. Typed constructors, reducers and destructors may throw without
crossing Rust; C++ RAII returns the opaque engine to Rust exactly once.

Retained quirks: update and rotation use explicit cur_month while queries read the
live TimerGameEconomy::month; children update/rotate first even for saturated or
skipped parents; no-prerequisite higher rotations still shift; query validity ORs
all children while IsValidHistory checks only the first; invalid children still
contribute data; invalid query ages keep the original fatal dispatch.

Evidence: the unchanged 288-month test (86 assertions in each standalone run), plus
`python3 tools/history-comparison.py` (11,133 records at O0/O2: phases, masks,
arbitrary chains, ages, typed operation order, exceptions, aliasing, graph fillers,
and the three verbatim production reducers; a nested-year fixture yields 0 where a
flattened reduction yields 1). ASan/UBSan cover the C++ side only. Fatal stubs
compare dispatch, not game fatal text. Evidence: `.local/history-comparison/`.

### Authentication and streaming owners

Issue #35 moved X25519 session and encryption-context ownership into Rust behind a
versioned, primitive-only host function table. Bundled Monocypher algorithms are
unchanged. Rust owns stable key/session allocations and vendor-context storage; C++
supplies each vendor context's size/alignment and starts its trivial lifetime, so
Rust mirrors no vendor struct and other archive consumers gain no vendor symbols.
Packet, RNG, policy and logging calls run after each Rust call returns.

Secrets are initialized in their final allocation; copies go heap to heap and
assignment overwrites fixed storage, as in C++; rvalue copies preserve the source.
Destruction wipes with the bundled volatile wipe before deallocation, in the
original reverse field order; temporary shared secrets have independently wiped
storage. Streaming contexts keep the bundled successful-rekey behavior (the counter
does not advance); failed authentication leaves context and output unchanged.
Register spills, caller copies and vendor temporaries are not covered; complete
secret erasure is not claimed.

Borrowed fixed-width views keep their address until owner destruction; callers
serialize access. Exchange extra payload may alias derived key bytes. MAC and
message regions must be disjoint; encryption is in place through raw pointers. The
original short nonempty `Packet::Recv_bytes` path is undefined; deferred #41 tracks
it.

Evidence: the five unchanged network cases, plus `python3 tools/auth-comparison.py`:
pinned and candidate session/Packet/vendor sources as separate endpoints, both mixed
directions, Rust/Rust and portable C++ against original transcripts (wire bytes,
derived keys, RNG traces, failure/retry, copies, aliasing, exception cleanup), with
a C++ sanitizer run that also checks Rust allocator leaks. Not constant-time or
erasure evidence. Evidence: `.local/auth-comparison/`.

### Paired Script Admin conversion

`ScriptAdminMakeJSON` and `ScriptEventAdminPort::GetObject` keep their C++
interfaces. Rust selects types, walks both conversion directions, propagates
results and schedules the original VM and JSON operations; an opaque per-invocation
handle returns scalar actions that C++ executes after each call returns. C++ keeps
the bundled Squirrel VM, nlohmann JSON, script logger and network send/framing. No
JSON tree or byte string crosses the ABI; stable C++ heap frames hold JSON
temporaries, iterators and copied keys.

Outgoing: `depth == 25` is checked before reading the VM type or changing JSON
(also for an explicit initial depth); live iteration, key stringification,
duplicate-key overwrite order, `index - 1` and `depth + 1` are kept; a failed child
leaves completed root children, with the original VM and iterator pops before
cleanup. Incoming: object root only, floats rejected, no depth limit; failure
restores the stack top before logging and pushing null; malformed input keeps the
original diagnostic. `SQInteger`/`SQRESULT` are signed 64-bit, `SQBool` unsigned
64-bit, depth signed 32-bit (also on i686); unsigned JSON still uses
`get<int64_t>()`. Allocation errors, `Script_FatalError`, nlohmann exceptions and
reentrant key metamethods occur between Rust calls; RAII destroys owners without
running pending pops, rollback or logging.

Evidence: the two unchanged `test_script_admin.cpp` cases (15 outgoing and 27
incoming checks), plus `python3 tools/admin-conversion-comparison.py`, which uses
the bundled VM and unchanged pinned bodies at O0/O2: depth 25/26, incoming depth 40,
integer extrema, byte keys, partial failure, colliding keys, reentrant `_tostring`,
allocation failures and a script allocation limit. Its GNU link-wrap cleanup checks
run on native Linux only. Game-log storage, arbitrary depths, network simulation
and complete script equivalence are outside this evidence.

### ScriptList storage and iteration

Issue #44 moved both item/value indexes, all four sort modes, live pending-cursor
and end state, mutation accounting, filters and list algebra into Rust. C++ keeps
script identity/bindings, pool enumeration, VM/error/operation charging and
save/load adapters. Valuation and serialization read copied ascending-item scalars;
every Rust borrow ends before a VM operation can reenter the list. The mutation
token is checked after the original callback return-type check; SetValue occurs
before the original pop and five-operation charge; earlier commits and callback
side effects remain on failure. Clone uses the original sort/initialization flow;
saving does not reset public iteration; mixed-type load validation is unchanged.

The scalar/pointer ABI avoids aggregate returns on 32-bit hosts; items/values are
signed 64-bit and modification tokens signed 32-bit. Two-list operations detect
self-aliasing before creating references. Outside the reproduced domain: original
signed modification-counter overflow, nonempty rank decrement overflow,
overflowing Count()-count, and callbacks that invalidate iterators while evading
the modification check.

Evidence: unchanged `regression_regression` and `regression_stationlist`, plus
`python3 tools/script-list-comparison.py` (original, Rust and portable binaries at
O0/O2 with the bundled VM: cursor swaps and insertions, self operations, pending
removals, empty-list union, filters, zero/negative ranks, callback failures,
partial commits, operation charges, List/TileList save/load and clones). It
substitutes a command-permission bool for the game instance and populates no
world; it is not VM, savegame or allocation-failure equivalence.

### Nested widget descriptor parser

`MakeNWidgets` and `MakeWindowNWidgetTree` use a widget-specific pull parser. Rust
owns the descriptor cursor, attribute traversal, container decisions, recursive
tree control, end markers, complete-consumption policy and first/root/body/shade
composition. C++ keeps the single-part factory, attribute operations, widget
classes, RTTI checks, `unique_ptr` owners, Add/GetWidgetOfType, generator callbacks
(which may run a nested parser), constexpr builders and public signatures.

The scalar ABI carries uint64 descriptor offsets, owner slots, uint8 widget tags
and capability observations; no union, virtual object, RTTI layout or function
pointer enters Rust. Enum widths, range markers, container tags and action offsets
are asserted in C++. Retained quirks: push-button bits are not masked for container
classification; function-produced subtrees never adopt following nodes; EOF inside
a container is accepted; null generator results keep the unconsumed cursor and
end-marker assertion; the `WITH_ASSERT`-only trailing-parts exception stays
separate. Window construction clears shade first, queries caption then shade only
with a remaining body, and writes the shade pointer before building the body; the
inserted stacked wrapper keeps INVALID_WIDGET and its vertical body container.
Exceptions keep already-committed children; the shade output is not reset. The
shared Rust archive imports no widget-library callbacks.

Evidence: all four unchanged `test_window_desc.cpp` bodies (every registered
WindowDesc through the production parser), plus `python3
tools/widget-parser-comparison.py` against unchanged pinned parser bodies with real
construction primitives (163 registered descs natively): types, order, attributes,
shade, ownership, callback/cleanup order, exceptions, and null generators under
three assertion policies. It runs on native Linux; fatal probes compare termination
category, not assertion text. Rendering, layout, events and arbitrary malformed
descriptors are outside this evidence.

### ChaCha20, Poly1305 and AEAD primitives

Issue #48 moved the bundled Monocypher 4.0.2 ChaCha20/Poly1305/AEAD family. Its C
interfaces and caller-owned context types stay. Rust owns cipher rounds, MAC
arithmetic, incremental buffering, authentication padding and composition. C++
starts context lifetimes and passes size/alignment/field-offset descriptors; raw
field access reads only initialized fields, preserves unwritten chunk/padding bytes
at init and wipes the complete caller context at finalization. Counter access is
raw and unaligned-capable (no i686 uint64 alignment assumption).

The nonthrowing wipe and constant-time verify16 come from a two-function explicit
cdecl table; Rust has no vendor imports or global callbacks. Cipher input and
output are disjoint or exactly in place; key/nonce loads precede output (Elligator
key generation overlaps its key). Unsigned arithmetic wraps; failed reads preserve
output and context; successful stream operations rekey without incrementing the
counter. Other Monocypher algorithms and the portable family remain C++. Compiler
copies and spills are not erased; this is not cryptographic certification.

ABI layout IDs 19 through 21 cover the two-leaf table and two field-layout
descriptors. Only the native ABI executable adds a vendor object (exercising all 15
facade/FFI calls); strgen and settingsgen link the archive with no vendor dependency.

Evidence: `python3 tools/auth-comparison.py` (1,800 transcript records) and its
`--primitives` mode (484 direct records against pinned vendor functions through Rust
and portable C++: variants, null keystream, overlap, split Poly1305 updates,
padding, initializers, rekey, failure/retry), with C++ ASan/UBSan; Rust accesses are
not instrumented.

### Station cargo-list queries

Issue #51 reuses the Rust ScriptList owner. Rust owns the four selector/filter
rules, pending run keys and unsigned 32-bit totals, positive flush decisions,
add-versus-set merges, per-origin cumulative-share decoding, and query-plan
selection (waiting/all versus equal_range, planned/all versus find). C++ keeps
station/cargo validation, pool/GoodsEntry access, HasData checks, packet/flow
iterators and scalar extraction. All eight specialized constructors and the generic
facades share the reducer. Non-cargo station lists, routing and packet/flow
ownership remain C++.

A caller-owned 16-byte collector (uint32 amount/previous, uint16 station IDs
including 0xFFFF, byte selector/finalized) is asserted in C++ and ABI layout ID 18;
Windows x86 uses explicit cdecl. Rust borrows the destination only during
feed/finalize. A C++ RAII finalizer flushes the last positive run on normal exit,
early return or unwinding; finalization is idempotent, with no rollback. Filtering
precedes key changes; equal-key totals and share differences wrap modulo 2^32;
previous resets per origin and advances on every visited share, including filtered
and restricted ones. Original signed result/modification-counter overflow is
outside the defined domain.

Evidence: the unchanged stationlist script (24 constructions, 41 output rows), both
scripted suites, the upstream unit inventory, and the bounded cargo mode of `python3
tools/script-list-comparison.py` (pinned loop bodies, portable and Rust feeds,
FlowStat origins, wrapping, sentinel keys, repeats, reentry, a C++ exception between
feeds). The direct fixture bypasses world lookup, so failed station/cargo/company
policy and missing-data probes are not executed; this is not cargo routing or
station-storage equivalence.
