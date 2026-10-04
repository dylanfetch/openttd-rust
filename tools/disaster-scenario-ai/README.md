# Disaster scenario AI (#103)

This harness AI builds the stationlist fixture's rail extension, parks the real
front train, replaces its small airport with a large airport, executes target
removal commands, and observes queued disaster events. The harness freezes its
sources before workers start and supplies `parameters.nut` for each invocation.

Parameters: `DISASTER_SETUP` (bool), `DISASTER_ACTION` (`none`, `industry`,
`road-sale`, `large-airport`), and `DISASTER_TARGET` (tile or vehicle ID).
All actions use the existing assets in `regression/stationlist/test.sav`.
Industry demolition requires the declared magic-bulldozer input setting.

Run through `python3 tools/migration.py simulate disasters`; source-only setup
and event checks live in `tools/simulation/disasters.py`.
