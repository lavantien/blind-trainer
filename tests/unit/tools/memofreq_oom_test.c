#include "unit/tools/spawn_util.h"

#include "memofreq/lib.h"

#include "common/alloc.h"
#include "common/bt_harness.h"
#include "common/paths.h"
#include "common/strbuf.h"
#include "data/paths.h"
#include "ga/dist.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

BT_TEST(memofreq_oom, draw_counts_new_and_clone_fail_closed_at_each_allocation) {
    for (size_t k = 0; k < 5; k++) {
        bt_mf_draw_counts fresh;
        bt_alloc_fail_arm(k);
        int rc = bt_mf_draw_counts_new(&fresh);
        bt_alloc_fail_disarm();
        if (rc != -1) {
            bt_harness_fail(__FILE__, __LINE__, "budget %zu let draw_counts_new succeed", k);
        }
    }
    bt_mf_draw_counts seed_counts;
    BT_CHECK_EQ(bt_mf_draw_counts_new(&seed_counts), 0);
    for (size_t k = 0; k < 5; k++) {
        bt_mf_draw_counts clone;
        bt_alloc_fail_arm(k);
        int rc = bt_mf_draw_counts_clone(&seed_counts, &clone);
        bt_alloc_fail_disarm();
        if (rc != -1) {
            bt_harness_fail(__FILE__, __LINE__, "budget %zu let draw_counts_clone succeed", k);
        }
    }
    bt_mf_draw_counts grown_clone;
    BT_CHECK_EQ(bt_mf_draw_counts_clone(&seed_counts, &grown_clone), 0);
    bt_mf_draw_counts_free(&grown_clone);
    bt_mf_draw_counts_free(&seed_counts);
}

static bt_ga_freq_data *frequencies_armed(uint64_t samples, uint64_t seed, size_t ok) {
    bt_alloc_fail_arm(ok);
    bt_ga_freq_data *data = bt_mf_count_frequencies(samples, seed);
    bt_alloc_fail_disarm();
    return data;
}

BT_TEST(memofreq_oom, count_frequencies_fails_closed_at_every_allocation_site) {
    size_t hi = 1;
    while (frequencies_armed(96, 7, hi) == NULL && hi < (1U << 16U)) {
        hi *= 2;
    }
    bt_ga_freq_data *full = frequencies_armed(96, 7, hi);
    if (full == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "count_frequencies never succeeds under a 64K budget");
        return;
    }
    bt_ga_freq_data_free(full);
    size_t lo = 1;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        bt_ga_freq_data *probe = frequencies_armed(96, 7, mid);
        if (probe != NULL) {
            bt_ga_freq_data_free(probe);
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    if (lo > 64) {
        bt_harness_fail(__FILE__, __LINE__, "allocation budget %zu is implausibly large", lo);
        return;
    }
    for (size_t k = 0; k < lo; k++) {
        bt_ga_freq_data *starved = frequencies_armed(96, 7, k);
        if (starved != NULL) {
            bt_ga_freq_data_free(starved);
            bt_harness_fail(__FILE__, __LINE__, "budget %zu let count_frequencies succeed", k);
            return;
        }
    }
}

BT_TEST(memofreq_oom, drain_pairs_fails_closed_on_its_row_allocation) {
    uint64_t counts[3] = {5, 0, 3};
    size_t kept = 99;
    bt_alloc_fail_arm(0);
    bt_ga_pair_count *starved = bt_mf_drain_pairs(counts, 3, 40, &kept);
    bt_alloc_fail_disarm();
    if (starved != NULL || kept != 0) {
        bt_harness_fail(__FILE__, __LINE__, "zero budget did not fail drain_pairs closed");
    }
    bt_ga_pair_count *rows = bt_mf_drain_pairs(counts, 3, 40, &kept);
    BT_CHECK(rows != NULL);
    BT_CHECK_EQ(kept, (size_t)2);
    if (rows != NULL && kept == 2) {
        BT_CHECK_EQ(rows[0].from, (uint32_t)40);
        BT_CHECK_EQ(rows[0].to, (uint32_t)40);
        BT_CHECK_EQ(rows[0].count, (uint64_t)5);
        BT_CHECK_EQ(rows[1].from, (uint32_t)41);
        BT_CHECK_EQ(rows[1].to, (uint32_t)41);
        BT_CHECK_EQ(rows[1].count, (uint64_t)3);
    }
    free(rows);
}

BT_TEST(memofreq_oom, zero_hit_rows_fail_closed_at_each_row_duplication) {
    bt_ga_freq_data data;
    memset(&data, 0, sizeof data);
    data.cases = malloc(3 * sizeof *data.cases);
    snprintf(data.cases[0].orbit, sizeof data.cases[0].orbit, "edge");
    snprintf(data.cases[0].letters, sizeof data.cases[0].letters, "ab");
    data.cases[0].count = 5;
    snprintf(data.cases[1].orbit, sizeof data.cases[1].orbit, "corner");
    snprintf(data.cases[1].letters, sizeof data.cases[1].letters, "cd");
    data.cases[1].count = 0;
    snprintf(data.cases[2].orbit, sizeof data.cases[2].orbit, "corner");
    snprintf(data.cases[2].letters, sizeof data.cases[2].letters, "ef");
    data.cases[2].count = 0;
    data.cases_count = 3;
    for (size_t k = 0; k < 3; k++) {
        size_t count = 99;
        bt_alloc_fail_arm(k);
        char **rows = bt_mf_zero_hit_cases(&data, &count);
        bt_alloc_fail_disarm();
        if (rows != NULL || count != 0) {
            bt_harness_fail(__FILE__, __LINE__, "budget %zu did not fail zero hits closed", k);
        }
    }
    size_t count = 0;
    char **rows = bt_mf_zero_hit_cases(&data, &count);
    BT_CHECK(rows != NULL);
    BT_CHECK_EQ(count, (size_t)2);
    if (rows != NULL && count == 2) {
        BT_CHECK_EQ(strcmp(rows[0], "corner/cd"), 0);
        BT_CHECK_EQ(strcmp(rows[1], "corner/ef"), 0);
    }
    bt_mf_str_list_free(rows, count);
    free(data.cases);
}

BT_TEST(memofreq_oom, freq_data_text_fails_closed_until_the_budget_covers_every_append) {
    bt_ga_freq_data data;
    memset(&data, 0, sizeof data);
    data.cases = malloc(2 * sizeof *data.cases);
    snprintf(data.cases[0].orbit, sizeof data.cases[0].orbit, "edge");
    snprintf(data.cases[0].letters, sizeof data.cases[0].letters, "ab");
    data.cases[0].count = 9;
    snprintf(data.cases[1].orbit, sizeof data.cases[1].orbit, "corner");
    snprintf(data.cases[1].letters, sizeof data.cases[1].letters, "cd");
    data.cases[1].count = 0;
    data.cases_count = 2;
    data.adjacent = malloc(sizeof *data.adjacent);
    data.adjacent[0].from = 4;
    data.adjacent[0].to = 9;
    data.adjacent[0].count = 3;
    data.adjacent_count = 1;
    data.cooccurring = malloc(sizeof *data.cooccurring);
    data.cooccurring[0].from = 2;
    data.cooccurring[0].to = 7;
    data.cooccurring[0].count = 8;
    data.cooccurring_count = 1;
    data.total_draws = 100;
    data.seed = 7;
    int results[48];
    for (size_t k = 0; k < 48; k++) {
        bt_strbuf out;
        bt_strbuf_init(&out, 64);
        bt_alloc_fail_arm(k);
        results[k] = bt_mf_freq_data_text(&data, &out);
        bt_alloc_fail_disarm();
        bt_strbuf_free(&out);
    }
    check_budget_walk(results, 48);
    free(data.cooccurring);
    free(data.adjacent);
    free(data.cases);
}

static const uint64_t MF_WALK_SAMPLES = 300;
static const uint64_t MF_FULL_SAMPLES = 30000;
static const uint64_t MF_SEED = 20261012;
static const unsigned MF_WALK_SPAWN_TIMEOUT_MS = 30000;
static const unsigned MF_FULL_SPAWN_TIMEOUT_MS = 120000;
static const size_t MF_FULL_BUDGET = 100000000;
static const size_t MF_WALK_STEPS = 13;
static const size_t MF_COUNT_SPAN = 16;
static const size_t MF_ZERO_HIT_BOUND = 8192;
static const size_t MF_CONFIG_BOUND_MAX = 1U << 16U;
static const size_t MF_SWEEP_PREFILL = 16;
static const size_t MF_SWEEP_BUDGETS = 10;
static const char *const MF_SWEEP_FILLER = "0123456789abcdef";

static void sweep_freq_data_text(const char *what, const bt_ga_freq_data *data) {
    bt_strbuf probe;
    bt_strbuf_init(&probe, 65536);
    int rc = bt_mf_freq_data_text(data, &probe);
    size_t total = probe.len;
    bt_strbuf_free(&probe);
    if (rc != 0) {
        bt_harness_fail(__FILE__, __LINE__, "%s does not render at a full budget", what);
        return;
    }
    for (size_t d = 0; d <= total; d++) {
        size_t first_ok = MF_SWEEP_BUDGETS;
        for (size_t k = 0; k < MF_SWEEP_BUDGETS; k++) {
            bt_strbuf out;
            bt_strbuf_init(&out, MF_SWEEP_PREFILL + 1 + d);
            bt_strbuf_append_str(&out, MF_SWEEP_FILLER);
            bt_alloc_fail_arm(k);
            int row_rc = bt_mf_freq_data_text(data, &out);
            bt_alloc_fail_disarm();
            bt_strbuf_free(&out);
            if (row_rc == 0) {
                if (first_ok == MF_SWEEP_BUDGETS) {
                    first_ok = k;
                }
            } else if (first_ok != MF_SWEEP_BUDGETS) {
                bt_harness_fail(__FILE__, __LINE__,
                                "%s budget %zu failed at capacity %zu after success at %zu", what,
                                k, d, first_ok);
                return;
            }
        }
        if (first_ok == MF_SWEEP_BUDGETS) {
            bt_harness_fail(__FILE__, __LINE__,
                            "%s never succeeds at capacity %zu within %zu budgets", what, d,
                            MF_SWEEP_BUDGETS);
            return;
        }
    }
}

BT_TEST(memofreq_oom, freq_data_text_fail_arms_land_on_every_append_position) {
    bt_ga_freq_data data;
    memset(&data, 0, sizeof data);
    data.cases = malloc(2 * sizeof *data.cases);
    snprintf(data.cases[0].orbit, sizeof data.cases[0].orbit, "edge");
    snprintf(data.cases[0].letters, sizeof data.cases[0].letters, "a\"b");
    data.cases[0].count = 9;
    snprintf(data.cases[1].orbit, sizeof data.cases[1].orbit, "corner");
    snprintf(data.cases[1].letters, sizeof data.cases[1].letters, "c\\d\001e");
    data.cases[1].count = 0;
    data.cases_count = 2;
    data.adjacent = malloc(2 * sizeof *data.adjacent);
    data.adjacent[0].from = 4;
    data.adjacent[0].to = 9;
    data.adjacent[0].count = 3;
    data.adjacent[1].from = 5;
    data.adjacent[1].to = 8;
    data.adjacent[1].count = 1;
    data.adjacent_count = 2;
    data.cooccurring = malloc(2 * sizeof *data.cooccurring);
    data.cooccurring[0].from = 2;
    data.cooccurring[0].to = 7;
    data.cooccurring[0].count = 8;
    data.cooccurring[1].from = 1;
    data.cooccurring[1].to = 6;
    data.cooccurring[1].count = 2;
    data.cooccurring_count = 2;
    data.total_draws = 100;
    data.seed = 7;
    sweep_freq_data_text("full rows", &data);
    free(data.cooccurring);
    free(data.adjacent);
    free(data.cases);

    bt_ga_freq_data empty;
    memset(&empty, 0, sizeof empty);
    sweep_freq_data_text("empty rows", &empty);
}

static void oom_scratch_dir(const char *name, char out[BT_PATH_MAX]) {
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    snprintf(out, BT_PATH_MAX, "%s/build/test-tmp/memofreq-oom/%s", root, name);
    for (char *cursor = out; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
    bt_test_mkdirs(out);
}

static void oom_write_config(const char *data_ga, uint64_t samples) {
    char text[BT_PATH_MAX * 2 + 256];
    snprintf(
        text, sizeof text,
        "{\"paths\": {\"data_ga\": \"%s\"}, \"memofreq\": {\"samples\": %llu, \"seed\": %llu}, "
        "\"trace\": {}}",
        data_ga, (unsigned long long)samples, (unsigned long long)MF_SEED);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/config.json", data_ga);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_ABORT("cannot write %s", path);
    }
    fwrite(text, 1, strlen(text), file);
    fclose(file);
}

static int spawn_memofreq_armed(const char *config_path, size_t budget, unsigned timeout_ms,
                                int *code, char *out, size_t out_cap, char *err, size_t err_cap) {
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "memofreq.exe");
    char budget_text[32];
    snprintf(budget_text, sizeof budget_text, "%zu", budget);
    const char *prior_config = getenv("BT_CONFIG");
    char saved_config[BT_PATH_MAX];
    snprintf(saved_config, sizeof saved_config, "%s", prior_config != NULL ? prior_config : "");
    const char *prior_budget = getenv("BT_MEMOFREQ_ALLOC_FAIL_OK");
    char saved_budget[32];
    snprintf(saved_budget, sizeof saved_budget, "%s", prior_budget != NULL ? prior_budget : "");
    _putenv_s("BT_CONFIG", config_path);
    _putenv_s("BT_MEMOFREQ_ALLOC_FAIL_OK", budget_text);
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    const char *argv[] = {NULL};
    int rc = bt_spawn_capture(exe, argv, root, timeout_ms, code, out, out_cap, err, err_cap);
    _putenv_s("BT_CONFIG", saved_config);
    if (saved_budget[0] != '\0') {
        _putenv_s("BT_MEMOFREQ_ALLOC_FAIL_OK", saved_budget);
    } else {
        _putenv("BT_MEMOFREQ_ALLOC_FAIL_OK=");
    }
    return rc;
}

static int freq_json_exists(const char *data_ga) {
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/freq.json", data_ga);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    fclose(file);
    remove(path);
    return 1;
}

BT_TEST(memofreq_oom, exe_fails_closed_across_sampled_budgets_and_completes_at_a_large_one) {
    char walk_ga[BT_PATH_MAX];
    char full_ga[BT_PATH_MAX];
    oom_scratch_dir("spawn-walk", walk_ga);
    oom_scratch_dir("spawn-full", full_ga);
    oom_write_config(walk_ga, MF_WALK_SAMPLES);
    oom_write_config(full_ga, MF_FULL_SAMPLES);
    char walk_config[BT_PATH_MAX];
    char full_config[BT_PATH_MAX];
    snprintf(walk_config, sizeof walk_config, "%s/config.json", walk_ga);
    snprintf(full_config, sizeof full_config, "%s/config.json", full_ga);
    int code = -1;
    char out[4096];
    char err[4096];
    for (size_t step = 0; step < MF_WALK_STEPS; step++) {
        size_t budget = ((size_t)1 << step) - 1;
        if (spawn_memofreq_armed(walk_config, budget, MF_WALK_SPAWN_TIMEOUT_MS, &code, out,
                                 sizeof out, err, sizeof err) != 0) {
            bt_harness_fail(__FILE__, __LINE__, "spawn at budget %zu failed", budget);
            return;
        }
        if (code != 1) {
            bt_harness_fail(__FILE__, __LINE__, "budget %zu exited %d instead of 1", budget, code);
            return;
        }
        if (freq_json_exists(walk_ga)) {
            bt_harness_fail(__FILE__, __LINE__, "budget %zu left a freq.json behind", budget);
            return;
        }
    }
    if (spawn_memofreq_armed(full_config, MF_FULL_BUDGET, MF_FULL_SPAWN_TIMEOUT_MS, &code, out,
                             sizeof out, err, sizeof err) != 0 ||
        code != 0) {
        bt_harness_fail(__FILE__, __LINE__, "large armed budget did not complete the full run");
        return;
    }
    char needle[128];
    snprintf(needle, sizeof needle, "memofreq: %llu draws seed %llu",
             (unsigned long long)MF_FULL_SAMPLES, (unsigned long long)MF_SEED);
    if (strstr(out, needle) == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "full budget stdout missing the run report");
        return;
    }
    if (!freq_json_exists(full_ga)) {
        bt_harness_fail(__FILE__, __LINE__, "full budget wrote no freq.json");
    }
}

BT_TEST(memofreq_oom, exe_reports_count_frequencies_failures_from_inside_the_count_stage) {
    char walk_ga[BT_PATH_MAX];
    oom_scratch_dir("spawn-count", walk_ga);
    oom_write_config(walk_ga, MF_WALK_SAMPLES);
    char config_path[BT_PATH_MAX];
    snprintf(config_path, sizeof config_path, "%s/config.json", walk_ga);
    int code = -1;
    char out[4096];
    char err[4096];
    size_t hi = 1;
    while (hi < MF_CONFIG_BOUND_MAX) {
        if (spawn_memofreq_armed(config_path, hi, MF_WALK_SPAWN_TIMEOUT_MS, &code, out, sizeof out,
                                 err, sizeof err) != 0) {
            bt_harness_fail(__FILE__, __LINE__, "spawn at budget %zu failed", hi);
            return;
        }
        if (strstr(err, "load config failed") == NULL) {
            break;
        }
        hi *= 2;
    }
    if (hi >= MF_CONFIG_BOUND_MAX) {
        bt_harness_fail(__FILE__, __LINE__, "the config stage never completes its allocations");
        return;
    }
    size_t lo = 0;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (spawn_memofreq_armed(config_path, mid, MF_WALK_SPAWN_TIMEOUT_MS, &code, out, sizeof out,
                                 err, sizeof err) != 0) {
            bt_harness_fail(__FILE__, __LINE__, "spawn at budget %zu failed", mid);
            return;
        }
        if (strstr(err, "load config failed") == NULL) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    for (size_t b = lo; b < lo + MF_COUNT_SPAN; b++) {
        if (spawn_memofreq_armed(config_path, b, MF_WALK_SPAWN_TIMEOUT_MS, &code, out, sizeof out,
                                 err, sizeof err) != 0) {
            bt_harness_fail(__FILE__, __LINE__, "spawn at budget %zu failed", b);
            return;
        }
        if (code != 1) {
            bt_harness_fail(__FILE__, __LINE__, "budget %zu exited %d instead of 1", b, code);
            return;
        }
        if (freq_json_exists(walk_ga)) {
            bt_harness_fail(__FILE__, __LINE__, "budget %zu left a freq.json behind", b);
            return;
        }
        if (strstr(err, "load config failed") != NULL) {
            bt_harness_fail(__FILE__, __LINE__, "budget %zu still failed inside the config stage",
                            b);
            return;
        }
    }
    if (spawn_memofreq_armed(config_path, lo + MF_ZERO_HIT_BOUND, MF_WALK_SPAWN_TIMEOUT_MS, &code,
                             out, sizeof out, err, sizeof err) != 0 ||
        code != 1) {
        bt_harness_fail(__FILE__, __LINE__, "the upper stage probe did not fail closed");
        return;
    }
    if (strstr(err, "never appeared over") == NULL) {
        bt_harness_fail(__FILE__, __LINE__, "the upper stage probe never reached the zero hits");
    }
}
