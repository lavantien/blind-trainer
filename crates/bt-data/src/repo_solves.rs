use crate::db::{Db, DbError};
use rusqlite::Transaction;
use rusqlite::params;
use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct SolveRecord {
    pub started_ms: i64,
    pub memo_ms: i64,
    pub exec_ms: i64,
    pub success: bool,
}

#[derive(Clone, PartialEq, Debug, Default, Serialize, Deserialize)]
pub struct SolveLog {
    pub solves: Vec<SolveRecord>,
}

pub fn save_tx(tx: &Transaction<'_>, log: &SolveLog) -> Result<(), DbError> {
    tx.execute("DELETE FROM solves", [])
        .map_err(|source| DbError::Exec {
            step: "clear solves",
            source,
        })?;
    for record in &log.solves {
        tx.execute(
            "INSERT INTO solves(started_ms, memo_ms, exec_ms, success) VALUES (?, ?, ?, ?)",
            params![
                record.started_ms,
                record.memo_ms,
                record.exec_ms,
                record.success as i64
            ],
        )
        .map_err(|source| DbError::Exec {
            step: "insert solves",
            source,
        })?;
    }
    Ok(())
}

pub fn save(db: &mut Db, log: &SolveLog) -> Result<(), DbError> {
    let tx = db.transaction()?;
    save_tx(&tx, log)?;
    tx.commit().map_err(|source| DbError::Exec {
        step: "commit solves",
        source,
    })
}

fn map_query(step: &'static str) -> impl Fn(rusqlite::Error) -> DbError {
    move |source| DbError::Query { step, source }
}

pub fn load(db: &Db) -> Result<SolveLog, DbError> {
    let step = "select solves";
    let mut stmt = db
        .conn()
        .prepare("SELECT started_ms, memo_ms, exec_ms, success FROM solves ORDER BY id")
        .map_err(map_query(step))?;
    let solves = stmt
        .query_map([], |row| {
            Ok(SolveRecord {
                started_ms: row.get(0)?,
                memo_ms: row.get(1)?,
                exec_ms: row.get(2)?,
                success: row.get::<_, i64>(3)? != 0,
            })
        })
        .map_err(map_query(step))?
        .collect::<Result<Vec<_>, _>>()
        .map_err(map_query(step))?;
    Ok(SolveLog { solves })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn db() -> Db {
        Db::open_in_memory().expect("db")
    }

    #[test]
    fn save_and_load_round_trips_records_in_order() {
        let mut db = db();
        let log = SolveLog {
            solves: vec![
                SolveRecord {
                    started_ms: 5,
                    memo_ms: 40_000,
                    exec_ms: 25_000,
                    success: true,
                },
                SolveRecord {
                    started_ms: -2,
                    memo_ms: 0,
                    exec_ms: i64::MAX / 4,
                    success: false,
                },
                SolveRecord {
                    started_ms: 100,
                    memo_ms: 1,
                    exec_ms: 2,
                    success: true,
                },
            ],
        };
        save(&mut db, &log).expect("save");
        assert_eq!(load(&db).expect("load"), log);
    }

    #[test]
    fn save_replaces_the_previous_log() {
        let mut db = db();
        let first = SolveLog {
            solves: vec![SolveRecord {
                started_ms: 1,
                memo_ms: 1,
                exec_ms: 1,
                success: true,
            }],
        };
        save(&mut db, &first).expect("first");
        let second = SolveLog {
            solves: vec![SolveRecord {
                started_ms: 2,
                memo_ms: 2,
                exec_ms: 2,
                success: false,
            }],
        };
        save(&mut db, &second).expect("second");
        assert_eq!(load(&db).expect("load"), second);
    }

    #[test]
    fn saving_an_empty_log_clears_the_table() {
        let mut db = db();
        let log = SolveLog {
            solves: vec![SolveRecord {
                started_ms: 1,
                memo_ms: 1,
                exec_ms: 1,
                success: true,
            }],
        };
        save(&mut db, &log).expect("seed");
        save(&mut db, &SolveLog::default()).expect("clear");
        assert_eq!(load(&db).expect("load"), SolveLog::default());
    }
}
