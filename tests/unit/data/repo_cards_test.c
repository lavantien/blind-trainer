#include "common/bt_harness.h"
#include "data/repo_cards.h"

#include <string.h>

#include "sqlite3.h"

static void open_db(bt_db *db) {
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(db, &err) == 0);
}

static void cfg_of(bt_fsrs_config *cfg) {
    bt_fsrs_config_defaults(cfg);
}

static bt_case_ref case_of(const char *event, const char *method, const char *stage,
                           const char *case_key) {
    bt_case_ref ref;
    bt_case_ref_of(&ref, event, method, stage, case_key);
    return ref;
}

static bt_card graded_card(void) {
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_card card;
    bt_card_init(&card, 0);
    bt_card_review(&card, BT_GRADE_GOOD, 0, &cfg);
    bt_card_review(&card, BT_GRADE_HARD, 3, &cfg);
    return card;
}

BT_TEST(repo_cards, save_and_load_round_trips_a_graded_case_card) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_case_ref key = case_of("2x2", "cll", "cll", "cll.1");
    bt_card card = graded_card();
    bt_db_error err;
    BT_CHECK(bt_repo_cards_save_card(&db, &key, &card, &err) == 0);
    bt_case_cards loaded;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_cards_load(&db, &cfg, &loaded, &load_err) == 0);
    BT_CHECK(bt_case_cards_len(&loaded) == 1);
    const bt_card *got = bt_case_cards_get(&loaded, &key);
    BT_CHECK(got != NULL);
    BT_CHECK(bt_card_eq(got, &card) == 1);
    bt_case_cards_free(&loaded);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_cards, load_excludes_the_3bld_pair_rows) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_srs_store pairs;
    bt_srs_store_init(&pairs);
    bt_card plain;
    bt_card_init(&plain, 0);
    bt_srs_store_set(&pairs, "edge/UF", &plain);
    bt_db_error err;
    BT_CHECK(bt_repo_srs_save(&db, &pairs, &err) == 0);
    bt_case_ref key = case_of("skewb", "sarah", "sarah", "s.1");
    BT_CHECK(bt_repo_cards_save_card(&db, &key, &plain, &err) == 0);
    bt_case_cards loaded;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_cards_load(&db, &cfg, &loaded, &load_err) == 0);
    BT_CHECK(bt_case_cards_len(&loaded) == 1);
    BT_CHECK(bt_case_cards_contains(&loaded, &key) == 1);
    bt_case_cards_free(&loaded);
    bt_srs_store_free(&pairs);
    bt_card_free(&plain);
    bt_db_close(&db);
}

BT_TEST(repo_cards, saving_a_card_replaces_only_its_own_row) {
    bt_db db;
    open_db(&db);
    bt_fsrs_config cfg;
    cfg_of(&cfg);
    bt_case_ref keep = case_of("skewb", "sarah", "sarah", "s.1");
    bt_case_ref retake = case_of("2x2", "cll", "cll", "cll.1");
    bt_card first = graded_card();
    bt_db_error err;
    BT_CHECK(bt_repo_cards_save_card(&db, &keep, &first, &err) == 0);
    bt_card fresh;
    bt_card_init(&fresh, 0);
    BT_CHECK(bt_repo_cards_save_card(&db, &retake, &fresh, &err) == 0);
    bt_card improved = graded_card();
    bt_card_review(&improved, BT_GRADE_EASY, 9, &cfg);
    BT_CHECK(bt_repo_cards_save_card(&db, &retake, &improved, &err) == 0);
    bt_case_cards loaded;
    bt_srs_load_error load_err;
    BT_CHECK(bt_repo_cards_load(&db, &cfg, &loaded, &load_err) == 0);
    BT_CHECK(bt_case_cards_len(&loaded) == 2);
    const bt_card *kept = bt_case_cards_get(&loaded, &keep);
    const bt_card *retaken = bt_case_cards_get(&loaded, &retake);
    BT_CHECK(kept != NULL && bt_card_eq(kept, &first) == 1);
    BT_CHECK(retaken != NULL && bt_card_eq(retaken, &improved) == 1);
    bt_case_cards_free(&loaded);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK(sqlite3_prepare_v2(bt_db_conn(&db), "SELECT COUNT(*) FROM review_log", -1, &stmt,
                                NULL) == SQLITE_OK);
    BT_CHECK(sqlite3_step(stmt) == SQLITE_ROW);
    BT_CHECK(sqlite3_column_int64(stmt, 0) ==
             (long long)(first.review_count + improved.review_count));
    sqlite3_finalize(stmt);
    bt_card_free(&first);
    bt_card_free(&fresh);
    bt_card_free(&improved);
    bt_db_close(&db);
}

BT_TEST(repo_cards, the_saved_row_carries_the_full_case_reference) {
    bt_db db;
    open_db(&db);
    bt_case_ref key = case_of("bigcube", "yau", "parity", "par.1");
    bt_card plain;
    bt_card_init(&plain, 0);
    bt_db_error err;
    BT_CHECK(bt_repo_cards_save_card(&db, &key, &plain, &err) == 0);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK(sqlite3_prepare_v2(bt_db_conn(&db), "SELECT event, method, stage, case_key FROM card",
                                -1, &stmt, NULL) == SQLITE_OK);
    BT_CHECK(sqlite3_step(stmt) == SQLITE_ROW);
    BT_CHECK_EQ((const char *)sqlite3_column_text(stmt, 0), "bigcube");
    BT_CHECK_EQ((const char *)sqlite3_column_text(stmt, 1), "yau");
    BT_CHECK_EQ((const char *)sqlite3_column_text(stmt, 2), "parity");
    BT_CHECK_EQ((const char *)sqlite3_column_text(stmt, 3), "par.1");
    sqlite3_finalize(stmt);
    bt_card_free(&plain);
    bt_db_close(&db);
}
