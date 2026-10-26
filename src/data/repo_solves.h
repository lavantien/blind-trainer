#ifndef BT_DATA_REPO_SOLVES_H
#define BT_DATA_REPO_SOLVES_H

#include "common/json.h"
#include "data/db.h"

#include <stddef.h>

typedef struct {
    long long started_ms;
    long long memo_ms;
    long long exec_ms;
    int success;
} bt_solve_record;

typedef struct {
    bt_solve_record *items;
    size_t count;
    size_t cap;
} bt_solve_log;

void bt_solve_log_init(bt_solve_log *log);
void bt_solve_log_free(bt_solve_log *log);
int bt_solve_log_push(bt_solve_log *log, bt_solve_record record);
int bt_solve_log_eq(const bt_solve_log *a, const bt_solve_log *b);
int bt_solve_log_save_tx(bt_db *db, const bt_solve_log *log, bt_db_error *err);
int bt_solve_log_save(bt_db *db, const bt_solve_log *log, bt_db_error *err);
int bt_solve_log_load(const bt_db *db, bt_solve_log *out, bt_db_error *err);
int bt_solve_log_from_json(const bt_json_value *root, bt_solve_log *out);

#endif
