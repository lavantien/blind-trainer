use crate::db::{Db, DbError, EVENT_3BLD, now_ms};
use crate::fsrs::FsrsConfig;
use crate::repo_srs::{LoadError, insert_card, load_card_rows};
use crate::repo_user::CaseRef;
use crate::srs::Card;
use rusqlite::params;
use std::collections::BTreeMap;

pub type CaseCards = BTreeMap<CaseRef, Card>;

pub fn load(db: &Db, cfg: &FsrsConfig) -> Result<CaseCards, LoadError> {
    let rows = load_card_rows(db, cfg, "c.event <> ?", &[&EVENT_3BLD])?;
    let mut cards = CaseCards::new();
    for row in rows {
        cards.insert(
            CaseRef {
                event: row.event,
                method: row.method,
                stage: row.stage,
                case_key: row.case_key,
            },
            row.card,
        );
    }
    Ok(cards)
}

pub fn save_card(db: &mut Db, key: &CaseRef, card: &Card) -> Result<(), DbError> {
    let now = now_ms();
    let tx = db.transaction()?;
    tx.execute(
        "DELETE FROM card WHERE event = ? AND method = ? AND stage = ? AND case_key = ?",
        params![key.event, key.method, key.stage, key.case_key],
    )
    .map_err(|source| DbError::Exec {
        step: "clear case card",
        source,
    })?;
    insert_card(
        &tx,
        &key.event,
        &key.method,
        &key.stage,
        &key.case_key,
        card,
        now,
    )?;
    tx.commit().map_err(|source| DbError::Exec {
        step: "commit case card",
        source,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::fsrs::FsrsConfig;
    use crate::srs::Rating;

    fn cfg() -> FsrsConfig {
        FsrsConfig::defaults()
    }

    fn db() -> Db {
        Db::open_in_memory().expect("db")
    }

    fn case(event: &str, method: &str, stage: &str, case_key: &str) -> CaseRef {
        CaseRef {
            event: event.to_string(),
            method: method.to_string(),
            stage: stage.to_string(),
            case_key: case_key.to_string(),
        }
    }

    fn graded_card() -> Card {
        let mut card = Card::new(0);
        card.review(Rating::Good, 0, &cfg());
        card.review(Rating::Hard, 3, &cfg());
        card
    }

    #[test]
    fn save_and_load_round_trips_a_graded_case_card() {
        let mut db = db();
        let key = case("2x2", "cll", "cll", "cll.1");
        let card = graded_card();
        save_card(&mut db, &key, &card).expect("save");
        let loaded = load(&db, &cfg()).expect("load");
        assert_eq!(loaded.len(), 1);
        assert_eq!(loaded.get(&key), Some(&card));
    }

    #[test]
    fn load_excludes_the_3bld_pair_rows() {
        let mut db = db();
        let mut pairs = crate::repo_srs::SrsStore::default();
        pairs.cards.insert("edge/UF".to_string(), Card::new(0));
        crate::repo_srs::save(&mut db, &pairs).expect("3bld save");
        let key = case("skewb", "sarah", "sarah", "s.1");
        save_card(&mut db, &key, &Card::new(0)).expect("case save");
        let loaded = load(&db, &cfg()).expect("load");
        assert_eq!(loaded.len(), 1);
        assert!(loaded.contains_key(&key));
    }

    #[test]
    fn saving_a_card_replaces_only_its_own_row() {
        let mut db = db();
        let keep = case("skewb", "sarah", "sarah", "s.1");
        let retake = case("2x2", "cll", "cll", "cll.1");
        let first = graded_card();
        save_card(&mut db, &keep, &first).expect("first");
        save_card(&mut db, &retake, &Card::new(0)).expect("second");
        let mut improved = graded_card();
        improved.review(Rating::Easy, 9, &cfg());
        save_card(&mut db, &retake, &improved).expect("retake");
        let loaded = load(&db, &cfg()).expect("load");
        assert_eq!(loaded.len(), 2);
        assert_eq!(loaded.get(&keep), Some(&first));
        assert_eq!(loaded.get(&retake), Some(&improved));
        let reviews: i64 = db
            .conn()
            .query_row("SELECT COUNT(*) FROM review_log", [], |row| row.get(0))
            .expect("review rows");
        assert_eq!(
            reviews,
            (first.reviews.len() + improved.reviews.len()) as i64,
            "the replaced card must not keep stale review rows"
        );
    }

    #[test]
    fn the_saved_row_carries_the_full_case_reference() {
        let mut db = db();
        let key = case("bigcube", "yau", "parity", "par.1");
        save_card(&mut db, &key, &Card::new(0)).expect("save");
        let row: (String, String, String, String) = db
            .conn()
            .query_row(
                "SELECT event, method, stage, case_key FROM card",
                [],
                |row| Ok((row.get(0)?, row.get(1)?, row.get(2)?, row.get(3)?)),
            )
            .expect("card row");
        assert_eq!(
            row,
            (
                "bigcube".to_string(),
                "yau".to_string(),
                "parity".to_string(),
                "par.1".to_string()
            )
        );
    }
}
