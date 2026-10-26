#include "common/bt_harness.h"
#include "data/repo_cards.h"
#include "data/repo_srs.h"

#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

static void open_db(bt_db *db) {
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(db, &err) == 0);
}

static void cfg_of(bt_fsrs_config *cfg) {
    bt_fsrs_config_defaults(cfg);
}

static void review_at(bt_card *card, long long ts, unsigned quality, long long elapsed,
                      long long scheduled) {
    if (card->review_count == card->review_cap) {
        size_t cap = card->review_cap == 0 ? 4 : card->review_cap * 2;
        bt_review_entry *grown = realloc(card->reviews, cap * sizeof *grown);
        if (grown == NULL) {
            return;
        }
        card->reviews = grown;
        card->review_cap = cap;
    }
    card->reviews[card->review_count++] = (bt_review_entry){ts, quality, elapsed, scheduled};
}

static bt_card card_of(int has_memory, bt_fsrs_memory memory) {
    bt_card card;
    bt_card_init(&card, 0);
    card.has_memory = has_memory;
    card.memory = memory;
    card.interval_days = 6;
    card.due_day = -3;
    card.reps = 4;
    card.lapses = 2;
    return card;
}

static bt_card graded_card(void) {
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_card card;
    bt_card_init(&card, 0);
    bt_card_review(&card, BT_GRADE_GOOD, 0, &cfg);
    bt_card_review(&card, BT_GRADE_GOOD, 1, &cfg);
    bt_card_review(&card, BT_GRADE_EASY, 7, &cfg);
    return card;
}

static bt_card far_due_card(void) {
    bt_card card;
    bt_card_init(&card, 0);
    card.due_day = 9223372036854775807LL / 4;
    return card;
}

static void exec_raw(bt_db *db, const char *sql) {
    char *err = NULL;
    BT_CHECK(sqlite3_exec(bt_db_conn(db), sql, NULL, NULL, &err) == SQLITE_OK);
}

BT_TEST(repo_srs, save_and_load_round_trips_cards_with_review_history) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_card graded = graded_card();
    bt_card far_due = far_due_card();
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_srs_store_set(&store, "edge/UF", &graded);
    bt_srs_store_set(&store, "corner/URB", &far_due);
    bt_db_error db_err;
    BT_CHECK(bt_repo_srs_save(&db, &store, &db_err) == 0);
    bt_srs_store loaded;
    bt_srs_load_error err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &loaded, &err) == 0);
    BT_CHECK(bt_srs_store_eq(&store, &loaded) == 1);
    bt_srs_store_free(&loaded);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK(sqlite3_prepare_v2(bt_db_conn(&db),
                                "SELECT stability, difficulty FROM srs_state JOIN card "
                                "ON card.id = srs_state.card_id WHERE card.case_key = 'UF'",
                                -1, &stmt, NULL) == SQLITE_OK);
    BT_CHECK(sqlite3_step(stmt) == SQLITE_ROW);
    BT_CHECK(sqlite3_column_type(stmt, 0) != SQLITE_NULL);
    BT_CHECK(sqlite3_column_double(stmt, 0) == graded.memory.stability);
    BT_CHECK(sqlite3_column_double(stmt, 1) == graded.memory.difficulty);
    sqlite3_finalize(stmt);
    bt_srs_store_free(&store);
    bt_card_free(&graded);
    bt_card_free(&far_due);
    bt_db_close(&db);
}

BT_TEST(repo_srs, review_order_survives_identical_entries) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_review_entry history[2] = {
        {5, 4, 0, 1},
        {6, 4, 1, 1},
    };
    bt_fsrs_memory memory;
    int has = 0;
    BT_CHECK(bt_replay_sm2(history, 2, &cfg, &has, &memory) == 0);
    BT_CHECK(has == 1);
    bt_card card = card_of(1, memory);
    review_at(&card, 5, 4, 0, 1);
    review_at(&card, 6, 4, 1, 1);
    review_at(&card, 5, 4, 0, 1);
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_srs_store_set(&store, "edge/UB", &card);
    bt_db_error db_err;
    BT_CHECK(bt_repo_srs_save(&db, &store, &db_err) == 0);
    bt_srs_store loaded;
    bt_srs_load_error err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &loaded, &err) == 0);
    BT_CHECK(bt_srs_store_eq(&store, &loaded) == 1);
    bt_srs_store_free(&loaded);
    bt_srs_store_free(&store);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_srs, save_replaces_the_previous_cards) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_card plain = card_of(0, (bt_fsrs_memory){0});
    bt_srs_store first;
    bt_srs_store_init(&first);
    bt_srs_store_set(&first, "edge/UF", &plain);
    bt_db_error db_err;
    BT_CHECK(bt_repo_srs_save(&db, &first, &db_err) == 0);
    bt_srs_store second;
    bt_srs_store_init(&second);
    bt_srs_store_set(&second, "corner/URB", &plain);
    BT_CHECK(bt_repo_srs_save(&db, &second, &db_err) == 0);
    bt_srs_store loaded;
    bt_srs_load_error err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &loaded, &err) == 0);
    BT_CHECK(bt_srs_store_eq(&second, &loaded) == 1);
    bt_srs_store_free(&loaded);
    bt_srs_store_free(&first);
    bt_srs_store_free(&second);
    bt_card_free(&plain);
    bt_db_close(&db);
}

BT_TEST(repo_srs, saving_an_empty_store_clears_every_card) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_card card = card_of(0, (bt_fsrs_memory){0});
    review_at(&card, 1, 4, 0, 1);
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_srs_store_set(&store, "edge/UF", &card);
    bt_db_error db_err;
    BT_CHECK(bt_repo_srs_save(&db, &store, &db_err) == 0);
    bt_srs_store empty;
    bt_srs_store_init(&empty);
    BT_CHECK(bt_repo_srs_save(&db, &empty, &db_err) == 0);
    bt_srs_store loaded;
    bt_srs_load_error err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &loaded, &err) == 0);
    BT_CHECK(bt_srs_store_eq(&empty, &loaded) == 1);
    bt_srs_store_free(&loaded);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK(sqlite3_prepare_v2(bt_db_conn(&db), "SELECT COUNT(*) FROM review_log", -1, &stmt,
                                NULL) == SQLITE_OK);
    BT_CHECK(sqlite3_step(stmt) == SQLITE_ROW);
    BT_CHECK(sqlite3_column_int64(stmt, 0) == 0);
    sqlite3_finalize(stmt);
    bt_srs_store_free(&store);
    bt_srs_store_free(&empty);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_srs, interval_before_tracks_the_previous_scheduled_interval) {
    bt_db db;
    open_db(&db);
    bt_card card = card_of(0, (bt_fsrs_memory){0});
    review_at(&card, 10, 4, 0, 1);
    review_at(&card, 16, 4, 6, 6);
    review_at(&card, 22, 4, 6, 15);
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_srs_store_set(&store, "edge/UF", &card);
    bt_db_error db_err;
    BT_CHECK(bt_repo_srs_save(&db, &store, &db_err) == 0);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK(sqlite3_prepare_v2(bt_db_conn(&db),
                                "SELECT interval_before FROM review_log ORDER BY id", -1, &stmt,
                                NULL) == SQLITE_OK);
    long long want[3] = {0, 1, 6};
    for (int i = 0; i < 3; i++) {
        BT_CHECK(sqlite3_step(stmt) == SQLITE_ROW);
        BT_CHECK(sqlite3_column_int64(stmt, 0) == want[i]);
    }
    sqlite3_finalize(stmt);
    bt_srs_store_free(&store);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_srs, legacy_null_state_rows_derive_memory_from_the_review_log) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_card card = card_of(0, (bt_fsrs_memory){0});
    review_at(&card, 0, 4, 0, 3);
    review_at(&card, 1, 4, 1, 6);
    review_at(&card, 7, 4, 6, 20);
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_srs_store_set(&store, "edge/UF", &card);
    bt_db_error db_err;
    BT_CHECK(bt_repo_srs_save(&db, &store, &db_err) == 0);
    exec_raw(&db, "UPDATE srs_state SET stability = NULL, difficulty = NULL");
    bt_srs_store loaded;
    bt_srs_load_error err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &loaded, &err) == 0);
    const bt_card *got = bt_srs_store_get(&loaded, "edge/UF");
    BT_CHECK(got != NULL);
    bt_fsrs_memory expected;
    int has = 0;
    BT_CHECK(bt_replay_sm2(card.reviews, card.review_count, &cfg, &has, &expected) == 0);
    BT_CHECK(has == 1);
    BT_CHECK(got->has_memory == 1);
    BT_CHECK(got->memory.stability == expected.stability);
    BT_CHECK(got->memory.difficulty == expected.difficulty);
    bt_srs_store_free(&loaded);
    bt_srs_store_free(&store);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_srs, a_card_without_reviews_or_state_loads_with_no_memory) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_card card = card_of(0, (bt_fsrs_memory){0});
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_srs_store_set(&store, "edge/UF", &card);
    bt_db_error db_err;
    BT_CHECK(bt_repo_srs_save(&db, &store, &db_err) == 0);
    bt_srs_store loaded;
    bt_srs_load_error err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &loaded, &err) == 0);
    BT_CHECK(bt_srs_store_eq(&store, &loaded) == 1);
    bt_srs_store_free(&loaded);
    bt_srs_store_free(&store);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_srs, corrupt_state_columns_fail_the_load_naming_the_card) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_review_entry history[1] = {{0, 4, 0, 3}};
    bt_fsrs_memory memory;
    int has = 0;
    BT_CHECK(bt_replay_sm2(history, 1, &cfg, &has, &memory) == 0);
    BT_CHECK(has == 1);
    bt_card card = card_of(1, memory);
    review_at(&card, 0, 4, 0, 3);
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_srs_store_set(&store, "edge/UF", &card);
    bt_db_error db_err;
    BT_CHECK(bt_repo_srs_save(&db, &store, &db_err) == 0);
    exec_raw(&db, "UPDATE srs_state SET stability = -1.0");
    bt_srs_store loaded;
    bt_srs_load_error err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &loaded, &err) != 0);
    BT_CHECK(err.kind == BT_SRS_LOAD_INVALID_MEMORY);
    BT_CHECK_EQ(err.key, "edge/UF");
    BT_CHECK(err.has_stability == 1);
    BT_CHECK(err.stability == -1.0);
    BT_CHECK(err.has_difficulty == 1);
    bt_srs_store_free(&store);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_srs, half_present_state_columns_fail_the_load) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_card card = card_of(0, (bt_fsrs_memory){0});
    review_at(&card, 0, 4, 0, 3);
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_srs_store_set(&store, "edge/UF", &card);
    bt_db_error db_err;
    BT_CHECK(bt_repo_srs_save(&db, &store, &db_err) == 0);
    exec_raw(&db, "UPDATE srs_state SET stability = 3.173, difficulty = NULL");
    bt_srs_store loaded;
    bt_srs_load_error err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &loaded, &err) != 0);
    char text[256];
    bt_srs_load_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "edge/UF") != NULL);
    bt_srs_store_free(&store);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_srs, an_unknown_legacy_quality_fails_the_load) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_card card = card_of(0, (bt_fsrs_memory){0});
    review_at(&card, 0, 7, 0, 3);
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_srs_store_set(&store, "edge/UF", &card);
    bt_db_error db_err;
    BT_CHECK(bt_repo_srs_save(&db, &store, &db_err) == 0);
    exec_raw(&db, "UPDATE srs_state SET stability = NULL, difficulty = NULL");
    bt_srs_store loaded;
    bt_srs_load_error err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &loaded, &err) != 0);
    BT_CHECK(err.kind == BT_SRS_LOAD_UNKNOWN_QUALITY);
    BT_CHECK_EQ(err.key, "edge/UF");
    BT_CHECK(err.quality == 7);
    bt_srs_store_free(&store);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_srs, legacy_derivation_uses_the_loaded_config_not_the_defaults) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_card card = card_of(0, (bt_fsrs_memory){0});
    review_at(&card, 0, 2, 0, 1);
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_srs_store_set(&store, "edge/UF", &card);
    bt_db_error db_err;
    BT_CHECK(bt_repo_srs_save(&db, &store, &db_err) == 0);
    exec_raw(&db, "UPDATE srs_state SET stability = NULL, difficulty = NULL");
    bt_fsrs_config perturbed;
    cfg_of(&perturbed);
    perturbed.weights[0] = 5.0;
    bt_srs_store loaded;
    bt_srs_load_error err;
    BT_CHECK(bt_repo_srs_load(&db, &perturbed, &loaded, &err) == 0);
    const bt_card *got = bt_srs_store_get(&loaded, "edge/UF");
    BT_CHECK(got != NULL && got->has_memory == 1);
    BT_CHECK(got->memory.stability == 5.0);
    bt_srs_store_free(&loaded);
    bt_srs_store_free(&store);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_srs, an_invalid_replayed_state_fails_the_load_naming_the_card) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_card card = card_of(0, (bt_fsrs_memory){0});
    review_at(&card, 0, 4, 0, 3);
    review_at(&card, 1, 4, 1, 6);
    review_at(&card, 7, 2, 6, 2);
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_srs_store_set(&store, "edge/UF", &card);
    bt_db_error db_err;
    BT_CHECK(bt_repo_srs_save(&db, &store, &db_err) == 0);
    exec_raw(&db, "UPDATE srs_state SET stability = NULL, difficulty = NULL");
    bt_fsrs_config degenerate;
    cfg_of(&degenerate);
    degenerate.weights[11] = 0.0;
    bt_srs_store loaded;
    bt_srs_load_error err;
    BT_CHECK(bt_repo_srs_load(&db, &degenerate, &loaded, &err) != 0);
    char text[512];
    bt_srs_load_error_string(&err, text, sizeof text);
    BT_CHECK(strstr(text, "edge/UF") != NULL);
    bt_srs_store_free(&store);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_srs, saving_the_3bld_store_spares_foreign_event_cards) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_case_ref foreign;
    bt_case_ref_of(&foreign, "2x2", "cll", "cll", "cll.1");
    bt_card graded;
    bt_card_init(&graded, 0);
    bt_card_review(&graded, BT_GRADE_GOOD, 0, &cfg);
    bt_db_error db_err;
    BT_CHECK(bt_repo_cards_save_card(&db, &foreign, &graded, &db_err) == 0);
    bt_card plain = card_of(0, (bt_fsrs_memory){0});
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_srs_store_set(&store, "edge/UF", &plain);
    BT_CHECK(bt_repo_srs_save(&db, &store, &db_err) == 0);
    bt_case_cards cards;
    bt_srs_load_error err;
    BT_CHECK(bt_repo_cards_load(&db, &cfg, &cards, &err) == 0);
    const bt_card *spared = bt_case_cards_get(&cards, &foreign);
    BT_CHECK(spared != NULL);
    BT_CHECK(spared->reps == 1);
    bt_case_cards_free(&cards);
    bt_srs_store loaded;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &loaded, &err) == 0);
    BT_CHECK(bt_srs_store_len(&loaded) == 1);
    bt_srs_store_free(&loaded);
    bt_srs_store_free(&store);
    bt_card_free(&plain);
    bt_card_free(&graded);
    bt_db_close(&db);
}

BT_TEST(repo_srs, load_ignores_foreign_event_rows) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_case_ref foreign;
    bt_case_ref_of(&foreign, "skewb", "sarah", "sarah", "s.1");
    bt_card plain = card_of(0, (bt_fsrs_memory){0});
    bt_db_error db_err;
    BT_CHECK(bt_repo_cards_save_card(&db, &foreign, &plain, &db_err) == 0);
    bt_srs_store loaded;
    bt_srs_load_error err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &loaded, &err) == 0);
    BT_CHECK(bt_srs_store_len(&loaded) == 0);
    bt_srs_store_free(&loaded);
    bt_card_free(&plain);
    bt_db_close(&db);
}
