# OpenTTD-Rust near-term roadmap

Root owns selection here; `AGENTS.md` and `docs/rust-migration.md` define process.
If an issue conflicts, follow this roadmap and report it to root. Keep this
forward-looking, about 200 lines; completed work is one row, with evidence in PRs.

## Where the fork stands (2026-10-13, `62ef446d1e`)

- Twenty-three ownership ports. Whole fork against the pinned original: Rust
  55,082 / tooling 28,602 / C++ glue 24,602 / original C++ retired 36,162 (7.68%
  of all `src/` sources); simulation-source share 28,215 / 102,615 (27.50%, #226).
- #230 caps, #236 water fidelity and #240 coverage are integrated and cleaned up.
  No additional original code retired. Six branches include the two ownership
  prototypes and selected #243 cache-peak repair; reviews precede more integration.
- Current calibrated ratios: Opus 1.258, Grok 1.254, Padhattan-1996 1.275,
  Padhattan-2000 1.349, Padhattan-2016 1.427, TGP 1.164. TGP remains above
  1.15. Aircraft's final six-case medians pass; a matched Padhattan-2016
  diagnosis shows 4.51% improvement. Initial noisy samples remain in #238.
- #212 P0/P1 fail TGP: +28/+38 ms against 11 ms; no broad port starts.
  Select only R0, the planned shared raw tile-type read experiment (#108),
  retaining C++ arrays and native assertion policy. #213's tick prototype is active.
- #233 is incomplete: concurrent cache writes peaked at 10.37 GiB and evicted
  entries; root pruned duplicates. #243 selects bounded protected writers per
  family, without artifact input or candidate write privileges. Warm timing and
  three-merge acceptance repeat after repair; no target is waived.
- Open #156 gaps include threaded execution, NewGRF/refit callbacks, positive
  gradual loading and station acceptance polling. #240's new prospect/overlap
  evidence is integrated. No whole-game fidelity claim.

## Tenth steering review (2026-10-13, CI)

Corrections and work order:
1. **CI throughput first (#233).** #235's implementation is integrated. Complete
   live timing, concurrent-status and three-merge cache acceptance before closing
   #233. Runner size and account plan remain the user's decision; runner classes
   stay repository settings. Integrate reviewed work by readiness.
2. **#234 fixed (#236)**: industry water checks reject ship depots/locks.
3. **Reviewed corrections await integration.** #217 enters once before the empty
   queue check, uses shared accessors and removes capacity_remaining. #216 uses
   direct shared road functions/building counts. #230 uses all six new-base
   calibrations rather than minima. Their evidence remains in the PRs.
4. **Shared accessors in review (AGENTS.md).** Review reports name each new
   callback that duplicates a shared accessor; there are no boundary exceptions.
5. **Aircraft conversion (#199/#237/#238)** is active: the largest measured
   recoverable boundary cost (0.13 of Padhattan-2016's ratio).
6. **Budgets for #212 and #213.** TGP has about 11 ms of slack. #212's scope is
   about 177 of TGP's 352 reference ms over 14.4M tile visits: at most about
   0.8 ns extra per visit, and no Rust-to-C++-to-Rust crossing for tile types
   Rust owns. #213 has about 6 ns per vehicle-tick on Opus. Prototype each hot
   path under `perf` against its budget before building the port.
7. **New-component gate met:** #226 and #230 integrated; prototype #212/#213 first.

## Earlier steering, still in force

Standing steering: direct shared services (#107/#168/#199); measure before map
changes (#108); proportionate tooling (#109); prioritize simulation ownership.
The harness checks end moments (#154), default branch witnesses/reference probes
(#198/#204/#210/#211), speed ratchets (#155) and documented coverage gaps (#156).
The WIP cap is six (#157); full CI is exact-head/on-demand (#171), with no admin
bypass. Fresh reviewers fix their findings; integrated worktrees are removed,
never archived. State keeps the original process scope. Original retirement
excludes removed fork glue (#223); caps use three invocation medians and lower
only beyond measured spread. Ownership comes before the remaining conversions.
The #108 map decision retains C++ arrays with direct bundled services.
Use a fixed hasher for lookup-only YAPF maps; ABI blocks stay per component.

## Completed ownership ports

Metrics are `tools/port-metrics.py`: Rust / tooling / C++ glue / C++ retired.

| Issue | Component | PR | Commit | Metrics |
| --- | --- | --- | --- | --- |
| #73 | TGP terrain generation | #94 | `014968bd38` | 658 / 85 / 84 / 991 |
| #74 | Link graph job computation | #95 | `7a2858c469` | 1040 / 159 / 135 / 1120 |
| #96 | Built-in town names (retired count is mostly data tables) | #98 | `ff3648ba09` | 3296 / 241 / 122 / 3848 |
| #99 | Tree generation, simulation, planting | #102 | `602b5a60ff` | 1288 / 561 / 247 / 816 |
| #101 | Effect vehicles | #105 | `f6a0d66b63` | 494 / 553 / 412 / 548 |
| #104 | Water-region cache and graph service | #114 | `9732d8bfa2` | 459 / 610 / 273 / 407 |
| #103 | Disaster scheduling, vehicles and event control | #118 | `d5edfcb4c5` | 1404 / 795 / 306 / 968 |
| #117 | Cargo payment and delivery | #123 via #134 | `adbe063372` | 396 / 618 / 370 / 315 |
| #119 | Ship YAPF and canonical path cache | #126 via #134 | `adbe063372` | 881 / 54 / 506 / 646 |
| #120 | Town growth and private state | #127 via #134 | `adbe063372` | 1430 / 668 / 625 / 1548 |
| #121 | Road controller/private state (includes 1475 movement-data lines) | #128 via #134 | `adbe063372` | 3159 / 471 / 1188 / 2842 |
| #122 | Rail YAPF search, cache and reservation | #133 via #141 | `ea80a746a3` | 1704 / 144 / 595 / 1517 |
| #124 | Road YAPF search and path construction | #135 via #141 | `ea80a746a3` | 663 / 55 / 329 / 532 |
| #125 | Station cargo-service control and state | #142 | `ba920555ca` | 1516 / 241 / 1022 / 1332 |
| #130 | Rail controller, reservation and private state | #143 via #166 | `dace87c9b1` | 4087 / 613 / 1660 / 2585 |
| #129 | Industry production, histories and builder state | #144 via #166 | `dace87c9b1` | 1508 / 474 / 678 / 932 |
| #136 | Aircraft controller and airport blocks | #145 | `f0e0b3d712` | 1773 / 764 / 380 / 1880 |
| #137 | Company finance, economy and lifecycle | #149 | `dbce82a309` | 1607 / 702 / 1052 / 1394 |
| #139 | Cargo storage and movement | #151 via #184 | `2a76d5f287` | 2838 / 460 / 973 / 2445 |
| #138 | Orders lifecycle and commands | #176 via #184 | `2a76d5f287` | 4554 / 519 / 1752 / 2518 |
| #146 | Ship controller and private state | #152 via #184 | `2a76d5f287` | 1101 / 270 / 893 / 721 |
| #147 | Fleet groups and autoreplace | #197 | `a7a6e6650a` | 2686 / 577 / 1199 / 1454 |
| #150 | Industry construction and lifecycle | #218 | `648d44e940` | 3066 / 701 / 1433 / 1736 |

Harness and process: #72 harness (#85), #84 play saves (#87), #97 provenance
freeze (#100), #88 Ruff (#92), #75 partial-pixel fidelity (#91), #90 world-state
design (`docs/design/world-state.md`).

Earlier maintenance: #109/#112, #107/#113/#115, #86/#110/#132, #108/#116,
#154/#160, #158/#159, #155/#163, #156/#161/#162/#164/#167, #170/#171,
#173/#174, #178/#185/#187 in #184, #169/#175/#177/#180 in #181, #200/#201,
#188/#196, #191/#193, #198/#204, and the #168 conversions #190/#194,
#189/#195, #203/#205, #202/#208 with #206/#207 and #210/#211; evidence
remains in those PRs. Rows through
#215 use legacy retirement including deleted glue; new rows use original
retirement, with net glue in the PR (#223).
| Issues | Maintenance PRs | Integration | Commit | Metrics |
| --- | --- | --- | --- | --- |
| #199 (shared road services) | #215 | direct | `4b130309a5` | 2288 / 565 / 1331 / 1354 |
| #192 (company direct services) | #209 | direct | `8fe6f5ed56` | 1495 / 159 / 954 / -3; net glue +424 |
| #224 (disk hygiene) | #225 | direct | `898eb33c17` | 0 / 149 / 0 / 0; net tooling -546 |
| #227 (loading context fidelity) | #228 | direct | `9ee89f69ad` | 3 / 47 / 0 / 0 |
| #231 (OpenGFX cache) | #232 | direct | `21e2c9ea60` | 0 / 0 / 0 / 0; workflow net -6 |
| #223 (original-retirement metric) | #226 | direct | `86322c8422` | 0 / 274 / 0 / 0; net glue 0 |
| #233 (CI implementation; acceptance pending) | #235 | direct | `21eeb445be` | 0 / 1567 / 0 / 0; net glue 0 |
| #234 (industry water fidelity) | #236 | direct | `b7a1429d38` | 5 / 97 / 32 / 0; net glue +32 |
| #229 (six calibrated caps) | #230 | direct | `e69df25522` | 0 / 12 / 0 / 0; net glue 0 |
| #239/#156 (prospect/overlap coverage) | #240 | direct | `62ef446d1e` | 0 / 114 / 0 / 0; net glue 0 |

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness and speed maintenance

The default semantic harness runs in CI; regressions come first. Port differences
go in `KNOWN_FAILURES` with an issue, never in masks (`docs/rust-migration.md`).

1. **CI throughput (#233/#243).** Protected-only family writers, measured hard
   upload caps and retained warm fallback bound peak bytes across save waves.
   Candidate routes stay read-only; no artifact handoff. Takes capacity first.
2. **Speed ratchet (#155).** `SPEED_BUDGETS` in `tools/simulation/roads.py`
   holds the caps: the median of three `simulate <name> --benchmark 3 --jobs 2`
   invocations on an idle host. A PR may exceed a cap by 3% unless root accepts
   a stated reason that is not a boundary exception. A PR lowers a cap only when
   its median of three beats it by more than the invocation spread (about 2%).
   Targets: play saves 1.5x, generation 1.15x. The null driver redraws whenever
   1 ms has elapsed, so a slower candidate draws more frames.
3. **#156 coverage, standing capacity.** #239/#240 covers failed prospecting
   and automatic overlap rejection and is integrated. Random and crash branches first. The
   tenth review added station stale-link restriction, town station ratings,
   exclusivity and unwanted months, and #218's NewGRF and game-script paths.
4. **#234 fidelity fix (#236)** and **#229 caps (#230)** are integrated.
   Diagnose aircraft timing against these caps; do not select passing minima.

## Phase 2: current work, in order

Integrate reviewed work first, in order of readiness; at most six unintegrated
branches (#157). Independent items (#156, #223, caps) may run in parallel.

1. **#217 station/loading** needs the Windows macro fix review/checks, then full CI.
2. **#216 town, narrowed** needs shared-table conflict resolution against the
   integrated base and then #217, fresh narrow review and full CI; then town
   founding/generation, the house tile loop, and town commands, authority
   actions and deletion as their own ports.
3. **#199/#237 aircraft conversion (#238)** has passing final timing; review active.
4. **New ownership:** only #212 R0 shared map-read experiment and #213 vehicle
   prototype now; broad ports require the unchanged budgets. #212 R0 follows
   the linked plan on its issue, including layout/lifetime/default checks.
   Then #219 signal blocks and #220 engines (#220 after #213 and #148).
5. **Conversions:** train and ship after #213; the #168 tail (orders, effect,
   disaster, #218's accessors) never blocks ownership.

#108 retains C++ map arrays; measured #212 failure now selects one bounded raw
read backend, with one shared tile-type helper and callback fallback for unaudited
owners/fixtures. Prove native layout/allocation stability and serial access; no
Rust references/slices survive callbacks. GetTileType's assertion policy stays.
No storage transfer, unchecked timing proxy, new accessor copy or budget waiver.

## Resume checkpoint

Active continuous development. #240's tested tree is the merged tree; post-merge
cache refresh is expected. Earlier code post-merge runs pass. CI peak acceptance
failed and #243 is selected first. Landscape P0/P1 failed; only R0 is selected.

| Issue / PR | Branch, head | State and next step |
| --- | --- | --- |
| #214 / #217 | `station-loading-direct-214`, `de6266066a` | Fresh fixing review/native checks active, then full CI. |
| #148 / #216 | `town-lifecycle-148`, `3504fc4706` | Clean/pushed; final #217 adoption and fresh conflict review wait. |
| #237 / #238 | `aircraft-shared-boundary-237`, `a019d4279e` | All six caps pass; fresh fixing review active. |
| #212 / #241 | `landscape-budget-prototype-212`, `6b33b1f45a` | P0/P1 fail; count attribution, then selected R0 experiment. |
| #213 / #242 | `vehicle-budget-prototype-213`, `649aa1d9fb` | Cargo passes; native/Opus feasibility pending. |
| #233 / #243 | `ci-cache-peak-233`, pending | Selected protected-writer fix starts from integrated base. |

Next unstarted: #219/#220. ABI blocks: train through 385, tree additions 386-389,
fleet 390-399, company 400-429, town 430-459, road 460-499, station/loading 500-559,
industry 560-619 (572 shared ByteSpan), CommandCost 620-629, town lifecycle 630-669,
aircraft 670-739, vehicle scheduling 740-779, landscape 780-819. Only consumed layouts.

Preserve the pinned reference, paused curve worktrees and evidence branches
`evidence-disaster-vehicles` (`a775543162`) and `evidence-water-regions`
(`c752070cde`); do not reapply effect helper `73ccd511fb`. Build and test with
`--jobs 2`; standalone fixture games share the benchmark lock.

## Out of scope for the near term

More `src/3rdparty` ports (monocypher, squirrel, others). GUI rendering and
layout. Platform or toolchain expansion. Deferred behavior improvements stay in
`deferred-improvement` issues.

## Progress metric

Every port PR pastes `python3 tools/port-metrics.py`: Rust / tooling / C++ glue
added, original C++ retired, fork glue removed and net glue. Conversions report
net glue; `--whole-fork` gives totals and the share of simulation sources retired.
Retire more original C++ than net glue plus tooling, or explain why.
Data tables inflate retirement; judge progress
by game logic owned and avoid utility-kernel extraction.
