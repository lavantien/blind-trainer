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

## final, 08:40

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
