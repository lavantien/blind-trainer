# lane-lint-fix progress

Toolset probe at 2026-11-06T18:42+07:00: Glob, Grep, Read, PowerShell, Write, Edit all functional.

## Current state

Delivered at 19:02. All 13 assigned files are lint-clean, confirmed by per-area runs and one full `python bt.py lint` run: 428 clean, 6 failed, every failure in live-lane files outside this lane (gendata_model_test.c, gendata_oom_test.c, release_verify_inflate_test.c, release_verify_png_test.c, release_verify_spawn_test.c, verify_ev_bigcube_test.c, the last one went red in the shared tree after this lane's tools-area pass at 18:51).

Commits: 192109a app, d7b0de4 common, 5f43855 core, f96eed8 data, d17b7f8 puzzles, a21717f scheme, db65209 tools.

Fixes: replay_scene_test.c 502 trailing comma, view_drill_card_test.c 571 trailing comma, view_memo_test.c 568/610 true and false literals, window_test.c 317 the dead wheel post now uses a destroyed driven window handle, bt_trace_test.c 176-179 CreateThread returns HANDLE so the uintptr_t casts are gone, alg_test.c 202/282 locals renamed text and err_token after the api parameters, repo_failures_test.c 526/541 icacls spawned through bt_spawn_capture instead of system, vt_roundtrip_test.c 191 flip constant 1U, guard_test.c 25/28 memset zeroing after {false} tripped missing-field-initializers, mutate_oom_test.c 13/14 macros to enum, mutate_test.c 726 trailing comma, tools/mutate/tests.c 868/894/952 trailing commas.

Suites green through bt.py test-unit: bt_trace, alg, repo_failures, vt_roundtrip, guard, replay_scene, view_drill_card, view_memo, window, mutate_oom. mutate_test holds only the pre-existing selftest failure below, every other case passes including the cases this lane edited.

## Incidents

18:56 Pre-existing mutate selftest failure, not caused by this lane. `python bt.py test-unit tools --only mutate_test` fails `mutate_driver.selftest_passes` (mutate_test.c:20-21): mutate.exe selftest exits 1, case `mutate.engine_stages_the_suite_exe_inside_the_workspace` fails at tools/mutate/tests_engine.c:98, baseline verdict path engine.c:854. Reproduced with this lane's tools/mutate/tests.c edit reverted, pure HEAD sources: same failure. No concurrent mutate.exe processes, 187 GB disk free, build/obj single objects dir with all three link objs present. Extra defect: the assertion expects "suite mutate: 55 passed, 0 failed" but the selftest table holds 54 cases (53 passed, 1 failed), so even a green engine run would fail the count check. Escalated to the hub for a mutate slot, lint fixes for the 3 mutate files are proven non-causal.

19:00 tools commit rejected once by the format gate: re-applying the trailing commas by hand kept the compact two-line wrap, clang-format requires the expanded four-line block, one clang-format -i pass settled it and db65209 committed clean.
