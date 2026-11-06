#include "common/alloc.h"
#include "common/bt_harness.h"
#include "common/json.h"
#include "data/db.h"
#include "data/fsrs.h"
#include "data/repo_srs.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

static unsigned srs_fail_counter;

static const char *srs_fail_dir(const char *tag) {
    static char dir[BT_TU_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/bt-srs-failures/%s-%d-%u", base ? base : ".", tag, _getpid(),
             srs_fail_counter++);
    tu_remove_tree(dir);
    tu_mkdir_p(dir);
    return dir;
}

static bt_card reviewed_card(void) {
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_card card;
    bt_card_init(&card, 100);
    bt_card_review(&card, BT_GRADE_GOOD, 100, &cfg);
    bt_card_review(&card, BT_GRADE_EASY, 104, &cfg);
    return card;
}

BT_TEST(srs_failures, load_error_string_renders_each_kind) {
    bt_srs_load_error err;
    char text[256];
    memset(&err, 0, sizeof err);
    err.kind = BT_SRS_LOAD_DB;
    err.db.kind = BT_DB_ERR_QUERY;
    snprintf(err.db.step, sizeof err.db.step, "%s", "select card");
    snprintf(err.db.detail, sizeof err.db.detail, "%s", "database is locked");
    bt_srs_load_error_string(&err, text, sizeof text);
    BT_CHECK_EQ(text, "query select card: database is locked");
    err.kind = BT_SRS_LOAD_UNKNOWN_QUALITY;
    snprintf(err.key, sizeof err.key, "%s", "edge/UF");
    err.quality = 9;
    bt_srs_load_error_string(&err, text, sizeof text);
    BT_CHECK_EQ(text, "card edge/UF holds review quality 9 with no sm-2 grade mapping");
    err.kind = BT_SRS_LOAD_INVALID_MEMORY;
    err.has_stability = 1;
    err.stability = 1.5;
    err.has_difficulty = 1;
    err.difficulty = 2.5;
    bt_srs_load_error_string(&err, text, sizeof text);
    BT_CHECK_EQ(text, "card edge/UF holds an invalid fsrs memory state (stability Some(1.5), "
                      "difficulty Some(2.5))");
    err.has_stability = 0;
    err.has_difficulty = 0;
    bt_srs_load_error_string(&err, text, sizeof text);
    BT_CHECK_EQ(text, "card edge/UF holds an invalid fsrs memory state (stability None, "
                      "difficulty None)");
}

BT_TEST(srs_failures, store_get_returns_null_for_an_absent_key) {
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_card card = reviewed_card();
    BT_CHECK(bt_srs_store_set(&store, "edge/UF", &card) == 0);
    bt_card_free(&card);
    BT_CHECK(bt_srs_store_get(&store, "edge/UF") != NULL);
    BT_CHECK(bt_srs_store_get(&store, "edge/UL") == NULL);
    bt_srs_store_free(&store);
}

BT_TEST(srs_failures, store_clone_and_eq_survive_each_allocation_failure) {
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_card card = reviewed_card();
    bt_srs_store_set(&store, "edge/UF", &card);
    bt_srs_store_set(&store, "corner/URB", &card);
    bt_card_free(&card);
    for (size_t ok = 0; ok < 12; ok++) {
        bt_srs_store clone;
        bt_alloc_fail_arm(ok);
        int rc = bt_srs_store_clone(&store, &clone);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            BT_CHECK_EQ((int)bt_srs_store_len(&clone), 2);
            BT_CHECK(bt_srs_store_eq(&store, &clone) == 1);
            bt_srs_store_free(&clone);
        }
    }
    bt_srs_store clone;
    BT_CHECK(bt_srs_store_clone(&store, &clone) == 0);
    BT_CHECK(bt_srs_store_eq(&store, &clone) == 1);
    bt_srs_store_free(&clone);
    bt_srs_store empty;
    bt_srs_store_init(&empty);
    BT_CHECK(bt_srs_store_eq(&store, &empty) == 0);
    bt_srs_store_free(&empty);
    bt_srs_store_free(&store);
}

BT_TEST(srs_failures, the_excluded_filter_separates_events) {
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_memory(&db, &db_err) == 0);
    bt_card card = reviewed_card();
    bt_db_error err;
    BT_CHECK(bt_repo_srs_insert_card(&db, "2x2", "speed", "stage", "case-a", &card, 5, &err) == 0);
    BT_CHECK(bt_repo_srs_insert_card(&db, BT_DB_EVENT_3BLD, BT_DB_METHOD_3STYLE, "stage", "case-b",
                                     &card, 5, &err) == 0);
    bt_card_free(&card);
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_card_row *rows = NULL;
    size_t count = 0;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_srs_load_rows(&db, &cfg, BT_SRS_FILTER_EXCLUDE_3BLD, &rows, &count,
                                   &load_err) == 0);
    BT_CHECK_EQ((int)count, 1);
    BT_CHECK_EQ(rows[0].event, "2x2");
    bt_repo_srs_free_rows(rows, count);
    BT_CHECK(bt_repo_srs_load_rows(&db, &cfg, BT_SRS_FILTER_ONLY_3BLD, &rows, &count, &load_err) ==
             0);
    BT_CHECK_EQ((int)count, 1);
    BT_CHECK_EQ(rows[0].event, "3bld");
    bt_repo_srs_free_rows(rows, count);
    bt_db_close(&db);
}

BT_TEST(srs_failures, save_round_trips_through_begin_tx_and_commit) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_card card = reviewed_card();
    bt_srs_store_set(&store, "edge/UF", &card);
    bt_card_free(&card);
    BT_CHECK(bt_repo_srs_save(&db, &store, &err) == 0);
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_store loaded;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &loaded, &load_err) == 0);
    BT_CHECK(bt_srs_store_eq(&store, &loaded) == 1);
    bt_srs_store_free(&loaded);
    bt_srs_store_free(&store);
    bt_db_close(&db);
}

BT_TEST(srs_failures, an_open_transaction_fails_save) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    BT_CHECK_EQ(sqlite3_exec(bt_db_conn(&db), "BEGIN", NULL, NULL, NULL), SQLITE_OK);
    bt_srs_store store;
    bt_srs_store_init(&store);
    BT_CHECK(bt_repo_srs_save(&db, &store, &err) != 0);
    sqlite3_exec(bt_db_conn(&db), "ROLLBACK", NULL, NULL, NULL);
    bt_srs_store_free(&store);
    bt_db_close(&db);
}

BT_TEST(srs_failures, a_held_reader_fails_the_save_commit) {
    const char *dir = srs_fail_dir("commit");
    char db_path[BT_TU_PATH_MAX];
    snprintf(db_path, sizeof db_path, "%s/srs.db", dir);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) == 0);
    sqlite3 *reader = NULL;
    BT_CHECK_EQ(sqlite3_open(db_path, &reader), SQLITE_OK);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK_EQ(sqlite3_prepare_v2(reader, "SELECT COUNT(*) FROM sqlite_master", -1, &stmt, NULL),
                SQLITE_OK);
    BT_CHECK_EQ(sqlite3_step(stmt), SQLITE_ROW);
    bt_srs_store store;
    bt_srs_store_init(&store);
    BT_CHECK(bt_repo_srs_save(&db, &store, &err) != 0);
    BT_CHECK_EQ(err.step, "commit card");
    sqlite3_finalize(stmt);
    sqlite3_close(reader);
    bt_srs_store_free(&store);
    bt_db_close(&db);
    tu_remove_tree(dir);
}

BT_TEST(srs_failures, a_locked_clear_step_fails_save_tx) {
    const char *dir = srs_fail_dir("locked-clear");
    char db_path[BT_TU_PATH_MAX];
    snprintf(db_path, sizeof db_path, "%s/srs.db", dir);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_at(db_path, &db, &err) == 0);
    sqlite3 *locker = NULL;
    BT_CHECK_EQ(sqlite3_open(db_path, &locker), SQLITE_OK);
    BT_CHECK_EQ(sqlite3_exec(locker, "BEGIN EXCLUSIVE", NULL, NULL, NULL), SQLITE_OK);
    bt_srs_store store;
    bt_srs_store_init(&store);
    BT_CHECK(bt_repo_srs_save_tx(&db, &store, &err) != 0);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_EXEC);
    BT_CHECK_EQ(err.step, "clear card");
    sqlite3_exec(locker, "ROLLBACK", NULL, NULL, NULL);
    sqlite3_close(locker);
    bt_srs_store_free(&store);
    bt_db_close(&db);
    tu_remove_tree(dir);
}

BT_TEST(srs_failures, a_five_review_card_grows_the_json_review_buffer) {
    const char *text =
        "{\"cards\":{\"edge/UF\":{\"memory\":null,\"interval_days\":6,\"due_day\":10,"
        "\"reps\":5,\"lapses\":0,\"reviews\":["
        "{\"ts_days\":1,\"quality\":4,\"elapsed_days\":0,\"scheduled_days\":1},"
        "{\"ts_days\":2,\"quality\":4,\"elapsed_days\":1,\"scheduled_days\":2},"
        "{\"ts_days\":3,\"quality\":4,\"elapsed_days\":1,\"scheduled_days\":3},"
        "{\"ts_days\":4,\"quality\":4,\"elapsed_days\":1,\"scheduled_days\":4},"
        "{\"ts_days\":5,\"quality\":4,\"elapsed_days\":1,\"scheduled_days\":6}]}}}";
    bt_json_error jerr;
    bt_json_value *root = NULL;
    BT_CHECK(bt_json_parse(text, strlen(text), &root, &jerr) == 0);
    bt_srs_store store;
    BT_CHECK(bt_srs_store_from_json(root, &store) == 0);
    const bt_card *card = bt_srs_store_get(&store, "edge/UF");
    BT_CHECK(card != NULL);
    BT_CHECK_EQ((int)card->review_count, 5);
    bt_srs_store_free(&store);
    bt_json_free(root);
}

BT_TEST(srs_failures, load_filtered_fails_closed_under_each_allocation_failure) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_card card = reviewed_card();
    bt_srs_store_set(&store, "edge/UF", &card);
    bt_srs_store_set(&store, "corner/URB", &card);
    bt_card_free(&card);
    BT_CHECK(bt_repo_srs_save(&db, &store, &err) == 0);
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    for (size_t ok = 0; ok < 16; ok++) {
        bt_srs_store loaded;
        bt_srs_load_error load_err;
        bt_alloc_fail_arm(ok);
        int rc = bt_repo_srs_load_filtered(&db, &cfg, BT_SRS_FILTER_ONLY_3BLD, &loaded, &load_err);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            BT_CHECK_EQ((int)bt_srs_store_len(&loaded), 2);
        }
        bt_srs_store_free(&loaded);
    }
    bt_srs_store loaded;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_srs_load(&db, &cfg, &loaded, &load_err) == 0);
    BT_CHECK_EQ((int)bt_srs_store_len(&loaded), 2);
    bt_srs_store_free(&loaded);
    bt_srs_store_free(&store);
    bt_db_close(&db);
}

BT_TEST(srs_failures, store_from_json_fails_closed_under_each_allocation_failure) {
    const char *text =
        "{\"cards\":{\"edge/UF\":{\"memory\":null,\"interval_days\":6,\"due_day\":10,"
        "\"reps\":1,\"lapses\":0,\"reviews\":["
        "{\"ts_days\":4,\"quality\":4,\"elapsed_days\":0,\"scheduled_days\":6}]}}}";
    bt_json_error jerr;
    bt_json_value *root = NULL;
    BT_CHECK(bt_json_parse(text, strlen(text), &root, &jerr) == 0);
    for (size_t ok = 0; ok < 10; ok++) {
        bt_srs_store store;
        bt_alloc_fail_arm(ok);
        int rc = bt_srs_store_from_json(root, &store);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            BT_CHECK_EQ((int)bt_srs_store_len(&store), 1);
        }
        bt_srs_store_free(&store);
    }
    bt_srs_store store;
    BT_CHECK(bt_srs_store_from_json(root, &store) == 0);
    BT_CHECK_EQ((int)bt_srs_store_len(&store), 1);
    bt_srs_store_free(&store);
    bt_json_free(root);
}
