# adv77b: adversary B on the t77 coverage target

Probes live in playground/adv77b/. Findings marked confirmed carry a probe or a line cite.

## done so far

1. CONFIRMED scope mismatch, both directions, against artifacts/coverage/coverage.json from the landed run:
   - leaked in: 51 tests/unit files (20488 regions at 80.3%) match the unanchored include_regex leg `[\\/]tools[\\/]` through their tests/unit/tools/ path, so the badge totals mix test code into a number presented as product scope. Stated scope alone (src+tools+tests/fuzz) measures 70.9%, the badge says 74.0%. t77.md line 16 claims the unit test files stay out of the totals, which is false for every tools area test file. Probes: playground/adv77b/buckets.py, denominator_holes.py.
   - left out: 20 c files claimed in scope never appear in the export: all 11 tools/*/main.c, src/app/main.c, src/core/lib.c, src/puzzles/lib.c, and all 6 tests/fuzz/*.c bodies (only their headers show). Tool mains and fuzz bodies are linked into exes the target never feeds to llvm-cov (only the 190 unit suite exes are), so they are absent from the denominator, not counted as zero.
2. CONFIRMED subprocess coverage loss: spawn_util suites (mutants-gate etc.) spawn instrumented tool exes that inherit the suite's LLVM_PROFILE_FILE, the parent writes its profraw last, the child's counters are discarded. mutants-gate reads 0/1472 at 0.0% in the report although the coverage run executed that code through bin-cov/mutants-gate.exe. tests/unit/tools/mutants_gate_test.c:59-70 spawns the exe; no default.profraw appears anywhere, confirming inheritance over the default path.
3. CONFIRMED badge failure reason swallowed: scripts/coverage_badge.py traces the exact failure to stderr, bt.run captures it, and tgt_coverage raises only `coverage badge failed with code N` without the tail. Probe: playground/adv77b run of bt.run with a corrupt export shows the child's `[fail] llvm-cov export unreadable: ...` line dropped. Every other failure path in the target funnels a tail; this one does not.
4. CONFIRMED structurally empty export crashes the badge script instead of failing traced: `{"data": []}` passes write_coverage_export's non-blank guard (35ae3b5 only rejects blank text), then coverage_badge.py:120 raises IndexError with a traceback. Loud (exit 1, no badge written) but breaks the entry/exit trace contract. Same for a missing totals key (KeyError at line 121). Probes: corrupt.json, nodata.json, nosummary.json, missing.json runs, exit codes 1/1/1/2.
5. CONFIRMED missing llvm tools fail loud before any build: BT_CONFIG override to absent paths exits 1 with `[fail] coverage BtError: tool override for llvm_profdata missing: ...` and zero compile/link lines. Logs: hub_no_profdata.log, hub_no_cov.log.
6. CONFIRMED unwritable badge dir fails loud: icacls write-denied sandbox docs dir gives PermissionError WinError 5, exit 1, neither report nor badge written, so no stale badge lands. Probe under playground/adv77b/sandbox with a copy of the 3 scripts.
7. HELD quiet-tree protection is not in the target: 35ae3b5's message says "wait for a quiet app tree" but bt.py got only the empty-export guard. The waiter lives in playground/t77cov/run_when_app_clean.py:20-38, watches only src/app and tests/unit/app, and is wired into nothing. `python bt.py coverage` on a busy shared checkout silently measures WIP code when the WIP still compiles and passes.
8. HELD bt.py grows past the 1000-line rule: 1899 non-blank lines before t77, 2046 after. The policy sloc scan's globs (tools/policy/lib.c:31-33) cover src, tools, tests, scripts and skip the repo root, so the driver is invisible to the cap gate. Pre-existing breach, deepened.
9. Toolchain verified against the installed 23.1.4 binaries: llvm-profdata merge documents `--output=<output>` and also accepts `-o` (probed: both parse, error came from the missing input), zero inputs errors loud, llvm-cov export has no output flag and writes json to stdout. The lane note "takes --output=, not -o" is imprecise but the code choice is safe.
10. Run 1 consumer truth: exit 0, 190 suites, badge reproduced 74.0% byte-identical to the committed svg (sha 2c44f7ba...), profiles dir and profdata scrubbed, tracked tree left clean.

## crash probe verdict

18. CONFIRMED loud failure on a corrupted suite exe, at the export stage: the run phase had already executed the suite before my corruption landed, then llvm-cov export failed with code 1 and the target raised `coverage export failed with code 1: error: failed to load coverage: ...tools_visual_diff_update_atomic_test.exe: The file was not recognized as a valid object file`, exit 1, badge untouched. This branch funnels the full error tail, unlike the badge branch. Log: playground/adv77b/e2e3_crash.log lines 329-334.
19. CONFIRMED spawn failure is loud through the whole chain: run_units_env lets OSError escape (probed live with the corrupted exe, WinError 216), and tgt_coverage's except tuple catches OSError into a traced [fail] line and exit 1. The nonzero-exit mid-run branch is covered by test_run_units_env_surfaces_nonzero_exit_codes plus the failed-list raise in tgt_coverage.
20. Noted: a failed coverage run leaves artifacts/coverage/profiles/ and coverage.profdata behind (scrub runs only on success). Intermediates for debugging, cleaned by the next successful run, acceptable.
21. Probe hygiene: corrupted exe deleted from bin-cov (relinks on demand), failed-run intermediates scrubbed, docs/badges/coverage.svg verified byte-identical to HEAD (sha 2c44f7ba) after every probe.

## resumed after run 2

11. CONFIRMED idempotence: run 2 exit 0, badge sha 2c44f7ba identical to committed and run 1, scrub held (only coverage.json and report.md remain). Report differs from run 1 only in the generated stamp and covered count 45427 vs 45425.
12. CONFIRMED measurement nondeterminism is one file: export run 1 vs run 2 differ only in tests/unit/tools/spawn_util.h (178 vs 176 covered of 203), totals 45427 vs 45425 of 61348, both 74.0% at 1 decimal. True value 74.043 to 74.046 percent, 3 to 4 regions below the 74.05 rounding flip, observed noise ±2 regions: the badge is stable today with roughly 2x margin over the noise scale. Probe: playground/adv77b/exportdrift.py.
13. CONFIRMED badge pin catches a hand-edited percentage: svg edited to 99.9% fails test_ci_badge with a byte assertion (41 ran, 1 failure), file restored byte-identical (sha 2c44f7ba). Residual: the pin is a hand-maintained literal tuple at tests/python/test_ci_badge.py:95-103, so editing svg and tuple together still passes with no rerun tying the number to a run.
14. CONFIRMED zero-guard ordering hazard: bt.py:1650 runs the badge script (child writes docs/badges/coverage.svg) before bt.py:1656 raises on percent <= 0. Probe: the script fed a valid all-zero export writes a red 0.0% badge and exits 0. A zero-measurement coverage run therefore leaves the tracked badge file modified to 0.0% in the shared checkout while exiting 1, and the pre-commit hook sweeps foreign staged files, so a concurrent lane commit can carry it. The guard belongs before the write or inside the script.
15. Policy: the full `python bt.py policy` run failed at link on build/bin/mutate.exe permission denied, a lane holds the exe locked, infra collision not a finding. The freshly linked build/bin/policy.exe ran the scan directly, exit 0, no findings, so the landed python passes the zero-comment, constants, and sloc scans over scripts and tests.
16. Cross-lane: coverage never writes build/bin (code: link_stage with coverage=True links bin_cov; unit test test_link_stage_coverage_uses_bin_cov_and_its_own_stamp). The build/bin churn observed during my windows (adv79a probe exes appearing and later cleared, app suite relinks, link_stamp.txt moving without link_stamp_cov.txt moving with it) tracks a concurrent plain build by another lane, not the coverage target. Shared surfaces that do race: generate_stage rewrites build/gen/*_main.c and hello.c under both flavors with identical bytes, a torn-read window for a concurrently compiling plain build, low severity.
17. Correction to finding 1's left-out list: src/core/lib.c, src/puzzles/lib.c and the tests/fuzz/*.c files are 1-line umbrella includes with no regions, immaterial. The material omissions are the 11 tools/*/main.c drivers plus src/app/main.c, 2049 source lines total, including tools/mutate/main.c at 532 lines and tools/verify/main.c at 261. The tests/fuzz logic itself is measured through its headers (528/946 regions), so the fuzz leg of the scope is real.

## reading log

read bt.py 100-200, 500-560, 809-830, 1374-1390, 1765-1800 - run helpers, prune, generate, child_env, policy target
read bt.py c8cc863 diff full - coverage target landing
read scripts/coverage_badge.py full - badge and report writer
read scripts/badgesvg.py full - svg renderer and color bands
read scripts/slog.py 1-30 - trace shim
read tests/python/test_bt_coverage.py full - 17 landed tests
read tests/python/test_ci_badge.py pin hunk - badge pin convention
read playground/t77cov/run_when_app_clean.py full - quiet tree waiter
read artifacts/progress/t77.md full - lane claims
read artifacts/progress/t77-cov-run3.log grep merge/export/badge/exit - landed run
read tools/policy/sloc.c full - cap logic
grep tools/policy/lib.c globs - scan roots
read tests/unit/tools/mutants_gate_test.c 55-70 - spawn pattern
grep src/common/paths.h - exe dir resolution
grep README.md coverage/badge/region - scope statements
