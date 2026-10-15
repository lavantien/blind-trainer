CREATE TABLE user_alg (
    event TEXT NOT NULL DEFAULT '3bld',
    method TEXT NOT NULL DEFAULT '3style',
    stage TEXT NOT NULL,
    case_key TEXT NOT NULL,
    alg TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL,
    UNIQUE (event, method, stage, case_key)
);

CREATE TABLE card (
    id INTEGER PRIMARY KEY,
    event TEXT NOT NULL DEFAULT '3bld',
    method TEXT NOT NULL DEFAULT '3style',
    stage TEXT NOT NULL,
    case_key TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL,
    UNIQUE (event, method, stage, case_key)
);

CREATE TABLE srs_state (
    card_id INTEGER NOT NULL PRIMARY KEY REFERENCES card (id) ON DELETE CASCADE,
    due_day INTEGER NOT NULL,
    interval_days INTEGER NOT NULL,
    ease REAL NOT NULL,
    reps INTEGER NOT NULL,
    lapses INTEGER NOT NULL,
    last_review_day INTEGER,
    stability REAL,
    difficulty REAL
);

CREATE TABLE review_log (
    id INTEGER PRIMARY KEY,
    card_id INTEGER NOT NULL REFERENCES card (id) ON DELETE CASCADE,
    rating INTEGER NOT NULL,
    reviewed_day INTEGER NOT NULL,
    interval_before INTEGER NOT NULL,
    interval_after INTEGER NOT NULL,
    elapsed_days INTEGER NOT NULL
);

CREATE TABLE solves (
    id INTEGER PRIMARY KEY,
    started_ms INTEGER NOT NULL,
    memo_ms INTEGER NOT NULL,
    exec_ms INTEGER NOT NULL,
    success INTEGER NOT NULL
);

CREATE TABLE settings (
    key TEXT NOT NULL PRIMARY KEY,
    value TEXT NOT NULL
);
