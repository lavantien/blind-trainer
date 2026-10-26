#include "common/bt_harness.h"
#include "data/repo_settings.h"

#include <stdlib.h>
#include <string.h>

static void open_db(bt_db *db) {
    bt_db_error err;
    BT_CHECK(bt_db_open_memory(db, &err) == 0);
}

BT_TEST(repo_settings, load_reports_missing_keys_as_none) {
    bt_db db;
    open_db(&db);
    char *text = NULL;
    bt_db_error err;
    BT_CHECK(bt_repo_settings_load_text(&db, BT_REPO_SETTINGS_APP_KEY, &text, &err) == 0);
    BT_CHECK(text == NULL);
    bt_db_close(&db);
}

BT_TEST(repo_settings, save_then_load_returns_the_exact_text) {
    bt_db db;
    open_db(&db);
    const char *text = "{\"memo_cadence_ms\":640,\"memo_mode\":\"per-orbit\",\"drill_reps\":6}";
    bt_db_error err;
    BT_CHECK(bt_repo_settings_save(&db, BT_REPO_SETTINGS_APP_KEY, text, &err) == 0);
    char *loaded = NULL;
    BT_CHECK(bt_repo_settings_load_text(&db, BT_REPO_SETTINGS_APP_KEY, &loaded, &err) == 0);
    BT_CHECK(loaded != NULL);
    BT_CHECK_EQ(loaded, text);
    free(loaded);
    bt_db_close(&db);
}

BT_TEST(repo_settings, save_overwrites_the_previous_value) {
    bt_db db;
    open_db(&db);
    bt_db_error err;
    BT_CHECK(bt_repo_settings_save(&db, BT_REPO_SETTINGS_APP_KEY, "first", &err) == 0);
    BT_CHECK(bt_repo_settings_save(&db, BT_REPO_SETTINGS_APP_KEY, "second", &err) == 0);
    char *loaded = NULL;
    BT_CHECK(bt_repo_settings_load_text(&db, BT_REPO_SETTINGS_APP_KEY, &loaded, &err) == 0);
    BT_CHECK(loaded != NULL);
    BT_CHECK_EQ(loaded, "second");
    free(loaded);
    bt_db_close(&db);
}

BT_TEST(repo_settings, keys_stay_independent) {
    bt_db db;
    open_db(&db);
    bt_db_error err;
    BT_CHECK(bt_repo_settings_save(&db, "settings", "app", &err) == 0);
    BT_CHECK(bt_repo_settings_save(&db, "other", "misc", &err) == 0);
    char *app = NULL;
    char *misc = NULL;
    BT_CHECK(bt_repo_settings_load_text(&db, "settings", &app, &err) == 0);
    BT_CHECK(bt_repo_settings_load_text(&db, "other", &misc, &err) == 0);
    BT_CHECK(app != NULL && strcmp(app, "app") == 0);
    BT_CHECK(misc != NULL && strcmp(misc, "misc") == 0);
    free(app);
    free(misc);
    bt_db_close(&db);
}
