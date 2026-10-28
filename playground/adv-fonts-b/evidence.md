# adv-fonts-b evidence

Independent reproduction of the lane-views-fonts surface, HEAD 0a7b7c6 plus acf68b6.

## Gallery reproduction, build/bin/bt-app.exe, exact _screenshots_steps argv and env

- run_shots.ps1, APPDATA redirected to scratch, cold start: usage exit 0, fatal exit 0, 20 shots.
- 16 of 20 byte-identical to docs/screenshots, fatal included. drill-card, drill-reps, drill-rate-a, drill-rate-b differ.
- run_shots2.ps1 second cold run: same 16 identical to run 1, the 4 drill shots differ again from run 1.
- Drill diff localized to bbox (205,238)-(278,353): 3 pulsing target stickers on the net, color lerp phase only, 3158 px, geometry identical. Sticker fill run1 (249,153,87) vs run2 (248,169,114), teal (98,231,220) vs (123,233,224). The drill net pulse runs on wall-clock, so usage drill shots are not byte-reproducible. e2e shots are not affected.

## Solve rail, region x586-1280 y90-780, bg (23,26,31)

- solve-ready: bands y94-104, 252-270, 393-400, lit 1893. Text: ready, seed 20261126, 10 pairs, space to start memo.
- solve-memo: bands y94-102, 184-221, 252-266, 392-400, lit 3974. Text: memo, seed 20261126, 3/10, memo 3.1s, space to finish memo.
- solve-exec: bands y94-102, 252-266, 393-402, lit 1748. Text: exec, seed 20261126, exec 2.1s, space to stop.
- solve-review: bands y94-102, 310-334, 342-356, 370-384, 392-412, lit 3905. Text: review, solved, memo 3.1s, exec 2.1s, total 5.2s, space next solve v 3d replay.
- Phase pairwise diffs in rail: 1366 to 6002 px, all 6 pairs far beyond 3 px. Old hollow shots were pixel-identical in this region.
- solve-review overlap: parts.splits is a 3-line string drawn at 26 px but the slot advances one line, so split line 3 total 5.2s lands at y388 exactly where the hint draws. Zoomed crop solve-review-collision.png shows total 5.2s painted over the gray hint text.

## Algs

- Rows x20-510: 21 text bands, pitch 30 px, glyph height 14-15 px (font_body 20 px). Detail x516-1270: 12 bands.
- algs vs algs-search: rows differ in exactly 2 row bands (834 px), the selection marker AB to AE. Both read 818 of 818 cases match. The list is not filtered after typing b.
- Production cause: bt_view_algs_read_query src/app/views/algs/mod.c 381 has no production caller, only tests. Typed chars reach bt_view_algs_frame keys but nothing appends them to app->query, so the filter stays on the empty query.

## Case drill

- show: bands y100-124 (title 25 px), 148-156, 169-177, 191-200. Text: cll.01, 2x2 > cll > cll, case 1 of 42, space to reveal.
- grade adds the alg line and 1 again 2 hard 3 good 4 easy at y211-222. show vs grade 1032 px differ.

## Cross view sizes

- Context line (font_small 16 px regular) band heights: solve and algs and case drill 12, drill 11, settings and stats 7 (esc menu has no ascenders). Same effective size.
- font_body 20 px: algs rows 14-15 px cap, settings body 15 px. font_title 44 px: case drill title 25, fatal title 25.

## Baselines

- baselines_probe.ps1: bt-e2e.exe driver, APPDATA scratch wiped once then two captures sharing it. All 10 shots byte-identical between captures and byte-identical to tests/visual/baselines. Determinism gate holds.
- Baseline decode: solve rail 4 bands lit 1849, algs rows 22 bands lit 5486, case drill text band 5 bands lit 2675. The baselines carry text.
- e2e solve baseline is the ready phase only, so the review-phase overlap is invisible to the baseline gate.

## Structure, old hollow shots vs new renders

- solve-ready and solve-review net region x20-565: 0 of 376050 px differ. case drill grid region x400-1270: 0 of 600300 px differ. algs rows region: 5051 of 338100 differ, the new text only.

## Gates rerun

- build/bin/app_view_draw_b_test.exe: 3 passed, 0 failed.
- No text touches the right window edge in the 8 affected shots, only the y90 hairline.

## Origin

- Raw rem sizes are present at the port commits themselves: solve.c 557-573 at 9bdce5c, algs mod.c 611-640 at a1d4113, case_drill_view.c 174-178 at eb1d60b, with theme already rem-based. No later commit removed a multiplier: the views never drew visible text before f53951f.
