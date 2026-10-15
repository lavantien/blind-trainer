use bt_core::{Face, Move, Slice};
use bt_scheme::Orbit;
use std::cmp::Ordering;

const LANE_SLICE_BASE: usize = 6;

#[derive(Clone, Debug, PartialEq)]
pub struct ScoringCfg {
    pub preferred: [bool; 9],
    pub preferred_edge: [bool; 9],
    pub penalty_other: f32,
    pub weight_len: f32,
    pub weight_subset: f32,
    pub weight_regrip: f32,
    pub weight_finger: f32,
    pub finger_cost: [[f32; 2]; 6],
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ScoreParts {
    pub qtm: u32,
    pub subset: f32,
    pub regrips: u32,
    pub finger: f32,
    pub total: f32,
}

fn face_letter(face: Face) -> char {
    match face {
        Face::U => 'U',
        Face::L => 'L',
        Face::F => 'F',
        Face::R => 'R',
        Face::B => 'B',
        Face::D => 'D',
    }
}

fn lane_index(letter: char) -> Option<usize> {
    match letter {
        'U' => Some(Face::U.index()),
        'L' => Some(Face::L.index()),
        'F' => Some(Face::F.index()),
        'R' => Some(Face::R.index()),
        'B' => Some(Face::B.index()),
        'D' => Some(Face::D.index()),
        'M' => Some(LANE_SLICE_BASE),
        'E' => Some(LANE_SLICE_BASE + 1),
        'S' => Some(LANE_SLICE_BASE + 2),
        _ => None,
    }
}

fn face_lane(face: Face) -> usize {
    face.index()
}

fn slice_lane(slice: Slice) -> usize {
    LANE_SLICE_BASE
        + match slice {
            Slice::M => 0,
            Slice::E => 1,
            Slice::S => 2,
        }
}

fn slice_follows(slice: Slice) -> Face {
    match slice {
        Slice::M => Face::L,
        Slice::E => Face::D,
        Slice::S => Face::F,
    }
}

fn finger_face_of(mv: Move) -> Option<Face> {
    match mv {
        Move::Rotation { .. } => None,
        Move::Slice { slice, .. } => Some(slice_follows(slice)),
        Move::Face { face, .. } | Move::Wide { face, .. } | Move::Inner { face, .. } => Some(face),
    }
}

fn preferred_lane_of(mv: Move, orbit: Orbit) -> Option<usize> {
    match mv {
        Move::Rotation { .. } => None,
        Move::Slice { slice, .. } => Some(if orbit == Orbit::Edge {
            slice_lane(slice)
        } else {
            face_lane(slice_follows(slice))
        }),
        Move::Face { face, .. } | Move::Wide { face, .. } | Move::Inner { face, .. } => {
            Some(face_lane(face))
        }
    }
}

fn side_of(face: Face) -> Option<usize> {
    match face {
        Face::U | Face::R => Some(0),
        Face::L | Face::D => Some(1),
        Face::F | Face::B => None,
    }
}

pub fn count_regrips(moves: &[Move]) -> Option<u32> {
    let mut displaced = [false; 2];
    let mut last = 0usize;
    let mut prev: Option<Face> = None;
    let mut regrips = 0u32;
    for &mv in moves {
        let face = finger_face_of(mv)?;
        if prev == Some(face) {
            continue;
        }
        prev = Some(face);
        let side = side_of(face);
        let hand = side.unwrap_or(last);
        if side.is_some() {
            last = hand;
        }
        if displaced[hand] {
            regrips += 1;
        }
        displaced[hand] = side != Some(hand);
    }
    Some(regrips)
}

pub fn score(moves: &[Move], orbit: Orbit, cfg: &ScoringCfg) -> Option<ScoreParts> {
    let preferred = match orbit {
        Orbit::Edge => &cfg.preferred_edge,
        Orbit::Corner => &cfg.preferred,
    };
    let mut qtm = 0u32;
    let mut subset = 0.0f32;
    let mut finger = 0.0f32;
    for &mv in moves {
        let quarter = mv.quarter();
        qtm += if quarter == 2 { 2 } else { 1 };
        let lane = preferred_lane_of(mv, orbit)?;
        if !preferred[lane] {
            subset += cfg.penalty_other;
        }
        let face = finger_face_of(mv)?;
        let dir = usize::from(quarter == 3);
        finger += cfg.finger_cost[face.index()][dir];
    }
    let regrips = count_regrips(moves)?;
    let total = cfg.weight_len * qtm as f32
        + cfg.weight_subset * subset
        + cfg.weight_regrip * regrips as f32
        + cfg.weight_finger * finger;
    Some(ScoreParts {
        qtm,
        subset,
        regrips,
        finger,
        total,
    })
}

pub fn rank_cmp(left: &[Move], right: &[Move], orbit: Orbit, cfg: &ScoringCfg) -> Ordering {
    let left_total = score(left, orbit, cfg).map_or(f32::INFINITY, |parts| parts.total);
    let right_total = score(right, orbit, cfg).map_or(f32::INFINITY, |parts| parts.total);
    left_total
        .total_cmp(&right_total)
        .then_with(|| left.len().cmp(&right.len()))
}

fn parse_lanes(value: &serde_json::Value, key: &str) -> Result<[bool; 9], String> {
    let entries = value.as_array().ok_or_else(|| {
        format!("synth.scoring.{key} entries must be single lanes from U D L R F B M E S")
    })?;
    if entries.is_empty() {
        return Err(format!("synth.scoring.{key} must list at least one lane"));
    }
    let mut lanes = [false; 9];
    for entry in entries {
        let letter = entry
            .as_str()
            .filter(|text| text.chars().count() == 1)
            .and_then(|text| lane_index(text.chars().next().unwrap()))
            .ok_or_else(|| {
                format!("synth.scoring.{key} entries must be single lanes from U D L R F B M E S")
            })?;
        lanes[letter] = true;
    }
    Ok(lanes)
}

fn parse_number(value: &serde_json::Value, key: &str, floor_zero: bool) -> Result<f32, String> {
    let suffix = if floor_zero { " >= 0" } else { "" };
    let message = format!("synth.scoring.{key} must be a finite number{suffix}");
    let raw = value.as_f64().ok_or_else(|| message.clone())?;
    let parsed = raw as f32;
    if !parsed.is_finite() || (floor_zero && parsed < 0.0) {
        return Err(message);
    }
    Ok(parsed)
}

fn parse_finger_cost(value: &serde_json::Value, key: &str) -> Result<[[f32; 2]; 6], String> {
    let lanes_message = format!("synth.scoring.{key} must hold exactly the lanes U D L R F B");
    let map = value.as_object().ok_or_else(|| lanes_message.clone())?;
    let mut costs = [[0.0f32; 2]; 6];
    for face in Face::ALL {
        let entry_message = || {
            format!(
                "synth.scoring.{key}.{} must be [cw, ccw] finite numbers",
                face_letter(face)
            )
        };
        let entry = map
            .get(&face_letter(face).to_string())
            .ok_or_else(entry_message)?;
        let pair = entry.as_array().ok_or_else(entry_message)?;
        if pair.len() != 2 {
            return Err(entry_message());
        }
        for (dir, half) in pair.iter().enumerate() {
            let parsed = half.as_f64().ok_or_else(entry_message)? as f32;
            if !parsed.is_finite() {
                return Err(entry_message());
            }
            costs[face.index()][dir] = parsed;
        }
    }
    if map.len() != Face::ALL.len() {
        return Err(lanes_message);
    }
    Ok(costs)
}

#[cfg(test)]
pub(crate) fn reference_config() -> serde_json::Value {
    serde_json::json!({
        "synth": {
            "scoring": {
                "preferred": ["U", "D", "R"],
                "preferred_edge": ["U", "D", "R", "M", "E", "S"],
                "penalty_other": 2.0,
                "weight_len": 1.0,
                "weight_subset": 2.0,
                "weight_regrip": 1.5,
                "weight_finger": 0.5,
                "finger_cost": {
                    "U": [0.2, 0.2], "D": [0.2, 0.2],
                    "L": [0.4, 0.4], "R": [0.4, 0.4],
                    "F": [0.8, 0.8], "B": [0.8, 0.8]
                }
            }
        }
    })
}

#[cfg(test)]
pub(crate) fn reference_scoring() -> ScoringCfg {
    scoring_from_config(&reference_config()).expect("reference scoring config parses")
}

pub fn scoring_from_config(config: &serde_json::Value) -> Result<ScoringCfg, String> {
    let Some(section) = config
        .pointer("/synth/scoring")
        .filter(|value| !value.is_null())
    else {
        return Err("config section missing: synth.scoring".to_string());
    };
    let field = |key: &str| -> Result<&serde_json::Value, String> {
        section
            .get(key)
            .filter(|value| !value.is_null())
            .ok_or_else(|| format!("config section missing: synth.scoring.{key}"))
    };
    Ok(ScoringCfg {
        preferred: parse_lanes(field("preferred")?, "preferred")?,
        preferred_edge: parse_lanes(field("preferred_edge")?, "preferred_edge")?,
        penalty_other: parse_number(field("penalty_other")?, "penalty_other", false)?,
        weight_len: parse_number(field("weight_len")?, "weight_len", true)?,
        weight_subset: parse_number(field("weight_subset")?, "weight_subset", true)?,
        weight_regrip: parse_number(field("weight_regrip")?, "weight_regrip", true)?,
        weight_finger: parse_number(field("weight_finger")?, "weight_finger", true)?,
        finger_cost: parse_finger_cost(field("finger_cost")?, "finger_cost")?,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use bt_core::Alg;
    use proptest::prelude::*;
    use serde_json::json;

    fn reference() -> ScoringCfg {
        scoring_from_config(&reference_config()).expect("reference scoring config parses")
    }

    fn scored(text: &str, orbit: Orbit) -> ScoreParts {
        let alg = Alg::parse(text).expect("alg parses");
        score(alg.moves(), orbit, &reference()).expect("sequence scores")
    }

    fn assert_close(actual: f32, expected: f32) {
        assert!(
            (actual - expected).abs() < 1e-6,
            "actual {actual} vs expected {expected}"
        );
    }

    #[test]
    fn right_hand_home_sequence_costs_its_length_and_finger_only() {
        let parts = scored("R U R'", Orbit::Corner);
        assert_eq!(parts.qtm, 3);
        assert_eq!(parts.regrips, 0);
        assert_close(parts.subset, 0.0);
        assert_close(parts.finger, 0.4 + 0.2 + 0.4);
        assert_close(parts.total, 1.0 * 3.0 + 0.5 * 1.0);
    }

    #[test]
    fn front_face_displaces_the_hand_and_costs_a_regrip() {
        let parts = scored("R F R'", Orbit::Corner);
        assert_eq!(parts.qtm, 3);
        assert_eq!(parts.regrips, 1);
        assert_close(parts.subset, 2.0);
        assert_close(parts.finger, 0.4 + 0.8 + 0.4);
        assert_close(parts.total, 1.0 * 3.0 + 2.0 * 2.0 + 1.5 * 1.0 + 0.5 * 1.6);
    }

    #[test]
    fn left_hand_dynamic_front_move_costs_a_regrip_on_return() {
        let parts = scored("L F L'", Orbit::Corner);
        assert_eq!(parts.regrips, 1);
        assert_close(parts.subset, 6.0);
        assert_close(parts.total, 3.0 + 12.0 + 1.5 + 0.8);
    }

    #[test]
    fn slice_turn_is_preferred_for_edges_and_plain_for_corners() {
        let edge = scored("M2", Orbit::Edge);
        assert_eq!(edge.qtm, 2);
        assert_eq!(edge.regrips, 0);
        assert_close(edge.subset, 0.0);
        assert_close(edge.finger, 0.4);
        assert_close(edge.total, 2.0 + 0.2);
        let corner = scored("M2", Orbit::Corner);
        assert_close(corner.subset, 2.0);
        assert_close(corner.total, 2.0 + 4.0 + 0.2);
    }

    #[test]
    fn alternating_home_hands_never_regrips() {
        let parts = scored("U L U' L'", Orbit::Corner);
        assert_eq!(parts.qtm, 4);
        assert_eq!(parts.regrips, 0);
        assert_close(parts.subset, 4.0);
        assert_close(parts.finger, 0.2 + 0.4 + 0.2 + 0.4);
        assert_close(parts.total, 4.0 + 8.0 + 0.5 * 1.2);
    }

    #[test]
    fn front_displacement_charges_the_right_hand_on_its_next_turn() {
        let parts = scored("F R U R'", Orbit::Corner);
        assert_eq!(parts.qtm, 4);
        assert_eq!(parts.regrips, 1);
        assert_close(parts.subset, 2.0);
        assert_close(parts.finger, 0.8 + 0.4 + 0.2 + 0.4);
        assert_close(parts.total, 4.0 + 4.0 + 1.5 + 0.5 * 1.8);
    }

    #[test]
    fn dynamic_hand_keeps_the_previous_hand_without_charging_it() {
        let parts = scored("L F R", Orbit::Corner);
        assert_eq!(parts.regrips, 0);
        assert_close(parts.subset, 4.0);
        assert_close(parts.finger, 0.4 + 0.8 + 0.4);
        assert_close(parts.total, 3.0 + 8.0 + 0.5 * 1.6);
    }

    #[test]
    fn consecutive_same_face_turns_are_scored_once_for_grip() {
        let parts = scored("R R'", Orbit::Corner);
        assert_eq!(parts.qtm, 2);
        assert_eq!(parts.regrips, 0);
        assert_close(parts.finger, 0.8);
        assert_close(parts.total, 2.0 + 0.4);
    }

    #[test]
    fn double_turns_count_two_qtm_and_charge_the_cw_finger_lane() {
        let parts = scored("U2", Orbit::Corner);
        assert_eq!(parts.qtm, 2);
        assert_close(parts.finger, 0.2);
        assert_close(parts.total, 2.0 + 0.1);
    }

    #[test]
    fn rotation_moves_reject_the_sequence() {
        let alg = Alg::parse("R x U'").expect("alg parses");
        assert!(score(alg.moves(), Orbit::Corner, &reference()).is_none());
        assert!(count_regrips(alg.moves()).is_none());
    }

    #[test]
    fn empty_sequence_scores_zero() {
        let parts = score(&[], Orbit::Edge, &reference()).expect("empty scores");
        assert_eq!(
            parts,
            ScoreParts {
                qtm: 0,
                subset: 0.0,
                regrips: 0,
                finger: 0.0,
                total: 0.0
            }
        );
    }

    #[test]
    fn parse_rejects_a_missing_scoring_section() {
        let err = scoring_from_config(&json!({"synth": {}})).unwrap_err();
        assert_eq!(err, "config section missing: synth.scoring");
    }

    #[test]
    fn parse_names_every_missing_field() {
        for key in [
            "preferred",
            "preferred_edge",
            "penalty_other",
            "weight_len",
            "weight_subset",
            "weight_regrip",
            "weight_finger",
            "finger_cost",
        ] {
            let mut config = reference_config();
            config
                .pointer_mut("/synth/scoring")
                .unwrap()
                .as_object_mut()
                .unwrap()
                .remove(key);
            let err = scoring_from_config(&config).unwrap_err();
            assert_eq!(
                err,
                format!("config section missing: synth.scoring.{key}"),
                "missing {key} must be named"
            );
        }
    }

    #[test]
    fn parse_rejects_unknown_lane_letters_and_empty_sets() {
        let mut bad_letter = reference_config();
        *bad_letter
            .pointer_mut("/synth/scoring/preferred")
            .unwrap()
            .as_array_mut()
            .unwrap()
            .get_mut(0)
            .unwrap() = json!("X");
        let err = scoring_from_config(&bad_letter).unwrap_err();
        assert_eq!(
            err,
            "synth.scoring.preferred entries must be single lanes from U D L R F B M E S"
        );

        let mut empty = reference_config();
        *empty.pointer_mut("/synth/scoring/preferred_edge").unwrap() = json!([]);
        let err = scoring_from_config(&empty).unwrap_err();
        assert_eq!(
            err,
            "synth.scoring.preferred_edge must list at least one lane"
        );
    }

    #[test]
    fn parse_rejects_negative_weights_and_nonfinite_numbers() {
        let mut negative = reference_config();
        *negative.pointer_mut("/synth/scoring/weight_len").unwrap() = json!(-0.5);
        let err = scoring_from_config(&negative).unwrap_err();
        assert_eq!(err, "synth.scoring.weight_len must be a finite number >= 0");

        let mut text = reference_config().to_string();
        let marker = "\"penalty_other\":2.0";
        assert!(text.contains(marker));
        text = text.replace(marker, "\"penalty_other\":1e300");
        let err = scoring_from_config(&serde_json::from_str(&text).unwrap()).unwrap_err();
        assert_eq!(err, "synth.scoring.penalty_other must be a finite number");
    }

    #[test]
    fn parse_rejects_finger_cost_typos_and_wrong_arities() {
        let mut extra = reference_config();
        extra
            .pointer_mut("/synth/scoring/finger_cost")
            .unwrap()
            .as_object_mut()
            .unwrap()
            .insert("N".to_string(), json!([0.1, 0.1]));
        let err = scoring_from_config(&extra).unwrap_err();
        assert_eq!(
            err,
            "synth.scoring.finger_cost must hold exactly the lanes U D L R F B"
        );

        let mut short = reference_config();
        *short.pointer_mut("/synth/scoring/finger_cost/R").unwrap() = json!([0.4]);
        let err = scoring_from_config(&short).unwrap_err();
        assert_eq!(
            err,
            "synth.scoring.finger_cost.R must be [cw, ccw] finite numbers"
        );

        let mut missing = reference_config();
        missing
            .pointer_mut("/synth/scoring/finger_cost")
            .unwrap()
            .as_object_mut()
            .unwrap()
            .remove("B");
        let err = scoring_from_config(&missing).unwrap_err();
        assert_eq!(
            err,
            "synth.scoring.finger_cost.B must be [cw, ccw] finite numbers"
        );
    }

    #[test]
    fn parse_maps_lanes_and_costs_into_the_model() {
        let cfg = reference();
        assert_eq!(
            cfg.preferred,
            [true, false, false, true, false, true, false, false, false]
        );
        assert_eq!(
            cfg.preferred_edge,
            [true, false, false, true, false, true, true, true, true]
        );
        assert_close(cfg.penalty_other, 2.0);
        assert_close(cfg.weight_len, 1.0);
        assert_close(cfg.weight_subset, 2.0);
        assert_close(cfg.weight_regrip, 1.5);
        assert_close(cfg.weight_finger, 0.5);
        assert_close(cfg.finger_cost[Face::F.index()][1], 0.8);
        assert_close(cfg.finger_cost[Face::D.index()][0], 0.2);
    }

    fn move_strategy() -> impl proptest::strategy::Strategy<Value = Vec<Move>> {
        (1..=24usize)
            .prop_flat_map(|len| proptest::collection::vec((0usize..9, 1u8..=3), len))
            .prop_map(|raw| {
                raw.into_iter()
                    .map(|(lane, quarter)| {
                        if lane < 6 {
                            Move::Face {
                                face: Face::ALL[lane],
                                quarter,
                            }
                        } else {
                            Move::Slice {
                                slice: [Slice::M, Slice::E, Slice::S][lane - 6],
                                quarter,
                            }
                        }
                    })
                    .collect()
            })
    }

    proptest! {
        #[test]
        fn score_is_deterministic_and_survives_the_format_round_trip(seq in move_strategy()) {
            let cfg = reference();
            for orbit in [Orbit::Edge, Orbit::Corner] {
                let once = score(&seq, orbit, &cfg);
                let twice = score(&seq, orbit, &cfg);
                prop_assert_eq!(once, twice);
                let alg = Alg::new(seq.clone());
                let reparsed = Alg::parse(&alg.format()).expect("formatted alg reparses");
                prop_assert_eq!(score(reparsed.moves(), orbit, &cfg), once);
            }
        }

        #[test]
        fn appending_a_penalized_move_strictly_raises_the_total(seq in move_strategy()) {
            let cfg = reference();
            for orbit in [Orbit::Edge, Orbit::Corner] {
                let before = score(&seq, orbit, &cfg).expect("sequence scores");
                let mut extended = seq.clone();
                extended.push(Move::Face { face: Face::F, quarter: 1 });
                let after = score(&extended, orbit, &cfg).expect("extended sequence scores");
                prop_assert!(after.total > before.total);
            }
        }

        #[test]
        fn rank_order_is_total(
            first in move_strategy(),
            second in move_strategy(),
            third in move_strategy()
        ) {
            let cfg = reference();
            for orbit in [Orbit::Edge, Orbit::Corner] {
                let ab = rank_cmp(&first, &second, orbit, &cfg);
                let ba = rank_cmp(&second, &first, orbit, &cfg);
                prop_assert_eq!(ab, ba.reverse());
                let bc = rank_cmp(&second, &third, orbit, &cfg);
                let ac = rank_cmp(&first, &third, orbit, &cfg);
                if ab != Ordering::Greater && bc != Ordering::Greater {
                    prop_assert_ne!(ac, Ordering::Greater);
                }
                let mut batch = [first.clone(), second.clone(), third.clone()];
                batch.sort_by(|left, right| rank_cmp(left, right, orbit, &cfg));
                for pair in batch.windows(2) {
                    prop_assert!(rank_cmp(&pair[0], &pair[1], orbit, &cfg) != Ordering::Greater);
                }
            }
        }
    }
}
