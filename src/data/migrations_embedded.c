#include "data/migrations_embedded.h"

static const char MIGRATION_001_INIT[] =
    "CREATE TABLE user_alg (\n    event TEXT NOT NULL DEFAULT '3bld',\n    method TEXT NOT NULL DEFAULT '3style',\n    stage TEXT NOT NULL,\n    case_key TEXT NOT NULL,\n    alg TEXT NOT NULL,\n    created_at INTEGER NOT NULL,\n    updated_at INTEGER NOT NULL,\n    UNIQUE (event, method, stage, case_key)\n);\n\nCREATE TABLE card (\n    id INTEGER PRIMARY KEY,\n    event TEXT NOT NULL DEFAULT '3bld',\n    method TEXT NOT NULL DEFAULT '3style',\n    stage TEXT NOT NULL,\n    case_key TEXT NOT NULL,\n    created_at INTEGER NOT NULL,\n    updated_at INTEGER NOT NULL,\n    UNIQUE (event, method, stage, case_key)\n);\n\nCREATE TABLE srs_state (\n    card_id INTEGER NOT NULL PRIMARY KEY REFERENCES card (id) ON DELETE CASCADE,\n    due_day INTEGER NOT NULL,\n    interval_days INTEGER NOT NULL,\n    ease REAL NOT NULL,\n    reps INTEGER NOT NULL,\n    lapses INTEGER NOT NULL,\n    last_review_day INTEGER,\n    stability REAL,\n    difficulty REAL\n);\n\nCREATE TABLE review_log (\n    id INTEGER PRIMARY KEY,\n    card_id INTEGER NOT NULL REFERENCES card (id) ON DELETE CASCADE,\n    rating INTEGER NOT NULL,\n    reviewed_day INTEGER NOT NULL,\n    interval_before INTEGER NOT NULL,\n    interval_after INTEGER NOT NULL,\n    elapsed_days INTEGER NOT NULL\n);\n\nCREATE TABLE solves (\n    id INTEGER PRIMARY KEY,\n    started_ms INTEGER NOT NULL,\n    memo_ms INTEGER NOT NULL,\n    exec_ms INTEGER NOT NULL,\n    success INTEGER NOT NULL\n);\n\nCREATE TABLE settings (\n    key TEXT NOT NULL PRIMARY KEY,\n    value TEXT NOT NULL\n);\n";
static const char MIGRATION_002_DROP_SRS_EASE[] = "ALTER TABLE srs_state DROP COLUMN ease;\n";
static const char MIGRATION_003_SEED_ALG[] =
    "CREATE TABLE seed_alg (\n    event TEXT NOT NULL,\n    method TEXT NOT NULL,\n    stage TEXT NOT NULL,\n    case_key TEXT NOT NULL,\n    alg TEXT NOT NULL,\n    front_json TEXT NOT NULL,\n    source_url TEXT NOT NULL,\n    source_at TEXT NOT NULL,\n    content_hash TEXT NOT NULL,\n    UNIQUE (event, method, stage, case_key)\n);\n";

static const char *const MIGRATION_NAMES[] = {
    "001_init",
    "002_drop_srs_ease",
    "003_seed_alg",
};

static const char *const MIGRATION_SQL[] = {
    MIGRATION_001_INIT,
    MIGRATION_002_DROP_SRS_EASE,
    MIGRATION_003_SEED_ALG,
};

#define EMBEDDED_COUNT ((size_t)(sizeof MIGRATION_SQL / sizeof MIGRATION_SQL[0]))

size_t bt_db_embedded_migration_count(void) {
    return EMBEDDED_COUNT;
}

const char *bt_db_embedded_migration_name(size_t index) {
    if (index >= EMBEDDED_COUNT) {
        return NULL;
    }
    return MIGRATION_NAMES[index];
}

const char *bt_db_embedded_migration_sql(size_t index) {
    if (index >= EMBEDDED_COUNT) {
        return NULL;
    }
    return MIGRATION_SQL[index];
}
