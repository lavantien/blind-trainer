use crate::packs::{RowOutcome, SectionResult, Tier2Outcome};
use bt_data::seedpack::SeedRow;
use bt_puzzles::pyra::{PyraState, Pyraminx};
use bt_puzzles::{FaceletGrid, Geometry, verify_tier1};

const FACES: usize = 4;
const TIP_LOCALS: [usize; 3] = [0, 4, 8];
const EDGE_LOCALS: [usize; 3] = [1, 3, 6];

fn flat(grid: &FaceletGrid, face: usize, local: usize) -> u8 {
    let (row, col) = if local == 0 {
        (0, 0)
    } else if local < 4 {
        (1, local - 1)
    } else {
        (2, local - 4)
    };
    grid.faces[face].rows[row][col]
}

fn center_local_of_tip(tip: usize) -> usize {
    match tip {
        0 => 2,
        4 => 5,
        _ => 7,
    }
}

pub fn vertex_center_specs() -> Vec<[(usize, usize); 3]> {
    let engine = Pyraminx;
    let solved = engine.facelets(&engine.solved_state());
    let mut specs = Vec::new();
    for token in ["u", "l", "r", "b"] {
        let alg = engine.parse_alg(token).expect("tip token parses");
        let mut state = engine.solved_state();
        engine
            .apply_alg(&mut state, &alg)
            .expect("tip turn applies");
        let grid = engine.facelets(&state);
        let mut spec = [(0usize, 0usize); 3];
        let mut slot = 0;
        for face in 0..FACES {
            for tip in TIP_LOCALS {
                if flat(&grid, face, tip) != flat(&solved, face, tip) {
                    spec[slot] = (face, center_local_of_tip(tip));
                    slot += 1;
                }
            }
        }
        assert_eq!(slot, 3, "tip turn {token} moves three stickers");
        specs.push(spec);
    }
    specs
}

pub fn centers_home(engine: &Pyraminx, state: &PyraState) -> bool {
    let grid = engine.facelets(state);
    vertex_center_specs().iter().all(|spec| {
        let mut held: Vec<u8> = spec
            .iter()
            .map(|(face, local)| flat(&grid, *face, *local))
            .collect();
        held.sort_unstable();
        let mut faces: Vec<u8> = spec.iter().map(|(face, _)| *face as u8).collect();
        faces.sort_unstable();
        held == faces
    })
}

pub fn l4e_disturbance(engine: &Pyraminx, state: &PyraState) -> Result<usize, String> {
    let grid = engine.facelets(state);
    let solved = engine.facelets(&engine.solved_state());
    if !centers_home(engine, state) {
        return Err("an axial center left its vertex".to_string());
    }
    let mut moved = 0;
    for face in 0..FACES {
        for local in EDGE_LOCALS {
            if flat(&grid, face, local) != flat(&solved, face, local) {
                moved += 1;
            }
        }
    }
    if !(4..=8).contains(&moved) {
        return Err(format!(
            "{moved} edge facelets moved, a last four edges case holds 4 to 8"
        ));
    }
    Ok(moved)
}

fn case_state(engine: &Pyraminx, alg_text: &str) -> Result<PyraState, String> {
    let alg = engine.parse_alg(alg_text).map_err(|err| err.to_string())?;
    if alg.moves.is_empty() {
        return Err("alg is empty".to_string());
    }
    let inverse = engine.invert_alg(&alg);
    let mut state = engine.solved_state();
    engine
        .apply_alg(&mut state, &inverse)
        .map_err(|err| err.to_string())?;
    Ok(state)
}

pub fn verify_l4e(rows: &[SeedRow]) -> SectionResult {
    let engine = Pyraminx;
    let mut outcomes = Vec::with_capacity(rows.len());
    for row in rows {
        let tier1 = if row.alg.is_empty() {
            Err("alg is empty".to_string())
        } else {
            verify_tier1(&engine, &row.alg).map_err(|err| err.to_string())
        };
        let tier2 = match case_state(&engine, &row.alg) {
            Err(_) => Tier2Outcome::Skipped("tier 1 failed"),
            Ok(state) => match l4e_disturbance(&engine, &state) {
                Ok(_) => Tier2Outcome::Pass,
                Err(err) => Tier2Outcome::Fail(err),
            },
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
            "l4e tag prefixes (DS DF US UF NP) name source page sections, their per-row edge geometry is not derivable from the engine state, only the edges-and-tips disturbance is checked with corner facelets pinned"
                .to_string(),
        ],
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const CENTER_LOCALS: [usize; 3] = [2, 5, 7];

    #[test]
    fn the_solved_grid_renders_one_color_per_face() {
        let engine = Pyraminx;
        let grid = engine.facelets(&engine.solved_state());
        for face in 0..FACES {
            for local in 0..9 {
                assert_eq!(flat(&grid, face, local), face as u8, "{face}.{local}");
            }
        }
    }

    #[test]
    fn tip_turns_touch_only_tip_local_stickers() {
        let engine = Pyraminx;
        let solved = engine.facelets(&engine.solved_state());
        for token in ["u", "u'", "l", "l'", "r", "r'", "b", "b'"] {
            let alg = engine.parse_alg(token).expect("tip token parses");
            let mut state = engine.solved_state();
            engine
                .apply_alg(&mut state, &alg)
                .expect("tip turn applies");
            let grid = engine.facelets(&state);
            for face in 0..FACES {
                for local in 0..9 {
                    let moved = flat(&grid, face, local) != flat(&solved, face, local);
                    assert!(
                        !moved || TIP_LOCALS.contains(&local),
                        "{token} moved {face}.{local}"
                    );
                }
            }
        }
    }

    #[test]
    fn a_vertex_turn_moves_edges_and_corners_but_never_only_edges() {
        let engine = Pyraminx;
        let solved = engine.facelets(&engine.solved_state());
        let alg = engine.parse_alg("R").expect("vertex token parses");
        let mut state = engine.solved_state();
        engine.apply_alg(&mut state, &alg).expect("vertex applies");
        let grid = engine.facelets(&state);
        let mut edge_moved = false;
        let mut other_moved = false;
        for face in 0..FACES {
            for local in 0..9 {
                let moved = flat(&grid, face, local) != flat(&solved, face, local);
                if EDGE_LOCALS.contains(&local) {
                    edge_moved |= moved;
                } else {
                    other_moved |= moved;
                }
            }
        }
        assert!(edge_moved && other_moved);
    }

    #[test]
    fn solved_state_rejects_the_l4e_predicate() {
        let engine = Pyraminx;
        assert_eq!(
            l4e_disturbance(&engine, &engine.solved_state()),
            Err("0 edge facelets moved, a last four edges case holds 4 to 8".to_string())
        );
    }

    #[test]
    fn an_edge_case_alg_yields_an_edges_only_start_state() {
        let engine = Pyraminx;
        let state = case_state(&engine, "R U' R' U' R U' R'").expect("case state builds");
        assert!(l4e_disturbance(&engine, &state).is_ok());
    }

    #[test]
    fn a_tip_turn_case_state_is_rejected_as_outside_l4e() {
        let engine = Pyraminx;
        let state = case_state(&engine, "u").expect("case state builds");
        assert!(l4e_disturbance(&engine, &state).is_err());
    }

    #[test]
    fn a_full_scramble_case_state_exceeds_the_edge_bound() {
        let engine = Pyraminx;
        let state = case_state(&engine, "L R B U").expect("case state builds");
        assert!(l4e_disturbance(&engine, &state).is_err());
    }

    #[test]
    fn vertex_center_specs_pin_three_stickers_per_vertex() {
        let specs = vertex_center_specs();
        assert_eq!(specs.len(), 4);
        let mut positions = std::collections::BTreeSet::new();
        for spec in &specs {
            let faces: Vec<usize> = spec.iter().map(|(face, _)| *face).collect();
            assert_eq!(faces.len(), 3);
            for (face, local) in spec {
                assert!(CENTER_LOCALS.contains(local));
                positions.insert(face * 9 + local);
            }
        }
        assert_eq!(positions.len(), 12);
        let engine = Pyraminx;
        assert!(centers_home(&engine, &engine.solved_state()));
    }
}
