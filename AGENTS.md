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
  is not compiled; it remains under `#ifndef WITH_RUST` for portable builds.
  Do not extract an
  algorithmic fragment while C++ keeps the surrounding state and loop, unless
  the roadmap names it as a stepping stone.
- Keep the full game running throughout migration. Preserve networking, saves,
  mods, interface, and shared random-number behavior, including the exact
  sequence of random draws.
- Reproduce the original over its whole reachable input domain. Do not add
  guards, assertions, rejections, or fatal paths that the original lacks. Where
  the C++ wraps or truncates, use explicit `wrapping_*` operations or `as`
  casts; Rust overflow checks (enabled in release, with `panic = "abort"`) may
  only back up operations that cannot overflow in the original. This applies to
  new work; known divergences in existing ports are tracked as issues (#75).
- Evidence for game-logic ports is the semantic simulation harness
  (`python3 tools/migration.py simulate`, added by #72; until then, existing
  tests plus narrow comparisons) plus the existing tests. Extend the harness's scenarios rather than writing a new per-component
  comparison tool. Add a narrow comparison against unchanged reference bodies
  only for a concrete gap the harness cannot reach.
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
  and the four line counts from the roadmap progress metric. About 60 lines at
  most. Integration PRs link the component PRs instead of re-describing them.
  Count lines with `git diff --numstat origin/rust-migration...HEAD`: Rust
  added is `rust/`; tooling is `tools/`; glue is lines added under `src/`
  outside `#ifndef WITH_RUST` blocks; C++ retired is the original lines newly
  enclosed by `#ifndef WITH_RUST` (or deleted). Comments and blanks count.
- Component entry in `docs/rust-migration.md`: about 25 lines at most.
- Review report: reviewed commit, findings, and dispositions. Do not narrate
  what was verified when there are no findings.
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

- `gpt-6.1-sol`: most implementation and analysis; high effort by default.
  Use lower effort only for a clearly bounded task that justifies it.
- `gpt-6-luna`: bounded mechanical work; low or medium effort.
- `gpt-6-astra`: all delegated planning at high effort and all independent review
  at medium effort. Root uses xhigh effort so every Astra subagent has strictly
  lower effort than root.

Every agent-authored GitHub issue, PR, comment, and review report must identify
the agent, exact model, and reasoning effort, including artifacts authored by root.
For example: `Agent: /root/implementation | Model: gpt-6.1-sol | Reasoning effort: high`.
A user-directed session outside Codex (for example Claude Code) names its exact
model and states its effort as reported by its host.

For substantive changes:

1. Create a fork issue with scope, affected interfaces, evidence plan, and
   acceptance criteria. Give each task a clear owner.
2. Use an isolated branch/worktree based on `rust-migration`. The implementation
   agent opens a draft PR targeting `rust-migration`, linking the issue and
   recording exact validation commands and limitations.
3. Assign a separate reviewer agent to examine the final commit and check evidence.
   Resolve findings, then review the resulting commit again before integration.
4. Root integrates after review and required checks, and records completion.

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

`python3 tools/migration.py verify` builds and tests the pinned original and fork,
checks that the candidate retains reference test names, and records evidence under
`.local/`. `python3 tools/migration.py build` builds both without running tests.
`python3 tools/migration.py tools` builds the native Rust generators. The optional
`--ccache` mode uses separate strict preprocessor caches and disables PCH; ordinary
builds remain the default. `--ccache-bypass` retains the same no-PCH build flags
while disabling cache reuse for measurement. Never cache test results or build
directories, and never clear another task's reference build for a cache trial.
The reference uses C++; the candidate explicitly enables `OPTION_RUST` and links
the migrated kernels into both the game and test executable.

Rust changes require reproducible `cargo fmt --all -- --check`,
`cargo check --workspace --all-targets --locked`,
`cargo clippy --workspace --all-targets --locked -- -D warnings`, and
`cargo test --workspace --locked` checks. The migration driver and CI enforce them.
Keep unsafe/FFI boundaries narrow and document safety, ownership, lifetimes, and
panic behavior. Make overflow and integer-conversion behavior explicit and faithful
to the original. Avoid unrelated C++ warning cleanup or blanket warning changes.

Builds, dependencies, toolchains, and reference worktrees belong in ignored
locations. Preserve upstream copyright notices, credits, and GPLv2.
