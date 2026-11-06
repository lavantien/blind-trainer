#include "ga-run/lib.h"
#include "ga-run/out.h"

#include "common/alloc.h"
#include "common/bt_harness.h"
#include "common/json.h"
#include "common/paths.h"
#include "common/strbuf.h"
#include "data/paths.h"
#include "ga/dist.h"
#include "ga/hash.h"
#include "ga/inventory.h"
#include "scheme/pairs.h"

#include <direct.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *GA_OOM =
    "{\"seed\": 77, \"population\": 30, \"generations\": 20, \"elite\": 2, \"tournament\": 2, "
    "\"pool_top_k\": 1600, \"weights\": {\"confusion\": 1.0, \"flow\": 0.5, \"complexity\": 0.1}, "
    "\"mutation\": {\"swap\": 0.15, \"replace\": 0.05}, \"islands\": {\"count\": 1, "
    "\"migrate_every\": 0, \"migrate_count\": 1}, \"adaptive_mutation\": {\"stagnation_threshold\": "
    "0, \"swap_scale\": 1.0, \"replace_scale\": 1.0}, \"elite_carry\": {\"policy\": \"count\", "
    "\"fraction\": 0.0}, \"restart\": {\"diversity_threshold\": 0.0, \"inject_fraction\": 0.5}}";

static void scratch_dir(const char *name, char out[BT_PATH_MAX]) {
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    snprintf(out, BT_PATH_MAX, "%s/build/test-tmp/ga-run-oom/%s", root, name);
    for (char *cursor = out; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
    _mkdir("build");
    _mkdir("build/test-tmp");
    _mkdir("build/test-tmp/ga-run-oom");
    _mkdir(out);
}

static void scratch_file(const char *dir, const char *name, const char *bytes, size_t len) {
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_ABORT("cannot write %s", path);
    }
    fwrite(bytes, 1, len, file);
    fclose(file);
}

static char *read_bytes(const char *path, size_t *len) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *bytes = malloc((size_t)size + 1);
    if (bytes != NULL) {
        *len = fread(bytes, 1, (size_t)size, file);
        bytes[*len] = '\0';
    }
    fclose(file);
    return bytes;
}

static void copy_repo_freq_json(const char *data_ga) {
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    char freq[BT_PATH_MAX];
    snprintf(freq, sizeof freq, "%s/data/ga/freq.json", root);
    size_t len = 0;
    char *bytes = read_bytes(freq, &len);
    if (bytes == NULL) {
        BT_ABORT("cannot read %s, run python bt.py memofreq first", freq);
    }
    scratch_file(data_ga, "freq.json", bytes, len);
    free(bytes);
}

static void make_tiny_env(const char *name, char config_path[BT_PATH_MAX]) {
    char data_ga[BT_PATH_MAX];
    char artifacts[BT_PATH_MAX];
    char artifacts_name[128];
    scratch_dir(name, data_ga);
    snprintf(artifacts_name, sizeof artifacts_name, "%s-artifacts", name);
    scratch_dir(artifacts_name, artifacts);
    char paths[BT_PATH_MAX * 2 + 128];
    snprintf(paths, sizeof paths,
             "{\"ref_doc\": \"ref_doc\", \"data_vendor\": \"data/vendor\", \"data_ga\": \"%s\", "
             "\"artifacts\": \"%s\"}",
             data_ga, artifacts);
    char text[8192];
    snprintf(text, sizeof text, "{\"paths\": %s, \"ga\": %s}", paths, GA_OOM);
    scratch_file(data_ga, "config.json", text, strlen(text));
    copy_repo_freq_json(data_ga);
    snprintf(config_path, BT_PATH_MAX, "%s/config.json", data_ga);
}

static void with_config_env(const char *path, void (*fn)(void *user), void *user) {
    const char *prior = getenv("BT_CONFIG");
    char saved[BT_PATH_MAX];
    snprintf(saved, sizeof saved, "%s", prior != NULL ? prior : "");
    _putenv_s("BT_CONFIG", path);
    fn(user);
    if (saved[0] != '\0') {
        _putenv_s("BT_CONFIG", saved);
    } else {
        _putenv("BT_CONFIG=");
    }
}

static void check_budget_walk(const int *results, size_t count) {
    size_t first_ok = count;
    for (size_t i = 0; i < count; i++) {
        if (results[i] == 0) {
            first_ok = i;
            break;
        }
    }
    if (first_ok == count) {
        bt_harness_fail(__FILE__, __LINE__, "no budget in the walk reached success");
        return;
    }
    for (size_t i = 0; i < first_ok; i++) {
        if (results[i] == 0) {
            bt_harness_fail(__FILE__, __LINE__, "budget %zu succeeded before the first full budget",
                            i);
        }
    }
    for (size_t i = first_ok; i < count; i++) {
        if (results[i] != 0) {
            bt_harness_fail(__FILE__, __LINE__, "budget %zu failed after the first full budget", i);
        }
    }
}

typedef struct {
    int rc;
    size_t ok;
} armed_ctx;

static void load_armed_once(void *user) {
    armed_ctx *ctx = user;
    bt_gr_run_inputs inputs;
    bt_gr_offenders offenders = {0};
    bt_alloc_fail_arm(ctx->ok);
    ctx->rc = bt_gr_load_run_inputs(&inputs, &offenders);
    bt_alloc_fail_disarm();
    if (ctx->rc == 0) {
        bt_gr_run_inputs_free(&inputs);
    }
    bt_gr_offenders_free(&offenders);
}

static int load_armed(const char *config_path, size_t ok) {
    armed_ctx ctx = {.rc = 1, .ok = ok};
    with_config_env(config_path, load_armed_once, &ctx);
    return ctx.rc;
}

static void run_all_armed_once(void *user) {
    armed_ctx *ctx = user;
    bt_gr_offenders offenders = {0};
    bt_alloc_fail_arm(ctx->ok);
    ctx->rc = bt_gr_run_all(&offenders);
    bt_alloc_fail_disarm();
    bt_gr_offenders_free(&offenders);
}

static int run_all_armed(const char *config_path, size_t ok) {
    armed_ctx ctx = {.rc = 1, .ok = ok};
    with_config_env(config_path, run_all_armed_once, &ctx);
    return ctx.rc;
}

static size_t smallest_success(const char *config_path, int (*armed)(const char *, size_t)) {
    size_t hi = 1;
    while (armed(config_path, hi) != 0 && hi < (1U << 24U)) {
        hi *= 2;
    }
    if (armed(config_path, hi) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "the pipeline never succeeds under a 16M budget");
        return 0;
    }
    size_t lo = 1;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (armed(config_path, mid) == 0) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    return lo;
}

static size_t load_budget_cache;

BT_TEST(ga_run_oom, load_fails_closed_across_the_entry_allocations) {
    char config_path[BT_PATH_MAX];
    make_tiny_env("walk", config_path);
    for (size_t k = 0; k < 96; k++) {
        if (load_armed(config_path, k) != -1) {
            bt_harness_fail(__FILE__, __LINE__, "budget %zu let the load succeed", k);
            return;
        }
    }
}

BT_TEST(ga_run_oom, load_tail_allocations_fail_closed_at_each_final_site) {
    char config_path[BT_PATH_MAX];
    make_tiny_env("tail", config_path);
    size_t total = smallest_success(config_path, load_armed);
    if (total <= 8) {
        bt_harness_fail(__FILE__, __LINE__, "load allocation budget %zu is implausibly small",
                        total);
        return;
    }
    load_budget_cache = total;
    for (size_t delta = 1; delta <= 4; delta++) {
        if (load_armed(config_path, total - delta) != -1) {
            bt_harness_fail(__FILE__, __LINE__, "budget %zu below full did not fail closed",
                            total - delta);
        }
    }
    if (load_armed(config_path, total) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "full budget %zu did not succeed", total);
    }
}

BT_TEST(ga_run_oom, run_all_write_stage_fails_closed_at_each_final_site) {
    char config_path[BT_PATH_MAX];
    make_tiny_env("write", config_path);
    size_t floor = load_budget_cache;
    if (floor == 0) {
        floor = smallest_success(config_path, load_armed);
        if (floor == 0) {
            return;
        }
    }
    if (run_all_armed(config_path, floor) == 0) {
        bt_harness_fail(__FILE__, __LINE__, "run_all succeeded at the bare load budget %zu", floor);
        return;
    }
    size_t hi = floor + 1024;
    if (run_all_armed(config_path, hi) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "run_all fails above load budget plus 1024");
        return;
    }
    size_t lo = floor;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (run_all_armed(config_path, mid) == 0) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    if (lo <= floor || lo > floor + 1024) {
        bt_harness_fail(__FILE__, __LINE__, "run_all budget %zu sits outside the expected band",
                        lo);
        return;
    }
    for (size_t delta = 1; delta <= 10; delta++) {
        if (run_all_armed(config_path, lo - delta) != -1) {
            bt_harness_fail(__FILE__, __LINE__, "budget %zu below full did not fail closed",
                            lo - delta);
            return;
        }
    }
    if (run_all_armed(config_path, lo) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "full budget %zu did not succeed", lo);
    }
}

BT_TEST(ga_run_oom, offenders_push_fails_closed_at_growth_and_duplication) {
    for (size_t k = 0; k <= 23; k++) {
        bt_gr_offenders offenders = {0};
        bt_alloc_fail_arm(k);
        size_t accepted = 0;
        for (size_t i = 0; i < 20; i++) {
            if (bt_gr_offenders_push(&offenders, "offender %zu", i) == 0) {
                accepted++;
            }
        }
        bt_alloc_fail_disarm();
        if (offenders.count != accepted) {
            bt_harness_fail(__FILE__, __LINE__, "budget %zu lost %zu accepted pushes", k, accepted);
        }
        if (k == 23 && accepted != 20) {
            bt_harness_fail(__FILE__, __LINE__, "full budget accepted only %zu of 20 pushes",
                            accepted);
        }
        bt_gr_offenders_free(&offenders);
    }
}

static bt_json_value *parse_json(const char *text) {
    bt_json_value *value = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &value, &error) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "fixture json parse failed: %s", error.message);
        return NULL;
    }
    return value;
}

static void fixture_report(bt_gr_ga_report *report) {
    memset(report, 0, sizeof *report);
    report->run_config =
        parse_json("{\"ga\": {\"seed\": 11, \"population\": 30, \"generations\": 7, \"elite\": 2, "
                   "\"weights\": {\"confusion\": 0.5, \"flow\": 0.3, \"complexity\": 0.2}, "
                   "\"mutation\": {\"swap\": 0.02, \"replace\": 0.01}}}");
    report->inventory.generated = 100;
    report->inventory.kept = 10;
    report->inventory.dropped_not_in_wordlist = 90;
    report->inventory.wordlist_size = 10;
    report->inventory.wordlist_covered = 9;
    report->pool_size = 15;
    snprintf(report->pool_top[0].text, BT_GA_TEXT_MAX, "ma");
    report->pool_top[0].frequency = 500;
    snprintf(report->pool_top[1].text, BT_GA_TEXT_MAX, "to");
    report->pool_top[1].frequency = 400;
    report->pool_top_count = 2;
    report->best_fitness = 0.654321F;
    report->best_parts[0] = 0.1F;
    report->best_parts[1] = 0.2F;
    report->best_parts[2] = 0.3F;
    report->best_generation = 4;
    report->generations_run = 7;
    report->per_gen_best = malloc(3 * sizeof *report->per_gen_best);
    report->per_gen_best[0] = 0.5F;
    report->per_gen_best[1] = 0.4F;
    report->per_gen_best[2] = 0.3F;
    report->per_gen_count = 3;
    report->stagnation_tail = 3;
    report->max_stagnation = 9;
    report->evaluations = 42;
    memset(report->freq_sha256, 'f', 64);
    memset(report->syllables_sha256, 's', 64);
}

static void fixture_report_free(bt_gr_ga_report *report) {
    bt_json_free((bt_json_value *)report->run_config);
    free(report->per_gen_best);
    memset(report, 0, sizeof *report);
}

BT_TEST(ga_run_oom, report_text_fails_closed_until_the_budget_covers_every_append) {
    bt_gr_ga_report report;
    fixture_report(&report);
    int results[64];
    for (size_t k = 0; k < 64; k++) {
        bt_strbuf out;
        bt_strbuf_init(&out, 64);
        bt_alloc_fail_arm(k);
        results[k] = bt_gr_report_text(&report, &out);
        bt_alloc_fail_disarm();
        bt_strbuf_free(&out);
    }
    check_budget_walk(results, 64);
    fixture_report_free(&report);
}

BT_TEST(ga_run_oom, markdown_render_fails_closed_until_the_budget_covers_every_append) {
    bt_gr_ga_report report;
    fixture_report(&report);
    int results[64];
    for (size_t k = 0; k < 64; k++) {
        bt_strbuf out;
        bt_strbuf_init(&out, 64);
        bt_alloc_fail_arm(k);
        results[k] = bt_gr_render_markdown(&report, &out);
        bt_alloc_fail_disarm();
        bt_strbuf_free(&out);
    }
    check_budget_walk(results, 64);
    fixture_report_free(&report);
}

BT_TEST(ga_run_oom, json_value_text_heap_order_fails_closed) {
    char text[2048];
    size_t at = 0;
    at += (size_t)snprintf(text + at, sizeof text - at, "{");
    for (size_t i = 0; i < 20; i++) {
        at += (size_t)snprintf(text + at, sizeof text - at, "%s\"key%02zu\": %zu",
                               i > 0 ? ", " : "", i, i);
    }
    snprintf(text + at, sizeof text - at, "}");
    bt_json_value *object = parse_json(text);
    BT_CHECK(object != NULL);
    int results[64];
    for (size_t k = 0; k < 64; k++) {
        bt_strbuf out;
        bt_strbuf_init(&out, 64);
        bt_alloc_fail_arm(k);
        results[k] = bt_gr_json_value_text(object, &out);
        bt_alloc_fail_disarm();
        bt_strbuf_free(&out);
    }
    check_budget_walk(results, 64);
    bt_json_free(object);
}

BT_TEST(ga_run_oom, display_f64_fails_closed_until_the_budget_covers_the_form) {
    int results[8];
    for (size_t k = 0; k < 8; k++) {
        bt_strbuf out;
        bt_strbuf_init(&out, 64);
        bt_alloc_fail_arm(k);
        results[k] = bt_gr_display_f64(&out, 0.15625);
        bt_alloc_fail_disarm();
        bt_strbuf_free(&out);
    }
    check_budget_walk(results, 8);
}

static void fixture_assignment(bt_gr_syllable_assignment *assignment) {
    memset(assignment, 0, sizeof *assignment);
    assignment->pool_size = 2;
    assignment->seed = 77;
    assignment->best_fitness = 0.5F;
    assignment->cases = malloc(2 * sizeof *assignment->cases);
    for (size_t i = 0; i < 2; i++) {
        snprintf(assignment->cases[i].orbit, sizeof assignment->cases[i].orbit, "edge");
        snprintf(assignment->cases[i].letters, sizeof assignment->cases[i].letters, "ab");
        snprintf(assignment->cases[i].syllable, sizeof assignment->cases[i].syllable, "syl-%zu", i);
    }
    assignment->cases_count = 2;
    memset(assignment->freq_sha256, 'f', 64);
    memset(assignment->syllables_sha256, 's', 64);
}

BT_TEST(ga_run_oom, assignment_and_case_rows_fail_closed_until_the_budget_covers_every_append) {
    bt_gr_syllable_assignment assignment;
    fixture_assignment(&assignment);
    int text_results[32];
    int compact_results[32];
    for (size_t k = 0; k < 32; k++) {
        bt_strbuf out;
        bt_strbuf_init(&out, 64);
        bt_alloc_fail_arm(k);
        text_results[k] = bt_gr_assignment_text(&assignment, &out);
        bt_alloc_fail_disarm();
        bt_strbuf_free(&out);
    }
    for (size_t k = 0; k < 32; k++) {
        bt_strbuf out;
        bt_strbuf_init(&out, 64);
        bt_alloc_fail_arm(k);
        compact_results[k] =
            bt_gr_cases_text_compact(assignment.cases, assignment.cases_count, &out);
        bt_alloc_fail_disarm();
        bt_strbuf_free(&out);
    }
    check_budget_walk(text_results, 32);
    check_budget_walk(compact_results, 32);
    free(assignment.cases);
}
