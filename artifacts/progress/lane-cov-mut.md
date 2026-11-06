# lane-cov-mut: mutation machinery coverage

## status

Active. Owned set: tools/mutate (14 files, 1552 dark regions), tools/mutants-gate (3 files, 353), tools/policy (7 files, 269). Lane tree: artifacts/lane/lane-cov-mut (plain + cov configs).

## ESCALATE 1: shared build/env.json poisoned by a lane

2026-11-05 22:50. The repo shared build/env.json now records `"obj_root": "artifacts/lane/lane-cov-verify/build/obj_cov/f7a6b67e7069"`. A lane ran a flow that writes build/env.json (doctor or an equivalent) under its own lane config, and lanes share dirs.env because scripts/lane_build.py redirects obj and bin but not env. Consumers of repo build state break for every lane except the one that wrote it: tools/mutate engine.c obj_root_of trusts the recorded obj_root when the dir exists, so the committed mutate_driver.selftest_passes spawn test fails here with link lists referencing lane-cov-verify objects and argv count drift (evidence in artifacts/progress/lane-cov-mut-suite1.log, failures at tools/mutate/tests.c:563, 610, 636).

Ask: hub fixes scripts/lane_build.py to redirect dirs.env (and raylib env consumers if any) per lane, and restores the shared build/env.json to the shared tree's own obj root. Until then I sequence engine-dependent spawn tests last and work the policy and mutants-gate areas plus the mutate units that need no obj tree.

## log

- 22:40 lane config created, scoped measurer playground/cov_scope_mut.py written (plain and instrumented configs under artifacts/lane/lane-cov-mut).
- 22:43 first scoped run: tools_mutate_test 16 passed 1 failed (selftest_passes, cause above). Full detail captured in lane-cov-mut-suite1.log.
- 22:50 root cause traced to shared build/env.json, escalated above.
- 23:24 relaunched as lane-cov-mut2 after guard disposal for staleness. Hub state: lane_build.py redirects dirs.env per lane since c705b34, shared build/env.json regenerated under the repo hub. Plan: regenerate lane config, rerun the mutate suite to confirm selftest and engine tests recover, then work policy, mutants-gate, and mutate coverage per the inherited plan.

## ESCALATE 2: shared build/env.json poisoned again, recurrence of ESCALATE 1

2026-11-05 23:35. During my selftest confirmation the shared build/env.json again recorded a foreign obj_root, this time artifacts/lane/lane-cov-core/cov-build/obj/f7a6b67e7069. The lane_build.py env redirect does not cover lane-authored measurement clones: cov_scope_*.py scripts build their own instrumented configs (the core lane used a cov-build tree) and some bt.py flow under those configs rewrites the shared build/env.json. Effect on this lane: tools/mutate engine obj_root_of trusts the dir when it exists, the fixture .d is missing there, so dep inputs are skipped and mutate selftest fails dep_inputs_land_under_scratch and stub_assembly_matches_the_bt_py_contract (53 vs 55 argv lines, the -I scratch/inputs pair missing). Local repair done: obj_root restored to the real shared dir build/obj/70dcf3245464 where the fixture .d exists, selftest rechecked below. Ask: pin env.json writes to the repo hub config inside bt.py, or extend the lane measurer recipe to inherit the lane_build clone dirs instead of authoring new ones.
- 23:27 ESCALATE 1 resolved end to end: selftest 32 passed 0 failed direct and the full tools_mutate_test suite green under bt.py (17 passed, 0 failed) with the shared obj_root restored. Coverage work starts with policy.
- 23:44 root-caused the policy fsutil reparse failure with playground/junction_probe.c: the committed make_junction overstated ReparseDataLength by 2 bytes (12+len+2 instead of 12+len) and copied the substitute name at PathBuffer+2, this Windows build rejects both with error 4392 ERROR_INVALID_REPARSE_DATA, and CI runners tolerated the malformed buffer. Probe dump of a native junction pinned the exact header: data_len 156 for a 144 byte name, print offset 146, name at offset 16. Fixed the helper in tests/unit/tools/policy_fsutil_test.c, keeping every assertion.

## relaunch 3, 2026-11-06 04:2x

lane-cov-mut3 replacing the 429-killed predecessor. Toolset probe passed. Inherited: env.json healthy at the real shared obj root (build/obj/70dcf3245464), the predecessor's local repair stands, junction planter fix committed as 71796c0 with the policy driver suite, 4 policy test files modified uncommitted (constants, lexer, lib, sloc), junction probe binaries stale in playground. Plan: refresh the lane clone, confirm the mutate selftest still green under the healthy env, assess the uncommitted policy batch, then self-measure and drive the remaining mutate, mutants-gate, and policy gaps to zero. My own measurer must redirect dirs.env inside its instrumented clone so this lane never writes the shared build state (ESCALATE 2 ask applies to my recipe too).

## progress, 04:22

- Selftest confirmed green under the healthy env.json (suite 17 passed via bt.py). Probe binaries purged from playground.
- Committed the inherited policy batch as a236e79: lexer multibyte and escape edges, constants, lib, sloc tests, 194 lines, all 7 policy suites green.
- Driver batch landed with a root-cause fix: mutate report silently wrote an empty successful outcomes.json for an unreadable ledger path (ledger load treats missing files as empty so engine resume works on first run, but report must fail closed like union does). report.c now probes readability first, red test was mutate_driver.report_with_a_missing_ledger_exits_1 expecting exit 1 plus cannot produce. Also added 12 spawn tests over the driver verbs: wildcard and question mark list, non-.c and trailing slash rejection, planted ledger report counters, union-report over a directory, usage exits for report, union-report, run, unknown verb, no-files-matched after config, selftest-probe pid output, selftest-flood killed at timeout (rc -2 with flood captured). Suite green. Oracle: the usage text and exit codes printed by the driver itself, the ledger format pinned by the union suites, fixture directory contents.
- Found and avoided a probe trap: running the suite exe directly with the lane BT_CONFIG inenv makes bt_test_repo_root strip two components off the lane config path and derive a wrong root, the in-process selftest then aborts hunting artifacts/lane/config/config.json. bt.py child_env pins the repo hub for children, so suite runs go through bt.py, never raw exe calls.
- Remaining dark after this batch, from the 04:16 measurement: tests.c 270, mutants-gate tests.c 231, engine.c 169, tests_ops 105, scratch.c 82 (bt_mut_scratch_root, pid_alive, dir_first_file_creation), token.c 81, fs.c 75, mutants-gate main.c 61, then smaller tails. Next: scratch.c unit drivers, mutants-gate main spawn edges, then the test-file helper branches.

## progress, 04:39

- Committed 8637924 gate driver edges (late missing inputs, unwritable merge output), b53149c scratch batch (scratch_root fallback, orphan marker pid judgment incl. self, foreign live via raw WinAPI flood child, dead pid, markerless first-file creation, missing roots refuse), ad2176e exponent literals (1.5e3, 0x1p4, 2e9f cover is_digit, the hex p/P branch, format_float: 4 float variants plus 1, minus 1, 0, 1), 1c3e6d6 obj root resolution (parameterized obj_root_from so the env record branch, non-string record, absent recorded dir, missing env file, single and doubled subdir scans, empty and missing bases are all driven; selftest pin now 39).
- Measurement at 04:29 after the first three commits: 1588 dark from 1831. scratch.c now 8 dark (the non-Windows stubs excluded), main.c 73 dark from 247, operators.c is_digit covered.
- ESCALATE 3: assertion-failure regions are structurally uncoverable. llvm-cov show over tools/mutate/tests.c: 69 zero-count executable lines, 43 of them inside the MUT_CHECK and BT_CHECK failure arms (if (!(cond)) { bt_harness_fail(...) }), with branch data True: 0 on every assert. A passing suite never executes those arms, and driving them would require failing tests. The same pattern dominates tools/mutants-gate/tests.c (231 dark) and the tails of tests_ops.c and tests_engine.c, and it applies to every measured test file under tools/ and tests/fuzz/. Line-level evidence: artifacts/lane/lane-cov-mut/show-tests.txt line 20 branch [True: 0, False: 5]. The hub owns the measurement decision: accept assertion arms as a documented residual, exclude them from the denominator by rule, or restructure the assert macros. My lane keeps closing every region that is reachable while green.
- read_arg seek, tell, oom, and short-read arms in mutants-gate main.c: unreachable through the exe on this platform (MSVC CRT fopen on a directory fails at open, regular files do not short-read), same defensive class.
- Next: engine.c 168 (dark helper paths), mutants-gate lib.c 61, fs.c 70, spawn.c 49, token.c 80, then remeasure.

## progress, 04:50

- Committed 0eaa6b3 engine refusal batch (limit stop, malformed ledger, unreadable target, poisoned ledger path over the file-parent trick, fake-root trio for the obj-root scan fallback, missing suite, unknown area, tools-path target assembly) and 483c579 area-map malformed src_deps shapes (non-object, non-array, non-string dep, 300-char area name overflow). Selftest now 47.
- Measurement 04:48: 1572 dark. Real-file gains: engine.c 168 to 108, report 17, main.c 73. But the assert-arm residual grows with every added test: tests.c count 1169 to 1260 with dark 292, tests_engine 65 to 107, tests_ops 105 to 137. Net movement is small because each new BT_CHECK contributes a structurally untaken arm. ESCALATE 3 is now the deciding factor for the endgame: without a hub policy on assertion arms, test files can never read 100% no matter how many tests land. I keep closing reachable dark in non-test sources and add tests only where they close real paths.
- Measurer hardened with one retry per stem after a transient spawn failure: the 04:47 run had 5 policy driver spawn failures with empty output from a just-relinked exe lock (the plain suite was green, the rerun clean).
- Transient exe-lock flake added to the known-issues list for the hub's wave-end coverage run: spawn-heavy suites can fail once right after relink.

## follow-on batch, 08:52

- 8f986be: both policy suites formatted for the fmt gate.
- da6a7d3: literal 12 centralized as FX_RETURN_COL and EXPONENT_FLOAT_MUTANTS at tests_ops.c file scope, policy scan no longer flags it. Same commit links the alloc seam object into the ws fixture: the core lane's db5f9e7 gave bt_trace a seam dependency and the ws baseline stopped linking, now green.
- 5e53421: 12 allocation sites across fs.c, token.c, operators.c, mutants-gate lib.c routed through bt_malloc, bt_realloc, bt_calloc, with arm(0) fail closed tests in both suites (read_file, tokenize, paths_push, enumerate, gate_run, gate_merge).
- 1f6b53b: bt_mut_mkdirs' only failure path, its to_wide allocation, now driven through the seam, converting the last classified mkdirs arm from unreachable to covered.

## relaunch 4, 2026-11-06 19:0x

lane-cov-mut4, generation 4, regression investigation. Toolset probe passed (all 6 tools). Task: mutate_driver.selftest_passes fails, mutate.exe selftest exits 1 at case mutate.engine_stages_the_suite_exe_inside_the_workspace (tests_engine.c:98, baseline verdict engine.c:854), plus a stale count pin 55 at tests/unit/tools/mutate_test.c:21 versus a 54 case table after the tests.c to tests_scratch.c split. Lint lane ruled non-causal, HEAD fails. Plan: reproduce under bt.py, read the baseline verdict path, bisect suspects (4bfecfd coverage_retry_unit, 4f9a6a9 strbuf zero-cap, db5f9e7 bt_trace alloc routing, suite exe relink changes), fix root cause red-green, fix the count pin to the true table count, lint, deliver.

## relaunch 4 incidents

- 19:0x reproduced: bt.py test-unit tools --only mutate_test fails only mutate_driver.selftest_passes, selftest prints "suite mutate: 53 passed, 1 failed" (54 cases in the table), only wsstage fails at tests_engine.c:98. Direct mutate.exe selftest runs fail identically, deterministic across 3 runs.
- 19:1x manual replication of the engine baseline (compile fx_ws_target, link the 5 ws objects, stage into build/bin/mutate-run, run from repo root) passes end to end. A playground probe (playground/wsstage_probe.c) running the 21-test selftest prefix plus the real bt_test_mutate_engine_stages_the_suite_exe_inside_the_workspace function against the current shared objects passes. The current sources are correct.
- 19:2x ROOT CAUSE: build/bin/mutate.exe is stale, linked 08:44:22 from tests.obj built 08:37 (before da6a7d3 08:47:26 added alloc.obj to the ws fixture link list) together with bt_trace.obj rebuilt 08:44:21 by db5f9e7 (bt_trace now needs the alloc seam symbols). That exe's ws baseline link misses alloc.obj, fails with unresolved externals, verdict BUILDERR, baseline refusal at engine.c:854. da6a7d3 fixed the source but was verified only in the lane clone, the shared tool exe was never relinked since 08:44:22.
- Why every later run kept the stale exe: bt.py link_stage line 909 skips a tool exe when only.replace("_","-") is not in the tool dir name. With --only mutate_test the probe string is "mutate-test", the tool dir is "mutate", no match, link skipped, silently. The suite exe (line 940, only in stem) still relinks and runs, so the suite spawns the stale tool exe. Every regression-hunt run used --only mutate_test. The lint lane's rebuild also went through a filtered run, so its "HEAD fails identically" verdict ran the same stale binary. Tool dirs without a hyphen (mutate, policy) hit this, hyphenated dirs (mutants-gate, ga-run) do not.
- Second defect confirmed: tests/unit/tools/mutate_test.c:21 pins "suite mutate: 55 passed, 0 failed" but the selftest table holds 54 cases, so even a correct exe fails the pin. Fix: pin states the true table count.

## relaunch 4 verdict and escalation

- 19:3x the reported second defect is the same staleness artifact, not real: the stale exe's table predates 5e53421 (08:50:27), which added mutate.allocation_arms_fail_closed as entry 55 and moved the pin 54 to 55 in the same commit. Current sources hold a 55 entry table and the pin at 55 is correct. Static count of the table in tools/mutate/tests.c: 55 entries, the missing name in the stale run was exactly allocation_arms_fail_closed. One root cause, no source defect in the area.
- Fix applied: refreshed the shared build/bin/mutate.exe via python bt.py test-unit tools --only mutate (the filter form that matches the tool dir and relinks it), then re-verified the hub reproduction command --only mutate_test.

ESCALATE 7 for the hub, two items:
1. bt.py link_stage tool filter (bt.py:909) never matches a hyphen-less tool dir from a suite stem filter: only.replace("_", "-") turns mutate_test into "mutate-test", which is not in "mutate", so every `--only mutate_test` (and `--only policy_test`) run relinks the suite exe but silently skips the tool exe relink. A tool whose objects changed keeps serving its old binary, and the skip is invisible in the log. Proposed root fix, hub owns bt.py: drop the trailing suite marker before the containment test (match only, only.replace("_", "-"), or only.removesuffix("_test") against tool_dir.name), or reuse matched_tool_dir for the tool loop.
2. The whole shared build/bin is one stale generation: every tool exe carries mtime 08:44:22 while later commits changed tools and src/common sources (mutate tests.c twice, strbuf.c in 4f9a6a9). Lane flows run in lane clones, so nothing in the shared tree relinked them for 10 hours. The hub should run one unfiltered build or test pass in the shared tree so the shared exes match HEAD before the wave-end gates, and the CI watcher flow should treat a tool exe older than its newest object as a build failure.

## relaunch 4 delivery, 19:4x

- Verification: python bt.py test-unit tools --only mutate relinked build/bin/mutate.exe at 19:17:51 and both mutate suites passed (tools_mutate_test, tools_mutate_oom_test). The hub reproduction command python bt.py test-unit tools --only mutate_test then passed 1 suite, 0 failed. Direct selftest on the refreshed exe prints "suite mutate: 55 passed, 0 failed" exit 0, the pin at tests/unit/tools/mutate_test.c:21 passes as written. No source file in the area changed, the regression needed no code fix.
- Lint: python bt.py lint tools reports 85 clean, 4 failed, all 4 in the verify lane's in-flight files (release_verify_inflate_test.c, release_verify_png_test.c, release_verify_spawn_test.c, verify_ev_bigcube_test.c), the documented ESCALATE 6 debt class. No mutate-owned file failed.
- Probe committed at playground/wsstage_probe.c: reproduces the engine ws baseline in isolation and runs the selftest prefix plus the real staging test in one process, the tool that separated stale-binary behavior from source behavior.
- Probe artifacts purged: temp probe trees, staged ws exe, scratch logs, build/test-tmp probe binary.

Reading log:
read artifacts/progress/lane-cov-mut.md full - inherited state, escalations, lane history
read tools/mutate/tests_engine.c 60-150, 205-340 - failing case, fake root tests
read tools/mutate/engine.c 468-660, 652-905 - prepare_scratch, run_one, baseline verdict path
read tools/mutate/buildcmd.c full - flag assembly and link argv
read tools/mutate/tests.c 1-200, 740-1030 - setup, ws link list, selftest table
read tools/mutate/mutate.h grep structs - engine and target fields for the probe
read tools/mutate/tests_shared.h full - timeout constant, shared helpers
read tools/mutate/spawn.c 85-312 - spawn capture, compile_file rc contract
read src/common/paths.c grep workspace_root, config_path - workspace walk
read tests/unit/tools/fixtures/fx_ws_main.c, fx_ws_target.c full - staged suite behavior
read tests/unit/tools/mutate_test.c full - selftest pin and driver spawn idiom
read tests/unit/tools/spawn_util.h 185-244 - repo root derivation
read src/common/bt_harness.c 40-124 - in-process table runner
read bt.py 868-952, 1341-1385, grep only/lint - link filter mechanism, lint area map
read build/obj/70dcf3245464/tests/unit/tools/fixtures/fx_ws_target.d - dep inputs
read scripts/commit_lane.py full - commit protocol

## unreachability proofs, item 5

- fs.c read_file seek and tell arms: fopen("rb") on a directory fails at open on the MSVC CRT, so only regular files reach fseek(END)/ftell/fseek(SET), and those calls are specified to succeed on seekable file handles, which every CRT file handle backed by a Windows file is. Proof re-derivation: read fs.c lines 21-39 plus the CRT fopen contract.
- fs.c read_file short read: the byte count comes from ftell on the same handle with no writer between the calls, the fixtures never mutate mid-read, so fread cannot come back short. Same shape for write_file's short write and fclose arms: they require disk full or quota exhaustion, not an honest driver.
- gate main.c read_arg seek, tell, oom, short read arms: same regular-file argument as fs.c, the directory case fails at fopen on this CRT.
- engine.c run_one compile spawn failure for the missing-clang case is drivable but the link spawn failure for the same override is preempted by the compile failing first with a stub exe present, and with a real clang the link spawn cannot fail while the compile spawn succeeded, proof: engine.c 666-717 order.
- report.c 144-145 class arms are bt_strbuf init and serialize arms living in common, ownership is the common routing lane, not this one.
- Remaining mkdirs error paths in prepare_scratch (cannot create scratch, cannot create inputs, cannot create input dir) call bt_mut_mkdirs whose Windows body ignores CreateDirectoryW results and returns 0 unconditionally, fs.c 131-155, so those arms are unreachable on this platform by construction, and the one reachable failure inside mkdirs is now seam-driven per 1f6b53b.

Commits this lane: a236e79 policy batch, 03a5093 report fail closed plus driver verbs, b53149c scratch root and orphan pid, 8637924 gate driver edges, ad2176e exponent literals, 1c3e6d6 obj root resolution, 0eaa6b3 engine refusal paths and fake roots, 483c579 area map shapes, e9e1831 prepare scratch write failures and dep copy rules, 159158d tokenizer edges, 68abbf3 gate violations and allowlist trims, 534f006 scratch suite split under the sloc cap plus the fsutil suffix fix and the measurer, 430007b spawn refusal arms, link argv overflow, continuation spans. Two root-cause fixes landed in product code: report refuses an unreadable ledger, and the policy junction planter writes a valid reparse buffer.

Measured end state, last full run before the final two commits: 1600 dark of 9048 regions over the three areas, from 1974 dark at handoff. Non-test sources closed to: engine 87, gate lib 46, token 44, fs 65, spawn 49, operators 66, main 73, gate main 47, ledger 30, buildcmd 30, union 30, report 17, policy lib 67, policy tails under 15 each. The remaining mass is classified: assertion arms in the 5 test files (hub ruled them out of my residue, macro restructure deferred to a dedicated lane), OOM arms in every allocator wrapper, IO defensive arms in fs and read_arg, and the Windows-unreachable mkdirs error paths. ESCALATE 4: the core lane's new src/common/bt_harness.c includes profile/InstrProf.h under instrumentation and my instrumented clone cannot compile it, the hub coverage run needs the clang resource include or that include needs a guard, this breaks scoped measurement for every lane. ESCALATE 5: policy flags tools/mutate/tests_ops.c:73 literal 12 recurring, it is a test count expectation, hub owns constants_allow.txt. ESCALATE 6: lint failures in other lanes' in-flight files (ga_run_inputs_test.c trailing commas, sleeper_test.c trailing comma) will fail the tools lint gate at wave end until those lanes format them.

Oracle ledger: driver exits from the usage text the tools print, ledger and union formats pinned by the committed suites, fixture files authored in test-tmp, float variant arithmetic derived from the numeric tweak rules, scratch ownership from the marker and creation clock contract, allowlist grammar from the parser's own error strings.

Reading log:
read artifacts/progress/lane-cov-mut.md full - inherited state and escalations
read tools/mutate/main.c full - driver verbs for spawn coverage
read tools/mutate/report.c full - fail closed fix
read tools/mutate/scratch.c full - orphan judgment branches
read tools/mutate/engine.c 68-260, 260-470, 455-630, 655-905 - obj root, targets, prepare scratch, run one, engine run
read tools/mutate/ledger.c 100-140 - missing file semantics
read tools/mutate/mutate.h structs and api - signatures for crafted fixtures
read tools/mutate/operators.c 140-270 - numeric core and variants
read tools/mutate/token.c 80-140 - continuation arms
read tools/mutants-gate/main.c full - driver edges
read tools/mutants-gate/lib.c 340-460 - allowlist grammar
read tools/mutants-gate/tests.c fixtures and table - idiom and registration
read tests/unit/tools/mutate_test.c full - driver spawn idiom
read tests/unit/tools/mutants_gate_test.c full - gate driver fixtures
read tests/unit/tools/spawn_util.h full - timeout and bin path contracts
read tools/mutate/tests.c, tests_ops.c, tests_engine.c partial - extensions and the split
