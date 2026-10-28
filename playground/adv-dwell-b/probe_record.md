# adv-dwell-b probe record over lane-capture-dwell (df82fee, 8d32900)

## golden three way compare

pre-regen blob 8d32900^:ref_ground/ga/report.json sha256 38a16771f776367c3cadbda2520ef421da455c0373a5d6a0980ac6ce6ddcac62
playground/l45/report_pre_change.json sha256 4b0b22fc3534bd3c5c0ca3540e4cab63b43708cb5af09dc5df312f3efb7b5f46
committed golden ref_ground/ga/report.json sha256 ea679342a0a6cb0f0e3a2e6c3fc6423425596919d149d02c66947e71de5f01bc
diff pre-regen to snapshot: exactly build.e2e_exe added, build.test_timeout_secs 120 to 300
diff snapshot to committed: exactly the removed capture_dwell_frames line
diff pre-regen to committed: exactly the three explained hunks, nothing else

## independent regen, two consecutive passes

env replica of bt.py child_env, cwd repo root: BT_REF_GROUND=ref_ground, BT_CONFIG=<repo>/config/config.json, BT_ARTIFACTS=artifacts, argv empty
build/bin/memofreq.exe then build/bin/ga-run.exe, pass 1 and pass 2, both exit 0
freq.json 11609bd494b8a4a9487ecdfda4237e5b7552500c0e9b77aa918a21ce04b657db identical to golden both passes
syllables.json 80682880a1c6d39cf8f30d38627223ea1493e7296e45ce069e45d93db29372f3 identical both passes
report.json ea679342a0a6cb0f0e3a2e6c3fc6423425596919d149d02c66947e71de5f01bc identical both passes
artifacts/ga/report.md 61b403bbb325ff33b430fe6a06319018bc02353fed30cd8f907cf56992d4ffdd identical both passes

## run_config drift

config.json flattened 153 keys vs golden run_config flattened 153 keys: zero absent either side, zero value mismatches
git diff 284f729..df82fee on config/config.json: exactly capture_dwell_frames removed (df82fee), test_timeout_secs 120 to 300 (2898270), e2e_exe added (89233d7)

## findings

stale pin: ref_ground/ga/PROVENANCE.md still lists report.json sha256 38a16771..., the pre-regen blob. the regen moved the file to ea679342... without touching PROVENANCE.md, the other three pins still hold

## suites

python bt.py test-unit app: 43 passed 0 failed
