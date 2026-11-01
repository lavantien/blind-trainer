# ci flake fix round, 2026-11-01

Two python suite failures on the runner, root caused and fixed after the v2.0 push.

- 21:00 run 41313121070 on df3a303: test_bt_coverage test_link_stage_coverage_uses_bin_cov_and_its_own_stamp failed, 0 link lines found while the exe existed. Rerun green.
- 21:46 run 41316972884 on 3577242: the same test failed again, plus test_ci_badge test_committed_ci_svg_carries_the_pinned_verdict failed because the committed badge said CI: failed while the test pinned a hardcoded passed verdict.

Root causes:

1. the coverage and cuda fixtures logged every stub invocation by appending one repr line to a shared argv.log from parallel processes. On Windows both open(a) and os.O_APPEND lose concurrent writes, because the CRT seek-then-write is not atomic between processes. Probe playground/append_race_probe measured 567 to 674 lost lines of 4000 in text mode and 401 to 453 through O_APPEND, 8 writers x 500 lines, twice each. The runner's 2 vCPU burst collides the stub writes and whichever line dies takes the assertion with it. Fix: each stub invocation writes its own file named by pid and nanosecond, argv_lines reads them all, no shared append and no pid-reuse clobber.
2. the badge test asserted the committed ci.svg equals the rendering of a constant passed verdict, so the first honest failure (badge job commits the failed badge) made every later run fail the pin forever. Fix: the test now asserts the committed svg is the byte-exact badge_svg product of exactly one verdict in VERDICT_COLORS, tolerant of any honest verdict, still rejecting hand edits.

Verification: python -m unittest discover -s tests/python, Ran 276 tests, OK, with the failed-verdict badge on main, exit 0. CI run on the fix head gates the release.
