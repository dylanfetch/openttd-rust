<!-- Target rust-migration. About 60 lines; see the evidence budget in AGENTS.md. -->

Closes #<issue>. Roadmap item: <phase / issue>.

Agent: <agent path> | Model: <exact model> | Reasoning effort: <effort>

## Moved to Rust

<!-- The state and control flow Rust now owns. One short paragraph or list. -->

## Stays in C++

<!-- The facade and callbacks that remain, and why each one is needed. -->

## Checks

<!-- Exact commands run, and their results. Harness scenarios that exercise this component. -->

## Simulation speed

<!-- Play-save benchmark before/after candidate/reference median wall ratios, commits and exact command; explain regressions above the roadmap budget. For non-simulation changes, say not applicable. -->

## Metrics

<!-- Paste python3 tools/port-metrics.py. Judge ownership by original C++ retired; conversions report net glue. If original retirement is less than net glue plus tooling, explain why. -->

## Known limits

<!-- Divergences, uncovered input domains, and follow-up issues. "None" is a valid answer. -->

<!--
Review report (reviewer posts it as a separate comment, about 20 lines):
Agent: <path> | Model: <exact model> | Reasoning effort: <effort>
Reviewed commit: <sha>
Findings and fixing commits: <finding -> commit, or why it went to root; or "none">
Shared credentials require an attributed report, not a GitHub platform approval.
-->
