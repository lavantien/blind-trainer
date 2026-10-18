use bt_data::seedpack::{NSpec, PackEntry, load_pack};
use bt_puzzles::pyra::{PyraState, Pyraminx};
use bt_puzzles::{FaceletGrid, Geometry};

const FACES: usize = 4;
const TIP_LOCALS: [usize; 3] = [0, 4, 8];
const EDGE_LOCALS: [usize; 3] = [1, 3, 6];
const CENTER_LOCALS: [usize; 3] = [2, 5, 7];

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

fn moved(engine: &Pyraminx, state: &PyraState, locals: [usize; 3]) -> usize {
    let grid = engine.facelets(state);
    let solved = engine.facelets(&engine.solved_state());
    let mut count = 0;
    for face in 0..FACES {
        for local in locals {
            if flat(&grid, face, local) != flat(&solved, face, local) {
                count += 1;
            }
        }
    }
    count
}

fn after(engine: &Pyraminx, state: &PyraState, word: &str) -> PyraState {
    let mut next = state.clone();
    if word.is_empty() {
        return next;
    }
    let alg = engine.parse_alg(word).expect("word parses");
    engine.apply_alg(&mut next, &alg).expect("word applies");
    next
}

fn case_state(engine: &Pyraminx, alg_text: &str) -> Option<PyraState> {
    let alg = engine.parse_alg(alg_text).ok()?;
    let mut state = engine.solved_state();
    engine
        .apply_alg(&mut state, &engine.invert_alg(&alg))
        .ok()?;
    Some(state)
}

fn main() {
    let engine = Pyraminx;
    let words = [
        "", "u", "u'", "l", "l'", "r", "r'", "b", "b'", "U", "U'", "U U", "L", "L'", "L L", "R",
        "R'", "R R", "B", "B'", "B B",
    ];
    let entry = PackEntry {
        file: "pyraminx_l4e.tsv".to_string(),
        event: "pyraminx".to_string(),
        method: "l4e".to_string(),
        stage: "l4e".to_string(),
        n: NSpec::Unset,
        expected_rows: 35,
    };
    let rows = load_pack("../../ref_ground/packs".as_ref(), &entry).expect("pack loads");
    for row in &rows {
        let Some(state) = case_state(&engine, &row.alg) else {
            println!("{}: parse or apply fail", row.case_key);
            continue;
        };
        let corners = moved(&engine, &state, TIP_LOCALS);
        let centers = moved(&engine, &state, CENTER_LOCALS);
        let edges = moved(&engine, &state, EDGE_LOCALS);
        let frames: Vec<&str> = words
            .iter()
            .filter(|word| {
                let next = after(&engine, &state, word);
                moved(&engine, &next, TIP_LOCALS) == 0 && moved(&engine, &next, CENTER_LOCALS) == 0
            })
            .copied()
            .collect();
        let framed_edges = frames
            .first()
            .map(|word| moved(&engine, &after(&engine, &state, word), EDGE_LOCALS));
        println!(
            "{}: corners={corners} centers={centers} edges={edges} frames={:?} framed_edges={:?}",
            row.case_key, frames, framed_edges
        );
    }
}
