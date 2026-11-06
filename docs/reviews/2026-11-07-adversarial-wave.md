# adversarial review record

## header

| field | value |
| --- | --- |
| wave or scope | coverage wave 2026-11-05 to 2026-11-07, 332 commits through e6fce5e |
| reviewer 1 | adv-wave-r1, independent read-only pass |
| reviewer 2 | adv-wave-r2, independent read-only pass |
| date range | 2026-11-07 to 2026-11-07 |
| final verdict | findings fixed, commit range pending the fix lane |

The two reviewers worked independently over the same commit range, one attacking the restructures, macro semantics, and tooling changes, the other the fail-closed conversions, allocation band pins, the data chain, and CI dependencies. Neither saw the other's findings before adjudication.

## findings

| id | reviewer | severity | location | finding | resolution |
| --- | --- | --- | --- | --- | --- |
| A1 | 1 | medium | src/data/db.c:264 | the removed sql==NULL check in migrate leaves a live NULL source, bt_db_migration_sql returns NULL on file-branch malloc failure, sqlite3_exec maps NULL to empty sql, so the migration silently runs nothing while user_version still bumps | fix: migration_sql falls back to the embedded text on file-branch allocation failure |
| A2 | 1 | medium | bt.py:1720 | the per-suite timeout overrides are wired into run_suite_exes only, tgt_coverage and its serial retry still pass the flat 300s for the same heavy suites under instrumentation | fix: pass the suite_timeout_of resolver into the coverage run |
| A3 | 1 | low | src/data/commutator.c:129 | a lone bracketed slot like [[[A,B]],C] parses to a SUM unit whose expansion is now empty, the slot drops out silently and decompose falls back | fix: reject the empty expansion at the unit level instead of dropping the slot |
| A4 | 1 | low | bt.py:909 | the relink prefix filter matches long filters but broke short ones, --only release no longer relinks release-verify, stale tool exe class | fix: bidirectional prefix test |
| A5 | 1 | low | tools/gendata/build.c:260 | the mirrored-inverse derivation attaches and marks covered regardless of its own verify verdict | fix: attach only on an ok verdict |
| A6 | 1 | low | tools/mutate/tests.c:18 | MUT_CHECK message args now evaluate eagerly on every call in unspecified order, a silent macro contract change, all 20 current sites verified safe | none: constraint recorded here, future sites must sequence writes before the call |
| A7 | 1 | low | tests/unit/tools/spawn_util.h:247 | deterministic spawn failures retry 3 times with 2s sleeps and the retry annotation is overwritten by the next attempt's error | fix: retry only transient error classes, keep the first diag |
| A8 | 1 | low | src/data/db.c:266 | BEGIN and PRAGMA user_version results are ignored, a pragma failure after DDL would commit without the version bump | fix: restore the pragma result check |
| A9 | 1 | info | bt.py:1603 | response file args are quoted without escaping embedded quotes, no repo arg can contain one today | fix: escape embedded quotes anyway |
| A10 | 1 | info | tools/verify/ev_cfop.c:285 | error message wording changed for exotic tokens, behavior identical | none |
| A11 | 1 | info | tools/gendata/build.c:243 | derived rows carry plain alg text in the commutator field, consumers degrade to plain display | none |
| A12 | 1 | info | tests/unit/tools/spawn_util.h:62 | bt_spawn_command writes parts[32] without a bound check | fix: bound the loop |
| R2-1 | 2 | medium | src/data/tsv.c:250 | the two allocation arms return -1 with the error kind unset, digest and corpus render an uninitialized struct through %s | fix: BT_TSV_ERR_OOM on both arms, assert the kind in the oom sweep |
| R2-2 | 2 | medium | tools/verify/report.c:178 | per-row bt_malloc results are written unchecked, the seam crashes instead of exiting 1 | fix: check every per-row allocation, take the exit 1 path |
| R2-3 | 2 | medium | tools/verify/report.c:346 | the json serializers ignore bt_json_object_set failures, a starved writer emits truncated-but-valid reports and exits 0, gendata serializers the same | fix: thread a failure flag through the to_json builders, fail the tools when a member is dropped |
| R2-4 | 2 | medium | tools/gendata/audit.c:32 | the audit gate drops offenders under allocation failure and passes, pinned as a ruling by audit_drops_offenders_when_allocation_fails | fix: flip the ruling, the gate fails closed, the pin updates |
| R2-5 | 2 | medium | .github/workflows/ci.yml:51 | CI never runs the app unit suites | none: refuted, the readme documents the deliberate exclusion, the app area needs an interactive desktop the runner lacks, focus verdicts stay two-sided |
| R2-6 | 2 | low | tools/gendata/main.c:130 | the regen skip trusts file existence, never the recorded sha256 | fix: recompute both hashes on the skip path, regenerate on mismatch |
| R2-7 | 2 | low | src/data/seedpack/hashing.c:13 | the empty marker hash self-verifies on later imports, disarming drift detection | fix: reject empty hashes at insert |
| R2-8 | 2 | low | tools/release-verify/exec.c:162 | unquoted args with trailing backslashes get them doubled, the child receives double slashes | fix: double trailing backslashes only inside quotes |
| R2-9 | 2 | low | tools/verify/report.c:52 | a row failing only through allocation_failed renders an empty reason | fix: push an out-of-memory problem line |
| R2-10 | 2 | low | src/app/views/algs/mod.c:229 | the badge prints 0 problem(s) for an allocation-failed verification | fix: print the out-of-memory text |
| R2-11 | 2 | info | tests/unit/tools band pins | hardcoded band pins fail loudly on drift but need recalibration, the data oom sweep lacks a fail-count assertion | fix: add the bt_alloc_fail_count assertion |

## adjudication

| id | outcome | reasoning |
| --- | --- | --- |
| A1 | confirmed | the sqlite3_exec NULL mapping is verified in the vendored source at 142852, the proof that removed the check covered only the file-miss path, not the malloc failure |
| A2 | confirmed | the coverage target demonstrably runs the same suites slower under instrumentation, the playground measurers carry timeout workarounds |
| A3 | confirmed | the input parses today and the slot loss is silent, the removed guard existed for this shape |
| A4 | confirmed | the prefix direction excludes the short filter form, the stale exe class is the one the fix targeted |
| A5 | confirmed | the verdict is recorded but unused for gating, the audit runs before derivation so nothing downstream re-checks |
| A6 | refuted as a defect, recorded as a constraint | all 20 sites verified safe, the contract note lives in this record |
| A7 | confirmed | the retry cost lands on every deliberate refusal arm, the annotation is overwritten in practice |
| A8 | confirmed as hardening | the failure needs a pragma error after successful DDL, unlikely but cheap to guard |
| A9 | confirmed as hardening | cheap escape, no current input triggers it |
| A10 | refuted | behavior identical, no pin asserts the old string |
| A11 | refuted | consumers degrade to plain display by design, the shipped row verifies ok |
| A12 | confirmed as hardening | a bound check is one line |
| R2-1 | confirmed | the same unset-error class the wave fixed twice elsewhere, the digest path renders the raw struct |
| R2-2 | confirmed | crash instead of controlled exit under the seam the wave itself introduced |
| R2-3 | confirmed | the report layer swallows what the verdict layer correctly propagates, the gendata variant can poison the manifest sha |
| R2-4 | confirmed, ruling flipped | the fail-open ruling predates the fail-closed sweep, the gate is the last line of defense for derived data |
| R2-5 | refuted | the readme documents the deliberate exclusion, the app suites need foreground interaction the runner cannot provide, the focus tests carry two-sided verdicts for exactly this |
| R2-6 | confirmed | existence without hash verification lets a truncated artifact skip regeneration forever |
| R2-7 | confirmed | the marker defeats the drift detector it was meant to protect |
| R2-8 | confirmed | the doubling rule only applies inside quotes per the cmd parsing contract |
| R2-9 | confirmed | the failure reason must reach the report |
| R2-10 | confirmed | the badge text misleads on the OOM path |
| R2-11 | confirmed as hardening | the assertion makes band staleness loud |

## verified-clean inventory

- branchless BT_CHECK and BT_CHECK_F64_NEAR semantics: single evaluation, ordered temporaries, identical messages and NaN handling
- net.c face letter table: exact mapping reproduction, unknown stays -1
- mirrored-inverse shipped data: 818 of 818 covered, the derived row is the exact verified inverse
- spawn absolute resolve: bare names stay bare, relative paths resolve on the CreateProcessW base
- per-suite timeout plumbing, response files, ledger parent dir, policy env save, badge pin derivation, CI memofreq chain
- dead-arm removals with live-path analysis across release-verify, verify engines, ga, db, normalizer, report
- strbuf zero-cap guard, watch title bound, ga-run ownership transfer
- allocation_failed propagation through every verdict consumer, corpus load error arms, regen chain write order

## fix routing

The confirmed findings route to one fix lane in three batches: the fail-closed completions (A1, R2-1, R2-2, R2-3, R2-4, R2-9, R2-10, A8), the tooling and guard repairs (A2, A4, A7, A9, A12, R2-8, A3, A5, R2-6, R2-7, R2-11), and the record updates. Verification waits for the running mutants fleet to release the build tree.
