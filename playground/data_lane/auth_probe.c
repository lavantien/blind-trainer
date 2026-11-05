#include "data/db.h"
#include "data/repo_cards.h"
#include "data/repo_user.h"

#include <stdio.h>
#include <string.h>

#include "sqlite3.h"

static int log_auth(void *ctx, int code, const char *arg1, const char *arg2, const char *db,
                    const char *trigger) {
    (void)ctx;
    (void)db;
    (void)trigger;
    fprintf(stdout, "auth code=%d arg1=%s arg2=%s\n", code, arg1 ? arg1 : "-", arg2 ? arg2 : "-");
    return SQLITE_OK;
}

int main(void) {
    bt_db db;
    bt_db_error err;
    if (bt_db_open_memory(&db, &err) != 0) {
        fprintf(stderr, "open failed\n");
        return 1;
    }
    sqlite3_set_authorizer(bt_db_conn(&db), log_auth, NULL);
    bt_case_ref ref;
    bt_case_ref_of(&ref, "2x2", "orto", "stage", "case-a");
    bt_card card;
    bt_card_init(&card, 0);
    bt_repo_cards_save_card(&db, &ref, &card, &err);
    bt_card_free(&card);
    bt_db_close(&db);
    return 0;
}
