# lane-cov-app progress

CURRENT STATE (2026-11-06T10:0x, lane-cov-app7 delivered and stopped):
src/app regions 7292/8026 (90.9 percent), 734 dark, all 48 app suites green in the final instrumented run. Successor map: artifacts/lane/lane-cov-app/scope9.log. Lane config and covbuild tree unchanged. Remaining work is ranked in scope9: window_focus 64 env-bound, detail.c 60 with the dark tier1_engine_check fn, ui draw half, memo 54, drill_card 47, drivers 40, window 42, browse 37, solve and settings tails, replay_scene, usage, config defaults block, stats tail. Generation run log and reading log preserved in git history at f4c2ae0 and earlier.

INCIDENTS AND FINDINGS:

1. Focus watch deadlock, real product defect, fixed 1b2bd5b. The watch thread's GetWindowTextW on a foreground window owned by the same process but the non-pumping main thread is a synchronous WM_GETTEXT send, and bt_app_focus_watch_stop then WaitForSingleObject(INFINITE)s on that stuck thread. Fires only when sentinel seizure plus SetForegroundWindow both succeed, which past lock-screen runs never had, so the suite used to pass vacuously. Stepwise isolation in playground/app_lane/focus_stall_probe.c and focus_stall_probe2.c, logs in artifacts/lane/lane-cov-app/focus_stall_probe*.log. The title read is now a 25ms SendMessageTimeoutW with SMTO_ABORTIFHUNG and the test pumps messages during its waits.

2. Foreground-contention flake class, fixed a49aa90 80851c6. Sentinel acquisition asserts in test files fail stochastically under the 16-way parallel batch whenever another suite holds the foreground. Test files now use plain acquire calls, and the two focus tests whose subject is acquisition retry 10x100ms. If contention flakes reappear, the pattern is the retry helper in focus_test.c.

3. Dispatch fixture defect, fixed 4018f7c. dispatch_config_with_title had no srs section, so every interactive spawn failed boot at the missing-srs check and sat in the FATAL view: bt_app_boot coverage showed 4 hits on the srs-fail arm and 0 hits on the main.c update arm. Interactive fixtures must carry a full valid srs section or the spawn never reaches menu.

4. Dead and unreachable regions, ruling ledger. main.c 52 NULL-arm region is a dead select artifact, executed with count 0, uncoverable. case_drill.c 110-113 unreachable, every event's display faces are a permutation of its engine faces. case_drill.c 198-207 212-215 221-224 defensive arms with no public input path: apply_alg cannot fail after a verified parse, dispatched events always have a face layout, and the bigcube clamp keeps nxn_init inside 2..7. open_url.c ShellExecuteW arms stay reserved for the user.

5. Parked: case_drill.c 192-196 state malloc arm needs the exact allocation index because bt_pz_alg_create fatals under OOM instead of returning NULL, an arm loop aborts the suite process mid-index.

6. Environment: this harness session can run with an interactive desktop where foreground seizure succeeds, and locked where it never does. Focus verdicts must stay two-sided. sqlite happily creates missing parent dirs, a db-open-failure fixture must block the path with a regular file, see blocked_user_root in views_case_drill_test.c.

7. bt_smoke_step resets exit_code to -1 at entry on every call, so a step onto an already-DONE tour returns -1, the real smoke loop exits on the finish frame itself. Pinned in smoke_test.c.

8. Still-live escalations from earlier generations: readme env table needs rows for BT_APP_WINDOW_FAIL, BT_APP_FOCUS_CURSOR_FAIL, BT_APP_FOCUS_CLASS_FAIL, BT_APP_FOCUS_GIVEBACK, and the smoke exit code line should mention window closed mid tour as exit 1. The memo stream now runs the seed the intro shows, so memo, recall, and review baselines may drift and need a baselines recapture at wave end, never a manual edit.

9. Takeover correction recorded at relaunch: the silent gen-6 predecessor had in fact landed the net.c indexed table as dc2957c before going silent, verify HEAD before redoing a predecessor's task.
