# baseline provenance

## what the baselines are

tests/visual/baselines/ holds the 8 reference PNGs for visual regression: menu.png, drill.png, memo.png, solve.png, algs.png, stats.png, settings.png, and fatal.png. Each one is a screenshot captured from the release exe running its `--smoke` tour, one shot per screen the tour visits, in that order. The bt-e2e harness checks the same shot set against `e2e.shots` in the config hub.

## capture recipe

From a clean workspace:

```sh
make build
# run the release exe through the smoke tour into the e2e artifacts dir
./target/release/blind-trainer.exe --smoke artifacts/e2e
make baselines
```

`make build` produces target/release/blind-trainer.exe. The `--smoke artifacts/e2e` run writes the 8 fresh PNGs. `make baselines` copies artifacts/e2e/*.png over the files in this directory. The bt-e2e harness (tests/e2e) drives the same flow with its own timeout and shot checks.

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

`make visual` runs bt-visual-diff over artifacts/e2e against this directory, using `visual.tolerance` from the config hub as the per-channel pixel tolerance. A run fails when any shot is missing or differs beyond that tolerance.

## capture log

| commit | date | seed | window | notes |
| --- | --- | --- | --- | --- |
| pending first refresh | | | | placeholder row, the first wiring task regenerates the baselines and records the real capture |
