# adv80b: adversary B on the t80 replay leg footprint

Scope: commits 27f75d1 (replay leg), 827e098 (probe plus lane log), 672e550 (progress close). Independent of adv-80-a.

## log

- 2026-10-31 read the 3 commits, src/app/usage_script.c, usage_script.h, usage.c, usage.h, views/solve.c, views/replay.c, replay_scene.c 505-596, drivers.c 195-345, tests/unit/app/usage_script_test.c, usage_test.c, config/config.json e2e section, playground/t80/drive_usage.py, artifacts/progress/t80.md.
- 2026-10-31 static facts: script emits 20 shots, order menu..solve-review, replay, algs..settings; BT_USAGE_SHOT_MAX 19 -> 20 matches; shots array has no runtime bound check at usage.c:384 but the unit pin on 20 catches drift first.
- 2026-10-31 replay enters paused (replay_scene.c:523), tick no-ops while paused (577-580), progress 0 at elapsed 0; capture settle is 0.3 elapse plus the 0.4 shot dwell (usage.c:343).
- 2026-10-31 waits are bounded at 30s with a loud stuck_out exit 1 (usage.c:194-198, 353-356), unit covered in usage_test.c:271-279.
- 2026-10-31 probe_shared: shared build/bin/bt-app.exe --usage, fresh APPDATA, exit 0 in 26.8s, exactly 20 pngs, creation order matches the documented 19 plus replay right after solve-review, all pngs valid and non-blank, manifest_shared.json written, throwaways purged. PASS.
- 2026-10-31 probe_idempotence: run A fresh, B reusing A's APPDATA, C fresh. All exit 0 with the full 20-shot set. Store check: exactly 1 solve row after a fresh tour, exactly 2 after the reuse tour, so the replay leg logs no extra solve records. replay.png byte-identical across A, B, C. Byte drift across fresh runs exists only on solve-review and stats, both render wall-clock split times by design (solve.c:330-338, stats.c:262-268). Reuse drift on drill-card and case-drill shots comes from persisted ratings in the store, expected app behavior. PASS.
- 2026-10-31 test-unit app --only usage: 2 suites passed, 0 failed.
- 2026-10-31 probe_parent round 1 failed: temp trees lacked the gitignored vendor/ (raylib headers, sqlite amalgamation). Fixed by robocopying vendor/ and data/ into the exports. No temp residue left behind.
- 2026-10-31 probe_parent round 2 failed at link: build/vendor/raylib.lib missing in the exports, fixed by copying the prebuilt lib. Round 3 running.
- 2026-10-31 manifest checks: all 20 shots 1280x800, 20 distinct content hashes, replay.png 52347 bytes matching the lane log's size.
- 2026-10-31 gallery state on main: docs/screenshots holds 20 pngs without replay.png, README pictures numbers 20 entries with no replay entry, tgt_screenshots wipes and regenerates so a regen now emits 21 files. Lane log defers gallery and README regen to the hub after t79.
- 2026-10-31 no stale 19 pin anywhere: grep found 19 only in fsrs and corpus tests. bt_app_transition_frame applies the state and runs enter with no fade, so no mid-transition capture window exists.
- 2026-11-01 probe_parent round 3 PASS: both exports built clean (567 compiles each), current tour exit 0 with 20 shots, parent tour exit 0 with 19 shots, all 19 shared shots byte-identical between the two builds (including solve-review and stats in this pair), store tables identical, replay.png present only in current and byte-equal to the shared binary's replay.png, tour order is the parent order with replay inserted after solve-review. Temp trees purged, no residue.

## reading log

- read git show 27f75d1/827e098/672e550 stats - footprint
- read src/app/usage_script.c full - leg order and shot set
- read src/app/usage_script.h full - shot cap and settle constant
- read src/app/usage.c full - runner wait/shot/finish/stuck contract
- read src/app/usage.h full - dwell and stuck constants, shots array bound
- read src/app/views/solve.c 54-61, 227-245, 330-449, 460-566 - v entry from review, session free keeps memo, finish_solve store write, enter resets to ready
- read src/app/views/replay.c full - enter builds scene from solve memo, v exit requests solve, draw is 3d only
- read src/app/replay_scene.c 505-596 - playback init paused, tick guard, progress smoothstep
- read src/app/drivers.c 195-345 - usage loop key down/up delivery and screenshot capture
- read tests/unit/app/usage_script_test.c full - pinned 20-shot table, replay leg pin, elapsed kinds
- read tests/unit/app/usage_test.c grep stuck/exit - expected_shots 20 pin, wait timeout pin
- read config/config.json 240-280 - e2e shots list already carries replay, sloc cap
- read bt.py 1195-1305, 1686-1700 grep - build flow, tool resolution, migrations freshness
- read playground/t80/drive_usage.py full - lane probe contract
- read artifacts/progress/t80.md full - lane log and deferrals
- read src/app/persist.c 120-200, 400-472 grep - store save paths and db name
- read src/data/paths.c 195-230, 280-300 - APPDATA user root resolution
- read src/app/views/stats.c grep snprintf/record - stats renders timing text
