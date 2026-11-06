# toolset probe, lane-macro-restruct generation 2

Probed 2026-11-06 23:28 local, all pass.

- Read: artifacts/progress/lane-macro-restruct.md, artifacts/progress/hub-fanout-2026-11-05.md, src/common/bt_harness.h
- Glob: artifacts/progress/*.md, playground/toolset_probe_macro*.md
- Grep: BT_CHECK and MUT_CHECK census, 14184 hits over 272 files
- PowerShell: Get-Date, git log HEAD 80f2000, lane dir check
- Write: this file
- Edit: progress line in artifacts/progress/lane-macro-restruct.md follows the probe

Generation 1 died to the fifth window halt right after its probe, nothing else landed.
