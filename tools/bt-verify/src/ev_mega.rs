use crate::packs::{RowOutcome, SectionResult, Tier2Outcome};
use bt_data::seedpack::SeedRow;
use bt_puzzles::mega::{CENTER, Mega, MegaState, NEIGHBOR_CW, U, corner_slot, edge_slot};
use bt_puzzles::{Geometry, verify_tier1};

fn ring() -> usize {
    NEIGHBOR_CW[U as usize].len()
}

pub fn edges_oriented(state: &MegaState) -> bool {
    (0..ring()).all(|k| state.color(U, edge_slot(k)) == U)
}

pub fn corners_oriented(state: &MegaState) -> bool {
    (0..ring()).all(|k| state.color(U, corner_slot(k)) == U)
}

pub fn corners_placed(state: &MegaState) -> bool {
    NEIGHBOR_CW[U as usize].iter().all(|&side| {
        let side_ring = &NEIGHBOR_CW[side as usize];
        let pos = side_ring
            .iter()
            .position(|&face| face == U)
            .expect("side ring holds U");
        state.color(side, corner_slot(pos)) == side
            && state.color(side, corner_slot((pos + ring() - 1) % ring())) == side
    })
}

enum LlStage {
    Eo,
    Co,
    Cp,
    Ep,
}

fn stage_of(tag: &str) -> Result<LlStage, String> {
    match tag.trim_end_matches(|ch: char| ch.is_ascii_digit()) {
        "EO" => Ok(LlStage::Eo),
        "CO" => Ok(LlStage::Co),
        "CP" => Ok(LlStage::Cp),
        "EP" => Ok(LlStage::Ep),
        other => Err(format!("tag {other} names no 4lll stage")),
    }
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

fn case_state(engine: &Mega, alg_text: &str) -> Result<MegaState, String> {
    let alg = engine.parse_alg(alg_text).map_err(|err| err.to_string())?;
    if alg.is_empty() {
        return Err("alg is empty".to_string());
    }
    let inverse = engine.invert_alg(&alg);
    let mut state = engine.solved_state();
    engine
        .apply_alg(&mut state, &inverse)
        .map_err(|err| err.to_string())?;
    Ok(state)
}

fn centers_home(state: &MegaState) -> bool {
    (0..12u8).all(|face| state.color(face, CENTER) == face)
}

fn corners_placed_in_center_frame(engine: &Mega, state: &MegaState) -> bool {
    let unspin = engine.parse_alg("y'").expect("y spin parses");
    let mut aligned = state.clone();
    for _ in 0..ring() {
        if centers_home(&aligned) {
            return corners_placed(&aligned);
        }
        engine
            .apply_alg(&mut aligned, &unspin)
            .expect("y spin applies");
    }
    false
}

fn stage_outcome(engine: &Mega, stage: LlStage, state: &MegaState) -> Tier2Outcome {
    let edges = edges_oriented(state);
    let corners = corners_oriented(state);
    let placed = corners_placed_in_center_frame(engine, state);
    match stage {
        LlStage::Eo if !edges => Tier2Outcome::Pass,
        LlStage::Eo => Tier2Outcome::Fail("edges already oriented".to_string()),
        LlStage::Co if !corners => Tier2Outcome::Pass,
        LlStage::Co => Tier2Outcome::Fail("corners already oriented".to_string()),
        LlStage::Cp if !placed => Tier2Outcome::Pass,
        LlStage::Cp => Tier2Outcome::Fail("corners already placed".to_string()),
        LlStage::Ep if !placed => Tier2Outcome::Fail("corners not placed".to_string()),
        LlStage::Ep if engine.is_solved(state) => {
            Tier2Outcome::Fail("start state is already solved".to_string())
        }
        LlStage::Ep => Tier2Outcome::Pass,
    }
}

pub fn verify_4lll(rows: &[SeedRow]) -> SectionResult {
    let engine = Mega;
    let mut outcomes = Vec::with_capacity(rows.len());
    for row in rows {
        let tier1 = if row.alg.is_empty() {
            Err("alg is empty".to_string())
        } else {
            verify_tier1(&engine, &row.alg).map_err(|err| err.to_string())
        };
        let tier2 = match case_state(&engine, &row.alg).and_then(|state| {
            tag_of(&row.front_json).and_then(|tag| stage_of(&tag).map(|stage| (stage, state)))
        }) {
            Err(_) => Tier2Outcome::Skipped("tier 1 failed"),
            Ok((stage, state)) => stage_outcome(&engine, stage, &state),
        };
        outcomes.push(RowOutcome {
            case_key: row.case_key.clone(),
            tier1,
            tier2,
        });
    }
    SectionResult {
        rows: outcomes,
        honesty: vec![
            "4lll stage predicates are necessary conditions on the start state, the cubeskills case states do not stack as a strict eo co cp ep pipeline, placement is checked in the center-pinned frame"
                .to_string(),
        ],
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn after(engine: &Mega, text: &str) -> MegaState {
        let alg = engine.parse_alg(text).expect("alg parses");
        let mut state = engine.solved_state();
        engine.apply_alg(&mut state, &alg).expect("alg applies");
        state
    }

    #[test]
    fn the_u_ring_holds_five_slots() {
        assert_eq!(ring(), 5);
    }

    #[test]
    fn solved_state_orients_and_places_everything() {
        let engine = Mega;
        let state = engine.solved_state();
        assert!(edges_oriented(&state));
        assert!(corners_oriented(&state));
        assert!(corners_placed(&state));
    }

    #[test]
    fn a_side_turn_disorients_the_u_ring() {
        let engine = Mega;
        let state = after(&engine, "R");
        assert!(!edges_oriented(&state));
        assert!(!corners_oriented(&state));
        assert!(!corners_placed(&state));
    }

    #[test]
    fn a_u_turn_keeps_the_u_face_uniform_but_unplaces_corners() {
        let engine = Mega;
        let state = after(&engine, "U");
        assert!(edges_oriented(&state));
        assert!(corners_oriented(&state));
        assert!(!corners_placed(&state));
    }

    #[test]
    fn tags_map_to_their_stages() {
        assert!(matches!(stage_of("EO1"), Ok(LlStage::Eo)));
        assert!(matches!(stage_of("CO4"), Ok(LlStage::Co)));
        assert!(matches!(stage_of("CP12"), Ok(LlStage::Cp)));
        assert!(matches!(stage_of("EP5"), Ok(LlStage::Ep)));
        assert!(matches!(stage_of("EO"), Ok(LlStage::Eo)));
        assert!(stage_of("Sune").is_err());
    }

    #[test]
    fn a_u_turn_case_passes_cp_and_fails_eo() {
        let engine = Mega;
        let state = case_state(&engine, "U").expect("case state builds");
        assert!(matches!(
            stage_outcome(&engine, LlStage::Cp, &state),
            Tier2Outcome::Pass
        ));
        assert!(matches!(
            stage_outcome(&engine, LlStage::Ep, &state),
            Tier2Outcome::Fail(_)
        ));
        assert!(matches!(
            stage_outcome(&engine, LlStage::Eo, &state),
            Tier2Outcome::Fail(_)
        ));
    }

    #[test]
    fn a_rollback_r_start_case_passes_the_eo_and_co_conditions() {
        let engine = Mega;
        let state = case_state(&engine, "R").expect("case state builds");
        assert!(matches!(
            stage_outcome(&engine, LlStage::Eo, &state),
            Tier2Outcome::Pass
        ));
        assert!(matches!(
            stage_outcome(&engine, LlStage::Co, &state),
            Tier2Outcome::Pass
        ));
        assert!(matches!(
            stage_outcome(&engine, LlStage::Ep, &state),
            Tier2Outcome::Fail(_)
        ));
    }

    #[test]
    fn tag_of_reads_the_case_field() {
        assert_eq!(tag_of("{\"case\":\"EP5\"}"), Ok("EP5".to_string()));
        assert!(tag_of("{\"case\":3}").is_err());
        assert!(tag_of("{}").is_err());
    }
}
