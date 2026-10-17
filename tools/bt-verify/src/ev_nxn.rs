use crate::packs::{RowOutcome, SectionResult, Tier2Outcome};
use bt_data::seedpack::SeedRow;
use bt_puzzles::nxn::{Nxn, NxnAlg, NxnState};
use bt_puzzles::{Geometry, verify_tier1};

const TIP_WORDS: [&str; 6] = ["", "x", "x2", "x'", "z", "z'"];
const TWIST_WORDS: [&str; 4] = ["", "y", "y2", "y'"];
const ROTATION_COUNT: usize = 24;

pub fn nxn_rotations(engine: &Nxn) -> Vec<NxnAlg> {
    let mut distinct: Vec<(NxnState, NxnAlg)> = Vec::new();
    for tip in TIP_WORDS {
        for twist in TWIST_WORDS {
            let text = format!("{tip} {twist}").trim().to_string();
            let Ok(alg) = engine.parse_alg(&text) else {
                continue;
            };
            let mut state = engine.solved_state();
            if engine.apply_alg(&mut state, &alg).is_err() {
                continue;
            }
            if !distinct.iter().any(|(seen, _)| *seen == state) {
                distinct.push((state, alg));
            }
        }
    }
    distinct.into_iter().map(|(_, alg)| alg).collect()
}

fn tier1_nonempty(engine: &Nxn, alg_text: &str) -> Result<(), String> {
    let alg = engine.parse_alg(alg_text).map_err(|err| err.to_string())?;
    if alg.is_empty() {
        return Err("alg is empty".to_string());
    }
    verify_tier1(engine, alg_text).map_err(|err| err.to_string())
}

fn d_layer_solved(engine: &Nxn, state: &NxnState) -> bool {
    let grid = engine.facelets(state);
    let down = &grid.faces[5].rows;
    let down_color = down[0][0];
    let down_uniform = down
        .iter()
        .all(|row| row.iter().all(|cell| *cell == down_color));
    let sides_uniform = (1..=4).all(|side| {
        let bottom = grid.faces[side].rows.last().expect("face holds rows");
        let color = bottom[0];
        bottom.iter().all(|cell| *cell == color)
    });
    down_uniform && sides_uniform
}

fn tier2_cll(engine: &Nxn, rotations: &[NxnAlg], alg_text: &str) -> Tier2Outcome {
    let alg = match engine.parse_alg(alg_text) {
        Ok(alg) => alg,
        Err(err) => return Tier2Outcome::Fail(err.to_string()),
    };
    let inverse = engine.invert_alg(&alg);
    let mut case = engine.solved_state();
    if let Err(err) = engine.apply_alg(&mut case, &inverse) {
        return Tier2Outcome::Fail(err.to_string());
    }
    for rotation in rotations {
        let mut turned = case.clone();
        if let Err(err) = engine.apply_alg(&mut turned, rotation) {
            return Tier2Outcome::Fail(err.to_string());
        }
        if engine.is_solved(&turned) {
            return Tier2Outcome::Fail("start state is already solved".to_string());
        }
    }
    for rotation in rotations {
        let mut turned = case.clone();
        if let Err(err) = engine.apply_alg(&mut turned, rotation) {
            return Tier2Outcome::Fail(err.to_string());
        }
        if d_layer_solved(engine, &turned) {
            return Tier2Outcome::Pass;
        }
    }
    Tier2Outcome::Fail("no rotation leaves the bottom layer solved".to_string())
}

pub fn verify_twoby2(rows: &[SeedRow]) -> SectionResult {
    let engine = Nxn::new(2).expect("2 is inside the engine range");
    let rotations = nxn_rotations(&engine);
    assert_eq!(rotations.len(), ROTATION_COUNT);
    SectionResult {
        rows: rows
            .iter()
            .map(|row| RowOutcome {
                case_key: row.case_key.clone(),
                tier1: tier1_nonempty(&engine, &row.alg),
                tier2: tier2_cll(&engine, &rotations, &row.alg),
            })
            .collect(),
        honesty: Vec::new(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn engine() -> Nxn {
        Nxn::new(2).expect("2 is inside the engine range")
    }

    #[test]
    fn rotation_words_dedupe_to_the_24_cube_orientations() {
        let engine = engine();
        assert_eq!(nxn_rotations(&engine).len(), ROTATION_COUNT);
        let big = Nxn::new(7).expect("7 is inside the engine range");
        assert_eq!(nxn_rotations(&big).len(), ROTATION_COUNT);
    }

    #[test]
    fn solved_state_has_a_solved_bottom_layer() {
        let engine = engine();
        assert!(d_layer_solved(&engine, &engine.solved_state()));
    }

    #[test]
    fn a_mere_u_turn_still_leaves_the_bottom_layer_solved() {
        let engine = engine();
        let rotations = nxn_rotations(&engine);
        assert!(matches!(
            tier2_cll(&engine, &rotations, "U"),
            Tier2Outcome::Pass
        ));
    }

    #[test]
    fn a_corner_three_cycle_has_no_solved_layer() {
        let engine = engine();
        let rotations = nxn_rotations(&engine);
        let outcome = tier2_cll(&engine, &rotations, "R U R'");
        assert!(matches!(outcome, Tier2Outcome::Fail(_)));
    }

    #[test]
    fn a_rotation_only_row_is_rejected_as_already_solved() {
        let engine = engine();
        let rotations = nxn_rotations(&engine);
        for alg in ["x", "z'", "x y2"] {
            assert_eq!(
                tier2_cll(&engine, &rotations, alg),
                Tier2Outcome::Fail("start state is already solved".to_string()),
                "{alg}"
            );
        }
    }

    #[test]
    fn empty_algs_fail_tier1() {
        let engine = engine();
        assert_eq!(tier1_nonempty(&engine, ""), Err("alg is empty".to_string()));
    }
}
