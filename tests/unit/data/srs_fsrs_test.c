#include "common.h"
#include "common/bt_harness.h"
#include "common/json.h"
#include "common/rng.h"
#include "data/db.h"
#include "data/fsrs.h"
#include "data/paths.h"
#include "data/repo_srs.h"
#include "data/srs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void golden_config(bt_fsrs_config *cfg) {
    bt_fsrs_config_defaults(cfg);
}

static void hub_config(bt_fsrs_config *cfg) {
    bt_json_value *value = bt_data_load_config();
    if (value == NULL) {
        BT_ABORT("config file");
    }
    char error[96];
    if (bt_fsrs_config_from_json(value, cfg, error, sizeof error) != 0) {
        BT_ABORT("srs.fsrs section: %s", error);
    }
    bt_json_free(value);
}

static bt_grade rating_of_quality(unsigned int quality) {
    bt_grade grade;
    if (bt_grade_from_quality(quality, &grade) != 0) {
        BT_ABORT("golden qualities map");
    }
    return grade;
}

static void assert_close(double actual, double expected, const char *what, const char *case_name,
                         size_t step) {
    double tolerance = 1e-12 * fmax(fabs(expected), 1.0);
    if (!(fabs(actual - expected) <= tolerance)) {
        BT_ABORT("%s mismatch in %s step %zu: got %.17g, expected %.17g", what, case_name, step,
                 actual, expected);
    }
}

BT_TEST(srs_fsrs, unknown_quality_reports_the_offending_quality_in_its_message) {
    bt_srs_load_error err;
    memset(&err, 0, sizeof err);
    err.kind = BT_SRS_LOAD_UNKNOWN_QUALITY;
    snprintf(err.key, sizeof err.key, "%s", "k");
    err.quality = 9;
    char message[160];
    bt_srs_load_error_string(&err, message, sizeof message);
    BT_CHECK_EQ(message, "card k holds review quality 9 with no sm-2 grade mapping");
}

BT_TEST(srs_fsrs, graded_walks_match_the_pinned_reference_conversions) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    char *text = td_ref_file("fsrs/sm2_migration.tsv");
    size_t text_len = strlen(text);
    while (text_len > 0 && (text[text_len - 1] == '\n' || text[text_len - 1] == '\r')) {
        text[--text_len] = '\0';
    }
    char *cursor = text;
    bool header = true;
    char case_name[64] = "";
    bt_card card;
    bt_card_init(&card, 0);
    long long day = 0;
    size_t step = 0;
    unsigned int expected_lapses = 0;
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
        if (strcmp(cols[0], case_name) != 0) {
            snprintf(case_name, sizeof case_name, "%s", cols[0]);
            bt_card_free(&card);
            bt_card_init(&card, 0);
            day = 0;
            step = 0;
            expected_lapses = 0;
        }
        unsigned int quality = (unsigned int)strtoul(cols[2], NULL, 10);
        long long elapsed = strtoll(cols[3], NULL, 10);
        day += elapsed;
        bt_card_review(&card, rating_of_quality(quality), day, &cfg);
        double expected_s = strtod(cols[4], NULL);
        double expected_d = strtod(cols[5], NULL);
        long long expected_ivl = strtoll(cols[6], NULL, 10);
        BT_CHECK(card.has_memory == 1);
        assert_close(card.memory.stability, expected_s, "stability", case_name, step);
        assert_close(card.memory.difficulty, expected_d, "difficulty", case_name, step);
        BT_CHECK(card.interval_days == expected_ivl);
        BT_CHECK(card.due_day == day + card.interval_days);
        BT_CHECK(card.reps == step + 1);
        if (quality == 2) {
            expected_lapses += 1;
        }
        BT_CHECK(card.lapses == expected_lapses);
        bt_review_entry entry = card.reviews[card.review_count - 1];
        BT_CHECK(entry.ts_days == day);
        BT_CHECK(entry.elapsed_days == elapsed);
        BT_CHECK(entry.scheduled_days == expected_ivl);
        BT_CHECK(entry.quality == quality);
        step += 1;
    }
    BT_CHECK(step >= 1);
    bt_card_free(&card);
    free(text);
}

BT_TEST(srs_fsrs, new_card_has_no_memory_and_is_due_immediately) {
    bt_card card;
    bt_card_init(&card, 100);
    BT_CHECK(card.has_memory == 0);
    BT_CHECK(card.interval_days == 0);
    BT_CHECK(card.due_day == 100);
    BT_CHECK(card.reps == 0);
    BT_CHECK(card.lapses == 0);
    BT_CHECK(card.review_count == 0);
    BT_CHECK(bt_card_is_due(&card, 100) == 1);
    BT_CHECK(bt_card_is_due(&card, 99) == 0);
    bt_card_free(&card);
}

BT_TEST(srs_fsrs, the_first_review_seeds_memory_from_the_grade) {
    struct {
        bt_grade rating;
        unsigned int quality;
        double stability;
    } cases[4] = {
        {BT_GRADE_AGAIN, 2, 0.40255},
        {BT_GRADE_HARD, 3, 1.18385},
        {BT_GRADE_GOOD, 4, 3.173},
        {BT_GRADE_EASY, 5, 15.69105},
    };
    bt_fsrs_config cfg;
    golden_config(&cfg);
    for (int i = 0; i < 4; i++) {
        bt_card card;
        bt_card_init(&card, 7);
        bt_card_review(&card, cases[i].rating, 7, &cfg);
        BT_CHECK(card.has_memory == 1);
        assert_close(card.memory.stability, cases[i].stability, "initial stability", "first",
                     cases[i].quality);
        BT_CHECK(card.reviews[0].quality == cases[i].quality);
        BT_CHECK(card.reviews[0].elapsed_days == 0);
        bt_card_free(&card);
    }
}

BT_TEST(srs_fsrs, again_counts_a_lapse_but_reps_keep_counting) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_card card;
    bt_card_init(&card, 0);
    bt_card_review(&card, BT_GRADE_GOOD, 0, &cfg);
    bt_card_review(&card, BT_GRADE_GOOD, 1, &cfg);
    bt_card_review(&card, BT_GRADE_AGAIN, 7, &cfg);
    bt_card_review(&card, BT_GRADE_GOOD, 8, &cfg);
    BT_CHECK(card.reps == 4);
    BT_CHECK(card.lapses == 1);
    BT_CHECK(card.has_memory == 1);
    BT_CHECK(bt_fsrs_memory_is_valid(card.memory) == 1);
    bt_card_free(&card);
}

BT_TEST(srs_fsrs, overdue_reviews_record_the_real_elapsed_gap) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_card card;
    bt_card_init(&card, 0);
    bt_card_review(&card, BT_GRADE_GOOD, 0, &cfg);
    bt_card_review(&card, BT_GRADE_GOOD, 10, &cfg);
    bt_review_entry second = card.reviews[1];
    BT_CHECK(second.elapsed_days == 10);
    BT_CHECK(second.ts_days == 10);
    BT_CHECK(card.due_day == 10 + card.interval_days);
    bt_card_free(&card);
}

BT_TEST(srs_fsrs, review_days_never_move_the_clock_backwards) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_card card;
    bt_card_init(&card, 50);
    bt_card_review(&card, BT_GRADE_GOOD, 40, &cfg);
    BT_CHECK(card.reviews[0].elapsed_days == 0);
    bt_card_review(&card, BT_GRADE_GOOD, 35, &cfg);
    BT_CHECK(card.reviews[1].elapsed_days == 0);
    bt_card_free(&card);
}

BT_TEST(srs_fsrs, intervals_stay_inside_the_hub_clamps) {
    bt_fsrs_config cfg;
    hub_config(&cfg);
    bt_card card;
    bt_card_init(&card, 0);
    long long day = 0;
    int hit_the_cap = 0;
    for (int i = 0; i < 12; i++) {
        day += 6;
        bt_card_review(&card, BT_GRADE_EASY, day, &cfg);
        BT_CHECK(card.interval_days >= 1 && card.interval_days <= 365);
        if (card.interval_days == 365) {
            hit_the_cap = 1;
        }
    }
    BT_CHECK(hit_the_cap == 1);
    bt_card_free(&card);
}

BT_TEST(srs_fsrs, the_hub_fixture_carries_the_approved_fsrs_values) {
    bt_fsrs_config cfg;
    hub_config(&cfg);
    bt_fsrs_config defaults;
    bt_fsrs_config_defaults(&defaults);
    for (int i = 0; i < BT_FSRS_WEIGHT_COUNT; i++) {
        BT_CHECK(cfg.weights[i] == defaults.weights[i]);
    }
    BT_CHECK(cfg.request_retention == 0.9);
    BT_CHECK(cfg.interval_clamp_days[0] == 1);
    BT_CHECK(cfg.interval_clamp_days[1] == 365);
}

BT_TEST(srs_fsrs, card_round_trips_through_the_srs_store) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_card card;
    bt_card_init(&card, 7);
    bt_card_review(&card, BT_GRADE_GOOD, 7, &cfg);
    bt_card_review(&card, BT_GRADE_EASY, 10, &cfg);
    bt_card_review(&card, BT_GRADE_AGAIN, 16, &cfg);
    bt_card_review(&card, BT_GRADE_HARD, 17, &cfg);
    bt_srs_store store;
    bt_srs_store_init(&store);
    BT_CHECK(bt_srs_store_set(&store, "edge/UF-FR", &card) == 0);
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_memory(&db, &db_err) == 0);
    BT_CHECK(bt_repo_srs_save(&db, &store, &db_err) == 0);
    bt_srs_store loaded;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &loaded, NULL) == 0);
    const bt_card *back = bt_srs_store_get(&loaded, "edge/UF-FR");
    BT_CHECK(back != NULL);
    BT_CHECK(bt_card_eq(&card, back) == 1);
    bt_srs_store_free(&loaded);
    bt_srs_store_free(&store);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(srs_fsrs, a_forgotten_memory_is_rebuilt_from_the_recorded_log) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_card graded;
    bt_card_init(&graded, 0);
    bt_card_review(&graded, BT_GRADE_GOOD, 0, &cfg);
    bt_card_review(&graded, BT_GRADE_EASY, 1, &cfg);
    bt_card_review(&graded, BT_GRADE_HARD, 11, &cfg);
    bt_card forgotten;
    BT_CHECK(bt_card_clone(&graded, &forgotten) == 0);
    forgotten.has_memory = 0;
    bt_card_review(&forgotten, BT_GRADE_GOOD, 21, &cfg);
    size_t expected_count = graded.review_count + 1;
    bt_review_entry *expected_log = malloc(expected_count * sizeof *expected_log);
    if (expected_log == NULL) {
        BT_ABORT("log allocation failed");
    }
    memcpy(expected_log, graded.reviews, graded.review_count * sizeof *expected_log);
    expected_log[graded.review_count] = forgotten.reviews[forgotten.review_count - 1];
    int has_state = 0;
    bt_fsrs_memory replayed = {0.0, 0.0};
    BT_CHECK(bt_replay_sm2(expected_log, expected_count, &cfg, &has_state, &replayed) == 0);
    BT_CHECK(has_state == 1);
    BT_CHECK(forgotten.memory.stability == replayed.stability);
    BT_CHECK(forgotten.memory.difficulty == replayed.difficulty);
    free(expected_log);
    bt_card_free(&forgotten);
    bt_card_free(&graded);
}

BT_TEST(srs_fsrs, a_memory_without_any_reviews_seeds_from_the_grade) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_card orphan;
    bt_card_init(&orphan, 0);
    orphan.has_memory = 1;
    orphan.memory.stability = 90.0;
    orphan.memory.difficulty = 5.0;
    orphan.interval_days = 90;
    orphan.due_day = 90;
    bt_card_review(&orphan, BT_GRADE_GOOD, 0, &cfg);
    bt_fsrs_memory expected = bt_fsrs_memory_initial(BT_GRADE_GOOD, &cfg);
    BT_CHECK(orphan.memory.stability == expected.stability);
    BT_CHECK(orphan.memory.difficulty == expected.difficulty);
    bt_card_free(&orphan);
}

BT_TEST(srs_fsrs, legacy_card_json_without_memory_defaults_to_none) {
    const char *text =
        "{\"cards\":{\"k\":{\"ease\":2.5,\"interval_days\":6,\"due_day\":10,\"reps\":2,"
        "\"lapses\":1,\"reviews\":[]}}}";
    bt_json_error jerr;
    bt_json_value *root = NULL;
    BT_CHECK(bt_json_parse(text, strlen(text), &root, &jerr) == 0);
    bt_srs_store store;
    BT_CHECK(bt_srs_store_from_json(root, &store) == 0);
    const bt_card *card = bt_srs_store_get(&store, "k");
    BT_CHECK(card != NULL);
    BT_CHECK(card->has_memory == 0);
    BT_CHECK(card->interval_days == 6);
    BT_CHECK(card->due_day == 10);
    bt_srs_store_free(&store);
    bt_json_free(root);
}

BT_TEST(srs_fsrs, a_card_without_memory_walks_the_reference_ladder_from_scratch) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_card graded;
    bt_card_init(&graded, 0);
    bt_card_review(&graded, BT_GRADE_GOOD, 0, &cfg);
    bt_card_review(&graded, BT_GRADE_GOOD, 1, &cfg);
    bt_card fresh;
    bt_card_init(&fresh, 0);
    bt_card_review(&fresh, BT_GRADE_GOOD, 0, &cfg);
    bt_card_review(&fresh, BT_GRADE_GOOD, 1, &cfg);
    BT_CHECK(fresh.memory.stability == graded.memory.stability);
    BT_CHECK(fresh.memory.difficulty == graded.memory.difficulty);
    int has_state = 0;
    bt_fsrs_memory replayed = {0.0, 0.0};
    BT_CHECK(bt_replay_sm2(fresh.reviews, fresh.review_count, &cfg, &has_state, &replayed) == 0);
    BT_CHECK(has_state == 1);
    BT_CHECK(fresh.memory.stability == replayed.stability);
    BT_CHECK(fresh.memory.difficulty == replayed.difficulty);
    bt_card_free(&fresh);
    bt_card_free(&graded);
}

BT_TEST(srs_fsrs, invariants_hold_over_random_histories) {
    bt_fsrs_config cfg;
    golden_config(&cfg);
    bt_rng rng;
    bt_rng_seed(&rng, 20261027);
    for (int trial = 0; trial < 64; trial++) {
        long long start = (long long)bt_rng_below(&rng, 10000);
        bt_card card;
        bt_card_init(&card, start);
        long long day = start;
        unsigned int lapses = 0;
        size_t rounds = 1 + (size_t)bt_rng_below(&rng, 39);
        for (size_t i = 0; i < rounds; i++) {
            bt_grade rating = (bt_grade)(1 + bt_rng_below(&rng, 4));
            day += (long long)bt_rng_below(&rng, 30);
            bt_card_review(&card, rating, day, &cfg);
            BT_CHECK(card.has_memory == 1);
            BT_CHECK(bt_fsrs_memory_is_valid(card.memory) == 1);
            BT_CHECK(card.interval_days >= 1 && card.interval_days <= 36500);
            BT_CHECK(card.due_day == day + card.interval_days);
            BT_CHECK(card.reps == i + 1);
            if (rating == BT_GRADE_AGAIN) {
                lapses += 1;
            }
            BT_CHECK(card.lapses == lapses);
            bt_review_entry entry = card.reviews[card.review_count - 1];
            BT_CHECK(entry.ts_days == day);
            BT_CHECK(entry.scheduled_days == card.interval_days);
            BT_CHECK(entry.quality == (unsigned int)rating + 1);
            int has_state = 0;
            bt_fsrs_memory replayed = {0.0, 0.0};
            BT_CHECK(bt_replay_sm2(card.reviews, card.review_count, &cfg, &has_state, &replayed) ==
                     0);
            BT_CHECK(has_state == 1);
            BT_CHECK(card.memory.stability == replayed.stability);
            BT_CHECK(card.memory.difficulty == replayed.difficulty);
        }
        bt_card_free(&card);
    }
}
