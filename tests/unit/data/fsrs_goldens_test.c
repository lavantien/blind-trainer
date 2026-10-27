#include "common.h"
#include "common/bt_harness.h"
#include "common/json.h"
#include "data/fsrs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const double PINNED_WEIGHTS[BT_FSRS_WEIGHT_COUNT] = {
    0.40255, 1.18385, 3.173, 15.69105, 7.1949, 0.5345, 1.4604, 0.0046,  1.54575, 0.1192,
    1.01925, 1.9395,  0.11,  0.29605,  2.2698, 0.2315, 2.9898, 0.51655, 0.6621,
};

static bt_json_value *reference_meta(void) {
    char *text = td_ref_file("fsrs/reference_meta.json");
    bt_json_error jerr;
    bt_json_value *meta = NULL;
    if (bt_json_parse(text, strlen(text), &meta, &jerr) != 0) {
        BT_ABORT("reference meta json");
    }
    free(text);
    return meta;
}

static bt_fsrs_config golden_config(void) {
    bt_json_value *meta = reference_meta();
    bt_json_value *scheduler = bt_json_get(meta, "scheduler");
    bt_json_value *parameters = bt_json_get(scheduler, "parameters");
    BT_CHECK(parameters != NULL && parameters->type == BT_JSON_ARRAY);
    BT_CHECK(parameters->as.array.count == 19);
    bt_fsrs_config cfg;
    for (int i = 0; i < 19; i++) {
        cfg.weights[i] = bt_json_at(parameters, (size_t)i)->as.number;
    }
    cfg.request_retention = bt_json_get(scheduler, "desired_retention")->as.number;
    bt_json_value *maximum = bt_json_get(scheduler, "maximum_interval");
    BT_CHECK(bt_json_is_integer(maximum));
    cfg.interval_clamp_days[0] = 1;
    cfg.interval_clamp_days[1] = bt_json_integer_or(maximum, 0);
    bt_json_free(meta);
    return cfg;
}

static bt_grade grade_from_code(long long code) {
    BT_CHECK(code >= 1 && code <= 4);
    return (bt_grade)code;
}

static void assert_close(double actual, double expected, const char *what, const char *case_name,
                         size_t step) {
    double tolerance = 1e-12 * fmax(fabs(expected), 1.0);
    if (!(fabs(actual - expected) <= tolerance)) {
        BT_ABORT("%s mismatch in %s step %zu: got %.17g, expected %.17g", what, case_name, step,
                 actual, expected);
    }
}

BT_TEST(fsrs_goldens, default_settings_match_the_pinned_reference) {
    bt_fsrs_config cfg = golden_config();
    for (int i = 0; i < BT_FSRS_WEIGHT_COUNT; i++) {
        BT_CHECK(cfg.weights[i] == PINNED_WEIGHTS[i]);
    }
    bt_fsrs_config defaults;
    bt_fsrs_config_defaults(&defaults);
    BT_CHECK(cfg.request_retention == defaults.request_retention);
    BT_CHECK(cfg.interval_clamp_days[0] == defaults.interval_clamp_days[0]);
    BT_CHECK(cfg.interval_clamp_days[1] == defaults.interval_clamp_days[1]);
}

BT_TEST(fsrs_goldens, reference_vectors_replay_exactly) {
    bt_fsrs_config cfg = golden_config();
    char *text = td_ref_file("fsrs/reference_vectors.tsv");
    size_t text_len = strlen(text);
    while (text_len > 0 && (text[text_len - 1] == '\n' || text[text_len - 1] == '\r')) {
        text[--text_len] = '\0';
    }
    bt_json_value *meta = reference_meta();
    bt_json_value *cases = bt_json_get(meta, "cases");
    BT_CHECK(cases != NULL && cases->type == BT_JSON_OBJECT);
    size_t meta_cases = cases->as.object.count;
    char *cursor = text;
    char *line = NULL;
    size_t row_count = 0;
    size_t seen_cases = 0;
    char current_case[64] = "";
    bt_fsrs_memory state = {0.0, 0.0};
    size_t step_index = 0;
    bool header = true;
    while (*cursor != '\0') {
        line = cursor;
        char *newline = strchr(line, '\n');
        if (newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor = line + strlen(line);
        }
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }
        if (header) {
            header = false;
            BT_CHECK_EQ(line,
                        "case\tstep\trating\telapsed_days\tretrievability\tstability\tdifficulty\t"
                        "interval_days");
            continue;
        }
        if (line[0] == '\0') {
            continue;
        }
        char *cols[8];
        size_t col = 0;
        cols[col] = line;
        for (char *p = line;; p++) {
            if (*p == '\t' || *p == '\0') {
                bool end = *p == '\0';
                *p = '\0';
                col += 1;
                if (end || col >= 8) {
                    break;
                }
                cols[col] = p + 1;
            }
        }
        BT_CHECK(col == 8);
        const char *case_name = cols[0];
        size_t step = (size_t)strtoul(cols[1], NULL, 10);
        long long rating = strtoll(cols[2], NULL, 10);
        bt_grade grade = grade_from_code(rating);
        bt_json_value *sequence = bt_json_get(cases, case_name);
        BT_CHECK(sequence != NULL);
        if (strcmp(case_name, current_case) != 0) {
            BT_CHECK(step == 0);
            snprintf(current_case, sizeof current_case, "%s", case_name);
            seen_cases += 1;
            step_index = 0;
            state = bt_fsrs_memory_initial(grade, &cfg);
        } else {
            step_index += 1;
            BT_CHECK(step == step_index);
        }
        row_count += 1;
        bt_json_value *pair = bt_json_at(sequence, step);
        BT_CHECK(pair != NULL);
        bt_json_value *meta_rating = bt_json_at(pair, 0);
        bt_json_value *meta_elapsed = bt_json_at(pair, 1);
        BT_CHECK(bt_json_is_integer(meta_rating));
        BT_CHECK(bt_json_integer_or(meta_rating, 0) == rating);
        bool tsv_empty = cols[3][0] == '\0';
        int meta_null = ((meta_elapsed == NULL) || (meta_elapsed->type == BT_JSON_NULL));
        BT_CHECK((tsv_empty && meta_null) || (!tsv_empty && !meta_null));
        if (!tsv_empty) {
            double elapsed = strtod(cols[3], NULL);
            BT_CHECK(bt_json_at(pair, 1)->type == BT_JSON_NUMBER);
            BT_CHECK(elapsed == bt_json_at(pair, 1)->as.number);
        }
        if (step > 0) {
            double elapsed = strtod(cols[3], NULL);
            if (cols[4][0] != '\0') {
                double expected_r = strtod(cols[4], NULL);
                assert_close(bt_fsrs_memory_retrievability(state, elapsed), expected_r,
                             "retrievability", case_name, step);
            }
            state = bt_fsrs_memory_review(state, grade, elapsed, &cfg);
        }
        double expected_s = strtod(cols[5], NULL);
        double expected_d = strtod(cols[6], NULL);
        long long expected_ivl = strtoll(cols[7], NULL, 10);
        assert_close(state.stability, expected_s, "stability", case_name, step);
        assert_close(state.difficulty, expected_d, "difficulty", case_name, step);
        BT_CHECK(bt_fsrs_next_interval_days(state.stability, &cfg) == expected_ivl);
    }
    BT_CHECK(strcmp(current_case, "huge_gap_after_easy") == 0);
    BT_CHECK(step_index >= 1);
    size_t meta_rows = 0;
    for (size_t i = 0; i < meta_cases; i++) {
        meta_rows += cases->as.object.values[i]->as.array.count;
    }
    BT_CHECK(row_count == meta_rows);
    BT_CHECK(seen_cases == meta_cases);
    bt_json_free(meta);
    free(text);
}
