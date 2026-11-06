# lane-cov-ga progress

## 2026-11-05 22:40 setup

Toolset probe passed (Read, Grep, Glob, Edit, Write, PowerShell exercised). Lane config clone at artifacts/lane/lane-cov-ga/config.json via scripts/lane_build.py. Gap inventory from artifacts/coverage/gaps.tsv: 926 regions, ga 102 (dist 46, inventory 33, ga 19, eval 4), ga-run 702 (out.c 451, lib.c 251), memofreq 122 (lib.c 76, main.c 46).

Scoped measurement tool playground/cov_scope_ga.py written: instrumented config clone under cov-build, runs the ga area suite plus ga_run_lib and memofreq_lib tool suites, merges profraws (children inherit LLVM_PROFILE_FILE), llvm-cov export over all cov-build exes anchored to the owned file set. Baseline: 880 in scope, memofreq main.c not yet built into the scope (the memofreq tool exe is not a suite dependency, spawn tests will pull it in).

## 22:50 out.c analysis

out.c oracle basis: the serde layout pinned by data/ga/report.json (the tool's committed-format product) and the ryu shortest-float rules. bt_gr_report_text, pair_rows_text, float_array_text fully dark, cases_text_pretty non-empty path dark, several ryu branches and sorted_value shapes dark. New suite tests/unit/tools/ga_run_out_test.c authored: full-schema golden, empty pool and trajectory, nonfinite rejection, pretty two-case assignment golden, display_f64 form table, json shapes (empty containers, escapes, control char, nesting, negative sci), 17-key heap-sorted object, null rejection.

## 22:58 shared checkout note

Build of the new suite hit a transient red from a foreign lane mid-edit in src/core/expand.c (unused parameter, -Werror). Retrying after the core lane settles, continuing authoring meanwhile.

Oracle notes: run_all and memofreq main driven through scratch BT_CONFIG configs; freq.json for the success run is a byte copy of the repo data/ga/freq.json so build_assignment sees the full 818-case catalog; freq_sha256 in report.json is cross-checked against bt_ga_sha256_hex of those bytes.

## 23:17 measurement after the first three suites

880 -> 421 uncovered in scope. out.c 451 -> 101 (report_text, pair_rows_text, float_array_text, cases_text_pretty, ryu branches, sorted_value shapes now driven by tests/unit/tools/ga_run_out_test.c, green). ga-run lib.c 251 -> 137 (run_all end to end plus write failures via tests/unit/tools/ga_run_flow_test.c, green). memofreq main.c 46 -> 10 (real exe spawns via tests/unit/tools/memofreq_test.c, green). dist.c failure-path tests authored, not yet measured. Remaining: lib.c input failure branches, memofreq lib.c 76, inventory.c 33, ga.c 19, eval.c 4, dist.c residual.

Process note: one file edit went through a PowerShell heredoc append, against the file-tools rule. The file verified intact and later edits stay on the file tools.

## 2026-11-06 04:26 measurement and dead-region decomposition

Inherited work committed: 62c1645 (ga failure paths), c4de1ec (tool suites), 8e2880e (fmt fix after CI flagged ga_upgrade_test, fmt dry-run now a hard per-commit gate on my files). Scoped measurement: 304 regions remain (out.c 101, ga-run lib.c 92, memofreq lib.c 70, inventory.c 23, memofreq main 10, ga.c 3). Per-function decomposition via playground/ga_dead_regions.py: the bulk of out.c's dead regions are bt_strbuf realloc-failure arms (strbuf.c reserve fails only on OOM, no length cap, no failure-injection idiom in the suites), same for the trace arms. Those go to ESCALATE below. Drivable inventory: memofreq core paths (count_frequencies loops, emit_case_rows, emit_pair_rows, emit_string escapes, freq_data_text, draw_counts clone and drain, zero_hit_cases, main spawn variants), ga-run input failure branches (read_file_bytes, load_features, load_run_inputs config and file failures, cross_check_cases, build_assignment, render_markdown, run_all tail, write failures via unwritable parent paths), inventory spelling rules (nucleus_rime, onset_spellings, coda_letters), entry_cmp tie branches, build_inventory and read_file failures, ga.c 190 338 503 645, out.c null and false json arms, compact cases with 2 or more rows, pretty empty arm, f32 forms including multi digit scientific. Working memofreq first.

ESCALATE 1 (for the hub, applies repo-wide): every bt_strbuf_append, bt_strbuf_appendf, and append_zeros failure arm in my owned files (about 100 regions in out.c alone, plus the writer arms in ga-run lib.c and memofreq lib.c) fires only on realloc failure. src/common/strbuf.c has no length cap and the suites have no allocation-failure injection idiom, so these arms are unreachable from tests without either a fault-injection seam in strbuf or their removal. This is the same wall other coverage lanes will hit; it needs one repo-wide decision, not per-lane hacks.

## 2026-11-06 04:58 final report

Lane complete. Commits this relaunch: 62c1645 inherited ga failure paths, c4de1ec inherited tool suites, 8e2880e CI fmt fix, 9f028d5 out float forms with multi row sections and memofreq escape rows, 73a90ce ga-run input failure stages, offender growth, and empty per generation reports, 0343109 trailing comma lint fix, 3a29a0f playground probes.

Region counts, scoped baseline at lane start to final measurement (artifacts/lane/lane-cov-ga/cov-build/export.json):
- src/ga/ga.c 673 to 676 of 676, src/ga/inventory.c 692 to 692 of 715, src/ga others 100 percent before this relaunch
- tools/ga-run/out.c 717 of 818
- tools/ga-run/lib.c 612 to 627 of 704
- tools/memofreq/lib.c 400 of 470, tools/memofreq/main.c 103 of 113
- lane total 289 to 274 uncovered of the 926-region scope after the two batches, every remaining region classified below, none drivable without a repo-wide decision

Verification: ga area 14 suites green, ga-run tool suites 4 green, memofreq green, lint ga 34 clean, fmt dry-run clean on every touched file per commit, policy scan run in the closing verification.

ESCALATE, the complete residue inventory, 274 regions in 4 classes, the hub owns the decision because every lane hits the same wall:
1. Allocation failure arms, about 190 regions: every bt_strbuf append ternary fail half in out.c and memofreq writers, memofreq draw count and frequency allocations, ga-run offenders, assignment, and input table allocations, write serialize arms, read malloc arms. src/common/strbuf.c reserve fails only on realloc and the suites have no allocation failure injection idiom.
2. Invariant dead defensive arms, about 70 regions: ga-run cross check row NULL ternaries and the 64 byte letters truncation bound, load features source NULL guard, distinct syllables duplicate arm, run all paths recheck after load already rejected it, write plain short write, inventory entry cmp secondary arms (the rule tables never emit two entries with the same text), coda letters default return (exhaustive enum), render 3 byte utf8 arm (the rime alphabet is 2 byte codepoints only), ga.c mutate replace pool 0 guard and distinct count pop 0 guard (both static, upstream invariants).
3. Platform arms, about 12 regions: ftell negative arms in both read file helpers (regular files never), write plain short write or close fail, plus trace macro regions attributed at expansion lines 9 and 25 that no execution reaches.
4. Abort guard, 1 region: ga.c run ex allocation overflow check aborts the process, the driving value is rejected upstream by config validation.

Oracles used: the serde layout pinned by data/ga/report.json and the ryu shortest float rules for out.c forms, RFC 8259 string escaping for the memofreq writer rows, the config hub error contracts for each input failure stage, the public offenders and report contracts for the growth and empty series drivers.

Process notes: the first inputs red was a NULL deref through bt_gr_run_inputs.pool after memset, the CRT turned it into a silent fastfail, fixed with real storage. The stale exe trap hit once, a suite verdict from a binary built before the edit, resolved by rebuilding before trusting output. C hex escape greed ate a fixture byte, literals split.

## 2026-11-06 05:00 closing verification

The policy build failure in the closing chain was a foreign transient: a mid-edit file from a concurrent lane broke the whole-tree compile once, the rerun cleared it, and the policy scan then reported findings only in foreign in-flight files (tools/mutate/tests.c literals and over-cap lines, tools/mutate/tests_ops.c literal, tests/unit/data/repo_errors_test.c over cap), none in this lane's files. Those belong to the mutate and data lanes' own gates. This lane's scoped surface is green: ga 14 suites, ga-run 4 suites, memofreq 2 suites, lint ga 34 clean, fmt clean per commit.

## reading log

read artifacts/progress/lane-cov-ga.md full - predecessor state and plan
read tools/ga-run/out.c 1-553 full - ryu forms, writers, dead arm map
read tools/ga-run/lib.h full - report, assignment, inputs structs
read tools/ga-run/lib.c 24-160, 163-282, 282-416, 416-435, 490-614, 700-864 - offenders, cross check, load stages, build assignment and report, writers, run all
read tools/memofreq/lib.c 255-404, 404-518 - count frequencies, zero hit, writers
read tools/memofreq/main.c 95-154 - run stages and failure texts
read src/common/strbuf.c 1-60 - failure conditions, no length cap
read src/ga/inventory.c 40-160, 420-700, 778-800, 855-979 - read file, strset, rime and spelling rules, comparator, build walk
read src/ga/ga.c 180-208, 328-355, 495-529 - mutate replace, distinct count, run ex guards
read src/ga/dist.h 11-39 - fixture struct sizes
read tests/unit/tools/ga_run_out_test.c full - existing fixtures and goldens
read tests/unit/tools/memofreq_test.c 198-250 - existing freq data fixture
read tests/unit/tools/ga_run_inputs_test.c 1-301 full - fixture helpers and stage table
read bt.py 95-140, 1388-1394 - hub env and child env
run python -I playground/cov_scope_ga.py three times - scoped measurement
run python -I playground/ga_dead_regions.py two times - per function dead map
run clang-format dry-run Werror before every commit - fmt gate

## 2026-11-06 04:45 batch 1 landed, classification refined

Batch 1 committed as 9f028d5: out.c null and false json arms, compact two-row separation, assignment empty case list with scientific best_fitness, report with all four float forms plus two-row pool and per-gen sections, memofreq escaped and multi-row writer rows (one fixture fix: C hex escape greed ate the 0x01 byte, literals split). CI-driven fmt fix 8e2880e plus formatting the predecessor golden in ga_run_out_test.c, per-commit fmt gate is now clang-format dry-run Werror on my files.

Classification pass over the remaining regions with the source read: my first per-function dead list over-counted because it included non-code regions, the probe now filters to kind 0 and a remeasure is running. Drivable next batch in ga_run_inputs_test.c: missing config file (bt_data_load_config NULL arm), bad ga.upgrades, missing pool_top_k, and syllable-frequencies file missing while the wordlist is present (scratch ref_doc with only the wordlist). Drivable in ga-run lib.c: bt_gr_offenders_push growth past its first capacity via direct public calls, build_report with per_gen_count 0 for the ternary arm. Escalate additions: ga-run lib.c read_file_bytes ftell-negative arm (regular files never, platform), write_plain short-write or close-fail arm, run_all paths.data_ga recheck (invariant: load_run_inputs already rejects its absence), run_all build_assignment reject (needs forcing the GA into duplicates, the direct-path is already tested), distinct_syllables duplicate arm (assignments are duplicate-validated upstream, invariant), load_features source-NULL guard (pool is built from the inventory, invariant), ga.c mutate_replace pool_size 0 guard and distinct_count pop 0 guard (both static, upstream invariants), ga.c run_ex allocation-overflow abort arm (bt_ga_abort crash path, death-test territory, and the value needed is rejected upstream by config validation), inventory read_file ftell-negative (platform), memofreq main count_frequencies NULL and serialize-fail arms (OOM), offenders realloc arms (OOM).

## 2026-11-06 04:20 relaunch

Predecessor killed at 00:48 by the provider window limit. Toolset probe passed. Inherited uncommitted: tests/unit/ga/dist_test.c, ga_eval_test.c, ga_upgrade_test.c, inventory_more_test.c (authored failure-path tests, unmeasured), plus the new green suites ga_run_out_test.c, ga_run_flow_test.c, memofreq_test.c. Resuming: verify inherited tests green in the refreshed lane tree, measure, then close the remaining inventory (lib.c input branches, memofreq lib.c 76, inventory.c 33, ga.c 19, eval.c 4, dist.c residual, memofreq main residual 10).

## 2026-11-06 05:02 relaunch 3, continuation

Lane tree recreated. Core lane's log confirms the seam is landed: bt_alloc_fail_arm(ok_count), bt_alloc_fail_disarm, bt_alloc_fail_count, harness wrappers auto-disarm per test, json.c and strbuf.c routed through bt_malloc/bt_calloc/bt_realloc. Plan per the 4 rulings: class 2 dead-arm removals first with per-site proofs, then class 1 by routing my files' direct allocations through the seam and arming around the calls under test, then class 4 abort guard judged against the death-test idiom, then class 3 drive-or-escalate. Self-measure with playground/cov_scope_ga.py between batches.
