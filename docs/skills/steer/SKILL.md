---
name: steer
description: Steering review of root's autonomous migration work since the last steering commit; audits, corrects docs/issues, refreshes /start-development.
disable-model-invocation: true
---

# Steer the OpenTTD-Rust migration

The user runs root (`gpt-6.1-sol` xhigh in Codex; Astra before 2026-10-08) for
long autonomous sessions, and runs this review in Claude Code between them. Your
job is to check that root is still on the path to a game whose simulation runs in Rust, find what it
cannot see in its own work, and correct the *system*: rules, roadmap order,
issues and PR comments. Root's team does the implementation. You edit docs,
issues and PR comments, and you leave code changes to root's PR process.

User focus for this run, if any: $ARGUMENTS

Attribution on every issue, comment and commit body you write:
`Agent: /root (Claude Code steering review) | Model: <your exact model ID> | Reasoning effort: <as reported by host, else "not reported to the agent by host">`.

## 1. Load the last steering

1. Find the last steering commit: `git log --format='%h %ad %s' --date=short | grep -i 'Doc: .*Steer'`.
   Its message and the roadmap's "steering review" section state the standing
   corrections.
2. Read the last steering session's transcript. `python3 docs/skills/steer/transcript.py`
   lists sessions newest first. The steering ones open with `/steer` or a request
   to review root (Astra or Sol). Skip unrelated sessions, but read the conclusion
   of any session in between that changed a rule. `transcript.py <id>` prints one session; read
   its final messages first.
3. Read `docs/skills/start-development/SKILL.md`, the restart prompt root
   last ran, and `AGENTS.md`. Root's own Codex sessions are under
   `~/.codex/sessions/YYYY/MM/DD/`; its session opens with `$start-development`.
   Read the user's messages to it, and how it ended.

Done when you can list each standing correction and the issue that tracks it.

## 2. Survey root's work since then

Run these together:

- `git fetch`, then `git log` from the steering commit to `origin/rust-migration`, and
  `git diff <steer>..origin/rust-migration -- AGENTS.md docs/ .github/`.
  A loosened rule is a red flag.
- `docs/roadmap.md` in full, including its resume checkpoint.
- CI on `rust-migration` (`gh run list --branch rust-migration`). Note PR checks and
  how many runs were cancelled.
- Open PRs and issues, and the latest review reports on the integrated and pending PRs.
- `python3 tools/port-metrics.py`, plus the PR-reported metrics and speed ratios.
- `git worktree list` and disk use.

Then post a short status line to the user and continue.

Judge each standing correction: applied, slipped, or reinterpreted. Watch for the
patterns root has repeated before:

- **Wheel-spinning**: evidence, tooling or process growing faster than retired game logic.
- **Sediment**: roadmap and docs turning into logs (run IDs, hashes, probe values).
- **Slippage**: a correction quietly reordered behind other work.
- **Template drift**: one port's boundary pattern copied into every later port.
- **WIP creep**: stacked or conflicting branches, and dependency-merge churn.
- **Finite prompts**: the last prompt ended because its listed items ran out.

## 3. Audit independently

Root's reviewers share its model family and usually report "Findings: none".
The value of this session is an audit that does not depend on them. Launch these
in one message as background read-only subagents (Claude Opus):

- One per large port integrated since the last steering, or about to integrate.
  Pick the largest by retired lines first, up to four.
- One performance agent: run `python3 tools/migration.py simulate <scenario>
  --benchmark 3 --jobs 2` on the play saves and generate-tgp-256-1, and profile
  the candidate against the reference. Attribute the gap by port, then judge the
  current speed plan against the profile.

Give each audit agent the worktree, commit and merge base, `AGENTS.md`, and the
pinned original at `.local/reference/openttd`. Ask it to compare against the
original line by line:

- **Ownership**: Rust owns the state and the control loop, and the original body
  sits under `#else` or `#ifndef WITH_RUST`.
- **Fidelity**: the exact count and order of Random draws, integer widths,
  wrapping and saturation, early returns, and the order of side effects.
- **Added fatal paths**: asserts, unwraps, indexing or shifts that can panic where
  the C++ does not.
- **Boundary cost**: the `AGENTS.md` direct-call and typed-boundary rules, and
  per-call allocation on hot paths.
- **Glue**: its share against retired lines.
- **Coverage**: which Random, crash and contention branches the harness reaches.

Each agent reports under about 70 lines. Tag findings REACHABLE-DIVERGENCE,
UNREACHABLE, RULE-VIOLATION, COVERAGE-GAP or PERF, with `file:line` in both
Rust and the original, and name what it compared. Relay a short summary to the
user as each audit returns. Before acting on a load-bearing claim, such as red CI,
a reachable divergence or a perf attribution, check it yourself.

Done when every audit has reported and every claim you will act on is verified.

## 4. Correct course

Choose a few high-leverage corrections. A systemic pattern counts for more than
a one-off finding. Each correction lands in exactly one authoritative place:

- **Rule change** goes in `AGENTS.md`, phrased as the target behavior.
- **New work** gets an issue, about 60 lines at most: problem with measurements,
  scope, evidence and acceptance. Label it `roadmap` when the roadmap orders it.
- **Finding on a pending PR** goes in a PR comment: what must change before
  integration, and what can follow.
- **New coverage gaps** go in a comment on the coverage tracker (#156).
- **Roadmap**: rewrite "Where the fork stands" and the work order. Add an "Nth
  steering review" section with findings and corrections. `/start-development`
  tells root to apply that section first, so state each action plainly. Fold
  older steering sections into the compact "still in force" paragraph. Keep the
  file under the `AGENTS.md` budget, moving per-PR plans to their PRs or issues.

Commit `AGENTS.md`, the roadmap and any restart-prompt edit (step 5) as one
docs-only commit pushed straight to `rust-migration`, with a title like
`Doc: Steer migration toward <corrections> [skip ci]`. Earlier steering commits
landed the same way. Confirm `origin/rust-migration` has it.

## 5. Check /start-development

`docs/skills/start-development/SKILL.md` is root's restart prompt. It stays
unchanged by default: it sends root to the roadmap's latest steering review,
and that section carries this review's corrections. Edit the prompt only when
this review shows it needs improving, for example because root stopped early,
misread it, or needs a new standing step in its work loop. Keep it open-ended:
finishing the listed items triggers planning the next work, and only the user
ends the assignment. Include any edit in the step 4 commit.

## 6. Report

Tell the user:

- the verdict on root's track;
- what the audits found, with numbers;
- what you changed, with issue and commit links;
- anything only the user can supply, such as a new play save or a policy decision.

Finish by telling them to start root with `/start-development` in Codex, and
whether this review changed it. The prompt lives in the skill, not in your reply.
