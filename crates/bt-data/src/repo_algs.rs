use crate::db::{Db, DbError, EVENT_3BLD, METHOD_3STYLE, now_ms};
use crate::paths::UserAlgs;
use rusqlite::Transaction;
use rusqlite::params;

pub fn split_key(key: &str) -> (&str, &str) {
    match key.split_once('/') {
        Some((stage, case_key)) => (stage, case_key),
        None => (key, ""),
    }
}

pub fn join_key(stage: &str, case_key: &str) -> String {
    if case_key.is_empty() {
        stage.to_string()
    } else {
        format!("{stage}/{case_key}")
    }
}

pub fn save_tx(tx: &Transaction<'_>, algs: &UserAlgs) -> Result<(), DbError> {
    let now = now_ms();
    tx.execute("DELETE FROM user_alg", [])
        .map_err(|source| DbError::Exec {
            step: "clear user_alg",
            source,
        })?;
    for (key, entry) in &algs.entries {
        let (stage, case_key) = split_key(key);
        tx.execute(
            "INSERT INTO user_alg(event, method, stage, case_key, alg, created_at, updated_at)
             VALUES (?, ?, ?, ?, ?, ?, ?)",
            params![
                EVENT_3BLD,
                METHOD_3STYLE,
                stage,
                case_key,
                entry.alg,
                now,
                now
            ],
        )
        .map_err(|source| DbError::Exec {
            step: "insert user_alg",
            source,
        })?;
    }
    Ok(())
}

pub fn save(db: &mut Db, algs: &UserAlgs) -> Result<(), DbError> {
    let tx = db.transaction()?;
    save_tx(&tx, algs)?;
    tx.commit().map_err(|source| DbError::Exec {
        step: "commit user_alg",
        source,
    })
}

pub fn load(db: &Db) -> Result<UserAlgs, DbError> {
    let step = "select user_alg";
    let mut stmt = db
        .conn()
        .prepare("SELECT stage, case_key, alg FROM user_alg")
        .map_err(map_query(step))?;
    let rows = stmt
        .query_map([], |row| {
            Ok((
                row.get::<_, String>(0)?,
                row.get::<_, String>(1)?,
                row.get::<_, String>(2)?,
            ))
        })
        .map_err(map_query(step))?;
    let mut entries = std::collections::BTreeMap::new();
    for row in rows {
        let (stage, case_key, alg) = row.map_err(map_query(step))?;
        entries.insert(join_key(&stage, &case_key), crate::paths::UserAlg { alg });
    }
    Ok(UserAlgs { entries })
}

fn map_query(step: &'static str) -> impl Fn(rusqlite::Error) -> DbError {
    move |source| DbError::Query { step, source }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::paths::UserAlg;

    fn db() -> Db {
        Db::open_in_memory().expect("db")
    }

    fn algs(items: &[(&str, &str)]) -> UserAlgs {
        let mut algs = UserAlgs::new();
        for (key, alg) in items {
            algs.entries.insert(
                key.to_string(),
                UserAlg {
                    alg: alg.to_string(),
                },
            );
        }
        algs
    }

    #[test]
    fn split_and_join_rebuild_every_domain_key() {
        for key in [
            "edge/UF",
            "corner/URB",
            "flips/none",
            "ltct/UF-UB",
            "edge/UF-UB-UR",
            "a/b/c",
            "/leading",
            "plain",
            "日本語/ケース",
            "quote'key",
        ] {
            let (stage, case_key) = split_key(key);
            assert_eq!(join_key(stage, case_key), key, "key {key}");
        }
    }

    #[test]
    fn save_and_load_round_trips_every_entry() {
        let mut db = db();
        let original = algs(&[
            ("edge/UF", "R2 U' S R2 S' R2 U R2"),
            ("corner/URB", "y' R U' R' U R U' R' U2 R U' R'"),
            ("flips/none", ""),
            ("日本語/ケース", "M2"),
        ]);
        save(&mut db, &original).expect("save");
        assert_eq!(load(&db).expect("load"), original);
    }

    #[test]
    fn save_replaces_the_previous_set() {
        let mut db = db();
        save(&mut db, &algs(&[("edge/UF", "M2"), ("edge/UB", "M2")])).expect("first");
        let second = algs(&[("corner/URB", "R U' R'")]);
        save(&mut db, &second).expect("second");
        assert_eq!(load(&db).expect("load"), second);
    }

    #[test]
    fn saving_an_empty_set_clears_the_table() {
        let mut db = db();
        save(&mut db, &algs(&[("edge/UF", "M2")])).expect("seed");
        save(&mut db, &UserAlgs::new()).expect("clear");
        assert_eq!(load(&db).expect("load"), UserAlgs::new());
    }

    #[test]
    fn rows_carry_the_3bld_event_and_3style_method() {
        let mut db = db();
        save(&mut db, &algs(&[("edge/UF", "M2")])).expect("save");
        let (event, method): (String, String) = db
            .conn()
            .query_row(
                "SELECT event, method FROM user_alg WHERE case_key = 'UF'",
                [],
                |row| Ok((row.get(0)?, row.get(1)?)),
            )
            .expect("row");
        assert_eq!(event, EVENT_3BLD);
        assert_eq!(method, METHOD_3STYLE);
    }
}
