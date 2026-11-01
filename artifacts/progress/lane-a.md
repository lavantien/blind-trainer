# lane-a: shared ui foundation (dynamic rem, fitted draw, frame guard, widget clamps)

Ownership: src/app/theme.c,h src/app/ui.c,h src/app/views/frame.c,h src/app/boot.c src/app/main.c tests/unit/app/theme_test.c ui_test.c ui_draw_test.c view_draw_test.c

Progress lines, newest last:

- 2026-11-01 14:35 read theme.c,h ui.c,h frame.c,h boot.c main.c window.c menu.c(draw_fitted_text) drivers.c(open_window_and_fonts) harness + 4 owned test files; repo clean at 8ed5a9c.
- 2026-11-01 14:35 out-of-lane breakage predicted: tests/unit/app/window_test.c:175 pins fonts baseSize 84 (= 3.0 rem x 28.0), new default root 800/21.5 = 37.2093 bakes 112; window_test.c belongs to no lane, hub must reroute. drivers.c open_window_and_fonts (line 112) loads fonts without calling the setter, same gap for smoke/usage/fatal shots.
- 2026-11-01 14:36 baseline test-unit app starting, then TDD: theme root tests first.
- 2026-11-01 14:36 baseline green: 43 suites passed, exit 0. Theme root tests written, red (bt_theme_set_root_for undeclared), theme.c/h implemented (named consts 21.5/24/64/800, default 800/21.5), theme suite green after float tolerance fix 1e-6 -> 1e-5 on the 5 new root asserts only (float32 ulp near 37 is 3.8e-6); preexisting srgb/mix tolerances kept at 1e-6.
- 2026-11-01 14:42 lane-c open_url.c TU briefly broke the area build (windows.h next to raylib.h), blocked 2 runs, self-repaired by lane-c at 14:42.
- 2026-11-01 14:44 ui fitted tests red (bt_ui_draw_fitted, bt_ui_fitted_width undeclared), ui.c/h implemented: shared ui_fit_prefix, fitted width + fitted draw, panel title clamp (width - 2 pad), select closed+open row clamps (width - 2 indent). ui_draw suite green.
- 2026-11-01 14:45 frame collision tests red under the old right-anchor (long context crosses the title band, differential capture test), frame.c guard implemented: allowed = w - 2 pad - title_w - spacing, fitted and right-anchored, view_draw suite green. view_draw pins re-derived from the 800/21.5 root: metrics pad 22.32558 header 102.32558, net quad 36, layout card_x 578.62326 card_w 681.37674.
- 2026-11-01 14:46 boot wiring: main.c calls bt_theme_set_root_for(config window height) after window open, before bt_app_fonts_load, so the atlas bakes at font_focal x root (112 px at 800). build exit 0.
- 2026-11-01 14:47 full app area: 40 suites pass, 4 fail, all out of lane from the rem change: view_algs_test (lane-c, capacity 18 -> 12, detail_fit counts), view_draw_b_test (lane-f, height <= 20), view_settings_test (lane-f, pinned row bands 89.6/8.4/72.8/392), window_test (unowned, baseSize 84 -> 112). Recorded, not fixed, per lane rules.
- 2026-11-01 14:48 fmt: 5 wrap violations in my 2 test files, fixed with clang-format -i on exactly those files (never bt.py fmt-fix from a lane). lint app: my files clean, 1 failure is lane-d case_drill_view.c nested conditional. policy: my literal repeats centralized (BT_UI_EDGE_PAIR in ui.c, BT_VIEW_FRAME_EDGE_PAIR in frame.c), remaining flags are lane-d case_drill_view.c only.
- 2026-11-01 14:51 owned suites rerun green: theme 1/0, ui+ui_draw 2/0, view_draw 1/0. Committing ui helper first, then theme+frame+wiring.
