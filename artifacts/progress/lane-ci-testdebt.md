# lane-ci-testdebt progress

Mission: make the 5 CI-failing suites green on the GitHub Actions runner. Run 42252595605 on 80f2000 failed at the test step (68 passed, 5 failed). Steps 1 to 12 including build were green, this is test-step debt only.

The failures:

1. tools_ga_run_flow_test, suite abort: cannot read data/ga/freq.json, "run python bt.py memofreq first" (arm run_all_fails_the_markdown_write_under_a_file_prefix).
2. tools_ga_run_io_test, suite abort: same freq.json abort (arm run_all_fails_closed_when_the_markdown_write_breaks).
3. tools_ga_run_oom_test, suite abort: same freq.json abort (arm load_fails_closed_across_the_entry_allocations).
4. tools_ga_run_inputs_test, 2 of 9 failed: load_accepts_freq_data_with_empty_pair_tables ("load rejected empty pair tables: read D:\a\blind-trainer\blind-trainer/data/vendor/cases.json") and load_reports_each_broken_input_stage.
5. tools_policy_driver_test, 7 of 12 failed: every spawn arm returns rc -1 want 0, exit -1 want 2, empty out and err (unknown_mode_prints_usage_and_exits_2, missing_config_exits_2, unparsable_config_exits_2, and 4 selftest arms, selftest_ok_maps_success_to_zero also policy_util_find_child 0 want 1).

Hub facts already established:
- data/ga and data/vendor are entirely untracked (git ls-files returns nothing): both are local generated trees, the runner has neither. freq.json comes from python bt.py memofreq. Establish what generates data/vendor/cases.json and what it costs.
- The CI test step runs tools "--without ga_run_lib", an exclusion that predates your fixes: the ga lane later added ga_run_flow, ga_run_io, ga_run_oom, ga_run_inputs suites with the same dependency. Prefer removing the exclusion by making the dependency satisfiable over widening it.
- The policy_driver signature (rc -1, empty out and err) means the spawn never produced output: the child did not start or died before writing. The spawn tests were never CI-validated, every recent run died before the test step, so runner behavior for the whole spawn idiom is unproven. Fix the class: absolute exe paths, explicit env, no cwd or console assumptions, and make policy_util_find_child's detection runner-proof.

Reproduction method: simulate the runner locally by renaming data/ga and data/vendor away and running the 5 suites through bt.py test-unit tools --only <stem>. That reproduces the runner state faithfully. Restore the trees afterward.

Fix rules:
- Root causes only, no skipped or relaxed assertions, no allowlists.
- Prefer test-side self-sufficiency: a test that needs derived data either builds its own scratch fixture or derives it from committed ref_ground sources. A CI-side generation step is acceptable only for genuinely shared derived artifacts, and any .github/workflows/ci.yml change must keep the badge job and all existing steps intact.
- Deterministic fixtures only, fixture origin recorded in the test.
- The concurrent lane-macro-restruct3 owns src/common/bt_harness.h, bt_harness.c, tools/mutate/tests.c, and tests/unit/common/bt_harness_test.c. Do not touch those four files.

Gates before delivery: affected suites green in the simulated clean tree, the full tools area green in the normal tree, fmt on touched files, python bt.py lint tools, python bt.py policy. Commits in small units through scripts/commit_lane.py (subject max 100 chars, explicit paths). First progress line within 5 minutes, poll long runs in your turn, never idle on a notification. The hub pushes and watches the CI run after your delivery: your definition of done is the local simulated-clean-tree green, the runner verdict belongs to the hub's watcher.

Current state: launched 2026-11-06 23:45 local, nothing landed yet.

- 2026-11-06 23:52 lane-ci-testdebt started, toolset probe written, reading repo layout and the 5 suites before the local reproduction.
- 2026-11-07 00:04 reproduced all 4 ga signatures locally with data/ga and data/vendor renamed away (flow/io/oom suite abort on freq.json, inputs 2 fails on vendor read plus freq copy). policy_driver passes locally in 1.7s, 12/12, alphabetical test order, spawn handles flat over 40 probes. CI log analysis: on the runner the suite took 307s, first 5 tests (11 spawns) passed, then every spawn returned rc -1 with empty out/err and the last test even failed fopen of the exe: progressive process-creation exhaustion under runner load, classic windows quota/commit saturation. probe playground/probe_spawn_leak.c committed later with the lane.
- 2026-11-07 00:05 cost findings: python bt.py gen-data from the clean tree TIMED OUT at tool_run's 300s cap, raw gendata.exe timing running now. gen-data cannot be a blind CI step until the cost is known.
- 2026-11-07 00:12 memofreq measured: raw exe 112s, exit 0, regenerated data/ga/freq.json byte-identical to the original (sha256 11609bd494b8a4a9487ecdfda4237e5b7552500c0e9b77aa918a21ce04b657db). memofreq is deterministic and viable as a CI step, but run_policy_exe's default 300s timeout is tight for a loaded 4-core runner, tgt_memofreq needs an explicit larger timeout.
- 2026-11-07 00:14 policy spawn class fixed in tests/unit/tools/spawn_util.h: split CreatePipe partial-failure leak, resolve the exe to an absolute path before CreateProcessW, bounded retry (3 attempts, 2s backoff) on pipe/process creation failure with the OS error kept in the diag. policy_driver_test.c now prints the diag plus errno on every spawn and copy_exe failure. Suite green locally after the change.
- 2026-11-07 00:16 gen-data verdict: raw gendata.exe from the clean tree ran 25 wall minutes and 415 CPU-seconds without finishing (killed by hand, no partial output), so data/vendor can never be a CI step. Design settled: data/ga/freq.json comes from a CI memofreq step (deterministic, 112s), the suites take scratch vendor fixtures derived from the committed scheme enumeration via the new tests/unit/tools/ga_run_fixture_util.h. flow/io/oom/inputs configs now point at the scratch vendor; ga_run_lib end_to_end and cross_check tests run under a spliced config with the same fixture, cross_check renamed to cross_check_passes_on_a_scheme_matching_vendor_file because it no longer reads the local-only generated tree. ci.yml gains a derived data step running memofreq and drops --without ga_run_lib. bt.py tool_run gained a timeout passthrough and tgt_memofreq passes 900s. All 6 suites green in the simulated clean tree, full tools area run in progress.
- 2026-11-07 00:21 regression caught and fixed: the first absolute-path resolve turned bare PATH names like tasklist into cwd-relative paths and broke release_verify_exec run_child_kills_the_tree_on_timeout, resolve now leaves names without a separator or drive untouched. Lesson recorded: a concurrent policy gate during a running test-unit pass relinks suite exes under them and corrupts that measurement, gates are strictly sequential now.
- 2026-11-07 00:32 delivered. Gates: full tools area 74 suites green in the simulated clean tree (freq.json only, vendor tree absent), full tools area 74 suites green in the restored normal tree, fmt green, lint tools 92 clean 0 failed, policy green. Commits e6fce46 (ga fixtures), 1fda1e5 (spawn hardening), d30e808 (ci), plus playground and progress commits. data/ga and data/vendor restored to their original trees. Reading log and root causes in the lane report to the hub.
