#include "mutate/tests_shared.h"

#include <stdio.h>
#include <string.h>

#include "common/bt_harness.h"

#ifdef _WIN32
#include <windows.h>
#endif

BT_TEST(mutate, engine_records_builderr_when_the_mutant_fails_to_compile) {
    bt_mut_mutant_list list;
    enumerate_fixture(&list);
    const bt_mut_mutant *mutant =
        bt_mut_find(&list, BT_MUT_UNARY_DROP, "++", "", FX_LINE_UNUSED_INC);
    BT_CHECK(mutant != NULL);
    if (mutant == NULL) {
        bt_mut_mutants_free(&list);
        return;
    }
    bt_mut_engine engine;
    engine_for("builderr", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    bt_mut_target target;
    fixture_target(&target);
    bt_mut_summary summary;
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, mutant->id, 0, &summary), 0);
    bt_mut_row row = row_for(engine.ledger_path, mutant->id);
    BT_CHECK_EQ(row.status, (long long)BT_MUT_BUILDERR);
    BT_CHECK_EQ(row.attempts, 1ULL);
    bt_mut_build_free(&engine.build);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, engine_records_killed_for_a_guarded_test) {
    bt_mut_mutant_list list;
    enumerate_fixture(&list);
    const bt_mut_mutant *mutant = bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "0", "1", FX_LINE_ADD_0);
    BT_CHECK(mutant != NULL);
    if (mutant == NULL) {
        bt_mut_mutants_free(&list);
        return;
    }
    bt_mut_engine engine;
    engine_for("killed", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    bt_mut_target target;
    fixture_target(&target);
    bt_mut_summary summary;
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, mutant->id, 0, &summary), 0);
    bt_mut_row row = row_for(engine.ledger_path, mutant->id);
    BT_CHECK_EQ(row.status, (long long)BT_MUT_KILLED);
    BT_CHECK_EQ(row.attempts, 1ULL);
    BT_CHECK_EQ(summary.killed, 1ULL);
    BT_CHECK_EQ(summary.pending, list.count - 1);
    bt_mut_build_free(&engine.build);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, engine_records_survived_for_unread_code) {
    bt_mut_mutant_list list;
    enumerate_fixture(&list);
    const bt_mut_mutant *mutant =
        bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "3", "4", FX_LINE_UNUSED_3);
    BT_CHECK(mutant != NULL);
    if (mutant == NULL) {
        bt_mut_mutants_free(&list);
        return;
    }
    bt_mut_engine engine;
    engine_for("survived", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    bt_mut_target target;
    fixture_target(&target);
    bt_mut_summary summary;
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, mutant->id, 0, &summary), 0);
    bt_mut_row row = row_for(engine.ledger_path, mutant->id);
    BT_CHECK_EQ(row.status, (long long)BT_MUT_SURVIVED);
    bt_mut_build_free(&engine.build);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, engine_stages_the_suite_exe_inside_the_workspace) {
    bt_mut_engine engine;
    engine_for("wsstage", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    char temp[BT_MUT_PATH_MAX];
    GetTempPathA(BT_MUT_PATH_MAX, temp);
    size_t len = strlen(temp);
    while (len > 0 && (temp[len - 1] == '\\' || temp[len - 1] == '/')) {
        len--;
    }
    temp[len] = '\0';
    snprintf(engine.scratch_root, BT_MUT_PATH_MAX, "%s/bt-mut-selftest-wsstage", temp);
    bt_mut_rmtree(engine.scratch_root);
    bt_mut_target target;
    ws_fixture_target(&target);
    bt_mut_summary summary;
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, "zzz-none", 0, &summary), 0);
    bt_mut_build_free(&engine.build);
}

BT_TEST(mutate, engine_records_timeout_for_a_frozen_loop) {
    bt_mut_mutant_list list;
    enumerate_fixture(&list);
    const bt_mut_mutant *mutant =
        bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "1", "0", FX_LINE_SPIN_1);
    BT_CHECK(mutant != NULL);
    if (mutant == NULL) {
        bt_mut_mutants_free(&list);
        return;
    }
    bt_mut_engine engine;
    engine_for("timeout", 2500, NULL, &engine);
    bt_mut_target target;
    fixture_target(&target);
    bt_mut_summary summary;
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, mutant->id, 0, &summary), 0);
    bt_mut_row row = row_for(engine.ledger_path, mutant->id);
    BT_CHECK_EQ(row.status, (long long)BT_MUT_TIMEOUT);
    bt_mut_build_free(&engine.build);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, engine_rejects_parallel_workers) {
    bt_mut_engine engine;
    engine_for("workers", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    engine.workers = 2;
    bt_mut_target target;
    fixture_target(&target);
    bt_mut_summary summary;
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, NULL, 0, &summary), 2);
    BT_CHECK(strstr(bt_mut_engine_last_error(), "workers must be 1") != NULL);
    bt_mut_build_free(&engine.build);
}

BT_TEST(mutate, ledger_resume_reruns_only_pending_rows) {
    bt_mut_mutant_list list;
    enumerate_fixture(&list);
    BT_CHECK(list.count >= 4);
    char only_first[128];
    char only_four[256];
    snprintf(only_first, sizeof only_first, "%s,%s", list.items[0].id, list.items[1].id);
    snprintf(only_four, sizeof only_four, "%s,%s,%s,%s", list.items[0].id, list.items[1].id,
             list.items[2].id, list.items[3].id);
    bt_mut_engine engine;
    engine_for("resume", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    bt_mut_target target;
    fixture_target(&target);
    bt_mut_summary summary;
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, only_first, 0, &summary), 0);
    bt_mut_ledger ledger;
    BT_CHECK_EQ(bt_mut_ledger_load(engine.ledger_path, &ledger), 0);
    BT_CHECK_EQ(ledger.rows[0].attempts, 1ULL);
    BT_CHECK_EQ(ledger.rows[1].attempts, 1ULL);
    BT_CHECK(ledger.rows[0].status != BT_MUT_PENDING);
    BT_CHECK(ledger.rows[1].status != BT_MUT_PENDING);
    unsigned long long pending = 0;
    for (size_t i = 0; i < ledger.count; i++) {
        if (ledger.rows[i].status == BT_MUT_PENDING) {
            pending++;
        }
    }
    BT_CHECK_EQ(pending, list.count - 2);
    bt_mut_ledger_free(&ledger);
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, only_four, 0, &summary), 0);
    BT_CHECK_EQ(bt_mut_ledger_load(engine.ledger_path, &ledger), 0);
    BT_CHECK_EQ(ledger.rows[2].attempts, 1ULL);
    BT_CHECK_EQ(ledger.rows[3].attempts, 1ULL);
    BT_CHECK_EQ(ledger.rows[0].attempts, 1ULL);
    BT_CHECK_EQ(ledger.rows[1].attempts, 1ULL);
    for (size_t i = 0; i < 4; i++) {
        BT_CHECK(ledger.rows[i].status != BT_MUT_PENDING);
    }
    bt_mut_ledger_free(&ledger);
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, only_four, 0, &summary), 0);
    BT_CHECK_EQ(bt_mut_ledger_load(engine.ledger_path, &ledger), 0);
    for (size_t i = 0; i < 4; i++) {
        BT_CHECK_EQ(ledger.rows[i].attempts, 1ULL);
    }
    unsigned long long done = 0;
    for (size_t i = 0; i < ledger.count; i++) {
        if (ledger.rows[i].status != BT_MUT_PENDING) {
            done++;
        }
    }
    BT_CHECK_EQ(done, 4ULL);
    bt_mut_ledger_free(&ledger);
    bt_mut_build_free(&engine.build);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, report_refuses_pending_rows) {
    bt_mut_mutant_list list;
    enumerate_fixture(&list);
    bt_mut_engine engine;
    engine_for("report-pending", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    bt_mut_target target;
    fixture_target(&target);
    bt_mut_summary summary;
    char only[64];
    snprintf(only, sizeof only, "%s", list.items[0].id);
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, only, 0, &summary), 0);
    char out[BT_MUT_PATH_MAX];
    snprintf(out, BT_MUT_PATH_MAX, "%s/outcomes-pending.json", g_dir);
    BT_CHECK_EQ(bt_mut_report(engine.ledger_path, g_root, out), 1);
    bt_mut_build_free(&engine.build);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, spawn_error_leaves_the_row_pending) {
    bt_mut_mutant_list list;
    enumerate_fixture(&list);
    const bt_mut_mutant *mutant = bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "0", "1", FX_LINE_ADD_0);
    BT_CHECK(mutant != NULL);
    bt_mut_engine engine;
    engine_for("spawnfail", MUT_TEST_TIMEOUT_MS, "no-such-clang.exe", &engine);
    bt_mut_target target;
    fixture_target(&target);
    bt_mut_summary summary;
    int rc = bt_mut_engine_run(&engine, &target, 1, mutant != NULL ? mutant->id : "", 0, &summary);
    BT_CHECK_EQ(rc, 1);
    BT_CHECK(strstr(bt_mut_engine_last_error(), "baseline run failed") != NULL);
    if (mutant != NULL) {
        bt_mut_row row = row_for(engine.ledger_path, mutant->id);
        BT_CHECK_EQ(row.status, (long long)BT_MUT_PENDING);
        BT_CHECK_EQ(row.attempts, 0ULL);
    }
    bt_mut_build_free(&engine.build);
    BT_CHECK_EQ(bt_mut_build_load(g_root, g_config, NULL, &engine.build), 0);
    rc = bt_mut_engine_run(&engine, &target, 1, mutant != NULL ? mutant->id : "", 0, &summary);
    BT_CHECK_EQ(rc, 0);
    if (mutant != NULL) {
        bt_mut_row row = row_for(engine.ledger_path, mutant->id);
        BT_CHECK_EQ(row.status, (long long)BT_MUT_KILLED);
        BT_CHECK_EQ(row.attempts, 1ULL);
    }
    bt_mut_build_free(&engine.build);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, stale_ledger_rows_are_dropped) {
    bt_mut_mutant_list list;
    enumerate_fixture(&list);
    const bt_mut_mutant *mutant = bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "0", "1", FX_LINE_ADD_0);
    BT_CHECK(mutant != NULL);
    bt_mut_engine engine;
    engine_for("stale", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    char stale_text[512];
    snprintf(stale_text, sizeof stale_text,
             "id\tfile\tline\tcol\top\tfrom\tto\tstatus\tattempts\tlast_run\ttest_exe\n"
             "deadbeefdeadbeef\t%s\t99\t9\tNumericTweak\t9\t8\tsurvived\t3\t2026-10-27T10:00:00\t"
             "fx_test\n",
             FX_REL);
    bt_mut_write_file(engine.ledger_path, stale_text, strlen(stale_text));
    bt_mut_target target;
    fixture_target(&target);
    bt_mut_summary summary;
    BT_CHECK_EQ(
        bt_mut_engine_run(&engine, &target, 1, mutant != NULL ? mutant->id : "", 0, &summary), 0);
    bt_mut_ledger ledger;
    BT_CHECK_EQ(bt_mut_ledger_load(engine.ledger_path, &ledger), 0);
    BT_CHECK_EQ(ledger.count, list.count);
    BT_CHECK(bt_mut_ledger_find(&ledger, "deadbeefdeadbeef") == NULL);
    bt_mut_ledger_free(&ledger);
    bt_mut_build_free(&engine.build);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, exe_switch_reruns_terminal_rows) {
    bt_mut_mutant_list list;
    enumerate_fixture(&list);
    const bt_mut_mutant *mutant = bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "0", "1", FX_LINE_ADD_0);
    BT_CHECK(mutant != NULL);
    if (mutant == NULL) {
        bt_mut_mutants_free(&list);
        return;
    }
    bt_mut_engine engine;
    bt_mut_target target;
    bt_mut_summary summary;
    engine_for("exeswitch", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    fixture_target(&target);
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, mutant->id, 0, &summary), 0);
    bt_mut_ledger ledger;
    BT_CHECK_EQ(bt_mut_ledger_load(engine.ledger_path, &ledger), 0);
    BT_CHECK_EQ(ledger.rows[0].attempts, 1ULL);
    bt_mut_ledger_free(&ledger);
    snprintf(engine.test_exe, BT_MUT_PATH_MAX, "fx_test2");
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, mutant->id, 0, &summary), 0);
    BT_CHECK_EQ(bt_mut_ledger_load(engine.ledger_path, &ledger), 0);
    unsigned long long attempts = 0;
    for (size_t i = 0; i < ledger.count; i++) {
        if (strcmp(ledger.rows[i].id, mutant->id) == 0) {
            attempts = ledger.rows[i].attempts;
        }
    }
    BT_CHECK_EQ(attempts, 2ULL);
    bt_mut_ledger_free(&ledger);
    bt_mut_build_free(&engine.build);
    bt_mut_mutants_free(&list);
}
