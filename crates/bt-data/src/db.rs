use rusqlite::Connection;
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

pub const EVENT_3BLD: &str = "3bld";
pub const METHOD_3STYLE: &str = "3style";

pub struct Migration {
    pub name: &'static str,
    pub sql: &'static str,
}

pub const MIGRATIONS: &[Migration] = &[
    Migration {
        name: "001_init",
        sql: include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/../../db/migrations/001_init.sql"
        )),
    },
    Migration {
        name: "002_drop_srs_ease",
        sql: include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/../../db/migrations/002_drop_srs_ease.sql"
        )),
    },
    Migration {
        name: "003_seed_alg",
        sql: include_str!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/../../db/migrations/003_seed_alg.sql"
        )),
    },
];

pub const LATEST_USER_VERSION: i64 = MIGRATIONS.len() as i64;

#[derive(Debug)]
pub enum DbError {
    Open {
        path: PathBuf,
        source: rusqlite::Error,
    },
    Mkdir {
        path: PathBuf,
        source: std::io::Error,
    },
    Pragma {
        step: &'static str,
        source: rusqlite::Error,
    },
    Migrate {
        name: &'static str,
        source: rusqlite::Error,
    },
    Future {
        version: i64,
        latest: i64,
    },
    Backup {
        from: PathBuf,
        to: PathBuf,
        source: std::io::Error,
    },
    Query {
        step: &'static str,
        source: rusqlite::Error,
    },
    Exec {
        step: &'static str,
        source: rusqlite::Error,
    },
}

impl std::fmt::Display for DbError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            DbError::Open { path, source } => {
                write!(f, "open db {}: {source}", path.display())
            }
            DbError::Mkdir { path, source } => {
                write!(f, "create db dir {}: {source}", path.display())
            }
            DbError::Pragma { step, source } => write!(f, "pragma {step}: {source}"),
            DbError::Migrate { name, source } => write!(f, "migration {name}: {source}"),
            DbError::Future { version, latest } => {
                write!(
                    f,
                    "db user_version {version} is newer than supported {latest}"
                )
            }
            DbError::Backup { from, to, source } => {
                write!(f, "backup {} to {}: {source}", from.display(), to.display())
            }
            DbError::Query { step, source } => write!(f, "query {step}: {source}"),
            DbError::Exec { step, source } => write!(f, "exec {step}: {source}"),
        }
    }
}

impl std::error::Error for DbError {}

#[derive(Debug)]
pub struct Db {
    conn: Connection,
    path: Option<PathBuf>,
}

pub fn backup_path(path: &Path) -> PathBuf {
    let mut name = path.as_os_str().to_os_string();
    name.push(".bak");
    PathBuf::from(name)
}

pub fn now_ms() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|span| span.as_millis() as i64)
        .unwrap_or(0)
}

impl Db {
    pub fn open_at(path: &Path) -> Result<Db, DbError> {
        if let Some(parent) = path
            .parent()
            .filter(|parent| !parent.as_os_str().is_empty() && !parent.exists())
        {
            std::fs::create_dir_all(parent).map_err(|source| DbError::Mkdir {
                path: parent.to_path_buf(),
                source,
            })?;
        }
        let conn = Connection::open(path).map_err(|source| DbError::Open {
            path: path.to_path_buf(),
            source,
        })?;
        Self::finish(conn, Some(path.to_path_buf()))
    }

    pub fn open_in_memory() -> Result<Db, DbError> {
        let conn = Connection::open_in_memory().map_err(|source| DbError::Open {
            path: PathBuf::from(":memory:"),
            source,
        })?;
        Self::finish(conn, None)
    }

    pub fn open_root(root: &Path) -> Result<Db, DbError> {
        Self::open_at(&crate::paths::db_path_at(root))
    }

    fn finish(conn: Connection, path: Option<PathBuf>) -> Result<Db, DbError> {
        conn.pragma_update(None, "foreign_keys", true)
            .map_err(|source| DbError::Pragma {
                step: "foreign_keys",
                source,
            })?;
        let mut db = Db { conn, path };
        db.migrate()?;
        Ok(db)
    }

    pub fn conn(&self) -> &Connection {
        &self.conn
    }

    pub fn path(&self) -> Option<&Path> {
        self.path.as_deref()
    }

    pub fn transaction(&mut self) -> Result<rusqlite::Transaction<'_>, DbError> {
        self.conn.transaction().map_err(|source| DbError::Exec {
            step: "begin",
            source,
        })
    }

    pub fn user_version(&self) -> Result<i64, DbError> {
        self.conn
            .query_row("PRAGMA user_version", [], |row| row.get(0))
            .map_err(|source| DbError::Query {
                step: "user_version",
                source,
            })
    }

    pub fn is_fresh(&self) -> Result<bool, DbError> {
        let count = |table: &'static str| -> Result<i64, DbError> {
            self.conn
                .query_row(&format!("SELECT COUNT(*) FROM {table}"), [], |row| {
                    row.get(0)
                })
                .map_err(|source| DbError::Query {
                    step: table,
                    source,
                })
        };
        Ok(count("user_alg")? + count("card")? + count("solves")? + count("settings")? == 0)
    }

    fn migrate(&mut self) -> Result<(), DbError> {
        let current = self.user_version()?;
        if current > LATEST_USER_VERSION {
            return Err(DbError::Future {
                version: current,
                latest: LATEST_USER_VERSION,
            });
        }
        let first_index = current.max(0) as usize;
        for (index, migration) in MIGRATIONS.iter().enumerate().skip(first_index) {
            if index == first_index && self.has_any_table()? {
                self.backup_file()?;
            }
            let target = index as i64 + 1;
            let tx = self.conn.transaction().map_err(|source| DbError::Migrate {
                name: migration.name,
                source,
            })?;
            tx.execute_batch(migration.sql)
                .map_err(|source| DbError::Migrate {
                    name: migration.name,
                    source,
                })?;
            tx.pragma_update(None, "user_version", target)
                .map_err(|source| DbError::Migrate {
                    name: migration.name,
                    source,
                })?;
            tx.commit().map_err(|source| DbError::Migrate {
                name: migration.name,
                source,
            })?;
        }
        Ok(())
    }

    fn has_any_table(&self) -> Result<bool, DbError> {
        let count: i64 = self
            .conn
            .query_row(
                "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%'",
                [],
                |row| row.get(0),
            )
            .map_err(|source| DbError::Query {
                step: "sqlite_master",
                source,
            })?;
        Ok(count > 0)
    }

    fn backup_file(&self) -> Result<(), DbError> {
        let Some(path) = &self.path else {
            return Ok(());
        };
        let to = backup_path(path);
        std::fs::copy(path, &to).map_err(|source| DbError::Backup {
            from: path.clone(),
            to,
            source,
        })?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU64, Ordering};

    static DIR_COUNTER: AtomicU64 = AtomicU64::new(0);

    fn temp_dir(tag: &str) -> PathBuf {
        let dir = std::env::temp_dir().join("bt-data-db-tests").join(format!(
            "{tag}-{}-{}",
            std::process::id(),
            DIR_COUNTER.fetch_add(1, Ordering::Relaxed)
        ));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(&dir).expect("temp dir");
        dir
    }

    fn table_columns(db: &Db, table: &str) -> Vec<String> {
        let mut stmt = db
            .conn()
            .prepare(&format!("PRAGMA table_info({table})"))
            .expect("prepare table_info");
        let rows = stmt
            .query_map([], |row| row.get::<_, String>(1))
            .expect("query table_info");
        rows.map(|row| row.expect("column name")).collect()
    }

    fn integrity_check(db: &Db) -> String {
        db.conn()
            .query_row("PRAGMA integrity_check", [], |row| row.get(0))
            .expect("integrity_check")
    }

    fn make_version_zero_db_with_data(path: &Path, table_sql: &str) -> Vec<u8> {
        let conn = Connection::open(path).expect("raw open");
        conn.execute_batch(table_sql).expect("seed v0 data");
        drop(conn);
        std::fs::read(path).expect("read pre-migration bytes")
    }

    #[test]
    fn fresh_open_migrates_to_latest_without_a_backup() {
        let dir = temp_dir("fresh");
        let db_path = dir.join(paths_db_file());
        let db = Db::open_at(&db_path).expect("open");
        assert_eq!(db.user_version().expect("version"), LATEST_USER_VERSION);
        assert_eq!(integrity_check(&db), "ok");
        assert!(db_path.is_file());
        assert!(!backup_path(&db_path).exists());
    }

    fn paths_db_file() -> String {
        crate::paths::db_file_name()
    }

    fn seed_user_alg(db: &Db) {
        db.conn()
            .execute(
                "INSERT INTO user_alg(stage, case_key, alg, created_at, updated_at) VALUES ('s', 'c', 'a', 0, 0)",
                [],
            )
            .expect("seed user_alg");
    }

    fn seed_card(db: &Db) {
        db.conn()
            .execute(
                "INSERT INTO card(stage, case_key, created_at, updated_at) VALUES ('s', 'c', 0, 0)",
                [],
            )
            .expect("seed card");
    }

    fn seed_solves(db: &Db) {
        db.conn()
            .execute(
                "INSERT INTO solves(started_ms, memo_ms, exec_ms, success) VALUES (0, 0, 0, 1)",
                [],
            )
            .expect("seed solves");
    }

    fn seed_settings(db: &Db) {
        db.conn()
            .execute("INSERT INTO settings(key, value) VALUES ('k', 'v')", [])
            .expect("seed settings");
    }

    #[test]
    fn schema_holds_every_mandated_table_and_column() {
        let db = Db::open_in_memory().expect("open");
        assert_eq!(
            table_columns(&db, "seed_alg"),
            [
                "event",
                "method",
                "stage",
                "case_key",
                "alg",
                "front_json",
                "source_url",
                "source_at",
                "content_hash"
            ]
        );
        assert_eq!(
            table_columns(&db, "user_alg"),
            [
                "event",
                "method",
                "stage",
                "case_key",
                "alg",
                "created_at",
                "updated_at"
            ]
        );
        assert_eq!(
            table_columns(&db, "card"),
            [
                "id",
                "event",
                "method",
                "stage",
                "case_key",
                "created_at",
                "updated_at"
            ]
        );
        assert_eq!(
            table_columns(&db, "srs_state"),
            [
                "card_id",
                "due_day",
                "interval_days",
                "reps",
                "lapses",
                "last_review_day",
                "stability",
                "difficulty"
            ]
        );
        assert_eq!(
            table_columns(&db, "review_log"),
            [
                "id",
                "card_id",
                "rating",
                "reviewed_day",
                "interval_before",
                "interval_after",
                "elapsed_days"
            ]
        );
        assert_eq!(
            table_columns(&db, "solves"),
            ["id", "started_ms", "memo_ms", "exec_ms", "success"]
        );
        assert_eq!(table_columns(&db, "settings"), ["key", "value"]);
    }

    #[test]
    fn migrations_embed_the_committed_sql_files() {
        assert_eq!(MIGRATIONS.len(), 3);
        for migration in MIGRATIONS {
            let committed = std::fs::read_to_string(
                crate::paths::workspace_root()
                    .join("db")
                    .join("migrations")
                    .join(format!("{}.sql", migration.name)),
            )
            .expect("committed migration");
            assert_eq!(migration.sql, committed);
        }
    }

    #[test]
    fn ease_column_drops_under_002_and_keeps_the_remaining_rows() {
        let dir = temp_dir("ease-drop");
        let db_path = dir.join(paths_db_file());
        let conn = Connection::open(&db_path).expect("raw open");
        conn.execute_batch(MIGRATIONS[0].sql).expect("v1 schema");
        conn.pragma_update(None, "user_version", 1)
            .expect("v1 mark");
        conn.execute(
            "INSERT INTO card(event, method, stage, case_key, created_at, updated_at)
             VALUES ('3bld', '3style', 'edge', 'UF', 1, 1)",
            [],
        )
        .expect("card row");
        let card_id = conn.last_insert_rowid();
        conn.execute(
            "INSERT INTO srs_state(card_id, due_day, interval_days, ease, reps, lapses, last_review_day)
             VALUES (?, 10, 6, 2.5, 2, 0, 4)",
            [card_id],
        )
        .expect("v1 state row");
        conn.execute(
            "INSERT INTO review_log(card_id, rating, reviewed_day, interval_before, interval_after, elapsed_days)
             VALUES (?, 4, 4, 0, 6, 3)",
            [card_id],
        )
        .expect("v1 review row");
        drop(conn);
        let db = Db::open_at(&db_path).expect("open migrates");
        assert_eq!(db.user_version().expect("version"), LATEST_USER_VERSION);
        assert!(!table_columns(&db, "srs_state").contains(&"ease".to_string()));
        let state: (i64, i64, i64) = db
            .conn()
            .query_row(
                "SELECT due_day, interval_days, reps FROM srs_state",
                [],
                |row| Ok((row.get(0)?, row.get(1)?, row.get(2)?)),
            )
            .expect("state row");
        assert_eq!(state, (10, 6, 2));
        let reviews: i64 = db
            .conn()
            .query_row("SELECT COUNT(*) FROM review_log", [], |row| row.get(0))
            .expect("review rows");
        assert_eq!(reviews, 1);
        assert!(backup_path(&db_path).is_file());
    }

    #[test]
    fn backup_before_migrate_keeps_pre_migration_bytes() {
        let dir = temp_dir("backup");
        let db_path = dir.join(paths_db_file());
        let before = make_version_zero_db_with_data(
            &db_path,
            "CREATE TABLE legacy_data(id INTEGER PRIMARY KEY, note TEXT);
             INSERT INTO legacy_data(note) VALUES ('keep me');",
        );
        let db = Db::open_at(&db_path).expect("open migrates");
        assert_eq!(db.user_version().expect("version"), LATEST_USER_VERSION);
        let bak = backup_path(&db_path);
        assert!(bak.is_file());
        assert_eq!(std::fs::read(&bak).expect("bak bytes"), before);
        let kept: i64 = db
            .conn()
            .query_row("SELECT COUNT(*) FROM legacy_data", [], |row| row.get(0))
            .expect("legacy rows");
        assert_eq!(kept, 1);
    }

    #[test]
    fn failed_migration_rolls_back_version_and_schema() {
        let dir = temp_dir("rollback");
        let db_path = dir.join(paths_db_file());
        make_version_zero_db_with_data(
            &db_path,
            "CREATE TABLE user_alg(event TEXT, method TEXT, stage TEXT, case_key TEXT, alg TEXT);",
        );
        let err = Db::open_at(&db_path).expect_err("conflicting schema must fail");
        assert!(err.to_string().contains("migration 001_init"), "err: {err}");
        let conn = Connection::open(&db_path).expect("reopen raw");
        let version: i64 = conn
            .query_row("PRAGMA user_version", [], |row| row.get(0))
            .expect("version");
        assert_eq!(version, 0);
    }

    #[test]
    fn open_fails_closed_when_the_db_path_is_unopenable() {
        let dir = temp_dir("unopenable");
        let blocker = dir.join("blocker");
        std::fs::write(&blocker, b"occupied").expect("write blocker");
        let err = Db::open_at(&blocker.join(paths_db_file())).expect_err("must fail");
        let shown = err.to_string();
        assert!(shown.contains("open db"), "err: {shown}");
        assert!(shown.contains("blind-trainer.db"), "err: {shown}");
    }

    #[test]
    fn open_fails_closed_on_a_corrupt_db_file() {
        let dir = temp_dir("corrupt");
        let db_path = dir.join(paths_db_file());
        std::fs::write(&db_path, b"definitely not a sqlite database").expect("corrupt write");
        let err = Db::open_at(&db_path).expect_err("must fail");
        assert!(err.to_string().contains("user_version"), "err: {err}");
    }

    #[test]
    fn fresh_db_reports_empty_and_fills_after_a_row() {
        let db = Db::open_in_memory().expect("open");
        assert!(db.is_fresh().expect("fresh"));
        seed_settings(&db);
        assert!(!db.is_fresh().expect("filled"));
    }

    #[test]
    fn is_fresh_is_false_for_each_single_nonempty_table() {
        for seed in [
            seed_user_alg as fn(&Db),
            seed_card,
            seed_solves,
            seed_settings,
        ] {
            let db = Db::open_in_memory().expect("open");
            seed(&db);
            assert!(!db.is_fresh().expect("filled"));
        }
    }

    #[test]
    fn is_fresh_is_false_when_two_equal_counts_could_cancel() {
        for partner in [seed_card as fn(&Db), seed_solves, seed_settings] {
            let db = Db::open_in_memory().expect("open");
            seed_user_alg(&db);
            partner(&db);
            assert!(!db.is_fresh().expect("filled"));
        }
    }

    #[test]
    fn reopening_a_fully_migrated_db_succeeds() {
        let dir = temp_dir("reopen");
        let db_path = dir.join(paths_db_file());
        {
            let db = Db::open_at(&db_path).expect("first open");
            assert_eq!(db.user_version().expect("version"), LATEST_USER_VERSION);
        }
        let reopened = Db::open_at(&db_path).expect("reopen migrated db");
        assert_eq!(
            reopened.user_version().expect("version"),
            LATEST_USER_VERSION
        );
    }

    #[test]
    fn open_at_creates_missing_parent_directories() {
        let dir = temp_dir("nested");
        let db_path = dir.join("deep/nested/blind-trainer.db");
        let db = Db::open_at(&db_path).expect("open nested path");
        drop(db);
        assert!(db_path.is_file());
    }

    #[test]
    fn path_reports_the_file_for_disk_and_none_for_memory() {
        let dir = temp_dir("path");
        let db_path = dir.join(paths_db_file());
        let db = Db::open_at(&db_path).expect("open");
        assert_eq!(db.path(), Some(db_path.as_path()));
        let memory = Db::open_in_memory().expect("memory");
        assert_eq!(memory.path(), None);
    }

    #[test]
    fn now_ms_is_epoch_millis_and_monotonic() {
        let first = now_ms();
        let second = now_ms();
        assert!(
            first > 1_700_000_000_000,
            "now_ms must report unix epoch millis"
        );
        assert!(second >= first);
    }
}
