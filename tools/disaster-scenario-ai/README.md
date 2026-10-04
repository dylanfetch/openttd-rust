# Disaster evidence checkpoint (#103)

Agent: /root/town_name_evidence | Model: gpt-6.1-sol | Reasoning effort: high

This is a tested reference-probe AI, not an integrated simulation scenario.
`parameters.nut` must define `DISASTER_SETUP` (bool), `DISASTER_ACTION`
(`none`, `industry`, `road-sale`, or `large-airport`), and `DISASTER_TARGET`.
The rail and airport actions use assets in `regression/stationlist/test.sav`.
Industry demolition requires the declared magic-bulldozer input setting.

The unchanged reference accepted rail setup, sale of road vehicle 12,
demolition of refinery 3, and replacement of airport 0 with a large airport.
Its observer logged zeppelin crash/clear, a small-UFO road crash, and industry
closure. Probe scripts, saves, logs, hashes, and tick receipts are retained in
`.local/disaster-probes/` in the disaster evidence worktree.

Before acceptance, integrate immutable inputs and this AI into `scenario_list`,
validate every allowed input edit against exact schemas and unrelated bytes,
assert branch outcomes and elapsed ticks in each runtime mode, and run
reference-self, negative probes, candidate comparisons, and soak checks.
The current probes do not complete all fifteen controllers, live target reload,
human-train big-UFO landing/destruction, scheduler boundaries, or map policy.
