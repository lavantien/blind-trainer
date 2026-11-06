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

## 2026-11-06 08:45 relaunch as lane-cov-ga5

Toolset probe passed (Glob, Read, Grep, PowerShell exercised here, Edit via this append, Write on the next new file). Read the predecessor state below. Plan per hub brief: fix tools/ga-run/lib.c:247 void-return compile break, fmt fix tests/unit/tools/ga_run_flow_test.c, lint trailing comma in ga_run_inputs test, then class 2 dead arms, class 1 bt_alloc_fail arming, class 4 abort judgment, class 3 platform arms.

## 2026-11-06 08:55 routed fixes and class 2+4 closeout, commits 1b33b9f 70edbe0 038e79e

Predecessor's uncommitted tree verified and landed. The 08:40 -Wreturn-mismatch was a mid-edit state, the finished tree compiles, 229 objects ok, ga_run 4 suites green, ga area 14 suites green, tidy clean on ga.c and lib.c, fmt clean per commit.

Commits: 1b33b9f style(tools) fmt fix ga_run_flow_test.c (2 rewraps at 173, 205) plus ga_run_inputs_test.c trailing commas (readability-trailing-comma at 104 and 327, clang-format expanded the 2 wrapped rows to one element per line to satisfy both gates). 70edbe0 refactor(ga) dead guards in src/ga/ga.c. 038e79e refactor(tools) dead arms in tools/ga-run/lib.c.

Class 4 closed by 70edbe0, run_ex migrants overflow abort removed, proofs: config path caps island_count at 1024 (ga.c:729, BT_GA_ISLAND_MAX) and migrate_count at 1e9 (ga.c:742, BT_GA_INTERVAL_MAX), product 1.03e12 far under SIZE_MAX; direct callers that wrap the product must pass upgrades_check (ga.c:447, migrate_count <= population - elite) and survive pop-proportional allocations at ga.c:456-496, which trip bt_ga_run_alloc_n's own overflow abort (ga.c:293) or a real allocation abort first, wrapping needs over 90 GB committed heap.

Class 2 per-site proofs, all verified this session:
- mutate_replace pool_size 0 guard (ga.c:190): pool_size >= 1 at every caller. Tool path rejects pool_count < bt_ga_case_count() (lib.c:328), test paths pass 2 (ga_test.c:441,444). pool_size 0 died in both versions anyway at bt_rng_below and the modulo at ga.c:203.
- distinct_count pop 0 guard (ga.c:338): run_ex entry aborts unless population >= 2 (ga.c:434-438), the only caller passes params->population.
- cross_check row NULL ternaries: bt_json_get returns NULL on a NULL container (json.c:856), the ternaries repeated that.
- load_features source-NULL guard: build_pool strcpy-copies inventory entry texts into the pool (inventory.c:1018), every pool row matches by construction.
- run_all paths.data_ga recheck: load_run_inputs rejects a missing paths.data_ga before run_all reaches its own read (lib.c:339 vs lib.c:832), run_all calls load first (lib.c:813).
- distinct_syllables removal: build_assignment rejects duplicate syllables (lib.c:529-534) and run_all prints only after it returned 0 (lib.c:820 early return), so distinct always equals cases_count.

Remaining: class 2 residual sites (letters truncation bound, entry_cmp secondary arms, coda letters default, render 3 byte utf8 arm), class 1 allocation arms via the common/alloc.h seam, class 3 platform arms.

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

## 2026-11-06 09:10 relaunch as lane-cov-ga6

Toolset probe passed (Read, Glob, Grep, PowerShell, Edit, Write all exercised). Inherited from the disposed ga5: seam routing edits in tools/ga-run/lib.c lib.h out.c, modified ga_run_inputs_test.c and ga_run_lib_test.c, new unwritten tests/unit/tools/ga_run_oom_test.c. The tree also carries tools/memofreq/lib.c main.c edits plus new tests/unit/tools/memofreq_oom_test.c, checking whether those are ga5's too before claiming. First task: audit the half-finished batch against its suites, finish it, land it.

## 2026-11-06 05:02 relaunch 3, continuation

Lane tree recreated. Core lane's log confirms the seam is landed: bt_alloc_fail_arm(ok_count), bt_alloc_fail_disarm, bt_alloc_fail_count, harness wrappers auto-disarm per test, json.c and strbuf.c routed through bt_malloc/bt_calloc/bt_realloc. Plan per the 4 rulings: class 2 dead-arm removals first with per-site proofs, then class 1 by routing my files' direct allocations through the seam and arming around the calls under test, then class 4 abort guard judged against the death-test idiom, then class 3 drive-or-escalate. Self-measure with playground/cov_scope_ga.py between batches.

## 2026-11-06 10:22 relaunch as lane-cov-ga7

Toolset probe passed, all 6 tools. Inherited uncommitted batch from ga6: seam routing in tools/ga-run/lib.c lib.h out.c and tools/memofreq/lib.c main.c, modified ga_run_inputs_test.c and ga_run_lib_test.c, unwritten ga_run_oom_test.c and memofreq_oom_test.c. Auditing the diff first, then finishing class 1 arming, then class 3 drive-or-escalate.

## 10:40 audit findings

Inherited source routing verified coherent: bt_gr_build_report gained an int return with offenders wiring and its run_all caller frees and returns -1 on failure, memofreq offenders_push and all lib.c allocators route through the seam, memofreq main arms from BT_MEMOFREQ_ALLOC_FAIL_OK. ga_run_oom suite green as inherited (207s). memofreq_oom timed out at 300s: the spawn test walked every budget below the full one, and one 30000-draw exe run costs 21s measured, so the exhaustive design was infeasible by construction. Reworked it: power-of-two budget sweep at 300 samples (13 spawns, all must exit 1 with no freq.json, zero-hit cases guarantee exit 1 at any finite budget) plus one large armed budget full run proving exit 0, freq.json, and the stdout report. Recurring literals centralized as MF_* constants. Rerun in flight.

## 11:05 class 3 driven with pipe and sparse evidence

memofreq_oom green after the rework (27s). All 5 ga_run tool suites green. Class 3 probes (playground/pipe_probe.c, playground/sparse_probe.c, both run): MSVC fopen on a named pipe client works with forward slashes in the pipe name, fseek succeeds and ftell returns 0 on a pipe so the pipe does not drive the ftell arm, a server-side close makes the client fwrite return 0 with errno 22 which drives the write_plain short write or close fail arm, and a sparse 2^31+1 file gives fopen success with ftell -1 errno 22 which drives both ftell arms. New suite tests/unit/tools/ga_run_io_test.c: cross_check fails closed on a sparse oversize cases.json, run_all writes syllables.json and report.json then fails closed on the markdown write into a named pipe whose server closes on connect, with the pipe connection asserted so the fopen arm is ruled out. Shared fixture bt_ga_make_oversize added to tests/unit/ga/ga_test_util.h, wordlist oversize test appended to inventory_more_test.c. Suites running.

## 14:15 window reset, resuming in place

State before the pause: cross_check oversize ftell test green, inventory wordlist oversize test pending rerun, markdown pipe test found the naive close-after-connect design racy in the client's favor (run_all returned 0, the 1.2KB markdown fits the default pipe buffer in one chunk and the parked server thread loses the wake race). Markdown is size-bounded by the 20-value trajectory cap so the payload can never overflow a default buffer. New hypothesis under probe: nInBufferSize=16 makes the client's flush block without a draining server, so a connect-then-close server breaks the blocked write at any interleaving, contingent on Windows honoring the small buffer hint. Probe playground/pipe_probe2.c compiles and runs next. Lint note from the hub received: no compound or negated boolean returns in bool functions.

## 13:35 class 3 fully driven, deterministic

pipe_probe2 proved the mechanism: with nInBufferSize=16 the client's fclose flush blocks in WriteFile because 1200 bytes cannot fit undrained, and the server close turns the blocked write into fclose=-1 errno 32. The markdown pipe fixture now uses PIPE_ACCESS_INBOUND with the 16-byte inbound buffer and a connect-then-close server, so the write can never complete at any interleaving: no race. ga_run_io suite green (cross_check oversize cases.json ftell arm plus markdown short-write arm, connection asserted to rule out the fopen arm), inventory_more green with the wordlist oversize ftell arm. clang-format clean over all 12 touched files after 3 rewraps. Class 4 confirmed already closed by 70edbe0 with the config-cap proofs, no death-test judgment remains. Suites rerun on the formatted state plus lint ga-run and memofreq in flight, commits next.

## 13:33 lint fixed on my files, foreign failures left to their lanes

Lint findings on my two oom files were the lowercase shift suffix and the signed right operand, fixed to the repo idiom 1U << 24U and 1U << 16U. Lint ga-run 15 clean, memofreq 14 clean, ga area 34 clean, tools area 80 clean with 5 remaining failures all in foreign in-flight files (gendata_model_test, gendata_oom_test, mutate_oom_test, mutate_test, release_verify_spawn_test), not mine to touch. One more scoped rerun of ga_run_oom and memofreq_oom on the final literal state, then commits.

## 19:15 window reset, resuming in place

Hub summary was stale: the small-buffer pipe probe was already answered before the halt (pipe_probe2: fwrite buffers all bytes, the flush into the 16-byte inbound buffer blocks, the server close makes fclose return -1 errno 32, ga_run_io suite green on that fixture), class 3 fully driven, class 4 confirmed closed by 70edbe0, fmt and lint clean on my files. The final rerun of both oom suites on the post-literal-fix sources completed before the halt with a lagged notification: ga_run_oom 1 passed, memofreq_oom 1 passed, fmt-exit 0 over all 12 touched files. Everything green, committing the three batches now.

## 19:20 batches landed

Commits: 22244a2 test(tools) allocation arms through the fail seam (9 files), d22b27d test(ga) platform arms with sparse and pipe fixtures (3 files), 470f7fe chore(playground) probes (4 files, one format-gate refusal fixed by formatting the probes). cov_scope_ga.py suite list extended with ga_run_oom, ga_run_io, and memofreq_oom so the measurement counts the new suites, measurement running for the delivery numbers.

## reading log

read artifacts/progress/lane-cov-ga.md full - predecessor state, 4-class plan, seam history
read tools/ga-run/lib.h full - report and inputs contracts, build_report signature change
read tools/ga-run/lib.c 60-180, 282-460, 631-745, 746-880 - read_file_bytes, cross check, load stages, render markdown, write_outputs, run_all
read tools/ga-run/out.c 280-300 grep - sorted_value heap order arm
read tools/memofreq/lib.c 120-400 diff - seam routing of drain, draw counts, count_frequencies, zero hits
read tools/memofreq/main.c full - offenders push, run stage order, env arm seam
read src/common/alloc.c full - seam semantics, failures counter, no total counter
read src/common/bt_harness.h 24-32 grep - arm and disarm wrappers
read tests/unit/tools/spawn_util.h full - spawn capture, bin path, mkdirs
read tests/unit/tools/ga_run_oom_test.c full - inherited budget walks
read tests/unit/tools/memofreq_oom_test.c full - inherited spawn walk, reworked
read tests/unit/tools/memofreq_test.c 88-130 grep - sample sizing and spawn idiom
read tests/unit/ga/ga_test_util.h full - scratch helpers, added oversize fixture
read tests/unit/ga/inventory_more_test.c 286-345 - append point, growth test shape
read src/common/paths.c 47-58, 115-140 - unc lead passthrough in join
read src/data/paths.c 81-101 - config path resolution
read bt.py 102-110, 1354-1394, 1461-1475, 1513-1549 - hub env, lint areas, suite exec, test-unit
read scripts/commit_lane.py usage - explicit paths, lock, staged verify
run python bt.py test-unit tools ga_run_oom, memofreq_oom, ga_run (5 suites), ga_run_io, ga_run inventory_more - green
run clang-format dry-run Werror per commit - fmt gate
run python bt.py lint ga-run, memofreq, tools, ga - lint gate
run playground pipe_probe.c, pipe_probe2.c, sparse_probe.c - pipe and sparse CRT facts
read tests/unit/tools/ga_run_io_test.c full - final formatted state check
read tests/unit/tools/memofreq_oom_test.c 218-262 - formatted config writer and spawn helper
read bt.py 1341-1352 - lint area matching
read playground/cov_scope_ga.py 1-20, 47-64 - suite list extended with the three new suites

## 2026-11-06 19:08 relaunch as lane-cov-ga8, delivery closed

Toolset probe passed, all 6 tools. The predecessor's closing measurement was complete on disk: the scoped config was rebuilt at 18:22:41 right after the last commit 470f7fe at 18:22:32, all 9 suite exes were built in cov-build between 18:22:46 and 18:28:33, and export.json was written at 18:29:03. No owned file changed after that. The numbers were regenerated from that export through cov_scope_ga.report(), no suite rerun needed.

Final numbers, uncovered regions per owned file from artifacts/lane/lane-cov-ga/cov-build/export.json: src/ga/alloc.c 0, dist.c 5 of 262, eval.c 0, features.c 0, ga.c 0 of 665, hash.c 0, inventory.c 20 of 715, lib.c 0, tools/ga-run/lib.c 37 of 665, tools/ga-run/out.c 76 of 818, tools/memofreq/lib.c 16 of 479, tools/memofreq/main.c 4 of 123. Lane total 158 uncovered of 4206 measured regions, 96.2 percent covered, from 880 uncovered at the first scoped measurement and 274 at the 04:58 classification, against the 926-region gap inventory at setup.

Commits, full lineage: 62c1645, c4de1ec, 8e2880e, 9f028d5, 73a90ce, 0343109, 3a29a0f, 1b33b9f, 70edbe0, 038e79e, 22244a2, d22b27d, 470f7fe, plus the 2 closing commits below.

Class verdicts with evidence:
1. Allocation arms: routed through the common seam and driven by ga_run_oom and memofreq_oom, commit 22244a2. The residue the budget walks did not reach, all still drivable by arming the seam around direct calls of the public entry points: the out.c writer append arms (the bulk of its 76), the memofreq writer arms 421 to 510 and the cases malloc at 290, the ga-run read_file_bytes malloc at 85, the pair weight cooc and adj mallocs at 395 and 397, the build_assignment cases malloc at 500, the compact serialization failure at 541, the per generation allocation at 587, the run_all build_report reject at 836, the memofreq main count_frequencies arm at 113. An exhaustive spawn walk is infeasible, 21 seconds per full memofreq run measured, so any extension must arm targeted call sites, not sweep budgets.
2. Invariant dead arms: proven sites removed in 70edbe0 and 038e79e, the per-site proofs recorded at 08:55. Residual sites identified but not removed, removal stays the verdict: the inventory entry_cmp secondary arms 784 to 796 (the rule tables never emit 2 entries with the same text), the coda letters default at 452 (exhaustive enum), the 3 byte utf8 decode arm at 696 (the rime alphabet holds 2 byte codepoints only), the dist.c json NULL ternaries at 64 and 65.
3. Platform arms: driven deterministically, commit d22b27d. Pipe and sparse probe outcomes: fopen works on a named pipe client with forward slashes and ftell returns 0 on a pipe, so a pipe cannot drive the ftell arms, a sparse 2^31+1 byte file gives fopen success with ftell -1 errno 22 and drove both ftell arms in the ga-run helpers and the inventory read, and a pipe with a 16 byte inbound buffer makes the client fclose flush block in WriteFile because the payload cannot fit undrained, so a connect then close server turns the blocked write into fclose -1 errno 32 at any interleaving with no race, the markdown payload being bounded by the 20 value trajectory cap. Residual: the dist.c ftell arm at 20, drivable with the same sparse fixture through a dist entry point.
4. Abort guard: closed by removal in 70edbe0. The caps proof holds on the current source, ga.c 18 and 19 pin island max 1024 and interval max 1e9, ga.c 729 and 742 enforce them in config validation, product 1.03e12 far under SIZE_MAX, and a wrapping caller must first pass upgrades_check and survive the population proportional allocations, wrapping needs over 90 GB of committed heap.

Escalations: the single escalate from 04:26, the repo-wide allocation failure seam, was resolved when the core lane landed bt_alloc_fail_arm and routed json.c and strbuf.c through bt_malloc and bt_realloc. No open escalation remains, the residue above is classified successor work.

Current state for a successor: all 9 scope suite entries were green in the 18:22 to 18:29 run (ga area 14 suites, ga-run 6, memofreq and memofreq_oom), lint rechecked clean at the close below. Measurement takes python -I playground/cov_scope_ga.py about 7 minutes, a report only reread of the existing export imports cov_scope_ga from the playground path and calls report(). The current export sits at artifacts/lane/lane-cov-ga/cov-build/export.json, written 18:29:03. Next work in order: extend the seam armed tests to direct writer entry points, remove the 4 class 2 residual sites, drive the dist.c ftell arm with the sparse fixture, drive the input dependent lib.c arms (cross_check row mismatch at 192, build_inventory failure at 322, build_env failure at 419, run_all assignment reject at 828).

## reading log

read artifacts/progress/lane-cov-ga.md full - predecessor state and the open closeout
read playground/cov_scope_ga.py full - suite list, merge, report logic
read tools/ga-run/lib.c 83-92, 190-198, 320-328, 393-423, 497-504, 539-547, 584-591, 826-844 - residue classification
read src/ga/inventory.c 450-456, 693-699, 782-798 - class 2 residual sites
read src/ga/dist.c 18-25, 62-66 - ftell arm and json ternaries
read tools/memofreq/lib.c 287-294, 419-458, 468-512 - writer allocation arms
read tools/memofreq/main.c 111-117 - count_frequencies arm
read tools/ga-run/out.c 46-51, 111-120, 250-257, 304-308, 448-453 - writer append arms
grep src/ga/ga.c island and interval caps - class 4 proof check on current lines
run cov_scope_ga.report() over the existing export - delivery numbers
run python bt.py lint ga, ga-run, memofreq - closing lint gate
