# blind trainer

A desktop 3BLD trainer: 3-style drilling, memo training, and timed solves. Every alg is mechanically verified against an emulated cube, so misspellings and wrong algs never contaminate muscle memory. Grounding data is the reconstructed alg sets of Tommy Cherry and Charlie Eggins, sha-256 pinned under local ref_ground/.

## architecture

Rust workspace. Arrows point from dependent to dependency:

```
bt-core
  <- bt-scheme
       <- bt-data
            <- bt-app
            <- bt-gendata
            <- bt-verify (also depends on bt-scheme)
       <- bt-ga
            <- bt-ga-run
       <- bt-memofreq
bt-visual-diff (no crate dependencies)
```

- bt-core: cube model, precomputed move permutations, siGN notation dialect parse/format/inverse/qtm, piece and orientation analysis. std only.
- bt-scheme: Speffz letter maps, floating-buffer pair spaces, classifier mirroring the blddb tracer, memo generator, seeded scrambles.
- bt-data: TSV corpus loader, alg verification verdicts, SM-2 spaced repetition, user-dir overlay.
- bt-ga: Vietnamese syllable inventory, perceptual features, genetic assignment optimizer. bt-app consumes its JSON output and never depends on the crate.
- bt-app: Bevy desktop app shipped as blind-trainer.exe, keyboard-first UI, JSON persistence in the user dir. AppLogicPlugin carries all game flow with no rendering, PresentationPlugin owns the UI, so headless e2e drives complete flows through MinimalPlugins.
- tools: bt-gendata builds data/vendor from ref_ground/, bt-verify writes the alg verification report, bt-memofreq runs the memo frequency Monte Carlo, bt-ga-run runs the optimizer into data/ga/, bt-visual-diff compares screenshots against committed baselines.
- fuzz/: cargo-fuzz targets, excluded from the workspace.

## cube model

Sticker id = face*4 + position within face. Corners 0..23, edges 24..47, centers 48..53. Face order U L F R B D, each face a 2x2 grid numbered row-major:

```
U: 0 1     L: 4 5     F: 8 9     R: 12 13    B: 16 17    D: 20 21
   2 3        6 7        10 11      14 15       18 19       22 23
edges: UB=24 UR=25 UF=26 UL=27 LU=28 ... DL=47 (orbit order follows the Speffz table)
```

Cube state is `occ: [u8; 54]` where occ[i] is the home sticker id currently at position i. The inverse state is the transpose: inverse[i] = position of home sticker i. Solved means occ[i] == i for all i. Speffz anchors: edge UF is sticker 26 letter C, corner UFR is sticker 2 letter C, the full map is equality-tested against ref_ground/position_map.tsv.

## notation

siGN dialect: faces U D L R F B with ' and 2, lowercase wides, inner slices 2U 2D 2L 2R 2F 2B, slices M E S, rotations x y z. M follows L, E follows D, S follows F. Rotation pins: z = F S B', x = R M' L', y = U E' D'. M2' canonicalizes to M2 because both produce the same permutation. Only face turns keep centers fixed, wides, inners, slices, and rotations move them, which is what makes centers_fixed a meaningful predicate. Every vendor alg in ref_ground is center-fixed by cancellation.

## execution model

3-style with floating-buffer sticker-following execution. Edge buffer UF, corner buffer UFR. Floating order: edges UF UB UR UL FR FL DF DB DR DL BR BL, corners UFR UFL UBR UBL DFR DFL DBR DBL. A case written A-B-C cycles sticker A to B, B to C, C to A (CYCLE_A direction, verified on all 988 vendor 3-style rows). The first position token of a vendor alg names the floating buffer the alg starts from, which is how later memo pairs execute at non-UF/UFR buffers: memo generation records each pair's floating buffer head, and the ideal cycle oracle takes that buffer as an argument. Defects: which pieces an alg flips or twists is read from the simcheck notes produced by the independent pysim simulator, never inferred from position tokens, the two derivations disagree on 213 of 1077 rows.

## user flows

- drill: pick a pair or let SRS choose, see the alg, execute it, the emulator verifies the resulting sticker cycle and rejects wrong algs, the review grades into SM-2.
- memo: a scramble is generated, syllables stream at the configured cadence, the user pronounces them, image mode flashes English letter-pair words instead.
- solve: full timed cycle, memo phase, execution phase, review shows the splits, the record persists in the user dir.
- algs: browse the vendor corpus per position with verification status and links, edits write only to the user overlay file.
- stats and settings: solve history and every tunable from the config hub, settings persist across restarts.

## runtime notes

Bevy 0.20 with bevy_feathers. The bevy facade needs the custom_cursor feature enabled alongside bevy_feathers, without it bevy_winit 0.20.0 fails to build from feature skew. The release binary resolves assets through an absolute AssetPlugin file_path built at startup, the default asset root is exe-relative and wrong for installed layouts. Text input uses the shipped FeathersTextInput, no custom widget.

## host tooling

cargo-fuzz 0.13.2 with the nightly MSVC toolchain, targets notation, classify, memo under fuzz/fuzz/ (nested project layout, nearest-ancestor Cargo.toml resolution). cargo-mutants 27.1.0, configured in .cargo/mutants.toml with gitignore=false so scratch copies keep ref_ground reachable through relative env paths. Both tools compile nothing from bt-app.

## make targets

- setup: ensure rustfmt and clippy components, report cargo-fuzz and cargo-mutants availability.
- docs-fetch: snapshot reference docs into ref_doc/ via scripts/fetch_docs.ps1.
- gen-data: build data/vendor from ref_ground/.
- verify-algs: verify every vendor alg against the emulated cube, write the report.
- memofreq: Monte Carlo memo pair frequencies.
- ga: run the syllable GA into data/ga/.
- fmt: cargo fmt --all -- --check.
- lint: cargo clippy --workspace --all-targets -- -D warnings.
- check: cargo check --workspace.
- test: cargo test --workspace.
- build: release build of bt-app, asserts target/release/blind-trainer.exe exists.
- fuzz: cargo fuzz run over the notation, classify, memo targets in fuzz/.
- mutation: cargo mutants over bt-core, bt-scheme, bt-data, bt-ga.
- e2e: headless flow suite (cargo test --release -p bt-app --test e2e) plus windowed blind-trainer.exe --smoke writing screenshots to artifacts/e2e.
- visual: bt-visual-diff over artifacts/e2e versus tests/visual/baselines.
- run: cargo run --release -p bt-app.
- chain: test, fmt, lint, check, build, fuzz, mutation, e2e, visual, verify-algs, each as a separate make invocation so a failure names its step.

## config hub

config/config.json is the single centralized hub for every tunable: app window and memo cadence, ga seed and weights, memofreq sampling, srs intervals, verify limits, fuzz budgets, and all paths. No constant lives anywhere else. The Makefile exports the path roots as BT_REF_GROUND, BT_CONFIG, and BT_ARTIFACTS so binaries resolve ref_ground/, config/config.json, and artifacts/ from the environment.

## development

Verification chain, run through make chain and committed at each green step: test, fmt, lint, check, build, fuzz, mutation, e2e, visual, verify-algs. Baseline-first TDD per milestone, property-based invariants where they pay off, adversarial review by two independent agents before any task closes. Conventional commits, zero code comments, max 1000 SLOC per file.

## licensing

Repository code is MIT. ref_ground/ and ref_doc/ are gitignored local grounding derived from GPL-3.0 blddb exports, see DATA_PROVENANCE.md for hashes, retrieval dates, and regeneration steps.
