#ifndef BT_DATA_REPO_SETTINGS_H
#define BT_DATA_REPO_SETTINGS_H

#include "data/db.h"

#define BT_REPO_SETTINGS_APP_KEY "settings"

int bt_repo_settings_save_tx(bt_db *db, const char *key, const char *value, bt_db_error *err);
int bt_repo_settings_save(bt_db *db, const char *key, const char *value, bt_db_error *err);
int bt_repo_settings_load_text(const bt_db *db, const char *key, char **out, bt_db_error *err);

#endif
