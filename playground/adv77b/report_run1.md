# workspace coverage report

generated: 2026-10-31T17:18:25Z
regions: 45427 of 61348 (74.0%)

| area | regions | percent |
| --- | --- | --- |
| mutants-gate | 0/1472 | 0.0% |
| mutate | 751/5011 | 15.0% |
| jobwrap | 309/669 | 46.2% |
| ga-run | 820/1522 | 53.9% |
| visual-diff | 862/1148 | 75.1% |
| release-verify | 1478/1817 | 81.3% |
| data | 4719/5767 | 81.8% |
| verify | 2698/3272 | 82.5% |
| app | 5739/6873 | 83.5% |
| memofreq | 394/470 | 83.8% |
| gendata | 1866/2207 | 84.5% |
| policy | 1531/1780 | 86.0% |
| common | 1473/1695 | 86.9% |
| scheme | 1087/1220 | 89.1% |
| core | 951/1035 | 91.9% |
| ga | 2030/2132 | 95.2% |
| puzzles | 1622/1697 | 95.6% |
| sleeper | 122/127 | 96.1% |

worst 25 files:

- tools/jobwrap/ffi.c: 0/106 regions (0.0%)
  uncovered: bt_jobwrap_create_ceiling_job, bt_jobwrap_spawn_suspended, bt_jobwrap_assign_and_resume, bt_jobwrap_wait_exit_code, bt_jobwrap_dotnet_start_ticks
- tools/mutants-gate/lib.c: 0/586 regions (0.0%)
  uncovered: bt_gate_last_error, bt_gate_run, bt_gate_merge, lib.c:survivor_multiset, lib.c:parse_mutant_key
- tools/mutants-gate/tests.c: 0/886 regions (0.0%)
  uncovered: bt_test_gate_an_empty_function_column_is_rejected_naming_the, bt_test_gate_a_fractional_counter_is_rejected, bt_test_gate_a_negative_count_is_not_an_integer, bt_test_gate_alien_scenario_string_fails, bt_test_gate_arrow_notation_replacement_matches_bare_token
- tools/mutate/buildcmd.c: 0/204 regions (0.0%)
  uncovered: bt_mut_build_load, bt_mut_build_free, bt_mut_compile_argv, bt_mut_link_argv, buildcmd.c:clang_from_env_json
- tools/mutate/operators.c: 0/544 regions (0.0%)
  uncovered: bt_mut_op_name, bt_mut_mutants_free, bt_mut_enumerate, bt_mut_apply, bt_mut_find
- tools/mutate/scratch.c: 0/137 regions (0.0%)
  uncovered: bt_mut_scratch_root, bt_mut_scratch_is_orphan, bt_mut_scratch_sweep, scratch.c:marker_owner_alive, scratch.c:pid_alive
- tools/mutate/spawn.c: 0/255 regions (0.0%)
  uncovered: bt_mut_spawn, bt_mut_proc_free, bt_mut_compile_file, spawn.c:build_command, spawn.c:quote_arg
- tools/mutate/tests.c: 0/889 regions (0.0%)
  uncovered: engine_for, ws_fixture_target, fixture_target, enumerate_fixture, row_for
- tools/mutate/tests_engine.c: 0/534 regions (0.0%)
  uncovered: bt_test_mutate_engine_records_builderr_when_the_mutant_fails, bt_test_mutate_engine_records_killed_for_a_guarded_test, bt_test_mutate_engine_records_survived_for_unread_code, bt_test_mutate_engine_stages_the_suite_exe_inside_the_worksp, bt_test_mutate_engine_records_timeout_for_a_frozen_loop
- tools/mutate/tests_ops.c: 0/509 regions (0.0%)
  uncovered: bt_test_mutate_comparison_swap_variants, bt_test_mutate_negation_guard_mutants, bt_test_mutate_numeric_variants_and_suffixes, bt_test_mutate_preprocessor_lines_stay_untouched, bt_test_mutate_statement_delete_skips_declarations_and_contr
- tools/mutate/engine.c: 32/769 regions (4.2%)
  uncovered: bt_mut_engine_last_error, bt_mut_area_set_contains, bt_mut_area_closure, bt_mut_area_map_load, bt_mut_target_from_repo
- tools/mutate/report.c: 16/71 regions (22.5%)
  uncovered: bt_mut_report
- tests/fuzz/expand_body.h: 20/68 regions (29.4%)
- tests/fuzz/notation_body.h: 20/62 regions (32.3%)
- tests/fuzz/db.h: 102/283 regions (36.0%)
- tools/mutate/ledger.c: 119/272 regions (43.8%)
  uncovered: bt_mut_ledger_save, bt_mut_ledger_find, bt_mut_ledger_upsert, bt_mut_ledger_drop
- tools/ga-run/out.c: 367/818 regions (44.9%)
  uncovered: bt_gr_report_text, out.c:pair_rows_text, out.c:float_array_text
- src/app/states.c: 18/38 regions (47.4%)
  uncovered: bt_app_state_valid
- src/app/drivers.c: 85/173 regions (49.1%)
  uncovered: bt_app_usage_main, bt_app_fatal_main, drivers.c:usage_run, drivers.c:usage_loop
- tools/mutate/fs.c: 109/221 regions (49.3%)
  uncovered: bt_mut_file_creation, bt_mut_set_file_creation, bt_mut_process_start
- tests/fuzz/memo.h: 8/15 regions (53.3%)
- tools/jobwrap/lib.c: 309/563 regions (54.9%)
  uncovered: bt_jobwrap_load_config, bt_jobwrap_run, lib.c:write_pid_file, lib.c:make_dirs, lib.c:make_one_dir
- src/common/fsrs_math.c: 13/23 regions (56.5%)
- tools/ga-run/lib.c: 453/704 regions (64.3%)
  uncovered: bt_gr_run_all, lib.c:write_outputs, lib.c:ensure_parent_dirs, lib.c:write_plain, lib.c:distinct_syllables
- src/data/repo_algs.c: 104/160 regions (65.0%)
  uncovered: repo_algs.c:db_step_error
