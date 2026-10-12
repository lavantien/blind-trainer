mod common;

use bt_core::Cube;
use bt_data::corpus::{Corpus, SheetClass};
use common::ref_ground;

fn loaded() -> Corpus {
    Corpus::load(&ref_ground()).expect("corpus loads from ref_ground")
}

fn flip_position(position: &str) -> String {
    let mut parts: Vec<&str> = position.split('-').collect();
    let n = parts.len();
    parts.swap(n - 2, n - 1);
    parts.join("-")
}

#[test]
fn first_row_alg_then_flipped_first_row_alg_returns_to_solved() {
    let corpus = loaded();
    let mut covered = 0usize;
    for sheet in [SheetClass::Edge, SheetClass::Corner] {
        let mut positions: Vec<String> = corpus
            .rows()
            .iter()
            .filter(|row| row.sheet == sheet)
            .map(|row| row.position.clone())
            .collect();
        positions.sort();
        positions.dedup();
        for position in positions {
            let flipped = flip_position(&position);
            let here = corpus.rows_for_position(sheet, &position);
            let there = corpus.rows_for_position(sheet, &flipped);
            if here.is_empty() || there.is_empty() {
                continue;
            }
            covered += 1;
            let mut cube = Cube::solved();
            cube.apply_alg(&here[0].alg);
            cube.apply_alg(&there[0].alg);
            assert!(
                cube.is_solved(),
                "{sheet} {position} then {flipped}: {} then {}",
                here[0].alg_text,
                there[0].alg_text
            );
        }
    }
    assert_eq!(covered, 584);
}
