#ifndef BT_FUZZ_DB_H
#define BT_FUZZ_DB_H

#include "common/bt_harness.h"
#include "common/rng.h"
#include "data/db.h"
#include "data/fsrs.h"
#include "data/paths.h"
#include "data/repo_algs.h"
#include "data/repo_settings.h"
#include "data/repo_solves.h"
#include "data/repo_srs.h"
#include "data/srs.h"
#include "fuzz/fuzz_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BT_FUZZ_DB_STAGES_MAX 6
#define BT_FUZZ_DB_STRING_MAX 24

static const char *BT_FUZZ_DB_STAGES[BT_FUZZ_DB_STAGES_MAX] = {
    "edge", "corner", "flips", "twists", "parity", "ltct",
};

static const char BT_FUZZ_DB_ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -'!/\\\"#$%&()[]{}<>?,.;:=+"
    "*~^@|_";

static void bt_fuzz_db_string(bt_rng *rng, char *out) {
    size_t len = (size_t)bt_rng_below(rng, BT_FUZZ_DB_STRING_MAX);
    for (size_t i = 0; i < len; i++) {
        out[i] = BT_FUZZ_DB_ALPHABET[bt_rng_below(rng, sizeof BT_FUZZ_DB_ALPHABET - 1)];
    }
    out[len] = '\0';
}

static long long bt_fuzz_db_i64(bt_rng *rng) {
    return (long long)bt_rng_below(rng, 1ULL << 62) - (1LL << 61);
}

static bt_card bt_fuzz_db_card(bt_rng *rng) {
    bt_card card;
    bt_card_init(&card, 0);
    card.interval_days = bt_fuzz_db_i64(rng);
    card.due_day = bt_fuzz_db_i64(rng);
    card.reps = (unsigned int)bt_rng_below(rng, 1ULL << 30);
    card.lapses = (unsigned int)bt_rng_below(rng, 1ULL << 30);
    size_t review_count = (size_t)bt_rng_below(rng, 20);
    if (review_count > 0) {
        card.reviews = malloc(review_count * sizeof *card.reviews);
        if (card.reviews == NULL) {
            BT_ABORT("review allocation failed");
        }
        for (size_t i = 0; i < review_count; i++) {
            card.reviews[i] = (bt_review_entry){
                bt_fuzz_db_i64(rng),
                2 + (unsigned int)bt_rng_below(rng, 4),
                bt_fuzz_db_i64(rng),
                bt_fuzz_db_i64(rng),
            };
        }
        card.review_count = review_count;
        card.review_cap = review_count;
    }
    return card;
}

static void bt_fuzz_db_once(const uint8_t *data, size_t size) {
    bt_rng rng;
    bt_rng_seed(&rng, bt_fuzz_seed_from(data, size));
    bt_db db;
    bt_db_error db_err;
    if (bt_db_open_memory(&db, &db_err) != 0) {
        BT_ABORT("open");
    }
    long long version = 0;
    if (bt_db_user_version(&db, &version, &db_err) != 0) {
        BT_ABORT("version");
    }
    BT_CHECK(version == bt_db_migration_count());

    bt_user_algs algs;
    bt_user_algs_init(&algs);
    bt_srs_store store;
    bt_srs_store_init(&store);
    size_t rounds = (size_t)bt_rng_below(&rng, 32);
    for (size_t i = 0; i < rounds; i++) {
        const char *stage = BT_FUZZ_DB_STAGES[bt_rng_below(&rng, BT_FUZZ_DB_STAGES_MAX)];
        char case_key[BT_FUZZ_DB_STRING_MAX];
        bt_fuzz_db_string(&rng, case_key);
        if (case_key[0] == '\0') {
            continue;
        }
        char key[BT_REPO_ALGS_KEY_MAX];
        if (bt_repo_algs_join_key(stage, case_key, key, sizeof key) != 0) {
            BT_ABORT("key join");
        }
        char alg_text[BT_FUZZ_DB_STRING_MAX];
        bt_fuzz_db_string(&rng, alg_text);
        if (bt_user_algs_set_entry(&algs, key, alg_text) != 0) {
            BT_ABORT("algs insert");
        }
        bt_card card = bt_fuzz_db_card(&rng);
        if (bt_srs_store_set(&store, key, &card) != 0) {
            BT_ABORT("store insert");
        }
        bt_card_free(&card);
    }
    bt_solve_log log;
    bt_solve_log_init(&log);
    size_t solves = (size_t)bt_rng_below(&rng, 32);
    for (size_t i = 0; i < solves; i++) {
        bt_solve_record record = {
            bt_fuzz_db_i64(&rng),
            bt_fuzz_db_i64(&rng),
            bt_fuzz_db_i64(&rng),
            bt_rng_below(&rng, 2) == 1,
        };
        if (bt_solve_log_push(&log, record) != 0) {
            BT_ABORT("solve push");
        }
    }
    char settings_text[BT_FUZZ_DB_STRING_MAX];
    bt_fuzz_db_string(&rng, settings_text);

    if (bt_repo_algs_save(&db, &algs, &db_err) != 0) {
        BT_ABORT("save algs");
    }
    if (bt_repo_srs_save(&db, &store, &db_err) != 0) {
        BT_ABORT("save srs");
    }
    if (bt_solve_log_save(&db, &log, &db_err) != 0) {
        BT_ABORT("save solves");
    }
    if (bt_repo_settings_save(&db, BT_REPO_SETTINGS_APP_KEY, settings_text, &db_err) != 0) {
        BT_ABORT("save settings");
    }

    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_store expected;
    if (bt_srs_store_clone(&store, &expected) != 0) {
        BT_ABORT("store clone");
    }
    for (size_t i = 0; i < expected.count; i++) {
        bt_card *card = &expected.items[i].card;
        if (card->review_count == 0) {
            continue;
        }
        int has_state = 0;
        bt_fsrs_memory memory = {0.0, 0.0};
        if (bt_replay_sm2(card->reviews, card->review_count, &cfg, &has_state, &memory) != 0 ||
            has_state == 0) {
            BT_ABORT("replay");
        }
        card->has_memory = 1;
        card->memory = memory;
    }

    bt_user_algs loaded_algs;
    bt_user_algs_init(&loaded_algs);
    if (bt_repo_algs_load(&db, &loaded_algs, &db_err) != 0) {
        BT_ABORT("load algs");
    }
    BT_CHECK(bt_user_algs_eq(&loaded_algs, &algs) == 1);
    bt_user_algs_free(&loaded_algs);
    bt_srs_store loaded_store;
    if (bt_repo_srs_load(&db, &cfg, &loaded_store, NULL) != 0) {
        BT_ABORT("load srs");
    }
    BT_CHECK(bt_srs_store_eq(&loaded_store, &expected) == 1);
    bt_srs_store_free(&loaded_store);
    bt_solve_log loaded_log;
    bt_solve_log_init(&loaded_log);
    if (bt_solve_log_load(&db, &loaded_log, &db_err) != 0) {
        BT_ABORT("load solves");
    }
    BT_CHECK(bt_solve_log_eq(&loaded_log, &log) == 1);
    bt_solve_log_free(&loaded_log);
    char *loaded_settings = NULL;
    if (bt_repo_settings_load_text(&db, BT_REPO_SETTINGS_APP_KEY, &loaded_settings, &db_err) !=
        0) {
        BT_ABORT("load settings");
    }
    BT_CHECK(loaded_settings != NULL && strcmp(loaded_settings, settings_text) == 0);
    free(loaded_settings);

    bt_user_algs empty_algs;
    bt_user_algs_init(&empty_algs);
    if (bt_repo_algs_save(&db, &empty_algs, &db_err) != 0) {
        BT_ABORT("clear algs");
    }
    bt_srs_store empty_store;
    bt_srs_store_init(&empty_store);
    if (bt_repo_srs_save(&db, &empty_store, &db_err) != 0) {
        BT_ABORT("clear srs");
    }
    bt_solve_log empty_log;
    bt_solve_log_init(&empty_log);
    if (bt_solve_log_save(&db, &empty_log, &db_err) != 0) {
        BT_ABORT("clear solves");
    }
    bt_user_algs reloaded_algs;
    bt_user_algs_init(&reloaded_algs);
    if (bt_repo_algs_load(&db, &reloaded_algs, &db_err) != 0) {
        BT_ABORT("reload algs");
    }
    BT_CHECK(bt_user_algs_eq(&reloaded_algs, &empty_algs) == 1);
    bt_user_algs_free(&reloaded_algs);
    bt_srs_store reloaded_store;
    if (bt_repo_srs_load(&db, &cfg, &reloaded_store, NULL) != 0) {
        BT_ABORT("reload srs");
    }
    BT_CHECK(bt_srs_store_eq(&reloaded_store, &empty_store) == 1);
    bt_srs_store_free(&reloaded_store);
    bt_solve_log reloaded_log;
    bt_solve_log_init(&reloaded_log);
    if (bt_solve_log_load(&db, &reloaded_log, &db_err) != 0) {
        BT_ABORT("reload solves");
    }
    BT_CHECK(bt_solve_log_eq(&reloaded_log, &empty_log) == 1);
    bt_solve_log_free(&reloaded_log);
    int fresh = 1;
    if (bt_db_is_fresh(&db, &fresh, &db_err) != 0) {
        BT_ABORT("fresh");
    }
    BT_CHECK(fresh == 0);

    bt_user_algs_free(&empty_algs);
    bt_srs_store_free(&empty_store);
    bt_solve_log_free(&empty_log);
    bt_solve_log_free(&log);
    bt_srs_store_free(&expected);
    bt_srs_store_free(&store);
    bt_user_algs_free(&algs);
    bt_db_close(&db);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!bt_fuzz_within_bounds()) {
        return 0;
    }
    bt_fuzz_db_once(data, size);
    return 0;
}

#endif
