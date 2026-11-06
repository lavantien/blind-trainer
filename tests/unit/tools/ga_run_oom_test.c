#include "ga-run/lib.h"
#include "ga-run/out.h"

#include "common/alloc.h"
#include "common/bt_harness.h"
#include "common/json.h"
#include "common/paths.h"
#include "common/strbuf.h"
#include "data/paths.h"
#include "ga/alloc.h"
#include "ga/dist.h"
#include "ga/hash.h"
#include "ga/inventory.h"
#include "scheme/pairs.h"
#include "unit/tools/ga_run_fixture_util.h"

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
    char vendor[BT_PATH_MAX];
    scratch_dir(name, data_ga);
    snprintf(artifacts_name, sizeof artifacts_name, "%s-artifacts", name);
    scratch_dir(artifacts_name, artifacts);
    ga_run_fixture_vendor("build/test-tmp/ga-run-oom", vendor);
    char paths[BT_PATH_MAX * 2 + 128];
    snprintf(paths, sizeof paths,
             "{\"ref_doc\": \"ref_doc\", \"data_vendor\": \"%s\", \"data_ga\": \"%s\", "
             "\"artifacts\": \"%s\"}",
             vendor, data_ga, artifacts);
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

static size_t run_all_budget_cache;

static size_t run_all_full_budget(const char *config_path, size_t floor) {
    if (run_all_armed(config_path, floor) == 0) {
        bt_harness_fail(__FILE__, __LINE__, "run_all succeeded at the bare load budget %zu", floor);
        return 0;
    }
    size_t hi = floor + 1024;
    if (run_all_armed(config_path, hi) != 0) {
        bt_harness_fail(__FILE__, __LINE__, "run_all fails above load budget plus 1024");
        return 0;
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
        return 0;
    }
    return lo;
}

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
    size_t lo = run_all_full_budget(config_path, floor);
    if (lo == 0) {
        return;
    }
    run_all_budget_cache = lo;
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

static bt_json_value *heap_order_object(void) {
    char text[2048];
    size_t at = 0;
    at += (size_t)snprintf(text + at, sizeof text - at, "{");
    for (size_t i = 0; i < 20; i++) {
        at += (size_t)snprintf(text + at, sizeof text - at, "%s\"key%02zu\": %zu",
                               i > 0 ? ", " : "", i, i);
    }
    snprintf(text + at, sizeof text - at, "}");
    return parse_json(text);
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
    bt_json_value *object = heap_order_object();
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

enum {
    GR_SWEEP_PREFILL = 16,
    GR_SWEEP_BUDGETS = 12,
    GR_RUN_ALL_TAIL_FIRST = 11,
    GR_RUN_ALL_TAIL_LAST = 16,
};

static const char *const GR_SWEEP_FILLER = "0123456789abcdef";

typedef int (*gr_sweep_writer)(void *fixture, bt_strbuf *out);

static int sweep_write_report(void *fixture, bt_strbuf *out) {
    return bt_gr_report_text(fixture, out);
}

static int sweep_write_assignment(void *fixture, bt_strbuf *out) {
    return bt_gr_assignment_text(fixture, out);
}

static int sweep_write_json(void *fixture, bt_strbuf *out) {
    return bt_gr_json_value_text(fixture, out);
}

static int sweep_write_display(void *fixture, bt_strbuf *out) {
    const double *value = fixture;
    return bt_gr_display_f64(out, *value);
}

typedef struct {
    bt_gr_syllable_case *cases;
    size_t count;
} gr_cases_fixture;

static int sweep_write_compact(void *fixture, bt_strbuf *out) {
    gr_cases_fixture *holder = fixture;
    return bt_gr_cases_text_compact(holder->cases, holder->count, out);
}

static void sweep_writer(const char *what, gr_sweep_writer writer, void *fixture) {
    bt_strbuf probe;
    bt_strbuf_init(&probe, 65536);
    int rc = writer(fixture, &probe);
    size_t total = probe.len;
    bt_strbuf_free(&probe);
    if (rc != 0) {
        bt_harness_fail(__FILE__, __LINE__, "%s does not render at a full budget", what);
        return;
    }
    for (size_t d = 0; d <= total; d++) {
        size_t first_ok = GR_SWEEP_BUDGETS;
        for (size_t k = 0; k < GR_SWEEP_BUDGETS; k++) {
            bt_strbuf out;
            bt_strbuf_init(&out, GR_SWEEP_PREFILL + 1 + d);
            bt_strbuf_append_str(&out, GR_SWEEP_FILLER);
            bt_alloc_fail_arm(k);
            int row_rc = writer(fixture, &out);
            bt_alloc_fail_disarm();
            bt_strbuf_free(&out);
            if (row_rc == 0) {
                if (first_ok == GR_SWEEP_BUDGETS) {
                    first_ok = k;
                }
            } else if (first_ok != GR_SWEEP_BUDGETS) {
                bt_harness_fail(__FILE__, __LINE__,
                                "%s budget %zu failed at capacity %zu after success at %zu", what,
                                k, d, first_ok);
                return;
            }
        }
        if (first_ok == GR_SWEEP_BUDGETS) {
            bt_harness_fail(__FILE__, __LINE__,
                            "%s never succeeds at capacity %zu within %d budgets", what, d,
                            GR_SWEEP_BUDGETS);
            return;
        }
    }
}

static const char *const GR_RICH_JSON_TEXT =
    "{\"a\": null, \"b\": true, \"c\": false, \"d\": 42, \"e\": -1e20, \"f\": 0.015625, "
    "\"g\": \"quote\\\"inside\", \"h\": \"back\\\\slash\", \"i\": \"ctrl\\u0001byte\", \"j\": [], "
    "\"k\": {}, \"l\": [1, \"two\", 3.5, [4, 5], {\"m\": 6}], \"n\": 120000.0, \"o\": 0.5, "
    "\"p\": 1.234567e300}";

BT_TEST(ga_run_oom, json_value_text_fail_arms_land_on_every_append_position) {
    bt_json_value *rich = parse_json(GR_RICH_JSON_TEXT);
    BT_CHECK(rich != NULL);
    if (rich != NULL) {
        sweep_writer("rich json", sweep_write_json, rich);
        bt_json_free(rich);
    }
    bt_json_value *heap = heap_order_object();
    BT_CHECK(heap != NULL);
    if (heap != NULL) {
        sweep_writer("heap order json", sweep_write_json, heap);
        bt_json_free(heap);
    }
}

BT_TEST(ga_run_oom, json_value_text_rejects_an_unknown_value_type) {
    bt_json_value *value = parse_json("null");
    BT_CHECK(value != NULL);
    if (value == NULL) {
        return;
    }
    bt_json_type saved = value->type;
    value->type = (bt_json_type)7777;
    bt_strbuf out;
    bt_strbuf_init(&out, 64);
    BT_CHECK_EQ(bt_gr_json_value_text(value, &out), -1);
    bt_strbuf_free(&out);
    value->type = saved;
    bt_json_free(value);
}

BT_TEST(ga_run_oom, display_f64_fail_arms_land_on_every_append_position) {
    static const double forms[] = {120000.0, 12.25, 0.015625, -0.015625, 1e20};
    for (size_t i = 0; i < sizeof forms / sizeof forms[0]; i++) {
        sweep_writer("display form", sweep_write_display, (void *)&forms[i]);
    }
}

static void fixture_report_forms(bt_gr_ga_report *report) {
    fixture_report(report);
    report->best_fitness = 120000.0F;
    report->best_parts[0] = 1e-7F;
    report->best_parts[1] = -0.015625F;
    report->best_parts[2] = 12.25F;
    report->per_gen_best[0] = 0.015625F;
    report->per_gen_best[1] = 1e13F;
    report->per_gen_best[2] = 100.0F;
}

static void fixture_report_empty(bt_gr_ga_report *report) {
    fixture_report(report);
    bt_json_free((bt_json_value *)report->run_config);
    report->run_config = parse_json("{}");
    free(report->per_gen_best);
    report->per_gen_best = NULL;
    report->per_gen_count = 0;
    report->pool_top_count = 0;
}

BT_TEST(ga_run_oom, report_text_fail_arms_land_on_every_append_position) {
    bt_gr_ga_report report;
    fixture_report(&report);
    sweep_writer("report", sweep_write_report, &report);
    fixture_report_forms(&report);
    sweep_writer("report forms", sweep_write_report, &report);
    fixture_report_free(&report);

    bt_gr_ga_report empty;
    fixture_report_empty(&empty);
    sweep_writer("empty report", sweep_write_report, &empty);
    fixture_report_free(&empty);
}

static void fixture_assignment_empty(bt_gr_syllable_assignment *assignment) {
    memset(assignment, 0, sizeof *assignment);
    assignment->pool_size = 0;
    assignment->seed = 1;
    assignment->best_fitness = 0.25F;
    memset(assignment->freq_sha256, 'f', 64);
    memset(assignment->syllables_sha256, 's', 64);
}

static void fixture_compact_cases(bt_gr_syllable_case *cases) {
    memset(cases, 0, 2 * sizeof *cases);
    snprintf(cases[0].orbit, sizeof cases[0].orbit, "edge");
    snprintf(cases[0].letters, sizeof cases[0].letters, "a\"b");
    snprintf(cases[0].syllable, sizeof cases[0].syllable, "s\\l\001x");
    cases[0].frequency = 7;
    snprintf(cases[1].orbit, sizeof cases[1].orbit, "corner");
    snprintf(cases[1].letters, sizeof cases[1].letters, "c\\d");
    snprintf(cases[1].syllable, sizeof cases[1].syllable, "t\"u");
    cases[1].frequency = 9000000000ULL;
}

BT_TEST(ga_run_oom, assignment_and_compact_fail_arms_land_on_every_append_position) {
    bt_gr_syllable_assignment assignment;
    fixture_assignment(&assignment);
    sweep_writer("assignment", sweep_write_assignment, &assignment);
    free(assignment.cases);

    bt_gr_syllable_assignment empty;
    fixture_assignment_empty(&empty);
    sweep_writer("empty assignment", sweep_write_assignment, &empty);

    bt_gr_syllable_case cases[2];
    fixture_compact_cases(cases);
    gr_cases_fixture holder = {.cases = cases, .count = 2};
    sweep_writer("compact rows", sweep_write_compact, &holder);

    gr_cases_fixture none = {.cases = NULL, .count = 0};
    sweep_writer("compact empty", sweep_write_compact, &none);
}

static void fixture_assignment_inputs(bt_gr_run_inputs *inputs, bt_ga_run_outcome *outcome) {
    memset(inputs, 0, sizeof *inputs);
    memset(outcome, 0, sizeof *outcome);
    size_t case_count = bt_ga_case_count();
    inputs->pool = malloc(case_count * sizeof *inputs->pool);
    inputs->freq.cases = malloc(case_count * sizeof *inputs->freq.cases);
    outcome->best = malloc(case_count * sizeof *outcome->best);
    for (size_t i = 0; i < case_count; i++) {
        snprintf(inputs->pool[i].text, BT_GA_TEXT_MAX, "syl-%zu", i);
        inputs->pool[i].frequency = (uint64_t)(case_count - i);
        snprintf(inputs->freq.cases[i].orbit, sizeof inputs->freq.cases[i].orbit, "edge");
        snprintf(inputs->freq.cases[i].letters, sizeof inputs->freq.cases[i].letters, "ab");
        inputs->freq.cases[i].count = (uint64_t)i;
        outcome->best[i] = (uint32_t)i;
    }
    inputs->pool_count = case_count;
    inputs->freq.cases_count = case_count;
    inputs->freq_bytes = malloc(5);
    snprintf(inputs->freq_bytes, 5, "freq");
    inputs->freq_bytes_len = 4;
    inputs->params.seed = 5;
    outcome->best_count = case_count;
    outcome->best_fitness = 0.5F;
}

static void fixture_assignment_inputs_free(bt_gr_run_inputs *inputs, bt_ga_run_outcome *outcome) {
    free(inputs->pool);
    free(inputs->freq.cases);
    free(inputs->freq_bytes);
    free(outcome->best);
}

BT_TEST(ga_run_oom, build_assignment_fails_closed_at_its_allocations) {
    bt_gr_run_inputs inputs;
    bt_ga_run_outcome outcome;
    fixture_assignment_inputs(&inputs, &outcome);
    int results[GR_SWEEP_BUDGETS];
    for (size_t k = 0; k < GR_SWEEP_BUDGETS; k++) {
        bt_gr_syllable_assignment assignment;
        bt_gr_offenders offenders = {0};
        bt_alloc_fail_arm(k);
        results[k] = bt_gr_build_assignment(&inputs, &outcome, &assignment, &offenders);
        bt_alloc_fail_disarm();
        if (results[k] == 0) {
            bt_gr_assignment_free(&assignment);
        }
        bt_gr_offenders_free(&offenders);
    }
    check_budget_walk(results, GR_SWEEP_BUDGETS);
    if (results[0] != -1 || results[1] != -1) {
        bt_harness_fail(__FILE__, __LINE__, "the first two budgets did not both fail: %d and %d",
                        results[0], results[1]);
        return;
    }
    fixture_assignment_inputs_free(&inputs, &outcome);
}

BT_TEST(ga_run_oom, build_report_fails_closed_at_the_per_generation_allocation) {
    static bt_ga_pool_entry pool[2];
    memset(pool, 0, sizeof pool);
    snprintf(pool[0].text, BT_GA_TEXT_MAX, "ma");
    pool[0].frequency = 9;
    snprintf(pool[1].text, BT_GA_TEXT_MAX, "nem");
    pool[1].frequency = 4;
    bt_gr_run_inputs inputs;
    memset(&inputs, 0, sizeof inputs);
    inputs.pool = pool;
    inputs.pool_count = 2;
    bt_ga_run_outcome outcome;
    memset(&outcome, 0, sizeof outcome);
    outcome.best_fitness = 0.5F;
    outcome.per_gen_best = malloc(3 * sizeof *outcome.per_gen_best);
    outcome.per_gen_count = 3;
    bt_gr_syllable_assignment assignment;
    memset(&assignment, 0, sizeof assignment);
    int results[2];
    for (size_t k = 0; k < 2; k++) {
        bt_gr_ga_report report;
        bt_gr_offenders offenders = {0};
        bt_alloc_fail_arm(k);
        results[k] = bt_gr_build_report(&inputs, &outcome, &assignment, NULL, &report, &offenders);
        bt_alloc_fail_disarm();
        if (results[k] == 0) {
            bt_gr_report_free(&report);
        }
        bt_gr_offenders_free(&offenders);
    }
    if (results[0] != -1 || results[1] != 0) {
        bt_harness_fail(__FILE__, __LINE__,
                        "build_report budgets behaved as %d then %d, expected -1 then 0",
                        results[0], results[1]);
    }
    free(outcome.per_gen_best);
}

static void *site_fault_alloc(size_t bytes, const char *site, void *user) {
    const char *armed = user;
    if (site != NULL && strcmp(armed, site) == 0) {
        return NULL;
    }
    return malloc(bytes);
}

static void site_fault_free(void *block, void *user) {
    (void)user;
    free(block);
}

static void load_under_site_fault(const char *config_path, const char *site, int *rc, char *first,
                                  size_t cap) {
    const char *prior = getenv("BT_CONFIG");
    char saved[BT_PATH_MAX];
    snprintf(saved, sizeof saved, "%s", prior != NULL ? prior : "");
    _putenv_s("BT_CONFIG", config_path);
    bt_gr_run_inputs inputs;
    bt_gr_offenders offenders = {0};
    bt_ga_alloc_install(site_fault_alloc, site_fault_free, (void *)site);
    *rc = bt_gr_load_run_inputs(&inputs, &offenders);
    bt_ga_alloc_reset();
    if (*rc == 0) {
        bt_gr_run_inputs_free(&inputs);
    }
    snprintf(first, cap, "%s", offenders.count > 0 ? offenders.items[0] : "");
    bt_gr_offenders_free(&offenders);
    if (saved[0] != '\0') {
        _putenv_s("BT_CONFIG", saved);
    } else {
        _putenv("BT_CONFIG=");
    }
}

BT_TEST(ga_run_oom, load_fails_closed_when_the_inventory_build_allocation_fails) {
    char config_path[BT_PATH_MAX];
    make_tiny_env("site-inventory", config_path);
    int rc = 0;
    char first[256];
    load_under_site_fault(config_path, "ga.build_inventory.list_grow", &rc, first, sizeof first);
    if (rc != -1) {
        bt_harness_fail(__FILE__, __LINE__, "inventory site fault let the load return %d saying %s",
                        rc, first);
        return;
    }
    if (strstr(first, "inventory list growth allocation failed") == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "inventory site fault said: %s", first);
    }
}

BT_TEST(ga_run_oom, load_fails_closed_when_the_env_table_allocation_fails) {
    char config_path[BT_PATH_MAX];
    make_tiny_env("site-env", config_path);
    int rc = 0;
    char first[256];
    load_under_site_fault(config_path, "ga.build_env.conf_table", &rc, first, sizeof first);
    if (rc != -1) {
        bt_harness_fail(__FILE__, __LINE__, "env site fault let the load return %d saying %s", rc,
                        first);
        return;
    }
    if (strstr(first, "table allocation failed") == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "env site fault said: %s", first);
    }
}

BT_TEST(ga_run_oom, run_all_write_tail_extends_the_fail_closed_walk_below_the_writers) {
    char config_path[BT_PATH_MAX];
    make_tiny_env("tail-band", config_path);
    size_t lo = run_all_budget_cache;
    if (lo == 0) {
        size_t floor = load_budget_cache;
        if (floor == 0) {
            floor = smallest_success(config_path, load_armed);
            if (floor == 0) {
                return;
            }
        }
        lo = run_all_full_budget(config_path, floor);
        if (lo == 0) {
            return;
        }
        run_all_budget_cache = lo;
    }
    if (lo <= GR_RUN_ALL_TAIL_LAST) {
        bt_harness_fail(__FILE__, __LINE__, "run_all budget %zu is implausibly small", lo);
        return;
    }
    for (size_t delta = GR_RUN_ALL_TAIL_FIRST; delta <= GR_RUN_ALL_TAIL_LAST; delta++) {
        int rc = run_all_armed(config_path, lo - delta);
        if (rc != -1) {
            bt_harness_fail(__FILE__, __LINE__, "budget %zu below full did not fail closed",
                            lo - delta);
            return;
        }
    }
}
