# adversarial review 2

## header

| field | value |
| --- | --- |
| wave or scope | full adoption on main, plan adopt-all-the-good-polymorphic-bonbon, bt-data, bt-puzzles, bt-app, bt-verify, tooling, Makefile |
| reviewer 1 | reviewer-1 (independent, not read) |
| reviewer 2 | reviewer-2 (this record) |
| date range | 2026-10-17 to 2026-10-18 |
| final verdict | findings open, reviewer 2 records only, no code edits in scope |

Exclusions honored: the 8 known bt-data seedpack survivors, the commutator survivors, and the verify survivors under fix by agent-a-seedpack, agent-b-commutator, agent-c-verify were not judged. Findings below target landed behavior outside those in-flight areas.

## findings

| id | reviewer | severity | location | finding | resolution |
| --- | --- | --- | --- | --- | --- |
| F1 | 2 | medium | tools/bt-verify/src/ev_skewb.rs:11-27 vs crates/bt-app/src/views/algs/detail.rs:105-121 and crates/bt-app/src/views/case_drill.rs:131-170 | the gate and the app disagree on the skewb grammar. bt-verify strips x y z rotations before tier 1, the app parses the stored alg verbatim under the skewb engine which rejects them. 7 of 15 rows of ref_ground/packs/skewb_sarah.tsv (sarah.04, 05, 11, 12, 13, 14, 15) keep y2 y z2 x' tokens after normalization. Reproduced with the engine: Skewb::parse_alg("y2 L F' L' F") errors UnknownToken("y2"). Consequences: the algs detail pane shows a false tier 1 warning on gate-certified seed rows, pressing d on those rows renders grid_error instead of the case net, and re-saving the prefilled override text fails geometry_check so the user cannot keep the alg they were shown | none |
| F2 | 2 | medium | tools/bt-verify/src/ev_pyra.rs:70-90 and 129-133 | the l4e tier 2 predicate never checks corner facelets and centers_home accepts per-vertex center cycles, so a start state that twists one corner passes whenever 4 to 8 edge facelets moved. Probe over the shipped pack: 9 rows pass tier 2 with 3 corner facelets displaced (l4e.06, 17, 18, 19, 20, 22, 23, 34, 35). 6 of them (edge counts 8, 5, 5, 8, 8, 4) cannot be a U layer framing of a pure case, a U offset always moves exactly 6 edge facelets. The honesty note claims corner facelets are pinned, the code pins nothing | none |
| F3 | 2 | low | tools/bt-verify/src/ev_nxn.rs:38-73 | tier2_cll lacks the solved-state rejection its sibling predicates have (ev_cfop and ev_sq1 fail a solved start state). Probe on Nxn(2): case state of alg x reports is_solved false and d_layer_solved true, so a rotation-only or identity-equivalent row would be certified stage-checked as a CLL case. No current pack row triggers it | none |
| F4 | 2 | low | tools/bt-release-verify/src/lib.rs:98-129, README.md:269 | the plan's N1 negative-config stage never landed. neg_config_exit appears nowhere in the repo, verify() runs zip, extract, p1 smoke, p1 db, p2 smoke, n2 usage only. The packaged exe booting with BT_CONFIG pointing at a missing config is never exercised by the release gate, only the dev-tree screenshots target and the headless bt-app test cover it. README claims the verifier runs the smoke plus negative paths while only the usage-arg negative runs | none |
| F5 | 2 | low | tools/bt-mutants-gate/src/lib.rs:64-152 vs 340-424 | gate() cross-checks the four category counters against parsed outcomes but never cross-checks total_mutants, while merge's check_complete_run enforces total_mutants equality. A doctored or truncated outcomes.json that drops outcomes, adjusts the four counters, and leaves total_mutants inconsistent passes gate() yet fails merge. The direct gate run on a single-run file is the weaker entry point | none |
| F6 | 2 | low | scripts/memguard.ps1:478 | wrap mode runs the command through sh -c on a Windows-native repo whose Makefile recipes were converted to native pwsh. Requires a POSIX sh on PATH, breaks on machines without one, and reintroduces the emulation layer the repo excludes. Reachability is narrow: only the memguard selftest and a playground Makefile call -Wrap, the Makefile itself never does | none |
| F7 | 2 | low | crates/bt-data/src/import.rs:53-63 | the legacy import renames files one by one after the db commit. If a rename fails mid-loop (file lock on Windows) the run errors and boots fatal once, but every later run sees a non-fresh db and skips the import, so the remaining legacy files are never renamed or consumed, silently and forever. Data already sits in the db so no loss occurs, the residue is stale files with no path to resolution | none |

## adjudication

| id | outcome | reasoning |
| --- | --- | --- |
| F1 | confirmed | the engine probe returns UnknownToken("y2") for the stored form of sarah.04 and sarah.14, the app code paths geometry_check and case_facelets contain no rotation stripping, and the pack column holds the tokens on disk |
| F2 | confirmed | the probe runs the exact predicate copy over all 35 pack rows loaded through bt_data::seedpack::load_pack and prints tier2 Ok with corner facelets moved 3 for the 9 listed rows |
| F3 | confirmed | the probe shows d_layer_solved true for the case state of x on Nxn(2) while is_solved is false, and tier2_cll gates only on d_layer_solved across the 24 rotations |
| F4 | confirmed | grep for neg_config over the repo returns zero hits, the verify function lists its stages, and README line 269 was read directly |
| F5 | confirmed | gate reads total counters for missed, timeout, caught, unviable only, check_complete_run additionally compares total_mutants, both functions were read in full |
| F6 | confirmed | line 478 reads `& sh -c $WrapCommand`, the Makefile wires memguard in monitor and selftest mode only, the only -Wrap callers are the selftest itself and playground/mem2wrap/Makefile |
| F7 | confirmed | the rename loop runs after tx.commit, the skip guard is db.is_fresh, and the import test suite pins commit-then-rename ordering |

## verified-clean inventory

- bt-data db.rs migrations, backup-before-migrate, transactional rollback, future-version guard, fail-closed opens: clean
- bt-data import.rs one-time JSON import parse order, atomic commit, fail-closed on corrupt legacy json: clean apart from F7
- bt-data repo_srs, repo_cards, repo_algs, repo_user, repo_settings, repo_solves key scoping, replace semantics, 3bld vs case card separation: clean
- bt-data fsrs.rs numeric paths, initial and review stability positivity, difficulty clamps, interval rounding and clamp saturation, config validation: clean
- bt-data seedpack loader registry rules, strict TSV parse, count drift, front_json enrichment, canonical row hash, prose detection, bracket and group repeat expansion limits, sq1 canonicalization: clean apart from the excluded in-flight survivors
- bt-data repo_seeds insert-or-verify, drift and stale detection feeding the app hide-with-warning path: clean
- bt-puzzles nxn grammar (digit prefixes, wide, lowercase, slices, rotations, both U2' and '2 forms) and geometric layer selection: clean
- bt-puzzles sq1 state-aware slash legality, twist range, wraparound canonicalization, invert correctness, cubeshape ring classification: clean
- bt-puzzles skewb and fto turn tables, bijectivity, order-three structure, adjacency closure: clean
- bt-puzzles mega pinned 16-token grammar, spin and turn algebra, pinned-move guard: clean
- bt-puzzles tier1 generic check and its mock-geometry test matrix: clean
- bt-app case drill state machine, due session selection, card repo read-modify-save against repo_cards, load error surfacing: clean apart from F1 rendering
- bt-app algs browse cascade, override CRUD tier 1 gating, delete reversion, seed vs effective alg resolution: clean apart from F1
- bt-app persist and boot fail-closed behavior, seed pack degrade-not-fatal policy, vendor corpus overlay: clean
- bt-app replay scene model, orbit clamps, zoom bounds, yaw wrap, playback settle and catch-up, config sanitization: clean
- bt-app logic RuntimePaths resolution, nav intents, escape focus handling: clean
- bt-verify packs pin table two-direction drift, per-row rulings with stale detection, prose row exemption: clean
- bt-verify ev_cfop pll oll f2l class predicates with rotation frames and conjugation rewrite, ev_sq1 stage predicates and family keys, ev_bigcube reduction audit and honesty note, ev_mega necessary-condition predicates, ev_skewb and ev_fto honest skips: clean apart from F2 and F3
- tools/bt-mutants-gate outcome parsing, unknown summary rejection, counter agreement, allowlist hygiene, merge end_time and version guards: clean apart from F5
- tools/bt-release-verify zip staging checks, env scrubbing, kill-on-timeout, sandboxed p1 p2 n2, migrated db assertion: clean apart from F4
- tests/e2e driver and bt-app headless e2e, APPDATA sandboxing on every entry point, no real-profile writes: clean
- Makefile visual retry verdicts, e2e and screenshots sandbox lines, chain order: clean
- scripts/memguard.ps1 matching by exe path only, pidfile start-time stamps, tree walks, kill verification: clean apart from F6

## probe note

Two early probe runs aborted with a 32 GiB allocation failure inside load_pack on pyraminx_l4e.tsv. The failure then never reproduced across 36 reruns of the same binaries, and a tracing global allocator over alloc, realloc and alloc_zeroed shows no oversized request on that path. Attributed to transient machine memory pressure from concurrent agent runs during the two failing executions, not reported as a code defect. Probe sources live in playground/r2probe.

## reading log

- read crates/bt-data/src/db.rs full - migrations, backup, fail-closed opens
- read crates/bt-data/src/fsrs.rs full - numeric edge cases, clamps, first-review states
- read crates/bt-data/src/import.rs full - json import rename ordering
- read crates/bt-data/src/repo_srs.rs full - card persistence and memory derivation
- read crates/bt-data/src/srs.rs full - card review state machine, sm2 replay
- read crates/bt-data/src/repo_cards.rs full - case card save replace semantics
- read crates/bt-data/src/repo_user.rs full - override upsert and remove
- read crates/bt-data/src/repo_algs.rs full - legacy key split join, 3bld scoping
- read crates/bt-data/src/repo_seeds.rs full - insert or verify, drift and stale
- read crates/bt-data/src/tsv.rs full - legacy sheet parse
- read crates/bt-data/src/seedpack.rs full - normalizer, rulings, registry, row hash
- read crates/bt-data/src/paths.rs lines 100-180 - user_root APPDATA precedence, packs root
- read crates/bt-puzzles/src/nxn.rs full - grammar and layer geometry
- read crates/bt-puzzles/src/sq1.rs full - slash legality, canonicalization
- read crates/bt-puzzles/src/pyra.rs full - cycles, grammar
- read crates/bt-puzzles/src/skewb.rs full - turn tables, tests
- read crates/bt-puzzles/src/mega.rs full - pinned tokens, turn and spin
- read crates/bt-puzzles/src/fto.rs full - turn tables, structural tests
- read crates/bt-puzzles/src/tier1.rs full - generic tier 1 and mocks
- read crates/bt-puzzles/src/geometry.rs full - seam
- read crates/bt-app/src/views/case_drill.rs full - drill state machine, case grids
- read crates/bt-app/src/views/algs/detail.rs full - override CRUD, tier 1 gate
- read crates/bt-app/src/views/algs/browse.rs full - cascade, hidden events
- read crates/bt-app/src/views/algs/mod.rs full - letters arm, bindings
- read crates/bt-app/src/persist.rs full - stores load, seed degrade policy
- read crates/bt-app/src/logic.rs full - boot, nav, runtime paths
- read crates/bt-app/src/replay_scene.rs full - orbit, playback, config sanitize
- read crates/bt-app/src/views/algs lines 1-40 via browse.rs includes - none, direct reads listed above
- read crates/bt-app/tests/e2e.rs full - headless sandbox check
- read tests/e2e/src/main.rs full - smoke driver, shot assertions
- read tools/bt-verify/src/packs.rs full - event routing, pin table
- read tools/bt-verify/src/packs_report.rs full - offender and stale ruling logic
- read tools/bt-verify/src/ev_cfop.rs full - pll oll f2l predicates
- read tools/bt-verify/src/ev_nxn.rs full - cll predicate
- read tools/bt-verify/src/ev_sq1.rs full - cubeshape and stage predicates
- read tools/bt-verify/src/ev_bigcube.rs full - reduction audit
- read tools/bt-verify/src/ev_pyra.rs full - l4e predicate
- read tools/bt-verify/src/ev_skewb.rs full - rotation stripping
- read tools/bt-verify/src/ev_mega.rs full - 4lll predicates
- read tools/bt-verify/src/ev_fto.rs full - honest skip
- read tools/bt-verify/src/lib.rs full - simcheck table, crate layout
- read tools/bt-mutants-gate/src/lib.rs full - gate and merge
- read tools/bt-release-verify/src/lib.rs full - stages, config keys
- read tools/bt-release-verify/src/exec.rs full - env scrub, kill tree
- read scripts/memguard.ps1 full - matcher, monitor, kill, selftest, wrap
- read Makefile full - targets, chain, sandboxes, visual retry
- read config/config.json full - hub sections
- read ref_ground/packs/registry.json full - pack entries
- read ref_ground/packs/skewb_sarah.tsv full - rotation tokens on disk
- read ref_ground/packs/pyraminx_l4e.tsv via Import-Csv - alg and front_json columns
- grep ref_ground/packs/*.tsv rotation tokens - cross-event rotation inventory
- grep repo neg_config - absence check for F4
- grep repo -Wrap, WrapCommand, memguard.ps1 - reachability for F6
- grep README.md release-verify, neg_config, negative, smoke - claims for F4
- grep bt-app repo_srs::save, repo_cards, import::run - save flow callers
- wrote and ran playground/r2probe (r2probe, only_load, trace_alloc, cll_probe) - engine and loader probes backing F1, F2, F3 and the probe note
