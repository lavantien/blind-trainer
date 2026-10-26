#include "common/bt_harness.h"
#include "data/repo_algs.h"

#include <string.h>

#include "sqlite3.h"

static void open_db(bt_db *db) {
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(db, &err) == 0);
}

static void algs_of(bt_user_algs *algs, const char *const items[][2], size_t count) {
    bt_user_algs_init(algs);
    for (size_t i = 0; i < count; i++) {
        char stage[512];
        char position[512];
        const char *slash = strchr(items[i][0], '/');
        if (slash == NULL) {
            snprintf(stage, sizeof stage, "%s", items[i][0]);
            snprintf(position, sizeof position, "%s", "");
        } else {
            size_t head = (size_t)(slash - items[i][0]);
            memcpy(stage, items[i][0], head);
            stage[head] = '\0';
            snprintf(position, sizeof position, "%s", slash + 1);
        }
        bt_user_algs_set(algs, stage, position, items[i][1]);
    }
}

BT_TEST(repo_algs, split_and_join_rebuild_every_domain_key) {
    const char *keys[] = {
        "edge/UF",
        "corner/URB",
        "flips/none",
        "ltct/UF-UB",
        "edge/UF-UB-UR",
        "a/b/c",
        "/leading",
        "plain",
        "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e/\xe3\x82\xb1\xe3\x83\xbc\xe3\x82\xb9",
        "quote'key",
    };
    for (int i = 0; i < 10; i++) {
        char stage[512];
        char case_key[512];
        char joined[1024];
        bt_repo_algs_split_key(keys[i], stage, sizeof stage, case_key, sizeof case_key);
        bt_repo_algs_join_key(stage, case_key, joined, sizeof joined);
        BT_CHECK_EQ(joined, keys[i]);
    }
}

BT_TEST(repo_algs, save_and_load_round_trips_every_entry) {
    bt_db db;
    open_db(&db);
    const char *const items[][2] = {
        {"corner/URB", "y' R U' R' U R U' R' U2 R U' R'"},
        {"edge/UF", "R2 U' S R2 S' R2 U R2"},
        {"flips/none", ""},
        {"\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e/\xe3\x82\xb1\xe3\x83\xbc\xe3\x82\xb9", "M2"},
    };
    bt_user_algs original;
    algs_of(&original, items, 4);
    bt_db_error err;
    BT_CHECK(bt_repo_algs_save(&db, &original, &err) == 0);
    bt_user_algs loaded;
    BT_CHECK(bt_repo_algs_load(&db, &loaded, &err) == 0);
    BT_CHECK(bt_user_algs_eq(&original, &loaded) == 1);
    bt_user_algs_free(&loaded);
    bt_user_algs_free(&original);
    bt_db_close(&db);
}

BT_TEST(repo_algs, save_replaces_the_previous_set) {
    bt_db db;
    open_db(&db);
    const char *const first_items[][2] = {{"edge/UF", "M2"}, {"edge/UB", "M2"}};
    bt_user_algs first;
    algs_of(&first, first_items, 2);
    bt_db_error err;
    BT_CHECK(bt_repo_algs_save(&db, &first, &err) == 0);
    const char *const second_items[][2] = {{"corner/URB", "R U' R'"}};
    bt_user_algs second;
    algs_of(&second, second_items, 1);
    BT_CHECK(bt_repo_algs_save(&db, &second, &err) == 0);
    bt_user_algs loaded;
    BT_CHECK(bt_repo_algs_load(&db, &loaded, &err) == 0);
    BT_CHECK(bt_user_algs_eq(&second, &loaded) == 1);
    bt_user_algs_free(&loaded);
    bt_user_algs_free(&first);
    bt_user_algs_free(&second);
    bt_db_close(&db);
}

BT_TEST(repo_algs, saving_an_empty_set_clears_the_table) {
    bt_db db;
    open_db(&db);
    const char *const items[][2] = {{"edge/UF", "M2"}};
    bt_user_algs seed;
    algs_of(&seed, items, 1);
    bt_db_error err;
    BT_CHECK(bt_repo_algs_save(&db, &seed, &err) == 0);
    bt_user_algs empty;
    bt_user_algs_init(&empty);
    BT_CHECK(bt_repo_algs_save(&db, &empty, &err) == 0);
    bt_user_algs loaded;
    BT_CHECK(bt_repo_algs_load(&db, &loaded, &err) == 0);
    BT_CHECK(bt_user_algs_eq(&empty, &loaded) == 1);
    bt_user_algs_free(&loaded);
    bt_user_algs_free(&seed);
    bt_user_algs_free(&empty);
    bt_db_close(&db);
}

BT_TEST(repo_algs, rows_carry_the_3bld_event_and_3style_method) {
    bt_db db;
    open_db(&db);
    const char *const items[][2] = {{"edge/UF", "M2"}};
    bt_user_algs algs;
    algs_of(&algs, items, 1);
    bt_db_error err;
    BT_CHECK(bt_repo_algs_save(&db, &algs, &err) == 0);
    sqlite3_stmt *stmt = NULL;
    BT_CHECK(sqlite3_prepare_v2(bt_db_conn(&db),
                                "SELECT event, method FROM user_alg WHERE case_key = 'UF'",
                                -1, &stmt, NULL) == SQLITE_OK);
    BT_CHECK(sqlite3_step(stmt) == SQLITE_ROW);
    BT_CHECK_EQ((const char *)sqlite3_column_text(stmt, 0), BT_DB_EVENT_3BLD);
    BT_CHECK_EQ((const char *)sqlite3_column_text(stmt, 1), BT_DB_METHOD_3STYLE);
    sqlite3_finalize(stmt);
    bt_user_algs_free(&algs);
    bt_db_close(&db);
}
