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
- bt-app: Bevy desktop app shipped as blind-trainer.exe, keyboard-first UI, JSON persistence in the user dir.
- tools: bt-gendata builds data/vendor from ref_ground/, bt-verify writes the alg verification report, bt-memofreq runs the memo frequency Monte Carlo, bt-ga-run runs the optimizer into data/ga/, bt-visual-diff compares screenshots against committed baselines.
- fuzz/: cargo-fuzz targets, excluded from the workspace.

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
