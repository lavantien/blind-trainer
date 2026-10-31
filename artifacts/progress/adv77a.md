# adv77a: adversarial review of the t77 coverage footprint

Reviewed commits: c8cc863, 5eb4eb3, 037b118, 35ae3b5, b51cb9f, be25313. All probes under playground/adv77a/. A second live coverage run started at 00:14 during the review and I stayed read-only on build/bin-cov and artifacts/coverage.

Findings sent to the hub in the final report. Confirmed: include regex pulls tests/unit/tools into the badge totals against the stated scope, suite-only export omits the 12 main translation units, the flag closure test is blind to a single uninstrumented TU, the zero-percent guard fires after the badge file is clobbered and has no test, the quiet-tree waiter discards the dirty file names and treats a failed git status as clean. Held: merge double counting, isolation of the cov surfaces, dead config keys, badge pin convention, ga_run_lib cap fix.

Probe inventory:

- recompute.py: independent totals from the landed export, duplicate file check, missing file diff against git ls-files
- scope_split.py: totals split by src, tools, tests/unit, tests/fuzz
- mains_gap2.py: static region counts of files only present in tool and app exes, via llvm-cov export -empty-profile
- closure_check.py: static file set of all 190 suite exes vs the landed export, flag closure proof
- redcheck/: sandbox bt.py plus test copy, 5 mutations, one survived (missed TU), 4 caught, baseline restored green
- badgeprobe/: copied badge script trio, zero-region export overwrites the badge with 0.0 percent and exits 0

Reading log:

read bt.py c8cc863 diff full - coverage target surface
read bt.py lines 156-181, 225-254, 498-530, 692-806, 1063-1092, 1538-1663 - run helpers, suite discovery, prune, compile and link stages, coverage target
read tests/python/test_bt_coverage.py full - 17 tests, any() flag assertion
read scripts/coverage_badge.py full - totals source, badge write order
read config/config.json c8cc863 diff - coverage section keys
read artifacts/progress/t77.md full - claims under attack
read artifacts/progress/t77-cov-run3.log grep - flow lines, 190 exes, profraws, objects
read artifacts/progress/t77-cov-run2.log grep - the 4 failing app suites
read tests/unit/tools/ga_run_lib_test.c lines 305-376 - the fixed fixture
read src/common/config.c lines 10-70 - production reader sizes dynamically
read tools/release-verify/main.c lines 1-40, lib.c lines 260-290 - fixed buffer readers
grep fread across repo - cap pattern census
read b51cb9f diff - badge svg and pin refresh
read playground/t77cov/run_when_app_clean.py full - quiet-tree waiter
run python bt.py help, playground/l24/check_readme_table.py - help row and 35-row match
