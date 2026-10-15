use crate::db::{Db, DbError};
use rusqlite::Transaction;
use rusqlite::params;

pub const APP_KEY: &str = "settings";

pub fn save_tx(tx: &Transaction<'_>, key: &str, value: &str) -> Result<(), DbError> {
    tx.execute(
        "INSERT INTO settings(key, value) VALUES (?, ?)
         ON CONFLICT(key) DO UPDATE SET value = excluded.value",
        params![key, value],
    )
    .map_err(|source| DbError::Exec {
        step: "upsert settings",
        source,
    })?;
    Ok(())
}

pub fn save(db: &mut Db, key: &str, value: &str) -> Result<(), DbError> {
    let tx = db.transaction()?;
    save_tx(&tx, key, value)?;
    tx.commit().map_err(|source| DbError::Exec {
        step: "commit settings",
        source,
    })
}

pub fn load_text(db: &Db, key: &str) -> Result<Option<String>, DbError> {
    db.conn()
        .query_row(
            "SELECT value FROM settings WHERE key = ?",
            params![key],
            |row| row.get(0),
        )
        .map(Some)
        .or_else(|err| match err {
            rusqlite::Error::QueryReturnedNoRows => Ok(None),
            source => Err(DbError::Query {
                step: "select settings",
                source,
            }),
        })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn db() -> Db {
        Db::open_in_memory().expect("db")
    }

    #[test]
    fn load_reports_missing_keys_as_none() {
        let db = db();
        assert_eq!(load_text(&db, APP_KEY).expect("load"), None);
    }

    #[test]
    fn save_then_load_returns_the_exact_text() {
        let mut db = db();
        let text = r#"{"memo_cadence_ms":640,"memo_mode":"per-orbit","drill_reps":6}"#;
        save(&mut db, APP_KEY, text).expect("save");
        assert_eq!(
            load_text(&db, APP_KEY).expect("load"),
            Some(text.to_string())
        );
    }

    #[test]
    fn save_overwrites_the_previous_value() {
        let mut db = db();
        save(&mut db, APP_KEY, "first").expect("first");
        save(&mut db, APP_KEY, "second").expect("second");
        assert_eq!(
            load_text(&db, APP_KEY).expect("load"),
            Some("second".to_string())
        );
    }

    #[test]
    fn keys_stay_independent() {
        let mut db = db();
        save(&mut db, "settings", "app").expect("app");
        save(&mut db, "other", "misc").expect("misc");
        assert_eq!(
            load_text(&db, "settings").expect("load"),
            Some("app".to_string())
        );
        assert_eq!(
            load_text(&db, "other").expect("load"),
            Some("misc".to_string())
        );
    }
}
