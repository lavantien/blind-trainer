# lane-cov-mut: mutation machinery coverage

## status

Active. Owned set: tools/mutate (14 files, 1552 dark regions), tools/mutants-gate (3 files, 353), tools/policy (7 files, 269). Lane tree: artifacts/lane/lane-cov-mut (plain + cov configs).

## ESCALATE 1: shared build/env.json poisoned by a lane

2026-11-05 22:50. The repo shared build/env.json now records `"obj_root": "artifacts/lane/lane-cov-verify/build/obj_cov/f7a6b67e7069"`. A lane ran a flow that writes build/env.json (doctor or an equivalent) under its own lane config, and lanes share dirs.env because scripts/lane_build.py redirects obj and bin but not env. Consumers of repo build state break for every lane except the one that wrote it: tools/mutate engine.c obj_root_of trusts the recorded obj_root when the dir exists, so the committed mutate_driver.selftest_passes spawn test fails here with link lists referencing lane-cov-verify objects and argv count drift (evidence in artifacts/progress/lane-cov-mut-suite1.log, failures at tools/mutate/tests.c:563, 610, 636).

Ask: hub fixes scripts/lane_build.py to redirect dirs.env (and raylib env consumers if any) per lane, and restores the shared build/env.json to the shared tree's own obj root. Until then I sequence engine-dependent spawn tests last and work the policy and mutants-gate areas plus the mutate units that need no obj tree.

## log

- 22:40 lane config created, scoped measurer playground/cov_scope_mut.py written (plain and instrumented configs under artifacts/lane/lane-cov-mut).
- 22:43 first scoped run: tools_mutate_test 16 passed 1 failed (selftest_passes, cause above). Full detail captured in lane-cov-mut-suite1.log.
- 22:50 root cause traced to shared build/env.json, escalated above.
- 23:24 relaunched as lane-cov-mut2 after guard disposal for staleness. Hub state: lane_build.py redirects dirs.env per lane since c705b34, shared build/env.json regenerated under the repo hub. Plan: regenerate lane config, rerun the mutate suite to confirm selftest and engine tests recover, then work policy, mutants-gate, and mutate coverage per the inherited plan.

## ESCALATE 2: shared build/env.json poisoned again, recurrence of ESCALATE 1

2026-11-05 23:35. During my selftest confirmation the shared build/env.json again recorded a foreign obj_root, this time artifacts/lane/lane-cov-core/cov-build/obj/f7a6b67e7069. The lane_build.py env redirect does not cover lane-authored measurement clones: cov_scope_*.py scripts build their own instrumented configs (the core lane used a cov-build tree) and some bt.py flow under those configs rewrites the shared build/env.json. Effect on this lane: tools/mutate engine obj_root_of trusts the dir when it exists, the fixture .d is missing there, so dep inputs are skipped and mutate selftest fails dep_inputs_land_under_scratch and stub_assembly_matches_the_bt_py_contract (53 vs 55 argv lines, the -I scratch/inputs pair missing). Local repair done: obj_root restored to the real shared dir build/obj/70dcf3245464 where the fixture .d exists, selftest rechecked below. Ask: pin env.json writes to the repo hub config inside bt.py, or extend the lane measurer recipe to inherit the lane_build clone dirs instead of authoring new ones.
- 23:27 ESCALATE 1 resolved end to end: selftest 32 passed 0 failed direct and the full tools_mutate_test suite green under bt.py (17 passed, 0 failed) with the shared obj_root restored. Coverage work starts with policy.
- 23:44 root-caused the policy fsutil reparse failure with playground/junction_probe.c: the committed make_junction overstated ReparseDataLength by 2 bytes (12+len+2 instead of 12+len) and copied the substitute name at PathBuffer+2, this Windows build rejects both with error 4392 ERROR_INVALID_REPARSE_DATA, and CI runners tolerated the malformed buffer. Probe dump of a native junction pinned the exact header: data_len 156 for a 144 byte name, print offset 146, name at offset 16. Fixed the helper in tests/unit/tools/policy_fsutil_test.c, keeping every assertion.
