# OpenTTD-Rust near-term roadmap

Root owns selection here; `AGENTS.md` and `docs/rust-migration.md` define process.
If an issue conflicts, follow this roadmap and report it to root. Keep this
forward-looking, about 200 lines; completed work is one row, with evidence in PRs.

## Where the fork stands (2026-10-15, `c9f4e5c770`)

- Twenty-seven ownership ports. Whole fork: Rust 60,497 / tooling 31,189 / glue 27,289 / original retired 38,219 of 470,612 `src/` lines (8.12%). Simulation: 30,248 / 102,615 (29.48%); source counts, not behavioral completion.
- Since the eleventh review, engine #248, signals #245 and town founding #258 integrated, retiring 1,717 original lines. Landscape #212 and vehicle #213 missed their final gates after profiled fix rounds and stopped as drafts at the pause.
- Every stored speed cap passes at tip (values on #258). CI on `rust-migration` is green.
- Two unintegrated component drafts, #255 and #257; both conflict with the town base.
- Largest original simulation files remaining: station_cmd 4.5k lines, rail_cmd 3.2k, vehicle 3.0k, road_cmd 2.7k, tunnelbridge 2.2k, town_cmd 1.9k, landscape 1.8k, water 1.5k.

## Twelfth steering review (2026-10-15, housekeeping)

Audits of #258, tooling, docs, tests, glue, Rust and repository state found no
reachable divergence in #258. Debt has built up; #259 and #261 list each item:

- **#258 leftovers.** The Rust branch of `InitializeLayout` and the exports `town_update_rate` and `town_grow` are orphaned. The founding services table re-creates the shared company accessors, and both reviews and #249 missed it because the copy is a Rust table field. Coverage gaps went to #156.
- **Dead FFI.** About 250 lines across ports: ten exports with no caller, uncalled C++ wrappers, and callback fields Rust never calls. The "call services directly" conversions left them behind.
- **Preserved state.** 69 merged branches remain on origin and 102 locally. Worktrees for closed or paused work hold 6.2 GB, and `.local/` holds 2.4 GB of stale output. The worktree rule tied removal only to a merge, so root kept these for eleven days. `AGENTS.md` now ties removal to an open PR and orphaned exports to the PR that removes their caller.
- **Inert CI and duplicated tooling.**
  - 2,130 lines of workflows that guard nothing the fork relies on, including a nightly run.
  - 3,337 lines of duplicated comparison scaffolding, which set the native critical path.
  - 32 of 48 component entries over budget, and about 20 doc citations of opt-in `--soak`/`--self` runs.
- **Tests and refactoring.** A second round of audits sized cleanup that changes no behaviour, now #261:
  - 34 s of serial Python tests on every push;
  - about 330 lines of protocol tests the harness already covers, and duplicated ABI layout lists;
  - about 600 glue lines of block and header boilerplate;
  - about 1,165 lines of Rust forwarding functions, and 660 callback fields declared `unsafe` without need;
  - save staging that the aliases make redundant.
- **Fidelity.** `ScriptList` rank and modification counts abort on overflow where the compiled original wraps, and a script can reach it (#260).

Actions, in order:
1. **#259 H1 first, by root, without a PR.** Change the repository setting, then delete branches, remove worktrees, close #66 and clean `.local/`, exactly as #259 lists. This takes minutes, not a session.
2. **#260 next.** It is a small fidelity fix.
3. **Housekeeping PRs, at most three in flight alongside the work order:**
   - #259 H2 (a docs-only commit by gpt-6-luna at medium effort), H3 (workflows) and H4 (dead code with its checks);
   - then #261 R1-R4; R1-R3 wait for H4, because they touch the same files;
   - #259 H5 last.

   These are maintenance PRs. They do not count against the component WIP cap and never block an ownership port.
4. **Then the Phase 2 work order:** update #255 and #257 from base, profile, re-select; then town.

## Earlier steering, still in force

- **Direct shared services and boundaries:** direct shared services (#107/#168/#199) and one definition per shared accessor, enforced in quick CI by #249, whose baseline only shrinks. Use a fixed hasher for lookup-only YAPF maps; ABI blocks stay per component.
- **Ownership:** prioritize simulation ownership, which comes before the remaining conversions. Keep state at the original's process scope.
- **Map storage:** the #108 decision retains C++ map arrays with direct bundled services; measure before map changes (#108).
- **Harness evidence:** end moments (#154), default branch witnesses and reference probes (#198/#204/#210/#211), speed ratchets (#155) and documented coverage gaps (#156).
- **Tooling and WIP:** proportionate tooling (#109) and a stated budget for infrastructure work. Prefer platform defaults to controllers (#233; #243/#244 closed). The WIP cap is six component branches.
- **CI and review:** full CI is exact-head and on demand (#171), with concurrent validation (#233) and no admin bypass. Fresh reviewers fix their findings.
- **Speed misses:** #212/#213 missed on implementation costs, not FFI cost. A miss gets a callgrind-attributed fix round before deferral. Budgets are about 11 ms on TGP for #212 and 6 ns per vehicle-tick visit on Opus for #213.
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

Harness and process: #72 harness (#85), #84 play saves (#87), #97 provenance
freeze (#100), #88 Ruff (#92), #75 partial-pixel fidelity (#91), #90 world-state
design (`docs/design/world-state.md`). Earlier maintenance PRs keep their
evidence; rows through #215 use legacy retirement including deleted glue, and
later rows use original retirement with net glue in the PR (#223).

| Issues | Maintenance PRs | Integration | Commit | Metrics |
| --- | --- | --- | --- | --- |
| #199 (shared road services) | #215 | direct | `4b130309a5` | 2288 / 565 / 1331 / 1354 |
| #192 (company direct services) | #209 | direct | `8fe6f5ed56` | 1495 / 159 / 954 / -3; net glue +424 |
| #224 (disk hygiene) | #225 | direct | `898eb33c17` | 0 / 149 / 0 / 0; net tooling -546 |
| #227 (loading context fidelity) | #228 | direct | `9ee89f69ad` | 3 / 47 / 0 / 0 |
| #231 (OpenGFX cache) | #232 | direct | `21e2c9ea60` | 0 / 0 / 0 / 0; workflow net -6 |
| #223 (original-retirement metric) | #226 | direct | `86322c8422` | 0 / 274 / 0 / 0; net glue 0 |
| #233 (CI throughput and cache acceptance complete) | #235 | direct | `21eeb445be` | 0 / 1567 / 0 / 0; net glue 0 |
| #234 (industry water fidelity) | #236 | direct | `b7a1429d38` | 5 / 97 / 32 / 0; net glue +32 |
| #229 (six calibrated caps) | #230 | direct | `e69df25522` | 0 / 12 / 0 / 0; net glue 0 |
| #239/#156 (prospect/overlap coverage) | #240 | direct | `62ef446d1e` | 0 / 114 / 0 / 0; net glue 0 |
| #214/#168 (typed station/loading) | #217 | direct | `3b3b931099` | 1018 / 42 / 1147 / 0; net glue +705 |
| #250 (CI waiter survives base pushes) | #251 | direct | `409cdb253e` | 0 / 53 / 0 / 0; net glue 0 |
| #237/#199 (typed aircraft boundary) | #238 | direct | `141e4ce397` | 798 / 420 / 470 / 0; net glue -310 |
| #252 (Windows runner) | #253 | direct | `cfc98a16a7` | 0 / 0 / 0 / 0 |
| #249 (shared-accessor enforcement) | #254 | direct | `8f9d4fc906` | 0 / 431 / 0 / 0 |

Paused, not fallbacks: #64 curve family, #68 SHA-512/Ed25519, #69 tile areas.

## Phase 1: harness and speed maintenance

The default semantic harness runs in CI; regressions come first. Port differences
go in `KNOWN_FAILURES` with an issue, never in masks (`docs/rust-migration.md`).

1. **Shared-accessor check (#249).** A new per-port copy fails quick CI. The baseline of existing copies only shrinks; #259 H4 extends the check to orphaned exports and duplicate table bindings.
2. **Speed ratchet (#155).**
   - **Caps:** `SPEED_BUDGETS` in `tools/simulation/roads.py` holds them. Each cap is the median of three `simulate <name> --benchmark 3 --jobs 2` invocations on an idle host.
   - **Exceeding a cap:** a PR may exceed a cap by 3%, unless root accepts a stated reason that is not a boundary exception.
   - **Lowering a cap:** a PR lowers a cap only when its median of three beats it by more than the invocation spread, about 2%.
   - **Targets:** play saves 1.5x, generation 1.15x.
   - **Misses:** attribute any miss with callgrind before changing scope (AGENTS.md).
   - **Redraw inflation:** the null driver redraws whenever 1 ms has elapsed, so a slower candidate draws more frames, which inflates play-save ratios. Say so in a PR when it decides a miss.
3. **#156 coverage, standing capacity.** Random and crash branches come first. These remain:
   - town generation with `larger_towns == 0` (skips a draw);
   - signal tbd/global propagation overflow and arbitrary mod layouts;
   - town exclusivity, unwanted months and authority refusals;
   - engine external NewGRF/retire_early and legacy save branches;
   - station stale links;
   - #218's NewGRF and game-script paths.

## Phase 2: current work, in order

Integrate reviewed work first, in order of readiness, with at most six
unintegrated component branches. Housekeeping (#259) and #156 run in parallel.

1. **Housekeeping:** #259 H1, then #260, then up to three of #259 H2-H5 and #261 R1-R4 at a time, alongside the items below.
2. **#212 landscape (#255) and #213 vehicle (#257).** Update each from base and resolve the town conflicts; a fresh reviewer checks the resolution. Re-freeze against the new tip, profile the head with callgrind, then root re-selects a bounded fix with its gate, or re-scopes the owner. The selected boundaries stand:
   - landscape: an inlined dispatch on the raw type byte, `noexcept` `TileLoop_*` handlers called directly, Rust transition control and no raw-map extension; gate within 11 ms on TGP, interleaved A/B against tip;
   - vehicle: a pool-cursor descriptor, one slim dispatch record and one post-tick bundle, at most about three C++ calls per vehicle (`docs/design/world-state.md`); gate at most 6 ns per vehicle-tick visit on Opus.
3. **Town:** #256 house lifecycle and tile simulation, then #247 commands, authority and deletion. Reuse the sole town owner and generation unwind contract.
4. **Next owners**, planned when fewer than two unstarted components remain: station tile, cargo and rating control (`station_cmd`); rail and road track commands and tile procs; tunnel/bridge. Guide the selection by the world-state design and the tip profile.
5. **Conversions buy speed headroom** and never block ownership:
   - aircraft (94 calls per tick);
   - road vehicles (12.6 per tick);
   - industry `next_tick` (13.2 ms on TGP);
   - town `next_town` and state;
   - the #168 tail, largest first: `order_cmd.cpp` and `economy.cpp` hold 65 of the 198 baseline copies;
   - the opaque train, road, ship, effect and cargo-packet owners. Convert them to C-layout blocks with aliases (#168 comment): about -850 glue lines, no field selectors, and inline loads. Do train with or after #213.

#108 retains C++ map arrays and integrated typed services. New owners reuse
integrated services, never draft copies. A Rust-allocated state block with C++
reference aliases (#216, #248) is acceptable. Keep save staging only for a field
that a `SaveLoad` descriptor cannot address through the alias.

## Resume checkpoint

Paused at the user's request. On resume, apply the twelfth steering review: #259
H1 first. Post-merge CI is green; no agent work is running.

| Issue / PR | Branch, head | State and next step |
| --- | --- | --- |
| #259 | unstarted | H1 by root first, then H2-H5 (at most three housekeeping PRs in flight). |
| #260 | unstarted | ScriptList fatal paths; the first code PR after H1. |
| #261 | unstarted | R1-R4 after #259 H4 merges. |
| #212 / #255 | `landscape-owner-212`, `63c67959ee` | Stopped: TGP gate missed (see PR). Conflicts with town base. Next: update, profile, root re-selection. |
| #213 / #257 | `vehicle-owner-213`, `67611082f7` | Stopped: CPU gate missed (see PR). Conflicts with town base. Next: update, profile, root re-selection. |
| #256 / #247 | unstarted | House lifecycle precedes town commands/deletion. |
| #64 / #66 | `issue/64-curve-family`, `aab5211801` | Paused vendored-crypto draft; H1 closes the PR and removes its worktrees; the branch stays on origin. |

ABI blocks:
- train through 385, tree additions 386-389, fleet 390-399
- company 400-429, town 430-459, road 460-499
- station/loading 500-559, industry 560-619 (572 is the shared ByteSpan), CommandCost 620-629
- town lifecycle 630-669, aircraft 670-739
- vehicle 740-779, landscape 780-819, signals 820-859, engines 860-919
- house lifecycle 920-979

Check only consumed layouts.

Preserve the pinned reference. Do not reapply effect helper `73ccd511fb`.
Build and test with `--jobs 2`; standalone fixture games share the benchmark lock.

## Out of scope for the near term

More `src/3rdparty` ports, GUI rendering/layout and platform/toolchain expansion.
Deferred behavior improvements stay in `deferred-improvement` issues.

## Progress metric

Every port PR pastes `python3 tools/port-metrics.py`: Rust/tooling/glue added,
original retired, fork glue removed and net glue; `--whole-fork` gives totals and
simulation share. Retire more original C++ than net glue plus tooling or explain
why. Judge game logic owned; data tables inflate retirement. Avoid utility kernels.
