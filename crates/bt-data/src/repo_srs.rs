use crate::db::{Db, DbError, EVENT_3BLD, METHOD_3STYLE, now_ms};
use crate::fsrs::{FsrsConfig, MemoryState};
use crate::repo_algs::{join_key, split_key};
use crate::srs::{Card, ReplayError, ReviewLogEntry, replay_sm2};
use rusqlite::Transaction;
use rusqlite::params;
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::fmt;

#[derive(Clone, PartialEq, Debug, Default, Serialize, Deserialize)]
pub struct SrsStore {
    pub cards: BTreeMap<String, Card>,
}

#[derive(Debug)]
pub enum LoadError {
    Db(DbError),
    UnknownQuality {
        key: String,
        quality: u32,
    },
    InvalidMemory {
        key: String,
        stability: Option<f64>,
        difficulty: Option<f64>,
    },
}

impl fmt::Display for LoadError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            LoadError::Db(source) => write!(f, "{source}"),
            LoadError::UnknownQuality { key, quality } => {
                write!(
                    f,
                    "card {key} holds review quality {quality} with no sm-2 grade mapping"
                )
            }
            LoadError::InvalidMemory {
                key,
                stability,
                difficulty,
            } => write!(
                f,
                "card {key} holds an invalid fsrs memory state (stability {stability:?}, difficulty {difficulty:?})"
            ),
        }
    }
}

impl std::error::Error for LoadError {}

impl From<DbError> for LoadError {
    fn from(source: DbError) -> Self {
        LoadError::Db(source)
    }
}

pub fn save_tx(tx: &Transaction<'_>, store: &SrsStore) -> Result<(), DbError> {
    let now = now_ms();
    tx.execute(
        "DELETE FROM card WHERE event = ? AND method = ?",
        params![EVENT_3BLD, METHOD_3STYLE],
    )
    .map_err(|source| DbError::Exec {
        step: "clear card",
        source,
    })?;
    for (key, card) in &store.cards {
        let (stage, case_key) = split_key(key);
        insert_card(tx, EVENT_3BLD, METHOD_3STYLE, stage, case_key, card, now)?;
    }
    Ok(())
}

pub(crate) fn insert_card(
    tx: &Transaction<'_>,
    event: &str,
    method: &str,
    stage: &str,
    case_key: &str,
    card: &Card,
    now: i64,
) -> Result<(), DbError> {
    tx.execute(
        "INSERT INTO card(event, method, stage, case_key, created_at, updated_at)
         VALUES (?, ?, ?, ?, ?, ?)",
        params![event, method, stage, case_key, now, now],
    )
    .map_err(|source| DbError::Exec {
        step: "insert card",
        source,
    })?;
    let card_id = tx.last_insert_rowid();
    tx.execute(
        "INSERT INTO srs_state(card_id, due_day, interval_days, reps, lapses, last_review_day, stability, difficulty)
         VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        params![
            card_id,
            card.due_day,
            card.interval_days,
            card.reps,
            card.lapses,
            card.last_review_day(),
            card.memory.map(|memory| memory.stability),
            card.memory.map(|memory| memory.difficulty),
        ],
    )
    .map_err(|source| DbError::Exec {
        step: "insert srs_state",
        source,
    })?;
    let mut interval_before = 0i64;
    for entry in &card.reviews {
        tx.execute(
            "INSERT INTO review_log(card_id, rating, reviewed_day, interval_before, interval_after, elapsed_days)
             VALUES (?, ?, ?, ?, ?, ?)",
            params![
                card_id,
                entry.quality,
                entry.ts_days,
                interval_before,
                entry.scheduled_days,
                entry.elapsed_days
            ],
        )
        .map_err(|source| DbError::Exec {
            step: "insert review_log",
            source,
        })?;
        interval_before = entry.scheduled_days;
    }
    Ok(())
}

pub fn save(db: &mut Db, store: &SrsStore) -> Result<(), DbError> {
    let tx = db.transaction()?;
    save_tx(&tx, store)?;
    tx.commit().map_err(|source| DbError::Exec {
        step: "commit card",
        source,
    })
}

fn map_query(step: &'static str) -> impl Fn(rusqlite::Error) -> DbError {
    move |source| DbError::Query { step, source }
}

fn memory_from_row(
    key: String,
    stability: Option<f64>,
    difficulty: Option<f64>,
    entries: &[ReviewLogEntry],
    cfg: &FsrsConfig,
) -> Result<Option<MemoryState>, LoadError> {
    let memory = match (stability, difficulty) {
        (None, None) => replay_sm2(entries, cfg).map_err(|source| match source {
            ReplayError::UnknownQuality(quality) => LoadError::UnknownQuality {
                key: key.clone(),
                quality,
            },
        })?,
        (Some(stability), Some(difficulty)) => Some(MemoryState {
            stability,
            difficulty,
        }),
        _ => {
            return Err(LoadError::InvalidMemory {
                key,
                stability,
                difficulty,
            });
        }
    };
    if let Some(memory) = memory
        && !memory.is_valid()
    {
        return Err(LoadError::InvalidMemory {
            key,
            stability: Some(memory.stability),
            difficulty: Some(memory.difficulty),
        });
    }
    Ok(memory)
}

pub(crate) struct CardRow {
    pub event: String,
    pub method: String,
    pub stage: String,
    pub case_key: String,
    pub card: Card,
}

pub(crate) fn load_card_rows(
    db: &Db,
    cfg: &FsrsConfig,
    filter: &str,
    filter_params: &[&dyn rusqlite::ToSql],
) -> Result<Vec<CardRow>, LoadError> {
    let step = "select card";
    let sql = format!(
        "SELECT c.id, c.event, c.method, c.stage, c.case_key, s.due_day, s.interval_days, s.reps, s.lapses, s.stability, s.difficulty
         FROM card c JOIN srs_state s ON s.card_id = c.id WHERE {filter}"
    );
    let mut cards = db.conn().prepare(&sql).map_err(map_query(step))?;
    let card_rows = cards
        .query_map(filter_params, |row| {
            Ok((
                row.get::<_, i64>(0)?,
                row.get::<_, String>(1)?,
                row.get::<_, String>(2)?,
                row.get::<_, String>(3)?,
                row.get::<_, String>(4)?,
                row.get::<_, i64>(5)?,
                row.get::<_, i64>(6)?,
                row.get::<_, u32>(7)?,
                row.get::<_, u32>(8)?,
                row.get::<_, Option<f64>>(9)?,
                row.get::<_, Option<f64>>(10)?,
            ))
        })
        .map_err(map_query(step))?
        .collect::<Result<Vec<_>, _>>()
        .map_err(map_query(step))?;
    let mut reviews = db
        .conn()
        .prepare(
            "SELECT rating, reviewed_day, elapsed_days, interval_after
             FROM review_log WHERE card_id = ? ORDER BY id",
        )
        .map_err(map_query(step))?;
    let mut rows = Vec::new();
    for (
        id,
        event,
        method,
        stage,
        case_key,
        due_day,
        interval_days,
        reps,
        lapses,
        stability,
        difficulty,
    ) in card_rows
    {
        let entries = reviews
            .query_map(params![id], |row| {
                Ok(ReviewLogEntry {
                    quality: row.get(0)?,
                    ts_days: row.get(1)?,
                    elapsed_days: row.get(2)?,
                    scheduled_days: row.get(3)?,
                })
            })
            .map_err(map_query(step))?
            .collect::<Result<Vec<_>, _>>()
            .map_err(map_query(step))?;
        let key = join_key(&stage, &case_key);
        let memory = memory_from_row(key, stability, difficulty, &entries, cfg)?;
        rows.push(CardRow {
            event,
            method,
            stage,
            case_key,
            card: Card {
                memory,
                interval_days,
                due_day,
                reps,
                lapses,
                reviews: entries,
            },
        });
    }
    Ok(rows)
}

pub fn load(db: &Db, cfg: &FsrsConfig) -> Result<SrsStore, LoadError> {
    let rows = load_card_rows(
        db,
        cfg,
        "c.event = ? AND c.method = ?",
        &[&EVENT_3BLD, &METHOD_3STYLE],
    )?;
    let mut store = SrsStore::default();
    for row in rows {
        store
            .cards
            .insert(join_key(&row.stage, &row.case_key), row.card);
    }
    Ok(store)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn cfg() -> FsrsConfig {
        FsrsConfig::defaults()
    }

    fn db() -> Db {
        Db::open_in_memory().expect("db")
    }

    fn card(memory: Option<MemoryState>, reviews: Vec<ReviewLogEntry>) -> Card {
        Card {
            memory,
            interval_days: 6,
            due_day: -3,
            reps: 4,
            lapses: 2,
            reviews,
        }
    }

    fn review(ts: i64, quality: u32, elapsed: i64, scheduled: i64) -> ReviewLogEntry {
        ReviewLogEntry {
            ts_days: ts,
            quality,
            elapsed_days: elapsed,
            scheduled_days: scheduled,
        }
    }

    #[test]
    fn save_and_load_round_trips_cards_with_review_history() {
        let mut db = db();
        let graded = {
            let mut graded = Card::new(0);
            graded.review(crate::srs::Rating::Good, 0, &cfg());
            graded.review(crate::srs::Rating::Good, 1, &cfg());
            graded.review(crate::srs::Rating::Easy, 7, &cfg());
            graded
        };
        let mut store = SrsStore::default();
        store.cards.insert("edge/UF".to_string(), graded.clone());
        store.cards.insert(
            "corner/URB".to_string(),
            Card {
                memory: None,
                interval_days: 0,
                due_day: i64::MAX / 4,
                reps: 0,
                lapses: 0,
                reviews: Vec::new(),
            },
        );
        save(&mut db, &store).expect("save");
        assert_eq!(load(&db, &cfg()).expect("load"), store);
        let stored: (Option<f64>, Option<f64>) = db
            .conn()
            .query_row(
                "SELECT stability, difficulty FROM srs_state JOIN card ON card.id = srs_state.card_id WHERE card.case_key = 'UF'",
                [],
                |row| Ok((row.get(0)?, row.get(1)?)),
            )
            .expect("state row");
        let graded_memory = graded.memory.expect("graded card holds memory");
        assert_eq!(
            stored,
            (
                Some(graded_memory.stability),
                Some(graded_memory.difficulty)
            )
        );
    }

    #[test]
    fn review_order_survives_identical_entries() {
        let mut db = db();
        let mut store = SrsStore::default();
        let memory = replay_sm2(&[review(5, 4, 0, 1), review(6, 4, 1, 1)], &cfg())
            .expect("replay")
            .expect("state");
        store.cards.insert(
            "edge/UB".to_string(),
            card(
                Some(memory),
                vec![review(5, 4, 0, 1), review(6, 4, 1, 1), review(5, 4, 0, 1)],
            ),
        );
        save(&mut db, &store).expect("save");
        assert_eq!(load(&db, &cfg()).expect("load"), store);
    }

    #[test]
    fn save_replaces_the_previous_cards() {
        let mut db = db();
        let mut first = SrsStore::default();
        first
            .cards
            .insert("edge/UF".to_string(), card(None, Vec::new()));
        save(&mut db, &first).expect("first");
        let mut second = SrsStore::default();
        second
            .cards
            .insert("corner/URB".to_string(), card(None, Vec::new()));
        save(&mut db, &second).expect("second");
        assert_eq!(load(&db, &cfg()).expect("load"), second);
    }

    #[test]
    fn saving_an_empty_store_clears_every_card() {
        let mut db = db();
        let mut store = SrsStore::default();
        store
            .cards
            .insert("edge/UF".to_string(), card(None, vec![review(1, 4, 0, 1)]));
        save(&mut db, &store).expect("seed");
        save(&mut db, &SrsStore::default()).expect("clear");
        assert_eq!(load(&db, &cfg()).expect("load"), SrsStore::default());
        let reviews: i64 = db
            .conn()
            .query_row("SELECT COUNT(*) FROM review_log", [], |row| row.get(0))
            .expect("review rows");
        assert_eq!(reviews, 0);
    }

    #[test]
    fn interval_before_tracks_the_previous_scheduled_interval() {
        let mut db = db();
        let mut store = SrsStore::default();
        store.cards.insert(
            "edge/UF".to_string(),
            card(
                None,
                vec![
                    review(10, 4, 0, 1),
                    review(16, 4, 6, 6),
                    review(22, 4, 6, 15),
                ],
            ),
        );
        save(&mut db, &store).expect("save");
        let mut stmt = db
            .conn()
            .prepare("SELECT interval_before FROM review_log ORDER BY id")
            .expect("prepare");
        let intervals: Vec<i64> = stmt
            .query_map([], |row| row.get(0))
            .expect("query")
            .map(|row| row.expect("row"))
            .collect();
        assert_eq!(intervals, [0, 1, 6]);
    }

    #[test]
    fn legacy_null_state_rows_derive_memory_from_the_review_log() {
        let mut db = db();
        let history = vec![review(0, 4, 0, 3), review(1, 4, 1, 6), review(7, 4, 6, 20)];
        let mut store = SrsStore::default();
        store
            .cards
            .insert("edge/UF".to_string(), card(None, history.clone()));
        save(&mut db, &store).expect("seed");
        db.conn()
            .execute(
                "UPDATE srs_state SET stability = NULL, difficulty = NULL",
                [],
            )
            .expect("strip state");
        let loaded = load(&db, &cfg()).expect("load");
        let expected = replay_sm2(&history, &cfg())
            .expect("replay")
            .expect("state");
        assert_eq!(loaded.cards["edge/UF"].memory, Some(expected));
    }

    #[test]
    fn a_card_without_reviews_or_state_loads_with_no_memory() {
        let mut db = db();
        let mut store = SrsStore::default();
        store
            .cards
            .insert("edge/UF".to_string(), card(None, Vec::new()));
        save(&mut db, &store).expect("seed");
        assert_eq!(load(&db, &cfg()).expect("load"), store);
    }

    #[test]
    fn corrupt_state_columns_fail_the_load_naming_the_card() {
        let mut db = db();
        let mut store = SrsStore::default();
        let memory = replay_sm2(&[review(0, 4, 0, 3)], &cfg())
            .expect("replay")
            .expect("state");
        store.cards.insert(
            "edge/UF".to_string(),
            card(Some(memory), vec![review(0, 4, 0, 3)]),
        );
        save(&mut db, &store).expect("seed");
        db.conn()
            .execute("UPDATE srs_state SET stability = -1.0", [])
            .expect("corrupt stability");
        let err = load(&db, &cfg()).expect_err("negative stability must fail");
        let LoadError::InvalidMemory {
            key,
            stability,
            difficulty,
        } = err
        else {
            panic!("expected InvalidMemory, got {err}")
        };
        assert_eq!(key, "edge/UF");
        assert_eq!(stability, Some(-1.0));
        assert!(difficulty.is_some());
    }

    #[test]
    fn half_present_state_columns_fail_the_load() {
        let mut db = db();
        let mut store = SrsStore::default();
        store
            .cards
            .insert("edge/UF".to_string(), card(None, vec![review(0, 4, 0, 3)]));
        save(&mut db, &store).expect("seed");
        db.conn()
            .execute(
                "UPDATE srs_state SET stability = 3.173, difficulty = NULL",
                [],
            )
            .expect("half state");
        let err = load(&db, &cfg()).expect_err("half state must fail");
        assert!(err.to_string().contains("edge/UF"), "err: {err}");
    }

    #[test]
    fn an_unknown_legacy_quality_fails_the_load() {
        let mut db = db();
        let mut store = SrsStore::default();
        store
            .cards
            .insert("edge/UF".to_string(), card(None, vec![review(0, 7, 0, 3)]));
        save(&mut db, &store).expect("seed");
        db.conn()
            .execute(
                "UPDATE srs_state SET stability = NULL, difficulty = NULL",
                [],
            )
            .expect("strip state");
        let err = load(&db, &cfg()).expect_err("quality 7 has no grade");
        let LoadError::UnknownQuality { key, quality } = err else {
            panic!("expected UnknownQuality, got {err}")
        };
        assert_eq!(key, "edge/UF");
        assert_eq!(quality, 7);
    }

    #[test]
    fn legacy_derivation_uses_the_loaded_config_not_the_defaults() {
        let mut db = db();
        let mut store = SrsStore::default();
        store
            .cards
            .insert("edge/UF".to_string(), card(None, vec![review(0, 2, 0, 1)]));
        save(&mut db, &store).expect("seed");
        db.conn()
            .execute(
                "UPDATE srs_state SET stability = NULL, difficulty = NULL",
                [],
            )
            .expect("strip state");
        let mut perturbed = FsrsConfig::defaults();
        perturbed.weights[0] = 5.0;
        let loaded = load(&db, &perturbed).expect("load");
        let memory = loaded.cards["edge/UF"].memory.expect("derived memory");
        assert_eq!(memory.stability, 5.0);
    }

    #[test]
    fn an_invalid_replayed_state_fails_the_load_naming_the_card() {
        let mut db = db();
        let mut store = SrsStore::default();
        store.cards.insert(
            "edge/UF".to_string(),
            card(
                None,
                vec![review(0, 4, 0, 3), review(1, 4, 1, 6), review(7, 2, 6, 2)],
            ),
        );
        save(&mut db, &store).expect("seed");
        db.conn()
            .execute(
                "UPDATE srs_state SET stability = NULL, difficulty = NULL",
                [],
            )
            .expect("strip state");
        let mut degenerate = FsrsConfig::defaults();
        degenerate.weights[11] = 0.0;
        let err = load(&db, &degenerate).expect_err("zero forget weight must invalidate");
        assert!(err.to_string().contains("edge/UF"), "err: {err}");
    }

    #[test]
    fn saving_the_3bld_store_spares_foreign_event_cards() {
        let mut db = db();
        let foreign = crate::repo_user::CaseRef {
            event: "2x2".to_string(),
            method: "cll".to_string(),
            stage: "cll".to_string(),
            case_key: "cll.1".to_string(),
        };
        let mut graded = Card::new(0);
        graded.review(crate::srs::Rating::Good, 0, &cfg());
        crate::repo_cards::save_card(&mut db, &foreign, &graded).expect("seed foreign");
        let mut store = SrsStore::default();
        store
            .cards
            .insert("edge/UF".to_string(), card(None, Vec::new()));
        save(&mut db, &store).expect("3bld save");
        let survived = crate::repo_cards::load(&db, &cfg()).expect("cards");
        assert_eq!(
            survived.get(&foreign).map(|card| card.reps),
            Some(1),
            "a 3bld store save must not wipe case cards"
        );
        assert_eq!(
            load(&db, &cfg()).expect("3bld load").cards.len(),
            1,
            "the 3bld store still loads its own rows"
        );
    }

    #[test]
    fn load_ignores_foreign_event_rows() {
        let mut db = db();
        let foreign = crate::repo_user::CaseRef {
            event: "skewb".to_string(),
            method: "sarah".to_string(),
            stage: "sarah".to_string(),
            case_key: "s.1".to_string(),
        };
        crate::repo_cards::save_card(&mut db, &foreign, &Card::new(0)).expect("seed");
        assert_eq!(load(&db, &cfg()).expect("load"), SrsStore::default());
    }
}
