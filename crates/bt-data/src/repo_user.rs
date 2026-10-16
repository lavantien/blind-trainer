use crate::db::{Db, DbError, now_ms};
use crate::seedpack::SeedRow;
use rusqlite::params;
use std::collections::BTreeMap;

#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Debug)]
pub struct CaseRef {
    pub event: String,
    pub method: String,
    pub stage: String,
    pub case_key: String,
}

impl CaseRef {
    pub fn of_row(row: &SeedRow) -> CaseRef {
        CaseRef {
            event: row.event.clone(),
            method: row.method.clone(),
            stage: row.stage.clone(),
            case_key: row.case_key.clone(),
        }
    }
}

pub type Overrides = BTreeMap<CaseRef, String>;

fn map_exec(step: &'static str) -> impl Fn(rusqlite::Error) -> DbError {
    move |source| DbError::Exec { step, source }
}

fn map_query(step: &'static str) -> impl Fn(rusqlite::Error) -> DbError {
    move |source| DbError::Query { step, source }
}

pub fn upsert(db: &mut Db, key: &CaseRef, alg: &str) -> Result<(), DbError> {
    let now = now_ms();
    let tx = db.transaction()?;
    let updated = tx
        .execute(
            "UPDATE user_alg SET alg = ?, updated_at = ?
             WHERE event = ? AND method = ? AND stage = ? AND case_key = ?",
            params![alg, now, key.event, key.method, key.stage, key.case_key],
        )
        .map_err(map_exec("update user_alg"))?;
    if updated == 0 {
        tx.execute(
            "INSERT INTO user_alg(event, method, stage, case_key, alg, created_at, updated_at)
             VALUES (?, ?, ?, ?, ?, ?, ?)",
            params![
                key.event,
                key.method,
                key.stage,
                key.case_key,
                alg,
                now,
                now
            ],
        )
        .map_err(map_exec("insert user_alg"))?;
    }
    tx.commit().map_err(map_exec("commit user_alg"))
}

pub fn remove(db: &mut Db, key: &CaseRef) -> Result<bool, DbError> {
    let deleted = db
        .conn()
        .execute(
            "DELETE FROM user_alg
             WHERE event = ? AND method = ? AND stage = ? AND case_key = ?",
            params![key.event, key.method, key.stage, key.case_key],
        )
        .map_err(map_exec("delete user_alg"))?;
    Ok(deleted > 0)
}

pub fn load_all(db: &Db) -> Result<Overrides, DbError> {
    let step = "select user_alg overrides";
    let mut stmt = db
        .conn()
        .prepare("SELECT event, method, stage, case_key, alg FROM user_alg")
        .map_err(map_query(step))?;
    let rows = stmt
        .query_map([], |row| {
            Ok((
                row.get::<_, String>(0)?,
                row.get::<_, String>(1)?,
                row.get::<_, String>(2)?,
                row.get::<_, String>(3)?,
                row.get::<_, String>(4)?,
            ))
        })
        .map_err(map_query(step))?
        .collect::<Result<Vec<_>, _>>()
        .map_err(map_query(step))?;
    let mut overrides = Overrides::new();
    for (event, method, stage, case_key, alg) in rows {
        overrides.insert(
            CaseRef {
                event,
                method,
                stage,
                case_key,
            },
            alg,
        );
    }
    Ok(overrides)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::db::EVENT_3BLD;
    use crate::db::METHOD_3STYLE;
    use crate::paths::UserAlg;
    use crate::repo_algs;
    use crate::seedpack::SeedRow;

    fn db() -> Db {
        Db::open_in_memory().expect("db")
    }

    fn key(case_key: &str) -> CaseRef {
        CaseRef {
            event: "2x2".to_string(),
            method: "cll".to_string(),
            stage: "cll".to_string(),
            case_key: case_key.to_string(),
        }
    }

    #[test]
    fn upsert_inserts_then_updates_a_single_row() {
        let mut db = db();
        upsert(&mut db, &key("cll.1"), "R U R'").expect("insert");
        let loaded = load_all(&db).expect("load");
        assert_eq!(loaded.len(), 1);
        assert_eq!(
            loaded.get(&key("cll.1")).map(String::as_str),
            Some("R U R'")
        );
        upsert(&mut db, &key("cll.1"), "F R F'").expect("update");
        let loaded = load_all(&db).expect("load");
        assert_eq!(loaded.len(), 1);
        assert_eq!(
            loaded.get(&key("cll.1")).map(String::as_str),
            Some("F R F'")
        );
    }

    #[test]
    fn upsert_keeps_distinct_keys_per_stage_and_method() {
        let mut db = db();
        upsert(&mut db, &key("cll.1"), "R U R'").expect("insert");
        let mut other = key("cll.1");
        other.method = "eg".to_string();
        upsert(&mut db, &other, "F R F'").expect("insert");
        let mut staged = key("cll.1");
        staged.stage = "eg1".to_string();
        upsert(&mut db, &staged, "y R'").expect("insert");
        let loaded = load_all(&db).expect("load");
        assert_eq!(loaded.len(), 3);
        assert_eq!(
            loaded.get(&key("cll.1")).map(String::as_str),
            Some("R U R'")
        );
        assert_eq!(loaded.get(&other).map(String::as_str), Some("F R F'"));
        assert_eq!(loaded.get(&staged).map(String::as_str), Some("y R'"));
    }

    #[test]
    fn remove_reports_whether_a_row_existed() {
        let mut db = db();
        assert!(!remove(&mut db, &key("cll.1")).expect("remove missing"));
        upsert(&mut db, &key("cll.1"), "R U R'").expect("insert");
        assert!(remove(&mut db, &key("cll.1")).expect("remove existing"));
        assert!(load_all(&db).expect("load").is_empty());
        assert!(!remove(&mut db, &key("cll.1")).expect("remove again"));
    }

    #[test]
    fn legacy_alg_saves_do_not_wipe_seed_overrides() {
        let mut db = db();
        upsert(&mut db, &key("cll.1"), "R U R'").expect("override");
        let mut legacy = crate::paths::UserAlgs::new();
        legacy.entries.insert(
            "edge/UF".to_string(),
            UserAlg {
                alg: "M2".to_string(),
            },
        );
        repo_algs::save(&mut db, &legacy).expect("legacy save");
        let loaded = load_all(&db).expect("load");
        assert_eq!(
            loaded.get(&key("cll.1")).map(String::as_str),
            Some("R U R'"),
            "legacy saves must keep seed overrides"
        );
    }

    #[test]
    fn legacy_alg_loads_skip_seed_override_rows() {
        let mut db = db();
        upsert(&mut db, &key("cll.1"), "R U R'").expect("override");
        let legacy = repo_algs::load(&db).expect("legacy load");
        assert!(
            legacy.entries.is_empty(),
            "seed overrides must not leak into the 3bld overlay"
        );
        let legacy_db = Db::open_in_memory().expect("db");
        legacy_db
            .conn()
            .execute(
                "INSERT INTO user_alg(event, method, stage, case_key, alg, created_at, updated_at)
                 VALUES (?, ?, ?, ?, ?, 0, 0)",
                params![EVENT_3BLD, METHOD_3STYLE, "edge", "UF", "M2"],
            )
            .expect("legacy row");
        let legacy = repo_algs::load(&legacy_db).expect("legacy load");
        assert_eq!(legacy.entries.len(), 1);
        assert_eq!(
            legacy
                .entries
                .get("edge/UF")
                .map(|entry| entry.alg.as_str()),
            Some("M2")
        );
    }

    #[test]
    fn case_ref_of_row_copies_the_row_identity() {
        let row = SeedRow {
            event: "skewb".to_string(),
            method: "sarah".to_string(),
            stage: "sarah".to_string(),
            case_key: "s.1".to_string(),
            alg: "R U R'".to_string(),
            front_json: "{}".to_string(),
            source_url: String::new(),
            source_at: "2026-01-01".to_string(),
            content_hash: String::new(),
        };
        assert_eq!(
            CaseRef::of_row(&row),
            CaseRef {
                event: "skewb".to_string(),
                method: "sarah".to_string(),
                stage: "sarah".to_string(),
                case_key: "s.1".to_string(),
            }
        );
    }
}
