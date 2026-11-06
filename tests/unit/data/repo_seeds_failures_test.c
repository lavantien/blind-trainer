#include "common/alloc.h"
#include "common/bt_harness.h"
#include "data/db.h"
#include "data/paths.h"
#include "data/repo_seeds.h"
#include "data/seedpack/hashing.h"
#include "data/seedpack/mod.h"
#include "unit/data/test_util.h"

#include <stdio.h>
#include <string.h>

#include "sqlite3.h"

static const char *HEADER = "case_key\talg\tfront_json\tsource_url\tsource_at";

static unsigned seeds_counter;

static const char *packs_root(const char *tag) {
    static char dir[BT_TU_PATH_MAX];
    const char *base = getenv("TEMP");
    snprintf(dir, sizeof dir, "%s/bt-seeds-fail/%s-%d-%u", base ? base : ".", tag, _getpid(),
             seeds_counter++);
    tu_remove_tree(dir);
    tu_mkdir_p(dir);
    return dir;
}

static bt_pack_entry entry_of(const char *file, size_t rows) {
    bt_pack_entry entry;
    memset(&entry, 0, sizeof entry);
    snprintf(entry.file, sizeof entry.file, "%s", file);
    snprintf(entry.event, sizeof entry.event, "%s", "3x3");
    snprintf(entry.method, sizeof entry.method, "%s", "cfop");
    snprintf(entry.stage, sizeof entry.stage, "%s", "pll");
    entry.expected_rows = rows;
    return entry;
}

static void write_pack(const char *root, const char *file, const char *key) {
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/%s", root, file);
    char text[512];
    snprintf(text, sizeof text, "%s\n%s\tR U R'\t{}\thttps://x\t2026-01-01\n", HEADER, key);
    tu_write_file(path, text);
}

static void write_registry(const char *root, const char *body) {
    char path[BT_TU_PATH_MAX];
    snprintf(path, sizeof path, "%s/registry.json", root);
    tu_write_file(path, body);
}

static bool saw_seed_insert;

static int deny_seed_reads_after_insert(void *ctx, int code, const char *arg1, const char *arg2,
                                        const char *db, const char *trigger) {
    (void)ctx;
    (void)arg2;
    (void)db;
    (void)trigger;
    if (code == SQLITE_INSERT && arg1 != NULL && strcmp(arg1, "seed_alg") == 0) {
        saw_seed_insert = true;
        return SQLITE_OK;
    }
    if (code == SQLITE_READ && saw_seed_insert && arg1 != NULL && strcmp(arg1, "seed_alg") == 0) {
        return SQLITE_DENY;
    }
    return SQLITE_OK;
}

BT_TEST(seeds_failures, a_missing_pack_file_fails_seed_pack_with_io) {
    const char *root = packs_root("missing");
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_memory(&db, &db_err) == 0);
    bt_pack_entry entry = entry_of("probe.tsv", 1);
    bt_seed_report report;
    bt_seed_error err;
    BT_CHECK(bt_seed_pack(&db, root, &entry, &report, &err) != 0);
    BT_CHECK(err.kind == BT_SEED_ERR_IO);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(seeds_failures, an_open_transaction_fails_seed_pack_begin) {
    const char *root = packs_root("begin");
    write_pack(root, "probe.tsv", "A");
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_memory(&db, &db_err) == 0);
    BT_CHECK_EQ(sqlite3_exec(bt_db_conn(&db), "BEGIN", NULL, NULL, NULL), SQLITE_OK);
    bt_pack_entry entry = entry_of("probe.tsv", 1);
    bt_seed_report report;
    bt_seed_error err;
    BT_CHECK(bt_seed_pack(&db, root, &entry, &report, &err) != 0);
    BT_CHECK(err.kind == BT_SEED_ERR_DB);
    sqlite3_exec(bt_db_conn(&db), "ROLLBACK", NULL, NULL, NULL);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(seeds_failures, a_held_reader_fails_seed_pack_commit) {
    const char *root = packs_root("commit");
    write_pack(root, "probe.tsv", "A");
    char db_path[BT_TU_PATH_MAX];
    snprintf(db_path, sizeof db_path, "%s/seeds.db", root);
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_at(db_path, &db, &db_err) == 0);
    sqlite3 *reader = NULL;
    BT_CHECK_EQ(sqlite3_open(db_path, &reader), SQLITE_OK);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK_EQ(sqlite3_prepare_v2(reader, "SELECT COUNT(*) FROM sqlite_master", -1, &stmt, NULL),
                SQLITE_OK);
    BT_CHECK_EQ(sqlite3_step(stmt), SQLITE_ROW);
    bt_pack_entry entry = entry_of("probe.tsv", 1);
    bt_seed_report report;
    bt_seed_error err;
    BT_CHECK(bt_seed_pack(&db, root, &entry, &report, &err) != 0);
    BT_CHECK(err.kind == BT_SEED_ERR_DB);
    BT_CHECK_EQ(err.db.step, "commit seed_alg");
    sqlite3_finalize(stmt);
    sqlite3_close(reader);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(seeds_failures, the_stale_scan_prepare_denial_fails_the_verified_tx) {
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_memory(&db, &db_err) == 0);
    bt_seed_row row;
    memset(&row, 0, sizeof row);
    snprintf(row.event, sizeof row.event, "%s", "3x3");
    snprintf(row.method, sizeof row.method, "%s", "cfop");
    snprintf(row.stage, sizeof row.stage, "%s", "pll");
    snprintf(row.case_key, sizeof row.case_key, "%s", "A");
    row.alg = (char *)"R U R'";
    row.front_json = (char *)"{ }";
    row.source_url = (char *)"https://x";
    row.source_at = (char *)"2026-01-01";
    bt_seedpack_row_hash(&row, row.content_hash);
    bt_seed_report report;
    bt_seed_error err;
    BT_CHECK(bt_seed_pack_tx(&db, "probe.tsv", &row, 1, &report, &err) == 0);
    BT_CHECK_EQ((int)report.inserted, 1);
    saw_seed_insert = false;
    sqlite3_set_authorizer(bt_db_conn(&db), deny_seed_reads_after_insert, NULL);
    BT_CHECK(bt_seed_pack_tx(&db, "probe.tsv", &row, 1, &report, &err) != 0);
    sqlite3_set_authorizer(bt_db_conn(&db), NULL, NULL);
    BT_CHECK(err.kind == BT_SEED_ERR_DB);
    BT_CHECK_EQ(err.db.step, "select seed_alg");
    BT_CHECK(bt_seed_pack_tx(&db, "probe.tsv", &row, 1, &report, &err) == 0);
    BT_CHECK_EQ((int)report.verified, 1);
    bt_db_close(&db);
}

BT_TEST(seeds_failures, a_failed_pack_aborts_seed_all) {
    const char *root = packs_root("all-fail");
    write_registry(root,
                   "[{\"file\":\"a.tsv\",\"event\":\"3x3\",\"method\":\"cfop\",\"stage\":\"pll\","
                   "\"expected_rows\":1}]");
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_memory(&db, &db_err) == 0);
    bt_seed_report *reports = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seed_all(&db, root, &reports, &count, &err) != 0);
    BT_CHECK(reports == NULL && count == 0);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(seeds_failures, seed_all_reports_each_pack_and_frees_cleanly) {
    const char *root = packs_root("all-ok");
    write_pack(root, "a.tsv", "A");
    write_pack(root, "b.tsv", "B");
    write_registry(root,
                   "[{\"file\":\"a.tsv\",\"event\":\"3x3\",\"method\":\"cfop\",\"stage\":\"pll\","
                   "\"expected_rows\":1},"
                   "{\"file\":\"b.tsv\",\"event\":\"3x3\",\"method\":\"cfop\",\"stage\":\"oll\","
                   "\"expected_rows\":1}]");
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_memory(&db, &db_err) == 0);
    bt_seed_report *reports = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seed_all(&db, root, &reports, &count, &err) == 0);
    BT_CHECK_EQ((int)count, 2);
    BT_CHECK_EQ((int)reports[0].inserted, 1);
    BT_CHECK_EQ((int)reports[1].inserted, 1);
    bt_seed_reports_free(reports);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(seeds_failures, a_locked_database_fails_load_seed_rows) {
    const char *root = packs_root("locked");
    write_pack(root, "probe.tsv", "A");
    char db_path[BT_TU_PATH_MAX];
    snprintf(db_path, sizeof db_path, "%s/seeds.db", root);
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_at(db_path, &db, &db_err) == 0);
    bt_pack_entry entry = entry_of("probe.tsv", 1);
    bt_seed_report report;
    bt_seed_error err;
    BT_CHECK(bt_seed_pack(&db, root, &entry, &report, &err) == 0);
    sqlite3 *locker = NULL;
    BT_CHECK_EQ(sqlite3_open(db_path, &locker), SQLITE_OK);
    BT_CHECK_EQ(sqlite3_exec(locker, "BEGIN EXCLUSIVE", NULL, NULL, NULL), SQLITE_OK);
    bt_seed_rows rows;
    BT_CHECK(bt_load_seed_rows(&db, &rows, &err) != 0);
    BT_CHECK(err.kind == BT_SEED_ERR_DB);
    sqlite3_exec(locker, "ROLLBACK", NULL, NULL, NULL);
    sqlite3_close(locker);
    BT_CHECK(bt_load_seed_rows(&db, &rows, &err) == 0);
    BT_CHECK_EQ((int)rows.count, 1);
    bt_seed_rows_free(&rows);
    bt_db_close(&db);
    tu_remove_tree(root);
}

BT_TEST(seeds_failures, load_seed_rows_fails_closed_under_each_allocation_failure) {
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_memory(&db, &db_err) == 0);
    bt_seed_row row;
    memset(&row, 0, sizeof row);
    snprintf(row.event, sizeof row.event, "%s", "3x3");
    snprintf(row.method, sizeof row.method, "%s", "cfop");
    snprintf(row.stage, sizeof row.stage, "%s", "pll");
    snprintf(row.case_key, sizeof row.case_key, "%s", "A");
    row.alg = (char *)"R U R'";
    row.front_json = (char *)"{ }";
    row.source_url = (char *)"https://x";
    row.source_at = (char *)"2026-01-01";
    bt_seedpack_row_hash(&row, row.content_hash);
    bt_seed_report report;
    bt_seed_error err;
    BT_CHECK(bt_seed_pack_tx(&db, "probe.tsv", &row, 1, &report, &err) == 0);
    for (size_t ok = 0; ok < 8; ok++) {
        bt_seed_rows rows;
        bt_alloc_fail_arm(ok);
        int rc = bt_load_seed_rows(&db, &rows, &err);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            BT_CHECK_EQ((int)rows.count, 1);
        }
        bt_seed_rows_free(&rows);
    }
    bt_seed_rows rows;
    BT_CHECK(bt_load_seed_rows(&db, &rows, &err) == 0);
    BT_CHECK_EQ((int)rows.count, 1);
    bt_seed_rows_free(&rows);
    bt_db_close(&db);
}

BT_TEST(seeds_failures, seed_all_fails_closed_under_each_allocation_failure) {
    const char *root = packs_root("all-oom");
    write_pack(root, "a.tsv", "A");
    write_registry(root,
                   "[{\"file\":\"a.tsv\",\"event\":\"3x3\",\"method\":\"cfop\",\"stage\":\"pll\","
                   "\"expected_rows\":1}]");
    bt_db db;
    bt_db_error db_err;
    BT_CHECK(bt_db_open_memory(&db, &db_err) == 0);
    for (size_t ok = 0; ok < 6; ok++) {
        bt_seed_report *reports = NULL;
        size_t count = 0;
        bt_seed_error err;
        bt_alloc_fail_arm(ok);
        int rc = bt_seed_all(&db, root, &reports, &count, &err);
        bt_alloc_fail_disarm();
        if (rc == 0) {
            BT_CHECK_EQ((int)count, 1);
            bt_seed_reports_free(reports);
        }
    }
    bt_seed_report *reports = NULL;
    size_t count = 0;
    bt_seed_error err;
    BT_CHECK(bt_seed_all(&db, root, &reports, &count, &err) == 0);
    BT_CHECK_EQ((int)count, 1);
    bt_seed_reports_free(reports);
    bt_db_close(&db);
    tu_remove_tree(root);
}
