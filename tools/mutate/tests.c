#include "mutate/tests.h"
#include "mutate/mutate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/config.h"
#include "common/json.h"
#include "unit/tools/spawn_util.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define MUT_CHECK(cond, ...)                                                                       \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            bt_harness_fail(__FILE__, __LINE__, __VA_ARGS__);                                      \
        }                                                                                          \
    } while (0)

static void ensure_setup(void);
static int bt_spawn_probe(char *argv[3], bt_mut_proc *proc);

#define FX_REL "tests/unit/tools/fixtures/fx_target.c"

enum {
    FX_LINE_ADD_0 = 6,
    FX_LINE_SPIN_1 = 13,
    FX_LINE_SPIN_WHILE = 11,
    FX_LINE_NOT_BANG = 19,
    FX_LINE_DEL_10 = 26,
    FX_LINE_DEL_STMT = 27,
    FX_LINE_UNUSED_3 = 36,
    FX_LINE_UNUSED_INC = 37,
};

static char g_root[BT_MUT_PATH_MAX];
static char g_dir[BT_MUT_PATH_MAX];
static bt_json_value *g_config;
static bt_mut_build g_build;
static char g_fx_main_obj[BT_MUT_PATH_MAX];
static char g_stub_exe[BT_MUT_PATH_MAX];
static char g_self_exe[BT_MUT_PATH_MAX];
static int g_ready;

static void ensure_setup(void) {
    if (g_ready) {
        return;
    }
    bt_test_repo_root(g_root);
    snprintf(g_dir, BT_MUT_PATH_MAX, "%s/build/test-tmp/mutate-selftest", g_root);
    bt_test_mkdirs(g_dir);
    char config_path[BT_MUT_PATH_MAX];
    snprintf(config_path, BT_MUT_PATH_MAX, "%s/config/config.json", g_root);
    if (bt_config_try_load(config_path, &g_config) != 0) {
        BT_ABORT("cannot load %s", config_path);
    }
    if (bt_mut_build_load(g_root, g_config, NULL, &g_build) != 0) {
        BT_ABORT("cannot load build settings");
    }
    char obj_dir[BT_MUT_PATH_MAX];
    snprintf(obj_dir, BT_MUT_PATH_MAX, "%s/obj", g_dir);
    bt_test_mkdirs(obj_dir);
    char fx_main[BT_MUT_PATH_MAX];
    snprintf(fx_main, BT_MUT_PATH_MAX, "%s/tests/unit/tools/fixtures/fx_main.c", g_root);
    snprintf(g_fx_main_obj, BT_MUT_PATH_MAX, "%s/fx_main.obj", obj_dir);
    char diag[2048];
    if (bt_mut_compile_file(&g_build, NULL, fx_main, g_fx_main_obj, diag, sizeof diag) != 0) {
        BT_ABORT("cannot compile fx_main: %s", diag);
    }
    char stub_src[BT_MUT_PATH_MAX];
    char stub_obj[BT_MUT_PATH_MAX];
    snprintf(stub_src, BT_MUT_PATH_MAX, "%s/tests/unit/tools/fixtures/stub_clang.c", g_root);
    snprintf(stub_obj, BT_MUT_PATH_MAX, "%s/stub_clang.obj", obj_dir);
    snprintf(g_stub_exe, BT_MUT_PATH_MAX, "%s/stub-clang.exe", g_dir);
    if (bt_mut_compile_file(&g_build, NULL, stub_src, stub_obj, diag, sizeof diag) != 0) {
        BT_ABORT("cannot compile stub: %s", diag);
    }
    char *objects[1] = {stub_obj};
    char *argv[32];
    size_t count = bt_mut_link_argv(&g_build, objects, 1, g_stub_exe, argv, 32);
    if (count == 0 || count + 1 > 32) {
        BT_ABORT("stub link argv assembly failed");
    }
    argv[count] = NULL;
    bt_mut_proc proc;
    if (bt_mut_spawn(g_build.clang, argv, NULL, 60000, &proc) != 0 || proc.exit_code != 0) {
        bt_mut_proc_free(&proc);
        BT_ABORT("cannot link stub");
    }
    bt_mut_proc_free(&proc);
    GetModuleFileNameA(NULL, g_self_exe, BT_MUT_PATH_MAX);
    g_ready = 1;
}

static void engine_for(const char *slot, unsigned long long timeout_ms, const char *clang_override,
                       bt_mut_engine *engine) {
    ensure_setup();
    memset(engine, 0, sizeof *engine);
    snprintf(engine->repo_root, BT_MUT_PATH_MAX, "%s", g_root);
    if (bt_mut_build_load(g_root, g_config, clang_override, &engine->build) != 0) {
        BT_ABORT("cannot load build settings");
    }
    snprintf(engine->scratch_root, BT_MUT_PATH_MAX, "%s/scratch-%s", g_dir, slot);
    snprintf(engine->ledger_path, BT_MUT_PATH_MAX, "%s/ledger-%s.tsv", g_dir, slot);
    bt_mut_rmtree(engine->scratch_root);
    remove(engine->ledger_path);
    engine->timeout_ms = timeout_ms;
    engine->workers = 1;
    snprintf(engine->test_exe, BT_MUT_PATH_MAX, "fx_test");
}

static char g_slot_obj[BT_MUT_PATH_MAX];

static void fixture_target(bt_mut_target *target) {
    memset(target, 0, sizeof *target);
    snprintf(target->file, BT_MUT_PATH_MAX, "%s", FX_REL);
    snprintf(g_slot_obj, BT_MUT_PATH_MAX, "%s/obj/fx_target.obj", g_dir);
    static char *objs[2];
    objs[0] = g_slot_obj;
    objs[1] = g_fx_main_obj;
    target->link_objs = objs;
    target->link_obj_count = 2;
    target->target_index = 0;
}

static char *fixture_text(size_t *len) {
    char path[BT_MUT_PATH_MAX];
    snprintf(path, sizeof path, "%s/%s", g_root, FX_REL);
    char *text = NULL;
    if (bt_mut_read_file(path, &text, len) != 0) {
        BT_ABORT("cannot read fixture");
    }
    return text;
}

static void enumerate_fixture(bt_mut_mutant_list *list) {
    size_t len = 0;
    char *text = fixture_text(&len);
    if (bt_mut_enumerate(FX_REL, text, len, list) != 0) {
        free(text);
        BT_ABORT("cannot enumerate fixture");
    }
    free(text);
}

static long find_token(const bt_mut_tokens *tokens, const char *text) {
    for (size_t i = 0; i < tokens->count; i++) {
        const char *candidate = bt_mut_token_text(tokens, &tokens->items[i]);
        size_t candidate_len = tokens->items[i].len;
        if (strlen(text) == candidate_len && memcmp(candidate, text, candidate_len) == 0) {
            return (long)i;
        }
    }
    return -1;
}

static bt_mut_row row_for(const char *ledger_path, const char *id) {
    bt_mut_ledger ledger;
    bt_mut_row found;
    memset(&found, 0, sizeof found);
    if (bt_mut_ledger_load(ledger_path, &ledger) != 0) {
        BT_ABORT("cannot load ledger");
    }
    bt_mut_row *row = bt_mut_ledger_find(&ledger, id);
    if (row != NULL) {
        found = *row;
        found.file = NULL;
        found.op = NULL;
        found.from = NULL;
        found.to = NULL;
        found.last_run = NULL;
        found.test_exe = NULL;
    }
    bt_mut_ledger_free(&ledger);
    return found;
}

BT_TEST(mutate, apply_compiles_for_every_operator) {
    ensure_setup();
    bt_mut_mutant_list list;
    enumerate_fixture(&list);
    const bt_mut_mutant *picks[5] = {
        bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "10", "11", FX_LINE_DEL_10),
        bt_mut_find(&list, BT_MUT_COMPARISON_SWAP, "<", "<=", FX_LINE_SPIN_WHILE),
        bt_mut_find(&list, BT_MUT_CONDITION_NEGATION, "while", "!(...)", FX_LINE_SPIN_WHILE),
        bt_mut_find(&list, BT_MUT_UNARY_DROP, "!", "", FX_LINE_NOT_BANG),
        bt_mut_find(&list, BT_MUT_STATEMENT_DELETE, "r", "", FX_LINE_DEL_STMT),
    };
    static const char *const names[5] = {"numeric", "comparison", "negation", "unary", "delete"};
    size_t len = 0;
    char *text = fixture_text(&len);
    for (int i = 0; i < 5; i++) {
        MUT_CHECK(picks[i] != NULL, "missing pick %s", names[i]);
        if (picks[i] == NULL) {
            continue;
        }
        char *mutated = NULL;
        size_t mutated_len = 0;
        BT_CHECK_EQ(bt_mut_apply(text, len, picks[i], &mutated, &mutated_len), 0);
        char source[BT_MUT_PATH_MAX];
        char obj[BT_MUT_PATH_MAX];
        snprintf(source, BT_MUT_PATH_MAX, "%s/apply-%s.c", g_dir, names[i]);
        snprintf(obj, BT_MUT_PATH_MAX, "%s/apply-%s.obj", g_dir, names[i]);
        bt_mut_write_file(source, mutated, mutated_len);
        free(mutated);
        char diag[2048];
        int rc = bt_mut_compile_file(&g_build, NULL, source, obj, diag, sizeof diag);
        MUT_CHECK(rc == 0, "%s mutant does not compile: %s", names[i], diag);
    }
    free(text);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, comparison_swap_variants) {
    const char *src =
        "int f(int a, int b) {\n    if (a < b) {\n        return 1;\n    }\n    if (a "
        "== b) {\n        return 2;\n    }\n    while (a >= b) {\n        return "
        "3;\n    }\n    return 0;\n}\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    size_t swaps = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].op == BT_MUT_COMPARISON_SWAP) {
            swaps++;
        }
    }
    BT_CHECK_EQ(swaps, 3ULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_COMPARISON_SWAP, "<", "<=", 2) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_COMPARISON_SWAP, "==", "!=", 5) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_COMPARISON_SWAP, ">=", ">", 8) != NULL);
    bt_mut_mutants_free(&list);
}

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
    engine_for("builderr", 60000, NULL, &engine);
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
    engine_for("killed", 60000, NULL, &engine);
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
    engine_for("survived", 60000, NULL, &engine);
    bt_mut_target target;
    fixture_target(&target);
    bt_mut_summary summary;
    BT_CHECK_EQ(bt_mut_engine_run(&engine, &target, 1, mutant->id, 0, &summary), 0);
    bt_mut_row row = row_for(engine.ledger_path, mutant->id);
    BT_CHECK_EQ(row.status, (long long)BT_MUT_SURVIVED);
    bt_mut_build_free(&engine.build);
    bt_mut_mutants_free(&list);
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
    engine_for("workers", 60000, NULL, &engine);
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
    engine_for("resume", 60000, NULL, &engine);
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

BT_TEST(mutate, mutant_ids_are_deterministic) {
    bt_mut_mutant_list first;
    bt_mut_mutant_list second;
    enumerate_fixture(&first);
    enumerate_fixture(&second);
    BT_CHECK_EQ(first.count, second.count);
    for (size_t i = 0; i < first.count && i < second.count; i++) {
        BT_CHECK_EQ(first.items[i].id, second.items[i].id);
    }
    bt_mut_mutants_free(&first);
    bt_mut_mutants_free(&second);
}

BT_TEST(mutate, negation_guard_mutants) {
    const char *src = "int f(int a) {\n    if (a) {\n        return 1;\n    }\n    while (a) "
                      "{\n        a = 0;\n    }\n    return 0;\n}\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    size_t negations = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].op == BT_MUT_CONDITION_NEGATION) {
            negations++;
        }
    }
    BT_CHECK_EQ(negations, 2ULL);
    const bt_mut_mutant *if_mutant =
        bt_mut_find(&list, BT_MUT_CONDITION_NEGATION, "if", "!(...)", 2);
    const bt_mut_mutant *while_mutant =
        bt_mut_find(&list, BT_MUT_CONDITION_NEGATION, "while", "!(...)", 5);
    BT_CHECK(if_mutant != NULL);
    BT_CHECK(while_mutant != NULL);
    char *applied = NULL;
    size_t applied_len = 0;
    if (if_mutant != NULL) {
        BT_CHECK_EQ(bt_mut_apply(src, strlen(src), if_mutant, &applied, &applied_len), 0);
        BT_CHECK(strstr(applied, "if (!(a)) {") != NULL);
        free(applied);
        applied = NULL;
    }
    if (while_mutant != NULL) {
        BT_CHECK_EQ(bt_mut_apply(src, strlen(src), while_mutant, &applied, &applied_len), 0);
        BT_CHECK(strstr(applied, "while (!(a)) {") != NULL);
        free(applied);
    }
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, numeric_variants_and_suffixes) {
    const char *src = "unsigned a = 3u;\ndouble d = 2.5f;\nlong z = 0;\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    size_t numeric = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].op == BT_MUT_NUMERIC_TWEAK) {
            numeric++;
        }
    }
    BT_CHECK_EQ(numeric, 10ULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "3u", "4u", 1) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "3u", "0u", 1) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "2.5f", "3.5f", 2) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "2.5f", "0.0f", 2) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "0", "-1", 3) != NULL);
    BT_CHECK(bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "0", "1", 3) != NULL);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, orphan_scratch_is_judged_by_creation_time) {
    ensure_setup();
    char root[BT_MUT_PATH_MAX];
    snprintf(root, BT_MUT_PATH_MAX, "%s/scratch-orphan", g_dir);
    bt_mut_rmtree(root);
    bt_test_mkdirs(root);
    char stale[BT_MUT_PATH_MAX];
    char live[BT_MUT_PATH_MAX];
    snprintf(stale, BT_MUT_PATH_MAX, "%s/stale", root);
    snprintf(live, BT_MUT_PATH_MAX, "%s/live", root);
    bt_test_mkdirs(stale);
    bt_test_mkdirs(live);
    char stale_marker[BT_MUT_PATH_MAX];
    char live_marker[BT_MUT_PATH_MAX];
    snprintf(stale_marker, BT_MUT_PATH_MAX, "%s/marker.txt", stale);
    snprintf(live_marker, BT_MUT_PATH_MAX, "%s/marker.txt", live);
    bt_mut_write_file(stale_marker, "stale", 5);
    uint64_t backdated = bt_mut_process_start() - 3600ULL * 10000000ULL;
    BT_CHECK_EQ(bt_mut_set_file_creation(stale_marker, backdated), 0);
    char *argv[3] = {NULL, "selftest-probe", NULL};
    argv[0] = g_self_exe;
    bt_mut_proc proc;
    BT_CHECK_EQ(bt_spawn_probe(argv, &proc), 0);
    uint64_t probe_start = strtoull(proc.output, NULL, 10);
    bt_mut_proc_free(&proc);
    BT_CHECK(probe_start > 0);
    bt_mut_write_file(live_marker, "live", 4);
    BT_CHECK_EQ(bt_mut_scratch_is_orphan(stale, probe_start), 1);
    BT_CHECK_EQ(bt_mut_scratch_is_orphan(live, probe_start), 0);
    BT_CHECK_EQ(bt_mut_scratch_sweep(root, probe_start), 1ULL);
    char probe[BT_MUT_PATH_MAX];
    snprintf(probe, BT_MUT_PATH_MAX, "%s/marker.txt", stale);
    FILE *gone = fopen(probe, "rb");
    BT_CHECK(gone == NULL);
    if (gone != NULL) {
        fclose(gone);
    }
    FILE *kept = fopen(live_marker, "rb");
    BT_CHECK(kept != NULL);
    if (kept != NULL) {
        fclose(kept);
    }
}

BT_TEST(mutate, preprocessor_lines_stay_untouched) {
    const char *src = "#define N 3\n#define CMP(a, b) ((a) < (b))\nint f(void) {\n    return N + "
                      "3;\n}\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    BT_CHECK_EQ(list.count, 4ULL);
    for (size_t i = 0; i < list.count; i++) {
        BT_CHECK_EQ(list.items[i].op, (long long)BT_MUT_NUMERIC_TWEAK);
        BT_CHECK_EQ(list.items[i].line, 4ULL);
    }
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, report_maps_verdicts_and_attributes_functions) {
    ensure_setup();
    const char *ledger_text =
        "id\tfile\tline\tcol\top\tfrom\tto\tstatus\tattempts\tlast_run\n"
        "1111111111111111\t" FX_REL "\t6\t20\tNumericTweak\t0\t1\tkilled\t1\t2026-10-27T10:00:00\n"
        "2222222222222222\t" FX_REL "\t36\t13\tNumericTweak\t3\t4\tsurvived\t1\t"
        "2026-10-27T10:00:01\n";
    char ledger_path[BT_MUT_PATH_MAX];
    char outcomes[BT_MUT_PATH_MAX];
    snprintf(ledger_path, BT_MUT_PATH_MAX, "%s/ledger-report.tsv", g_dir);
    snprintf(outcomes, BT_MUT_PATH_MAX, "%s/outcomes-report.json", g_dir);
    bt_mut_write_file(ledger_path, ledger_text, strlen(ledger_text));
    BT_CHECK_EQ(bt_mut_report(ledger_path, g_root, outcomes), 0);
    char *text = NULL;
    size_t len = 0;
    BT_CHECK_EQ(bt_mut_read_file(outcomes, &text, &len), 0);
    bt_json_value *root = NULL;
    bt_json_error error;
    BT_CHECK_EQ(bt_json_parse(text, len, &root, &error), 0);
    const bt_json_value *total = bt_json_get(root, "total_mutants");
    const bt_json_value *caught = bt_json_get(root, "caught");
    const bt_json_value *missed = bt_json_get(root, "missed");
    BT_CHECK(total != NULL && total->type == BT_JSON_NUMBER);
    BT_CHECK(caught != NULL && caught->type == BT_JSON_NUMBER);
    BT_CHECK(missed != NULL && missed->type == BT_JSON_NUMBER);
    if (total != NULL && total->type == BT_JSON_NUMBER) {
        BT_CHECK_EQ((unsigned long long)total->as.number, 2ULL);
    }
    if (caught != NULL && caught->type == BT_JSON_NUMBER) {
        BT_CHECK_EQ((unsigned long long)caught->as.number, 1ULL);
    }
    if (missed != NULL && missed->type == BT_JSON_NUMBER) {
        BT_CHECK_EQ((unsigned long long)missed->as.number, 1ULL);
    }
    const bt_json_value *outcomes_array = bt_json_get(root, "outcomes");
    BT_CHECK(outcomes_array != NULL && outcomes_array->type == BT_JSON_ARRAY);
    if (outcomes_array != NULL && outcomes_array->type == BT_JSON_ARRAY) {
        BT_CHECK_EQ(outcomes_array->as.array.count, 3ULL);
        const bt_json_value *baseline = bt_json_get(bt_json_at(outcomes_array, 0), "scenario");
        BT_CHECK(baseline != NULL && baseline->type == BT_JSON_STRING &&
                 strcmp(baseline->as.string, "Baseline") == 0);
        const bt_json_value *first =
            bt_json_get(bt_json_get(bt_json_at(outcomes_array, 1), "scenario"), "Mutant");
        const bt_json_value *first_name =
            bt_json_get(bt_json_get(first, "function"), "function_name");
        BT_CHECK(first_name != NULL && first_name->type == BT_JSON_STRING &&
                 strcmp(first_name->as.string, "fx_add") == 0);
        const bt_json_value *first_summary = bt_json_get(bt_json_at(outcomes_array, 1), "summary");
        BT_CHECK(first_summary != NULL && first_summary->type == BT_JSON_STRING &&
                 strcmp(first_summary->as.string, "CaughtMutant") == 0);
        const bt_json_value *second =
            bt_json_get(bt_json_get(bt_json_at(outcomes_array, 2), "scenario"), "Mutant");
        const bt_json_value *second_name =
            bt_json_get(bt_json_get(second, "function"), "function_name");
        BT_CHECK(second_name != NULL && second_name->type == BT_JSON_STRING &&
                 strcmp(second_name->as.string, "fx_unused") == 0);
        const bt_json_value *second_summary = bt_json_get(bt_json_at(outcomes_array, 2), "summary");
        BT_CHECK(second_summary != NULL && second_summary->type == BT_JSON_STRING &&
                 strcmp(second_summary->as.string, "MissedMutant") == 0);
        const bt_json_value *replacement = bt_json_get(second, "replacement");
        BT_CHECK(replacement != NULL && replacement->type == BT_JSON_STRING &&
                 strcmp(replacement->as.string, "4") == 0);
    }
    bt_json_free(root);
    free(text);
}

BT_TEST(mutate, report_refuses_pending_rows) {
    bt_mut_mutant_list list;
    enumerate_fixture(&list);
    bt_mut_engine engine;
    engine_for("report-pending", 60000, NULL, &engine);
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

BT_TEST(mutate, statement_delete_skips_declarations_and_control) {
    const char *src = "int f(int v) {\n    int r = 10;\n    r = v + 5;\n    for (r = 0; r < v; r"
                      "++) {\n        r = 9;\n    }\n    return r;\n}\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    size_t deletions = 0;
    const bt_mut_mutant *target = NULL;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].op == BT_MUT_STATEMENT_DELETE) {
            deletions++;
            if (target == NULL || list.items[i].line < target->line) {
                target = &list.items[i];
            }
        }
    }
    BT_CHECK_EQ(deletions, 2ULL);
    BT_CHECK(target != NULL);
    char *applied = NULL;
    size_t applied_len = 0;
    if (target != NULL) {
        BT_CHECK_EQ(target->line, 3ULL);
        BT_CHECK_EQ(bt_mut_apply(src, strlen(src), target, &applied, &applied_len), 0);
        BT_CHECK(strstr(applied, "r = v + 5;") == NULL);
        BT_CHECK(strstr(applied, "r = 9;") != NULL);
        free(applied);
    }
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, stub_assembly_matches_the_bt_py_contract) {
    bt_mut_mutant_list list;
    enumerate_fixture(&list);
    const bt_mut_mutant *mutant = bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "0", "1", FX_LINE_ADD_0);
    BT_CHECK(mutant != NULL);
    char record[BT_MUT_PATH_MAX];
    snprintf(record, BT_MUT_PATH_MAX, "%s/record-assembly.txt", g_dir);
    remove(record);
    bt_mut_engine engine;
    engine_for("assembly", 60000, g_stub_exe, &engine);
    bt_mut_target target;
    fixture_target(&target);
    SetEnvironmentVariableA("BT_STUB_RECORD", record);
    bt_mut_summary summary;
    int rc = bt_mut_engine_run(&engine, &target, 1, mutant != NULL ? mutant->id : "", 0, &summary);
    SetEnvironmentVariableA("BT_STUB_RECORD", NULL);
    BT_CHECK_EQ(rc, 1);
    BT_CHECK(strstr(bt_mut_engine_last_error(), "baseline run failed") != NULL);
    char *text = NULL;
    size_t len = 0;
    BT_CHECK_EQ(bt_mut_read_file(record, &text, &len), 0);
    char *lines[128];
    size_t line_count = 0;
    for (char *cursor = text; cursor != NULL && *cursor != '\0';) {
        char *newline = strchr(cursor, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }
        size_t line_len = strlen(cursor);
        if (line_len > 0 && cursor[line_len - 1] == '\r') {
            cursor[line_len - 1] = '\0';
        }
        lines[line_count++] = cursor;
        cursor = newline == NULL ? NULL : newline + 1;
    }
    char scratch_dir[BT_MUT_PATH_MAX];
    snprintf(scratch_dir, BT_MUT_PATH_MAX, "%s/scratch-assembly/baseline-0", g_dir);
    char expected[128][BT_MUT_PATH_MAX];
    size_t expected_count = 0;
    snprintf(expected[expected_count++], BT_MUT_PATH_MAX, "-target");
    const char *target_name = "";
    bt_config_str(g_config, "build.target", &target_name);
    snprintf(expected[expected_count++], BT_MUT_PATH_MAX, "%s", target_name);
    const char *std_name = "";
    bt_config_str(g_config, "build.std", &std_name);
    snprintf(expected[expected_count++], BT_MUT_PATH_MAX, "-std=%s", std_name);
    const bt_json_value *cflags = bt_json_get(bt_json_get(g_config, "build"), "cflags");
    if (cflags != NULL && cflags->type == BT_JSON_ARRAY) {
        for (size_t i = 0; i < cflags->as.array.count; i++) {
            snprintf(expected[expected_count++], BT_MUT_PATH_MAX, "%s",
                     cflags->as.array.items[i]->as.string);
        }
    }
    const bt_json_value *release_flags =
        bt_json_get(bt_json_get(g_config, "build"), "release_flags");
    if (release_flags != NULL && release_flags->type == BT_JSON_ARRAY) {
        for (size_t i = 0; i < release_flags->as.array.count; i++) {
            snprintf(expected[expected_count++], BT_MUT_PATH_MAX, "%s",
                     release_flags->as.array.items[i]->as.string);
        }
    }
    size_t flag_count = expected_count;
    size_t compile_argc = 1 + flag_count + 8 + 2 + 4;
    snprintf(expected[expected_count++], BT_MUT_PATH_MAX, "-I");
    snprintf(expected[expected_count++], BT_MUT_PATH_MAX, "%s/inputs", scratch_dir);
    static const char *const includes[3] = {"src", "tools", "tests"};
    for (int i = 0; i < 3; i++) {
        snprintf(expected[expected_count++], BT_MUT_PATH_MAX, "-I");
        snprintf(expected[expected_count++], BT_MUT_PATH_MAX, "%s/%s", g_root, includes[i]);
    }
    snprintf(expected[expected_count++], BT_MUT_PATH_MAX, "-isystem");
    snprintf(expected[expected_count++], BT_MUT_PATH_MAX, "%s/vendor/sqlite", g_root);
    snprintf(expected[expected_count++], BT_MUT_PATH_MAX, "-c");
    snprintf(expected[expected_count++], BT_MUT_PATH_MAX, "%s/fx_target.c", scratch_dir);
    snprintf(expected[expected_count++], BT_MUT_PATH_MAX, "-o");
    snprintf(expected[expected_count++], BT_MUT_PATH_MAX, "%s/fx_target.obj", scratch_dir);
    const bt_json_value *link_flags = bt_json_get(bt_json_get(g_config, "build"), "link_flags");
    size_t link_flag_count =
        link_flags != NULL && link_flags->type == BT_JSON_ARRAY ? link_flags->as.array.count : 0;
    size_t link_argc = 1 + flag_count + 1 + 2 * link_flag_count + 2 + 2;
    BT_CHECK_EQ(line_count, compile_argc + link_argc);
    for (size_t i = 0; i < expected_count; i++) {
        MUT_CHECK(strcmp(lines[1 + i], expected[i]) == 0, "compile argv %zu: got %s want %s", i,
                  lines[1 + i], expected[i]);
    }
    size_t link_start = compile_argc;
    MUT_CHECK(strcmp(lines[link_start], g_stub_exe) == 0, "link argv starts with stub, got %s",
              lines[link_start]);
    MUT_CHECK(strcmp(lines[link_start + 1], "-target") == 0, "link argv has -target");
    char *fuse = NULL;
    for (size_t i = link_start; i < line_count; i++) {
        if (strcmp(lines[i], "-fuse-ld=lld-link") == 0) {
            fuse = lines[i];
            break;
        }
    }
    BT_CHECK(fuse != NULL);
    BT_CHECK(strstr(lines[line_count - 1], "fx_test.exe") != NULL);
    free(text);
    bt_mut_build_free(&engine.build);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, tokenizer_kinds_and_positions) {
    const char *src = "int f(void) {\n    return 3;\n}\n#define X 4\n";
    bt_mut_tokens tokens;
    BT_CHECK_EQ(bt_mut_tokenize(src, strlen(src), &tokens), 0);
    long kw = find_token(&tokens, "int");
    long ident = find_token(&tokens, "f");
    long number = find_token(&tokens, "3");
    long ret = find_token(&tokens, "return");
    BT_CHECK(kw >= 0 && tokens.items[kw].kind == BT_MUT_TOK_KEYWORD);
    BT_CHECK_EQ(tokens.items[kw].line, 1ULL);
    BT_CHECK_EQ(tokens.items[kw].col, 1ULL);
    BT_CHECK(ident >= 0 && tokens.items[ident].kind == BT_MUT_TOK_IDENT);
    BT_CHECK(number >= 0 && tokens.items[number].kind == BT_MUT_TOK_NUMBER);
    BT_CHECK_EQ(tokens.items[number].line, 2ULL);
    BT_CHECK_EQ(tokens.items[number].col, 12ULL);
    BT_CHECK(ret >= 0 && tokens.items[ret].kind == BT_MUT_TOK_KEYWORD);
    BT_CHECK(tokens.count >= 1 && tokens.items[tokens.count - 1].kind == BT_MUT_TOK_PP);
    BT_CHECK(strncmp(bt_mut_token_text(&tokens, &tokens.items[tokens.count - 1]), "#define", 7) ==
             0);
    bt_mut_tokens_free(&tokens);
}

BT_TEST(mutate, unary_drop_mutants) {
    const char *src = "int f(int a) {\n    if (!a) {\n        a++;\n    }\n    --a;\n    return "
                      "a;\n}\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    size_t drops = 0;
    const bt_mut_mutant *bang = NULL;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].op == BT_MUT_UNARY_DROP) {
            drops++;
            if (strcmp(list.items[i].from, "!") == 0) {
                bang = &list.items[i];
            }
        }
    }
    BT_CHECK_EQ(drops, 3ULL);
    BT_CHECK(bang != NULL);
    char *applied = NULL;
    size_t applied_len = 0;
    if (bang != NULL) {
        BT_CHECK_EQ(bang->to[0], '\0');
        BT_CHECK_EQ(bt_mut_apply(src, strlen(src), bang, &applied, &applied_len), 0);
        BT_CHECK(strstr(applied, "if (a)") != NULL);
        free(applied);
    }
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, flooding_child_hits_the_deadline) {
    ensure_setup();
    char *argv[3] = {NULL, "selftest-flood", NULL};
    argv[0] = g_self_exe;
    bt_mut_proc proc;
    int rc = bt_mut_spawn(g_self_exe, argv, g_root, 2000, &proc);
    BT_CHECK_EQ(rc, -2);
    BT_CHECK_EQ(proc.timed_out, 1);
    bt_mut_proc_free(&proc);
}

BT_TEST(mutate, dep_inputs_land_under_scratch) {
    ensure_setup();
    bt_mut_engine engine;
    engine_for("inputs", 60000, NULL, &engine);
    size_t len = 0;
    char *text = fixture_text(&len);
    char source[BT_MUT_PATH_MAX];
    char include[BT_MUT_PATH_MAX] = "";
    BT_CHECK_EQ(
        bt_mut_prepare_scratch(&engine, "probe-0", FX_REL, text, len, NULL, source, include), 0);
    char copied[BT_MUT_PATH_MAX];
    snprintf(copied, BT_MUT_PATH_MAX, "%s/tests/unit/tools/fixtures/fx_target.h", include);
    FILE *header = fopen(copied, "rb");
    MUT_CHECK(header != NULL, "header not copied under %s", include);
    if (header != NULL) {
        fclose(header);
    }
    FILE *source_file = fopen(source, "rb");
    BT_CHECK(source_file != NULL);
    if (source_file != NULL) {
        fclose(source_file);
    }
    free(text);
    bt_mut_rmtree(engine.scratch_root);
    bt_mut_build_free(&engine.build);
}

BT_TEST(mutate, link_list_never_references_missing_objects) {
    ensure_setup();
    bt_mut_engine engine;
    engine_for("linklist", 60000, NULL, &engine);
    bt_mut_target target;
    int rc = bt_mut_target_from_repo(&engine, "src/core/alg.c", "core_alg_test", &target);
    BT_CHECK_EQ(rc, 0);
    if (rc == 0) {
        BT_CHECK(target.link_obj_count > 1);
        BT_CHECK(target.target_index < target.link_obj_count);
        for (size_t i = 0; i < target.link_obj_count; i++) {
            FILE *probe = fopen(target.link_objs[i], "rb");
            MUT_CHECK(probe != NULL, "link list references missing object %s", target.link_objs[i]);
            if (probe != NULL) {
                fclose(probe);
            }
        }
    }
    bt_mut_target_free(&target);
    bt_mut_build_free(&engine.build);
}

BT_TEST(mutate, spawn_error_leaves_the_row_pending) {
    bt_mut_mutant_list list;
    enumerate_fixture(&list);
    const bt_mut_mutant *mutant = bt_mut_find(&list, BT_MUT_NUMERIC_TWEAK, "0", "1", FX_LINE_ADD_0);
    BT_CHECK(mutant != NULL);
    bt_mut_engine engine;
    engine_for("spawnfail", 60000, "no-such-clang.exe", &engine);
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
    engine_for("stale", 60000, NULL, &engine);
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
    engine_for("exeswitch", 60000, NULL, &engine);
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

BT_TEST(mutate, for_headers_and_unbraced_bodies_delete_correctly) {
    const char *src = "int f(int v, int n) {\n    int r = 0;\n    for (r = 0; r < n; r++) {\n"
                      "        r = 9;\n    }\n    if (v)\n        r = 1;\n    return r;\n}\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    size_t deletions = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].op != BT_MUT_STATEMENT_DELETE) {
            continue;
        }
        deletions++;
        BT_CHECK(list.items[i].line == 4 || list.items[i].line == 7);
    }
    BT_CHECK_EQ(deletions, 2ULL);
    bt_mut_mutants_free(&list);
}

BT_TEST(mutate, literal_only_trace_calls_stay_undeleted) {
    const char *src = "int f(int v) {\n    int r = 0;\n    BT_TRACE_ENTER(\"unit\");\n"
                      "    BT_TRACE_FAIL(\"unit %d\", helper(r));\n    r = v + 1;\n    return "
                      "r;\n}\n";
    bt_mut_mutant_list list;
    BT_CHECK_EQ(bt_mut_enumerate("t.c", src, strlen(src), &list), 0);
    size_t deletions = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (list.items[i].op != BT_MUT_STATEMENT_DELETE) {
            continue;
        }
        deletions++;
        BT_CHECK(list.items[i].line == 4 || list.items[i].line == 5);
    }
    BT_CHECK_EQ(deletions, 2ULL);
    bt_mut_mutants_free(&list);
}

static int bt_spawn_probe(char *argv[3], bt_mut_proc *proc) {
    ensure_setup();
    return bt_mut_spawn(g_self_exe, argv, g_root, 30000, proc);
}

int bt_mutate_selftest(void) {
    bt_test_entry tests[] = {
        {
            "mutate.apply_compiles_for_every_operator",
            bt_test_mutate_apply_compiles_for_every_operator,
        },
        {"mutate.comparison_swap_variants", bt_test_mutate_comparison_swap_variants},
        {
            "mutate.engine_records_builderr_when_the_mutant_fails_to_compile",
            bt_test_mutate_engine_records_builderr_when_the_mutant_fails_to_compile,
        },
        {
            "mutate.engine_records_killed_for_a_guarded_test",
            bt_test_mutate_engine_records_killed_for_a_guarded_test,
        },
        {
            "mutate.engine_records_survived_for_unread_code",
            bt_test_mutate_engine_records_survived_for_unread_code,
        },
        {
            "mutate.engine_records_timeout_for_a_frozen_loop",
            bt_test_mutate_engine_records_timeout_for_a_frozen_loop,
        },
        {"mutate.engine_rejects_parallel_workers", bt_test_mutate_engine_rejects_parallel_workers},
        {
            "mutate.exe_switch_reruns_terminal_rows",
            bt_test_mutate_exe_switch_reruns_terminal_rows,
        },
        {"mutate.dep_inputs_land_under_scratch", bt_test_mutate_dep_inputs_land_under_scratch},
        {
            "mutate.link_list_never_references_missing_objects",
            bt_test_mutate_link_list_never_references_missing_objects,
        },
        {
            "mutate.flooding_child_hits_the_deadline",
            bt_test_mutate_flooding_child_hits_the_deadline,
        },
        {
            "mutate.for_headers_and_unbraced_bodies_delete_correctly",
            bt_test_mutate_for_headers_and_unbraced_bodies_delete_correctly,
        },
        {
            "mutate.ledger_resume_reruns_only_pending_rows",
            bt_test_mutate_ledger_resume_reruns_only_pending_rows,
        },
        {
            "mutate.literal_only_trace_calls_stay_undeleted",
            bt_test_mutate_literal_only_trace_calls_stay_undeleted,
        },
        {"mutate.mutant_ids_are_deterministic", bt_test_mutate_mutant_ids_are_deterministic},
        {"mutate.negation_guard_mutants", bt_test_mutate_negation_guard_mutants},
        {"mutate.numeric_variants_and_suffixes", bt_test_mutate_numeric_variants_and_suffixes},
        {
            "mutate.orphan_scratch_is_judged_by_creation_time",
            bt_test_mutate_orphan_scratch_is_judged_by_creation_time,
        },
        {
            "mutate.preprocessor_lines_stay_untouched",
            bt_test_mutate_preprocessor_lines_stay_untouched,
        },
        {
            "mutate.report_maps_verdicts_and_attributes_functions",
            bt_test_mutate_report_maps_verdicts_and_attributes_functions,
        },
        {"mutate.report_refuses_pending_rows", bt_test_mutate_report_refuses_pending_rows},
        {
            "mutate.spawn_error_leaves_the_row_pending",
            bt_test_mutate_spawn_error_leaves_the_row_pending,
        },
        {
            "mutate.stale_ledger_rows_are_dropped",
            bt_test_mutate_stale_ledger_rows_are_dropped,
        },
        {
            "mutate.statement_delete_skips_declarations_and_control",
            bt_test_mutate_statement_delete_skips_declarations_and_control,
        },
        {
            "mutate.stub_assembly_matches_the_bt_py_contract",
            bt_test_mutate_stub_assembly_matches_the_bt_py_contract,
        },
        {"mutate.tokenizer_kinds_and_positions", bt_test_mutate_tokenizer_kinds_and_positions},
        {"mutate.unary_drop_mutants", bt_test_mutate_unary_drop_mutants},
    };
    return bt_harness_run("mutate", tests, sizeof tests / sizeof tests[0]);
}
