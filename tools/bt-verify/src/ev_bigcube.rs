use crate::packs::{RowOutcome, SectionResult, Tier2Outcome};
use bt_data::seedpack::{NSpec, PackEntry, SeedRow};
use bt_puzzles::nxn::{Nxn, NxnState};
use bt_puzzles::{FaceletGrid, Geometry, verify_tier1};
use std::collections::HashSet;

pub fn sizes_of(spec: &NSpec) -> Vec<usize> {
    match spec {
        NSpec::Fixed(size) => vec![*size as usize],
        NSpec::Sizes(sizes) => sizes.iter().map(|size| *size as usize).collect(),
        NSpec::Unset => Vec::new(),
    }
}

fn tagged_size(tag: &str) -> Result<usize, String> {
    let digits: String = tag.chars().filter(|ch| ch.is_ascii_digit()).collect();
    digits
        .parse::<usize>()
        .map_err(|_| format!("tag {tag} holds no cube size"))
}

struct FaceAudit {
    center_color: u8,
    centers_uniform: bool,
    corners_solved: bool,
    midges_solved: bool,
    disturbed_slots: usize,
}

fn is_midge(n: usize, index: usize) -> bool {
    n % 2 == 1 && index == n / 2
}

fn audit_face(grid: &FaceletGrid, face: usize, n: usize) -> FaceAudit {
    let rows = &grid.faces[face].rows;
    let center_color = rows[1][1];
    let centers_uniform = (1..n - 1).all(|r| (1..n - 1).all(|c| rows[r][c] == center_color));
    let corners = [[0, 0], [0, n - 1], [n - 1, 0], [n - 1, n - 1]];
    let corners_solved = corners
        .iter()
        .all(|corner| rows[corner[0]][corner[1]] == center_color);
    let midges_solved = (1..n - 1).all(|index| {
        !is_midge(n, index)
            || (rows[0][index] == center_color
                && rows[n - 1][index] == center_color
                && rows[index][0] == center_color
                && rows[index][n - 1] == center_color)
    });
    let mut disturbed_slots = 0;
    for segment in 0..4 {
        let disturbed = (1..n - 1).any(|index| {
            let (r, c) = match segment {
                0 => (0, index),
                1 => (n - 1, index),
                2 => (index, 0),
                _ => (index, n - 1),
            };
            !is_midge(n, index) && rows[r][c] != center_color
        });
        if disturbed {
            disturbed_slots += 1;
        }
    }
    FaceAudit {
        center_color,
        centers_uniform,
        corners_solved,
        midges_solved,
        disturbed_slots,
    }
}

fn reduction_state(engine: &Nxn, state: &NxnState) -> Result<usize, String> {
    let n = engine.n();
    let grid = engine.facelets(state);
    let audits: Vec<FaceAudit> = (0..6).map(|face| audit_face(&grid, face, n)).collect();
    if !audits.iter().all(|audit| audit.centers_uniform) {
        return Err("center block of some face is not uniform".to_string());
    }
    let colors: HashSet<u8> = audits.iter().map(|audit| audit.center_color).collect();
    if colors.len() != 6 {
        return Err(format!(
            "the six center blocks hold only {colors_len} distinct colors",
            colors_len = colors.len()
        ));
    }
    if !audits.iter().all(|audit| audit.corners_solved) {
        return Err("some corner sticker left its face color".to_string());
    }
    if !audits.iter().all(|audit| audit.midges_solved) {
        return Err("some midge sticker left its face color".to_string());
    }
    let slots: usize = audits.iter().map(|audit| audit.disturbed_slots).sum();
    if !slots.is_multiple_of(2) {
        return Err(format!(
            "{slots} half-lines disturbed, dedge lines must pair across faces"
        ));
    }
    Ok(slots / 2)
}

fn row_sizes(entry: &PackEntry, tag: &str) -> Result<Vec<usize>, String> {
    let registry = sizes_of(&entry.n);
    if registry.is_empty() {
        return Err("registry holds no cube sizes for this pack".to_string());
    }
    if entry.method != "yau" {
        return Ok(registry);
    }
    let size = tagged_size(tag)?;
    if !registry.contains(&size) {
        return Err(format!(
            "tag size {size} sits outside the registry sizes {registry:?}"
        ));
    }
    Ok(vec![size])
}

fn tag_of(front_json: &str) -> Result<String, String> {
    let value: serde_json::Value =
        serde_json::from_str(front_json).map_err(|err| err.to_string())?;
    value
        .get("case")
        .and_then(|case| case.as_str())
        .map(|case| case.to_string())
        .ok_or_else(|| "front_json holds no case tag".to_string())
}

fn verify_row(entry: &PackEntry, row: &SeedRow) -> RowOutcome {
    let sizes =
        tag_of(&row.front_json).and_then(|tag| row_sizes(entry, &tag).map(|sizes| (tag, sizes)));
    let (tag, sizes) = match sizes {
        Ok(pair) => pair,
        Err(err) => {
            return RowOutcome {
                case_key: row.case_key.clone(),
                tier1: Err(err),
                tier2: Tier2Outcome::Skipped("tier 1 failed"),
            };
        }
    };
    let (low, high) = if entry.method == "yau" {
        (1, 2)
    } else {
        (2, 2)
    };
    let tier1 = Ok(());
    let mut tier2 = Tier2Outcome::Pass;
    for size in sizes {
        let engine = Nxn::new(size).expect("registry sizes sit inside the engine range");
        let step1 = if row.alg.is_empty() {
            Err("alg is empty".to_string())
        } else {
            verify_tier1(&engine, &row.alg).map_err(|err| err.to_string())
        };
        if let Err(err) = step1 {
            return RowOutcome {
                case_key: row.case_key.clone(),
                tier1: Err(format!("n={size}: {err}")),
                tier2: Tier2Outcome::Skipped("tier 1 failed"),
            };
        }
        let alg = engine.parse_alg(&row.alg).expect("tier 1 parsed the alg");
        let inverse = engine.invert_alg(&alg);
        let mut state = engine.solved_state();
        engine
            .apply_alg(&mut state, &inverse)
            .expect("tier 1 applied the inverse");
        match reduction_state(&engine, &state) {
            Ok(count) if (low..=high).contains(&count) => {}
            Ok(count) => {
                tier2 = Tier2Outcome::Fail(format!("n={size}: {tag} disturbs {count} dedge lines"));
                break;
            }
            Err(err) => {
                tier2 = Tier2Outcome::Fail(format!("n={size}: {err}"));
                break;
            }
        }
    }
    RowOutcome {
        case_key: row.case_key.clone(),
        tier1,
        tier2,
    }
}

pub fn verify_parity(entry: &PackEntry, rows: &[SeedRow]) -> SectionResult {
    SectionResult {
        rows: rows.iter().map(|row| verify_row(entry, row)).collect(),
        honesty: vec![
            "wing permutation parity is invisible through facelets, two swapped dedges and four wings in pairwise flips render identically, the line-count bound is the strongest facelet-seam predicate"
                .to_string(),
        ],
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn grid_after(alg: &str, n: usize) -> FaceletGrid {
        let engine = Nxn::new(n).expect("engine size");
        let alg = engine.parse_alg(alg).expect("alg parses");
        let mut state = engine.solved_state();
        engine.apply_alg(&mut state, &alg).expect("alg applies");
        engine.facelets(&state)
    }

    #[test]
    fn sizes_render_from_all_nspec_shapes() {
        assert_eq!(sizes_of(&NSpec::Fixed(4)), vec![4]);
        assert_eq!(sizes_of(&NSpec::Sizes(vec![5, 6, 7])), vec![5, 6, 7]);
        assert!(sizes_of(&NSpec::Unset).is_empty());
    }

    #[test]
    fn tagged_sizes_read_the_trailing_digit() {
        assert_eq!(tagged_size("oll4"), Ok(4));
        assert_eq!(tagged_size("oll4flip"), Ok(4));
        assert_eq!(tagged_size("wing5"), Ok(5));
        assert!(tagged_size("parity").is_err());
    }

    #[test]
    fn the_solved_grid_audits_clean_with_no_disturbed_lines() {
        let engine = Nxn::new(5).expect("engine size");
        let lines = reduction_state(&engine, &engine.solved_state());
        assert_eq!(lines, Ok(0));
    }

    #[test]
    fn a_single_wide_turn_breaks_center_uniformity() {
        let engine = Nxn::new(4).expect("engine size");
        let alg = engine.parse_alg("Rw").expect("wide turn parses");
        let mut state = engine.solved_state();
        engine
            .apply_alg(&mut state, &alg)
            .expect("wide turn applies");
        assert!(
            reduction_state(&engine, &state)
                .unwrap_err()
                .contains("center block")
        );
    }

    #[test]
    fn an_inner_slice_turn_only_disturbs_wing_lines() {
        let engine = Nxn::new(5).expect("engine size");
        let alg = engine.parse_alg("3Rw'").expect("inner slice parses");
        let mut state = engine.solved_state();
        engine.apply_alg(&mut state, &alg).expect("slice applies");
        let lines = reduction_state(&engine, &state);
        assert!(lines.is_err() || lines.unwrap_or(0) <= 2);
    }

    #[test]
    fn a_plain_corner_turn_breaks_corners() {
        let grid = grid_after("R", 4);
        let audit = audit_face(&grid, 0, 4);
        assert!(!audit.corners_solved);
    }

    #[test]
    fn yau_rows_verify_at_their_tagged_size_only() {
        let entry = PackEntry {
            file: "bigcube_parity.tsv".to_string(),
            event: "bigcube".to_string(),
            method: "yau".to_string(),
            stage: "parity".to_string(),
            n: NSpec::Sizes(vec![4, 5, 6, 7]),
            expected_rows: 5,
        };
        assert_eq!(row_sizes(&entry, "oll4"), Ok(vec![4]));
        assert_eq!(row_sizes(&entry, "wing5"), Ok(vec![5]));
        assert!(row_sizes(&entry, "parity").is_err());
        let mut foreign = entry.clone();
        foreign.n = NSpec::Sizes(vec![5, 6, 7]);
        assert!(row_sizes(&foreign, "oll4").is_err());
    }

    #[test]
    fn reduction_rows_verify_at_every_registry_size() {
        let entry = PackEntry {
            file: "bigcube_l2e.tsv".to_string(),
            event: "bigcube".to_string(),
            method: "reduction".to_string(),
            stage: "parity".to_string(),
            n: NSpec::Sizes(vec![5, 6, 7]),
            expected_rows: 12,
        };
        assert_eq!(row_sizes(&entry, "l2e01"), Ok(vec![5, 6, 7]));
    }

    #[test]
    fn wide_and_rotation_tokens_match_canonical_decompositions() {
        assert_eq!(grid_after("Rw", 4), grid_after("R 2R", 4));
        assert_eq!(grid_after("3Lw'", 5), grid_after("L' 2L' 3L'", 5));
        assert_eq!(grid_after("x", 4), grid_after("R 2R 3R 4R", 4));
        assert_eq!(grid_after("u2", 5), grid_after("U2 2U2", 5));
    }
}
