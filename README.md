# blind-trainer

[![CI](https://github.com/lavantien/blind-trainer/actions/workflows/ci.yml/badge.svg)](https://github.com/lavantien/blind-trainer/actions/workflows/ci.yml)
[![coverage](docs/badges/coverage.svg)](scripts/coverage_badge.py)
[![alg coverage](docs/badges/alg-coverage.svg)](scripts/alg_coverage_badge.py)

A desktop 3BLD trainer: 3-style drilling, memo training, and timed solves, plus a multi-event alg repository covering CFOP, OH, 2x2, big cubes 4x4 to 7x7, pyraminx, megaminx, skewb, square-1, and FTO, every case drilled under the same FSRS-5 scheduler. Every alg is mechanically verified against an emulated cube or its own event engine, so misspellings and wrong algs never contaminate muscle memory. A single C23 codebase renders through raylib, stores in sqlite, and builds, tests, and releases through the `bt.py` driver.

## Table of contents

1. [Pictures](#pictures)
2. [Data and formats](#data-and-formats)
3. [Architecture and flows](#architecture-and-flows)
4. [Local development](#local-development)
5. [Runtime data and config](#runtime-data-and-config)
6. [Surfaces](#surfaces)
7. [QA harness](#qa-harness)
8. [Roadmap](#roadmap)
9. [Licensing](#licensing)

## Pictures

1. ![menu](docs/screenshots/menu.png)

   the menu, digits 1 to 6 open the views, boot warnings in the header strip

2. ![drill card](docs/screenshots/drill-card.png)

   the drill card, buffer and both targets highlighted on the net, side A colored by mechanic, side B held in the prepare line

3. ![drill reps](docs/screenshots/drill-reps.png)

   the rep phase, the net mirrors the prepared cube, space executes each side and flips A and B

4. ![drill rate a](docs/screenshots/drill-rate-a.png)

   side A rating, 1 to 4 grade again, hard, good, easy

5. ![drill rate b](docs/screenshots/drill-rate-b.png)

   the 2nd rating prompt, for side B

6. ![memo intro](docs/screenshots/memo-intro.png)

   memo intro, seed, pair count, mode and cadence

7. ![memo stream](docs/screenshots/memo-stream.png)

   mid stream, the focal token over the recent tokens with the stream counter

8. ![memo recall](docs/screenshots/memo-recall.png)

   the recall prompt, every token hidden

9. ![memo review](docs/screenshots/memo-review.png)

   the full memo with residue lines for parity, flips and twists

10. ![solve ready](docs/screenshots/solve-ready.png)

    the solve ready screen, scramble seed and pair count

11. ![solve memo](docs/screenshots/solve-memo.png)

    the memo phase, timer with the stream counter

12. ![solve exec](docs/screenshots/solve-exec.png)

    the execution phase

13. ![solve review](docs/screenshots/solve-review.png)

    the result with memo, exec and total splits

14. ![algs](docs/screenshots/algs.png)

    the alg browser, the 3BLD letters plus 1 arm per event

15. ![algs search](docs/screenshots/algs-search.png)

    the list filtered to the b prefix with the selection moved

16. ![case drill show](docs/screenshots/case-drill-show.png)

    a repository case drill card with the alg hidden

17. ![case drill grade](docs/screenshots/case-drill-grade.png)

    case drill grading, keys 1 to 4

18. ![stats](docs/screenshots/stats.png)

    the stats grid over the solve log and both card stores

19. ![settings](docs/screenshots/settings.png)

    settings, memo cadence, memo mode, drill reps

20. ![fatal](docs/screenshots/fatal.png)

    the fatal screen, shown with the reason when the config or the ref_ground corpus sheets are unreadable

## Data and formats

### letter scheme, colors, buffers

Speffz letter scheme, 1 letter per sticker position per orbit, A through X:

```
edges   A UB  B UR  C UF  D UL  E LU  F LF  G LD  H LB
        I FU  J FR  K FD  L FL  M RU  N RB  O RD  P RF
        Q BU  R BL  S BD  T BR  U DF  V DR  W DB  X DL
corners A UBL B UBR C UFR D UFL E LUB F LUF G LDF H LDB
        I FUL J FUR K FDR L FDL M RUF N RUB O RDB P RDF
        Q BUR R BUL S BDL T BDR U DFL V DFR W DBR X DBL
```

The full map is equality-tested against ref_ground/position_map.tsv. Colors: white up, green front, red right, blue back, orange left, yellow down. The net lays out U on top, L F R B across the middle band with F centered, D at the bottom, faces identified by color. Every face plate carries a frame outline and every sticker a thin stroke whose ink is picked by luminance against the sticker color, so faces and stickers read clearly through the buffer and target highlights. Buffers: edge buffer UF, corner buffer UFR, both letter C. When the buffer piece itself gets displaced the buffer floats: edges follow UF UB UR UL FR FL DF DB DR DL BR BL, corners follow UFR UFL UBR UBL DFR DFL DBR DBL.

### memo tokens

Edge pairs memo as single Vietnamese syllables from data/ga/syllables.json, 1 syllable per pair, assigned by the genetic optimizer under memo-frequency weighting, `python bt.py ga` regenerates the assignment. Corner pairs memo as image words from the letter-pair image table embedded in the app, AB is Anthony Brooks, QM is Quail Man, pairs without an entry fall back to the bare letters. The memo mode setting picks the stream: syllables, images, or per-orbit, the default, syllables for edges and image words for corners.

### synthesis

Pairs with no vendor alg get a synthesized commutator from the gendata tool. Candidates are scored for fingertrick feel: move count, a penalty for turns outside the preferred face set, regrip count, and per-face finger costs that price each turn by direction, every weight a knob in the synth.scoring hub section. Regeneration short-circuits when the synth parameter hash and corpus digest recorded in data/vendor/manifest.json are unchanged.

### scheduling

FSRS-5 spaced repetition schedules every drilled position separately. Ratings 1 to 4 map to again, hard, good, easy, stability and difficulty follow the pinned reference math, intervals clamp to configurable bounds. The 19 weights default to the reference release and live in srs.fsrs with the request retention, reference vectors under ref_ground/fsrs pin the math, and the fsrs fuzz target walks the scheduler.

### cube model

Faces in order U L F R B D, each a 3x3 sticker grid. Cube state is `occ`, 54 bytes, where occ[i] is the home sticker id currently at position i, the inverse state is the transpose, and solved means occ[i] equals i for all i. The full letter to sticker map is equality-tested against ref_ground/position_map.tsv.

### notation

siGN dialect: faces U D L R F B with `'` and `2`, lowercase wides, inner slices, M E S slices, rotations x y z. Only face turns keep centers fixed, wides, inners, slices, and rotations move them, which is what makes the centers-fixed check meaningful. Every vendor alg in ref_ground is checked center-fixed after execution.

### execution model

3-style with floating-buffer sticker-following execution. Edge buffer UF, corner buffer UFR, floating orders listed under the letter scheme. A case written A-B-C cycles sticker A to B, B to C, C to A, verified on every vendor 3-style row. The first position token of a vendor alg names the floating buffer the alg starts from, which is how later memo pairs execute at non-UF/UFR buffers: memo generation records each pair's floating buffer head, and the ideal cycle check takes that buffer as an argument. The classifier derives flips and twists from the emulated state itself, and every corpus row is cross-checked against independent simulator notes in ref_ground/simcheck.tsv for move count and resulting position.

### event engines and pack verification

Every non-3BLD event runs on its own engine in src/puzzles: nxn handles 2x2 through 7x7 on the same stride-based model, sq1 carries the exact square-1 geometry with the state-aware slash, mega the 5-sided ring, pyra the facelet model, skewb the corner turns, fto all 8 faces of the octahedron. Each engine parses its event's notation dialect and applies moves to its own state, 1 module per event behind 1 shared geometry interface.

Pack rows verify under a 2 tier model. Tier 1 round trips the row's alg against its inverse from solved in both orders through its event engine, so tokens that do not parse, sequences that do not execute, and wrong algs all fail closed. Tier 2 checks an independent predicate over the resulting state where such a predicate exists, the sq1 cubeshape rows compare against the canonical shape classifier and the bld oldpoch rows against a piece audit, rows without an honest predicate are skipped and the report counts them. Known source defects stay verbatim on the expected-failure table compiled into the seedpack module with per-row reasons, and `python bt.py verify-algs` fails on any unruled failure, any ruled row that fails nothing, and any row-count drift against the registry pins.

## Architecture and flows

![workspace](docs/diagrams/01-workspace.png)

the workspace overview, solid arrows are code dependencies, dashed arrows are build, run and file relations

![cube core](docs/diagrams/02-cube-core.png)

the core pipeline, alg text through move tokens into permutation analysis

![scheme](docs/diagrams/03-scheme.png)

the scheme pipeline, speffz map through the classifier, synthesis for uncovered pairs, memo generation, seeded scrambles

![data](docs/diagrams/04-data.png)

the data pipeline, ref_ground corpus sheets load into memory at boot, ref_ground/packs seed the sqlite stores, data/vendor is the gendata output and cross-check input

![ga](docs/diagrams/05-ga.png)

the ga pipeline, letter pairs through evolution into syllables.json

![app boot](docs/diagrams/06-app-boot.png)

the app boot sequence, args and the config hub through the stores into the state machine, or the fatal screen

![drill flow](docs/diagrams/07-drill-flow.png)

the drill flow, due selection through card, reps and rating into FSRS, the same loop drives the case drill

![memo flow](docs/diagrams/08-memo-flow.png)

the memo flow, seeded scramble through pairs and memo words into the stream phases

![solve flow](docs/diagrams/09-solve-flow.png)

the solve flow, scramble through the memo and exec clocks, corpus replay verification, the solve log, the 3d replay

![verification chain](docs/diagrams/10-verification-chain.png)

the verification chain, every gate from test to verify-algs, release adds dist and release-verify

## Local development

Prerequisites: the LLVM 23 toolchain with clang, clang-format, clang-tidy and llvm-cov on PATH, python 3, git, and java for the diagram renders. `python bt.py setup` verifies the required tools and reports the optional nvcc probe.

1. Clone, vendor and build:

```sh
git clone https://github.com/lavantien/blind-trainer
cd blind-trainer
python scripts/vendor.py
python bt.py setup
python bt.py build
```

`python scripts/vendor.py` downloads raylib 6.2 and the sqlite 3.54.1 amalgamation under the sha pins in the config hub, builds the raylib library, and writes build/vendor.lock. `python bt.py build` compiles and links every unit, the app lands in build/bin/bt-app.exe.

2. Generate the vendor data:

```sh
python bt.py gen-data
```

gendata builds data/vendor from ref_ground and synthesizes verified commutator algs for every uncovered pair, skipping outright when the recorded hashes are unchanged.

3. Run the app:

```sh
.\build\bin\bt-app.exe
```

The first run creates blind-trainer.db in the user dir, runs the numbered migrations, then imports legacy JSON files once. A missing ref_ground root only warns, the app runs on user data alone.

4. Regenerate artifacts:

```sh
python bt.py screenshots
python scripts/diagrams.py
python scripts/alg_coverage_badge.py
```

`python bt.py screenshots` rebuilds the gallery from a sandboxed run of the app with APPDATA redirected, the fatal shot comes from a run whose config points at a missing path. `python scripts/diagrams.py` re-renders every png from its puml source with the sha-pinned plantuml jar. `python scripts/alg_coverage_badge.py` refreshes the alg coverage badge from the vendor manifest.

## Runtime data and config

1. User data. blind-trainer.db, a sqlite database in the user dir: `%APPDATA%/blind-trainer` on Windows, `$XDG_DATA_HOME/blind-trainer` when the variable is set, `~/.local/share/blind-trainer` otherwise. Schema changes run as numbered migrations from db/migrations/, embedded into the binary by scripts/gen_migrations.py. The database copies to a `.bak` backup once before the first pending migration of a run, and only when tables already exist. Legacy JSON files (settings, srs, solves, user alg overlay) import once on first run after the migrations and rename to `.migrated`.

2. Config hub. `config/config.json` is the single centralized source for every tunable: the app section (window, memo cadence and mode, drill reps, smoke seed, the app.replay playback and camera knobs, fonts, render), ga seed and weights, memofreq sampling, srs.fsrs (the FSRS-5 weights, request retention, interval clamp), verify limits, synth including synth.scoring (preferred faces, finger costs, ranking weights), visual tolerance, fuzz budgets, mutants workers and timeouts, policy scanner settings, the sloc cap, the job memory ceiling, memguard, the disk floor and scratch staleness window, the e2e shot list and timeout, the release verifier contract, all paths, the build toolchain, flags, and vendored library pins. `python bt.py policy` enforces the hub: numeric literals recurring in src outside the hub must carry a justified row in `config/constants_allow.txt`, and the same scan rejects every code comment and every file over the sloc cap.

3. Environment overrides, the complete set the binaries honor:

| variable | shape | overrides |
|---|---|---|
| BT_REF_GROUND | path | ref_ground root the corpus and packs load from |
| BT_CONFIG | path | config hub path |
| BT_ARTIFACTS | path | artifacts root |

bt.py sets them for every suite and tool run. Runtime roots for config, vendor data and assets resolve from the workspace layout first and fall back to an exe-relative install layout when the workspace paths are missing.

## Surfaces

### the app keyboard

bt-app.exe with no flags opens the menu. Exactly 1 of the shot flags may be set and each takes the output directory, a violation prints the reason and exits 2.

| flag | argument | purpose |
|---|---|---|
| --smoke | dir | run the smoke tour over every view, write 1 shot per state, exit with the tour verdict |
| --usage | dir | write the usage screenshots |
| --fatal-shot | dir | write the fatal screen with the config pointed at a missing path |

The menu: digits 1 to 6 open the views. Every row shows the view name with a short description, boot warnings such as a missing ref_ground root appear in the header strip. esc returns to the menu from every view and first drops any focused text input.

The drill view runs physical 3-style pair drilling with FSRS-5 scheduling, no typed input anywhere. The card shows the pair letters, orbit and position, the memo token (edges use the Vietnamese syllable, corners the image word, bare letters when uncovered), the side A alg colored by mechanic with setup muted and interchange and insertion accented, falling back to the raw commutator when it does not decompose, the prepare line holding side B, the vendor alg of the flipped case or the derived inverse labeled inverse, plus qtm and htm lengths, solver, verified badge, FSRS due for both sides, and muted alternative vendor rows. Apply prepare to your solved cube, space starts the rep phase: each space executes the shown alg and flips to the other side, alternating A and B until the even rep count ends back in the prepared state, the drill reps setting controls it. Rate AB then BA with 1 to 4 (again, hard, good, easy), each rating writes FSRS for its own position and the next covered pair loads automatically, due or new pairs first. esc aborts to the menu writing nothing.

The memo view runs streaming memo on seeded scrambles with per-orbit tokens by default: edges stream Vietnamese syllables, corners stream image words, both fall back to the bare letters. Intro shows the seed, pair count, mode and cadence, space starts the stream. The current token renders focal with the previous tokens muted below it and a stream counter. When the stream ends the app asks you to recall with no tokens shown, space reveals the full memo with its residue lines (parity, flips, twists). r replays the stream, 1 to 4 rate every streamed pair into FSRS, space moves to the next scramble.

The solve view is the timed full cycle with the net on the left and a rail on the right. Ready shows the scramble seed and pair count, space starts the memo clock and the token stream with a focal counter, space again switches to execution, space stops everything. The app replays your memo against the vendor corpus on the emulated cube, reports solved or failed with the broken pairs, and logs the record with memo, exec and total splits. space starts the next scramble.

The replay view: v at the solve review replays your solve as an animated 3d cube, the executed memo sequence turning on the scramble. Hold any mouse button and drag to orbit, the wheel zooms, r resets the camera, space pauses and resumes playback, v returns to the solve. Playback speed and the camera defaults and clamps live in the app.replay config section.

The algs view is a 2-column browser over the full 3-style pair space plus the multi-event repository: the windowed list sits left, the selected case right. The list starts at the 3BLD letters, then 1 arm per event from the seed registry, each nesting method, stage, then cases from its seeded pack. An event whose pack fails to load hides with a boot warning, the catalog keeps working. At the letters, slash focuses the filter and typed letters filter by pair prefix. Up down and n p move the selection everywhere, right descends into a repository node, left ascends, d or enter sends a 3BLD case to the drill or arms the case drill at a repository node, esc drops the filter focus first and exits the view on the 2nd press. The 3BLD detail shows the letters, orbit, position and coverage, the first vendor alg colored by mechanic when its commutator decomposes, alternative algs, solver, qtm and htm lengths, a verification badge, and recon and video links. A repository case shows its event, method, stage, the net rendered through that event's engine, the seed alg with a seed or override badge, and a tier 1 verification line. o opens an override draft prefilled with the effective alg, enter submits it through the event's tier 1 verifier so a broken override never lands, tab cancels, x deletes the override and reverts to the seed.

The case drill view: d at a repository node arms a due session over every case under that node, d at a case node drills that single case. The session orders due and new cards first, space reveals the case, 1 to 4 grade again, hard, good, easy into FSRS with the case card store, and the next card loads automatically until the queue empties and returns to the browser. esc exits to the menu at any time. 3BLD keeps its own drill view, the case drill serves every other event.

The stats view is a block grid over the solve log and both card stores: solve count, trailing streak of successful reviews, memo and exec mean and median, due counts per orbit for 3BLD, and a due count per event that holds case cards, so the session load reads at a glance.

The settings view holds memo cadence, memo mode (syllables, images, or per-orbit, the default), and drill reps. The number inputs and the select apply immediately, 1 2 and 3 switch the memo mode from the keyboard when no input is focused, everything persists across restarts.

The fatal screen: if the config cannot be read, or the ref_ground root exists but its corpus sheets are unreadable, the app opens here with the reason and no menu appears. A missing ref_ground root only warns at the menu and the app runs on user data alone. The fatal screen ignores keys, fix the reported path and restart.

### bt.py targets

`python bt.py` with no target prints this table from the target registry:

```text
help                     show this help
doctor                   probe the toolchain and write build/env.json
setup                    verify the required toolchain and report optional probes
build                    compile and link every c unit, test exe, and the hello smoke exe
check                    compile every translation unit without linking
fmt                      clang-format dry run over every c and h file
fmt-fix                  clang-format in place
lint                     clang-tidy over every c file, or one area: python bt.py lint <area>
test                     run every unit test exe and the python suite
test-unit                run one area's unit tests: python bt.py test-unit <area> [--only substring]
policy                   selftest then the zero-comment, constants-hub, and sloc scans
policy-selftest          prove bt-policy detects planted comments, literals, and cap breaches
fuzz                     run every fuzz_*_test suite exe in plainloop mode, harness bodies keep the early-return, exit-at-bound applies only under a libFuzzer driver
mutants                  mutate area sources against their unit suites: python bt.py mutants [area...]
mutants-gate             adjudicate mutation survivors in artifacts/mutants/outcomes.json against the allowlist
mutants-gate-selftest    the mutants gate selftest over planted outcomes
visual-diff              diff e2e shots against the committed baselines
visual-selftest          prove the diff passes identical and catches drift
visual                   visual selftest then the diff with one re-render retry
baselines                recapture the committed baselines from two byte-identical smoke captures
gen-data                 build data/vendor from ref_ground and synthesize uncovered pairs
memofreq                 memo pair frequency monte carlo
ga                       run the syllable optimizer into data/ga/
verify-algs              verify every vendor alg and pack row, write the verify and packs reports
verify-algs-selftest     run every verify_* unit suite with the ref ground and config env
screenshots              regenerate every usage shot and the fatal screen
e2e-smoke                sandboxed smoke tour over the release exe
e2e                      headless flow suite then the smoke tour
release-verify           byte-check the dist zip and run the packaged exe: python bt.py release-verify v=<tag>
release-verify-selftest  the release-verify unit suite
dist                     build the app, stage and verify the dist zip: python bt.py dist v=<tag>
clean                    remove artifacts and recreate the keep file
clean-c                  remove the c build tree
```

### exit codes

bt-app.exe in smoke mode: 0 tour complete with all shots written, 1 verification failure or boot stuck past the limit, 2 flag usage error, 3 a tour state never arrived. `python bt.py visual`: 0 clean, 2 regression on both diffs, 3 infra flake that passed the re-render, 4 infra error. visual-diff.exe: 0 match, 1 mismatch or missing baseline, 2 usage. bt-e2e.exe: 0 all shots verified, 1 shot failures, 2 harness error. policy.exe, mutants-gate.exe, release-verify.exe: 0 pass, 1 findings, 2 usage or harness error.

## QA harness

The verification chain runs through bt.py in order, committing at each green step: test, fmt, lint, policy, check, build, fuzz, mutants, mutants-gate, e2e, visual, verify-algs, each a separate invocation so a failure names its step. Every custom gate ships a selftest target that proves it detects what it claims: policy-selftest, mutants-gate-selftest, visual-selftest, verify-algs-selftest, release-verify-selftest. Surviving mutants are never ignored: mutants-gate fails unless each survivor matches a justified row in `config/allowlist.tsv`, stale rows also fail. visual always diffs fresh e2e renders against the committed baselines, baselines recaptures them from 2 byte-identical smoke captures, and the baselines carry their capture recipe in tests/visual/baselines/PROVENANCE.md. Adversarial review by 2 independent agents before any task closes, records land in docs/reviews/ from the template there. Conventional commits, zero code comments, the sloc cap. CI runs on every push to main and every pull request and still executes the gates of the earlier application stack, item 2 of the roadmap moves it onto the bt.py gates.

## Roadmap

1. a coverage target that builds instrumented binaries, exports the llvm-cov json, and refreshes the coverage badge through scripts/coverage_badge.py
2. CI moved onto the bt.py gates
3. a chain target that runs every gate in order
4. a memguard target that reports repo process memory against the cap through scripts/memguard.py
5. targets wiring the alg coverage badge and the diagram renders into bt.py
6. a run target that starts the app after a build
7. a cuda acceleration lane behind the nvcc probe bt.py doctor already reports

## Licensing

Repository code is MIT. ref_ground/ ships GPL-3.0 derived data reconstructed from external workbook exports and tables, and ref_ground/packs/ holds the seed TSVs under mixed MIT and GPL terms. DATA_PROVENANCE.md and ref_ground/packs/PROVENANCE.md record hashes, sources, licenses, and regeneration steps.
