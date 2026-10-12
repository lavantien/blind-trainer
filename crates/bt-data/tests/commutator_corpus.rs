mod common;

use bt_data::commutator::{decompose, decompose_verified};
use bt_data::corpus::{Corpus, SheetClass, Solver};
use common::ref_ground;

fn loaded() -> Corpus {
    Corpus::load(&ref_ground()).expect("corpus loads from ref_ground")
}

const PINNED_PARSE_HITS: usize = 1009;
const PINNED_VERIFIED_HITS: usize = 1009;

#[test]
fn aggregate_coverage_is_pinned() {
    let corpus = loaded();
    let mut parsed = 0usize;
    let mut verified = 0usize;
    let mut not_found = 0usize;
    for row in corpus.rows() {
        if row.commutator.trim() == "Not found." {
            not_found += 1;
            continue;
        }
        if decompose(&row.commutator).is_some() {
            parsed += 1;
        }
        if decompose_verified(&row.commutator, &row.alg).is_some() {
            verified += 1;
        }
    }
    assert_eq!(not_found, 68);
    assert_eq!(parsed, PINNED_PARSE_HITS);
    assert_eq!(verified, PINNED_VERIFIED_HITS);
}

#[test]
fn combo_and_nested_rows_parse_and_verify() {
    let corpus = loaded();
    let mut combos = 0usize;
    let mut nested = 0usize;
    for row in corpus.rows() {
        let commutator = row.commutator.trim();
        if commutator == "Not found." {
            continue;
        }
        if commutator.contains('+') {
            combos += 1;
            assert!(decompose(commutator).is_some(), "{commutator}");
            assert!(
                decompose_verified(commutator, &row.alg).is_some(),
                "{commutator} against {}",
                row.alg_text
            );
        }
        if commutator.matches('[').count() > 1 {
            nested += 1;
        }
    }
    assert_eq!(combos, 19);
    assert_eq!(nested, 19);
}

#[test]
fn named_nested_shapes_parse_and_verify() {
    let corpus = loaded();
    let targets = ["U:[[R' E' R,U2]+U2:[S,R2]]", "S R':[[E,R2]+[F2,R S' R']]"];
    for target in targets {
        let rows: Vec<&bt_data::corpus::Row> = corpus
            .rows()
            .iter()
            .filter(|row| row.commutator.trim() == target)
            .collect();
        assert!(!rows.is_empty(), "{target} missing from corpus");
        for row in rows {
            assert!(decompose(target).is_some());
            assert!(
                decompose_verified(target, &row.alg).is_some(),
                "{target} against {}",
                row.alg_text
            );
        }
    }
    let row = corpus
        .row(Solver::Cherry, SheetClass::Edge, 4)
        .expect("cherry edge rowid 4");
    assert_eq!(row.commutator.trim(), "R2 U':[S,R2]");
    assert!(decompose_verified(&row.commutator, &row.alg).is_some());
}
