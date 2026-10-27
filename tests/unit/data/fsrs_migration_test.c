#include "common.h"
#include "common/bt_harness.h"
#include "data/fsrs.h"
#include "data/srs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void assert_close(double actual, double expected, const char *what, const char *case_name,
                         size_t step) {
    double tolerance = 1e-12 * fmax(fabs(expected), 1.0);
    if (!(fabs(actual - expected) <= tolerance)) {
        BT_ABORT("%s mismatch in %s step %zu: got %.17g, expected %.17g", what, case_name, step,
                 actual, expected);
    }
}

BT_TEST(fsrs_migration, quality_mapping_is_the_pinned_sm2_table) {
    struct {
        unsigned int quality;
        int code;
    } cases[4] = {{2, 1}, {3, 2}, {4, 3}, {5, 4}};
    for (int i = 0; i < 4; i++) {
        bt_grade grade;
        BT_CHECK(bt_grade_from_quality(cases[i].quality, &grade) == 0);
        BT_CHECK((int)grade == cases[i].code);
    }
    unsigned int bad[5] = {0, 1, 6, 9, 0xffffffffU};
    for (int i = 0; i < 5; i++) {
        bt_grade grade;
        BT_CHECK(bt_grade_from_quality(bad[i], &grade) != 0);
    }
}

BT_TEST(fsrs_migration, replay_of_empty_history_is_none) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    int has_state = 1;
    bt_fsrs_memory memory = {0.0, 0.0};
    BT_CHECK(bt_replay_sm2(NULL, 0, &cfg, &has_state, &memory) == 0);
    BT_CHECK(has_state == 0);
}

BT_TEST(fsrs_migration, migration_vectors_replay_exactly_at_every_prefix) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    char *text = td_ref_file("fsrs/sm2_migration.tsv");
    size_t text_len = strlen(text);
    while (text_len > 0 && (text[text_len - 1] == '\n' || text[text_len - 1] == '\r')) {
        text[--text_len] = '\0';
    }
    char *cursor = text;
    bool header = true;
    char current_case[64] = "";
    bt_review_entry *history = NULL;
    size_t history_count = 0;
    size_t history_cap = 0;
    while (*cursor != '\0') {
        char *line = cursor;
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
            BT_CHECK_EQ(line, "case\tstep\tquality\telapsed_days\tstability\tdifficulty\t"
                              "interval_days");
            continue;
        }
        if (line[0] == '\0') {
            continue;
        }
        char *cols[7];
        size_t col = 0;
        cols[col] = line;
        for (char *p = line;; p++) {
            if (*p == '\t' || *p == '\0') {
                bool end = *p == '\0';
                *p = '\0';
                col += 1;
                if (end || col >= 7) {
                    break;
                }
                cols[col] = p + 1;
            }
        }
        BT_CHECK(col == 7);
        const char *case_name = cols[0];
        size_t step = (size_t)strtoul(cols[1], NULL, 10);
        unsigned int quality = (unsigned int)strtoul(cols[2], NULL, 10);
        long long elapsed = strtoll(cols[3], NULL, 10);
        if (strcmp(case_name, current_case) != 0) {
            BT_CHECK(step == 0);
            snprintf(current_case, sizeof current_case, "%s", case_name);
            history_count = 0;
        }
        if (history_count == history_cap) {
            history_cap = history_cap == 0 ? 16 : history_cap * 2;
            bt_review_entry *grown = realloc(history, history_cap * sizeof *grown);
            if (grown == NULL) {
                BT_ABORT("history growth failed");
            }
            history = grown;
        }
        history[history_count].ts_days = (long long)history_count;
        history[history_count].quality = quality;
        history[history_count].elapsed_days = elapsed;
        history[history_count].scheduled_days = 0;
        history_count += 1;
        int has_state = 0;
        bt_fsrs_memory memory = {0.0, 0.0};
        if (bt_replay_sm2(history, history_count, &cfg, &has_state, &memory) != 0) {
            BT_ABORT("replay failed in %s step %zu", case_name, step);
        }
        BT_CHECK(has_state == 1);
        double expected_s = strtod(cols[4], NULL);
        double expected_d = strtod(cols[5], NULL);
        long long expected_ivl = strtoll(cols[6], NULL, 10);
        assert_close(memory.stability, expected_s, "stability", case_name, step);
        assert_close(memory.difficulty, expected_d, "difficulty", case_name, step);
        if (bt_fsrs_next_interval_days(memory.stability, &cfg) != expected_ivl) {
            BT_ABORT("interval mismatch in %s step %zu", case_name, step);
        }
    }
    BT_CHECK(strcmp(current_case, "long_gap") == 0);
    BT_CHECK(history_count >= 1);
    free(history);
    free(text);
}
