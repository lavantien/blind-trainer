use bt_data::seedpack::{NSpec, PackEntry, load_pack};
use bt_puzzles::pyra::{PyraState, Pyraminx};
use bt_puzzles::skewb::Skewb;
use bt_puzzles::{FaceletGrid, Geometry};

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

fn vertex_center_specs() -> Vec<[(usize, usize); 3]> {
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

fn centers_home(engine: &Pyraminx, state: &PyraState) -> bool {
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

fn l4e_disturbance(engine: &Pyraminx, state: &PyraState) -> Result<usize, String> {
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
        return Err(format!("{moved} edge facelets moved"));
    }
    Ok(moved)
}

fn case_state(engine: &Pyraminx, alg_text: &str) -> PyraState {
    let alg = engine.parse_alg(alg_text).expect("alg parses");
    let inverse = engine.invert_alg(&alg);
    let mut state = engine.solved_state();
    engine.apply_alg(&mut state, &inverse).expect("apply");
    state
}

fn corner_facelets_moved(engine: &Pyraminx, state: &PyraState) -> usize {
    let grid = engine.facelets(state);
    let solved = engine.facelets(&engine.solved_state());
    let mut moved = 0;
    for face in 0..FACES {
        for tip in TIP_LOCALS {
            if flat(&grid, face, tip) != flat(&solved, face, tip) {
                moved += 1;
            }
        }
    }
    moved
}

fn main() {
    let pyra = Pyraminx;
    for text in ["U", "U'", "R", "L'", "U U'"] {
        let state = case_state(&pyra, text);
        let corners = corner_facelets_moved(&pyra, &state);
        println!(
            "pyra case {text}: l4e = {:?}, corner facelets moved = {corners}",
            l4e_disturbance(&pyra, &state)
        );
    }

    let skewb = Skewb::new();
    for text in [
        "y2 L F' L' F",
        "R' F R F' y2 R' F R F' y2 x' R' F R F' y2 R' F R F'",
        "L F' L'",
    ] {
        println!(
            "skewb app-side parse {text:?}: {:?}",
            skewb.parse_alg(text).err()
        );
    }

    let entry = PackEntry {
        file: "pyraminx_l4e.tsv".to_string(),
        event: "pyraminx".to_string(),
        method: "l4e".to_string(),
        stage: "l4e".to_string(),
        n: NSpec::Unset,
        expected_rows: 35,
    };
    eprintln!("loading pack");
    let rows = load_pack("../../ref_ground/packs".as_ref(), &entry).expect("pack loads");
    eprintln!("pack loaded, {} rows", rows.len());
    for row in rows {
        eprintln!("row {} alg {:?}", row.case_key, row.alg);
        if row.alg.is_empty() {
            continue;
        }
        let Ok(alg) = pyra.parse_alg(&row.alg) else {
            println!("{}: tier1 parse fail: {}", row.case_key, row.alg);
            continue;
        };
        let mut state = pyra.solved_state();
        let inverse = pyra.invert_alg(&alg);
        if pyra.apply_alg(&mut state, &inverse).is_err() {
            println!("{}: tier1 apply fail", row.case_key);
            continue;
        }
        let corners = corner_facelets_moved(&pyra, &state);
        let verdict = l4e_disturbance(&pyra, &state);
        println!(
            "{}: tier2 = {:?}, corner facelets moved = {}",
            row.case_key, verdict, corners
        );
    }
}
