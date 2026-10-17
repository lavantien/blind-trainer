# adversarial review record

## header

| field | value |
| --- | --- |
| wave or scope | full adoption per ~/.claude/plans/adopt-all-the-good-polymorphic-bonbon.md as landed on main through 2e467d1: tooling (bt-policy, bt-mutants-gate, bt-release-verify, bt-jobwrap, memguard, clean-rust), bt-puzzles engines, bt-data seedpack and repos and db and fsrs, bt-verify per-event packs report, bt-app algs split, case drill, replay scene, Makefile wiring |
| reviewer 1 | adversarial-1 (independent, no other reviewer record or agent progress file read) |
| reviewer 2 | independent second reviewer, not visible to this record |
| date range | 2026-10-17 to 2026-10-18 |
| final verdict | findings reported, fixes pending |

## findings

| id | reviewer | severity | location | finding | resolution |
| --- | --- | --- | --- | --- | --- |
| F1 | 1 | major | Makefile:137-139, Makefile:170-172, Makefile:128-129 | the chain runs `e2e baselines visual` in that order: the `baselines` recipe copies `artifacts/e2e/*.png` over the committed `tests/visual/baselines/` immediately before `visual` diffs `artifacts/e2e` against them. Inside `make chain` the visual gate compares the same run's renders with its own copies, so any rendering regression since the last committed baseline passes the chain, and every chain run with changed rendering silently rewrites committed baseline files (dirty tree, review-by-git-status is the only remaining catch). the new flake-vs-regression verdicts in the `visual` recipe can never fire inside the chain. the order predates the adoption (e5221f9 chain had baselines before visual) but the plan re-specified the same order and the stage 1 wiring kept it while rebuilding the recipe around it | none, fix pending |
| F2 | 1 | minor | crates/bt-data/src/seedpack.rs:626-636, tools/bt-verify/src/packs_report.rs:40-60 | an `expected_failure` ruling is keyed only by event/method/stage/case_key and excuses whatever failure text the row produces. an engine regression that changes how a ruled row fails (for example a parse failure replacing the recorded unexecutable-slash failure on cs.119, or a tier 2 shape flip on cs.075) stays invisible to verify-algs: the stale-ruling tripwire fires only when the row passes, never when the failure mode drifts. sub-case: a ruling attached to a prose row can never surface as stale because `row_offender` returns None for prose before consulting the ruling table (packs_report.rs:41-43) | none, fix pending |
| F3 | 1 | minor | tools/bt-mutants-gate/src/lib.rs:154-159 vs lib.rs:198-199 | `parse_key` accepts a module-level mutant (function null) and builds a GateKey with an empty function, but `parse_allowlist` rejects any row with an empty function column. a surviving or timing-out module-level mutant produces a key that no allowlist row can ever match, so the gate deadlocks with an unexcusable survivor whose only escapes are a code change or a dishonest function name (which then mismatches the key) | none, fix pending |
| F4 | 1 | minor | crates/bt-data/src/seedpack.rs (whole file), README.md:287 | seedpack.rs holds 1242 non-blank lines with no inline test module, over the repo's own max 1000 SLOC per file rule stated in the README development section. repo_seeds.rs was split off the seed surface but seedpack.rs itself stayed above the cap, and no tool (bt-policy covers comments and constants only) enforces the SLOC rule anywhere | none, fix pending |
| F5 | 1 | minor | scripts/memguard.ps1:478 | the memguard wrap mode executes its payload through `& sh -c $WrapCommand`, routing a process launch through the git-for-windows sh wrapper, against the repo rule to use native toolchains and never route through sh wrappers. production launchers were moved to bt-jobwrap (Makefile:65-172), so the sh route survives only in the wrap code path that memguard-selftest itself exercises (memguard.ps1:353-354), and a machine without sh on PATH fails memguard-selftest | none, fix pending |
| F6 | 1 | minor | tools/bt-jobwrap/src/lib.rs:87-104 | `quote_argument` wraps only whitespace-containing arguments in double quotes and never escapes backslashes or embedded quotes, then the payload is spawned through CreateProcessW with CommandLineToArgvW semantics. an argument containing both whitespace and a trailing backslash (for example `C:\some dir\`) becomes `"C:\some dir\"` where the trailing backslash escapes the closing quote, splicing the next argument in; an embedded bare quote breaks the same way. current make payloads pass space-free paths so the corruption is latent, but the wrapper accepts arbitrary commands | none, fix pending |
| F7 | 1 | minor | crates/bt-app/src/views/algs/detail.rs:105-121 | the override tier 1 gate routes events 3x3, oh, and 3bld to `nxn_check(3)`, the bt-puzzles token grammar, while the 3BLD arm, its corpus, and bt-core accept commutator brackets, group repeats, and comma separators. a user pasting a 3style alg in `[A, B]` or `(R U R')2` notation at the override prompt is rejected with unknown token even though the same notation is valid everywhere else in the app. also detail.rs:109 defaults bigcube to n=7 when no size node sits in the browser path, verifying the override at the wrong cube size instead of failing | none, fix pending |
| F8 | 1 | minor | Makefile:129 | in the visual retry, when the re-render itself fails (`$r1 -ne 0`, for example an e2e-smoke build failure) the recipe writes "verdict: regression, both runs mismatched" and exits 2 even though the second diff never ran, mislabeling an infra failure as a rendering regression | none, fix pending |

## adjudication

| id | outcome | reasoning |
| --- | --- | --- |
| F1 | confirmed | recipes verified directly: baselines is an unconditional Copy-Item from artifacts/e2e, visual consumes artifacts/e2e against tests/visual/baselines, chain orders e2e then baselines then visual. inside the chain the comparator input and the baseline are byte-copies of the same render. the pre-adoption chain held the same order, so this is a carried-forward defect that the adoption re-encoded rather than fixed |
| F2 | confirmed | row_offender matches (true, Some) to None regardless of failure text, and the summary counts a ruled failure as failed plus ruled without recording the reason. a ruled row cannot go stale unless it passes. the prose early-return precedes the ruling lookup |
| F3 | confirmed | parse_allowlist errors on rows whose function column is empty, and parse_key maps a null function to the empty string, so the key space of survivors includes a value the allowlist format cannot express |
| F4 | confirmed | measured 1279 total lines, 1242 non-blank, zero cfg(test) markers in the file, tests live in crates/bt-data/tests/seedpack.rs. the README states the max 1000 SLOC rule as a development discipline and no gate checks it |
| F5 | confirmed | line 478 is `& sh -c $WrapCommand`. the Makefile wires memguard only in monitor and selftest mode and anchors launches on bt-jobwrap, so the sh route is reachable only through -Wrap, which only the selftest drives |
| F6 | confirmed | the quoting function has no backslash or quote escaping, and the ffi passes the joined line straight to CreateProcessW. the failure needs whitespace plus a trailing backslash or an embedded quote in one payload argument, which no current make target produces |
| F7 | confirmed | geometry_check maps 3x3/oh/3bld to Nxn(3) whose parse_token only accepts face, wide, digit-prefixed, slice, and rotation tokens with 2 and prime suffixes, so brackets, group repeats, and commas fail. the bigcube arm clamps a missing size to 7 |
| F8 | confirmed | the else branch of the retry covers both the re-render failure and the second-diff failure with one message and exit code, and the second diff is skipped entirely when r1 is nonzero |

## verified-clean inventory

- bt-puzzles sq1 engine: twist and slash semantics, canonical tuple and wraparound inversion, state-aware slash legality match the physical model where the slash needs the aligned seam and layer twists are free. the unenforced twist precondition is not a defect: on the real puzzle the closed middle band lets layers rotate freely, only the split needs seam alignment, and consecutive twists merge algebraically so no legal sequence is excluded
- bt-puzzles nxn engine: layer window selection, per-face turn sign table checked against all six faces, wide and digit-prefix depths, M E S following L D F, x y z rotations, invert algebra, format round trips; structural oracles and the n=3 bt-core agreement corpus pin it
- bt-puzzles pyraminx engine: band and tip cycles pinned by derived facelet tables, band scope tests, tip-only tests, order-three and counter-clock properties, 35-row pack tier 1 with a token inventory restricted to the pack grammar
- bt-puzzles skewb engine: geometric vertex rotations, quarter-2 as the square of quarter-1, bijective turn tables, order-three sweep, grammar restricted to F L R B with prime
- bt-puzzles fto engine: all 8 faces including BR, bijectivity and cycle structure pinned at 51 fixed plus 7 three-cycles, order three, opposite-face commutativity, 2 and 2' suffix algebra on a 120-degree turn
- bt-puzzles megaminx engine: adjacency pinned against a golden icosahedron, rotation-system validity over 20 corner orbits, independent ring derivation, y-perm structure, quarter-turn strip anatomy
- tier 1 verifier: both round-trip directions, apply legality surfaced, mock-geometry tests for identity inversion, sign drift, non-bijective landing, and forward-legal-reverse-illegal sequences
- bt-verify packs registry: bidirectional registry versus pin reconciliation with row-count drift, unpinned entries, and missing pins all offending
- bt-verify per-event predicates: sq1 stage ordering and cubeshape family classification with majority-vote keys and ambiguity rejection, cfop pll and oll discrimination is mutually exclusive over rotation frames, f2l slot displacement with regrasp rewriting conjugated correctly, bigcube reduction audit with declared facelet-seam honesty, 2x2 bottom-layer predicate, pyraminx centers-pinned edge disturbance, megaminx center-frame stage predicates, fto and skewb honestly declared tier 1 with skips recorded
- bt-verify packs report and main: fail-closed on load failures, unruled failures, stale rulings, and drift; offenders drive exit 1; report artifacts written before the verdict
- bt-data seedpack parser: strict header, five-column rows, field emptiness, duplicate keys, source_at shape, count drift against the registry, enriched notes collision detection, content hash over the normalized row
- bt-data seedpack normalizer: chain notes, bracket and group repeat expansion with limits and balanced checks, grouping paren stripping, prose detection, X to x mapping recorded, sq1 wraparound, auf prefix stripping, equator marker stripping, all recorded as notes that enter the hashed content
- bt-data repo_seeds: insert-or-verify with hash equality, drift and stale key errors inside one transaction
- bt-data db: embedded migrations pinned byte-equal to committed files, per-migration transactions carrying user_version, backup before the first applied migration on a populated file, rollback on failure, future-version and corrupt-file fail-closed, fresh detection immune to count cancellation
- bt-data import: db commit precedes the legacy renames so no window loses data, corrupt json fails closed naming the file
- bt-data fsrs: 19-weight FSRS-5 formulas match the pinned py-fsrs 5.1.3 reference structure, initial stability floor, difficulty linear damping and mean reversion to D0(4), short-term and recall and forget stability with the lapse ceiling, elapsed floored to whole days, interval formula with ties-even rounding and hub clamps, config validation rejects non-finite weights and out-of-range retention and clamps
- bt-mutants-gate gate: baseline must succeed, unknown summaries and scenario shapes rejected, counter reconciliation on all four outcomes, empty runs refused, count-aware allowlist matching in both directions with duplicate and shape checks, merge refuses killed or partial runs, version disagreement, and counter lies
- bt-release-verify zipcheck: normalization of backslash names and root prefixes, duplicate detection, set equality both directions, byte comparison per entry, extraction rejects parent escapes; the absolute-path extraction gap is unreachable because check_zip set equality runs before any extraction
- bt-release-verify stages: P1 scrubbed env with fresh APPDATA plus shot set and dims from the hub, packaged db asserted at latest user_version by header read, P2 with BT_CONFIG into the extracted tree, N2 usage exit, child runner kills the tree on timeout and verifies it gone
- bt-jobwrap: job object with process-memory ceiling and kill-on-close, suspended spawn then assign then resume, struct layouts pinned by const asserts, limit flags correct, pidfiles stamped with process start ticks and removed by a drop guard
- memguard matcher: executable-path prefix matching only, decoy command lines ignored, pidfile trees counted with start-time stamping, stale and malformed pidfiles ignored, dedupe across both sources, kill verified gone
- bt-policy constants scanner: radix and float and exponent and suffix aware literal extraction with boundary and range handling (light check, deep lexer prose not re-derived)
- Makefile: native pwsh recipes, jobwrap anchoring on every cargo launcher, fuzz leg env handling, usage-gated parameterized targets, clean rungs separated between report and apply
- bt-app override CRUD: tier 1 gate at input with per-event geometry dispatch, delete reverts to seed, errors surfaced in the draft
- bt-app case drill: FSRS grading through the card repo with due-session ordering by document order and fallback when nothing is due
- bt-app replay scene: config sanitation clamps zoom and pitch with inverted-range swap
- clean-rust: orphan scratch selection compares directory creation time against the oldest live cargo-mutants start with slack
- comments and stray literals: zero comment lines across all adoption rust sources and both new scripts, allowlist discipline visible in 45 justified rows

## reading log

- read C:\Users\lavantien\.claude\plans\adopt-all-the-good-polymorphic-bonbon.md full - review scope and pinned intent
- read C:\Users\lavantien\dev\github\blind-trainer\docs\reviews\TEMPLATE.md full - record format
- read git log and status via git -C - adoption boundary, in-flight commutator.rs exclusion
- read crates/bt-puzzles/src/sq1.rs full - slash and twist legality, canonical shape, inversion
- read crates/bt-puzzles/tests/sq1_oracle.rs lines 1-120 - solved layout pins, slash expectations
- grep ref_ground/packs/PROVENANCE.md sq1 and engine - ruling context for the 18 ruled sq1 rows
- read crates/bt-data/src/seedpack.rs lines 560-890, 890-1279, grep pub fn - registry, pack parse, hash, normalizer both arms
- read crates/bt-puzzles/src/nxn.rs full - geometry, notation, slice and rotation semantics
- read crates/bt-puzzles/src/pyra.rs full - cycles, tip model, grammar
- read crates/bt-puzzles/tests/pyra_oracle.rs full - derived tables and pack tier 1 pins
- read crates/bt-puzzles/src/skewb.rs full - vertex tables and tests
- read crates/bt-puzzles/src/fto.rs full - turn table construction and structural tests
- read crates/bt-puzzles/src/mega.rs full - adjacency, turn, spin, pinned grammar
- grep crates/bt-puzzles/tests/mega_oracle.rs fn and assert - oracle coverage inventory
- read crates/bt-puzzles/src/tier1.rs full - tier 1 semantics and adversarial mock geometry
- read crates/bt-puzzles/src/geometry.rs full - trait seam
- read tools/bt-verify/src/packs.rs full - event dispatch and registry pins
- read tools/bt-verify/src/ev_sq1.rs full - stage and cubeshape predicates
- read tools/bt-verify/src/ev_cfop.rs full - regrasp rewrite, pll, oll, f2l predicates
- read tools/bt-verify/src/ev_bigcube.rs full - reduction audit and size resolution
- read tools/bt-verify/src/ev_nxn.rs full - cll bottom-layer predicate and 24 rotation words
- read tools/bt-verify/src/ev_fto.rs full - tier 1 honesty
- read tools/bt-verify/src/ev_skewb.rs full - rotation stripping and honesty
- read tools/bt-verify/src/ev_pyra.rs lines 1-160 - centers and edge disturbance
- read tools/bt-verify/src/ev_mega.rs lines 1-150 - center-frame stage predicates
- read tools/bt-verify/src/packs_report.rs full - fail-closed reconciliation and markdown
- read tools/bt-verify/src/main.rs and lib.rs full - exit wiring and sheet pins
- grep tools/bt-verify/src/ev_bld.rs PROSE and Skipped - prose and letter-scheme skip semantics
- read crates/bt-data/src/repo_seeds.rs full - insert-or-verify and stale detection
- read crates/bt-data/src/db.rs full - migrations, backup, rollback, tests
- read crates/bt-data/src/fsrs.rs full - FSRS-5 math against the pinned reference
- read tools/bt-mutants-gate/src/lib.rs lines 1-500 plus fixture constants - gate and merge
- read tools/bt-mutants-gate/src/main.rs full - merge dispatch at index 1
- read tools/bt-mutants-gate/allowlist.tsv head plus row scan - row shape, 45 rows, no empty function rows
- read tools/bt-jobwrap/src/lib.rs full - invocation, quoting, pidfiles, config
- read tools/bt-jobwrap/src/ffi.rs full - job object, spawn, resume, wait, tick stamping
- read scripts/memguard.ps1 full - matcher, monitor, kill, selftest, wrap mode
- read Makefile full - targets, chain order, jobwrap anchoring
- read git show e5221f9:Makefile chain and baselines - pre-adoption order comparison
- read tools/bt-release-verify/src/zipcheck.rs full - zip verification and tests
- read tools/bt-release-verify/src/lib.rs lines 1-530 - stages and config contract
- read tools/bt-release-verify/src/exec.rs full - child runner, scrub, timeout kill
- read tools/bt-policy/src/constants.rs lines 1-120 - literal scanner mechanics
- read crates/bt-app/src/views/algs/detail.rs lines 60-260 and grep tier and override - override gate
- grep crates/bt-app/src/views/case_drill.rs grade and card - fsrs grading path
- grep crates/bt-app/src/replay_scene.rs clamp and fn - clamp sanitation
- grep scripts/clean-rust.ps1 orphan and CreationTime - orphan scratch rule
- grep crates/bt-data/src/import.rs migrated and commit - migration ordering
- read README.md lines 280-293 - hub claim and development rules
- scan SLOC and comment lines across crates and tools and scripts - cap and comment findings
