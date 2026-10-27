#include "memofreq/lib.h"

#include "common/bt_trace.h"
#include "common/config.h"
#include "common/paths.h"
#include "data/paths.h"
#include "ga/dist.h"
#include "ga/hash.h"

#include <direct.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} bt_mf_offenders;

static int offenders_push(bt_mf_offenders *list, const char *fmt, ...) {
    char text[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof text, fmt, args);
    va_end(args);
    if (list->count == list->cap) {
        size_t cap = list->cap == 0 ? 8 : list->cap * 2;
        char **grown = realloc(list->items, cap * sizeof *grown);
        if (grown == NULL) {
            return -1;
        }
        list->items = grown;
        list->cap = cap;
    }
    char *copy = _strdup(text);
    if (copy == NULL) {
        return -1;
    }
    list->items[list->count] = copy;
    list->count++;
    return 0;
}

static void offenders_free(bt_mf_offenders *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static int data_ga_path(const bt_json_value *config, char out[BT_PATH_MAX],
                        bt_mf_offenders *offenders) {
    BT_TRACE_ENTER("mf.data_ga_path");
    const char *relative = NULL;
    if (bt_config_str(config, "paths.data_ga", &relative) != 0) {
        offenders_push(offenders, "config section missing: paths.data_ga");
        BT_TRACE_FAIL("mf.data_ga_path", "paths.data_ga missing");
        return -1;
    }
    bt_mf_data_ga_path(relative, out);
    BT_TRACE_EXIT("mf.data_ga_path");
    return 0;
}

static void ensure_parent_dirs(const char *path) {
    BT_TRACE_ENTER("mf.ensure_parent_dirs");
    char copy[BT_PATH_MAX];
    snprintf(copy, sizeof copy, "%s", path);
    for (char *cursor = copy + 1; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            char saved = *cursor;
            *cursor = '\0';
            _mkdir(copy);
            *cursor = saved;
        }
    }
    BT_TRACE_EXIT("mf.ensure_parent_dirs");
}

static uint64_t sum_pair_counts(const bt_ga_pair_count *rows, size_t count) {
    uint64_t total = 0;
    for (size_t i = 0; i < count; i++) {
        total += rows[i].count;
    }
    return total;
}

static int run(const bt_json_value *config, bt_mf_offenders *offenders) {
    BT_TRACE_ENTER("mf.run");
    char err[BT_GA_ERR_MAX];
    uint64_t samples = 0;
    uint64_t seed = 0;
    if (bt_ga_config_memofreq(config, &samples, &seed, err) != 1) {
        offenders_push(offenders, "%s", err);
        BT_TRACE_FAIL("mf.run", "memofreq config rejected: %s", err);
        return -1;
    }
    char out_dir[BT_PATH_MAX];
    if (data_ga_path(config, out_dir, offenders) != 0) {
        return -1;
    }
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/freq.json", out_dir);
    bt_ga_freq_data *data = bt_mf_count_frequencies(samples, seed);
    if (data == NULL) {
        offenders_push(offenders, "count_frequencies failed");
        BT_TRACE_FAIL("mf.run", "count_frequencies allocation failed");
        return -1;
    }
    size_t zero_count = 0;
    char **zeros = bt_mf_zero_hit_cases(data, &zero_count);
    if (zero_count > 0) {
        offenders_push(offenders, "%zu of %zu cases never appeared over %llu draws", zero_count,
                       bt_mf_total_cases(), (unsigned long long)samples);
        for (size_t i = 0; i < zero_count; i++) {
            offenders_push(offenders, "%s", zeros[i]);
        }
        bt_mf_str_list_free(zeros, zero_count);
        bt_ga_freq_data_free(data);
        free(data);
        BT_TRACE_FAIL("mf.run", "%zu zero hit cases", zero_count);
        return -1;
    }
    bt_mf_str_list_free(zeros, zero_count);
    bt_strbuf bytes;
    bt_strbuf_init(&bytes, 65536);
    if (bt_mf_freq_data_text(data, &bytes) != 0) {
        bt_strbuf_free(&bytes);
        bt_ga_freq_data_free(data);
        free(data);
        offenders_push(offenders, "serialize freq data failed");
        BT_TRACE_FAIL("mf.run", "freq data serialization failed");
        return -1;
    }
    ensure_parent_dirs(path);
    if (bt_paths_write_atomic(path, bytes.data, bytes.len) != 0) {
        bt_strbuf_free(&bytes);
        bt_ga_freq_data_free(data);
        free(data);
        offenders_push(offenders, "write %s", path);
        BT_TRACE_FAIL("mf.run", "write failed for %s", path);
        return -1;
    }
    char sha[BT_GA_SHA256_HEX_LEN];
    bt_ga_sha256_hex(bytes.data, bytes.len, sha);
    uint64_t pair_events = sum_pair_counts(data->cooccurring, data->cooccurring_count);
    uint64_t adjacent_events = sum_pair_counts(data->adjacent, data->adjacent_count);
    printf("memofreq: %llu draws seed %llu -> %zu cases all hit (%llu pair events, %llu adjacent "
           "events), sha256 %s, wrote %s\n",
           (unsigned long long)samples, (unsigned long long)seed, data->cases_count,
           (unsigned long long)pair_events, (unsigned long long)adjacent_events, sha, path);
    bt_strbuf_free(&bytes);
    bt_ga_freq_data_free(data);
    free(data);
    BT_TRACE_EXIT("mf.run");
    return 0;
}

int main(void) {
    bt_json_value *config = bt_data_load_config();
    long long ring_bytes = 0;
    long long max_threads = 0;
    long long line_max = 0;
    if (config != NULL && bt_config_int(config, "trace.ring_bytes", &ring_bytes) == 0 &&
        bt_config_int(config, "trace.max_threads", &max_threads) == 0 &&
        bt_config_int(config, "trace.line_max", &line_max) == 0 &&
        bt_trace_init((size_t)ring_bytes, (size_t)max_threads, (size_t)line_max) == 0) {
        bt_trace_install_crash_handler();
    }
    bt_mf_offenders offenders = {0};
    int failed = config == NULL || run(config, &offenders) != 0;
    if (config == NULL) {
        offenders_push(&offenders, "load config failed");
        failed = 1;
    }
    bt_json_free(config);
    if (failed) {
        for (size_t i = 0; i < offenders.count; i++) {
            fprintf(stderr, "memofreq: %s\n", offenders.items[i]);
        }
        offenders_free(&offenders);
        bt_trace_shutdown();
        return 1;
    }
    offenders_free(&offenders);
    bt_trace_shutdown();
    return 0;
}
