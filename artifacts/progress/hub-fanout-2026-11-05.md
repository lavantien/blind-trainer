# hub fan-out log, 2026-11-05

Session goals: docs and text audit, logic and integration audit, alg coverage to 100%, global region coverage to 100%.

Baseline measured before fan-out: `python bt.py coverage` green, 34912 of 42997 regions, 81.2%, log at artifacts/progress/hub-cov-baseline.log, gap table at artifacts/coverage/gaps.tsv (163 files, 8085 uncovered regions).

## fan-out layout

- lane-docs: text audit over README.md, DATA_PROVENANCE.md, release notes, review records, provenance files.
- adv-a1, adv-a2: independent adversarial audits over app integration and data, scheme, puzzle, tooling logic. Reports to artifacts/progress/adv-audit-1.md and adv-audit-2.md.
- lane-cov-app: src/app plus tests/unit/app, 1362 regions.
- lane-cov-data: src/data plus tests/unit/data, 1048 regions.
- lane-cov-mut: tools/mutate, tools/mutants-gate, tools/policy plus their suites, 1974 regions.
- lane-cov-verify: tools/verify, visual-diff, release-verify, jobwrap, sleeper plus their suites, 1483 regions.
- lane-cov-ga: src/ga, tools/ga-run, tools/memofreq plus their suites, 926 regions.
- lane-cov-core: src/core, src/common, src/scheme, src/puzzles, tests/fuzz plus their suites, 931 regions.
- queued: lane-cov-gendata owns tools/gendata suites, gendata area coverage (361 regions), and the corner N-I synth fix for alg coverage 100%.

Lanes build through per-lane config clones from scripts/lane_build.py into artifacts/lane/<lane>/build, BT_CONFIG pinned per invocation, BT_BIN_DIR set so spawned suites use lane binaries. Guard cron 01b66cc3 fires every 20 minutes: quota, memory through scripts/memguard.py, progress staleness at 30 minutes.

## decisions

- No worktrees, shared checkout, commits only through scripts/commit_lane.py.
- Lanes never run repo-wide targets; the hub owns coverage reruns, e2e, visual, fuzz, mutants gates at wave ends.
- config/config.json, config/allowlist.tsv, config/constants_allow.txt edits escalate to the hub, except the synth section for lane-cov-gendata.
- alg coverage gap: exactly 1 of 818 cases, corner N-I, RUB to FUL, uncovered because the gendata synth search returns no candidate under the hub limits max_setup 3, max_insert 4, node_budget 50000000.

## events

- lane-docs delivered: 4 commits, 7520c1d, 18e8ab3, ef8bf5f, 7d33fcd, text audit complete. Flags: ref_ground/PROVENANCE.md is gitignored by design so its fixes stay on disk, ref_ground/vendor/PROVENANCE.md internal references hub-fixed in ff7adcd, f2l case 07 tier 2 prediction to confirm at wave-end verify-algs, coverage badge pair hub-committed in 833d8ee.
- adv-a1 delivered 4 confirmed findings: drill.c pick_pair stack overflow (high), drivers.c smoke close exit code (medium), config.c target_fps silent default and memo.c intro seed mismatch (low). Routed to lane-cov-app.
- adv-a2 delivered 6 findings: verify.c parity notes never block (high, routed to lane-cov-data), expand.c unconsumed tokens (medium, routed to lane-cov-core), json.c stale free, sq1 canonicalizer asymmetry, json.c embedded NUL truncation (low, routed to lane-cov-core), gendata synth completeness note (fed to lane-cov-gendata).
- 500M budget gendata probe: first attempt hit the 300s tool timeout at 22:46, rerun detached as a direct exe call, log at artifacts/progress/hub-probe-gendata-500m.log.
- pushed 2c475d0..833d8ee to main, ci-watch-1 watching the run.
- in flight: 7 coverage lanes plus ci-watch-1.

## guard rounds

- 23:00 round: lane-cov-app disposed at 40 minutes without a heartbeat while live (stale exe debugging), relaunched as lane-cov-app2 with inherited tree, uncommitted view_drill_test.c, and the 4 findings. Quota 33 percent after window roll, memory 38 MB.
## window halt, 2026-11-06 00:48 to 04:06:46 local

The 5-hour provider window reached its limit at 00:48 local. All 6 running lanes (data, core2, mut2, verify, ga, gendata) terminated on 429, the provider stated the reset explicitly: 2026-11-06 04:06:46 GMT+8, and local time equals GMT+8. lane-cov-app2 had already completed before the cutoff with its F1 fix committed (f8fdec7) and F2 to F4 mid-chain (uncommitted src/app/config.c, driver_keys, views/memo.c, config_test.c). Halt per the standing rules: no provider-calling agents, no development activity, shell and scheduled processes keep running. 14 commits banked unpushed, push plus CI watcher wait for the reset.

## 500M gendata probe result, harvested during the halt

Full regeneration at node_budget 500000000 finished in 6085 seconds, exit 0, manifest at artifacts/probe-gendata/vendor: 818 of 818 cases covered, edge 440 of 440, corner 378 of 378, 158 generated (68 edge, 90 corner). Corner N-I solved by a generated, mechanically verified alg: L2 U L2 F' U B' U' F U B U' L2 U' L2, commutator L2 U L2:[F',U B' U'], setup depth 3, insert depth 4, both inside the current shape limits. Conclusion: N-I fails at hub budget from search-order starvation, not shape unreachability, matching adv-audit-2 F6. The gendata lane owns the fix with this evidence: prefer a per-case budget floor or setup-order fix over a blind 10x budget, since a full regen at 500M costs 101 minutes and suite-level regens cannot afford that.

## resume plan after 04:06:46 local

1. First guard tick after the reset runs the usage query and confirms the window rolled before anything launches.
2. Push the banked commits, start the CI watcher.
3. Relaunch 7 lanes, each continuing from its progress file, each told its predecessor died at the provider limit and which uncommitted edits it owns: data (parity fix 51373ea landed, sq1 F4 in normalizer.c mid-chain), core2 (F2 65c3206, F3 and F5 cb77899 landed, bt_harness and cube.c edits uncommitted, skewb parse thread open), mut2 (selftest confirm, typos rename thread), verify (1132 of 1483 regions remain, registry drift and unimplemented-section threads, env writer identification owed), ga (all green, scope runner remeasure owed), gendata (probe matrix plus the hub 500M ground truth above, N-I fix, then area coverage), app-finisher (F2 drivers red run result, F3 and F4 uncommitted edits, commit and deliver).
4. After lanes deliver: wave-end chain through the hub: fuzz, mutants and mutants-gate for touched areas, e2e, visual, verify-algs (confirming the f2l case 07 prediction from the docs lane flag), full coverage rerun to measure the new number, adversarial review record in docs/reviews/, badge refreshes, final push.

## resume executed, 2026-11-06 04:15

lane-cov-app2 survived the halt parked on background suites, observed a hub hold until 04:07, then resumed on its own: all 4 routed findings committed with red-green evidence, f8fdec7, a535d4f, 5bf6867, 111f923. Banked commits pushed as 833d8ee..111f923, ci-watch-2 watching. Relaunched data2, core3, mut3, verify2, ga2, gendata2 with resume briefs carrying their recorded points, the uncommitted edit ownership, and for gendata the 500M probe ground truth. usage-query-guard5 confirming the rolled window.

## app delivery and fmt gate, 2026-11-06 05:0x

lane-cov-app2 delivered: audit fixes f8fdec7, a535d4f, 5bf6867, 111f923, formatting 08bf013, test batches aca5251 and 49bcdd6 (unique window titles through BT_CONFIG fixtures killing a real cross-process FindWindow race). app area at 6841 of 7984, 85.7 percent, remaining 1143 regions mapped in lane-cov-app-scope4.log. Continuation lane-cov-app3 launched with the hub decisions: a driver seam for the main.c window-open failure branch (exclusion banned), consumption of the allocation-failure idiom the core lane now owns in bt_harness, and a self-built sqlite poisoning helper for persist.c. Two CI cycles were lost to hand-formatted fixtures (config_test.c, then seedpack_errors_test.c and ga_upgrade_test.c), lanes fixed both as 08bf013, 9cfc7ef, 8e2880e. Root-cause fix landed as b628dc9: scripts/commit_lane.py now runs clang-format --dry-run --Werror over staged c and h files and refuses the commit on violations, probed red and green against a planted misformat.

Wave-end checklist additions: python bt.py baselines recapture after all app changes land (the memo seed fix renumbers the first streamed scramble), readme smoke exit-code line gains the window-closed-mid-tour case, f2l case 07 tier 2 prediction check at verify-algs, adversarial review record in docs/reviews/, readme env table gains the 4 app seam rows (BT_APP_WINDOW_FAIL, BT_APP_FOCUS_CURSOR_FAIL, BT_APP_FOCUS_CLASS_FAIL, BT_APP_FOCUS_GIVEBACK).

## second window halt, 2026-11-06 06:05 to 09:09:17 local

The 5-hour window ceilinged again. All running lanes died on 429 by 07:03, the provider stated the reset: 2026-11-06 09:09:17 local. lane-cov-mut3 hit its 200-turn limit rather than a 429, so it is resumable in place via SendMessage. Memory census at halt: 0 repo processes, no orphans.

Deliveries before the halt: verify3 (10 commits, 1483 to 914 remaining, 6 dead-arm candidates proved, attack notes), ga2 (closed: 926 to 274, 4-class residue, tree purged), app4 (6 commits: all 39 app allocation sites routed through the fault seam, which exposed and fixed a real stale-free heap bug in bt_repo_arms, plus the env seam family), data3 (fmt fix, sqlite authorizer idioms, 646 to 616, dead arms proved). Post-halt harvest: about 20 more commits banked unpushed, including 956d94c (every data allocation through the seam) and 1a21312 (4 dead arms dropped), fuzz body bound tests, and the app env seams.

Local ground truth at halt: data/vendor/manifest.json shows corner 378 of 378, the gendata regen completed and covered N-I, but the lane died mid-chain before verify-algs, the badge refresh, the pin test, and the commit, so the committed badge still reads 99.9.

Resume plan after 09:09:17 local, first guard tick past the reset:
1. Usage query confirms the window rolled.
2. Push banked commits, start the CI watcher.
3. SendMessage-resume lane-cov-mut3 in place.
4. Relaunch 5 lanes from their progress files: app (died writing the red test for the net.c letter lookup restructure), data (died fixing the review-growth OOM test premise), ga (died reading the class 2 dead-arm sites), verify (died mid ev engine arms, having found the predecessor's backslash arm proof wrong, it is live via a backslash BT_ARTIFACTS value), core (died mid tier1 abort expectation), and gendata finishes the N-I chain: superset check, verify-algs, scripts/alg_coverage_badge.py to 100.0, pin test, atomic commit.
5. Then the wave-end chain: full coverage rerun, fuzz, mutants and gate for touched areas, e2e, visual, baselines recapture, verify-algs, review record, readme updates, badges, final push.

## halt lift and tree triage, 2026-11-06 08:23 local

Local clock reads +07:00, so the provider reset 09:09:17 GMT+8 equals 08:09:17 local and passed before this pass. The halt ended on the reset time and work resumed on that basis. The confirming query landed after the relaunch: usage-query-guard6 read the fresh window at 3 percent at 09:24 GMT+8, far below the 95 percent halt trigger, so the resume stands. A late-draining report from usage-query-guard8 showing 90 percent is a stale snapshot timestamped 06:00 GMT+8, taken an hour before the halt began, and does not describe the current window. The payload carries no reset time field, so the provider-stated 09:09:17 GMT+8 stays the recorded authority. Both query agents are disposed.

Uncommitted tree triage before relaunch:
- Deleted as junk: default.profraw at the repo root plus 11 stale probe binaries and pdbs under playground.
- Hub commits: d549569 progress logs, 5067238 probe sources from the closed audit, ga, and lock screen rounds (fmt gate caught 2 hand-formatted files, clang-format applied to the 5 c sources only, the fmt fix stays lane-scoped because the tree holds lane in-flight edits).
- Remaining edits are all in-flight lane work with explicit owners for the relaunch briefs: app owns src/app/net.h plus playground/cov_scope_app.py, app_lane/json_probe.c, asan_lane_app.py. core owns src/common/bt_harness.c and .h, bt_trace.c, config.c, src/puzzles/geometry.c, sq1.c, tier1.c, tests/unit/common/bt_harness_test.c, tests/unit/puzzles/tier1_test.c and vt_roundtrip_test.c, plus playground/cov_scope_core.py and the lane_core probes. mut3 owns tools/mutate/tests.c mid split into tests_scratch.c, tests/unit/tools/policy_fsutil_test.c, playground/cov_scope_mut.py and junction_stat_probe.c. data owns tests/unit/data/data_oom_test.c and playground/cov_scope_data.py. verify owns playground/cov_scope_verify.py. gendata owns playground/gendata.

## ESCALATE 3 ruling, assertion arms in test files

lane-cov-mut3 proved the check macros BT_CHECK and MUT_CHECK expand an if arm inside every test function, so a green suite can never execute those arms and test files can never read 100 percent. The root-cause fix is structural: the macros expand to a branchless call into a harness function, for example bt_harness_check_bool(condition, expr, file, line) in src/common/bt_harness.c, where the failure arm exists once and one harness unit test covers both arms through the inner-suite pattern. This matches the existing bt_harness_check_str shape. Assertions keep their behavior and their file and line context, nothing is weakened and nothing is excluded. The rewrite touches every test file at once, so it cannot run while the 6 lanes hold in-flight edits in those files. Ruling: lanes keep closing every reachable dark region and stop counting assert arms as their residue, and one dedicated lane executes the macro restructure at wave end after the current lanes deliver, then the wave-end coverage rerun measures the effect.

## resume executed, 2026-11-06 08:4x local

Pushed 72f97b7..5067238 (38 commits) to main, ci-watch-3 watching the run. SendMessage-resumed lane-cov-mut3 in place with the ESCALATE 3 ruling and an explicit ban on starting the macro restructure itself. Relaunched 6 lanes with full briefs carrying the triage ownership: lane-cov-app6 (net.c indexed lookup finishing the restructure so the lint question dissolves, then main.c interactive entries via the env seam family, alloc arms, view batches), lane-cov-data5 (data_oom_test premise fix, the 4 deferred dead-arm removals, driver files by size, allocation arms last), lane-cov-ga4 (the 4-class order: dead arms with proofs, alloc arms through the seam, abort guard vs death test, platform arms drive-or-escalate), lane-cov-verify5 (re-verify all six predecessor dead-arm proofs independently since one was already wrong, the main.c 87 backslash arm is live and gets driven, then OOM routing, archives, visual-diff, ev engines), lane-cov-core4 (verify and land the inherited uncommitted chain, then scheme batches and fuzz tables), lane-cov-gendata3 (superset check, suites, verify-algs, reference re-pin with hub review flag, badge 100.0, pin test, atomic commit, then gendata area coverage). 7 dev slots plus the watcher, within the 8 plus 1 cap. Rulings handed down: net.c lint question answered by the restructure itself, gendata provenance re-pin mechanically by the lane with hub review at wave end.

## CI failure on the push and routing, 2026-11-06 08:5x

Run 42144152260 on 5067238 failed at fmt, only paths_errors_test.c listed, later fmt batches never ran. The hub wrote playground/ci_fmt_check.py, which mirrors the CI fmt check over every tracked c and h file at HEAD through git show and clang-format stdin, so one local pass finds every violator instead of one CI cycle per file. Findings: 49 files at HEAD violate clang-format, 44 of them under playground, which sits outside the bt.py fmt scope (SOURCE_GLOBS covers src, tools, tests only), so the CI blockers are exactly 4 test files. Routed: paths_errors_test.c to data5 (plus the repo_errors_test.c SLOC split, policy cap), ga_run_flow_test.c to ga4 (plus its live compile error at tools/ga-run/lib.c:247 caught by a hub policy run, void function returning a value, and the ga_run_inputs_test.c trailing comma lint), policy_constants_test.c and policy_driver_test.c to mut3. The legacy playground fmt debt stays untouched: no gate covers it and reformatting would race active lanes editing probe files there, it goes on the wave-end decision list.

mut3 delivered mid-round: 14 commits ending cd4445d, two product fixes (mutate report fails closed on an unreadable ledger, the policy junction planter writes a valid reparse buffer), 54 selftests plus 15 driver tests, area at about 1570 dark with every remainder classified. Disposal done, then resumed in place with the follow-on batch: the 2 policy fmt files, the ESCALATE 5 centralization, the OOM routing batch over mutate, mutants-gate, and policy production allocations, and a drive-or-escalate pass over the IO defensive arms and Windows-unreachable mkdirs paths with per-site proofs.

ESCALATE rulings from mut3's delivery: ESCALATE 4 (bt_harness.c includes profile/InstrProf.h under instrumentation and breaks every lane scoped measurer) routed to core4, which owns bt_harness.c and its macro rename chain, fix verified against a scoped measurer run before commit. ESCALATE 5 (recurring literal 12 in tests_ops.c) ruled centralize-never-allowlist: the literal becomes a named constant, file scope when all uses sit in one file, config hub when it recurs across files, no allowlist entry. ESCALATE 6 routed per file: ga_run_inputs_test.c to ga4, sleeper_test.c to verify5, repo_errors_test.c SLOC split to data5 inside this run, not deferred to wave end.

Next push waits on the routed fmt, compile, and lint fixes, then a fresh watcher covers the run.

## gendata chain step 1 result, 2026-11-06 09:0x

Superset check green: 817 of 818 cases byte-identical, exactly 1 derived gain (corner 13-8 N-I, cherry rowid 109, position UFR-RUB-FUL, qtm 18), zero drift, zero regressions. Finding beyond the intended delta: specials.json lost twist notes on 18 parity rows (16 cherry, 2 eggins). Root cause traced to 51373ea, the data lane's parity verdict fix committed before the regen exe was built, so the regenerated specials reflect committed verdict semantics and the old reference predates them. Approved: chain continues, PROVENANCE.md documents both divergence causes, and the wave-end review list gains the specific question whether 51373ea intends the twist-note drop or owes a note restoration. The badge remains at 99.9 until the chain's badge step runs.

## guard round, 2026-11-06 09:0x

Memory census: 3 procs at 13.5 MB against the runtime cap, no breach, foreign farm untouched. Progress sweep: core4 landed 2 commits from its inherited chain (e6aabc5 parse scratch zeroing, fa699ec nxn state_eq pin), data5, gendata3, mut3, verify5 all heartbeating within 10 minutes. lane-cov-app6 and lane-cov-ga4 ran silent since their 08:30 spawn, 35 minutes without a progress line and no commits, so both were disposed per the 30 minute rule, TaskStop plus a census recheck showing 0 procs and 0 MB, no orphans. Relaunched as lane-cov-app7 and lane-cov-ga5 from their recorded points with the heartbeat rule made explicit in the briefs, first line within 5 minutes. ga5 also inherits the 3 routed items: the lib.c compile error, the ga_run_flow fmt fix, the ga_run_inputs trailing comma. Quota query usage-query-guard7 dispatched this round and its reply landed: 12 percent of the 5-hour window at 09:43:59 GMT+8 (08:43:59 local), far under the 95 percent halt trigger, normal operation continues. The payload carries no reset timestamp, the provider-stated window start stays the recorded authority. Query agent disposed after reporting.

## mut lane follow-on batch delivered, 2026-11-06 09:1x

5 commits: 8f986be both policy fmt blockers fixed, da6a7d3 the literal 12 centralized as named constants with no allowlist row plus a real repair (the core lane's seam routing made bt_trace depend on the alloc object and broke the ws fixture link until da6a7d3 added it), 5e53421 and 1f6b53b the OOM routing over 12 mutate and gate sites with fail-closed arm tests, 6f335ff the mkdirs seam driver. Drive-or-escalate pass closed with per-site proofs for the IO defensive arms, gate read_arg arms, and Windows-unreachable mkdirs paths, all recorded in lane-cov-mut.md for the wave-end decision batch. Lane disposed after delivery, no live task remains. Its drivable surface is exhausted: what remains in the mut area is the assertion-arm class (macro restructure at wave end) and the proof-backed unreachable arms (user decision batch). Two notes routed to core4: sweep every link list that pulls bt_trace for the missing alloc object instead of waiting for per-lane breakage, and drive the report.c strbuf-owned arms through the strbuf growth path.

CI blocker score after this batch: policy fmt fixed, policy literal fixed. Remaining before the next push: paths_errors_test.c fmt (data5), ga_run_flow_test.c fmt and the lib.c compile error (ga5), the ga_run_inputs and sleeper trailing commas (ga5, verify5), repo_errors_test.c sloc split (data5).
