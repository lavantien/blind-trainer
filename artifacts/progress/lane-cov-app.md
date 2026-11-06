# lane-cov-app progress

CURRENT STATE (2026-11-06T13:33, lane-cov-app8 delivered and stopped):
src/app regions 7571/8037 (94.2 percent), 466 dark, all 48 app suites green in the final instrumented run, report at artifacts/lane/lane-cov-app/scope11.log. Before this generation: 7292/8026 (90.9 percent), 734 dark. The honest tail from scope9 is fully driven or ruled: memo.c, drill_card.c, usage.c, and driver_keys.c are fully green, and every remaining dark region in the touched files carries a proof or an environment ruling in finding 11. Successor surface if another generation runs: ui 39, mod 53, drivers 40, solve 27, state 25, stats 23, drill 23, window_focus 66 env-bound two-sided, open_url 12 reserved for the user, plus the small tails in boot, config, fonts, logic, persist, menu, replay, layout, display, drill_view, case_drill_view, fatal, frame, smoke.

DELIVERY SUMMARY (lane-cov-app8):

Numbers: 7292/8026 (90.9 percent, 734 dark) before, 7571/8037 (94.2 percent, 466 dark) after. 279 regions newly covered, 11 new regions added by the wheel injection function and all covered, 268 dark regions closed net. 12 commits: 4599073 d7b5bcd d7e41be b8c48a3 985f55b dd1bf70 f288520 8661fa1 4365a7f 2a37117 ef08865 e42e2dd.

Driven to green: memo.c 335/335 (residue lines with hand-built parity, flips, twists, none, and cap stops, syllable malloc failure, cadence zero tick, recents window, row draw guards, wrap clamps), drill_card.c 222/222 (micro and unknown styles, alg copy failure, user solver, span budget stops on every push, b_first order, pinned and bare hints, row budget, cap stops), usage.c 177/177 (seed wraparound, dir creation and failure, set actions OOM, empty shot file, every wait name plus unknown, stuck shot), driver_keys.c 119/119 (wheel injection and its dead-window failure).

Driven with proofs for the rest: detail.c 286/289 (engines, override arms, schema-break submit and delete), browse.c 407/408, case_drill.c 439/443 (state malloc arm at seam index 4, sq1 unappliable inverse, set pending OOM), settings.c 200/202, replay_scene.c 378/382, window.c 224/238 (vsync, forced fail, posted keys and wheel, font face names, missing bold, missing codepoint, overflow).

The case_drill 192-196 question resolved: bt_pz_alg_create aborts under the seam, but arming after the two alg creates and their two growth reallocs (arm index 4) fails only the state malloc, so the arm drives green with no death-test spawn. The 198-207 claim was wrong: a sq1 alg whose inverse slashes a straddled state parses then fails to apply, now pinned by test.

READING LOG (lane-cov-app8):

read artifacts/progress/lane-cov-app.md full - predecessor state and rulings
read artifacts/lane/lane-cov-app/scope9.log full - successor map and dark regions
read artifacts/lane/lane-cov-app/config.json full - lane paths and tools
read playground/cov_scope_app.py full - measurement flow
read src/app/views/algs/detail.c full - geometry and override arms
read tests/unit/app/view_detail_test.c full - existing suite and fixtures
read tests/unit/app/common.h full - temp and file helpers
read src/app/views/algs/browse.h full, browse.c 1-544 - tree, navigation, and arms
read src/app/views/algs/mod.c 375-414 - algs enter flow
read tests/unit/app/views_case_drill_test.c greps - fixture patterns
read src/puzzles/nxn.h full, pyra.c 30-160, mega.c 1-50, sq1.c 64-121 and 332-472, tier1.c full, geometry.c 26-180 - engine notations and alloc behavior
read tests/unit/data/commutator_test.c 88-249 - b_first fixtures
read src/app/views/drill_card.h full, drill_card.c full - card arms
read src/app/views/drill.h 30-69 - session struct
read src/app/views/memo.c full - memo arms
read tests/unit/app/view_memo_test.c full - suite and font harness
read src/scheme/memo.h 25-99 - residue struct
read src/common/bt_harness.h full, bt_harness.c greps, alloc.c full - harness and seam
read src/data/db.h full, repo_user.c greps - schema surgery
read tests/unit/app/algs_more_test.c full - override fixtures
read src/app/window.h full, window.c full - window and font arms
read tests/unit/app/window_test.c full - suite
read src/app/driver_keys.h full, driver_keys.c full - input injection
read vendor/raylib/src/rtext.c 614-693 - missing glyph semantics
read src/app/ui.c 434-508 - number field edit path
read src/app/views/settings.c full, tests/unit/app/view_settings_test.c full - settings arms
read src/app/replay_scene.h full, replay_scene.c full, tests/unit/app/replay_scene_test.c parts - scene arms
read src/app/usage.h full, usage.c full, tests/unit/app/usage_test.c full - tour arms
read src/app/views/case_drill.c full - proofs, state malloc, sq1 apply
read src/app/main.c 1-99, boot.c 89-164, logic.c 340-409 - main 52 proof
read artifacts/lane/lane-cov-app/build/gen/app_view_detail_test_main.c full - test ordering

INCIDENTS AND FINDINGS (app8 additions):

10. detail.c proofs. 72-75 dead: bt_view_case_site_of returns early when the node is NULL, and a non-NULL node at depth d implies every prefix walk node_at(1..d) resolves, so the walked-NULL arm cannot fire. 95-98 dead: bt_pz_nxn_init fails only outside 2..7 and every nxn_check caller passes 2, 3, or a size clamped into 2..7.

11. Re-derived unreachable rulings. main.c 52: bt_app_init loads only args and config, bootstrap (the sole has_corpus setter) runs later as the BOOT-state enter at the main loop, so the corpus arm of the select is the dead one, the NULL arm always runs, and the corpus glyphs never reach the font atlas at startup, worth a product look. case_drill 110-113 dead: every display_faces table is a permutation of its engine_faces table (skewb reorders CUBE_FACES, fto reorders FTO_FACES, others identity), unknown events return at the empty-list guard. case_drill 212-215 dead: geometry_grid is static and every dispatch branch passes a literal known event with a non-empty face table. case_drill 221-224 dead: nxn_grid is static, called only from the bigcube branch with the size clamped to 2..7. case_drill 198-207 LIVE for sq1: a slash is rejected when a wide piece straddles the cut after rotations, so an alg whose inverse slashes a straddled state parses but fails to apply, driven by test in app8. browse 464-466 dead: bt_view_list_window already clamps start to len-count so the re-clamp never fires. replay_scene 288-290 dead: the 54 pos-normal pairs are closed under quarter rotations about the coordinate axes, so a turned cell always lands on a cell. settings 112-114 dead: 3.2rem minus 2 times 0.3rem is 2.6rem which always stays above root_rem. settings 81-83 metric-bound dead: pinned by test, no hint measures wider than the widest label under the shipped theme and NotoSans. window 45 46-48 71 74-76 environment-bound: InitWindow clamps a zero size instead of failing, a genuinely failed create needs a desktop-less session. window 234-239 dead: raylib 6.2 LoadFontData skips missing codepoints so glyphCount equals the request only when every codepoint was found, the miss arm after the count check cannot fire.

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
