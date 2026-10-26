#ifndef BT_DATA_MIGRATIONS_EMBEDDED_H
#define BT_DATA_MIGRATIONS_EMBEDDED_H

#include <stddef.h>

size_t bt_db_embedded_migration_count(void);
const char *bt_db_embedded_migration_name(size_t index);
const char *bt_db_embedded_migration_sql(size_t index);

#endif
