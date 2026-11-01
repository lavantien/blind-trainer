# lane docs progress log

started 2026-11-01

- 2026-11-01 gh repo edit done, verified with `gh repo view --json description,repositoryTopics`: description "3BLD/WCA cubing trainer in C23/raylib: alg repository with mechanical cube verification, evolved optimal audio/image pairs, commutator-to-plain-alg, FSRS-5 case drills, SQLite persistence, 3D replay, full fuzz/mutation gating", topics 3bld, desktop-app, rubiks-cube, spaced-repetition, speedcubing, trainer, 3-style, blindfold-cubing, evolution, genetic-algorithm, rubiks, fsrs, sqlite, c, c23, raylib (rust and bevy removed)
- 2026-11-01 `python bt.py test` launched in background, awaiting the suite count
- 2026-11-01 README roadmap item 7 removed, roadmap keeps 6 items, committed 78d6f0a
- 2026-11-01 dist staging verified from scripts/dist.py and config release section: zip blind-trainer-<version>-windows-x64.zip carries blind-trainer.exe, config/config.json, assets/fonts
- 2026-11-01 baselines counted from tests/visual/baselines: 10 png shots plus PROVENANCE.md
- 2026-11-01 reproducible builds verified: /brepro at config/config.json line 310, object timestamps zeroed in bt.py and scripts/vendor.py
- 2026-11-01 test run finished green, summary from the run log: 192 passed, 0 failed, exit 0, 191 c suite exes plus the python suite; case counts from the sources: 1968 BT_TEST cases in tests/unit, 276 python test defs in tests/python
- 2026-11-01 mutation numbers read from artifacts/mutants/outcomes.json (hub ran the gate at HEAD, exit 0): bt-mutate, caught 14984, missed 5974, total 20958, survivors adjudicated by the gate allowlist
- 2026-11-01 fuzz suite count from the registry (discover_test_suites prefix fuzz_): 7, python bt.py fuzz launched in background
- 2026-11-01 recon link verified as system browser open: ShellExecuteW in src/app/open_url.c
- 2026-11-01 first python bt.py fuzz attempt failed on 2 locked links (tools_gendata_build_test.exe, tools_gendata_synth_test.exe, permission denied from a concurrent lane), rerun started serially after the build tree went idle
- 2026-11-01 docs/releases/v2.0.md written, commit held for the fuzz verdict
- 2026-11-01 fuzz rerun green: summary 7 passed, 0 failed, exit 0
- 2026-11-01 release notes committed 36277a5

