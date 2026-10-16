use crate::packs::{RowOutcome, SectionResult, Tier2Outcome};
use bt_data::seedpack::SeedRow;
use bt_puzzles::skewb::Skewb;
use bt_puzzles::verify_tier1;

pub fn is_rotation(token: &str) -> bool {
    let mut chars = token.chars();
    matches!(chars.next(), Some('x' | 'y' | 'z')) && chars.all(|ch| ch == '\'' || ch == '2')
}

pub fn strip_rotations(alg: &str) -> String {
    alg.split_whitespace()
        .filter(|token| !is_rotation(token))
        .collect::<Vec<&str>>()
        .join(" ")
}

pub fn verify_sarah(rows: &[SeedRow]) -> SectionResult {
    let engine = Skewb::new();
    let mut outcomes = Vec::with_capacity(rows.len());
    for row in rows {
        let stripped = strip_rotations(&row.alg);
        let tier1 = if stripped.is_empty() {
            Err("alg is empty after rotation stripping".to_string())
        } else {
            verify_tier1(&engine, &stripped).map_err(|err| err.to_string())
        };
        outcomes.push(RowOutcome {
            case_key: row.case_key.clone(),
            tier1,
            tier2: Tier2Outcome::Skipped("sarah stage tags are not state-derivable"),
        });
    }
    SectionResult {
        rows: outcomes,
        honesty: vec![
            "skewb sarah tags name method stages and repeat across rows holding one shared alg (R' F R F' covers L2 and L4), so no per-row case state exists to compare"
                .to_string(),
        ],
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rotation_tokens_cover_all_three_axes_and_suffixes() {
        for token in ["x", "y", "z", "x2", "y'", "z2'", "x'2"] {
            assert!(is_rotation(token), "{token}");
        }
        for token in ["F", "R'", "L2", "u", "w", "xx", ""] {
            assert!(!is_rotation(token), "{token}");
        }
    }

    #[test]
    fn strip_rotations_removes_regrip_labels_only() {
        assert_eq!(strip_rotations("y2 R' F R F'"), "R' F R F'");
        assert_eq!(
            strip_rotations("y2 F' L F L' F' L F L'"),
            "F' L F L' F' L F L'"
        );
        assert_eq!(
            strip_rotations("R' F R F' y2 R' F R F' z2 x' R' F R F'"),
            "R' F R F' R' F R F' R' F R F'"
        );
        assert_eq!(strip_rotations("L F' L'"), "L F' L'");
        assert_eq!(strip_rotations("y2 y'"), "");
    }

    #[test]
    fn a_rotation_only_alg_fails_tier1_as_empty() {
        let rows = vec![SeedRow {
            event: "skewb".to_string(),
            method: "sarah_intermediate".to_string(),
            stage: "sarah".to_string(),
            case_key: "sarah.probe".to_string(),
            alg: "y2".to_string(),
            front_json: "{\"case\":\"probe\"}".to_string(),
            source_url: "https://example.test".to_string(),
            source_at: "2026-10-10".to_string(),
            content_hash: String::new(),
        }];
        let result = verify_sarah(&rows);
        assert_eq!(result.rows.len(), 1);
        assert_eq!(
            result.rows[0].tier1,
            Err("alg is empty after rotation stripping".to_string())
        );
        assert_eq!(
            result.rows[0].tier2,
            Tier2Outcome::Skipped("sarah stage tags are not state-derivable")
        );
        assert_eq!(result.honesty.len(), 1);
    }

    #[test]
    fn sarah_rows_pass_tier1_after_rotation_stripping() {
        let rows = vec![
            sarah_row("sarah.01", "L F' L'"),
            sarah_row("sarah.05", "y2 F' L F L' F' L F L'"),
            sarah_row(
                "sarah.15",
                "R' F R F' y2 R' F R F' z2 x' R' F R F' y2 R' F R F'",
            ),
        ];
        let result = verify_sarah(&rows);
        for row in &result.rows {
            assert!(row.tier1.is_ok(), "{}: {:?}", row.case_key, row.tier1);
        }
    }

    fn sarah_row(key: &str, alg: &str) -> SeedRow {
        SeedRow {
            event: "skewb".to_string(),
            method: "sarah_intermediate".to_string(),
            stage: "sarah".to_string(),
            case_key: key.to_string(),
            alg: alg.to_string(),
            front_json: "{\"case\":\"probe\"}".to_string(),
            source_url: "https://example.test".to_string(),
            source_at: "2026-10-10".to_string(),
            content_hash: String::new(),
        }
    }
}
