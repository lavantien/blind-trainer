# hub fan-out log, 2026-11-05

Mission: docs and text audit, logic and integration audit, alg coverage to 100 percent, global region coverage to 100 percent. Baseline 34912 of 42997 regions, 81.2 percent, gap table at artifacts/coverage/gaps.tsv. Docs audit and both adversarial audits delivered early, all 10 findings fixed. Goal status: alg coverage reached 100.0 with the badge regenerated from a real run, landed as 7296f65 with references re-pinned and both provenance divergence causes stated. Wave-end verify-algs rerun still confirms it in the full chain.

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
- Shared-checkout incident: the strbuf guard briefly vanished from the working tree with no other change. Confirmed as core4's red run for the pin (300 second hang proved it, gendata3's mid-run restore left the file byte-identical to the committed fix, pin landed as 928b9b9). Rule adopted by the lanes: a deliberately broken red state exists only inside a lane-local build copy, never in the shared tree.
- data5 delivered 616 to 284 dark across 12 commits with a seventh real defect: verify reported zero problems and claimed ok when problem recording ran out of memory, now fail-closed through bt_verdict.allocation_failed. The hub fixed the paths_errors fmt wrap itself (5a5ec70, trivial edit) and data5, resumed by the routing message, landed the repo_errors split as 289de00 (52 tests before and after, three files) plus the logging-rule collapse 31bcf4f. data6 goes straight to the residuals.
- core4 delivered 894 to 659 across 14 commits, all 4 routed items closed. The InstrProf fix is a direct runtime declaration because this LLVM ships no profile/InstrProf.h and clang 23 defines only __LLVM_INSTR_PROFILE_GENERATE (7c7fcf9, probe-proven). The strbuf pin landed red-first as 928b9b9, which also settles the working-tree clobber arbitration: the guard deletion was core4's red run. Worth carrying: core seam routing shifts exact-count death arms, so arms get counted by seam order, and the legal wide setup form is U U U U:[[R,U],S]. core5 continues with json 86, paths 43, config 19, strbuf and trace tails, the fuzz bodies, and the mutate report.c arms.
- Guard disposals at 09:40: ga5 silent 45 minutes mid class-1 OOM routing (uncommitted seam edits in ga-run lib, out, and the new ga_run_oom_test.c), verify5 silent 40 minutes mid its OOM routing batch (uncommitted seam edits across verify lib, main, packs, report, ev_sq1, visual-diff, five test files, visual_death_test.h). Both relaunched as ga6 and verify6 with the uncommitted sets handed over explicitly. verify5's batch-1 proof corrections stand: main.c 87 live through truncation, ev_pyra arms removed on the bt_pz_fatal abort proof, ev_sq1 round-trip arm removed on the retrace proof.
- gendata3 closed both missions: the alg coverage chain landed as 7296f65 (hub verified the badge reads 100.0 and the re-pin commit carries references, provenance with both divergence causes, and the probes), and mission 2 took the tools from 264 to 162 dark with 3 real defects fixed at root, all reproduced by committed probes. gendata4 continues with the seam routing, the note_text dead-arm proofs, and verification of the claimed inline-artifact regions.
- app7 delivered 87.0 to 90.9 percent (734 dark, 48 suites green, plus 342 regions). The focus suite turned out not flaky but deadlocked: a synchronous WM_GETTEXT from the watch thread against a non-pumping same-process owner, fixed with a SendMessageTimeoutW read (1b2bd5b), and the racy sentinel asserts it exposed are now plain calls or retries (a49aa90, 80851c6). The silent app6 had landed the net.c restructure after all (dc2957c). app8 continues with the classified tail, the OOM-fatal allocator arm, and re-derivation of the unreachable-by-construction claims.
- verify6 delivered 886 to 785 across 6 commits: the inherited OOM routing finished with env seams on both exes (BT_VERIFY_ALLOC_FAIL_OK, BTVD_ALLOC_FAIL_OK) and a real wmain argv-cleanup fix, the sleeper lint landed as 342a927 closing the last CI blocker, and a ninth real defect fell: release-verify reported an out-of-range window width as a height problem (bb6f38f). Escalation routed to data6: corpus.c line 151 returns -1 without setting its error struct on the malloc-failure path. All CI scope files verify clean at HEAD through the fmt mirror, 100 plus commits pushed as 5067238..494f9b1, ci-watch-4 watching, verify7 launched on the remaining order.

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
