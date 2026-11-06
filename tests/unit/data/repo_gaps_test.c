#include "common/alloc.h"
#include "common/bt_harness.h"
#include "data/db.h"
#include "data/fsrs.h"
#include "data/repo_algs.h"
#include "data/repo_cards.h"
#include "data/repo_seeds.h"
#include "data/repo_settings.h"
#include "data/repo_solves.h"
#include "data/repo_srs.h"
#include "data/repo_user.h"
#include "data/seedpack/mod.h"
#include "unit/data/test_util.h"

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

static unsigned repo_gaps_counter;

static const char *repo_gaps_dir(const char *tag) {
    static char dir[BT_TU_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/bt-repo-gaps/%s-%d-%u", base ? base : ".", tag, _getpid(),
             repo_gaps_counter++);
    tu_remove_tree(dir);
    tu_mkdir_p(dir);
    return dir;
}

static void raw_exec(bt_db *db, const char *sql) {
    char *err = NULL;
    BT_CHECK(sqlite3_exec(bt_db_conn(db), sql, NULL, NULL, &err) == SQLITE_OK);
}

static void open_memory(bt_db *db) {
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(db, &err) == 0);
}

static void open_migrated_file(const char *tag, bt_db *db, char *path_out, size_t path_len) {
    const char *dir = repo_gaps_dir(tag);
    snprintf(path_out, path_len, "%s/custom.db", dir);
    bt_db_error err;
    BT_CHECK(bt_db_open_at(path_out, db, &err) == 0);
}

static bt_card plain_card(void) {
    bt_card card;
    bt_card_init(&card, 0);
    card.due_day = 7;
    card.interval_days = 3;
    return card;
}

static bt_card reviewed_card(void) {
    bt_card card = plain_card();
    card.review_count = 2;
    card.reviews = malloc(2 * sizeof(bt_review_entry));
    card.reviews[0].ts_days = 1;
    card.reviews[0].quality = 3;
    card.reviews[0].elapsed_days = 0;
    card.reviews[0].scheduled_days = 1;
    card.reviews[1].ts_days = 2;
    card.reviews[1].quality = 4;
    card.reviews[1].elapsed_days = 1;
    card.reviews[1].scheduled_days = 2;
    return card;
}

static int deny_review_log(void *ctx, int code, const char *arg1, const char *arg2,
                           const char *db_name, const char *trigger) {
    (void)ctx;
    (void)arg2;
    (void)db_name;
    (void)trigger;
    if (code == SQLITE_READ && arg1 != NULL && strcmp(arg1, "review_log") == 0) {
        return SQLITE_DENY;
    }
    return SQLITE_OK;
}

static sqlite3_stmt *held_reader(const char *path, const char *sql, sqlite3 **conn_out) {
    sqlite3 *conn = NULL;
    BT_CHECK(sqlite3_open(path, &conn) == SQLITE_OK);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK(sqlite3_prepare_v2(conn, sql, -1, &stmt, NULL) == SQLITE_OK);
    BT_CHECK(sqlite3_step(stmt) == SQLITE_ROW);
    *conn_out = conn;
    return stmt;
}

BT_TEST(repo_gaps, srs_save_rolls_back_when_the_clear_step_fails) {
    bt_db db;
    open_memory(&db);
    raw_exec(&db, "INSERT INTO card(id, event, method, stage, case_key, created_at, updated_at) "
                  "VALUES (1, '3bld', '3style', 'edge', 'UF', 1, 1)");
    raw_exec(&db, "CREATE TRIGGER block_card_delete BEFORE DELETE ON card "
                  "BEGIN SELECT RAISE(ABORT, 'no card delete'); END");
    bt_srs_store store;
    bt_srs_store_init(&store);
    bt_card card = plain_card();
    BT_CHECK(bt_srs_store_set(&store, "edge/UF", &card) == 0);
    bt_db_error err;
    BT_CHECK(bt_repo_srs_save(&db, &store, &err) != 0);
    BT_CHECK(err.kind == BT_DB_ERR_EXEC);
    BT_CHECK_EQ(err.step, "clear card");
    BT_CHECK(strstr(err.detail, "no card delete") != NULL);
    bt_srs_store_free(&store);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_gaps, srs_load_rows_fails_when_the_reviews_prepare_is_denied) {
    bt_db db;
    open_memory(&db);
    sqlite3_set_authorizer(bt_db_conn(&db), deny_review_log, NULL);
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_srs_load_error err;
    bt_srs_card_row *rows = NULL;
    size_t count = 0;
    BT_CHECK(bt_repo_srs_load_rows(&db, &cfg, BT_SRS_FILTER_ONLY_3BLD, &rows, &count, &err) != 0);
    BT_CHECK(err.kind == BT_SRS_LOAD_DB);
    BT_CHECK(err.db.kind == BT_DB_ERR_QUERY);
    BT_CHECK_EQ(err.db.step, "select card");
    BT_CHECK(strstr(err.db.detail, "access to review_log.rating is prohibited") != NULL);
    sqlite3_set_authorizer(bt_db_conn(&db), NULL, NULL);
    bt_db_close(&db);
}

BT_TEST(repo_gaps, srs_store_set_replace_clone_starves) {
    bt_card card = reviewed_card();
    int saw_replace_fail = 0;
    for (size_t ok = 0; ok < 6; ok++) {
        bt_srs_store store;
        bt_srs_store_init(&store);
        bt_alloc_fail_arm(ok);
        int rc_first = bt_srs_store_set(&store, "edge/UF", &card);
        int rc_replace = bt_srs_store_set(&store, "edge/UF", &card);
        bt_alloc_fail_disarm();
        if (rc_first == 0 && rc_replace != 0) {
            saw_replace_fail = 1;
        }
        if (rc_replace == 0) {
            BT_CHECK(rc_first == 0);
            BT_CHECK(bt_srs_store_len(&store) == 1);
        }
        bt_srs_store_free(&store);
    }
    BT_CHECK(saw_replace_fail == 1);
    bt_srs_store store;
    bt_srs_store_init(&store);
    BT_CHECK(bt_srs_store_set(&store, "edge/UF", &card) == 0);
    BT_CHECK(bt_srs_store_set(&store, "edge/UF", &card) == 0);
    BT_CHECK(bt_srs_store_len(&store) == 1);
    bt_srs_store_free(&store);
    bt_card_free(&card);
}

BT_TEST(repo_gaps, card_from_json_accepts_a_valid_memory) {
    const char *good =
        "{\"cards\":{\"edge/UF\":{\"memory\":{\"stability\":1.5,\"difficulty\":5.5},"
        "\"interval_days\":2,\"due_day\":3,\"reps\":1,\"lapses\":0,\"reviews\":[]}}}";
    bt_json_value *root = NULL;
    bt_json_error parse_err;
    BT_CHECK(bt_json_parse(good, strlen(good), &root, &parse_err) == 0);
    bt_srs_store store;
    BT_CHECK(bt_srs_store_from_json(root, &store) == 0);
    const bt_card *card = bt_srs_store_get(&store, "edge/UF");
    BT_CHECK(card != NULL);
    BT_CHECK(card->has_memory == 1);
    BT_CHECK_F64_NEAR(card->memory.stability, 1.5, 1e-12);
    BT_CHECK_F64_NEAR(card->memory.difficulty, 5.5, 1e-12);
    bt_srs_store_free(&store);
    bt_json_free(root);
}

BT_TEST(repo_gaps, overrides_remove_shifts_the_tail) {
    bt_overrides ov;
    bt_overrides_init(&ov);
    bt_case_ref t;
    bt_case_ref_of(&t, "3x3", "cfop", "pll", "T");
    bt_case_ref z;
    bt_case_ref_of(&z, "3x3", "cfop", "pll", "Z");
    bt_case_ref a;
    bt_case_ref_of(&a, "3x3", "cfop", "pll", "A");
    BT_CHECK(bt_overrides_put(&ov, &t, "R U R'") == 0);
    BT_CHECK(bt_overrides_put(&ov, &z, "F R U") == 0);
    BT_CHECK(bt_overrides_put(&ov, &a, "U' L'") == 0);
    BT_CHECK(bt_overrides_len(&ov) == 3);
    BT_CHECK(bt_overrides_remove(&ov, &a) == 1);
    BT_CHECK(bt_overrides_len(&ov) == 2);
    BT_CHECK(bt_overrides_contains(&ov, &t) == 1);
    BT_CHECK(bt_overrides_contains(&ov, &z) == 1);
    BT_CHECK(bt_overrides_contains(&ov, &a) == 0);
    BT_CHECK(strcmp(ov.items[0].alg, "U' L'") == 0 || strcmp(ov.items[0].alg, "R U R'") == 0);
    bt_overrides_free(&ov);
}

BT_TEST(repo_gaps, overrides_insert_starves_on_each_arm) {
    bt_case_ref t;
    bt_case_ref_of(&t, "3x3", "cfop", "pll", "T");
    bt_case_ref z;
    bt_case_ref_of(&z, "3x3", "cfop", "pll", "Z");
    int saw_fail = 0;
    for (size_t ok = 0; ok < 6; ok++) {
        bt_overrides ov;
        bt_overrides_init(&ov);
        bt_alloc_fail_arm(ok);
        int rc_put = bt_overrides_put(&ov, &t, "R U R'");
        int rc_replace = bt_overrides_put(&ov, &t, "F R U");
        int rc_new = bt_overrides_put(&ov, &z, "U");
        bt_alloc_fail_disarm();
        if (rc_put != 0 || rc_replace != 0 || rc_new != 0) {
            saw_fail = 1;
        }
        if (rc_new == 0 && rc_replace == 0 && rc_put == 0) {
            BT_CHECK(bt_overrides_len(&ov) == 2);
            BT_CHECK(strcmp(bt_overrides_get(&ov, &t), "F R U") == 0);
        }
        bt_overrides_free(&ov);
    }
    BT_CHECK(saw_fail == 1);
    bt_overrides ov;
    bt_overrides_init(&ov);
    BT_CHECK(bt_overrides_put(&ov, &t, "R U R'") == 0);
    BT_CHECK(bt_overrides_put(&ov, &t, "F R U") == 0);
    BT_CHECK(bt_overrides_put(&ov, &z, "U") == 0);
    BT_CHECK(bt_overrides_len(&ov) == 2);
    bt_overrides_free(&ov);
}

BT_TEST(repo_gaps, user_upsert_fails_when_begin_fails) {
    bt_db db;
    open_memory(&db);
    bt_db_error err;
    BT_CHECK(bt_db_begin(&db, &err) == 0);
    bt_case_ref ref;
    bt_case_ref_of(&ref, "3x3", "cfop", "pll", "T");
    BT_CHECK(bt_repo_user_upsert(&db, &ref, "alg", &err) != 0);
    BT_CHECK_EQ(err.step, "begin");
    bt_db_rollback(&db);
    bt_db_close(&db);
}

BT_TEST(repo_gaps, user_upsert_commit_fails_under_a_held_reader) {
    bt_db db;
    char path[BT_TU_PATH_MAX];
    open_migrated_file("user-commit-reader", &db, path, sizeof path);
    sqlite3 *seed = NULL;
    BT_CHECK(sqlite3_open(path, &seed) == SQLITE_OK);
    char *seed_err = NULL;
    BT_CHECK(sqlite3_exec(seed,
                          "INSERT INTO user_alg(event, method, stage, case_key, alg, "
                          "created_at, updated_at) VALUES ('3x3', 'cfop', 'pll', 'J', 'seed', "
                          "1, 1)",
                          NULL, NULL, &seed_err) == SQLITE_OK);
    sqlite3_close(seed);
    sqlite3 *reader_conn = NULL;
    sqlite3_stmt *reader = held_reader(path, "SELECT alg FROM user_alg", &reader_conn);
    bt_case_ref ref;
    bt_case_ref_of(&ref, "3x3", "cfop", "pll", "T");
    bt_db_error err;
    BT_CHECK(bt_repo_user_upsert(&db, &ref, "alg", &err) != 0);
    BT_CHECK_EQ(err.step, "commit user_alg");
    sqlite3_finalize(reader);
    sqlite3_close(reader_conn);
    bt_db_close(&db);
}

BT_TEST(repo_gaps, user_load_all_starves_mid_insert) {
    bt_db db;
    open_memory(&db);
    raw_exec(&db, "INSERT INTO user_alg(event, method, stage, case_key, alg, created_at, "
                  "updated_at) VALUES ('3x3', 'cfop', 'pll', 'T', 'R U', 1, 1);"
                  "INSERT INTO user_alg(event, method, stage, case_key, alg, created_at, "
                  "updated_at) VALUES ('3x3', 'cfop', 'pll', 'Z', 'F R', 1, 1)");
    int saw_fail = 0;
    for (size_t ok = 0; ok < 5; ok++) {
        bt_overrides ov;
        bt_alloc_fail_arm(ok);
        bt_db_error err;
        int rc = bt_repo_user_load_all(&db, &ov, &err);
        bt_alloc_fail_disarm();
        if (rc != 0) {
            saw_fail = 1;
        } else {
            BT_CHECK(bt_overrides_len(&ov) == 2);
        }
        bt_overrides_free(&ov);
    }
    BT_CHECK(saw_fail == 1);
    bt_overrides ov;
    bt_db_error err;
    BT_CHECK(bt_repo_user_load_all(&db, &ov, &err) == 0);
    BT_CHECK(bt_overrides_len(&ov) == 2);
    bt_overrides_free(&ov);
    bt_db_close(&db);
}

BT_TEST(repo_gaps, algs_save_tx_clear_step_fails_under_a_trigger) {
    bt_db db;
    open_memory(&db);
    raw_exec(&db, "INSERT INTO user_alg(event, method, stage, case_key, alg, created_at, "
                  "updated_at) VALUES ('3bld', '3style', 'edge', 'UF', 'R U', 1, 1)");
    raw_exec(&db, "CREATE TRIGGER block_alg_clear BEFORE DELETE ON user_alg "
                  "BEGIN SELECT RAISE(ABORT, 'no alg clear'); END");
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    bt_db_error err;
    BT_CHECK(bt_repo_algs_save_tx(&db, &algs, &err) != 0);
    BT_CHECK_EQ(err.step, "clear user_alg");
    BT_CHECK(strstr(err.detail, "no alg clear") != NULL);
    bt_user_algs_free(&algs);
    bt_db_close(&db);
}

BT_TEST(repo_gaps, algs_save_tx_insert_step_fails_under_a_trigger) {
    bt_db db;
    open_memory(&db);
    raw_exec(&db, "CREATE TRIGGER block_alg_insert BEFORE INSERT ON user_alg "
                  "BEGIN SELECT RAISE(ABORT, 'no alg insert'); END");
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    BT_CHECK(bt_user_algs_set_entry(&algs, "edge/UF", "R U R'") == 0);
    bt_db_error err;
    BT_CHECK(bt_repo_algs_save_tx(&db, &algs, &err) != 0);
    BT_CHECK_EQ(err.step, "insert user_alg");
    BT_CHECK(strstr(err.detail, "no alg insert") != NULL);
    bt_user_algs_free(&algs);
    bt_db_close(&db);
}

BT_TEST(repo_gaps, algs_save_commit_fails_under_a_held_reader) {
    bt_db db;
    char path[BT_TU_PATH_MAX];
    open_migrated_file("algs-commit-reader", &db, path, sizeof path);
    sqlite3 *seed = NULL;
    BT_CHECK(sqlite3_open(path, &seed) == SQLITE_OK);
    char *seed_err = NULL;
    BT_CHECK(sqlite3_exec(seed,
                          "INSERT INTO user_alg(event, method, stage, case_key, alg, "
                          "created_at, updated_at) VALUES ('3bld', '3style', 'edge', 'UF', "
                          "'R U', 1, 1)",
                          NULL, NULL, &seed_err) == SQLITE_OK);
    sqlite3_close(seed);
    sqlite3 *reader_conn = NULL;
    sqlite3_stmt *reader = held_reader(path, "SELECT alg FROM user_alg", &reader_conn);
    bt_user_algs algs;
    bt_user_algs_init(&algs);
    BT_CHECK(bt_user_algs_set_entry(&algs, "edge/UF", "R U R'") == 0);
    bt_db_error err;
    BT_CHECK(bt_repo_algs_save(&db, &algs, &err) != 0);
    BT_CHECK_EQ(err.step, "commit user_alg");
    sqlite3_finalize(reader);
    sqlite3_close(reader_conn);
    bt_user_algs_free(&algs);
    bt_db_close(&db);
}

BT_TEST(repo_gaps, algs_load_starves_mid_insert) {
    bt_db db;
    open_memory(&db);
    raw_exec(&db, "INSERT INTO user_alg(event, method, stage, case_key, alg, created_at, "
                  "updated_at) VALUES ('3bld', '3style', 'edge', 'UF', 'R U', 1, 1)");
    int saw_fail = 0;
    for (size_t ok = 0; ok < 5; ok++) {
        bt_user_algs algs;
        bt_alloc_fail_arm(ok);
        bt_db_error err;
        int rc = bt_repo_algs_load(&db, &algs, &err);
        bt_alloc_fail_disarm();
        if (rc != 0) {
            saw_fail = 1;
        } else {
            BT_CHECK(bt_user_algs_len(&algs) == 1);
        }
        bt_user_algs_free(&algs);
    }
    BT_CHECK(saw_fail == 1);
    bt_user_algs algs;
    bt_db_error err;
    BT_CHECK(bt_repo_algs_load(&db, &algs, &err) == 0);
    BT_CHECK(bt_user_algs_len(&algs) == 1);
    bt_user_algs_free(&algs);
    bt_db_close(&db);
}

BT_TEST(repo_gaps, cards_insert_before_the_first_entry_shifts) {
    bt_case_cards cards;
    bt_case_cards_init(&cards);
    bt_case_ref r;
    bt_case_ref_of(&r, "skewb", "beginner", "clamp", "R");
    bt_case_ref l;
    bt_case_ref_of(&l, "skewb", "beginner", "clamp", "L");
    bt_card card = plain_card();
    BT_CHECK(bt_case_cards_put(&cards, &r, &card) == 0);
    BT_CHECK(bt_case_cards_put(&cards, &l, &card) == 0);
    BT_CHECK(bt_case_cards_len(&cards) == 2);
    BT_CHECK_EQ(cards.items[0].ref.case_key, "L");
    BT_CHECK_EQ(cards.items[1].ref.case_key, "R");
    bt_case_cards_free(&cards);
    bt_card_free(&card);
}

BT_TEST(repo_gaps, cards_put_replace_clone_starves) {
    bt_card card = reviewed_card();
    int saw_replace_fail = 0;
    for (size_t ok = 0; ok < 5; ok++) {
        bt_case_cards cards;
        bt_case_cards_init(&cards);
        bt_case_ref ref;
        bt_case_ref_of(&ref, "skewb", "beginner", "clamp", "L");
        bt_alloc_fail_arm(ok);
        int rc_put = bt_case_cards_put(&cards, &ref, &card);
        int rc_replace = bt_case_cards_put(&cards, &ref, &card);
        bt_alloc_fail_disarm();
        if (rc_put == 0 && rc_replace != 0) {
            saw_replace_fail = 1;
        }
        if (rc_replace == 0) {
            BT_CHECK(rc_put == 0);
            BT_CHECK(bt_case_cards_len(&cards) == 1);
        }
        bt_case_cards_free(&cards);
    }
    BT_CHECK(saw_replace_fail == 1);
    bt_case_cards cards;
    bt_case_cards_init(&cards);
    bt_case_ref ref;
    bt_case_ref_of(&ref, "skewb", "beginner", "clamp", "L");
    BT_CHECK(bt_case_cards_put(&cards, &ref, &card) == 0);
    BT_CHECK(bt_case_cards_put(&cards, &ref, &card) == 0);
    BT_CHECK(bt_case_cards_len(&cards) == 1);
    bt_case_cards_free(&cards);
    bt_card_free(&card);
}

BT_TEST(repo_gaps, cards_load_starves_mid_insert) {
    bt_db db;
    open_memory(&db);
    raw_exec(&db, "INSERT INTO card(id, event, method, stage, case_key, created_at, updated_at) "
                  "VALUES (1, 'skewb', 'beginner', 'clamp', 'L', 1, 1);"
                  "INSERT INTO srs_state(card_id, due_day, interval_days, reps, lapses, "
                  "last_review_day, stability, difficulty) VALUES (1, 2, 1, 1, 0, 1, NULL, NULL)");
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    int saw_fail = 0;
    for (size_t ok = 0; ok < 4; ok++) {
        bt_case_cards cards;
        bt_alloc_fail_arm(ok);
        bt_srs_load_error err;
        int rc = bt_repo_cards_load(&db, &cfg, &cards, &err);
        bt_alloc_fail_disarm();
        if (rc != 0) {
            saw_fail = 1;
        } else {
            BT_CHECK(bt_case_cards_len(&cards) == 1);
        }
        bt_case_cards_free(&cards);
    }
    BT_CHECK(saw_fail == 1);
    bt_case_cards cards;
    bt_srs_load_error err;
    BT_CHECK(bt_repo_cards_load(&db, &cfg, &cards, &err) == 0);
    BT_CHECK(bt_case_cards_len(&cards) == 1);
    bt_case_cards_free(&cards);
    bt_db_close(&db);
}

BT_TEST(repo_gaps, cards_save_card_clear_step_fails_under_a_trigger) {
    bt_db db;
    open_memory(&db);
    raw_exec(&db, "INSERT INTO card(id, event, method, stage, case_key, created_at, updated_at) "
                  "VALUES (1, 'skewb', 'beginner', 'clamp', 'L', 1, 1)");
    raw_exec(&db, "CREATE TRIGGER block_case_card_delete BEFORE DELETE ON card "
                  "BEGIN SELECT RAISE(ABORT, 'no case delete'); END");
    bt_case_ref ref;
    bt_case_ref_of(&ref, "skewb", "beginner", "clamp", "L");
    bt_card card = plain_card();
    bt_db_error err;
    BT_CHECK(bt_repo_cards_save_card(&db, &ref, &card, &err) != 0);
    BT_CHECK_EQ(err.step, "clear case card");
    BT_CHECK(strstr(err.detail, "no case delete") != NULL);
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(repo_gaps, solves_save_rolls_back_when_the_insert_step_fails) {
    bt_db db;
    open_memory(&db);
    raw_exec(&db, "CREATE TRIGGER block_solve_insert BEFORE INSERT ON solves "
                  "BEGIN SELECT RAISE(ABORT, 'no solve insert'); END");
    bt_solve_log log;
    bt_solve_log_init(&log);
    bt_solve_record record = {1000, 200, 300, 1};
    BT_CHECK(bt_solve_log_push(&log, record) == 0);
    bt_db_error err;
    BT_CHECK(bt_solve_log_save(&db, &log, &err) != 0);
    BT_CHECK_EQ(err.step, "insert solves");
    BT_CHECK(strstr(err.detail, "no solve insert") != NULL);
    bt_solve_log_free(&log);
    bt_db_close(&db);
}

BT_TEST(repo_gaps, solves_save_commit_fails_under_a_held_reader) {
    bt_db db;
    char path[BT_TU_PATH_MAX];
    open_migrated_file("solves-commit-reader", &db, path, sizeof path);
    sqlite3 *seed = NULL;
    BT_CHECK(sqlite3_open(path, &seed) == SQLITE_OK);
    char *seed_err = NULL;
    BT_CHECK(sqlite3_exec(seed,
                          "INSERT INTO solves(started_ms, memo_ms, exec_ms, success) "
                          "VALUES (1, 2, 3, 1)",
                          NULL, NULL, &seed_err) == SQLITE_OK);
    sqlite3_close(seed);
    sqlite3 *reader_conn = NULL;
    sqlite3_stmt *reader = held_reader(path, "SELECT started_ms FROM solves", &reader_conn);
    bt_solve_log log;
    bt_solve_log_init(&log);
    bt_solve_record record = {1000, 200, 300, 1};
    BT_CHECK(bt_solve_log_push(&log, record) == 0);
    bt_db_error err;
    BT_CHECK(bt_solve_log_save(&db, &log, &err) != 0);
    BT_CHECK_EQ(err.step, "commit solves");
    sqlite3_finalize(reader);
    sqlite3_close(reader_conn);
    bt_solve_log_free(&log);
    bt_db_close(&db);
}

BT_TEST(repo_gaps, solves_load_starves_mid_push) {
    bt_db db;
    open_memory(&db);
    raw_exec(&db, "INSERT INTO solves(started_ms, memo_ms, exec_ms, success) VALUES (1, 2, 3, 1);"
                  "INSERT INTO solves(started_ms, memo_ms, exec_ms, success) VALUES (4, 5, 6, 0)");
    int saw_fail = 0;
    for (size_t ok = 0; ok < 3; ok++) {
        bt_solve_log log;
        bt_alloc_fail_arm(ok);
        bt_db_error err;
        int rc = bt_solve_log_load(&db, &log, &err);
        bt_alloc_fail_disarm();
        if (rc != 0) {
            saw_fail = 1;
        } else {
            BT_CHECK(log.count == 2);
        }
        bt_solve_log_free(&log);
    }
    BT_CHECK(saw_fail == 1);
    bt_solve_log log;
    bt_db_error err;
    BT_CHECK(bt_solve_log_load(&db, &log, &err) == 0);
    BT_CHECK(log.count == 2);
    bt_solve_log_free(&log);
    bt_db_close(&db);
}

BT_TEST(repo_gaps, seeds_all_starves_on_the_reports_array) {
    const char *dir = repo_gaps_dir("seeds-all-starve");
    char registry_path[BT_TU_PATH_MAX];
    snprintf(registry_path, sizeof registry_path, "%s/registry.json", dir);
    tu_write_file(registry_path, "[{\"file\":\"probe.tsv\",\"event\":\"3x3\",\"method\":\"cfop\","
                                 "\"stage\":\"pll\",\"expected_rows\":1}]");
    char pack_path[BT_TU_PATH_MAX];
    snprintf(pack_path, sizeof pack_path, "%s/probe.tsv", dir);
    tu_write_file(pack_path, "case_key\talg\tfront_json\tsource_url\tsource_at\n"
                             "T\tR U R'\t{}\thttps://x\t2026-01-01\n");
    bt_db db;
    open_memory(&db);
    int saw_fail = 0;
    for (size_t ok = 0; ok < 32; ok++) {
        bt_seed_report *reports = NULL;
        size_t count = 0;
        bt_seed_error err;
        bt_alloc_fail_arm(ok);
        int rc = bt_seed_all(&db, dir, &reports, &count, &err);
        bt_alloc_fail_disarm();
        if (rc != 0) {
            saw_fail = 1;
            BT_CHECK(reports == NULL);
        } else {
            BT_CHECK(count == 1 && reports != NULL);
            bt_seed_reports_free(reports);
        }
    }
    BT_CHECK(saw_fail == 1);
    bt_seed_report *reports = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seed_all(&db, dir, &reports, &count, &err) == 0);
    BT_CHECK(count == 1 && reports[0].inserted == 1);
    bt_seed_reports_free(reports);
    bt_db_close(&db);
}

BT_TEST(repo_gaps, settings_save_rolls_back_when_the_upsert_step_fails) {
    bt_db db;
    open_memory(&db);
    raw_exec(&db, "CREATE TRIGGER block_settings_insert BEFORE INSERT ON settings "
                  "BEGIN SELECT RAISE(ABORT, 'no settings'); END");
    bt_db_error err;
    BT_CHECK(bt_repo_settings_save(&db, "k", "v", &err) != 0);
    BT_CHECK_EQ(err.step, "upsert settings");
    BT_CHECK(strstr(err.detail, "no settings") != NULL);
    bt_db_close(&db);
}
