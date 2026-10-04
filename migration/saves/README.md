# Simulation harness saves

Fixed inputs for `tools/simulate.py` (`play-*` scenarios). They are not
expected results. Each is the final game of one episode in the sibling
play-openttd project: an LLM player built a road network on the same 128x128
map through an MCP server, using OpenTTD 15.1. The player's company is an AI
company driven by a bridge AI that is missing from the harness runtime, so on
load the idle dummy AI replaces it; AI-company rather than human-company logic
is exercised. No save has a GameScript or NewGRFs. Every save was written
paused; the scenario's `console` lines unpause it, and the harness checks that
their settings appear in the first snapshot.

| Save | Episode |
| --- | --- |
| `opus-55-167-002.sav` | 167-game-time-variance-opus-55-xhigh-episode-002 |
| `grok-159-001.sav` | 159-game-time-variance-grok-episode-001 |
| `astra-156-003.sav` | 156-game-time-variance-astra-episode-003 |
| `opus-55-165-002.sav` | 165-game-time-variance-opus-55-episode-002 |
| `opus-5-133-009.sav` | 133-game-time-variance-opus-episode-009 |
| `fable-152-004.sav` | 152-game-time-variance-fable-episode-004 |

Road vehicles only. Rail, ship and aircraft scenarios are #86.
