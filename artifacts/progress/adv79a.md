# adv79a: adversarial review of the t79 settings and font footprint

date 2026-11-01, attacks commits e5f0f9b, 96ac17d, 7c66435, 6147085, fd1b181

## probes

- playground/adv79a/probe_layout.py: band and edge-clip decoder over captures
- playground/adv79a/probe_review.c plus build_probe.py: rebuilds the old and new solve review fixtures at 1280x800 through the real draw path and captures them
- playground/adv79a/probe_select.c: prints the real settings layout rects at 1280x800 and 640x480 and drives one click through bt_view_settings_draw
- captures: shots/settings.png, shots/solve-review.png, shots/algs.png from a fresh usage tour, probe_out/review_seed7_null.png, probe_out/review_seed4_r.png

## findings

1. confirmed. solve review overflows the window at the new scale and the committed test dodges it. the old fixture itself (seed 7, null scramble, empty corpus, exactly what view_draw_b used before) renders 8 broken lines: the danger block runs y 621 to 793 in an 800 px window and the phase hint is fully off screen (probe_out/review_seed7_null.png, probe prints "seed 7 broken lines 8"). the changed test swapped to a single R scramble with 3 broken lines so its gap assertion still passes (probe_out/review_seed4_r.png). production reach: the no-vendor boot mode (logic.c 361, "running on user data only") and any partially uncovered corpus produce the same unbounded block, broken cap is 16 (BT_SOLVE_BROKEN_MAX). at root 20 the budget was about 20 lines, at 28 it is 5. judgment: fitted test hiding a production regression, not a scope reduction, because the new fixture sits below the failure threshold while the defect stays reachable. root fix: bottom-anchor the hint inside the content rect and clamp the broken block to the remaining height with a trailing count line, clamping alone hides why the solve failed.
2. confirmed. the visual chain is broken and was not run. fresh usage captures differ over tolerance from the committed baselines for every shared name: algs 28098 px, menu 18280, replay 211096, settings 19437, stats 13332 (visual-diff.exe exit 1, probe run 2026-11-01). baselines were last regenerated in 0a7b7c6, before the rem lift. the lane verification list has red, green, lint, fmt, policy only, no e2e, no visual, no fuzz, no mutation, and no app mutation ledger entry exists for the window.
3. confirmed. one click in the open mode dropdown also focuses the drill reps field. dropdown option 0 spans y 485.4 to 558.2 and option 1 spans 558.2 to 631.0 while the reps control spans 530.2 to 603.0, same x range. probe: a click at y 535.2 inside option 0 returned "select open 0, reps focused 1" from a single input, because bt_view_settings_draw runs the reps number field before the select. the next typed digit then edits reps with select-all semantics. also latent: at 640x480 option 2 spans 471.0 to 543.8, 63.8 px past the 480 px window, and layout_fits_the_small_probe_window_too checks rows and controls but not the open dropdown extent.
4. held. bt_view_settings_enter_state resets the draft and changed only, never the widgets. two escape events delivered inside one frame (injected input or a repeat burst) both drain in bt_app_process_keys before any settings draw runs, so the select can stay open and a number field can stay focused across a menu round trip. practically unreachable with physical repeat rates, but the state should reset on enter.
5. held. the drill geometry pin 538.72 does not match the formula value 538.56 (0.44 of 1224) and carries 13 px of slack against an actual net width of 525.6, so it cannot catch a modest geometry regression. recomputed pins elsewhere check out: frame metrics 28/98/1.4/126/1224/646 and the drill split pins are exact formula derivations.
6. held. hygiene: t79.md decisions still say 2.6 rem rows and 1 rem gap while the code ships 3.2 rem rows. the gap and hint gap in bt_view_settings_layout_for are inline multipliers (spacing times 2.0 and 0.5) beside named BT_SETTINGS constants, tolerated by the policy scan but inconsistent with the hub rule. zero comments and sloc caps hold across the touched files, tracing covers the new units including a fail path on empty rows.
7. held. the red log is a genuine stub-stage red: layout zeros, select never opening, cursor never moving, and the new body pixel floor failing at root 20. the root_rem pin flip itself was never red, which is inherent to a deliberate constant change, and the theme suite red predates the pin update.

## what held under attack

- settings layout at 1280x800: 3 rows, labels left at x 291 with hints under them, controls right-aligned ending x 1004, column centered with 248 px gaps both sides, steppers and value inside the control rect, no edge clipping, measured from the real capture.
- the select state machine: collapsed default, click and mode key open, arrows and digits move, enter commits, escape and outside click close without a commit, click on the control collapses, digits past the row count ignored, no draft and store desync found, escape routing through input_focus keeps the view.
- arrow plumbing: arrow_up and arrow_down are additive, filled beside enter and escape in bt_app_poll_input, the algs view reads arrows through the key queue not bt_ui_input, no other consumer exists.
- algs capacity 13 re-derived: (800 - 154 - 84) / 42 matches bt_view_algs_list_capacity at 28 rem.
- every other tour view decodes without edge clipping except algs: the algs detail panel is unbounded (mod.c 698) and its shipped capture already ends at y 798 of 800, so any case with one more detail line clips, flagged as part of finding 2 fallout.

## reading log

read src/app/views/settings.c full - layout math, widget flow, draw order
read src/app/views/settings.h full - draft, widgets, layout contract
read src/app/ui.c full - select transition, number field click semantics
read src/app/ui.h full - input and widget structs
read src/app/window.c 72-131 - poll keys and poll input idiom
read src/app/theme.c 1-40 - root rem table
read src/app/views/frame.c full - frame metrics contract
read src/app/main.c full - frame loop order
read src/app/drivers.c full - usage and smoke drivers, frame input publish
read src/app/logic.c 180-260, 350-400 - escape routing, no-vendor boot
read src/app/views/solve.c 85-230, 227-350, 430-646 - broken outcome paths, review lines, draw stack
read src/app/views/algs/mod.c 160-200, 620-744 - list capacity, unbounded detail panel
read src/app/views/drill_view.c 1-60 and src/app/net.c 150-195 - geometry pins
read tests/unit/app/view_settings_test.c full - layout and key assertions
read tests/unit/app/ui_test.c full - select transition suite
read tests/unit/app/view_draw_b_test.c 200-320 - changed review fixture
read artifacts/progress/t79-red.log grep fails - red claim
read artifacts/progress/t79-green6.log tail - green claim
read artifacts/progress/t79.md full - lane decisions and verification list
read scripts/commit_lane.py full - commit path
read tests/unit/app/common.h 40-80 - temp capture locations
grep arrow_up, arrow_down, bt_app_frame_input, BT_SOLVE_BROKEN_MAX - consumers and caps
