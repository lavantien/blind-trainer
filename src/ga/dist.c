#include "ga/dist.h"

#include "common/bt_trace.h"
#include "common/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *bt_ga_dist_read_file(const char *path, size_t *len, char err[BT_GA_ERR_MAX]) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(err, BT_GA_ERR_MAX, "read %s", path);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        fclose(file);
        snprintf(err, BT_GA_ERR_MAX, "read %s", path);
        return NULL;
    }
    char *text = malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(file);
        snprintf(err, BT_GA_ERR_MAX, "read %s", path);
        return NULL;
    }
    size_t got = fread(text, 1, (size_t)size, file);
    fclose(file);
    text[got] = '\0';
    *len = got;
    return text;
}

static int bt_ga_dist_integer(const bt_json_value *object, const char *key, uint64_t *out) {
    const bt_json_value *value = bt_json_get(object, key);
    if (value == NULL || !bt_json_is_integer(value) || value->integer < 0) {
        return 0;
    }
    *out = (uint64_t)value->integer;
    return 1;
}

static int bt_ga_dist_parse_cases(const bt_json_value *array, bt_ga_case_row **rows,
                                  size_t *count) {
    *rows = NULL;
    *count = 0;
    if (array == NULL || array->type != BT_JSON_ARRAY) {
        return 0;
    }
    *count = array->as.array.count;
    *rows = malloc((*count > 0 ? *count : 1) * sizeof **rows);
    for (size_t i = 0; i < *count; i++) {
        const bt_json_value *item = bt_json_at(array, i);
        const bt_json_value *orbit = item != NULL ? bt_json_get(item, "orbit") : NULL;
        const bt_json_value *letters = item != NULL ? bt_json_get(item, "letters") : NULL;
        if (item == NULL || item->type != BT_JSON_OBJECT || orbit == NULL ||
            orbit->type != BT_JSON_STRING || letters == NULL || letters->type != BT_JSON_STRING) {
            free(*rows);
            *rows = NULL;
            *count = 0;
            return 0;
        }
        if (strlen(orbit->as.string) >= sizeof(*rows)[i].orbit ||
            strlen(letters->as.string) >= sizeof(*rows)[i].letters) {
            free(*rows);
            *rows = NULL;
            *count = 0;
            return 0;
        }
        snprintf((*rows)[i].orbit, sizeof(*rows)[i].orbit, "%s", orbit->as.string);
        snprintf((*rows)[i].letters, sizeof(*rows)[i].letters, "%s", letters->as.string);
        if (!bt_ga_dist_integer(item, "count", &(*rows)[i].count)) {
            free(*rows);
            *rows = NULL;
            *count = 0;
            return 0;
        }
    }
    return 1;
}

static int bt_ga_dist_u32(const bt_json_value *object, const char *key, uint32_t *out) {
    uint64_t value = 0;
    if (!bt_ga_dist_integer(object, key, &value) || value > UINT32_MAX) {
        return 0;
    }
    *out = (uint32_t)value;
    return 1;
}

static int bt_ga_dist_parse_pairs(const bt_json_value *array, bt_ga_pair_count **rows,
                                  size_t *count) {
    *rows = NULL;
    *count = 0;
    if (array == NULL || array->type != BT_JSON_ARRAY) {
        return 0;
    }
    *count = array->as.array.count;
    *rows = malloc((*count > 0 ? *count : 1) * sizeof **rows);
    for (size_t i = 0; i < *count; i++) {
        const bt_json_value *item = bt_json_at(array, i);
        if (item == NULL || item->type != BT_JSON_OBJECT ||
            !bt_ga_dist_u32(item, "from", &(*rows)[i].from) ||
            !bt_ga_dist_u32(item, "to", &(*rows)[i].to) ||
            !bt_ga_dist_integer(item, "count", &(*rows)[i].count)) {
            free(*rows);
            *rows = NULL;
            *count = 0;
            return 0;
        }
    }
    return 1;
}

int bt_ga_load_freq_data(const char *path, bt_ga_freq_data *out, char err[BT_GA_ERR_MAX]) {
    BT_TRACE_ENTER("ga.load_freq_data");
    out->cases = NULL;
    out->cases_count = 0;
    out->adjacent = NULL;
    out->adjacent_count = 0;
    out->cooccurring = NULL;
    out->cooccurring_count = 0;
    out->total_draws = 0;
    out->seed = 0;
    size_t len = 0;
    char *text = bt_ga_dist_read_file(path, &len, err);
    if (text == NULL) {
        BT_TRACE_FAIL("ga.load_freq_data", "read failed for %s", path);
        return 0;
    }
    bt_json_error json_error;
    bt_json_value *root = NULL;
    if (bt_json_parse(text, len, &root, &json_error) != 0) {
        free(text);
        snprintf(err, BT_GA_ERR_MAX, "parse %s", path);
        BT_TRACE_FAIL("ga.load_freq_data", "parse failed for %s", path);
        return 0;
    }
    free(text);
    int ok = root != NULL && root->type == BT_JSON_OBJECT &&
             bt_ga_dist_parse_cases(bt_json_get(root, "cases"), &out->cases, &out->cases_count) &&
             bt_ga_dist_parse_pairs(bt_json_get(root, "adjacent"), &out->adjacent,
                                    &out->adjacent_count) &&
             bt_ga_dist_parse_pairs(bt_json_get(root, "cooccurring"), &out->cooccurring,
                                    &out->cooccurring_count) &&
             bt_ga_dist_integer(root, "total_draws", &out->total_draws) &&
             bt_ga_dist_integer(root, "seed", &out->seed);
    bt_json_free(root);
    if (!ok) {
        bt_ga_freq_data_free(out);
        snprintf(err, BT_GA_ERR_MAX, "parse %s", path);
        BT_TRACE_FAIL("ga.load_freq_data", "shape mismatch for %s", path);
        return 0;
    }
    BT_TRACE_EXIT("ga.load_freq_data");
    return 1;
}

void bt_ga_freq_data_free(bt_ga_freq_data *data) {
    BT_TRACE_ENTER("ga.freq_data_free");
    free(data->cases);
    free(data->adjacent);
    free(data->cooccurring);
    data->cases = NULL;
    data->adjacent = NULL;
    data->cooccurring = NULL;
    data->cases_count = 0;
    data->adjacent_count = 0;
    data->cooccurring_count = 0;
    BT_TRACE_EXIT("ga.freq_data_free");
}

static int bt_ga_dist_pairs_eq(const bt_ga_pair_count *a, size_t a_count, const bt_ga_pair_count *b,
                               size_t b_count) {
    if (a_count != b_count) {
        return 0;
    }
    for (size_t i = 0; i < a_count; i++) {
        if (a[i].from != b[i].from || a[i].to != b[i].to || a[i].count != b[i].count) {
            return 0;
        }
    }
    return 1;
}

int bt_ga_freq_data_eq(const bt_ga_freq_data *a, const bt_ga_freq_data *b) {
    BT_TRACE_ENTER("ga.freq_data_eq");
    int equal =
        a->cases_count == b->cases_count && a->total_draws == b->total_draws &&
        a->seed == b->seed &&
        bt_ga_dist_pairs_eq(a->adjacent, a->adjacent_count, b->adjacent, b->adjacent_count) &&
        bt_ga_dist_pairs_eq(a->cooccurring, a->cooccurring_count, b->cooccurring,
                            b->cooccurring_count);
    if (equal) {
        for (size_t i = 0; i < a->cases_count; i++) {
            if (strcmp(a->cases[i].orbit, b->cases[i].orbit) != 0 ||
                strcmp(a->cases[i].letters, b->cases[i].letters) != 0 ||
                a->cases[i].count != b->cases[i].count) {
                equal = 0;
                break;
            }
        }
    }
    BT_TRACE_EXIT("ga.freq_data_eq");
    return equal;
}

int bt_ga_config_memofreq(const bt_json_value *config, uint64_t *samples, uint64_t *seed,
                          char err[BT_GA_ERR_MAX]) {
    BT_TRACE_ENTER("ga.config_memofreq");
    const bt_json_value *samples_value = bt_config_value(config, "memofreq.samples");
    if (samples_value == NULL || !bt_json_is_integer(samples_value) || samples_value->integer < 0) {
        snprintf(err, BT_GA_ERR_MAX, "config section missing: memofreq.samples");
        BT_TRACE_FAIL("ga.config_memofreq", "memofreq.samples missing");
        return 0;
    }
    *samples = (uint64_t)samples_value->integer;
    const bt_json_value *seed_value = bt_config_value(config, "memofreq.seed");
    if (seed_value == NULL || !bt_json_is_integer(seed_value) || seed_value->integer < 0) {
        snprintf(err, BT_GA_ERR_MAX, "config section missing: memofreq.seed");
        BT_TRACE_FAIL("ga.config_memofreq", "memofreq.seed missing");
        return 0;
    }
    *seed = (uint64_t)seed_value->integer;
    BT_TRACE_EXIT("ga.config_memofreq");
    return 1;
}
