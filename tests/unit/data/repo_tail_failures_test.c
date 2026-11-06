#include "common/alloc.h"
#include "common/bt_harness.h"
#include "common/json.h"
#include "data/db.h"
#include "data/digest.h"
#include "data/repo_cards.h"
#include "data/repo_settings.h"
#include "data/repo_solves.h"
#include "data/seedpack/hashing.h"
#include "data/seedpack/mod.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

static const char *HEADER = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link";

static unsigned tail_counter;

static const char *tail_dir(const char *tag) {
    static char dir[BT_TU_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/bt-tail-failures/%s-%d-%u", base ? base : ".", tag, _getpid(),
             tail_counter++);
    tu_remove_tree(dir);
    tu_mkdir_p(dir);
    return dir;
}

static int deny_code;
static const char *deny_table;

static int deny_on_table(void *ctx, int code, const char *arg1, const char *arg2, const char *db,
                         const char *trigger) {
    (void)ctx;
    (void)arg2;
    (void)db;
    (void)trigger;
    if (code == deny_code && arg1 != NULL && strcmp(arg1, deny_table) == 0) {
        return SQLITE_DENY;
    }
    return SQLITE_OK;
}

static void deny_start(int code, const char *table) {
    deny_code = code;
    deny_table = table;
}

static bt_card plain_card(void) {
    bt_card card;
    bt_card_init(&card, 10);
    return card;
}

BT_TEST(tail_failures, settings_report_each_save_failure_step) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    deny_start(SQLITE_INSERT, "settings");
    sqlite3_set_authorizer(bt_db_conn(&db), deny_on_table, NULL);
    BT_CHECK(bt_repo_settings_save_tx(&db, "k", "v", &err) != 0);
    sqlite3_set_authorizer(bt_db_conn(&db), NULL, NULL);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_EXEC);
    BT_CHECK_EQ(err.step, "upsert settings");
    BT_CHECK_EQ(sqlite3_exec(bt_db_conn(&db), "BEGIN", NULL, NULL, NULL), SQLITE_OK);
    BT_CHECK(bt_repo_settings_save(&db, "k", "v", &err) != 0);
    sqlite3_exec(bt_db_conn(&db), "ROLLBACK", NULL, NULL, NULL);
    const char *dir = tail_dir("settings-commit");
    char db_path[BT_TU_PATH_MAX];
    snprintf(db_path, sizeof db_path, "%s/s.db", dir);
    bt_db file_db;
    BT_CHECK(bt_db_open_at(db_path, &file_db, &err) == 0);
    sqlite3 *reader = NULL;
    BT_CHECK_EQ(sqlite3_open(db_path, &reader), SQLITE_OK);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK_EQ(sqlite3_prepare_v2(reader, "SELECT COUNT(*) FROM sqlite_master", -1, &stmt, NULL),
                SQLITE_OK);
    BT_CHECK_EQ(sqlite3_step(stmt), SQLITE_ROW);
    BT_CHECK(bt_repo_settings_save(&file_db, "k", "v", &err) != 0);
    BT_CHECK_EQ(err.step, "commit settings");
    sqlite3_finalize(stmt);
    sqlite3_close(reader);
    bt_db_close(&file_db);
    bt_db_close(&db);
    tu_remove_tree(dir);
}

BT_TEST(tail_failures, settings_load_denies_locks_and_starves) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    deny_start(SQLITE_READ, "settings");
    sqlite3_set_authorizer(bt_db_conn(&db), deny_on_table, NULL);
    char *text = NULL;
    BT_CHECK(bt_repo_settings_load_text(&db, "k", &text, &err) != 0);
    sqlite3_set_authorizer(bt_db_conn(&db), NULL, NULL);
    BT_CHECK_EQ(err.kind, BT_DB_ERR_QUERY);
    BT_CHECK_EQ(err.step, "select settings");
    const char *dir = tail_dir("settings-lock");
    char db_path[BT_TU_PATH_MAX];
    snprintf(db_path, sizeof db_path, "%s/s.db", dir);
    bt_db file_db;
    BT_CHECK(bt_db_open_at(db_path, &file_db, &err) == 0);
    BT_CHECK(bt_repo_settings_save(&file_db, "k", "stored", &err) == 0);
    sqlite3 *locker = NULL;
    BT_CHECK_EQ(sqlite3_open(db_path, &locker), SQLITE_OK);
    BT_CHECK_EQ(sqlite3_exec(locker, "BEGIN EXCLUSIVE", NULL, NULL, NULL), SQLITE_OK);
    BT_CHECK(bt_repo_settings_load_text(&file_db, "k", &text, &err) != 0);
    BT_CHECK_EQ(err.step, "select settings");
    sqlite3_exec(locker, "ROLLBACK", NULL, NULL, NULL);
    sqlite3_close(locker);
    bt_alloc_fail_arm(0);
    int rc = bt_repo_settings_load_text(&file_db, "k", &text, &err);
    bt_alloc_fail_disarm();
    BT_CHECK(rc != 0);
    BT_CHECK_EQ(err.detail, "out of memory");
    BT_CHECK(bt_repo_settings_load_text(&file_db, "k", &text, &err) == 0);
    BT_CHECK_EQ(text, "stored");
    free(text);
    bt_db_close(&file_db);
    bt_db_close(&db);
    tu_remove_tree(dir);
}

BT_TEST(tail_failures, solves_report_each_save_and_load_failure) {
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    bt_solve_log log;
    bt_solve_log_init(&log);
    bt_solve_record record = {5, 6, 7, 1};
    bt_solve_log_push(&log, record);
    deny_start(SQLITE_DELETE, "solves");
    sqlite3_set_authorizer(bt_db_conn(&db), deny_on_table, NULL);
    BT_CHECK(bt_solve_log_save_tx(&db, &log, &err) != 0);
    BT_CHECK_EQ(err.step, "clear solves");
    sqlite3_set_authorizer(bt_db_conn(&db), NULL, NULL);
    deny_start(SQLITE_INSERT, "solves");
    sqlite3_set_authorizer(bt_db_conn(&db), deny_on_table, NULL);
    BT_CHECK(bt_solve_log_save_tx(&db, &log, &err) != 0);
    sqlite3_set_authorizer(bt_db_conn(&db), NULL, NULL);
    BT_CHECK_EQ(err.step, "insert solves");
    BT_CHECK_EQ(sqlite3_exec(bt_db_conn(&db), "BEGIN", NULL, NULL, NULL), SQLITE_OK);
    BT_CHECK(bt_solve_log_save(&db, &log, &err) != 0);
    sqlite3_exec(bt_db_conn(&db), "ROLLBACK", NULL, NULL, NULL);
    deny_start(SQLITE_READ, "solves");
    sqlite3_set_authorizer(bt_db_conn(&db), deny_on_table, NULL);
    bt_solve_log loaded;
    BT_CHECK(bt_solve_log_load(&db, &loaded, &err) != 0);
    sqlite3_set_authorizer(bt_db_conn(&db), NULL, NULL);
    BT_CHECK_EQ(err.step, "select solves");
    bt_solve_log_free(&loaded);
    bt_solve_log_free(&log);
    bt_db_close(&db);
}

BT_TEST(tail_failures, solve_log_push_and_from_json_survive_allocation_failure) {
    for (size_t ok = 0; ok < 10; ok++) {
        bt_solve_log log;
        bt_solve_log_init(&log);
        bt_alloc_fail_arm(ok);
        int rc = 0;
        for (int i = 0; i < 5; i++) {
            bt_solve_record record = {(long long)i, 2, 3, i % 2};
            if (bt_solve_log_push(&log, record) != 0) {
                rc = -1;
                break;
            }
        }
        bt_alloc_fail_disarm();
        if (rc == 0) {
            BT_CHECK_EQ((int)log.count, 5);
        }
        bt_solve_log_free(&log);
    }
    const char *text =
        "{\"solves\":[{\"started_ms\":1,\"memo_ms\":2,\"exec_ms\":3,\"success\":true},"
        "{\"started_ms\":4,\"memo_ms\":5,\"exec_ms\":6,\"success\":false}]}";
    bt_json_error jerr;
    bt_json_value *root = NULL;
    BT_CHECK(bt_json_parse(text, strlen(text), &root, &jerr) == 0);
    for (size_t ok = 0; ok < 6; ok++) {
        bt_solve_log log;
        bt_alloc_fail_arm(ok);
        int rc = bt_solve_log_from_json(root, &log);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            BT_CHECK_EQ((int)log.count, 2);
        }
        bt_solve_log_free(&log);
    }
    bt_solve_log log;
    BT_CHECK(bt_solve_log_from_json(root, &log) == 0);
    BT_CHECK_EQ((int)log.count, 2);
    bt_solve_log_free(&log);
    bt_json_free(root);
}

BT_TEST(tail_failures, cards_survive_each_allocation_failure_and_report_db_steps) {
    bt_case_cards cards;
    bt_case_cards_init(&cards);
    bt_case_ref a;
    bt_case_ref_of(&a, "2x2", "orto", "stage", "case-a");
    bt_case_ref b;
    bt_case_ref_of(&b, "2x2", "orto", "stage", "case-b");
    bt_case_ref c;
    bt_case_ref_of(&c, "2x2", "orto", "stage", "case-c");
    bt_card card = plain_card();
    bt_fsrs_config cfg;
    bt_fsrs_config_defaults(&cfg);
    bt_card rich;
    bt_card_init(&rich, 10);
    bt_card_review(&rich, BT_GRADE_GOOD, 10, &cfg);
    for (size_t ok = 0; ok < 10; ok++) {
        bt_alloc_fail_arm(ok);
        int rc = bt_case_cards_put(&cards, &a, &card);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            BT_CHECK_EQ((int)bt_case_cards_len(&cards), 1);
        }
    }
    BT_CHECK(bt_case_cards_put(&cards, &a, &card) == 0);
    BT_CHECK(bt_case_cards_put(&cards, &b, &card) == 0);
    BT_CHECK_EQ((int)bt_case_cards_len(&cards), 2);
    BT_CHECK(bt_case_cards_contains(&cards, &a) == 1);
    BT_CHECK(bt_case_cards_get(&cards, &b) != NULL);
    bt_alloc_fail_arm(0);
    BT_CHECK(bt_case_cards_entry(&cards, &c, &rich) == NULL);
    bt_alloc_fail_disarm();
    BT_CHECK(bt_case_cards_entry(&cards, &c, &rich) != NULL);
    BT_CHECK(bt_case_cards_entry(&cards, &a, &rich) != NULL);
    bt_card_free(&rich);
    bt_card_free(&card);
    bt_case_cards_free(&cards);
    bt_db db;
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(&db, &err) == 0);
    card = plain_card();
    deny_start(SQLITE_DELETE, "card");
    sqlite3_set_authorizer(bt_db_conn(&db), deny_on_table, NULL);
    BT_CHECK(bt_repo_cards_save_card(&db, &a, &card, &err) != 0);
    sqlite3_set_authorizer(bt_db_conn(&db), NULL, NULL);
    BT_CHECK_EQ(err.step, "clear case card");
    bt_card_free(&card);
    bt_db_close(&db);
}

BT_TEST(tail_failures, row_hash_starves_to_an_empty_marker) {
    bt_seed_row row;
    memset(&row, 0, sizeof row);
    snprintf(row.event, sizeof row.event, "%s", "3x3");
    snprintf(row.method, sizeof row.method, "%s", "cfop");
    snprintf(row.stage, sizeof row.stage, "%s", "pll");
    snprintf(row.case_key, sizeof row.case_key, "%s",
             "case-with-a-deliberately-long-key-that-pushes-the-join-past-the-buffer");
    row.alg = (char *)"R2 U2 R2 U2 R2 U2 R2 U2 R2 U2 R2 U2 R2 U2 R2 U2 R2 U2 R2 U2 R2 U2 R2 U2 "
                      "R2 U2 R2 U2 R2 U2 R2 U2 R2 U2 R2 U2 R2 U2 R2 U2 R2 U2 R2 U2";
    row.front_json = (char *)"{ }";
    row.source_url = (char *)"https://x";
    row.source_at = (char *)"2026-01-01";
    char hash[BT_SEED_HASH_LEN];
    bt_alloc_fail_arm(0);
    bt_seedpack_row_hash(&row, hash);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(hash, "");
    bt_alloc_fail_arm(1);
    bt_seedpack_row_hash(&row, hash);
    bt_alloc_fail_disarm();
    BT_CHECK_EQ(hash, "");
    bt_seedpack_row_hash(&row, hash);
    BT_CHECK_EQ((int)strlen(hash), 64);
    char twin[BT_SEED_HASH_LEN];
    bt_seedpack_row_hash(&row, twin);
    BT_CHECK_EQ(hash, twin);
}

BT_TEST(tail_failures, digest_pins_known_vectors_and_reports_each_failure) {
    char hex[BT_DIGEST_SHA256_HEX_LEN];
    bt_digest_sha256_hex("abc", 3, hex);
    BT_CHECK_EQ(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const unsigned char *parts[2] = {(const unsigned char *)"ab", (const unsigned char *)"c"};
    const size_t lens[2] = {2, 1};
    bt_digest_corpus_sha256_hex(parts, lens, 2, hex);
    BT_CHECK_EQ(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const char *dir = tail_dir("digest");
    static const char *solvers[2] = {"cherry", "eggins"};
    static const char *sheets[6] = {"edge", "corner", "flips", "twists", "parity", "ltct"};
    for (int s = 0; s < 2; s++) {
        for (int c = 0; c < 6; c++) {
            char path[BT_TU_PATH_MAX];
            snprintf(path, sizeof path, "%s/%s/%s.tsv", dir, solvers[s], sheets[c]);
            char body[256];
            snprintf(body, sizeof body, "%s\nP\tR U R'\t[A,B]\tl\tv\n", HEADER);
            tu_write_file(path, body);
        }
    }
    bt_corpus_digest digest;
    char err[512];
    BT_CHECK(bt_digest_corpus(dir, &digest, err, sizeof err) == 0);
    BT_CHECK_EQ((int)digest.file_count, 12);
    BT_CHECK_EQ((int)digest.rows, 12);
    BT_CHECK_EQ(digest.files[0].rows, 1);
    BT_CHECK_EQ(digest.files[0].solver, "cherry");
    BT_CHECK_EQ(digest.files[11].sheet, "ltct");
    BT_CHECK_EQ((int)strlen(digest.sha256), 64);
    const char *missing = tail_dir("digest-missing");
    BT_CHECK(bt_digest_corpus(missing, &digest, err, sizeof err) == 1);
    BT_CHECK(strstr(err, "read ") == err);
    char bad[BT_TU_PATH_MAX];
    snprintf(bad, sizeof bad, "%s/cherry/edge.tsv", dir);
    tu_write_file(bad, "wrong header\n");
    BT_CHECK(bt_digest_corpus(dir, &digest, err, sizeof err) == 1);
    BT_CHECK(strstr(err, "parse ") == err);
    for (size_t ok = 0; ok < 3; ok++) {
        bt_alloc_fail_arm(ok);
        int rc = bt_digest_corpus(dir, &digest, err, sizeof err);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            BT_CHECK_EQ((int)digest.file_count, 12);
        }
    }
    tu_remove_tree(dir);
    tu_remove_tree(missing);
}
