# OpenTTD-Rust near-term roadmap

Root owns selection here; `AGENTS.md` and `docs/rust-migration.md` define process.
If an issue conflicts, follow this roadmap and report it to root. Keep this
forward-looking, about 200 lines; completed work is one row, with evidence in PRs.

## Where the fork stands (2026-10-14, `334df5ef8c`)

- Twenty-four ownership ports. Whole fork: Rust 56,978 / tooling 29,319 / glue 26,077 / original retired 36,502 of 470,612 `src/` lines (7.76%). Simulation: 28,522 / 102,615 (27.80%); source counts, not behavioral completion.
- Since the tenth review, six integrations retired 340 original lines (all #216) and added 2,709 tooling and 1,507 glue lines. Both large owners (#212, #213) were deferred after prototype timing failures; #244 grew to 1,644 tooling lines.
- Speed at tip (eleventh-review audit, median of three): TGP 1.123, Opus 1.250, Grok 1.245, Ridge 1996 1.225, 2000 1.293, 2016 1.344. All are inside their caps.
- Drafts: #245 signals and #248 engine, both faithful with listed fixes. #244 is not to be worked on (action 1). #66 is paused.
- Largest original simulation files remaining: station_cmd 4.5k lines, rail_cmd 3.2k, vehicle 3.0k, road_cmd 2.7k, town_cmd 2.5k, tunnelbridge 2.2k, landscape 1.8k, water 1.5k, engine 1.3k.

## Eleventh steering review (2026-10-14, throughput and enforcement)

Independent audits of #216, #245, #248 and #217/#238 found no reachable divergence.
The ports are faithful. Retirement stalled for three reasons, each fixable:

- **#212/#213 failed on fixable implementation costs, not FFI cost.**
  - #241 R0 profile: the per-tile closure in `sequence` (`tile_loop.rs`) is not inlined (47 instructions per visit against C++'s 18). The `noexcept` lambdas at `landscape.cpp:832-840` add a call frame per native tile. Fixing both measured 402 ms against base 406 ms on TGP.
  - #242 P1's +11.5 ns per vehicle visit is mostly a pool rescan in `VehicleSchedulerNext` on every call, a large dispatch struct, an assert crossing and timing wrappers (`vehicle_services.cpp:224-288`). About 9 ns is estimated recoverable.
- **The duplicate-accessor review rule did not hold.** There are about 122 meanings with about 286 extra definitions tree-wide, yet the final reviews of #216, #217 and #238 each said "none". #249 makes this a CI check.
- **CI tooling grew.** #233's wall-time and concurrency goals are met: 13.8 min for a small PR, 17-19 min for ports. Its cache-peak criterion produced #243/#244 (1,644 tooling lines) and stays open.
- **Measured costs at tip:**
  - On TGP, the tree facade adds 16.8 ms and industry `next_tick` adds 13.2 ms.
  - Aircraft make 94 C++ calls per aircraft tick; road vehicles make 12.6 per tick.
  - On play saves, the null driver redraws per elapsed millisecond, which inflates game-logic cost about 1.33x in wall ratios.

Actions, in order:
0. **#252 Windows CI on Blacksmith (user-selected, first).** Follow the issue: the first Blacksmith Windows run decides breakpad compatibility; if incompatible, leave Windows on GitHub, close it and continue.
1. **Do not work on #244.** Blacksmith moved the Linux caches out of GitHub's 10 GiB, so its cache-peak problem is likely gone. Over the next three code merges, record GitHub's peak cache usage on #233. If it stays under 10 GiB, close #233 as met and #243/#244 unmerged (keep the branch, remove the worktree). If it goes over, resume #244: fix the platform gate failure and the quick-job ccache fixture, then bootstrap, full CI and #233 live acceptance.
2. **#249 shared-accessor check:** small infrastructure, run in parallel. Do not block ports on it.
3. **#245 signals:** apply the two fixes in its steering comment, record its gaps (posted on #156), then do final-head validation, timing, a fresh fixing review, full CI and integrate.
4. **#248 engine:** fix the build break, add `engines.py` witnesses, delete the duplicated accessors and apply the rest of its steering comment. Then review, CI and integrate.
5. **Re-select #212 from integrated tip.**
   - Boundary: R0's scheduler with an inlined dispatch on the raw type byte and direct native handler pointers. Declare `TileLoop_*` `noexcept` under `WITH_RUST`; no lambda trampolines and no pointer cast.
   - Then the full ground/flood scope.
   - Gate: interleaved A/B against tip within 11 ms on TGP; attribute any miss with callgrind (AGENTS.md).
6. **Re-select #213 from integrated tip.**
   - Boundary: a pool-cursor descriptor instead of a `next()` call per vehicle, one slim dispatch record and one post-tick bundle, the assert check in Rust, and about three C++ calls per vehicle at most.
   - Gate: at most 6 ns per vehicle-tick visit, A/B against tip on Opus.
   - #212 and #213 may run concurrently within the WIP cap.
7. **Town #246** carries the #216 follow-ups listed on #246, including deleting the save staging if it is redundant. Select the house-tile-loop owner no later than #247.

## Earlier steering, still in force

- **Direct shared services and boundaries:** direct shared services (#107/#168/#199) and one definition per shared accessor. Use a fixed hasher for lookup-only YAPF maps; ABI blocks stay per component.
- **Ownership:** prioritize simulation ownership, which comes before the remaining conversions. Keep state at the original's process scope.
- **Map storage:** the #108 decision retains C++ map arrays with direct bundled services; measure before map changes (#108).
- **Harness evidence:** end moments (#154), default branch witnesses and reference probes (#198/#204/#210/#211), speed ratchets (#155) and documented coverage gaps (#156).
- **Tooling and WIP:** proportionate tooling (#109). The WIP cap is six (#157).
- **CI and review:** full CI is exact-head and on demand (#171), with concurrent validation (#233) and no admin bypass. Fresh reviewers fix their findings.
- **Worktrees:** integrated worktrees are removed, never archived.
- **Metrics and caps:** original retirement excludes removed fork glue (#223). Caps use three invocation medians and are lowered only beyond measured spread.
- **Speed budgets:** #212 has about 11 ms of slack on TGP. #213 has about 6 ns per vehicle-tick on Opus.

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
| #233 (CI throughput; cache acceptance pending) | #235 | direct | `21eeb445be` | 0 / 1567 / 0 / 0; net glue 0 |
| #234 (industry water fidelity) | #236 | direct | `b7a1429d38` | 5 / 97 / 32 / 0; net glue +32 |
| #229 (six calibrated caps) | #230 | direct | `e69df25522` | 0 / 12 / 0 / 0; net glue 0 |
| #239/#156 (prospect/overlap coverage) | #240 | direct | `62ef446d1e` | 0 / 114 / 0 / 0; net glue 0 |
| #214/#168 (typed station/loading) | #217 | direct | `3b3b931099` | 1018 / 42 / 1147 / 0; net glue +705 |
| #250 (CI waiter survives base pushes) | #251 | direct | `409cdb253e` | 0 / 53 / 0 / 0; net glue 0 |
| #237/#199 (typed aircraft boundary) | #238 | direct | `141e4ce397` | 798 / 420 / 470 / 0; net glue -310 |

Paused, not fallbacks: #64/#66 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness and speed maintenance

The default semantic harness runs in CI; regressions come first. Port differences
go in `KNOWN_FAILURES` with an issue, never in masks (`docs/rust-migration.md`).

1. **Shared-accessor check (#249).** A new per-port copy fails quick CI. The baseline of existing copies only shrinks.
2. **Speed ratchet (#155).**
   - **Caps:** `SPEED_BUDGETS` in `tools/simulation/roads.py` holds them. Each cap is the median of three `simulate <name> --benchmark 3 --jobs 2` invocations on an idle host.
   - **Exceeding a cap:** a PR may exceed a cap by 3%, unless root accepts a stated reason that is not a boundary exception.
   - **Lowering a cap:** a PR lowers a cap only when its median of three beats it by more than the invocation spread, about 2%.
   - **Targets:** play saves 1.5x, generation 1.15x.
   - **Misses:** attribute any miss with callgrind before changing scope (AGENTS.md).
   - **Redraw inflation:** the null driver redraws whenever 1 ms has elapsed, so a slower candidate draws more frames, which inflates play-save ratios. Say so in a PR when it decides a miss.
3. **#156 coverage, standing capacity.** Random and crash branches come first. These remain:
   - signal tbd/global overflow and combo-red;
   - town exclusivity, unwanted months and authority refusals;
   - the engine preview, commands and late-date branches;
   - station stale links;
   - #218's NewGRF and game-script paths.

## Phase 2: current work, in order

Integrate reviewed work first, in order of readiness, with at most six
unintegrated branches (#157). Independent items (#156, #249) may run in parallel.

1. **#245 signals, then #248 engine,** per the eleventh review's actions 3-4.
2. **#212 landscape, then #213 vehicle tick,** per actions 5-6. These are the largest remaining simulation owners. They also delete the tree facade (16.8 ms on TGP) and the per-vehicle scheduler crossings.
3. **Town:** #246 founding, generation and names from integrated #216; then the house tile loop; then #247 commands, authority and deletion. All of them reuse the sole town owner and the generation unwind contract.
4. **Next owners**, planned when fewer than two unstarted components remain: station tile, cargo and rating control (`station_cmd`); rail and road track commands and tile procs; tunnel/bridge. Guide the selection by the world-state design and the tip profile.
5. **Conversions buy speed headroom** and never block ownership:
   - aircraft (94 calls per tick);
   - road vehicles (12.6 per tick);
   - industry `next_tick` (13.2 ms on TGP);
   - town `next_town` and state;
   - the #168 tail (orders, effect, disaster, #218's accessors).

#108 retains C++ map arrays and integrated typed services. New owners reuse
integrated services, never draft copies. A Rust-allocated state block with C++
reference aliases (#216, #248) is acceptable. Keep save staging only for a field
that a `SaveLoad` descriptor cannot address through the alias.

## Resume checkpoint

Root resumed from `fe337e4e40` with `/start-development`. The last code revision
is green. Four implementation agents own #252, #249, #245 and #248; root owns
review, CI, integration and the three-merge GitHub cache observation on #233.

| Issue / PR | Branch, head | State and next step |
| --- | --- | --- |
| #233 / #243 / #244 | `ci-cache-peak-233`, `1288db61b4` | Root sampling; zero of three code merges observed. Leave #244 idle (action 1). |
| #252 | `windows-blacksmith-252` | `windows_blacksmith_252`: first, prepare runner variable and toolset inventory for review/bootstrap CI. |
| #249 | `shared-accessor-check-249` | `shared_accessor_check_249`: bounded quick-CI detector and shrinking baseline. |
| #219 / #245 | `signal-owner-219`, `df2f77b247` | `signals_final_245`: steering fixes and base adoption, then validation, timing and fresh review. |
| #220 / #248 | `engine-lifecycle-220`, `9c4b950b56` | `engine_final_248`: build/boundary fixes and default witnesses, then validation and fresh review. |
| #212 / #241 | `landscape-budget-prototype-212`, `45633ef6d6` | Re-selected: new branch from tip with the action-5 boundary. |
| #213 / #242 | `vehicle-budget-prototype-213`, `92b8a04075` | Re-selected: new branch from tip with the action-6 boundary. |
| #64 / #66 | `issue/64-curve-family`, `aab5211801` | Paused draft/worktree; leave untouched, not a fallback. |
| Paused integration | `integration/curve-script`, `c19f09cf8b` | Locked worktree preserved; no work. |
| Disaster evidence | `evidence-disaster-vehicles`, `a775543162` | Locked evidence worktree preserved; no work. |
| Water evidence | `evidence-water-regions`, `c752070cde` | Locked evidence worktree preserved; no work. |

ABI blocks:
- train through 385, tree additions 386-389, fleet 390-399
- company 400-429, town 430-459, road 460-499
- station/loading 500-559, industry 560-619 (572 is the shared ByteSpan), CommandCost 620-629
- town lifecycle 630-669, aircraft 670-739
- vehicle 740-779, landscape 780-819, signals 820-859, engines 860-919

Check only consumed layouts.

Preserve the pinned reference and all paused/evidence worktrees above. Do not
reapply effect helper `73ccd511fb`. Build and test with `--jobs 2`; standalone
fixture games share the benchmark lock.

## Out of scope for the near term

More `src/3rdparty` ports, GUI rendering/layout and platform/toolchain expansion.
Deferred behavior improvements stay in `deferred-improvement` issues.

## Progress metric

Every port PR pastes `python3 tools/port-metrics.py`: Rust/tooling/glue added,
original retired, fork glue removed and net glue; `--whole-fork` gives totals and
simulation share. Retire more original C++ than net glue plus tooling or explain
why. Judge game logic owned; data tables inflate retirement. Avoid utility kernels.
