mod common;

use bt_data::corpus::Corpus;
use bt_gendata::build::{build, manifest};
use bt_gendata::hash::{digest_corpus, sha256_hex};
use common::ref_ground;

fn loaded() -> Corpus {
    Corpus::load(&ref_ground()).expect("corpus loads from ref_ground")
}

fn stamped(built: &bt_gendata::build::Built) -> bt_gendata::model::Manifest {
    let cases = serde_json::to_vec_pretty(&built.cases).unwrap();
    let specials = serde_json::to_vec_pretty(&built.specials).unwrap();
    let digest = digest_corpus(&ref_ground()).expect("corpus digest");
    manifest(
        built,
        &digest,
        &sha256_hex(&cases),
        &sha256_hex(&specials),
        "2026-10-12T00:00:00Z".to_string(),
    )
}

#[test]
fn manifest_reports_the_coverage_md_numbers() {
    let record = stamped(&build(&loaded()));
    assert_eq!(record.generated_at, "2026-10-12T00:00:00Z");
    assert_eq!(record.generator, "bt-gendata");
    assert_eq!(record.corpus_rows, 1077);
    assert_eq!(record.corpus_files.len(), 12);
    assert_eq!(record.rows_in_pair_space, 796);
    assert_eq!(record.rows_other_buffer, 192);
    assert_eq!(record.cases.path, "cases.json");
    assert_eq!(record.cases.entries, 818);
    assert_eq!(record.cases.edge.total, 440);
    assert_eq!(record.cases.edge.covered, 372);
    assert_eq!(record.cases.edge.uncovered, 68);
    assert_eq!(record.cases.corner.total, 378);
    assert_eq!(record.cases.corner.covered, 288);
    assert_eq!(record.cases.corner.uncovered, 90);
    assert_eq!(record.specials.path, "specials.json");
    assert_eq!(record.specials.entries, 89);
    assert_eq!(
        record
            .specials
            .counts
            .iter()
            .map(|c| (c.solver.as_str(), c.class.as_str(), c.rows))
            .collect::<Vec<_>>(),
        vec![
            ("cherry", "flips", 25),
            ("cherry", "twists", 20),
            ("cherry", "parity", 26),
            ("cherry", "ltct", 8),
            ("eggins", "flips", 0),
            ("eggins", "twists", 5),
            ("eggins", "parity", 2),
            ("eggins", "ltct", 3),
        ]
    );
}

#[test]
fn manifest_output_hashes_match_the_serialized_files() {
    let built = build(&loaded());
    let cases = serde_json::to_vec_pretty(&built.cases).unwrap();
    let specials = serde_json::to_vec_pretty(&built.specials).unwrap();
    let record = stamped(&built);
    assert_eq!(record.cases.sha256, sha256_hex(&cases));
    assert_eq!(record.specials.sha256, sha256_hex(&specials));
    assert_eq!(record.cases.sha256.len(), 64);
    assert_ne!(record.cases.sha256, record.specials.sha256);
}

#[test]
fn output_files_round_trip_through_serde() {
    let built = build(&loaded());
    let cases_text = serde_json::to_string_pretty(&built.cases).unwrap();
    let back: bt_gendata::model::CasesFile = serde_json::from_str(&cases_text).unwrap();
    assert_eq!(back, built.cases);
    let specials_text = serde_json::to_string_pretty(&built.specials).unwrap();
    let back: bt_gendata::model::SpecialsFile = serde_json::from_str(&specials_text).unwrap();
    assert_eq!(back, built.specials);
    assert!(cases_text.starts_with("{\n  \"cases\": ["));
    assert!(specials_text.starts_with("{\n  \"specials\": ["));
}

#[test]
fn specials_carry_classes_links_and_solver_grouping() {
    let built = build(&loaded());
    let mut solver_rank = 0;
    for entry in &built.specials.specials {
        let rank = match entry.solver.as_str() {
            "cherry" => 0,
            "eggins" => 1,
            other => panic!("unknown solver {other}"),
        };
        assert!(rank >= solver_rank, "{} out of order", entry.solver);
        solver_rank = rank;
        assert!(["flips", "twists", "parity", "ltct"].contains(&entry.class.as_str()));
        assert!(entry.rowid >= 2);
    }
    let with_links = built
        .specials
        .specials
        .iter()
        .filter(|entry| !entry.recon_link.is_empty())
        .count();
    assert!(with_links > 0);
}
