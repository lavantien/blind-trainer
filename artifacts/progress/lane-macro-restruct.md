# lane-macro-restruct progress

Mission: BT_CHECK and MUT_CHECK become branchless calls into bt_harness.c so the assert failure arm exists once, in one place, and is coverable. This is the last coverage lane because it touches every test file.

Scope: src/common/bt_harness.h and bt_harness.c, every macro use site under tests/ and tools/*/tests*.c including tools/mutants-gate. The macros currently expand to `if (!(cond)) { bt_harness_fail(...) }` at every site, so every site carries an uncoverable failure arm. The target shape: the macro expands to a plain call like `bt_check(cond, "file", line, "expr", fmt, ...)` or the area-appropriate wrapper, the branch lives once inside bt_harness.c, and one committed test drives that single failure arm.

Rules inherited from the wave:
- First progress line within 5 minutes of launch.
- Red states only inside lane-local build copies, never the shared tree.
- Poll outputs on a bounded cadence inside your turn, never wait idle on a notification.
- python bt.py lint over every touched area before each delivery commit, fmt gate rides every commit through scripts/commit_lane.py with explicit paths.
- Suites through bt.py test-unit <area> --only <stem> under the lane config (scripts/lane_build.py clone at artifacts/lane/lane-macro-restruct), never repo-wide targets.
- Keep every file at or under 1000 SLOC, zero comments.
- Measurement: scoped measurer in playground measuring the dark-region delta over the touched test files plus bt_harness.c, before and after. The expectation: the assertion-arm mass (hundreds of zero-count regions across every test file, roughly 40 percent of the tools area's residue) collapses to the single covered arm in bt_harness.c.
- Do not change any test's semantics: same messages, same evaluation order, same pass and fail behavior. The suite count and every suite verdict must be identical before and after.

Current state: launched 2026-11-06 23:05 local, nothing landed yet.

- 23:33 gen2 relaunched after the fifth window halt. Toolset probe passed (playground/toolset_probe_macro2.md). Census: BT_CHECK 7585 use sites over 249 files, BT_CHECK_F64_NEAR 93 over 14 files, BT_CHECK_F64_BITS 0 uses, MUT_CHECK 24 all in tools/mutate/tests.c, BT_CHECK_EQ and BT_ABORT already branchless calls. Use sites stay byte-identical, the edit is confined to bt_harness.h, bt_harness.c, tools/mutate/tests.c, plus one new failure-arm test in tests/unit/common/bt_harness_test.c (which already drives failing checks through a nested bt_harness_run).
