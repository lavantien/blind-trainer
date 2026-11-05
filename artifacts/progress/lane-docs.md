# lane docs audit, 2026-11-05

Scope: README.md, DATA_PROVENANCE.md, docs/releases/*, docs/reviews/*, the 3 ref_ground and baselines PROVENANCE files. Standard: CLAUDE.md readme and voice rules, quotes-manager readme as template.

- 22:40 toolset probe: Read, Glob, Grep, Edit, PowerShell all available. ref_doc/MANIFEST.md untracked, out of scope. Template read.
- 23:0x sweeps done: banned chars zero at start except 16 prose semicolons across 5 files, all split into sentences. Banned words fixed: gap, honestly, honest, actually, lands, rests on, lives in. Internal ids dropped: a0, a8, f2, wave A B c, stage 1 5 wiring, adopt plan refs, agent-a/b/c names, rust era paths (crates/bt-data, crates/bt-core, playground/ref_ground/dump_all.py, artifacts/f2-fsrs). Stale claims modernized: planned seed loader normalizer now stated in present tense, fsrs goldens point at src/data/fsrs.c, dump_all.py at ref_ground/.
- Accuracy checks green: bt.py help table byte-matches the readme table, all 21 screenshots exist, badge links resolve, roadmap items 1 to 6 all unbuilt, no rust or make leftovers in the readme, exit code paragraph matches main returns in tests/e2e and tools.
- Kept on purpose: number words in idiomatic spots match the quotes-manager template, pin and pinned in the sha and test anchor sense, verbatim readme sheet captures in ref_ground/PROVENANCE.md untouched, dates and hashes intact everywhere.
- Found but not mine: ref_ground/PROVENANCE.md edits apply locally, the file is gitignored. ref_ground/vendor/PROVENANCE.md (out of scope) still carries "the t31 log" internal reference, flagging to hub. packs/PROVENANCE.md previously predicted f2l case 07 would fail a strict slot-local tier 2 predicate, no f2l ruling exists today and the C f2l predicate exists, hub should confirm its outcome from the verify-algs report. docs/badges/coverage.svg and coverage.json changed from the hub coverage run, left for the hub to commit.

