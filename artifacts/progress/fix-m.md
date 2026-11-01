# fix-m

2026-11-01T18:26 committed d8e4037: mutate now resolves the hub config through bt_mut_config_path (BT_CONFIG honored, relative values joined to the workspace root), selftest went 30 to 32 tests, red was the bridge selftest crashing on C:\Program Files\LLVM\bin\clang.EXE plus the undefined-symbol link, green via test-unit --only mutate_test, mutants-gate-selftest, build, fmt, lint tools and lint mutate, build/env.json refreshed by setup under the bridge because it still recorded the corrupted original
