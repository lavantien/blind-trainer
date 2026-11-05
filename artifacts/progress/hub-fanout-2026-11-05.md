# hub fan-out log, 2026-11-05

Session goals: docs and text audit, logic and integration audit, alg coverage to 100%, global region coverage to 100%.

Baseline measured before fan-out: `python bt.py coverage` green, 34912 of 42997 regions, 81.2%, log at artifacts/progress/hub-cov-baseline.log, gap table at artifacts/coverage/gaps.tsv (163 files, 8085 uncovered regions).

## fan-out layout

- lane-docs: text audit over README.md, DATA_PROVENANCE.md, release notes, review records, provenance files.
- adv-a1, adv-a2: independent adversarial audits over app integration and data, scheme, puzzle, tooling logic. Reports to artifacts/progress/adv-audit-1.md and adv-audit-2.md.
- lane-cov-app: src/app plus tests/unit/app, 1362 regions.
- lane-cov-data: src/data plus tests/unit/data, 1048 regions.
- lane-cov-mut: tools/mutate, tools/mutants-gate, tools/policy plus their suites, 1974 regions.
- lane-cov-verify: tools/verify, visual-diff, release-verify, jobwrap, sleeper plus their suites, 1483 regions.
- lane-cov-ga: src/ga, tools/ga-run, tools/memofreq plus their suites, 926 regions.
- lane-cov-core: src/core, src/common, src/scheme, src/puzzles, tests/fuzz plus their suites, 931 regions.
- queued: lane-cov-gendata owns tools/gendata suites, gendata area coverage (361 regions), and the corner N-I synth fix for alg coverage 100%.

Lanes build through per-lane config clones from scripts/lane_build.py into artifacts/lane/<lane>/build, BT_CONFIG pinned per invocation, BT_BIN_DIR set so spawned suites use lane binaries. Guard cron 01b66cc3 fires every 20 minutes: quota, memory through scripts/memguard.py, progress staleness at 30 minutes.

## decisions

- No worktrees, shared checkout, commits only through scripts/commit_lane.py.
- Lanes never run repo-wide targets; the hub owns coverage reruns, e2e, visual, fuzz, mutants gates at wave ends.
- config/config.json, config/allowlist.tsv, config/constants_allow.txt edits escalate to the hub, except the synth section for lane-cov-gendata.
- alg coverage gap: exactly 1 of 818 cases, corner N-I, RUB to FUL, uncovered because the gendata synth search returns no candidate under the hub limits max_setup 3, max_insert 4, node_budget 50000000.
