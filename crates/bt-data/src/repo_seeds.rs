use crate::db::{Db, DbError};
use crate::seedpack::{PackEntry, SeedError, SeedRow, load_pack, load_registry};
use rusqlite::{Transaction, params};
use std::path::Path;

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct SeedReport {
    pub file: String,
    pub inserted: usize,
    pub verified: usize,
}

fn map_db(step: &'static str) -> impl Fn(rusqlite::Error) -> SeedError {
    move |source| SeedError::Db(DbError::Query { step, source })
}

pub fn seed_pack_tx(
    tx: &Transaction<'_>,
    file: &str,
    rows: &[SeedRow],
) -> Result<SeedReport, SeedError> {
    let mut report = SeedReport {
        file: file.to_string(),
        inserted: 0,
        verified: 0,
    };
    let step = "select seed_alg";
    let Some(first) = rows.first() else {
        return Ok(report);
    };
    let mut check = tx
        .prepare(
            "SELECT content_hash FROM seed_alg
             WHERE event = ? AND method = ? AND stage = ? AND case_key = ?",
        )
        .map_err(map_db(step))?;
    let mut insert = tx
        .prepare(
            "INSERT INTO seed_alg(event, method, stage, case_key, alg, front_json, source_url, source_at, content_hash)
             VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
        )
        .map_err(map_db("insert seed_alg"))?;
    for row in rows {
        let stored = check
            .query_row(
                params![row.event, row.method, row.stage, row.case_key],
                |hit| hit.get::<_, String>(0),
            )
            .map(Some)
            .or_else(|source| match source {
                rusqlite::Error::QueryReturnedNoRows => Ok(None),
                other => Err(other),
            })
            .map_err(map_db(step))?;
        match stored {
            Some(hash) if hash == row.content_hash => report.verified += 1,
            Some(hash) => {
                return Err(SeedError::Drift {
                    file: file.to_string(),
                    case_key: row.case_key.clone(),
                    stored: hash,
                    packed: row.content_hash.clone(),
                });
            }
            None => {
                insert
                    .execute(params![
                        row.event,
                        row.method,
                        row.stage,
                        row.case_key,
                        row.alg,
                        row.front_json,
                        row.source_url,
                        row.source_at,
                        row.content_hash
                    ])
                    .map_err(map_db("insert seed_alg"))?;
                report.inserted += 1;
            }
        }
    }
    let mut stale = tx
        .prepare(
            "SELECT case_key FROM seed_alg
             WHERE event = ? AND method = ? AND stage = ?",
        )
        .map_err(map_db(step))?;
    let stored_keys = stale
        .query_map(params![first.event, first.method, first.stage], |hit| {
            hit.get::<_, String>(0)
        })
        .map_err(map_db(step))?
        .collect::<Result<Vec<String>, _>>()
        .map_err(map_db(step))?;
    for key in stored_keys {
        if !rows.iter().any(|row| row.case_key == key) {
            return Err(SeedError::Stale {
                file: file.to_string(),
                case_key: key,
            });
        }
    }
    Ok(report)
}

pub fn seed_pack(
    db: &mut Db,
    packs_root: &Path,
    entry: &PackEntry,
) -> Result<SeedReport, SeedError> {
    let rows = load_pack(packs_root, entry)?;
    let tx = db.transaction().map_err(SeedError::Db)?;
    let report = seed_pack_tx(&tx, &entry.file, &rows)?;
    tx.commit().map_err(|source| {
        SeedError::Db(DbError::Exec {
            step: "commit seed_alg",
            source,
        })
    })?;
    Ok(report)
}

pub fn seed_all(db: &mut Db, packs_root: &Path) -> Result<Vec<SeedReport>, SeedError> {
    let entries = load_registry(packs_root)?;
    let mut reports = Vec::with_capacity(entries.len());
    for entry in &entries {
        reports.push(seed_pack(db, packs_root, entry)?);
    }
    Ok(reports)
}

pub fn seed_row_count(db: &Db) -> Result<i64, SeedError> {
    db.conn()
        .query_row("SELECT COUNT(*) FROM seed_alg", [], |row| row.get(0))
        .map_err(map_db("count seed_alg"))
}

pub fn load_seed_rows(db: &Db) -> Result<Vec<SeedRow>, SeedError> {
    let step = "select all seed_alg";
    let mut stmt = db
        .conn()
        .prepare(
            "SELECT event, method, stage, case_key, alg, front_json, source_url, source_at, content_hash
             FROM seed_alg ORDER BY rowid",
        )
        .map_err(map_db(step))?;
    let rows = stmt
        .query_map([], |row| {
            Ok(SeedRow {
                event: row.get(0)?,
                method: row.get(1)?,
                stage: row.get(2)?,
                case_key: row.get(3)?,
                alg: row.get(4)?,
                front_json: row.get(5)?,
                source_url: row.get(6)?,
                source_at: row.get(7)?,
                content_hash: row.get(8)?,
            })
        })
        .map_err(map_db(step))?
        .collect::<Result<Vec<_>, _>>()
        .map_err(map_db(step))?;
    Ok(rows)
}
