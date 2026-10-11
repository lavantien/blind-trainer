mod common;

use bt_data::corpus::{Corpus, SheetClass, Solver};
use bt_gendata::build::build;
use common::ref_ground;

fn loaded() -> Corpus {
    Corpus::load(&ref_ground()).expect("corpus loads from ref_ground")
}

fn find<'a>(
    built: &'a bt_gendata::build::Built,
    orbit: &str,
    t1: u8,
    t2: u8,
) -> &'a bt_gendata::model::CaseEntry {
    built
        .cases
        .cases
        .iter()
        .find(|c| c.orbit == orbit && c.t1 == t1 && c.t2 == t2)
        .unwrap_or_else(|| panic!("case {orbit} {t1} {t2} missing"))
}

#[test]
fn case_space_counts_match_coverage_md() {
    let built = build(&loaded());
    assert_eq!(built.cases.cases.len(), 818);
    let edge: Vec<_> = built
        .cases
        .cases
        .iter()
        .filter(|c| c.orbit == "edge")
        .collect();
    let corner: Vec<_> = built
        .cases
        .cases
        .iter()
        .filter(|c| c.orbit == "corner")
        .collect();
    assert_eq!(edge.len(), 440);
    assert_eq!(corner.len(), 378);
    assert_eq!(edge.iter().filter(|c| c.covered).count(), 372);
    assert_eq!(edge.iter().filter(|c| !c.covered).count(), 68);
    assert_eq!(corner.iter().filter(|c| c.covered).count(), 288);
    assert_eq!(corner.iter().filter(|c| !c.covered).count(), 90);
}

#[test]
fn covered_flag_equals_vendor_algs_presence() {
    let built = build(&loaded());
    for case in &built.cases.cases {
        assert_eq!(case.covered, !case.vendor_algs.is_empty());
    }
}

#[test]
fn case_entries_are_unique_sorted_and_fully_labeled() {
    let built = build(&loaded());
    let mut keys: Vec<(u8, u8, u8)> = built
        .cases
        .cases
        .iter()
        .map(|c| match c.orbit.as_str() {
            "edge" => (0, c.t1, c.t2),
            _ => (1, c.t1, c.t2),
        })
        .collect();
    let mut sorted = keys.clone();
    sorted.sort_unstable();
    assert_eq!(keys, sorted);
    keys.dedup();
    assert_eq!(keys.len(), 818);
    for case in &built.cases.cases {
        let stickers = if case.orbit == "edge" { 24..48 } else { 0..24 };
        assert_eq!(case.t1_letter.to_ascii_uppercase(), case.t1_letter);
        assert_eq!(case.t1_letter, bt_scheme::letter_of(case.t1));
        assert_eq!(case.t2_letter, bt_scheme::letter_of(case.t2));
        assert_eq!(case.t1_name, bt_scheme::name_of(case.t1));
        assert_eq!(case.t2_name, bt_scheme::name_of(case.t2));
        assert!(stickers.contains(&case.t1));
        assert!(stickers.contains(&case.t2));
    }
}

#[test]
fn row_flow_matches_coverage_md() {
    let built = build(&loaded());
    assert_eq!(built.rows_in_pair_space, 796);
    assert_eq!(built.rows_other_buffer, 192);
    assert_eq!(built.rows_in_pair_space + built.rows_other_buffer, 988);
}

#[test]
fn known_case_join_carries_every_matching_row() {
    let built = build(&loaded());
    let case = find(&built, "edge", 24, 27);
    assert_eq!(case.t1_name, "UB");
    assert_eq!(case.t2_name, "UL");
    assert_eq!(case.t1_letter, 'A');
    assert_eq!(case.t2_letter, 'D');
    assert!(case.covered);
    assert_eq!(case.vendor_algs.len(), 3);
    assert_eq!(case.vendor_algs[0].solver, "cherry");
    assert_eq!(case.vendor_algs[0].rowid, 2);
    assert_eq!(case.vendor_algs[0].position, "UF-UB-UL");
    assert_eq!(case.vendor_algs[0].alg, "U M2 U M' U2 M U M2 U'");
    assert_eq!(case.vendor_algs[0].qtm, 12);
    assert_eq!(case.vendor_algs[0].htm, 9);
    assert!(case.vendor_algs[0].ok);
    assert_eq!(case.vendor_algs[1].solver, "cherry");
    assert_eq!(case.vendor_algs[1].rowid, 3);
    assert_eq!(case.vendor_algs[2].solver, "eggins");
    for alg in &case.vendor_algs {
        assert!(alg.ok, "{:?}", alg.problems);
        assert_eq!(alg.position, "UF-UB-UL");
    }
}

#[test]
fn corner_case_join_spans_solvers() {
    let built = build(&loaded());
    let case = built
        .cases
        .cases
        .iter()
        .find(|c| c.orbit == "corner" && c.t1_name == "BDR" && c.t2_name == "BUL")
        .expect("corner case T-R");
    assert!(case.covered);
    let solvers: Vec<&str> = case.vendor_algs.iter().map(|a| a.solver.as_str()).collect();
    assert!(solvers.contains(&"cherry"));
    assert!(solvers.contains(&"eggins"));
    for alg in &case.vendor_algs {
        assert_eq!(alg.position, "UFR-BDR-BUL");
    }
}

#[test]
fn specials_counts_match_coverage_md() {
    let built = build(&loaded());
    assert_eq!(built.specials.specials.len(), 89);
    let mut counts = std::collections::BTreeMap::new();
    for entry in &built.specials.specials {
        *counts
            .entry((entry.solver.as_str(), entry.class.as_str()))
            .or_insert(0) += 1;
    }
    assert_eq!(
        counts,
        [
            (("cherry", "flips"), 25),
            (("cherry", "twists"), 20),
            (("cherry", "parity"), 26),
            (("cherry", "ltct"), 8),
            (("eggins", "flips"), 0),
            (("eggins", "twists"), 5),
            (("eggins", "parity"), 2),
            (("eggins", "ltct"), 3),
        ]
        .into_iter()
        .filter(|&(_, n)| n > 0)
        .collect()
    );
    assert!(!counts.contains_key(&("eggins", "flips")));
}

#[test]
fn every_vendor_row_and_special_verifies_ok() {
    let built = build(&loaded());
    assert!(built.offenders.is_empty(), "{}", built.offenders.join("\n"));
    for case in &built.cases.cases {
        for alg in &case.vendor_algs {
            assert!(
                alg.ok,
                "{} {} {}",
                alg.solver,
                alg.rowid,
                alg.problems.join("; ")
            );
            assert!(alg.problems.is_empty());
        }
    }
    for entry in &built.specials.specials {
        assert!(
            entry.ok,
            "{} {} {}",
            entry.solver,
            entry.rowid,
            entry.problems.join("; ")
        );
    }
}

#[test]
fn audit_passes_on_the_real_corpus() {
    let corpus = loaded();
    let built = build(&corpus);
    let offenders = bt_gendata::audit::audit(&built, &corpus, 1077);
    assert!(offenders.is_empty(), "{}", offenders.join("\n"));
}

#[test]
fn audit_flags_count_and_verify_breaks() {
    let corpus = loaded();
    let mut rows: Vec<bt_data::corpus::Row> = corpus.rows().to_vec();
    let donor = corpus
        .row(Solver::Cherry, SheetClass::Edge, 4)
        .expect("donor row")
        .alg
        .clone();
    rows[0].alg = donor;
    let broken = Corpus::from_rows(rows);
    let built = build(&broken);
    assert!(!built.offenders.is_empty(), "swapped alg must be flagged");
    let offenders = bt_gendata::audit::audit(&built, &broken, 1077);
    assert!(offenders.iter().any(|o| o.contains("cherry/edge/2")));
    let mut truncated = corpus.rows().to_vec();
    truncated.pop();
    let short = Corpus::from_rows(truncated);
    let built = build(&short);
    let offenders = bt_gendata::audit::audit(&built, &short, 1076);
    assert!(offenders.iter().any(|o| o.contains("expected 1077")));
}
