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

The premature Rust integer-square-root implementation, crate, linkage, and dedicated
comparison test have been removed. The current candidate uses the original C++
implementation throughout. Rust toolchain/bootstrap setup remains available for
future work; no first Rust component has been selected. Preserve the complete game,
including networking, saves, NewGRF mods, graphics, and shared random-number behavior.

## Build and verification

The current verification setup targets native Linux and needs a C++20 compiler,
CMake, Ninja, Python 3.11 or newer, SDL2 development files, and the normal OpenTTD
libraries described in `COMPILING.md`. OpenGFX supplies free graphics for regression
games; commercial game assets are unnecessary. Rust is not required by the current
build or verification driver.

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
`.local/deps`, and installs Rust into `.local/cargo` and `.local/rustup`. This retained
future Rust setup is separate from the current C++ build requirements. The bootstrap
changes no shell profiles or system packages. It relies on installed compiler/runtime
libraries and host-specific package names; it is not a portable installer.
`rust-toolchain.toml` pins future Cargo compiler, formatting, and lint tools.

The driver builds both graphical executables, runs both CTest suites (upstream unit
and scripted game tests), and requires the candidate to retain all reference test
names. Empty test inventories fail verification. CI runs this native verification
and uploads its evidence.

```sh
python3 tools/migration.py build --jobs 6
python3 tools/migration.py verify --jobs 6
```

Each invocation retains command logs, test reports, executable hashes, source revision,
and local changes under `.local/verification/<timestamp>/`. Passing establishes only
covered behavior. The driver does not itself compare every game state or prove full
game equivalence. Removal of the premature port has received focused checks; no
post-removal full build/test pass is claimed here.

The fork executable is `build-rust/openttd-rust`; the directory name does not imply
that any current component uses Rust. The reference executable is
`build-reference/openttd`. Most in-game branding remains original. When using this
host's extracted dependencies, launch with their library path:

```sh
LD_LIBRARY_PATH="$PWD/.local/deps/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  ./build-rust/openttd-rust -X
```

`-X` avoids global game folders. Use separate development configuration and saves.

## Selecting and validating components

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

Current automation enforces native build/tests, nonempty test inventories, and
reference test-name preservation.
When a Rust crate is introduced, add CI checks for formatting, compiler checking,
Clippy with warnings denied, and tests:

```sh
cargo fmt --all -- --check
cargo check --workspace --all-targets --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo test --workspace --locked
```

Keep unsafe/FFI code narrow and document safety, ownership, lifetimes, and panic
behavior. Define overflow and integer conversions explicitly to preserve behavior.
Avoid unrelated blanket C++ warning changes. Server-side branch protection and required
review rules are separate repository controls; these documents do not claim they have
been configured. Rust checks above are future requirements, not checks currently run.

Preserve OpenTTD copyright notices, credits, and GPLv2. Agent-generated work is welcome
in this fork; upstream submission policies govern contributions to OpenTTD itself.
