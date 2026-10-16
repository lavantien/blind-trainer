use crate::packs::{RowOutcome, SectionResult, Tier2Outcome};
use bt_data::seedpack::SeedRow;
use bt_puzzles::Geometry;
use bt_puzzles::sq1::{SQUARE_SHAPE, Sq1, State, UNITS, canonical_shape};
use std::collections::{HashMap, HashSet};

#[derive(Debug)]
struct StartCase {
    state: State,
    classes: (Vec<u8>, Vec<u8>),
    front_json: String,
}

fn start_case(engine: &Sq1, alg_text: &str) -> Result<StartCase, String> {
    let alg = engine.parse_alg(alg_text).map_err(|err| err.to_string())?;
    if alg.is_empty() {
        return Err("alg is empty".to_string());
    }
    let inverse = engine.invert_alg(&alg);
    let mut state = engine.solved_state();
    engine
        .apply_alg(&mut state, &inverse)
        .map_err(|err| err.to_string())?;
    let start = state;
    let classes = (canonical_shape(&start, 0), canonical_shape(&start, 1));
    engine
        .apply_alg(&mut state, &alg)
        .map_err(|err| err.to_string())?;
    if !engine.is_solved(&state) {
        return Err("start state round trip misses solved".to_string());
    }
    Ok(StartCase {
        state: start,
        classes,
        front_json: String::new(),
    })
}

fn mirrored(widths: &[u8]) -> Vec<u8> {
    let mut reversed = widths.to_vec();
    reversed.reverse();
    let count = reversed.len();
    (0..count)
        .map(|rotation| {
            (0..count)
                .map(|unit| reversed[(rotation + unit) % count])
                .collect::<Vec<u8>>()
        })
        .min()
        .expect("widths started")
}

fn famkey(widths: &[u8]) -> Vec<u8> {
    let mirror = mirrored(widths);
    if mirror.as_slice() < widths {
        mirror
    } else {
        widths.to_vec()
    }
}

fn shape_names(front_json: &str) -> Result<[String; 2], String> {
    let value: serde_json::Value =
        serde_json::from_str(front_json).map_err(|err| err.to_string())?;
    let case = value
        .get("case")
        .and_then(|case| case.as_str())
        .ok_or_else(|| "front_json holds no case name".to_string())?;
    let (top, bottom) = case
        .split_once(" / ")
        .ok_or_else(|| format!("case name {case} does not name two layers"))?;
    Ok([top.to_string(), bottom.to_string()])
}

fn class_invariants(case: &StartCase) -> Result<(), String> {
    for (layer, class) in [&case.classes.0, &case.classes.1].iter().enumerate() {
        let sum: u8 = class.iter().sum();
        if sum != 12 {
            return Err(format!("layer {layer} ring sums to {sum}, not 12"));
        }
    }
    let corners = [&case.classes.0, &case.classes.1]
        .iter()
        .flat_map(|class| class.iter())
        .filter(|width| **width == 2)
        .count();
    if corners != 8 {
        return Err(format!("state holds {corners} corners, not 8"));
    }
    Ok(())
}

fn resolve_family_keys(cases: &[&StartCase]) -> (HashMap<String, Vec<u8>>, HashSet<String>) {
    let mut tally: HashMap<String, HashMap<Vec<u8>, usize>> = HashMap::new();
    for case in cases {
        if let Ok(names) = shape_names(&case.front_json) {
            tally_entry(&mut tally, &names[0], famkey(&case.classes.0));
            tally_entry(&mut tally, &names[1], famkey(&case.classes.1));
        }
    }
    let mut keys = HashMap::new();
    let mut ambiguous = HashSet::new();
    for (name, counts) in tally {
        let best = counts.values().copied().max().unwrap_or(0);
        let winners: Vec<(&Vec<u8>, &usize)> =
            counts.iter().filter(|(_, count)| **count == best).collect();
        if winners.len() != 1 {
            ambiguous.insert(name);
        } else {
            keys.insert(name, winners[0].0.clone());
        }
    }
    (keys, ambiguous)
}

fn tally_entry(tally: &mut HashMap<String, HashMap<Vec<u8>, usize>>, name: &str, key: Vec<u8>) {
    *tally
        .entry(name.to_string())
        .or_default()
        .entry(key)
        .or_insert(0) += 1;
}

pub fn verify_cubeshape(rows: &[SeedRow]) -> SectionResult {
    let engine = Sq1;
    let mut tracked: Vec<RowStart> = Vec::with_capacity(rows.len());
    for row in rows {
        let tier1 = start_case(&engine, &row.alg);
        let case = tier1.as_ref().ok().map(|case| StartCase {
            state: case.state,
            classes: case.classes.clone(),
            front_json: row.front_json.clone(),
        });
        tracked.push(RowStart {
            tier1: tier1.map(|_| ()),
            case,
        });
    }
    let observed: Vec<&StartCase> = tracked
        .iter()
        .filter_map(|start| start.case.as_ref())
        .collect();
    let (keys, ambiguous) = resolve_family_keys(&observed);
    let mut outcomes = Vec::with_capacity(rows.len());
    for (row, start) in rows.iter().zip(tracked) {
        let tier2 = match &start.case {
            None => Tier2Outcome::Skipped("tier 1 failed"),
            Some(case) => cubeshape_tier2(case, &keys, &ambiguous),
        };
        outcomes.push(RowOutcome {
            case_key: row.case_key.clone(),
            tier1: start.tier1,
            tier2,
        });
    }
    SectionResult {
        rows: outcomes,
        honesty: Vec::new(),
    }
}

struct RowStart {
    tier1: Result<(), String>,
    case: Option<StartCase>,
}

fn cubeshape_tier2(
    case: &StartCase,
    keys: &HashMap<String, Vec<u8>>,
    ambiguous: &HashSet<String>,
) -> Tier2Outcome {
    if let Err(err) = class_invariants(case) {
        return Tier2Outcome::Fail(err);
    }
    let names = match shape_names(&case.front_json) {
        Ok(names) => names,
        Err(err) => return Tier2Outcome::Fail(err),
    };
    if names[0] == names[1] && case.classes.0 != case.classes.1 {
        return Tier2Outcome::Fail(format!(
            "symmetric case {} holds two different layer classes",
            names[0]
        ));
    }
    for name in &names {
        if ambiguous.contains(name) {
            return Tier2Outcome::Fail(format!(
                "family key for {name} is ambiguous across the pack"
            ));
        }
    }
    match (keys.get(&names[0]), keys.get(&names[1])) {
        (Some(top), Some(bottom)) => {
            let observed = (famkey(&case.classes.0), famkey(&case.classes.1));
            if observed.0 == *top && observed.1 == *bottom {
                Tier2Outcome::Pass
            } else {
                Tier2Outcome::Fail(format!(
                    "names crossed: {} / {} declares keys {:?} / {:?}, state holds {:?} / {:?}",
                    names[0], names[1], top, bottom, observed.0, observed.1
                ))
            }
        }
        (None, _) | (_, None) => Tier2Outcome::Fail(format!(
            "shape names {} / {} hold no family keys",
            names[0], names[1]
        )),
    }
}

#[derive(Clone, Copy)]
enum Stage {
    Co,
    Eo,
    Cp,
    Ep,
}

fn ring_spans(ring: &[u8]) -> Vec<(u8, u8)> {
    let n = ring.len();
    let mut begin = 0;
    while begin < n && ring[begin] == ring[(begin + n - 1) % n] {
        begin += 1;
    }
    if begin == n {
        return vec![(ring[0], n as u8)];
    }
    let mut spans = Vec::new();
    let mut index = begin;
    while index < begin + n {
        let id = ring[index % n];
        let mut width = 0;
        while ring[(index + width) % n] == id {
            width += 1;
        }
        spans.push((id, width as u8));
        index += width;
    }
    spans
}

const TOP_CORNERS: [u8; 4] = [0, 2, 4, 6];
const BOTTOM_CORNERS: [u8; 4] = [9, 11, 13, 15];
const TOP_EDGES: [u8; 4] = [1, 3, 5, 7];
const BOTTOM_EDGES: [u8; 4] = [8, 10, 12, 14];

fn layer_holds(ring: &[u8], corners: &[u8; 4], edges: &[u8; 4]) -> (bool, bool) {
    let mut corners_ok = true;
    let mut edges_ok = true;
    for (id, width) in ring_spans(ring) {
        if width == 2 {
            corners_ok &= corners.contains(&id);
        } else {
            edges_ok &= edges.contains(&id);
        }
    }
    (corners_ok, edges_ok)
}

fn cyclic_rotation(sequence: &[u8], pattern: &[u8]) -> bool {
    if sequence.len() != pattern.len() {
        return false;
    }
    let count = pattern.len();
    (0..count)
        .any(|shift| (0..count).all(|index| sequence[index] == pattern[(index + shift) % count]))
}

fn corners_ordered(state: &State) -> bool {
    let top: Vec<u8> = ring_spans(&state.units[0..UNITS])
        .iter()
        .filter(|(_, width)| *width == 2)
        .map(|(id, _)| *id)
        .collect();
    let bottom: Vec<u8> = ring_spans(&state.units[UNITS..2 * UNITS])
        .iter()
        .filter(|(_, width)| *width == 2)
        .map(|(id, _)| *id)
        .collect();
    cyclic_rotation(&top, &TOP_CORNERS) && cyclic_rotation(&bottom, &BOTTOM_CORNERS)
}

fn stage_predicates(state: &State) -> (bool, bool, bool) {
    let (top_corners, top_edges) = layer_holds(&state.units[0..UNITS], &TOP_CORNERS, &TOP_EDGES);
    let (bottom_corners, bottom_edges) = layer_holds(
        &state.units[UNITS..2 * UNITS],
        &BOTTOM_CORNERS,
        &BOTTOM_EDGES,
    );
    let corners_placed = (0..4)
        .all(|k| state.units[3 * k] == TOP_CORNERS[k] && state.units[3 * k + 1] == TOP_CORNERS[k])
        && (0..4).all(|k| {
            let corner = BOTTOM_CORNERS[k];
            state.units[UNITS + 3 * k + 1] == corner && state.units[UNITS + 3 * k + 2] == corner
        });
    (
        top_corners && bottom_corners,
        top_edges && bottom_edges,
        corners_placed,
    )
}

fn stage_outcome(engine: &Sq1, stage: Stage, case: &StartCase) -> Tier2Outcome {
    if case.classes.0 != SQUARE_SHAPE.to_vec() || case.classes.1 != SQUARE_SHAPE.to_vec() {
        return Tier2Outcome::Fail("start state is not square on both layers".to_string());
    }
    let (corners_oriented, edges_oriented, corners_placed) = stage_predicates(&case.state);
    match stage {
        Stage::Co if !corners_oriented => Tier2Outcome::Pass,
        Stage::Co => Tier2Outcome::Fail("corners already oriented".to_string()),
        Stage::Eo if !corners_oriented => Tier2Outcome::Fail("corners not oriented".to_string()),
        Stage::Eo if !edges_oriented => Tier2Outcome::Pass,
        Stage::Eo => Tier2Outcome::Fail("edges already oriented".to_string()),
        Stage::Cp if !corners_oriented => Tier2Outcome::Fail("corners not oriented".to_string()),
        Stage::Cp if !edges_oriented => Tier2Outcome::Fail("edges not oriented".to_string()),
        Stage::Cp if !corners_placed => Tier2Outcome::Pass,
        Stage::Cp => Tier2Outcome::Fail("corners already placed".to_string()),
        Stage::Ep if !corners_oriented => Tier2Outcome::Fail("corners not oriented".to_string()),
        Stage::Ep if !corners_ordered(&case.state) => {
            Tier2Outcome::Fail("corners not in solved order".to_string())
        }
        Stage::Ep if engine.is_solved(&case.state) => {
            Tier2Outcome::Fail("start state is already solved".to_string())
        }
        Stage::Ep => Tier2Outcome::Pass,
    }
}

fn verify_stage(rows: &[SeedRow], stage: Stage) -> SectionResult {
    let engine = Sq1;
    let mut outcomes = Vec::with_capacity(rows.len());
    for row in rows {
        let tier1 = start_case(&engine, &row.alg);
        let tier2 = match &tier1 {
            Err(_) => Tier2Outcome::Skipped("tier 1 failed"),
            Ok(case) => stage_outcome(&engine, stage, case),
        };
        outcomes.push(RowOutcome {
            case_key: row.case_key.clone(),
            tier1: tier1.map(|_| ()),
            tier2,
        });
    }
    SectionResult {
        rows: outcomes,
        honesty: Vec::new(),
    }
}

pub fn verify_co(rows: &[SeedRow]) -> SectionResult {
    verify_stage(rows, Stage::Co)
}

pub fn verify_eo(rows: &[SeedRow]) -> SectionResult {
    verify_stage(rows, Stage::Eo)
}

pub fn verify_cp(rows: &[SeedRow]) -> SectionResult {
    verify_stage(rows, Stage::Cp)
}

pub fn verify_ep(rows: &[SeedRow]) -> SectionResult {
    verify_stage(rows, Stage::Ep)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn case_of(alg: &str) -> StartCase {
        start_case(&Sq1, alg).expect("start case builds")
    }

    fn spans_of(ring: &[u8]) -> Vec<(u8, u8)> {
        ring_spans(ring)
    }

    #[test]
    fn solved_rings_split_into_eight_spans() {
        let top = [0, 0, 1, 2, 2, 3, 4, 4, 5, 6, 6, 7];
        let bottom = [8, 9, 9, 10, 11, 11, 12, 13, 13, 14, 15, 15];
        assert_eq!(
            spans_of(&top),
            vec![
                (0, 2),
                (1, 1),
                (2, 2),
                (3, 1),
                (4, 2),
                (5, 1),
                (6, 2),
                (7, 1)
            ]
        );
        assert_eq!(
            spans_of(&bottom),
            vec![
                (8, 1),
                (9, 2),
                (10, 1),
                (11, 2),
                (12, 1),
                (13, 2),
                (14, 1),
                (15, 2)
            ]
        );
    }

    #[test]
    fn spans_follow_a_run_wrapped_over_the_ring_seam() {
        let wrapped = [0, 1, 2, 2, 3, 4, 4, 5, 6, 6, 7, 0];
        assert_eq!(
            spans_of(&wrapped),
            vec![
                (1, 1),
                (2, 2),
                (3, 1),
                (4, 2),
                (5, 1),
                (6, 2),
                (7, 1),
                (0, 2)
            ]
        );
    }

    #[test]
    fn famkey_picks_the_lexicographically_smaller_mirror() {
        assert_eq!(mirrored(&[1, 2, 3]), vec![1, 3, 2]);
        assert_eq!(famkey(&[1, 2, 3]), vec![1, 2, 3]);
        assert_eq!(famkey(&[1, 3, 2]), vec![1, 2, 3]);
        assert_eq!(famkey(&SQUARE_SHAPE), SQUARE_SHAPE.to_vec());
    }

    #[test]
    fn empty_algs_fail_start_case() {
        assert_eq!(
            start_case(&Sq1, "").unwrap_err(),
            "alg is empty".to_string()
        );
    }

    #[test]
    fn the_start_state_round_trip_holds_for_a_pack_alg() {
        let case = case_of("(1,0) / (-1,0)");
        assert_eq!(case.classes.0, SQUARE_SHAPE.to_vec());
        assert_eq!(case.classes.1, SQUARE_SHAPE.to_vec());
    }

    #[test]
    fn a_slash_start_state_is_kite_kite_not_square() {
        let case = case_of("/");
        assert_eq!(case.classes.0, vec![1, 1, 2, 1, 2, 2, 1, 2]);
        assert_ne!(case.classes.0, SQUARE_SHAPE.to_vec());
    }

    #[test]
    fn solved_state_passes_every_stronger_predicate() {
        let engine = Sq1;
        let state = engine.solved_state();
        assert_eq!(stage_predicates(&state), (true, true, true));
    }

    #[test]
    fn slash_swaps_corners_across_layers_so_orientation_fails() {
        let engine = Sq1;
        let mut state = engine.solved_state();
        engine
            .apply_alg(&mut state, &engine.parse_alg("/").unwrap())
            .unwrap();
        let (corners, _, _) = stage_predicates(&state);
        assert!(!corners);
    }

    #[test]
    fn stage_predicates_partition_post_cubeshape_states() {
        let engine = Sq1;
        let co_case = case_of("(1,0) / (-1,0)");
        let (corners, _, _) = stage_predicates(&co_case.state);
        assert!(!corners);
        let solved = engine.solved_state();
        assert_eq!(stage_predicates(&solved), (true, true, true));
    }

    #[test]
    fn stage_outcome_reports_the_strongest_failure_reason() {
        let engine = Sq1;
        let mut case = case_of("/");
        case.classes = (vec![1, 1, 2, 1, 2, 2, 1, 2], vec![1, 1, 2, 1, 2, 2, 1, 2]);
        assert!(matches!(
            stage_outcome(&engine, Stage::Co, &case),
            Tier2Outcome::Fail(_)
        ));
    }

    #[test]
    fn shape_names_split_the_declared_pair() {
        let names = shape_names("{\"case\":\"Kite / Shield\"}").expect("names parse");
        assert_eq!(names[0], "Kite");
        assert_eq!(names[1], "Shield");
        assert!(shape_names("{\"case\":\"Kite\"}").is_err());
        assert!(shape_names("{\"note\":\"x\"}").is_err());
    }

    #[test]
    fn class_invariants_hold_on_the_solved_shapes() {
        let engine = Sq1;
        let mut state = engine.solved_state();
        engine
            .apply_alg(&mut state, &engine.parse_alg("/ 3,3 /").unwrap())
            .unwrap();
        let case = StartCase {
            state,
            classes: (canonical_shape(&state, 0), canonical_shape(&state, 1)),
            front_json: String::new(),
        };
        assert_eq!(class_invariants(&case), Ok(()));
        let mut broken = case;
        broken.classes.0[0] += 1;
        assert!(class_invariants(&broken).is_err());
    }
}
