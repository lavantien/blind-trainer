use bt_core::Cube;
use bt_data::corpus::{Corpus, SheetClass};
use std::path::PathBuf;

fn flip_position(position: &str) -> Option<String> {
    let parts: Vec<&str> = position.split('-').collect();
    if parts.len() < 3 {
        return None;
    }
    let mut flipped = parts.clone();
    let n = flipped.len();
    flipped.swap(n - 2, n - 1);
    Some(flipped.join("-"))
}

fn main() {
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .and_then(|p| p.parent())
        .unwrap()
        .join("ref_ground");
    let corpus = Corpus::load(&root).expect("corpus");
    let mut covered = 0usize;
    let mut failures = Vec::new();
    let mut bad_flip_shape = Vec::new();
    for sheet in [SheetClass::Edge, SheetClass::Corner] {
        let mut positions: Vec<String> = corpus
            .rows()
            .iter()
            .filter(|r| r.sheet == sheet)
            .map(|r| r.position.clone())
            .collect();
        positions.sort();
        positions.dedup();
        for position in positions {
            let flipped = match flip_position(&position) {
                Some(f) => f,
                None => {
                    bad_flip_shape.push(position.clone());
                    continue;
                }
            };
            let here = corpus.rows_for_position(sheet, &position);
            let there = corpus.rows_for_position(sheet, &flipped);
            if here.is_empty() || there.is_empty() {
                continue;
            }
            covered += 1;
            let mut cube = Cube::solved();
            cube.apply_alg(&here[0].alg);
            cube.apply_alg(&there[0].alg);
            if !cube.is_solved() {
                failures.push(format!(
                    "{sheet:?} {position} -> {flipped}: {} then {}",
                    here[0].alg_text, there[0].alg_text
                ));
            }
        }
    }
    println!("positions with both sides covered: {covered}");
    println!("alternation failures: {}", failures.len());
    for f in &failures {
        println!("  FAIL {f}");
    }
    println!("positions without 3 dash parts: {}", bad_flip_shape.len());
    for p in &bad_flip_shape {
        println!("  ODD {p}");
    }
}
