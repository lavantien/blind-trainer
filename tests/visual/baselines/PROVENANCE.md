# baseline provenance

## what the baselines are

tests/visual/baselines/ holds the 10 reference PNGs for visual regression: menu.png, drill.png, memo.png, solve.png, replay.png, algs.png, case-drill.png, stats.png, settings.png, and fatal.png. Each one is a screenshot captured from the release exe running its `--smoke` tour, one shot per screen the tour visits, in that order. The bt-e2e harness checks the same shot set against `e2e.shots` in the config hub.

## capture recipe

From a clean workspace:

```sh
make e2e
make baselines
```

`make e2e` builds the release exe, runs the headless flow suite, then e2e-smoke drives the exe through the `--smoke` tour via bt-e2e with APPDATA sandboxed to artifacts/e2e-user, writing the 10 fresh PNGs into artifacts/e2e. `make baselines` copies artifacts/e2e/*.png over the files in this directory. The shot list and window size come from the `e2e` and `app.window` config sections.

## determinism inputs

The smoke tour is seeded and fixed-size, so a recapture on the same inputs reproduces the same pixels:

| input | source |
| --- | --- |
| seed | config `app.smoke_seed` |
| window size | config `app.window` (width and height) |
| fonts | assets/fonts |
| tour and shot order | the `--smoke` implementation in crates/bt-app |

Change any of these, or the rendering they feed, and the baselines must be recaptured with the recipe above.

## comparison contract

`make visual` runs its own selftest first, then bt-visual-diff over artifacts/e2e against this directory, using `visual.tolerance` from the config hub as the per-channel pixel tolerance. A run fails when any shot is missing or differs beyond that tolerance; on a mismatch it re-renders via e2e-smoke and re-diffs once, exiting 3 for an infra flake that passed the second run and 2 for a regression that failed both, with the verdict appended to artifacts/visual/report.md.

## capture log

| commit | date | seed | window | notes |
| --- | --- | --- | --- | --- |
| 303dec0 | 2026-10-15 | 20261012 | 1280x800 | first capture under the stage 1 wiring: sandboxed e2e-smoke via bt-e2e, shot list from the e2e config section |
| 3e8f5ce | 2026-10-16 | 20261012 | 1280x800 | stage 5 single refresh: replay shot listed in e2e.shots after solve, case drill shot from the wave c tours, menu algs row text and stats due block from the seeded event repos |
| fb2383e | 2026-10-18 | 20261012 | 1280x800 | drill.png only: net pulse frozen at a fixed factor in smoke mode (44067cc), refresh captured after the freeze over 2 green 10/10 visual runs, other 9 shots byte-identical |
