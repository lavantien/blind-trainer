use crate::db::{Db, DbError, EVENT_3BLD, METHOD_3STYLE, now_ms};
use crate::repo_algs::{join_key, split_key};
use crate::srs::{Card, ReviewLogEntry};
use rusqlite::Transaction;
use rusqlite::params;
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;

#[derive(Clone, PartialEq, Debug, Default, Serialize, Deserialize)]
pub struct SrsStore {
    pub cards: BTreeMap<String, Card>,
}

pub fn save_tx(tx: &Transaction<'_>, store: &SrsStore) -> Result<(), DbError> {
    let now = now_ms();
    tx.execute("DELETE FROM card", [])
        .map_err(|source| DbError::Exec {
            step: "clear card",
            source,
        })?;
    for (key, card) in &store.cards {
        let (stage, case_key) = split_key(key);
        tx.execute(
            "INSERT INTO card(event, method, stage, case_key, created_at, updated_at)
             VALUES (?, ?, ?, ?, ?, ?)",
            params![EVENT_3BLD, METHOD_3STYLE, stage, case_key, now, now],
        )
        .map_err(|source| DbError::Exec {
            step: "insert card",
            source,
        })?;
        let card_id = tx.last_insert_rowid();
        tx.execute(
            "INSERT INTO srs_state(card_id, due_day, interval_days, ease, reps, lapses, last_review_day)
             VALUES (?, ?, ?, ?, ?, ?, ?)",
            params![
                card_id,
                card.due_day,
                card.interval_days,
                f64::from(card.ease),
                card.reps,
                card.lapses,
                card.last_review_day()
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

pub fn load(db: &Db) -> Result<SrsStore, DbError> {
    let step = "select card";
    let mut cards = db
        .conn()
        .prepare(
            "SELECT c.id, c.stage, c.case_key, s.due_day, s.interval_days, s.ease, s.reps, s.lapses
             FROM card c JOIN srs_state s ON s.card_id = c.id",
        )
        .map_err(map_query(step))?;
    let card_rows = cards
        .query_map([], |row| {
            Ok((
                row.get::<_, i64>(0)?,
                row.get::<_, String>(1)?,
                row.get::<_, String>(2)?,
                row.get::<_, i64>(3)?,
                row.get::<_, i64>(4)?,
                row.get::<_, f64>(5)?,
                row.get::<_, u32>(6)?,
                row.get::<_, u32>(7)?,
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
    let mut store = SrsStore::default();
    for (id, stage, case_key, due_day, interval_days, ease, reps, lapses) in card_rows {
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
        store.cards.insert(
            join_key(&stage, &case_key),
            Card {
                ease: ease as f32,
                interval_days,
                due_day,
                reps,
                lapses,
                reviews: entries,
            },
        );
    }
    Ok(store)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn db() -> Db {
        Db::open_in_memory().expect("db")
    }

    fn card(ease: f32, reviews: Vec<ReviewLogEntry>) -> Card {
        Card {
            ease,
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
        let mut store = SrsStore::default();
        store.cards.insert(
            "edge/UF".to_string(),
            card(
                2.500_000_2,
                vec![
                    review(10, 4, 0, 1),
                    review(16, 3, 6, 6),
                    review(22, 5, 6, 15),
                ],
            ),
        );
        store.cards.insert(
            "corner/URB".to_string(),
            Card {
                ease: 1.3,
                interval_days: 0,
                due_day: i64::MAX / 4,
                reps: 0,
                lapses: 0,
                reviews: Vec::new(),
            },
        );
        save(&mut db, &store).expect("save");
        assert_eq!(load(&db).expect("load"), store);
    }

    #[test]
    fn review_order_survives_identical_entries() {
        let mut db = db();
        let mut store = SrsStore::default();
        store.cards.insert(
            "edge/UB".to_string(),
            card(
                2.5,
                vec![review(5, 4, 0, 1), review(6, 4, 1, 1), review(5, 4, 0, 1)],
            ),
        );
        save(&mut db, &store).expect("save");
        assert_eq!(load(&db).expect("load"), store);
    }

    #[test]
    fn save_replaces_the_previous_cards() {
        let mut db = db();
        let mut first = SrsStore::default();
        first
            .cards
            .insert("edge/UF".to_string(), card(2.5, Vec::new()));
        save(&mut db, &first).expect("first");
        let mut second = SrsStore::default();
        second
            .cards
            .insert("corner/URB".to_string(), card(1.9, Vec::new()));
        save(&mut db, &second).expect("second");
        assert_eq!(load(&db).expect("load"), second);
    }

    #[test]
    fn saving_an_empty_store_clears_every_card() {
        let mut db = db();
        let mut store = SrsStore::default();
        store
            .cards
            .insert("edge/UF".to_string(), card(2.5, vec![review(1, 4, 0, 1)]));
        save(&mut db, &store).expect("seed");
        save(&mut db, &SrsStore::default()).expect("clear");
        assert_eq!(load(&db).expect("load"), SrsStore::default());
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
                2.5,
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
}
