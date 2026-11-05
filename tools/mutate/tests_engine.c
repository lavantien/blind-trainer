#include "mutate/tests_shared.h"

#include <stdio.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/json.h"
#include "unit/tools/spawn_util.h"

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

BT_TEST(mutate, engine_run_stops_after_the_limit) {
    bt_mut_mutant_list list;
    enumerate_fixture(&list);
    BT_CHECK(list.count >= 3);
    bt_mut_engine engine;
    engine_for("limit", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    bt_mut_target target;
    fixture_target(&target);
    bt_mut_summary summary;
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, NULL, 1, &summary), 0);
    bt_mut_ledger ledger;
    BT_CHECK_EQ(bt_mut_ledger_load(engine.ledger_path, &ledger), 0);
    unsigned long long settled = 0;
    for (size_t i = 0; i < ledger.count; i++) {
        if (ledger.rows[i].status != BT_MUT_PENDING) {
            settled++;
        }
    }
    bt_mut_ledger_free(&ledger);
    BT_CHECK_EQ(settled, 1ULL);
    BT_CHECK_EQ(summary.pending, list.count - 1);
    bt_mut_build_free(&engine.build);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, engine_run_refuses_a_malformed_ledger) {
    bt_mut_engine engine;
    engine_for("badledger", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    const char *broken = "id\tbroken\naaa\tb\n";
    BT_CHECK_EQ(bt_mut_write_file(engine.ledger_path, broken, strlen(broken)), 0);
    bt_mut_target target;
    fixture_target(&target);
    bt_mut_summary summary;
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, NULL, 0, &summary), 2);
    BT_CHECK(strstr(bt_mut_engine_last_error(), "cannot load ledger") != NULL);
    bt_mut_build_free(&engine.build);
}

BT_TEST(mutate, engine_run_refuses_an_unreadable_target) {
    bt_mut_engine engine;
    engine_for("noread", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    bt_mut_target target;
    fixture_target(&target);
    snprintf(target.file, BT_MUT_PATH_MAX, "src/absent-%lu.c", GetCurrentProcessId());
    bt_mut_summary summary;
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, NULL, 0, &summary), 2);
    BT_CHECK(strstr(bt_mut_engine_last_error(), "cannot read target") != NULL);
    bt_mut_build_free(&engine.build);
}

BT_TEST(mutate, engine_run_refuses_a_poisoned_ledger_path) {
    bt_mut_engine engine;
    engine_for("poisledger", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    char blocker[BT_MUT_PATH_MAX];
    snprintf(blocker, BT_MUT_PATH_MAX, "%s/blocker-poisledger.txt", g_dir);
    BT_CHECK_EQ(bt_mut_write_file(blocker, "x", 1), 0);
    snprintf(engine.ledger_path, BT_MUT_PATH_MAX, "%s/ledger.tsv", blocker);
    bt_mut_target target;
    fixture_target(&target);
    bt_mut_summary summary;
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, "zzz-none", 0, &summary), 2);
    BT_CHECK(strstr(bt_mut_engine_last_error(), "cannot save ledger") != NULL);
    bt_mut_build_free(&engine.build);
    remove(blocker);
}

static void fake_root_make(const char *slot, int obj_dirs, int with_suite, const char *suite_area,
                           int with_src, char root_out[BT_MUT_PATH_MAX]) {
    snprintf(root_out, BT_MUT_PATH_MAX, "%s/fake-%s", g_dir, slot);
    bt_mut_rmtree(root_out);
    char dir[BT_MUT_PATH_MAX];
    snprintf(dir, BT_MUT_PATH_MAX, "%s/build/obj", root_out);
    bt_test_mkdirs(dir);
    for (int i = 0; i < obj_dirs; i++) {
        char obj_dir[BT_MUT_PATH_MAX];
        snprintf(obj_dir, BT_MUT_PATH_MAX, "%s/d%d", dir, i);
        bt_test_mkdirs(obj_dir);
    }
    if (with_src) {
        snprintf(dir, BT_MUT_PATH_MAX, "%s/src/core", root_out);
        bt_test_mkdirs(dir);
        char src[BT_MUT_PATH_MAX];
        const char *body = "int bt_alg_id(void) {\n    return 0;\n}\n";
        snprintf(src, BT_MUT_PATH_MAX, "%s/alg.c", dir);
        bt_mut_write_file(src, body, strlen(body));
        snprintf(dir, BT_MUT_PATH_MAX, "%s/build/obj/d0/src/core", root_out);
        bt_test_mkdirs(dir);
        char obj[BT_MUT_PATH_MAX];
        snprintf(obj, BT_MUT_PATH_MAX, "%s/alg.obj", dir);
        bt_mut_write_file(obj, "", 0);
    }
    if (with_suite) {
        snprintf(dir, BT_MUT_PATH_MAX, "%s/tests/unit/%s", root_out, suite_area);
        bt_test_mkdirs(dir);
        char suite[BT_MUT_PATH_MAX];
        const char *body = "int fake_suite(void) {\n    return 0;\n}\n";
        snprintf(suite, BT_MUT_PATH_MAX, "%s/fake_test.c", dir);
        bt_mut_write_file(suite, body, strlen(body));
    }
}

BT_TEST(mutate, target_from_repo_builds_a_tools_path_target_over_a_fake_root) {
    char root[BT_MUT_PATH_MAX];
    fake_root_make("tools", 1, 1, "core", 1, root);
    bt_mut_engine engine;
    engine_for("fakeroot", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    snprintf(engine.repo_root, BT_MUT_PATH_MAX, "%s", root);
    bt_mut_target target;
    BT_CHECK_EQ(bt_mut_target_from_repo(&engine, "tools/mutate/main.c", "core_fake_test", &target),
                0);
    BT_CHECK_EQ(target.target_index, 1ULL);
    BT_CHECK_EQ(target.link_obj_count, 4ULL);
    bt_mut_target_free(&target);
    bt_mut_build_free(&engine.build);
    bt_mut_rmtree(root);
}

BT_TEST(mutate, target_from_repo_rejects_ambiguous_and_missing_obj_roots) {
    bt_mut_engine engine;
    engine_for("fakeobj", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    char root[BT_MUT_PATH_MAX];
    bt_mut_target target;
    fake_root_make("nodir", 0, 0, "core", 0, root);
    snprintf(engine.repo_root, BT_MUT_PATH_MAX, "%s", root);
    BT_CHECK_EQ(bt_mut_target_from_repo(&engine, "src/core/alg.c", "core_fake_test", &target), -1);
    BT_CHECK(strstr(bt_mut_engine_last_error(), "expected 1 object root") != NULL);
    bt_mut_rmtree(root);
    fake_root_make("twodir", 2, 0, "core", 0, root);
    snprintf(engine.repo_root, BT_MUT_PATH_MAX, "%s", root);
    BT_CHECK_EQ(bt_mut_target_from_repo(&engine, "src/core/alg.c", "core_fake_test", &target), -1);
    BT_CHECK(strstr(bt_mut_engine_last_error(), "expected 1 object root") != NULL);
    bt_mut_rmtree(root);
    fake_root_make("noobjtree", 0, 0, "core", 0, root);
    char obj_tree[BT_MUT_PATH_MAX];
    snprintf(obj_tree, BT_MUT_PATH_MAX, "%s/build/obj", root);
    bt_mut_rmtree(obj_tree);
    snprintf(engine.repo_root, BT_MUT_PATH_MAX, "%s", root);
    BT_CHECK_EQ(bt_mut_target_from_repo(&engine, "src/core/alg.c", "core_fake_test", &target), -1);
    BT_CHECK(strstr(bt_mut_engine_last_error(), "no build objects") != NULL);
    bt_mut_build_free(&engine.build);
    bt_mut_rmtree(root);
}

BT_TEST(mutate, target_from_repo_names_the_missing_suite_and_unknown_area) {
    bt_mut_engine engine;
    engine_for("fakesuite", MUT_TEST_TIMEOUT_MS, NULL, &engine);
    char root[BT_MUT_PATH_MAX];
    bt_mut_target target;
    fake_root_make("nosuite", 1, 0, "core", 0, root);
    snprintf(engine.repo_root, BT_MUT_PATH_MAX, "%s", root);
    BT_CHECK_EQ(bt_mut_target_from_repo(&engine, "src/core/alg.c", "core_fake_test", &target), -1);
    BT_CHECK(strstr(bt_mut_engine_last_error(), "no test suite named core_fake_test") != NULL);
    bt_mut_rmtree(root);
    fake_root_make("oddarea", 1, 1, "nosucharea", 0, root);
    snprintf(engine.repo_root, BT_MUT_PATH_MAX, "%s", root);
    BT_CHECK_EQ(bt_mut_target_from_repo(&engine, "src/core/alg.c", "core_fake_test", &target), -1);
    bt_mut_build_free(&engine.build);
    bt_mut_rmtree(root);
}

static bt_json_value *area_config(const char *body) {
    bt_json_value *value = NULL;
    bt_json_error error;
    if (bt_json_parse(body, strlen(body), &value, &error) != 0) {
        BT_ABORT("fixture config does not parse");
    }
    return value;
}

BT_TEST(mutate, area_map_rejects_malformed_src_deps) {
    const char *not_object = "{\"build\": {\"src_deps\": 5}}";
    const char *not_array = "{\"build\": {\"src_deps\": {\"core\": 5}}}";
    const char *bad_dep = "{\"build\": {\"src_deps\": {\"core\": [5]}}}";
    char long_name[512];
    snprintf(long_name, sizeof long_name, "{\"build\": {\"src_deps\": {\"%0300d\": []}}}", 7);
    const char *cases[] = {not_object, not_array, bad_dep, long_name};
    const char *errors[] = {
        "build.src_deps is not an object",
        "build.src_deps[core] is not an array",
        "build.src_deps[core] holds an invalid dependency",
        "area map overflow",
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        bt_json_value *config = area_config(cases[i]);
        bt_mut_area_map map;
        BT_CHECK_EQ(bt_mut_area_map_load(config, &map), -1);
        BT_CHECK(strstr(bt_mut_engine_last_error(), errors[i]) != NULL);
        bt_json_free(config);
    }
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
