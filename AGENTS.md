# OpenTTD-Rust

This independent experimental fork incrementally replaces C++ with Rust while
preserving the original game's observable behavior. The original game supplies
the specification, including historical quirks. Record possible improvements in
fork GitHub issues for consideration after near-full Rust reproduction.

Read `docs/rust-migration.md` for setup, validation, and project process.
`migration/baseline.json` pins the original revision. `README.md` identifies the
fork; the remaining upstream documentation explains behavior and architecture.

## Migration method

- Choose bounded components or coupled groups from actual dependencies and test
  coverage. Prioritize heavily tested modules whose unchanged upstream tests can
  exercise Rust through the existing interface. Investigate before choosing the
  first implementation; no subsystem or transport mode has a special priority.
- Keep the full game running throughout migration. Preserve networking, saves,
  mods, interface, and shared random-number behavior.
- Reuse existing tests first. Identify concrete behavior gaps before adding narrow
  comparisons against unchanged reference functions or the reference executable.
  There is no mandatory up-front simulation harness or blanket requirement to
  build a new differential test for every port.
- Keep the pinned reference worktree unchanged. Never change candidate behavior
  and expected results together merely to make checks pass. Existing test success
  establishes covered behavior, not complete game equivalence.
- When simulation comparisons become necessary, compare semantic state; screenshots
  alone and compressed-save byte equality cannot establish equivalent simulation.
  Retain small failures and explain discrepancies before accepting changes.
- Record changes, reproducible checks, and remaining limits in PRs and migration
  documentation. Scaffolding does not complete a subsystem.

## Agent team and review

The root agent orchestrates and delegates heavily, owns component selection and
integration, and verifies delegated diffs and evidence. Target concurrency is nine
agents total: the root plus eight subagents. Respect the running host's actual
limit; this document cannot raise a session limit.

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

For substantive changes:

1. Create a fork issue with scope, affected interfaces, existing test evidence,
   behavior gaps, and acceptance criteria. Give each task a clear owner.
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

`python3 tools/migration.py verify` builds and tests the pinned original and fork,
checks that the candidate retains reference test names, and records evidence under
`.local/`. `python3 tools/migration.py build` builds both without running tests.
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
