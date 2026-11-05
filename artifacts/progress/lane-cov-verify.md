# lane-cov-verify: verification tools coverage to 100 percent

Baseline from the hub export: 1483 uncovered regions across 28 files in tools/verify (632), tools/release-verify (352), tools/visual-diff (315), tools/jobwrap (175), tools/sleeper (9).

## setup

- lane build config at artifacts/lane/lane-cov-verify/config.json, isolated bin, obj, gen, test_tmp.
- measurer playground/cov_scope_verify.py builds instrumented suites into bin_cov (bin and obj aliased onto the cov dirs) and reports per-file region gaps with uncovered line starts, per-group profile dirs merged on export.
- run 1 failed: suites spawned exes from bin_cov before any link landed there, fixed by the alias.

## findings while measuring

- sleeper_test.c bin_path tests deleted BT_BIN_DIR from the process env and never restored it, so every later spawn test in the same suite ran the plain build/bin exes instead of the instrumented bin_cov exes, silently losing all spawned-exe coverage. Fixed by saving and restoring the env value, committed in 7e49355. Sibling lanes with spawn suites may hit the same pattern, worth a hub check.
- default.profraw had landed in the repo root from earlier spawned exes losing LLVM_PROFILE_FILE through that env deletion, removed.
- shared build/env.json poisoning traced to my lane: every python bt.py test-unit run under the lane config reaches note_plain_obj_root (bt.py line 1084) with root equal to the repo root and the lane obj root, and dirs.env pointed at the shared build/env.json before the hub added the env redirect in c705b34, so the lane obj_root overwrote the shared state on each compile. Lane config refreshed at 23:23 with the redirect, the measurer re-copies the clone each run so future runs write only artifacts/lane/lane-cov-verify/build/env.json. bt.py doctor was never run from this lane.

## progress

- 2026-11-06 04:15 relaunched after the 00:48 provider window limit killed the predecessor. Toolset probe passed. Owned paths carry no uncommitted edits, the other lanes' in-flight files are untouched. Clone refreshed from the c705b34 lane_build with the env redirect. Env poisoner identification is already recorded in the findings section above for the hub. Registry drift landed as bb60324 at the cutoff. Open thread: the unimplemented-section fallback, then the remaining gap census to 100 percent.
- 2026-11-06 04:2x unimplemented-section fallback already landed too: verify_packs_test.c rows_for_an_unimplemented_section_still_report_one_outcome_each rides in bb60324. Fresh scoped measurement: 1132 dark regions across 41 files, top yields ev_bld.c 102, visual-diff lib.c 136, release-verify lib.c 123, visual-diff fs.c 98, ev_sq1.c 79, ev_cfop.c 67, release-verify inflate.c 69, zip.c 56, png.c 49, ev_nxn.c 51, bigcube 41, pyra 31, mega 25, jobwrap lib 47. Sleeper escalation re-judged drivable: plain 1e300 sleeps indefinitely (spawn, alive past 3s, kill), hold-secs ULLONG_MAX saturates line 75 (marker then kill), alloc failure 53 to 57 through a job object with a process memory cap so calloc fails at ~40 MiB with no system pressure. Attack order: ev_bld, visual-diff lib and fs, release-verify lib then zip inflate png exec, remaining ev_*, jobwrap, small tails, sleeper last.
- 2026-11-05 23:04 sleeper closed: lib.c 127 of 127, main.c 60 of 69. Commit 7e49355. ESCALATE: sleeper main.c lines 41, 53 to 57, 75, saturation and calloc failure branches, undrivable in a bounded test without an endless sleep or memory exhaustion.
- 2026-11-05 23:07 jobwrap tests written: load_config missing and broken, pid_dir empty and bad utf8, run rejection paths (non wrap kind, config without job section, anchor under a blocked pid dir), run happy paths with anchor and bare plus exit code passthrough 2, missing payload program, over budget command line, ffi broken and oversized lines, ffi dead handle assign resume wait, exe wrap path with bad anchor exit 2. Suites running.

## oracle notes

- exit code contracts come from the readme: visual-diff 0 match, 1 mismatch or missing baseline, 2 usage. sleeper 0 ok, 2 usage. release-verify 0 pass, 1 findings, 2 usage or harness error.
- png fixtures for visual-diff compare tests reuse the committed baseline shots under tests/visual/baselines, copied at runtime, byte-identical pairs for match and distinct shots for mismatch.
- simcheck fixture tsv rows under scratch dirs encode each parser rejection branch by hand.
- jobwrap expectations come from its usage text and memguard contract: usage exit 2, wrap passthrough of the child exit code, anchor pid files under memguard.pid_dir.
