# OpenTTD-Rust near-term roadmap

Root owns selection here; `AGENTS.md` and `docs/rust-migration.md` define process.
If an issue conflicts, follow this roadmap and report it to root. Keep this
forward-looking, about 200 lines; completed work is one row, with evidence in PRs.

## Where the fork stands (2026-10-17, `8bfcbd3abd`)

- Twenty-nine ownership ports. Whole fork: Rust 64,552 / tooling 31,760 / glue 28,235 / original retired 40,145 of 470,612 `src/` lines (8.53%). Simulation: 32,218 / 102,615 (31.40%).
- This resumption integrated #268 save aliases, #273 train/orders cleanup, #277 native inline ClampTo, #279 retained road YAPF scratch, #270 rail commands and #257 vehicle scheduling/maintenance.
- Rail commands retire 1,578 original C++ lines and vehicles 388. #268 restored 44 original lines while removing save staging. The two remaining ownership drafts are #269 houses (891) and #255 landscape (844).
- Vehicle CPU evidence passes; root accepted its small 2016 cap overrun within earlier measured spread. Landscape's selected #274 boundary exception and TGP cap 1.1875 remain draft evidence. No production boundary view is adopted.
- Coverage gaps remain #156. #274 measurements are recorded and paused; two component drafts and no maintenance drafts remain. Development is paused at the user's request.

## Thirteenth steering review (2026-10-16, unblock the core loops)

Independent audits of #270, #269, #268 and #273 found **no reachable divergence**. They did find:

- **Deadlocked deferrals.** Neither deferred core loop can return.
  - #212 waits for a map-read measurement that nobody owns. #255's remaining cost is the per-tile map boundary: `RustClearSurface` alone is 275M Ir.
  - #213 waits for the #168 conversions, which waited for #213. They also cannot make base `Vehicle` fields inline. Its last miss (6.05 ns against 6) was within its measured spread.
  - The tile loop and vehicle tick are the centre of the simulation.
- **Shared-service copies that CI cannot see.**
  - #270 re-wraps `CheckOwnership` and `EnsureNoVehicleOnGround`, which fleet and industry already wrap, and re-reads road map accessors through compound callbacks.
  - #269 has three accessors for each of several house spec fields.
  - Reviewers missed these, as they missed #258's founding table.
  - #266 and #267 would copy the same command checks again.
- **Whole-record copies.** #269 copies the 12-field `HouseTile` at 17 sites and the 8-field `HouseBehaviour` at 4.
- **Unsound types.** #273 makes 113 orders callbacks that take raw pointers safe `extern "C" fn`.
- **The speed plan is mis-ordered.**
  - Road costs Opus +3.07G Ir (controller) and +1.31G (YAPF); aircraft costs nothing there.
  - `ClampTo` calls a Rust utility kernel on a hot path (+0.52G, about 9% of the Opus gap).
  - Road YAPF allocates and rehashes on every search.
  - Trees are the largest TGP item (+222M) and appear nowhere in the plan.

Actions, in order:
1. **Integrate the drafts with the must-change items from their audit comments.**
   - #268: re-add layout 60 in the resolution.
   - #273: restore `unsafe` on fields that take pointers, then verify at the head.
   - #270: shared typed command checks, deleting the fleet and industry copies; road accessors through the shared set; one `IsSafeWaitingPosition`; real return types.
   - #269: the R1 fix round, extended to `HouseBehaviour`, one accessor per spec field, and the owner resolved once.
2. **Reopen #257 and #255 alongside item 1** (user decision). #213's miss is within spread. #212 integrates over budget once its profile pins the excess on map reads that #274 removes. Each PR updates from base, gets a fresh review and Full validation, and raises its caps to the measured medians.
   **#274, one agent, alongside.** Measure a read-only map view on #255's clear and water reads, and a post-tick vehicle view plus direct controller calls on `vehicle-owner-213`. If a view clears its gate, amend world-state.md and land the view as one shared PR that #212 and #213 adopt.
3. **#275 cheap items alongside, as maintenance PRs.** Remove the `ClampTo` call to Rust and keep one road YAPF arena. Then fold road `GetCurrentMaxSpeed`, then convert road to a C-layout block, then the TGP items (industry `next_tick`, trees). Aircraft comes last.
4. **#266 starts after #270 integrates,** on its shared command checks; then #267 and #247.

## Earlier steering, still in force

- **Housekeeping (twelfth):** #259 H1-H4, #260 and #261 R1/R4 (#268) are done. The reviewed R3 train/orders cleanup (#273) is integrated. Remaining R2 waits for #269/#270, and H5 comes last. At most three maintenance PRs are in flight. They never count against the component cap or block an ownership port.
- **Direct shared services and boundaries:** direct shared services (#107/#168/#199) and one definition per shared accessor, enforced in quick CI by #249, whose baseline only shrinks. Use a fixed hasher for lookup-only YAPF maps; ABI blocks stay per component.
- **Ownership:** prioritize simulation ownership. Keep state at the original's process scope.
- **Map storage:** #108 retains C++ map arrays with direct bundled services until #274 measures a read-only view.
- **Harness evidence:** end moments (#154), default branch witnesses and reference probes (#198/#204/#210/#211), speed ratchets (#155) and documented coverage gaps (#156).
- **Tooling and WIP:** proportionate tooling (#109) and a stated budget for infrastructure work. Prefer platform defaults to controllers (#233). The WIP cap is six component branches.
- **CI and review:** full CI is exact-head and on demand (#171), with concurrent validation (#233) and no admin bypass. Fresh reviewers fix their findings.
- **Metrics and caps:** original retirement excludes removed fork glue (#223). Caps use three invocation medians and are lowered only beyond measured spread.

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
| #148 | Town canonical state and periodic policy (narrow scope) | #216 | `59385bce59` | 1848 / 694 / 1418 / 340 |
| #220 | Engine lifecycle and canonical design state | #248 | `d2115d1c3d` | 1218 / 369 / 833 / 579 |
| #219 | Rail signal blocks and fixed work sets | #245 | `9487f3079f` | 1229 / 812 / 445 / 646 |
| #246 | Town founding, generation and names | #258 | `09bb0b4df8` | 1123 / 321 / 470 / 492 |
| #265 | Rail track, depot and signal commands | #270 | `0cb20cec28` | 2965 / 301 / 740 / 1578 |
| #213 | Vehicle scheduling and base maintenance | #257 | `8bfcbd3abd` | 1833 / 375 / 1160 / 388 |

Harness and process: #72 harness (#85), #84 play saves (#87), #97 provenance
freeze (#100), #88 Ruff (#92), #75 partial-pixel fidelity (#91), #90 world-state
design (`docs/design/world-state.md`). Maintenance PRs #209-#254 (shared
services, CI throughput, disk hygiene, caps, #249 enforcement) keep their
evidence in their PRs; rows below use original retirement and net glue (#223).

| Issues | Maintenance PRs | Integration | Commit | Metrics |
| --- | --- | --- | --- | --- |
| #260 (ScriptList overflow fidelity) | #262 | direct | `c54129895b` | 33 / 3 / 0 / 0; net glue 0 |
| #259 H3 (inert workflows) | #263 | direct | `7feb00248f` | 0 / 0 / 0 / 0; workflow/comment net -2142 |
| #259 H4 (dead FFI, orphan guards) | #264 | direct | `af1abf96b4` | 6 / 174 / 12 / 4; net glue -75 |
| #261 R1 (tests, one ABI layout table) | #271 | direct | `15e0ae1390` | 3 / 171 / 189 / 0; net glue -405 |
| #156 (no-city towns, stale-link restrict/release) | #272 | direct | `6ffbf8f302` | 99 / 20 / 0 / 0; net glue 0 |
| #261 R4 (direct save aliases) | #268 | direct | `5b3c666698` | 7 / 4 / 35 / -44; net glue -204 |
| #261 R3 part (train/orders forwarders and unsafe contracts) | #273 | direct | `bdab12a31b` | 1571 / 0 / 0 / 0; Rust net -532, net glue 0 |
| #276 (#275 native inline ClampTo) | #277 | direct | `08e5372c8f` | 0 / 7 / 0 / 0; Rust net -44, net glue -33 |
| #278 (#275 retained road YAPF scratch) | #279 | direct | `35d872ca3c` | 86 / 11 / 0 / 0; net glue 0 |

Paused, not fallbacks: #64 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness and speed maintenance

The default semantic harness runs in CI; regressions come first. Port differences
go in `KNOWN_FAILURES` with an issue, never in masks (`docs/rust-migration.md`).

1. **Shared-accessor check (#249).** A new per-port copy fails quick CI. The baseline of existing copies only shrinks; #259 H4 extends the check to orphaned exports and duplicate table bindings.
2. **Speed ratchet (#155).**
   - **Caps:** `SPEED_BUDGETS` in `tools/simulation/roads.py` holds them. Each cap is the median of three `simulate <name> --benchmark 3 --jobs 2` invocations on an idle host.
   - **Exceeding a cap:** a PR may exceed a cap by 3%, unless root accepts a stated reason that is not a boundary exception. Core loops follow the AGENTS.md boundary rule instead.
   - **Lowering a cap:** a PR lowers a cap only when its median of three beats it by more than the invocation spread, about 2%.
   - **Targets:** play saves 1.5x, generation 1.15x.
   - **Misses:** attribute any miss with callgrind before changing scope (AGENTS.md).
   - **Redraw inflation:** the null driver redraws whenever 1 ms has elapsed, so a slower candidate draws more frames, which inflates play-save ratios. Say so in a PR when it decides a miss.
3. **#156 coverage, standing capacity.** Random and crash branches come first. These remain:
   - signal tbd/global propagation overflow and arbitrary mod layouts;
   - town exclusivity, unwanted months and authority refusals;
   - engine external NewGRF/retire_early and legacy save branches;
   - #218's NewGRF and game-script paths;
   - the #268, #269 and #270 audit gaps (#156 comment, 2026-10-16).

## Phase 2: current work, in order

Integrate reviewed work first, in order of readiness, with at most six
unintegrated component branches. Maintenance (#259, #261, #275) and #156 run in parallel.

1. **Drafts first:** #269 houses and #255 landscape remain paused at the heads/blockers below. #270 and #257 are integrated. Resume their required base resolution/review/checks before starting another component.
2. **Core-loop boundary work (#274):** #213 is integrated; #212 remains draft under the selected exception. Measurement branches stay unchanged; no production view, raw-offset vehicle path or base-scalar extraction is selected.
3. **#275 speed:**
   - the cheap items are integrated (#277 native `ClampTo`, #279 road YAPF arena);
   - then road `GetCurrentMaxSpeed` and the road C-layout block;
   - then industry `next_tick` and the trees facade;
   - then aircraft.
4. **Commands:**
   - #266 road commands after #270, reusing its shared command checks and crossing services;
   - #267 station construction on the #125 owner;
   - #247 town commands, authority and deletion.

   Hot per-vehicle procs stay native. Tunnel/bridge follows once #247 and #212 settle.
5. **#168 tail, largest first.** `order_cmd.cpp` and `economy.cpp` hold 65 of the 198 baseline copies. Train, ship, effect and cargo-packet C-layout blocks wait for nothing.

New owners reuse integrated services, never draft copies. A Rust-allocated
state block with C++ reference aliases (#216, #248) is acceptable. Keep save
staging only for a field a `SaveLoad` descriptor cannot address through the alias.

## Resume checkpoint

Paused at the user's request (2026-10-17). Root is gpt-6.1-sol, xhigh.
All three scoped reviewers finished. #257 integrated first by readiness; later
source conflicts stop both remaining drafts. No further fixes, reviews or timing
start during wind-down. The selected work order above is unchanged; #275's next
items, #266, #267, #247, R2 and H5 remain unstarted.

| Issue / PR | Branch, head | State and next step |
| --- | --- | --- |
| #256 / #269 | `house-lifecycle-256`, `e65bfcccc9` | R3/root verification and final-head Full pass; later #257 base conflicts in witness.rs. Remains draft. On resumption resolve witness allocation against current base, scoped review/affected checks, fresh Full. |
| #212 / #255 | `landscape-owner-212`, `13db617a86` | Draft: Windows annotation warnings plus five source/tool base conflicts. On resumption fix conversions/base resolution, review, affected comparisons/semantics and Full. Current-head comparisons/semantics were not launched after the gate stop. |
| #274 | `measurement-274-vehicle`, `1037723bc7` | Measurement only, unchanged; root owns future boundary work. No production adoption or new timing selected. |
| #64 / #66 | remote `issue/64-curve-family`, `aab5211801` | #66 closed unmerged; #64 and older paused work remain untouched. |

Merged rail/vehicle worktrees and merged leftover `t3/*` branches were removed.
Unmerged worktrees, branches and `.local` caches are preserved. Post-merge CI
is green; exact runs and evidence stay on the PRs.

ABI blocks:
- train through 385, tree additions 386-389, fleet 390-399
- company 400-429, town 430-459, road 460-499
- station/loading 500-559, industry 560-619 (572 is the shared ByteSpan), CommandCost 620-629
- town lifecycle 630-669, aircraft 670-739
- vehicle 740-779, landscape 780-819, signals 820-859, engines 860-919
- house lifecycle 920-979, rail commands 980-1019, road commands 1020-1059, station construction 1060-1119

Check only consumed layouts.

Preserve the pinned reference. Do not reapply effect helper `73ccd511fb`.
Build and test with `--jobs 2`; standalone fixture games share the benchmark lock.

## Out of scope for the near term

More `src/3rdparty` ports, GUI rendering/layout and platform/toolchain expansion.
Deferred behavior improvements stay in `deferred-improvement` issues.

## Progress metric

Every port PR pastes `python3 tools/port-metrics.py`: Rust/tooling/glue added, original retired, fork glue removed and net glue.
`--whole-fork` gives totals and simulation share. Retire more original C++ than net glue plus tooling or explain why.
A port that misses this ratio gets a boundary fix (shared services, #274 views) before deferral.
Judge game logic owned; data tables inflate retirement. Avoid utility kernels.
