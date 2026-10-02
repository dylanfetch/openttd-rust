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
C++ interface. The shared crate also implements StringConsumer's integer parsing
and lexical skipping, including native string/settings generator uses. Neither
replacement completes its containing subsystem. Preserve the complete game,
including networking, saves, NewGRF mods, graphics, and shared random-number behavior.

## Build and verification

The current verification setup targets native Linux and needs a C++20 compiler,
CMake, Ninja, Python 3.11 or newer, SDL2 development files, and the normal OpenTTD
libraries described in `COMPILING.md`. OpenGFX supplies free graphics for regression
games; commercial game assets are unnecessary. The verification driver requires
the pinned Rust toolchain and always configures the candidate with `OPTION_RUST=ON`.
Ordinary CMake builds default to `OPTION_RUST=OFF`, preserving the original portable
C++ path. Rust linkage currently supports native Linux; other platforms and cross
compilation remain outstanding migration work and reject an enabled Rust option.

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
StringConsumer cases remain the primary existing tests. Other consumer/builder/UTF8
algorithms, C++ adapters, and non-Linux Rust integration remain migration work.

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
