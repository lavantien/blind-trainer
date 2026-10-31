# adv79b: adversarial review of the t79 settings and rem footprint

Footprint judged: e5f0f9b, 96ac17d, 7c66435, 6147085, fd1b181 on main.
All probes live in playground/adv79b/ and ran against a fresh `python bt.py build` of HEAD (build log: artifacts/progress/adv79b-build.log).

## Verdict

3 confirmed findings, 3 held notes, everything else attacked and held.

## Confirmed

1. Dropdown click-through focuses or steps the covered reps field. The open mode select drops its option rows over the drill reps control: slot 0 spans y 485.4-558.2 while the reps control spans y 530.2-603.0 at 1280x800, a 28 px overlap band (playground/adv79b/geom.py). Live proof: clicking the lower band of option 1 (x 865, y 544) picked mode syllables and left the reps field focused with an accent ring (shots s_16_select_open.png, s_17_clickthrough.png, crop_s17_settings.png), then typing 9 changed reps 100 to 9 (s_18) and persisted drill_reps 9 to the settings db (drive.log). Clicking the left 72.8 px of that band hits the hidden minus stepper instead. Root cause: bt_view_settings_draw hit-tests cadence, reps, then the select in that fixed order (src/app/views/settings.c:271-303) and bt_ui_number_field acts on any click inside its rect (src/app/ui.c:379-392) with no knowledge of the overlay that bt_ui_select draws over it (src/app/ui.c:482-495, ui_select_option_slot at src/app/ui.c:155-163).
2. The visual regression gate is green on stale pixels. 96ac17d lifted every rendered text size by 40 percent, but tests/visual/baselines was last regenerated on Oct 28 in 0a7b7c6, before the change. `python bt.py visual` passes at HEAD only because artifacts/e2e holds shots rendered Oct 31 17:01, also before the change, so the diff compares two pre-t79 renders. Fresh smoke shots from the HEAD exe exceed the tolerance-48 gate on every text-bearing view (settings about 19.5k pixels over, solve about 79k, case-drill about 84k; measured against the baselines with the tool's own criterion). The chain's visual step proved nothing about the landed code. Needed: e2e-smoke refresh, `python bt.py baselines`, commit alongside the rem lift.
3. The solve review column bottoms out at the window edge. The right column stack (src/app/views/solve.c:603-631) has no clamping and grows with the failure detail. Live capture t_04_solve_review.png: with 3 split lines and 4 broken lines the hint line "space next solve, v 3d replay" ends at y 794 of 800, a 6 px margin (crop_t04_review_column.png). The block pitch is 22.4 px per extra broken line, so a failure with 5 or more uncovered pieces pushes the hint off-window and further failures push the broken lines themselves off. At root_rem 20 the same stack ended near y 570 with about 230 px spare, so this is a regression introduced by the rem lift against the fixed 1280x800 window.

## Held

4. The open select overflows the 640x480 unit test probe window: slot 2 bottom lands at 543.8 (geom.py). Test-only surface, the real window is fixed at 1280x800, and `layout_fits_the_small_probe_window_too` only checks the closed rows.
5. Focal text now renders at 95.2 px from the 64 px font atlas (src/app/window.c:169 loads fonts at base 64), a 1.49x upscale versus 1.06x before the lift. Softer edges on the largest text. Not measured as a defect, worth a base size bump.
6. One unreproduced window loss: the first deep tour run lost the app window during the memo space burst. Two clean reruns of the full sequence and an isolated memo probe (40 spaces, exit 0) did not reproduce it, so it stays a probe-side suspicion, not an app defect.

## Attacked and held

- Settings page reads normal at 1280x800: labels left at column x 276, controls right-aligned to the column edge, attached minus and plus steppers inside each number control, collapsed select with a v mark, hints under labels and left of controls. All labels, hints, option rows, and values fit their boxes with wide margins (geom.py text widths overestimate the real raylib widths by about 1.37x, so the fits are wider than computed).
- Body readability: nominal 28 px, measured lowercase bands 16 px and label digit bands 19-20 px (measure.py over s_01). The claimed 16.8 px worst band holds under my threshold.
- Input model live through pixels and db: typing digits while a field holds focus never touches the mode (s_04 to s_06 keep row 1 at per-orbit), the transition frame clears the frame input so the menu digit 6 does not leak into the fields (s_01 shows defaults), escape ordering unfocuses then leaves the view (s_12, s_13, s_14), digit key opens and commits the mode (s_09 to s_11), arrows move the cursor, enter commits, escape and outside click close without a commit (unit ui_test plus live s_16 to s_17).
- Persistence: typed 999999 clamps to 10000 and persists, plus at max leaves 100 unchanged, reload shows the same values in a fresh process, and the db JSON is identical across the round trip (drive.log).
- Scale sweep: menu, drill card, reps and rate, memo intro, stream and rate, solve ready, memo and exec phases, replay, algs and scrolled algs, case drill, stats, and fatal all render coherently at root_rem 28 in 10 smoke shots and 16 deep phase shots; the only tight container is the solve review column (finding 3). Stats needs 554 px of 646, memo centers a 252 px stack, drill stacks stay centered.
- Commit surface: e5f0f9b touches exactly the 8 claimed files, window.c is exactly the two arrow fills, ui.h is additive (two input flags inserted before chars, one new struct, two new declarations, no signature moved), and the views_stats_test rewrite replaces the removed hardcoded field geometry with the published layout API plus new select cases. Direction of fit note: those interaction tests take their click coordinates from bt_view_settings_layout_for itself, so a layout shift alone cannot fail them; the structural pins live in view_settings_test.
- Hygiene: zero comments added in src across the footprint, all touched files at or under 498 SLOC, new units trace entry, exit, and failure like their neighbors, and the 7 touched unit suites pass at HEAD.

## Probes

playground/adv79b/: layout.py (theme and settings layout math), geom.py (geometry and text width sweep), winlib.py (win32 input and capture), drive.py (interactive settings scenario with db round trip), measure.py (band and span measurement), tour.py (deep phase tour), memo_probe.py (memo stress rerun), analyze_tour.py (bounds sweep), shots/ (44 captures), drive.log, tour.log, memo_probe.out.

## Reading log

- read src/app/theme.c 1-111 - role table for the sweep
- read src/app/views/settings.c 1-339 - rows, draw order, key guard
- read src/app/ui.c 1-499 - number field, select, hit testing
- grep src/app/ui.h - struct caps and fields
- read src/app/window.c 1-253 - arrow poll, 64 px font atlas
- read src/app/main.c 1-99 - loop order and input clear
- read src/app/boot.c 1-165 - shot modes, state table
- read src/app/logic.c 150-269, 402-440 - escape gating, settings apply
- read src/app/views/frame.c 1-53 - content metrics
- read src/app/views/solve.c 505-632 - review stack, keys
- read src/app/views/memo.c 230-300 - centered stack
- read src/app/views/drill_view.c 100-180 - drill stacks
- read src/app/views/stats.c 240-410 - stats grid
- read src/app/views/algs/mod.c 540-610, 700-760 - list and detail draw
- grep src/app/views - px literal sweep
- read tests/e2e/main.c 1-329 - driver scope
- read src/app/smoke.c 1-120 - tour mechanics
- read src/common/paths.c 200-247 - APPDATA redirect
- grep src/data/repo_settings.* - settings table key
- git show e5f0f9b, 96ac17d, 7c66435, 6147085, fd1b181 - footprint audit
- read tools/visual-diff/lib.c 90-150, 280-310 - verdict criterion
- read bt.py 1252-1300, 1955-2018 - build and visual targets
- read scripts/commit_lane.py - lane commit path
