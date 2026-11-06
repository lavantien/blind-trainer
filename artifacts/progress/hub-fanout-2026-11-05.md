# hub fan-out log, 2026-11-05

Mission: docs and text audit, logic and integration audit, alg coverage to 100 percent, global region coverage to 100 percent. Baseline 34912 of 42997 regions, 81.2 percent, gap table at artifacts/coverage/gaps.tsv. Docs audit and both adversarial audits delivered early, all 10 findings fixed.

## layout

Lanes over a shared checkout, no worktrees: app (src/app, tests/unit/app), data (src/data), mut (mutate, mutants-gate, policy), verify (verify, visual-diff, release-verify, jobwrap, sleeper), ga (src/ga, ga-run, memofreq), core (core, common, scheme, puzzles, fuzz), gendata (gendata suites plus the alg coverage chain). Lane builds through scripts/lane_build.py clones under artifacts/lane, commits only through scripts/commit_lane.py (fmt gate included since b628dc9), suites through bt.py under BT_CONFIG per call. Guard cron 01b66cc3 every 20 minutes: quota halt at 95 percent, memguard census at the runtime cap, disposal at 30 minutes of lane silence.

## rulings

- Check macros restructure runs as one dedicated lane at wave end: BT_CHECK and MUT_CHECK become branchless calls into bt_harness.c so the failure arm exists once and is coverable. Lanes do not count assert arms as their residue.
- Recurring literals centralize as named constants, never allowlist rows.
- net.c letter switch becomes an indexed table lookup, which removes the switch and the missing-default lint question.
- Dead arms leave only with a per-site proof the hub can re-derive, as refactor commits.
- gendata provenance: ref_ground re-pins to the regenerated outputs, PROVENANCE.md states both divergence causes (mirrored-inverse derivation, the 51373ea parity fix dropping twist notes on 18 specials rows), hub reviews at wave end with the open question whether 51373ea intends that note drop.
- Reserved for the user at wave end: open_url ShellExecuteW arms, ENOSPC and EIO copy arms, platform arms (ftell negative, short writes), the race-only digest arm, and the proof-backed unreachable sets from the mut and verify lanes.

## incidents

- Provider window ceilings at 2026-11-06 00:48 and 06:05 local killed all lanes on 429, resets 04:06:46 and 09:09:17 GMT+8. GMT+8 minus 1 hour is local. No work lost beyond uncommitted mid-batch edits, every lane resumed from its progress file.
- Shared build/env.json poisoned twice by lane measurement configs writing a foreign obj_root. Root cause dirs.env unredirected, fixed in lane_build.py c705b34.
- Three CI cycles lost to hand-formatted fixtures. Root cause fix is the commit-lane fmt gate b628dc9, and playground/ci_fmt_check.py mirrors the CI fmt check at HEAD (scope src, tools, tests only) so every violator is found locally in one pass.
- Five lane disposals for 30 minute silence (app twice, ga twice, mut once), each relaunched from its progress file. Two of them had run their whole life without a single progress line, so briefs now demand a first line within 5 minutes.
- Core's fault-seam routing made bt_trace depend on the alloc object and broke the ws fixture link. mut repaired that fixture in da6a7d3, core owns the full link-list sweep.
- The 500M gendata probe proved corner N-I solvable inside the shape limits, so the hub-budget failure was search-order starvation, fixed by the mirrored-inverse derivation 010cf3b. Regen covered 818 of 818, the superset check found exactly the 1 derived gain and zero regressions.
- Third real defect of the wave: bt_strbuf_reserve hung forever on a zero-cap strbuf after a failed init, because cap doubling never leaves 0. gendata3 applied the minimal guard in src/common/strbuf.c (flagged, outside its set) and core4 reviews plus pins it with a direct red-green suite test.

## current state, 2026-11-06 09:0x local

- Quota 24 percent at 09:00 local, window rolled 08:09 local.
- app7: net.c indexed lookup in flight, alloc arms batch landed 255b45b.
- data5: import batch in flight, owes the paths_errors fmt fix and the repo_errors sloc split.
- ga5: routed set and class 2 dead arms cleared, class 1 OOM routing next.
- verify5: ev engine arms, owes the sleeper trailing comma.
- core4: inherited harness and puzzles chain landed, owns the link sweep and the strbuf-armed report.c arms.
- gendata3: chain continues at verify-algs, then re-pin, badge to 100.0, atomic commit.
- mut: delivered and disposed, its residue is the assertion-arm class plus proof-backed unreachable arms.
- CI blockers left: paths_errors_test.c fmt, repo_errors sloc split, sleeper lint. Push and watcher fire after those land. Banked unpushed commits run through 980d7f6.

## wave-end chain

Full coverage rerun, fuzz, mutants and gate for touched areas, e2e, visual, baselines recapture (the memo seed fix renumbers the first streamed scramble), verify-algs confirming the f2l case 07 prediction, the check-macro restructure lane, adversarial review record in docs/reviews/, readme updates (the 4 env seam rows and the smoke exit-code line), badge refreshes, final push. Teardown: delete the guard cron, TaskStop every agent, orphan sweep, purge artifacts/lane trees.
