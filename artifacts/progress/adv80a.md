# adv80a: adversarial review of the t80 replay leg

STATUS: done

Scope: commits 27f75d1, 827e098, 672e550, attacking machine correctness, determinism, richer content, test fidelity, settle constant centralization, hygiene.

Approach: the shared checkout carries lane-79's uncommitted theme and settings work, and build/bin/bt-app.exe went stale against it, so all dynamic probes run in a scratch copy built from HEAD (robocopy plus git archive overlay) under the system temp dir, purged after.

## log

- 2026-10-31 static read of usage_script.c, usage.c, views/replay.c, views/solve.c, replay_scene.c, drivers.c, boot.c, logic.c, states.c, smoke.c, main.c: the leg's state path holds by line citations, see findings.
- 2026-10-31 test diff check: 27f75d1 changes to usage_script_test.c and usage_test.c are purely additive, 19 -> 20 pins plus the new replay leg test, nothing weakened.
- 2026-10-31 settle centralization: 0.3F appears only as the BT_USAGE_SETTLE_SECS define in usage_script.h:9, all four call sites use the macro (usage_script.c:194, 206, 210, 223), constants_allow.txt holds only the 0.1 row for this file.
- 2026-10-31 policy: comments and sloc scans exit 0 repo-wide; constants scan flags only src/app/views/settings.c literal 2.0 x8, which is lane-79's uncommitted working tree (committed settings.c has 3 occurrences), not the committed tree and not a t80 file.
- 2026-10-31 residue: artifacts/tmp (Oct 15), artifacts/usage-user (Oct 28, screenshots flow by design), artifacts/tmp-adv80b-idem (live counterpart agent), all gitignored, none from t80.
- 2026-10-31 scratch pipeline first run failed: build wanted build/vendor/raylib.lib which the copy excluded. Fixed by copying build/vendor into the scratch. Rerun in flight.
- 2026-10-31 scratch pipeline green: build 42s, drive 70s, redfirst 41s. Both usage tours exit 0 with the exact 20-name shot set, smoke exits 0 with its 10.
- 2026-10-31 determinism: 18 of 20 usage shots byte-identical across the two runs, replay.png included. solve-review.png and stats.png differ: their text carries the wall-clock memo and exec splits of the finished solve, pre-existing legs untouched by t80.
- 2026-10-31 richer content: usage replay shows all 6 face colors (26850 to 67988 px each of 1024000), smoke replay also shows all 6 (24586 to 68084), and the two differ by 211096 px. Both are scrambled cubes, distinct scrambles. The smoke baseline is not a solved default cube: the smoke tour enters solve before replay and bt_view_solve_enter_state rebuilds the session with has_memo set, so the solved-default premise in the review brief is wrong while the lane caption itself holds by bytes.
- 2026-10-31 red first: parent sources restored in the scratch, test-unit app exit 1 with 41 passed 2 failed, app_usage_script_test red at the 20-shot pins (usage_script_test.c:109, 124, 133) and the replay-leg test aborts the suite exe, app_usage_test red at usage_test.c:376. Landed sources restored, 43 passed 0 failed. Log in playground/adv80a/redfirst.log.
- 2026-10-31 probe defect owned: drive_and_compare.py printed FAIL usage1/usage2 run checks from its own sorted-name-order assertion (names[-1] == settings is meaningless on a sorted list). Every app-level check passed: exit 0, 20 shots, exact name set. Assertion removed from the committed probe.
- 2026-10-31 shared checkout hazard hit: build/bin/bt-app.exe went stale against lane-79's uncommitted theme and settings work mid-review, which is why every dynamic probe ran from a HEAD scratch copy, purged after.

## reading log

- read src/app/usage_script.c full - tour action idiom and the landed leg
- read src/app/usage_script.h full - shot cap and settle macro
- read src/app/usage.c full - wait, elapse, shot, stuck contracts
- read src/app/views/replay.c full - enter resets orbit and playback, v exit, draw inputs
- read src/app/views/solve.c 1-210, 420-580 - session rebuild, finish, v entry, enter resets phase
- read src/app/replay_scene.c 460-597 - playback init paused, tick no-op when paused, progress 0
- read src/app/drivers.c 29-110, 130-345 - vk mapping, usage_loop key phases, screenshot point
- read src/app/boot.c full - state table rows and transition enter
- read src/app/logic.c grep - request, pending, nav_to_menu
- read src/app/states.c full - tour order and slugs
- read src/app/smoke.c 1-100 - smoke tour shape for the baseline comparison
- read src/app/main.c full - mode dispatch
- read tests/unit/app/usage_script_test.c full - pinned tables
- read tests/unit/app/usage_test.c grep - expected_shots pin
- read bt.py 1374-1380, 1447-1460, 1765-1772, 2120-2185 - child_env, suite output, policy, screenshots flow
- read tools/policy/main.c, lib.c grep - scan modes and root
- read config/constants_allow.txt grep - usage_script rows
