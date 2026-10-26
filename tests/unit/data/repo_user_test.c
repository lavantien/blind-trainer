#include "common/bt_harness.h"
#include "data/repo_algs.h"
#include "data/repo_user.h"

#include <string.h>

#include "sqlite3.h"

static void open_db(bt_db *db) {
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(db, &err) == 0);
}

static bt_case_ref key_of(const char *case_key) {
    bt_case_ref ref;
    bt_case_ref_of(&ref, "2x2", "cll", "cll", case_key);
    return ref;
}

BT_TEST(repo_user, upsert_inserts_then_updates_a_single_row) {
    bt_db db;
    open_db(&db);
    bt_case_ref key = key_of("cll.1");
    bt_db_error err;
    BT_CHECK(bt_repo_user_upsert(&db, &key, "R U R'", &err) == 0);
    bt_overrides loaded;
    BT_CHECK(bt_repo_user_load_all(&db, &loaded, &err) == 0);
    BT_CHECK(bt_overrides_len(&loaded) == 1);
    BT_CHECK_EQ(bt_overrides_get(&loaded, &key), "R U R'");
    bt_overrides_free(&loaded);
    BT_CHECK(bt_repo_user_upsert(&db, &key, "F R F'", &err) == 0);
    BT_CHECK(bt_repo_user_load_all(&db, &loaded, &err) == 0);
    BT_CHECK(bt_overrides_len(&loaded) == 1);
    BT_CHECK_EQ(bt_overrides_get(&loaded, &key), "F R F'");
    bt_overrides_free(&loaded);
    bt_db_close(&db);
}

BT_TEST(repo_user, upsert_keeps_distinct_keys_per_stage_and_method) {
    bt_db db;
    open_db(&db);
    bt_case_ref key = key_of("cll.1");
    bt_case_ref other = key_of("cll.1");
    snprintf(other.method, BT_CASE_FIELD_MAX, "%s", "eg");
    bt_case_ref staged = key_of("cll.1");
    snprintf(staged.stage, BT_CASE_FIELD_MAX, "%s", "eg1");
    bt_db_error err;
    BT_CHECK(bt_repo_user_upsert(&db, &key, "R U R'", &err) == 0);
    BT_CHECK(bt_repo_user_upsert(&db, &other, "F R F'", &err) == 0);
    BT_CHECK(bt_repo_user_upsert(&db, &staged, "y R'", &err) == 0);
    bt_overrides loaded;
    BT_CHECK(bt_repo_user_load_all(&db, &loaded, &err) == 0);
    BT_CHECK(bt_overrides_len(&loaded) == 3);
    BT_CHECK_EQ(bt_overrides_get(&loaded, &key), "R U R'");
    BT_CHECK_EQ(bt_overrides_get(&loaded, &other), "F R F'");
    BT_CHECK_EQ(bt_overrides_get(&loaded, &staged), "y R'");
    bt_overrides_free(&loaded);
    bt_db_close(&db);
}

BT_TEST(repo_user, remove_reports_whether_a_row_existed) {
    bt_db db;
    open_db(&db);
    bt_case_ref key = key_of("cll.1");
    bt_db_error err;
    BT_CHECK(bt_repo_user_remove(&db, &key, &err) == 0);
    BT_CHECK(bt_repo_user_upsert(&db, &key, "R U R'", &err) == 0);
    BT_CHECK(bt_repo_user_remove(&db, &key, &err) == 1);
    bt_overrides loaded;
    BT_CHECK(bt_repo_user_load_all(&db, &loaded, &err) == 0);
    BT_CHECK(bt_overrides_len(&loaded) == 0);
    bt_overrides_free(&loaded);
    BT_CHECK(bt_repo_user_remove(&db, &key, &err) == 0);
    bt_db_close(&db);
}

BT_TEST(repo_user, legacy_alg_saves_do_not_wipe_seed_overrides) {
    bt_db db;
    open_db(&db);
    bt_case_ref key = key_of("cll.1");
    bt_db_error err;
    BT_CHECK(bt_repo_user_upsert(&db, &key, "R U R'", &err) == 0);
    bt_user_algs legacy;
    bt_user_algs_init(&legacy);
    bt_user_algs_set(&legacy, "edge", "UF", "M2");
    BT_CHECK(bt_repo_algs_save(&db, &legacy, &err) == 0);
    bt_overrides loaded;
    BT_CHECK(bt_repo_user_load_all(&db, &loaded, &err) == 0);
    BT_CHECK_EQ(bt_overrides_get(&loaded, &key), "R U R'");
    bt_overrides_free(&loaded);
    bt_user_algs_free(&legacy);
    bt_db_close(&db);
}

BT_TEST(repo_user, legacy_alg_loads_skip_seed_override_rows) {
    bt_db db;
    open_db(&db);
    bt_case_ref key = key_of("cll.1");
    bt_db_error err;
    BT_CHECK(bt_repo_user_upsert(&db, &key, "R U R'", &err) == 0);
    bt_user_algs legacy;
    BT_CHECK(bt_repo_algs_load(&db, &legacy, &err) == 0);
    BT_CHECK(bt_user_algs_is_empty(&legacy) == 1);
    bt_user_algs_free(&legacy);
    char *exec_err = NULL;
    BT_CHECK(sqlite3_exec(bt_db_conn(&db),
                          "INSERT INTO user_alg(event, method, stage, case_key, alg, "
                          "created_at, updated_at) VALUES ('3bld', '3style', 'edge', "
                          "'UF', 'M2', 0, 0)",
                          NULL, NULL, &exec_err) == SQLITE_OK);
    BT_CHECK(bt_repo_algs_load(&db, &legacy, &err) == 0);
    BT_CHECK(bt_user_algs_len(&legacy) == 1);
    BT_CHECK_EQ(bt_user_algs_alg_for(&legacy, "edge", "UF"), "M2");
    bt_user_algs_free(&legacy);
    bt_db_close(&db);
}

BT_TEST(repo_user, case_ref_of_copies_the_row_identity) {
    bt_case_ref ref;
    bt_case_ref_of(&ref, "skewb", "sarah", "sarah", "s.1");
    BT_CHECK_EQ(ref.event, "skewb");
    BT_CHECK_EQ(ref.method, "sarah");
    BT_CHECK_EQ(ref.stage, "sarah");
    BT_CHECK_EQ(ref.case_key, "s.1");
}
