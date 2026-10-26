#ifndef BT_DATA_IMPORT_H
#define BT_DATA_IMPORT_H

#include "data/db.h"

#include <stddef.h>

void bt_import_migrated_path(const char *path, char *out, size_t out_len);
int bt_import_run(bt_db *db, const char *root, char *error, size_t error_len);

#endif
