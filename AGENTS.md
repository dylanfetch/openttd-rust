# OpenTTD-Rust

This independent experimental fork incrementally replaces C++ with Rust while
preserving the original game's observable behavior. The original game supplies
the specification, including historical quirks. Record possible improvements in
fork GitHub issues for consideration after near-full Rust reproduction.

Read `docs/rust-migration.md` for setup, validation, and project process, and
`docs/roadmap.md` for current priorities. The roadmap decides what to work on
next; do not start work outside it without root re-selecting and updating it.
`migration/baseline.json` pins the original revision. `README.md` identifies the
fork; the remaining upstream documentation explains behavior and architecture.

## Migration method

- The goal is a game whose simulation runs in Rust. Prioritize game code: map
  generation, landscape and tile loops, towns, industries, vehicles, cargo,
  economy, link graph, pathfinding, and the commands that change them. Support
  code is ported only when a selected game component needs it. Vendored
  libraries (`src/3rdparty`), GUI rendering, and platform code come last.
  Upstream unit-test coverage alone is not a reason to select a component.
- Prefer ownership ports: Rust owns the component's state and control flow, and
  C++ keeps only a thin facade plus callbacks for shared services (`Random`,
  pools, map access, progress). With `WITH_RUST` defined the original C++ body
  is not compiled; it remains in the `#else` of `#ifdef WITH_RUST` (or under
  `#ifndef WITH_RUST`) for portable builds. Do not extract an algorithmic
  fragment while C++ keeps the surrounding state and loop, unless the roadmap
  names it as a stepping stone.
- Keep the full game running throughout migration. Preserve networking, saves,
  mods, interface, and shared random-number behavior, including the exact
  sequence of random draws.
- Reproduce the original over its whole reachable input domain. Do not add
  guards, assertions, rejections, or fatal paths that the original lacks. Where
  the C++ wraps or truncates, use explicit `wrapping_*` operations or `as`
  casts; Rust overflow checks (enabled in release, with `panic = "abort"`) may
  only back up operations that cannot overflow in the original. This applies to
  new work; known divergences in existing ports are tracked as issues (#75).
- Call shared services directly. Rust calls `Random`, map and pool accessors and
  other shared services through `noexcept` C++ wrappers and keeps the original's
  control flow, so a port reads like the C++ body it replaces. Environmental
  failures are not simulation behavior: allocation failure, I/O errors in debug
  or log output, and paths that need developer-only defines such as
  `RANDOM_DEBUG`. An exception escaping a wrapper terminates. Return control to
  C++ (an action protocol) only where a C++ exception can unwind through the
  call during ordinary play (script VMs, save/load errors). Name each such
  service. Reentry alone is not a reason: end every Rust borrow before a
  callback that can reenter Rust or mutate its state, then call it directly.
- Keep the boundary cheap and typed. Entries are plain synchronous calls with
  no per-call heap allocation and no async, future, task or mailbox machinery.
  Each service is its own typed `noexcept` function, not an opcode switch or
  positional array, and hot reads fetch only the fields they use rather than
  whole-record views. Existing ports convert under #168.
- Evidence for game-logic ports is the semantic simulation harness (`python3
  tools/migration.py simulate`, #72) plus the existing tests; a new game-logic
  port integrates only after the harness exists and its scenarios exercise the
  component. Fidelity fixes to existing ports (such as #75) are not new ports
  and do not wait for it. Extend the harness's scenarios rather than writing a
  new per-component comparison tool. Add a narrow comparison against unchanged
  reference bodies only for a concrete gap the harness cannot reach, and say in
  the PR which gap.
- Keep the pinned reference worktree unchanged. Never change candidate behavior
  and expected results together merely to make checks pass. Existing test success
  establishes covered behavior, not complete game equivalence.
- Compare semantic state. Screenshots alone and compressed-save byte equality
  cannot establish equivalent simulation. Retain small failures and explain
  discrepancies before accepting changes.
- Record changes, reproducible checks, and remaining limits in PRs and migration
  documentation. Scaffolding does not complete a subsystem.

## Evidence budget

Evidence must be checkable, not exhaustive prose. Spend effort on code and on
checks that run; do not restate them in paragraphs.

- Issue: scope, affected interfaces, evidence plan, and acceptance criteria.
  About 60 lines at most; leave design detail to the implementation.
- PR description: what moved, what stays in C++, exact commands, known limits,
  and the output of `python3 tools/port-metrics.py` (the roadmap progress
  metric). About 60 lines at most. Integration PRs link the component PRs
  instead of re-describing them. `.github/PULL_REQUEST_TEMPLATE.md` and the
  migration issue form follow this budget.
- Component entry in `docs/rust-migration.md`: about 25 lines at most.
- Roadmap: forward-looking, about 200 lines at most. A completed item becomes
  one table row (issue, PR, commit, metrics); its evidence stays in the PR. The
  resume checkpoint is a short status table, not a log: CI run IDs, executable
  hashes, probe values and per-PR plans go in the PR or issue. Committed docs do
  not cite `.local/` receipts, which nobody else can check.
- Harness tooling for a port goes in that component's scenario module. Tooling
  is code to maintain; keep it proportionate to the C++ the port retires.
- Review report: reviewed commit, findings with their fixing commits (or why one
  went to root), plus one line naming the functions or files compared against
  the original body. Do not
  narrate further when there are no findings. Branches the harness does not
  reach go in the coverage tracker (#156), not only in the disposition.
- One PR per component, targeting `rust-migration` directly. Use an integration
  branch only when CI capacity forces batching. Updating a PR branch from its
  base (merge or rebase) is fine; do not add merges whose only purpose is to
  keep reviewed commit ancestry.

## Agent team and review

The root agent orchestrates and delegates heavily, owns component selection and
integration, and verifies delegated diffs and evidence. Target concurrency is six
agents total: the root plus five subagents, matching `.codex/config.toml`. Respect
the running host's actual limit; this document cannot raise a session limit.

Every spawn must specify a model and reasoning effort rather than inherit them:

- `gpt-6.1-sol`: most implementation and analysis, and all independent review;
  high effort by default. Use lower effort only for a clearly bounded task that
  justifies it.
- `gpt-6-luna`: bounded mechanical work; low or medium effort.
- `gpt-6-astra`: all delegated planning at high effort. Root uses xhigh effort
  so every Astra subagent has strictly lower effort than root.

Every agent-authored GitHub issue, PR, comment, and review report must identify
the agent, exact model, and reasoning effort, including artifacts authored by root.
For example: `Agent: /root/implementation | Model: gpt-6.1-sol | Reasoning effort: high`.
A user-directed session outside Codex (for example Claude Code) names its exact
model and states its effort as reported by its host.

Spawn a fresh agent for each task and name it after that task. Do not reassign a
finished agent to unrelated work: its name is its attribution, and its context
carries over. Each review round, including a re-review, gets a fresh reviewer; it
reads the PR's earlier attributed review reports for context.

Close each agent when its task ends. When the host's slot limit is reached, root
works through its own queue (integration, verifying fix commits, roadmap) until a
slot frees. Root leaves host state alone: no archiving threads or editing host
databases to free slots.

For substantive changes:

1. Create a fork issue with scope, affected interfaces, evidence plan, and
   acceptance criteria. Give each task a clear owner.
2. Use an isolated branch/worktree based on `rust-migration`. The implementation
   agent opens a draft PR targeting `rust-migration`, linking the issue and
   recording exact validation commands and limitations.
3. Assign a separate reviewer agent to examine the final commit and check evidence.
   The reviewer fixes what it finds: it commits each fix to the PR branch, reruns
   the affected checks, and lists each finding with its fixing commit. Root
   verifies the reviewer's fix commits before integration. Findings that need a
   scope or policy decision go to root unfixed. Updating a reviewed PR from its
   base needs no new review when the update has no conflicts in `src/` or `rust/`
   and CI passes; otherwise a fresh reviewer checks and fixes only the conflict
   resolution.
4. Root integrates after review and required checks, records completion, and
   removes the merged worktree. A red check on `rust-migration` itself is fixed
   before more integrations.

Keep at most six unintegrated component branches, counting drafts and WIP
(#157). Integrating reviewed work comes before starting a new component, and a
component starts from integrated `rust-migration`, not from an unmerged
component branch, unless the roadmap names the dependency.

Do not self-approve or imply that agents sharing GitHub credentials are independent
GitHub accounts. With shared credentials, publish an explicitly attributed review
report naming agent, exact model, reasoning effort, reviewed commit, findings, and
disposition. Platform approval requires a reviewer with separate credentials.

The user authorizes ordinary issues, PRs, and review reports on this fork. Do not
contact upstream maintainers or submit upstream changes. This fork welcomes
agent-generated work; upstream contribution policies apply to upstream submissions.

## Standards and commands

C/C++ uses tabs; Rust uses `cargo fmt`. The inherited commit checker examines
every commit's diff and title: fix style in the introducing commit, and use
supported prefixes such as `Add:`, `Change:`, `Fix:`, `Doc:`, or `Update:`.
Use ASCII without tabs in commit messages, including their bodies. Integration
merge titles also need a supported prefix; `Merge:` is rejected by inherited CI.
Python tools use `uvx --from ruff==0.16.8 ruff check tools/` and
`uvx --from ruff==0.16.8 ruff format --check tools/`.

`python3 tools/migration.py verify` builds and tests the pinned original and fork,
checks that the candidate retains reference test names, and records evidence under
`.local/`. `python3 tools/migration.py build` builds both without running tests.
`python3 tools/migration.py tools` builds the native Rust generators.
`python3 tools/migration.py simulate [name...] [--soak] [--self]` builds both
games and compares their simulation state (`tools/simulate.py`).
`python3 tools/run-comparisons.py [name...]` runs the reference comparison tools
in parallel and picks up any `tools/*-comparison.py` automatically. The
reference uses C++; the candidate explicitly
enables `OPTION_RUST` and links the migrated kernels into both the game and test
executable.

Every worktree of a clone shares one pinned reference checkout and build (in the
main checkout, serialized by a lock), the bootstrapped toolchain and dependencies,
and per-role ccache stores. ccache is used whenever it is installed (PCH off);
`--no-ccache` gives an ordinary PCH build. Cache build outputs freely when the
key covers what determines them; never cache test results.

Iterate locally: incremental builds take seconds to minutes, while every push
starts roughly 15 minutes of CI. Push when a change is ready for CI or review,
with its commits batched, and keep working while CI runs.

Docs-only changes (only `docs/` or `*.md` files, such as roadmap updates) are
committed directly to `rust-migration` as a single commit, without a PR.
Everything else (code, tools, workflows, saves) goes through a PR that passes
CI before it merges; never merge a PR past CI with `--admin`.

Rust changes require reproducible `cargo fmt --all -- --check`,
`cargo check --workspace --all-targets --locked`,
`cargo clippy --workspace --all-targets --locked -- -D warnings`, and
`cargo test --workspace --locked` checks. The migration driver and CI enforce them.
Keep unsafe/FFI boundaries narrow and document safety, ownership, lifetimes, and
panic behavior. Make overflow and integer-conversion behavior explicit and faithful
to the original. Avoid unrelated C++ warning cleanup or blanket warning changes.

Builds, dependencies, toolchains, and reference worktrees belong in ignored
locations. Preserve upstream copyright notices, credits, and GPLv2.
