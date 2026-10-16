use crate::packs::{RowOutcome, SectionResult, Tier2Outcome};
use bt_data::seedpack::SeedRow;
use bt_puzzles::fto::Fto;
use bt_puzzles::verify_tier1;

pub fn verify_l4t(rows: &[SeedRow]) -> SectionResult {
    let engine = Fto::new();
    let mut outcomes = Vec::with_capacity(rows.len());
    for row in rows {
        let tier1 = if row.alg.is_empty() {
            Err("alg is empty".to_string())
        } else {
            verify_tier1(&engine, &row.alg).map_err(|err| err.to_string())
        };
        outcomes.push(RowOutcome {
            case_key: row.case_key.clone(),
            tier1,
            tier2: Tier2Outcome::Skipped("l4t case families have no state predicate"),
        });
    }
    SectionResult {
        rows: outcomes,
        honesty: vec![
            "fto l4t tags name source-sheet case families (Last Layer, Last Three Edges, bar shapes), the reduction layer geometry has no engine predicate, rows are tier-1 round trips only"
                .to_string(),
        ],
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn row(key: &str, alg: &str) -> SeedRow {
        SeedRow {
            event: "fto".to_string(),
            method: "l4t_redux".to_string(),
            stage: "l4t".to_string(),
            case_key: key.to_string(),
            alg: alg.to_string(),
            front_json: "{\"case\":\"probe\"}".to_string(),
            source_url: "https://example.test".to_string(),
            source_at: "2026-10-10".to_string(),
            content_hash: String::new(),
        }
    }

    #[test]
    fn a_pack_alg_passes_tier1_and_reports_the_honest_skip() {
        let result = verify_l4t(&[row("l4t.probe", "U BR U BR' F U F'")]);
        assert_eq!(result.rows.len(), 1);
        assert!(result.rows[0].tier1.is_ok());
        assert_eq!(
            result.rows[0].tier2,
            Tier2Outcome::Skipped("l4t case families have no state predicate")
        );
        assert_eq!(result.honesty.len(), 1);
    }

    #[test]
    fn a_bad_token_fails_tier1() {
        let result = verify_l4t(&[row("l4t.bad", "Q")]);
        assert!(result.rows[0].tier1.is_err());
    }

    #[test]
    fn an_empty_alg_fails_tier1() {
        let result = verify_l4t(&[row("l4t.empty", "")]);
        assert_eq!(result.rows[0].tier1, Err("alg is empty".to_string()));
    }
}
