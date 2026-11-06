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
