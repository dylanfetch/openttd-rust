# OpenTTD-Rust near-term roadmap

Root owns selection here; `AGENTS.md` and `docs/rust-migration.md` define process.
If an issue conflicts, follow this roadmap and report it to root. Keep this
forward-looking, about 200 lines; completed work is one row, with evidence in PRs.

## Where the fork stands (2026-10-13, `3b3b931099`)

- Twenty-three ownership ports. Whole fork against the pinned original: Rust
  55,453 / tooling 28,635 / C++ glue 25,307 / original C++ retired 36,162 (7.68%
  of all `src/` sources); simulation-source share 28,215 / 102,615 (27.50%, #226).
- #230/#236/#240/#217 are integrated and cleaned up. The fourth integration
  leaves original retirement unchanged; five branches remain. Finish the bounded
  ownership experiments or defer them, then #219 before further conversions.
- Current calibrated ratios: Opus 1.258, Grok 1.254, Padhattan-1996 1.275,
  Padhattan-2000 1.349, Padhattan-2016 1.427, TGP 1.164. TGP remains above
  1.15. Aircraft's final six-case medians pass; a matched Padhattan-2016
  diagnosis shows 4.51% improvement. Initial noisy samples remain in #238.
- #212 P0/P1 fail TGP: +28/+38 ms against 11 ms; only R0 shared raw tile-type
  reads are selected. #213 P0 fails: about 16.8 ns/visit against 6 ns and above
  the wall cap. Select only the two-phase scalar-address experiment below.
- #233 is incomplete: concurrent cache writes peaked at 10.37 GiB; missing
  entries suggest eviction. Root pruned duplicates. #243 selects protected writers per
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
| #214/#168 (typed station/loading) | #217 | direct | `3b3b931099` | 1018 / 42 / 1147 / 0; net glue +705 |

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
3. **#156 coverage, standing capacity.** Random/crash branches first; station
   stale links, town station ratings/exclusivity/unwanted months and #218's
   NewGRF/game-script paths remain. #239/#240 prospect/overlap checks are integrated.

## Phase 2: current work, in order

Integrate reviewed work first, in order of readiness; at most six unintegrated
branches (#157). Independent items (#156, #223, caps) may run in parallel.

1. **#216 town, narrowed** adopts integrated #217, with fresh conflict review
   and full CI; then town
   founding/generation, the house tile loop, and town commands, authority
   actions and deletion as their own ports.
2. **#199/#237 aircraft (#238)** passed full CI; fresh #217 conflict review runs.
3. **New ownership:** only #212 R0 shared map-read experiment and #213 vehicle
   prototype now; broad ports require the unchanged budgets. #212 R0 follows
   the linked plan on its issue, including layout/lifetime/default checks.
   Then #219 signal blocks and #220 engines (#220 after #213 and #148).
4. **Conversions:** train and ship after #213; the #168 tail (orders, effect,
   disaster, #218's accessors) never blocks ownership.

#108 retains C++ map arrays; measured #212 failure now selects one bounded raw
read backend, with one shared tile-type helper and callback fallback for unaudited
owners/fixtures. Prove native layout/allocation stability and serial access; no
Rust references/slices survive callbacks. GetTileType's assertion policy stays.
No storage transfer, unchecked timing proxy, new accessor copy or budget waiver.

#213 selects two shared typed native scalar-address bundles, only for post-tick
cargo and motion/sound phases, following the linked source/lifetime plan on #213.
Rust dereferences fields at original branches; status stays a typed service.
No Vehicle/STL layout view, offsets, storage transfer, reference across callback,
joined phase record or timer change. Preserve native assertion configurations.
Measure crossings/bytes and matched CPU/wall time; both 6 ns and current cap must
pass. Failure defers this owner; broad maintenance remains unstarted.

## Resume checkpoint

Active continuous development. #217 passed full CI and is merged/cleaned; its
post-merge native policy hit a GitHub API failure and root reran it. More merges
wait for main recovery. #243 is first; each failed owner has one bounded revision.

| Issue / PR | Branch, head | State and next step |
| --- | --- | --- |
| #148 / #216 | `town-lifecycle-148`, `3d1164d918` | #217 adoption pushed; fresh conflict checks active. |
| #237 / #238 | `aircraft-shared-boundary-237`, `ca601fd547` | Earlier full passed; #217 conflicts resolved; fresh checks active. |
| #212 / #241 | `landscape-budget-prototype-212`, `650b55321a` | R0 native checks; fixture reentry fix pending before timing. |
| #213 / #242 | `vehicle-budget-prototype-213`, `d1d2e4bad0` | Native/semantics pass; P0 budget fails; scalar-address revision selected. |
| #233 / #244 | `ci-cache-peak-233`, `85dd778eb1` | #243 protected-only two-slot publisher in progress; bootstrap after review. |

Next unstarted: #219/#220. ABI blocks: train through 385, tree additions 386-389,
fleet 390-399, company 400-429, town 430-459, road 460-499, station/loading 500-559,
industry 560-619 (572 shared ByteSpan), CommandCost 620-629, town lifecycle 630-669,
aircraft 670-739, vehicle scheduling 740-779, landscape 780-819. Only consumed layouts.

Preserve the pinned reference, paused curve worktrees and evidence branches
`evidence-disaster-vehicles` (`a775543162`) and `evidence-water-regions`
(`c752070cde`); do not reapply effect helper `73ccd511fb`. Build and test with
`--jobs 2`; standalone fixture games share the benchmark lock.

## Out of scope for the near term

More `src/3rdparty` ports, GUI rendering/layout and platform/toolchain expansion.
Deferred behavior improvements stay in `deferred-improvement` issues.

## Progress metric

Every port PR pastes `python3 tools/port-metrics.py`: Rust/tooling/glue added,
original retired, fork glue removed and net glue; `--whole-fork` gives totals and
simulation share. Retire more original C++ than net glue plus tooling or explain
why. Judge game logic owned; data tables inflate retirement. Avoid utility kernels.
