#include "mutate/mutate.h"
#include "unit/tools/spawn_util.h"

#include <string.h>

#include "common/bt_harness.h"
#include "common/json.h"

BT_TEST(mutate_driver, selftest_passes) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "mutate.exe");
    const char *argv[] = {"selftest", NULL};
    char out[32768];
    char err[8192];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 110000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(strstr(out, "suite mutate: 29 passed, 0 failed") != NULL);
}

BT_TEST(mutate_driver, usage_without_args_exits_2) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "mutate.exe");
    const char *argv[] = {NULL};
    char out[4096];
    char err[4096];
    int code = -1;
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "usage") != NULL);
}

BT_TEST(mutate_driver, list_missing_file_exits_2) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "mutate.exe");
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    const char *argv[] = {"list", "src/does-not-exist.c", NULL};
    char out[4096];
    char err[4096];
    int code = -1;
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 2);
    BT_CHECK(strstr(err, "cannot read") != NULL);
}

BT_TEST(mutate_driver, list_enumerates_a_fixture) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "mutate.exe");
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    const char *argv[] = {"list", "tests/unit/tools/fixtures/fx_target.c", NULL};
    char out[16384];
    char err[4096];
    int code = -1;
    int rc = bt_spawn_capture(exe, argv, root, 30000, &code, out, sizeof out, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 0);
    BT_CHECK(strstr(out, "NumericTweak") != NULL);
    BT_CHECK(strstr(out, "ComparisonSwap") != NULL);
    BT_CHECK(strstr(out, "ConditionNegation") != NULL);
    BT_CHECK(strstr(out, "UnaryDrop") != NULL);
    BT_CHECK(strstr(out, "StatementDelete") != NULL);
}

#define UNION_FX_REL "tests/unit/tools/fixtures/fx_target.c"
#define UNION_HEADER "id\tfile\tline\tcol\top\tfrom\tto\tstatus\tattempts\tlast_run\ttest_exe\n"
#define UNION_SUITE_A "core_alg_test"
#define UNION_SUITE_B "core_scheme_test"

static char g_union_root[BT_MUT_PATH_MAX];
static char g_union_base[BT_MUT_PATH_MAX];

static void union_dir_for(const char *slot, char out[BT_MUT_PATH_MAX]) {
    if (g_union_base[0] == '\0') {
        bt_test_repo_root(g_union_root);
        snprintf(g_union_base, BT_MUT_PATH_MAX, "%s/build/test-tmp/mutate-union", g_union_root);
    }
    snprintf(out, BT_MUT_PATH_MAX, "%s/%s", g_union_base, slot);
    bt_test_mkdirs(out);
}

static void union_ledger_write(const char *slot, const char *name, const char *body) {
    char dir[BT_MUT_PATH_MAX];
    union_dir_for(slot, dir);
    char path[BT_MUT_PATH_MAX];
    snprintf(path, BT_MUT_PATH_MAX, "%s/%s", dir, name);
    BT_CHECK_EQ(bt_mut_write_file(path, body, strlen(body)), 0);
}

static size_t union_row(char *buffer, size_t cap, size_t used, const char *id, size_t line,
                        size_t col, const char *op, const char *from, const char *to,
                        const char *status) {
    return used + (size_t)snprintf(buffer + used, cap - used,
                                   "%s\t" UNION_FX_REL "\t%zu\t%zu\t%s\t%s\t%s\t%s\t1\t"
                                   "2026-10-29T10:00:00\tfx_test\n",
                                   id, line, col, op, from, to, status);
}

static bt_json_value *union_json(const char *path) {
    char *text = NULL;
    size_t len = 0;
    if (bt_mut_read_file(path, &text, &len) != 0) {
        return NULL;
    }
    bt_json_value *root = NULL;
    bt_json_error error;
    if (bt_json_parse(text, len, &root, &error) != 0) {
        free(text);
        return NULL;
    }
    free(text);
    return root;
}

static unsigned long long union_counter(const bt_json_value *root, const char *name) {
    const bt_json_value *value = bt_json_get(root, name);
    if (value == NULL || value->type != BT_JSON_NUMBER) {
        return (unsigned long long)-1;
    }
    return (unsigned long long)value->as.number;
}

static const char *union_scenario_string(const bt_json_value *root, size_t index) {
    const bt_json_value *outcomes = bt_json_get(root, "outcomes");
    if (outcomes == NULL || outcomes->type != BT_JSON_ARRAY || index >= outcomes->as.array.count) {
        return NULL;
    }
    const bt_json_value *scenario = bt_json_get(bt_json_at(outcomes, index), "scenario");
    if (scenario == NULL || scenario->type != BT_JSON_STRING) {
        return NULL;
    }
    return scenario->as.string;
}

static const char *union_summary_at(const bt_json_value *root, size_t index) {
    const bt_json_value *outcomes = bt_json_get(root, "outcomes");
    if (outcomes == NULL || outcomes->type != BT_JSON_ARRAY || index >= outcomes->as.array.count) {
        return NULL;
    }
    const bt_json_value *summary = bt_json_get(bt_json_at(outcomes, index), "summary");
    if (summary == NULL || summary->type != BT_JSON_STRING) {
        return NULL;
    }
    return summary->as.string;
}

static const char *union_mutant_field(const bt_json_value *root, size_t index, const char *field) {
    const bt_json_value *outcomes = bt_json_get(root, "outcomes");
    if (outcomes == NULL || outcomes->type != BT_JSON_ARRAY || index >= outcomes->as.array.count) {
        return NULL;
    }
    const bt_json_value *scenario = bt_json_get(bt_json_at(outcomes, index), "scenario");
    if (scenario == NULL || scenario->type != BT_JSON_OBJECT) {
        return NULL;
    }
    const bt_json_value *mutant = bt_json_get(scenario, "Mutant");
    if (mutant == NULL || mutant->type != BT_JSON_OBJECT) {
        return NULL;
    }
    if (strcmp(field, "function") == 0) {
        const bt_json_value *function = bt_json_get(mutant, "function");
        if (function == NULL || function->type != BT_JSON_OBJECT) {
            return NULL;
        }
        const bt_json_value *name = bt_json_get(function, "function_name");
        return name != NULL && name->type == BT_JSON_STRING ? name->as.string : NULL;
    }
    const bt_json_value *leaf = bt_json_get(mutant, field);
    return leaf != NULL && leaf->type == BT_JSON_STRING ? leaf->as.string : NULL;
}

BT_TEST(mutate_union, killed_in_one_suite_and_survived_in_a_sibling_is_one_caught_site) {
    char dir[BT_MUT_PATH_MAX];
    union_dir_for("kill-surv", dir);
    char body[4096];
    size_t used = (size_t)snprintf(body, sizeof body, "%s", UNION_HEADER);
    used = union_row(body, sizeof body, used, "1111111111111111", 6, 20, "NumericTweak", "0", "1",
                     "killed");
    union_ledger_write("kill-surv", "ledger-" UNION_SUITE_A ".tsv", body);
    used = (size_t)snprintf(body, sizeof body, "%s", UNION_HEADER);
    used = union_row(body, sizeof body, used, "2222222222222222", 6, 20, "NumericTweak", "0", "1",
                     "survived");
    union_ledger_write("kill-surv", "ledger-" UNION_SUITE_B ".tsv", body);
    char out[BT_MUT_PATH_MAX];
    snprintf(out, BT_MUT_PATH_MAX, "%s/outcomes.json", dir);
    const char *inputs[] = {dir};
    BT_CHECK_EQ(bt_mut_union_report(inputs, 1, g_union_root, out), 0);
    bt_json_value *root = union_json(out);
    BT_CHECK(root != NULL);
    if (root != NULL) {
        BT_CHECK_EQ(union_counter(root, "total_mutants"), 1ULL);
        BT_CHECK_EQ(union_counter(root, "caught"), 1ULL);
        BT_CHECK_EQ(union_counter(root, "missed"), 0ULL);
        BT_CHECK_EQ(union_counter(root, "timeout"), 0ULL);
        BT_CHECK_EQ(union_counter(root, "unviable"), 0ULL);
        const bt_json_value *outcomes = bt_json_get(root, "outcomes");
        BT_CHECK(outcomes != NULL && outcomes->type == BT_JSON_ARRAY);
        if (outcomes != NULL && outcomes->type == BT_JSON_ARRAY) {
            BT_CHECK_EQ(outcomes->as.array.count, 2ULL);
            const char *baseline = union_scenario_string(root, 0);
            BT_CHECK(baseline != NULL && strcmp(baseline, "Baseline") == 0);
            const char *summary = union_summary_at(root, 1);
            BT_CHECK(summary != NULL && strcmp(summary, "CaughtMutant") == 0);
            const char *function = union_mutant_field(root, 1, "function");
            BT_CHECK(function != NULL && strcmp(function, "fx_add") == 0);
            const char *genre = union_mutant_field(root, 1, "genre");
            BT_CHECK(genre != NULL && strcmp(genre, "NumericTweak") == 0);
            const char *replacement = union_mutant_field(root, 1, "replacement");
            BT_CHECK(replacement != NULL && strcmp(replacement, "1") == 0);
            const char *file = union_mutant_field(root, 1, "file");
            BT_CHECK(file != NULL && strcmp(file, UNION_FX_REL) == 0);
        }
        bt_json_free(root);
    }
}

BT_TEST(mutate_union, all_builderr_rows_resolve_unviable) {
    char dir[BT_MUT_PATH_MAX];
    union_dir_for("unviable", dir);
    char body[4096];
    size_t used = (size_t)snprintf(body, sizeof body, "%s", UNION_HEADER);
    used = union_row(body, sizeof body, used, "1111111111111111", 27, 5, "StatementDelete", "r", "",
                     "builderr");
    union_ledger_write("unviable", "ledger-" UNION_SUITE_A ".tsv", body);
    used = (size_t)snprintf(body, sizeof body, "%s", UNION_HEADER);
    used = union_row(body, sizeof body, used, "2222222222222222", 27, 5, "StatementDelete", "r", "",
                     "builderr");
    union_ledger_write("unviable", "ledger-" UNION_SUITE_B ".tsv", body);
    char first[BT_MUT_PATH_MAX];
    char second[BT_MUT_PATH_MAX];
    char out[BT_MUT_PATH_MAX];
    snprintf(first, BT_MUT_PATH_MAX, "%s/ledger-" UNION_SUITE_A ".tsv", dir);
    snprintf(second, BT_MUT_PATH_MAX, "%s/ledger-" UNION_SUITE_B ".tsv", dir);
    snprintf(out, BT_MUT_PATH_MAX, "%s/outcomes.json", dir);
    const char *inputs[] = {first, second};
    BT_CHECK_EQ(bt_mut_union_report(inputs, 2, g_union_root, out), 0);
    bt_json_value *root = union_json(out);
    BT_CHECK(root != NULL);
    if (root != NULL) {
        BT_CHECK_EQ(union_counter(root, "total_mutants"), 1ULL);
        BT_CHECK_EQ(union_counter(root, "unviable"), 1ULL);
        BT_CHECK_EQ(union_counter(root, "caught"), 0ULL);
        const char *summary = union_summary_at(root, 1);
        BT_CHECK(summary != NULL && strcmp(summary, "Unviable") == 0);
        const char *replacement = union_mutant_field(root, 1, "replacement");
        BT_CHECK(replacement != NULL && strcmp(replacement, "") == 0);
        bt_json_free(root);
    }
}

BT_TEST(mutate_union, timeout_plus_kill_resolves_killed) {
    char dir[BT_MUT_PATH_MAX];
    union_dir_for("time-kill", dir);
    char body[4096];
    size_t used = (size_t)snprintf(body, sizeof body, "%s", UNION_HEADER);
    used = union_row(body, sizeof body, used, "1111111111111111", 6, 20, "NumericTweak", "0", "1",
                     "timeout");
    union_ledger_write("time-kill", "ledger-" UNION_SUITE_A ".tsv", body);
    used = (size_t)snprintf(body, sizeof body, "%s", UNION_HEADER);
    used = union_row(body, sizeof body, used, "2222222222222222", 6, 20, "NumericTweak", "0", "1",
                     "killed");
    union_ledger_write("time-kill", "ledger-" UNION_SUITE_B ".tsv", body);
    char out[BT_MUT_PATH_MAX];
    snprintf(out, BT_MUT_PATH_MAX, "%s/outcomes.json", dir);
    const char *inputs[] = {dir};
    BT_CHECK_EQ(bt_mut_union_report(inputs, 1, g_union_root, out), 0);
    bt_json_value *root = union_json(out);
    BT_CHECK(root != NULL);
    if (root != NULL) {
        BT_CHECK_EQ(union_counter(root, "total_mutants"), 1ULL);
        BT_CHECK_EQ(union_counter(root, "caught"), 1ULL);
        BT_CHECK_EQ(union_counter(root, "timeout"), 0ULL);
        const char *summary = union_summary_at(root, 1);
        BT_CHECK(summary != NULL && strcmp(summary, "CaughtMutant") == 0);
        bt_json_free(root);
    }
}

BT_TEST(mutate_union, timeout_plus_survived_resolves_timeout) {
    char dir[BT_MUT_PATH_MAX];
    union_dir_for("time-surv", dir);
    char body[4096];
    size_t used = (size_t)snprintf(body, sizeof body, "%s", UNION_HEADER);
    used = union_row(body, sizeof body, used, "1111111111111111", 6, 20, "NumericTweak", "0", "1",
                     "timeout");
    union_ledger_write("time-surv", "ledger-" UNION_SUITE_A ".tsv", body);
    used = (size_t)snprintf(body, sizeof body, "%s", UNION_HEADER);
    used = union_row(body, sizeof body, used, "2222222222222222", 6, 20, "NumericTweak", "0", "1",
                     "survived");
    union_ledger_write("time-surv", "ledger-" UNION_SUITE_B ".tsv", body);
    char out[BT_MUT_PATH_MAX];
    snprintf(out, BT_MUT_PATH_MAX, "%s/outcomes.json", dir);
    const char *inputs[] = {dir};
    BT_CHECK_EQ(bt_mut_union_report(inputs, 1, g_union_root, out), 0);
    bt_json_value *root = union_json(out);
    BT_CHECK(root != NULL);
    if (root != NULL) {
        BT_CHECK_EQ(union_counter(root, "total_mutants"), 1ULL);
        BT_CHECK_EQ(union_counter(root, "timeout"), 1ULL);
        BT_CHECK_EQ(union_counter(root, "missed"), 0ULL);
        const char *summary = union_summary_at(root, 1);
        BT_CHECK(summary != NULL && strcmp(summary, "Timeout") == 0);
        bt_json_free(root);
    }
}

BT_TEST(mutate_union, pending_row_refuses_naming_suite_and_site) {
    char dir[BT_MUT_PATH_MAX];
    union_dir_for("pending", dir);
    char body[4096];
    size_t used = (size_t)snprintf(body, sizeof body, "%s", UNION_HEADER);
    used = union_row(body, sizeof body, used, "1111111111111111", 6, 20, "NumericTweak", "0", "1",
                     "killed");
    union_ledger_write("pending", "ledger-" UNION_SUITE_A ".tsv", body);
    used = (size_t)snprintf(body, sizeof body, "%s", UNION_HEADER);
    used = union_row(body, sizeof body, used, "2222222222222222", 6, 20, "NumericTweak", "0", "1",
                     "killed");
    used = union_row(body, sizeof body, used, "3333333333333333", 36, 13, "NumericTweak", "3", "4",
                     "pending");
    union_ledger_write("pending", "ledger-" UNION_SUITE_B ".tsv", body);
    char out[BT_MUT_PATH_MAX];
    snprintf(out, BT_MUT_PATH_MAX, "%s/outcomes.json", dir);
    const char *inputs[] = {dir};
    BT_CHECK_EQ(bt_mut_union_report(inputs, 1, g_union_root, out), 1);
    const char *error = bt_mut_union_last_error();
    BT_CHECK(strstr(error, UNION_SUITE_B) != NULL);
    BT_CHECK(strstr(error, UNION_FX_REL) != NULL);
    BT_CHECK(strstr(error, "NumericTweak") != NULL);
    BT_CHECK(strstr(error, "3->4") != NULL);
    FILE *leaked = fopen(out, "rb");
    BT_CHECK(leaked == NULL);
    if (leaked != NULL) {
        fclose(leaked);
    }
}

BT_TEST(mutate_union, function_names_come_from_the_site_line) {
    char dir[BT_MUT_PATH_MAX];
    union_dir_for("functions", dir);
    char body[4096];
    size_t used = (size_t)snprintf(body, sizeof body, "%s", UNION_HEADER);
    used = union_row(body, sizeof body, used, "1111111111111111", 6, 20, "NumericTweak", "0", "1",
                     "killed");
    used = union_row(body, sizeof body, used, "2222222222222222", 36, 13, "NumericTweak", "3", "4",
                     "killed");
    union_ledger_write("functions", "ledger-" UNION_SUITE_A ".tsv", body);
    char ledger[BT_MUT_PATH_MAX];
    char out[BT_MUT_PATH_MAX];
    snprintf(ledger, BT_MUT_PATH_MAX, "%s/ledger-" UNION_SUITE_A ".tsv", dir);
    snprintf(out, BT_MUT_PATH_MAX, "%s/outcomes.json", dir);
    const char *inputs[] = {ledger};
    BT_CHECK_EQ(bt_mut_union_report(inputs, 1, g_union_root, out), 0);
    bt_json_value *root = union_json(out);
    BT_CHECK(root != NULL);
    if (root != NULL) {
        BT_CHECK_EQ(union_counter(root, "total_mutants"), 2ULL);
        const bt_json_value *outcomes = bt_json_get(root, "outcomes");
        BT_CHECK(outcomes != NULL && outcomes->type == BT_JSON_ARRAY);
        if (outcomes != NULL && outcomes->type == BT_JSON_ARRAY) {
            BT_CHECK_EQ(outcomes->as.array.count, 3ULL);
            const char *first = union_mutant_field(root, 1, "function");
            BT_CHECK(first != NULL && strcmp(first, "fx_add") == 0);
            const char *second = union_mutant_field(root, 2, "function");
            BT_CHECK(second != NULL && strcmp(second, "fx_unused") == 0);
        }
        bt_json_free(root);
    }
}

BT_TEST(mutate_union, counters_count_distinct_sites_and_sum) {
    char dir[BT_MUT_PATH_MAX];
    union_dir_for("counters", dir);
    char body[4096];
    size_t used = (size_t)snprintf(body, sizeof body, "%s", UNION_HEADER);
    used = union_row(body, sizeof body, used, "1111111111111111", 6, 20, "NumericTweak", "0", "1",
                     "killed");
    used = union_row(body, sizeof body, used, "2222222222222222", 36, 13, "NumericTweak", "3", "4",
                     "survived");
    used = union_row(body, sizeof body, used, "3333333333333333", 19, 9, "UnaryDrop", "!", "",
                     "timeout");
    used = union_row(body, sizeof body, used, "4444444444444444", 27, 5, "StatementDelete", "r", "",
                     "builderr");
    union_ledger_write("counters", "ledger-" UNION_SUITE_A ".tsv", body);
    used = (size_t)snprintf(body, sizeof body, "%s", UNION_HEADER);
    used = union_row(body, sizeof body, used, "5555555555555555", 6, 20, "NumericTweak", "0", "1",
                     "killed");
    used = union_row(body, sizeof body, used, "6666666666666666", 36, 13, "NumericTweak", "3", "4",
                     "builderr");
    used = union_row(body, sizeof body, used, "7777777777777777", 19, 9, "UnaryDrop", "!", "",
                     "survived");
    used = union_row(body, sizeof body, used, "8888888888888888", 27, 5, "StatementDelete", "r", "",
                     "builderr");
    union_ledger_write("counters", "ledger-" UNION_SUITE_B ".tsv", body);
    char out[BT_MUT_PATH_MAX];
    snprintf(out, BT_MUT_PATH_MAX, "%s/outcomes.json", dir);
    const char *inputs[] = {dir};
    BT_CHECK_EQ(bt_mut_union_report(inputs, 1, g_union_root, out), 0);
    bt_json_value *root = union_json(out);
    BT_CHECK(root != NULL);
    if (root != NULL) {
        unsigned long long total = union_counter(root, "total_mutants");
        unsigned long long caught = union_counter(root, "caught");
        unsigned long long missed = union_counter(root, "missed");
        unsigned long long timeout = union_counter(root, "timeout");
        unsigned long long unviable = union_counter(root, "unviable");
        BT_CHECK_EQ(total, 4ULL);
        BT_CHECK_EQ(caught, 1ULL);
        BT_CHECK_EQ(missed, 1ULL);
        BT_CHECK_EQ(timeout, 1ULL);
        BT_CHECK_EQ(unviable, 1ULL);
        BT_CHECK_EQ(caught + missed + timeout + unviable, total);
        const bt_json_value *outcomes = bt_json_get(root, "outcomes");
        BT_CHECK(outcomes != NULL && outcomes->type == BT_JSON_ARRAY);
        if (outcomes != NULL && outcomes->type == BT_JSON_ARRAY) {
            BT_CHECK_EQ(outcomes->as.array.count, total + 1ULL);
        }
        bt_json_free(root);
    }
}

BT_TEST(mutate_union, missing_ledger_input_refuses) {
    char dir[BT_MUT_PATH_MAX];
    union_dir_for("missing", dir);
    char out[BT_MUT_PATH_MAX];
    snprintf(out, BT_MUT_PATH_MAX, "%s/outcomes.json", dir);
    const char *inputs[] = {"no-such-ledger-dir"};
    BT_CHECK_EQ(bt_mut_union_report(inputs, 1, g_union_root, out), 1);
    BT_CHECK(strstr(bt_mut_union_last_error(), "no-such-ledger-dir") != NULL);
    BT_CHECK(strstr(bt_mut_union_last_error(), "not found") != NULL);
}

BT_TEST(mutate_union, directory_without_ledgers_refuses) {
    char dir[BT_MUT_PATH_MAX];
    union_dir_for("empty", dir);
    char out[BT_MUT_PATH_MAX];
    snprintf(out, BT_MUT_PATH_MAX, "%s/outcomes.json", dir);
    const char *inputs[] = {dir};
    BT_CHECK_EQ(bt_mut_union_report(inputs, 1, g_union_root, out), 1);
    BT_CHECK(strstr(bt_mut_union_last_error(), "no ledger") != NULL);
    BT_CHECK(strstr(bt_mut_union_last_error(), "empty") != NULL);
}

BT_TEST(mutate_union, ledgers_without_rows_refuse) {
    char dir[BT_MUT_PATH_MAX];
    union_dir_for("norows", dir);
    union_ledger_write("norows", "ledger-" UNION_SUITE_A ".tsv", UNION_HEADER);
    char ledger[BT_MUT_PATH_MAX];
    char out[BT_MUT_PATH_MAX];
    snprintf(ledger, BT_MUT_PATH_MAX, "%s/ledger-" UNION_SUITE_A ".tsv", dir);
    snprintf(out, BT_MUT_PATH_MAX, "%s/outcomes.json", dir);
    const char *inputs[] = {ledger};
    BT_CHECK_EQ(bt_mut_union_report(inputs, 1, g_union_root, out), 1);
    BT_CHECK(strstr(bt_mut_union_last_error(), "no ledger rows") != NULL);
}

BT_TEST(mutate_stage, run_exe_lands_sanitized_inside_the_repo_build_tree) {
    char root[BT_MUT_PATH_MAX];
    bt_test_repo_root(root);
    char dir[BT_MUT_PATH_MAX];
    snprintf(dir, BT_MUT_PATH_MAX, "%s/build/test-tmp/mutate-stage", root);
    bt_test_mkdirs(dir);
    char fake[BT_MUT_PATH_MAX];
    snprintf(fake, BT_MUT_PATH_MAX, "%s/slot.exe", dir);
    BT_CHECK_EQ(bt_mut_write_file(fake, "abc", 3), 0);
    char staged[BT_MUT_PATH_MAX];
    BT_CHECK_EQ(bt_mut_stage_run_exe(root, "fx_test", "ba:se/0", fake, staged), 0);
    BT_CHECK(strstr(staged, "/build/bin/mutate-run/fx_test-ba_se_0.exe") != NULL);
    char *text = NULL;
    size_t len = 0;
    BT_CHECK_EQ(bt_mut_read_file(staged, &text, &len), 0);
    BT_CHECK(len == 3 && memcmp(text, "abc", 3) == 0);
    free(text);
    BT_CHECK_EQ(bt_mut_write_file(fake, "zyxw", 4), 0);
    BT_CHECK_EQ(bt_mut_stage_run_exe(root, "fx_test", "ba:se/0", fake, staged), 0);
    text = NULL;
    BT_CHECK_EQ(bt_mut_read_file(staged, &text, &len), 0);
    BT_CHECK(len == 4 && memcmp(text, "zyxw", 4) == 0);
    free(text);
    (void)remove(staged);
}

BT_TEST(mutate_stage, scratch_rooted_run_exe_stages_under_the_workspace) {
    char root[BT_MUT_PATH_MAX];
    bt_test_repo_root(root);
    char temp[BT_MUT_PATH_MAX];
    GetTempPathA(BT_MUT_PATH_MAX, temp);
    size_t len = strlen(temp);
    while (len > 0 && (temp[len - 1] == '\\' || temp[len - 1] == '/')) {
        len--;
    }
    temp[len] = '\0';
    char planted[BT_MUT_PATH_MAX];
    snprintf(planted, BT_MUT_PATH_MAX, "%s/bt-mut-stage-unit", temp);
    bt_mut_rmtree(planted);
    char dir[BT_MUT_PATH_MAX];
    snprintf(dir, BT_MUT_PATH_MAX, "%s/core/0", planted);
    bt_test_mkdirs(dir);
    char built[BT_MUT_PATH_MAX];
    snprintf(built, BT_MUT_PATH_MAX, "%s/core_alg_test.exe", dir);
    BT_CHECK_EQ(bt_mut_write_file(built, "abc", 3), 0);
    char staged[BT_MUT_PATH_MAX];
    BT_CHECK_EQ(bt_mut_stage_run_exe(root, "core_alg_test", "core/0", built, staged), 0);
    char expected[BT_MUT_PATH_MAX];
    snprintf(expected, BT_MUT_PATH_MAX, "%s/build/bin/mutate-run/core_alg_test-core_0.exe", root);
    BT_CHECK(strcmp(staged, expected) == 0);
    BT_CHECK(strstr(staged, planted) != staged);
    char *text = NULL;
    size_t body_len = 0;
    BT_CHECK_EQ(bt_mut_read_file(staged, &text, &body_len), 0);
    BT_CHECK(body_len == 3 && memcmp(text, "abc", 3) == 0);
    free(text);
    (void)remove(staged);
    bt_mut_rmtree(planted);
}
