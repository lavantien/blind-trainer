#include "common/bt_harness.h"
#include "data/repo_solves.h"

#include <string.h>

static void open_db(bt_db *db) {
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(db, &err) == 0);
}

BT_TEST(repo_solves, solve_log_eq_distinguishes_the_first_record) {
    bt_solve_record base[2] = {{5, 40000, 25000, 1}, {100, 1, 2, 1}};
    bt_solve_log a;
    bt_solve_log_init(&a);
    for (int i = 0; i < 2; i++) {
        BT_CHECK(bt_solve_log_push(&a, base[i]) == 0);
    }
    bt_solve_log same;
    bt_solve_log_init(&same);
    for (int i = 0; i < 2; i++) {
        BT_CHECK(bt_solve_log_push(&same, base[i]) == 0);
    }
    BT_CHECK(bt_solve_log_eq(&a, &same) == 1);
    bt_solve_record shifted[2] = {{6, 40000, 25000, 1}, {100, 1, 2, 1}};
    bt_solve_log differ;
    bt_solve_log_init(&differ);
    for (int i = 0; i < 2; i++) {
        BT_CHECK(bt_solve_log_push(&differ, shifted[i]) == 0);
    }
    BT_CHECK(bt_solve_log_eq(&a, &differ) == 0);
    bt_solve_record tail[2] = {{5, 40000, 25000, 1}, {100, 1, 3, 1}};
    bt_solve_log differ_tail;
    bt_solve_log_init(&differ_tail);
    for (int i = 0; i < 2; i++) {
        BT_CHECK(bt_solve_log_push(&differ_tail, tail[i]) == 0);
    }
    BT_CHECK(bt_solve_log_eq(&a, &differ_tail) == 0);
    bt_solve_log_free(&differ_tail);
    bt_solve_log_free(&differ);
    bt_solve_log_free(&same);
    bt_solve_log_free(&a);
}

BT_TEST(repo_solves, save_and_load_round_trips_records_in_order) {
    bt_db db;
    open_db(&db);
    bt_solve_log log;
    bt_solve_log_init(&log);
    bt_solve_record records[3] = {
        {5, 40000, 25000, 1},
        {-2, 0, 9223372036854775807LL / 4, 0},
        {100, 1, 2, 1},
    };
    for (int i = 0; i < 3; i++) {
        BT_CHECK(bt_solve_log_push(&log, records[i]) == 0);
    }
    bt_db_error err;
    BT_CHECK(bt_solve_log_save(&db, &log, &err) == 0);
    bt_solve_log loaded;
    BT_CHECK(bt_solve_log_load(&db, &loaded, &err) == 0);
    BT_CHECK(bt_solve_log_eq(&log, &loaded) == 1);
    bt_solve_log_free(&loaded);
    bt_solve_log_free(&log);
    bt_db_close(&db);
}

BT_TEST(repo_solves, save_replaces_the_previous_log) {
    bt_db db;
    open_db(&db);
    bt_solve_log first;
    bt_solve_log_init(&first);
    bt_solve_log_push(&first, (bt_solve_record){1, 1, 1, 1});
    bt_db_error err;
    BT_CHECK(bt_solve_log_save(&db, &first, &err) == 0);
    bt_solve_log second;
    bt_solve_log_init(&second);
    bt_solve_log_push(&second, (bt_solve_record){2, 2, 2, 0});
    BT_CHECK(bt_solve_log_save(&db, &second, &err) == 0);
    bt_solve_log loaded;
    BT_CHECK(bt_solve_log_load(&db, &loaded, &err) == 0);
    BT_CHECK(bt_solve_log_eq(&second, &loaded) == 1);
    bt_solve_log_free(&loaded);
    bt_solve_log_free(&first);
    bt_solve_log_free(&second);
    bt_db_close(&db);
}

BT_TEST(repo_solves, saving_an_empty_log_clears_the_table) {
    bt_db db;
    open_db(&db);
    bt_solve_log log;
    bt_solve_log_init(&log);
    bt_solve_log_push(&log, (bt_solve_record){1, 1, 1, 1});
    bt_db_error err;
    BT_CHECK(bt_solve_log_save(&db, &log, &err) == 0);
    bt_solve_log empty;
    bt_solve_log_init(&empty);
    BT_CHECK(bt_solve_log_save(&db, &empty, &err) == 0);
    bt_solve_log loaded;
    BT_CHECK(bt_solve_log_load(&db, &loaded, &err) == 0);
    BT_CHECK(loaded.count == 0);
    bt_solve_log_free(&loaded);
    bt_solve_log_free(&log);
    bt_solve_log_free(&empty);
    bt_db_close(&db);
}
