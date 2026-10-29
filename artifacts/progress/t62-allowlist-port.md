# t62 allowlist port analysis lane

Working log, newest step first.

- 2026-10-29 ~11:55 local: deliverable drafted in full below. Union recomputed with an alg.c coordinate remap (2b50aff moved lines mid-fleet): 1,680 fresh + 2,960 remapped rows verified token-exact, 1,970 obsolete rows (the deleted whitespace_len block) excluded, 0 ambiguous, 0 stale-coordinate rulings after the hoist commit. Family split done, port table done, puzzles snapshot confirmed the 6 candidate rows. Committing.
- 2026-10-29 ~11:20 local: fleet snapshot run for data,puzzles,ga: puzzles complete (9 suites, 4,308 sites, 643 survivors), data 18 suites live (pending 877 to 9,179 per suite), ga 10 suites queued (2,352 pending each). src/common and src/app are in no plan's source list, flag recorded.
- 2026-10-29 ~10:45 local: Rust twin shapes verified for every reachable-path comparison survivor (iterator/range loops, sort_unstable, derive(PartialEq), Vec based buffers). Result: no strict port-gap candidates in core plus scheme, section 3 documents the family proofs and the claim boundary.
- 2026-10-29 ~10:20 local: lane open. Read plan ruling section, tools/bt-mutants-gate/allowlist.tsv (42 rows), config/allowlist.tsv (header only, confirmed), located artifacts/mutants ledgers (59 suites) and %TEMP%\bt-mut-union-preview.ps1. Next: lib.c row semantics, tools/mutate op names, report.c function derivation, union evidence for core plus scheme.

## 1. Genre mapping table

The 42 Rust rows carry 3 cargo-mutants genre tokens: BinaryOperator, FnValue, MatchArmGuard. The C ledger genre column is the bt_mut_op_name value from tools/mutate/operators.c:10-24. The C engine emits exactly 5 ops and its comparison swaps are a fixed table (operators.c:271-291), so a Rust token ports only when the C table emits that exact direction.

| cargo-mutants class | token in the 42 rows | C op and token | verdict |
|---|---|---|---|
| BinaryOperator, comparison boundary | >->>=, <-><= | ComparisonSwap, >->>=, <-><= | exact port |
| BinaryOperator, comparison flip | ==->!= | ComparisonSwap, ==->!= | exact port |
| BinaryOperator, comparison boundary | >->==, >=-><, <->> | nothing | C never emits these directions (C emits only >->>=, >=->>, <-><=, <=-><, ==->!=, !=->==) |
| BinaryOperator, arithmetic | %->+, *->/, +->-, -->/ | nothing | C mutates no arithmetic operator |
| BinaryOperator, bitwise | ^->\|, \|->^ | nothing | C mutates no bitwise operator |
| BinaryOperator, assignment | +=->*= | nothing | C mutates no assignment operator |
| BinaryOperator, logical | &&->\|\|, \|\|->&& | nothing | C never swaps logical operators, ConditionNegation negates a whole if or while condition instead |
| FnValue | String::new(), "xyzzy".into() | nothing | C has no function body replacement mutator |
| MatchArmGuard | true | nothing | C has no match arms, the twin control flow is if else chains |
| no Rust genre | numeric literals | NumericTweak (4 variants per literal: value+1, value-1, 0, 1) | C specific |
| no Rust genre | statements | StatementDelete (identifier-led statements, trace macros exempt) | C specific |
| no Rust genre | whole conditions | ConditionNegation (if and while (...) to !(...)) | C specific |
| no Rust genre | !, ++, -- | UnaryDrop | C specific |

## 2. Port table, all 42 rows

Mapping rule verified against the tree: crates/<crate>/src/<mod>.rs maps to src/<area>/<mod>.c for every row below, including the seedpack subdirectory. Function names are what bt_mut_function_at reports (token.c:398-438 takes the identifier before the parameter list paren), confirmed by locating each C definition. Counts for the 2 core rows come from the remapped union evidence, counts for the 6 candidates from the puzzles evidence snapshot in section 4, minting takes the final union-report count.

| # | Rust row | C row draft | fate |
|---|---|---|---|
| 1 | bt-scheme scramble.rs build_occ %->+ 1 | | DROPPED, arithmetic class never generated |
| 2 | bt-scheme scramble.rs random_occ %->+ 1 | | DROPPED, arithmetic class never generated |
| 3 | bt-scheme memo.rs OrbitWalk::run +=->*= 2 | | DROPPED, assignment class never generated, the C hang twins at the same guards are engine specific rows (bt_scheme_walk_run StatementDelete spins-> and BT_SCHEME_ABORT-> both timeout) |
| 4 | bt-core analysis.rs corner_twist >->>= 1 | src/core/analysis.c bt_corner_twist ComparisonSwap >->>= 1 | PORTED, union survivor at 279:35, count 1 |
| 5 | bt-core moves.rs layer_perm <-><= 1 | src/core/moves.c move_layer_perm ComparisonSwap <-><= 1 | PORTED, union survivor at 192:38, count 1 |
| 6 | bt-core moves.rs layer_perm %->+ 1 | | DROPPED, arithmetic class never generated |
| 7 | bt-core expand.rs tokenize +=->*= 2 | | DROPPED, assignment class never generated, the C hang twins are engine specific rows (expand_tokenize NumericTweak 1->0 and StatementDelete index-> at 98:13 both timeout) |
| 8 | bt-data verify.rs verify_threestyle >=->< 1 | | DROPPED, C never emits >=-><, a surviving >=->> at the site would take an adapted row at data adjudication |
| 9 | bt-ga hash.rs sha256 ^->\| 3 | | DROPPED, bitwise class never generated and the target code left src/ga/hash.c in the port, the implementation lives in src/common/sha256.c which no fleet plan mutates |
| 10 | bt-data seedpack/normalizer.rs valid_cube_token >->>= 1 | src/data/seedpack/normalizer.c valid_cube_token ComparisonSwap >->>= | candidate, exact token, count pending data evidence |
| 11 | normalizer.rs expand_bracket_repeats \|\|->&& 1 | | DROPPED, logical class never generated |
| 12 | normalizer.rs find_group_repeat &&->\|\| 1 | | DROPPED, logical class never generated |
| 13 | normalizer.rs normalize_sq1 +=->*= 2 | | DROPPED, assignment class never generated |
| 14 | normalizer.rs find_group_repeat +=->*= 1 | | DROPPED, assignment class never generated |
| 15 | bt-puzzles fto.rs vertex_id <->> 1 | | DROPPED, C never emits <->>, the surviving C twin class vertex_id <-><= (snapshot count 1) mirrors the same site and takes a new row at adjudication |
| 16 | bt-puzzles fto.rs vertex_id <-><= 1 | src/puzzles/fto.c vertex_id ComparisonSwap <-><= 1 | candidate, snapshot survivor at 34:36, count 1 |
| 17 | bt-puzzles fto.rs face_mask \|->^ 1 | | DROPPED, bitwise class never generated |
| 18 | bt-puzzles fto.rs rotate ==->!= 1 | src/puzzles/fto.c rotate_point ComparisonSwap ==->!= 1 | candidate, snapshot survivor at 55:17, count 1 |
| 19 | bt-puzzles fto.rs dot *->/ 3 | | DROPPED, arithmetic class never generated |
| 20 | bt-puzzles fto.rs dot +->- 1 | | DROPPED, arithmetic class never generated |
| 21 | bt-puzzles fto.rs cross + 5 | | DROPPED, arithmetic class never generated |
| 22 | bt-puzzles fto.rs cross / 2 | | DROPPED, arithmetic class never generated |
| 23 | bt-puzzles skewb.rs rotate ==->!= 1 | src/puzzles/skewb.c rotate_point ComparisonSwap ==->!= 1 | candidate, snapshot survivor at 41:17, count 1 |
| 24 | bt-puzzles skewb.rs dot *->/ 3 | | DROPPED, arithmetic class never generated |
| 25 | bt-puzzles skewb.rs cross + 5 | | DROPPED, arithmetic class never generated |
| 26 | bt-puzzles skewb.rs cross / 2 | | DROPPED, arithmetic class never generated |
| 27 | bt-puzzles skewb.rs anchors +->- 2 | | DROPPED, arithmetic class never generated |
| 28 | bt-puzzles mega.rs describe_move FnValue String::new() 1 | | DROPPED, FnValue class never generated, C describe_move exists at mega.c:149 and its surviving engine classes get their own rows at adjudication |
| 29 | bt-puzzles mega.rs describe_move FnValue "xyzzy".into() 1 | | DROPPED, FnValue class never generated |
| 30 | bt-puzzles sq1.rs parse_int >->== 1 | | DROPPED, C never emits >->== |
| 31 | bt-puzzles sq1.rs parse_int >->>= 1 | src/puzzles/sq1.c sq1_parse_int ComparisonSwap >->>= 2 | candidate, snapshot survivors at 192:26 and 199:27, count 2 versus the Rust count 1 |
| 32 | bt-puzzles sq1.rs scan +=->*= 1 | | DROPPED, assignment class never generated |
| 33 | bt-puzzles nxn.rs parse_token \|\|->&& 1 | | DROPPED, logical class never generated |
| 34 | bt-puzzles nxn.rs format_token MatchArmGuard true 1 | | DROPPED, no match arms in C |
| 35 | bt-puzzles nxn.rs face_of_normal >->>= 3 | src/puzzles/nxn.c nxn_face_of_normal ComparisonSwap >->>= 3 | candidate, snapshot survivors at 89:26, 92:26, 94:22, count 3 matches the Rust count |
| 36 | bt-puzzles nxn.rs find_notation +=->*= 1 | | DROPPED, assignment class never generated |
| 37 | bt-puzzles nxn.rs lookup_letter +=->*= 1 | | DROPPED, assignment class never generated |
| 38 | bt-puzzles nxn.rs cell_of -->/ 1 | | DROPPED, arithmetic class never generated |
| 39 | bt-puzzles nxn.rs sticker_center +->- 2 | | DROPPED, arithmetic class never generated |
| 40 | bt-puzzles nxn.rs apply_move +=->*= 4 | | DROPPED, assignment class never generated |
| 41 | bt-puzzles nxn.rs solved_state +=->*= 2 | | DROPPED, assignment class never generated |
| 42 | bt-puzzles nxn.rs is_solved +=->*= 3 | | DROPPED, assignment class never generated |

Fate tally: 2 rows ported with live core evidence, 6 candidates with exact token matches awaiting their area evidence (1 data, 5 puzzles), 34 dropped (23 arithmetic or assignment, 2 bitwise, 3 logical, 3 comparison directions the C table never emits, 2 FnValue, 1 MatchArmGuard). Justifications carry over verbatim for all 8 surviving mappings, they argue C level equivalence at the same statements.

## 3. Core and scheme union classification

Evidence: all 28 core and scheme suites complete, 0 pending rows. Union over the ruling's site key (file, line, col, op, from, to) with precedence killed over timeout over survived, builderr only when no suite built the mutant. Computed by playground/allowlist-port/union_core_scheme.py, which also replicates bt_mut_function_at for names and remaps alg.c coordinates (see integrity note). Totals: 5,163 distinct sites, 3,311 killed, 837 survived, 25 timeout, 990 unviable. Survivor surface: 450 classes (file, function, op, replacement), 837 rows.

Integrity note, alg.c mid-fleet tree change: commit 2b50aff (10-28 07:38) removed the local whitespace_len block while 10 of the 15 core suites had already enumerated alg.c on the old coordinates. The remap verified every row's token at its coordinates in one of the two revisions: 1,680 rows fresh, 2,960 rows remapped to the current tree, 1,970 rows excluded as obsolete (their sites were the deleted whitespace_len block, whose descendant code lives in src/common/space.c, a file no plan mutates), 0 ambiguous. No remapped row was ruled after 07:38, so no verdict was earned against a mismatched file state. Consequence for t61: the union-report must apply the same remap or it will emit 1,970 phantom outcomes with names resolved from the wrong lines.

### Class a, covered by a ported row: 2 classes, 2 rows

| C class | count | site |
|---|---|---|
| src/core/analysis.c bt_corner_twist ComparisonSwap >->>= | 1 | 279:35 |
| src/core/moves.c move_layer_perm ComparisonSwap <-><= | 1 | 192:38 |

### Class c, port-gap candidates: 0 classes

No core or scheme survivor class qualifies. The ruling defines a port-gap candidate as a site whose Rust twin mutant was killed by the Rust suite, making the C twin provably weaker. Every reachable-path comparison survivor was checked against its Rust twin's shape and each fails the test for one of three verified reasons:

1. The Rust twin has no mutable operator at the site: Rust used iterator or range loops (corner_twist at analysis.rs:164-176, position_of at cube.rs:97-102, the classifier defect walks), sort_unstable (face_key at scheme.rs:60-67), derive(PartialEq) equality (classifier.rs:6-117, the Residue and trace types), or Vec based buffers with no capacity constant (alg.rs has no 64 cap, expand.rs has no 256 word cap). The C port lowered these to index loops, hand sorts, hand equality, and fixed buffers, so the survivor targets structure cargo-mutants never saw.
2. The C token is an admission flip the cargo-mutants table does not generate: the C engine turns >= into > and < into <=, admitting the boundary value, while the workspace's cargo-mutants rows show its flips reject or reverse (>=-><, <->>). The C admission directions land on guards whose boundary value is outside the suite domain (sticker >= 48, face >= 6, edge >= 12, quarter > 3, s >= 54), so the admitted case never occurs.
3. The class is one of the 4 C-only ops (NumericTweak, StatementDelete, ConditionNegation, UnaryDrop), for which no cargo-mutants twin exists anywhere, so no Rust kill ever happened.

### Class b, C specific justified rows: 448 classes, 835 rows

Families with their justification. Row counts are exact from the ledger grouping.

| family | classes | rows | justification |
|---|---|---|---|
| StatementDelete on crash paths: abort 11 classes, bt_trace_dump 11, BT_SCHEME_ABORT 4, BT_TRACE_FAIL 1 | 27 | 40 | the deleted statements are the crash handlers for invalid internal state (face_char, slice_char, axis_char, piece_of, sticker guards, walk budgets), no suite input reaches the guarded branch because a reached handler would abort the clean baseline, and cargo-mutants generated no statement mutants, so no Rust twin exists |
| StatementDelete on cleanup: bt_alg_free 5, free 2, tokens 1, alg 1 | 9 | 36 | the deleted statements release error-path buffers, the leak changes no verdict without allocator instrumentation and the Rust Drop impls were never mutated |
| StatementDelete on lazy init: analysis_ensure 7, g_analysis_ready 1, g_tables_ready 1, require_cube_shaped 2 | 11 | 11 | the port introduced runtime lazy init where Rust evaluated tables at compile time, the first in-process caller initializes and every later deletion of the guard or call is absorbed, and validation-only calls see only valid inputs in the suite |
| StatementDelete on state and output writes: out 5, equal 2, walk 2, w 1, entries 1, face_used 1, identity_occ 1, swap_u8 1, edges 1, corners 1, seen 1, split 1, j 1, twist_first 1, twist_second 1, swap_twist 1, summary 1, snprintf 1, bt_scheme_build_occ 1 | 25 | 40 | each deletion changes output or state only on shapes the suite never distinguishes, cargo-mutants has no statement genre, flag: these are the weakest rows, listed below for hub review |
| NumericTweak on literal 0, loop inits, sentinels, mod bases | 74 | 162 | the 4 variants shift a loop start or a zeroed base, the shift is absorbed by first-iteration redundancy or equal treatment of the shifted value under suite inputs, cargo-mutants has no literal genre |
| NumericTweak on quarter and axis class literals 1, 2, 2U, 3, 4U | 112 | 200 | the tweak moves a quarter class or an axis count, the suite outcomes are invariant under the class shift at these sites, flag: the two 4U rows sit in quarter arithmetic where the Rust identity-composition argument may carry, hub looks at moves.c |
| NumericTweak on domain bounds 6, 8, 12, 24, 48, 255 | 67 | 98 | the boundary value is outside the valid domain, the tweak admits or shifts a case no suite input produces, the 255 sentinel sits above every legal sticker id |
| NumericTweak on the domain size 54 | 44 | 111 | the extra or missing cell walks past a 54 array into memory the loop order or a later pass rewrites, 6 sibling rows present as timeout instead, flag: layout dependent, listed below |
| NumericTweak on capacities and budgets 10U, 16, 64, 200, 256 | 15 | 15 | suite inputs never cross the growth threshold, buffer cap, or abort budget the tweak moves, the guard boundary shift keeps the abort semantics the should_panic twins test |
| ComparisonSwap <-><= beyond the ported row | 24 | 33 | 2 shapes: equal-never domains (sorted keys over distinct stickers, min scans over distinct cycle members, NUL terminated walks where the admitted index hits the terminator) and index loops over invariants where the extra probe reads a value the invariant makes unreachable |
| ComparisonSwap ==->!= and !=->== | 8 | 38 | the flipped equality guards crash paths and kind dispatch dead under valid inputs (bt_move_table, assert_case_legal), or feeds hand written equality whose Rust twin is derive(PartialEq) and was never mutated (classification_eq, trace_eq, residue_class_eq), flag: the equality family is weak, listed below |
| ComparisonSwap >=->> plus >->>= beyond the ported rows | 21 | 31 | boundary admission on guards whose boundary value no suite input reaches (range checks, depth caps, word caps, block budgets), or budget guards where one extra spin changes no outcome |
| ConditionNegation | 8 | 15 | band_of negation is provably invariant because both operands of the compared bands pass through the same negated function, the rest negate dead guards, hand sorts with no Rust twin (face_key), or derive backed equality |
| UnaryDrop on ++ | 3 | 5 | the dropped increment leaves an output count unadvanced, the suite never reads the count back, cargo-mutants has no increment genre |

Hub review list, the weakest justifications where a 1 line test may be cheaper than a row: bt_alg_eq StatementDelete equal-> and NumericTweak 0->1, 0->-1 at alg.c:49 (no test asserts bt_alg_eq false on unequal same-length algs), the classification_eq, trace_eq, residue_class_eq flip families (no test compares records differing in exactly one field), scheme.c face_key ConditionNegation at 81:5 (breaks the third sort pass), classifier.c bt_scheme_report_position StatementDelete out-> at 295:17 and UnaryDrop ++-> at 295:23 (no test asserts a 2 block position string), the 54 size walks whose survival depends on the overflow landing where a later pass rewrites (bt_summarize 289 and 294, random_occ 89 and 125 and 128, defect_flip 339, defect_twist 352, flipped_pieces 366, twisted_pieces 378, corner_succ 107), and moves.c bt_core_init <-><= at 248 and 251 plus the 4U tweaks (the extra init pass writes one table block past its array).

### Timeouts: 25 rows, not classified this round

25 timeout rows across 6 files await the mandated serial single-worker re-run before any ruling: alg.c bt_alg_parse (2), expand.c expand_seq (6) and expand_tokenize (2), moves.c move_compose (2) and move_power (4), classifier.c bt_scheme_trace (2), memo.c bt_scheme_memo (1) and bt_scheme_walk_run (2), scramble.c bt_scheme_perm_sign (1), bt_scheme_random_occ (1), identity_occ (2). Mechanism matches the Rust hang rows: loop-advance deletions and 54 bound tweaks that walk memory until the harness cap.

## 4. Data, puzzles, ga

Rust rows by area: data 6 (verify.rs verify_threestyle, normalizer valid_cube_token, expand_bracket_repeats, find_group_repeat twice, normalize_sq1), puzzles 31 (fto 8, skewb 5, mega 2, sq1 3, nxn 10 plus the second skewb block row 27 counted here, total 28 in the table numbering 15 to 42 plus rows 8 and 10 belong to data), ga 1 (hash.rs sha256). After the port: data keeps 1 candidate row (valid_cube_token >->>=), puzzles keeps 5 candidate rows plus 26 dropped, ga keeps 0 (row 9 dropped, class never generated and code moved).

Fleet evidence snapshot taken 2026-10-29 ~11:20 local by the same script over data,puzzles,ga:

| area | suites | evidence |
|---|---|---|
| puzzles | 9 | complete, 0 pending, 4,308 sites, 2,900 killed, 643 survivors, 52 timeout, 713 unviable, adjudication ready next and the 5 candidate rows confirmed surviving with counts 1, 1, 1, 2, 3 |
| data | 24 ruled or ruling | 18 suites still pending (877 to 9,179 rows each), all 25 data files excluded from union evidence until every data suite closes, verify.c and normalizer.c among them |
| ga | 10 queued | 2,352 pending rows each, all 6 ga files excluded, and with row 9 dropped the ga area needs no ported row at all |

Coverage fact for the hub: the 80 plans mutate 52 files, the 5 area trees only. src/common (15 files, including the sha256 and space code that rows 9 and the obsolete alg.c rows point at) and src/app (about 40 files) are in no plan's source list, so no mutation evidence will ever exist for them under the current plan set.

## Reading log

- read C:\Users\lavantien\.claude\plans\full-port-and-nuke-hidden-acorn.md lines 1-224 and 380-465 - the port plan, ruling, and fleet state for this lane
- read tools/bt-mutants-gate/allowlist.tsv all 47 lines - the 42 Rust rows being ported
- read config/allowlist.tsv 1 line - confirmed header only, no live rows
- read tools/mutate/operators.c all 641 lines - op names, comparison swap table, numeric variants, statement rules
- read tools/mutate/mutate.h all 245 lines - ledger row shape, status enum, engine structs
- read tools/mutate/report.c all 152 lines - genre and replacement column semantics, function lookup wiring
- read tools/mutate/token.c lines 1-120 and 352-438 - keyword list and bt_mut_function_at semantics
- read tools/mutants-gate/lib.c lines 330-629 - parse_allowlist and bt_gate_run, key columns, count and stale rules
- read %TEMP%\bt-mut-union-preview.ps1 65 lines - the hub preview's grouping and completeness rule
- read %TEMP%\bt-mut-plans.json plans array - the 80 suite plans and their source closures
- read artifacts/mutants ledger-core_analysis_test.tsv head and status column - ledger format and status values
- grep artifacts/mutants ledger-core_*.tsv for alg.c rows at lines 365 plus and max line per suite - found the stale coordinate split
- ran git log and git diff 2b50aff for src/core/alg.c - the mid-fleet hoist, built the remap from it
- read src/core/alg.c all 364 lines - survivor sites in parse, format, inverse, qtm, htm, char guards
- read src/core/analysis.c lines 80-103, 125-169, 226-302 - corner_twist, sticker_cycles, summarize, guard fns
- read src/core/moves.c lines 186-235, 236-365 - move_layer_perm, init loops, move_table guards, static table order
- read src/core/cube.c lines 33-62 - band_of and try_from_occ validation
- read src/core/expand.c lines 70-194 - parse_count_digits, expand_tokenize, expand_unit boundaries
- read src/scheme/classifier.c lines 108-157, 282-411 - trace budget guards, shape_sig, report_position, defect walks, classification_eq
- read src/scheme/memo.c lines 58-112, 355-409 - in_place sort, corner_succ, walk_run budgets
- read src/scheme/scheme.c lines 50-99 - name_of, face_key, same_piece
- read src/scheme/scramble.c lines 78-152 - random_occ loops and fallthrough
- grep src for corner_twist, layer_perm, build_occ, random_occ and the puzzles, data, ga twin names - C function names for the port table
- read crates/bt-core/src/moves.rs lines 136-205 - const fn table build, no loops in layer_tables
- read crates/bt-core/src/analysis.rs lines 160-178 - corner_twist range loops
- read crates/bt-core/src/expand.rs lines 64-78 - tokenize while bound
- read crates/bt-core/src/cube.rs lines 56-109 - inline band asserts and iterator position_of
- read crates/bt-scheme/src/scheme.rs lines 51-75 - name_of assert, face_key sort_unstable, same_piece
- read crates/bt-scheme/src/scramble.rs lines 154-164 - occ_is_legal iterators
- grep crates/bt-scheme/src/memo.rs residue and should_panic - derive backed equality, walk budget panic tests
- grep crates/bt-scheme/src/classifier.rs derive - all 4 classification types derive PartialEq
- glob crates/bt-core and bt-scheme file lists - test file locations for evidence sources
- grep crates for cfg(test) - inline test module map
