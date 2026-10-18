use bt_puzzles::nxn::Nxn;
use bt_puzzles::{FaceletGrid, Geometry};

fn d_layer_solved(engine: &Nxn, state: &bt_puzzles::nxn::NxnState) -> bool {
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

fn main() {
    let engine = Nxn::new(2).expect("2x2");
    for text in ["x", "y", "R U R'"] {
        let alg = engine.parse_alg(text).expect("alg parses");
        let inverse = engine.invert_alg(&alg);
        let mut state = engine.solved_state();
        engine.apply_alg(&mut state, &inverse).expect("apply");
        let solved = engine.is_solved(&state);
        println!(
            "case {text}: is_solved={solved} d_layer_solved={}",
            d_layer_solved(&engine, &state)
        );
    }
}
