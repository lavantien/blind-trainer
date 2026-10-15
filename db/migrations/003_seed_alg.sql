CREATE TABLE seed_alg (
    event TEXT NOT NULL,
    method TEXT NOT NULL,
    stage TEXT NOT NULL,
    case_key TEXT NOT NULL,
    alg TEXT NOT NULL,
    front_json TEXT NOT NULL,
    source_url TEXT NOT NULL,
    source_at TEXT NOT NULL,
    content_hash TEXT NOT NULL,
    UNIQUE (event, method, stage, case_key)
);
