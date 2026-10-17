mod common;

use bt_core::{Alg, Cube, cancel, expand};
use bt_data::commutator::{decompose, decompose_verified};
use bt_data::corpus::Corpus;
use bt_data::seedpack::{Normalized, load_pack, load_registry, normalize_alg};
use common::ref_ground;
use std::path::Path;

fn state_of(alg: &Alg) -> Cube {
    let mut cube = Cube::solved();
    cube.apply_alg(alg);
    cube
}

fn state_of_flat(text: &str) -> Cube {
    state_of(&Alg::parse(text).expect("flat alg parses"))
}

fn assert_state_true(text: &str, expanded: &Alg, reference: &Alg) {
    assert_eq!(
        state_of(expanded),
        state_of(reference),
        "expansion of {text} must reach the reference state"
    );
    let twice = expand(&expanded.format()).expect("expanded form reparses");
    assert_eq!(twice, *expanded, "expansion of {text} must be idempotent");
}

fn corpus_rows() -> Corpus {
    Corpus::load(&ref_ground()).expect("corpus loads from ref_ground")
}

#[test]
fn every_corpus_row_expands_state_true_and_idempotently() {
    let corpus = corpus_rows();
    assert_eq!(corpus.total_rows(), 1077);
    for row in corpus.rows() {
        let expanded = expand(&row.alg_text)
            .unwrap_or_else(|| panic!("flat alg must expand: {}", row.alg_text));
        assert_state_true(&row.alg_text, &expanded, &row.alg);
    }
}

#[test]
fn every_corpus_commutator_expands_state_true_through_verification() {
    let corpus = corpus_rows();
    let mut decomposable = 0usize;
    let mut written_order = 0usize;
    for row in corpus.rows() {
        if decompose(&row.commutator).is_none() {
            continue;
        }
        decomposable += 1;
        let units = decompose_verified(&row.commutator, &row.alg)
            .unwrap_or_else(|| panic!("verified decomposition: {}", row.commutator));
        let mut moves = Vec::new();
        for unit in &units {
            moves.extend_from_slice(unit.expansion().moves());
        }
        let cancelled = cancel(&Alg::new(moves));
        assert_state_true(&row.commutator, &cancelled, &row.alg);
        if units.iter().all(|unit| !unit.b_first) {
            written_order += 1;
            let expanded = expand(&row.commutator)
                .unwrap_or_else(|| panic!("written order expands: {}", row.commutator));
            assert_state_true(&row.commutator, &expanded, &row.alg);
        }
    }
    assert_eq!(decomposable, 1009);
    assert_eq!(written_order, 1009);
}

fn bld_registry_entries() -> Vec<bt_data::seedpack::PackEntry> {
    load_registry(&ref_ground().join("packs"))
        .expect("registry loads")
        .into_iter()
        .filter(|entry| entry.event == "3bld")
        .collect()
}

#[test]
fn every_bld_seed_row_expands_state_true_and_idempotently() {
    let root = ref_ground().join("packs");
    let mut rows = 0usize;
    let mut skipped = 0usize;
    for entry in bld_registry_entries() {
        for row in load_pack(&root, &entry).expect("pack loads") {
            if row.alg.is_empty() || Alg::parse(&row.alg).is_err() {
                skipped += 1;
                continue;
            }
            let expanded =
                expand(&row.alg).unwrap_or_else(|| panic!("seed alg must expand: {}", row.alg));
            assert_state_true(&row.alg, &expanded, &Alg::parse(&row.alg).unwrap());
            rows += 1;
        }
    }
    assert!(rows > 0, "bld seedpacks must contribute rows");
    assert_eq!(skipped, 11, "prose rows and off-grammar algs stay lineless");
}

#[test]
fn every_bld_raw_pack_alg_expands_to_its_normalized_state() {
    let mut raws = 0usize;
    let mut skipped = 0usize;
    for entry in bld_registry_entries() {
        let text =
            std::fs::read_to_string(Path::new(&ref_ground()).join("packs").join(&entry.file))
                .expect("pack file reads");
        for line in text.lines().skip(1) {
            let line = line.trim_end_matches('\r');
            if line.is_empty() {
                continue;
            }
            let mut cells = line.split('\t');
            let raw = cells.nth(1).expect("alg cell present");
            let Ok(Normalized { alg, .. }) = normalize_alg(&entry.event, raw) else {
                continue;
            };
            if alg.is_empty() || Alg::parse(&alg).is_err() {
                skipped += 1;
                continue;
            }
            let expanded = expand(raw).unwrap_or_else(|| panic!("raw pack alg must expand: {raw}"));
            assert_state_true(raw, &expanded, &Alg::parse(&alg).unwrap());
            raws += 1;
        }
    }
    assert!(raws > 0, "raw bld pack algs must be exercised");
    assert_eq!(skipped, 11, "off-grammar raw algs match the stored set");
}
