mod common;

use bt_data::corpus::{Corpus, SheetClass, Solver};
use bt_verify::load_simcheck;
use bt_verify::report::{audit, build_report, render_markdown};
use common::ref_ground;

fn corpus() -> Corpus {
    Corpus::load(&ref_ground()).expect("corpus loads from ref_ground")
}

fn sim() -> bt_verify::SimMap {
    load_simcheck(&ref_ground()).expect("simcheck loads from ref_ground")
}

fn stamp() -> String {
    "2026-10-12T00:00:00Z".to_string()
}

#[test]
fn report_passes_every_row_against_simcheck() {
    let report = build_report(&corpus(), &sim(), stamp(), "0".repeat(64));
    assert_eq!(report.rows.len(), 1077);
    assert_eq!(report.corpus_rows, 1077);
    assert_eq!(report.simcheck_total, 1077);
    assert_eq!(report.simcheck_matched, 1077);
    assert!(
        report.failures.is_empty(),
        "{}",
        report
            .failures
            .iter()
            .map(|f| format!(
                "{}/{}/{}: {}",
                f.solver,
                f.sheet,
                f.rowid,
                f.reasons().join("; ")
            ))
            .collect::<Vec<_>>()
            .join("\n")
    );
    for row in &report.rows {
        assert!(row.pass);
        assert!(row.verify_ok);
        assert!(row.qtm_match);
        assert!(row.position_match);
        assert_eq!(row.simcheck_qtm, Some(row.qtm));
        assert_eq!(
            row.simcheck_position.as_deref(),
            Some(row.position.as_str())
        );
    }
    let offenders = audit(&report, 1077);
    assert!(offenders.is_empty(), "{}", offenders.join("\n"));
}

#[test]
fn per_sheet_counts_match_coverage_md() {
    let report = build_report(&corpus(), &sim(), stamp(), "0".repeat(64));
    assert_eq!(report.sheets.len(), 12);
    for &(solver, sheet, expected) in bt_verify::SHEET_EXPECTED_ROWS.iter() {
        let summary = report
            .sheets
            .iter()
            .find(|s| s.solver == solver && s.sheet == sheet)
            .unwrap_or_else(|| panic!("{solver}/{sheet} summary missing"));
        assert_eq!(summary.rows, expected, "{solver}/{sheet}");
        assert_eq!(summary.passed, expected, "{solver}/{sheet}");
        assert_eq!(summary.failed, 0, "{solver}/{sheet}");
        assert_eq!(summary.qtm_agreed, expected, "{solver}/{sheet}");
        assert_eq!(summary.position_agreed, expected, "{solver}/{sheet}");
    }
    assert_eq!(report.sheets.iter().map(|s| s.rows).sum::<usize>(), 1077);
}

#[test]
fn corrupted_alg_is_flagged_with_named_problems() {
    let base = corpus();
    let mut rows: Vec<bt_data::corpus::Row> = base.rows().to_vec();
    let donor = base
        .row(Solver::Cherry, SheetClass::Edge, 4)
        .expect("donor row")
        .alg
        .clone();
    rows[0].alg = donor;
    let broken = Corpus::from_rows(rows);
    let report = build_report(&broken, &sim(), stamp(), "0".repeat(64));
    assert_eq!(report.failures.len(), 1);
    let failure = &report.failures[0];
    assert_eq!(failure.rowid, 2);
    assert_eq!(failure.solver, "cherry");
    assert_eq!(failure.sheet, "edge");
    assert!(!failure.pass);
    assert!(!failure.verify_ok);
    assert!(failure.qtm_match);
    assert!(
        failure.problems.iter().any(|p| p.contains("should sit on")),
        "{:?}",
        failure.problems
    );
    let offenders = audit(&report, 1077);
    assert!(offenders.iter().any(|o| o.contains("cherry/edge/2")));
}

#[test]
fn qtm_mismatch_fails_only_that_row() {
    let mut records = sim();
    let entry = records
        .get_mut(&(Solver::Cherry, SheetClass::Edge, 2))
        .expect("simcheck record");
    entry.qtm += 1;
    let report = build_report(&corpus(), &records, stamp(), "0".repeat(64));
    assert_eq!(report.failures.len(), 1);
    let failure = &report.failures[0];
    assert!(failure.verify_ok);
    assert!(!failure.qtm_match);
    assert!(failure.position_match);
    assert!(failure.reasons().iter().any(|r| r.contains("vs simcheck")));
    let offenders = audit(&report, 1077);
    assert!(offenders.iter().any(|o| o.contains("cherry/edge/2")));
    let summary = report
        .sheets
        .iter()
        .find(|s| s.solver == "cherry" && s.sheet == "edge")
        .unwrap();
    assert_eq!(summary.qtm_agreed, 550);
}

#[test]
fn position_mismatch_fails_only_that_row() {
    let mut records = sim();
    let entry = records
        .get_mut(&(Solver::Eggins, SheetClass::Ltct, 2))
        .expect("simcheck record");
    entry.position.push('X');
    let report = build_report(&corpus(), &records, stamp(), "0".repeat(64));
    assert_eq!(report.failures.len(), 1);
    assert!(report.failures[0].verify_ok);
    assert!(report.failures[0].qtm_match);
    assert!(!report.failures[0].position_match);
}

#[test]
fn missing_and_leftover_simcheck_rows_are_flagged() {
    let mut records = sim();
    records.remove(&(Solver::Cherry, SheetClass::Edge, 2));
    let report = build_report(&corpus(), &records, stamp(), "0".repeat(64));
    assert_eq!(report.failures.len(), 1);
    assert!(
        report.failures[0]
            .problems
            .iter()
            .any(|p| p.contains("missing simcheck record"))
    );
    assert_eq!(report.simcheck_matched, 1076);
    let offenders = audit(&report, 1076);
    assert!(
        offenders
            .iter()
            .any(|o| o.contains("simcheck matched 1076"))
    );
    records.insert(
        (Solver::Cherry, SheetClass::Edge, 99999),
        bt_verify::SimRow {
            position: "UF-UB-UL".to_string(),
            qtm: 12,
        },
    );
    let report = build_report(&corpus(), &records, stamp(), "0".repeat(64));
    let offenders = audit(&report, 1077);
    assert!(
        offenders
            .iter()
            .any(|o| o.contains("simcheck holds 1077 rows"))
    );
}

#[test]
fn sheet_count_drift_is_flagged() {
    let base = corpus();
    let mut rows = base.rows().to_vec();
    rows.retain(|row| !(row.solver == Solver::Eggins && row.sheet == SheetClass::Ltct));
    let short = Corpus::from_rows(rows);
    let report = build_report(&short, &sim(), stamp(), "0".repeat(64));
    let offenders = audit(&report, 1077);
    assert!(
        offenders
            .iter()
            .any(|o| o.contains("corpus holds 1074 rows"))
    );
    assert!(
        offenders
            .iter()
            .any(|o| o.contains("eggins/ltct holds 0 rows"))
    );
}

#[test]
fn markdown_renders_the_summary_and_every_sheet() {
    let report = build_report(&corpus(), &sim(), stamp(), "a".repeat(64));
    let md = render_markdown(&report);
    assert!(md.contains("# alg verification report"));
    assert!(md.contains("generator: bt-verify"));
    assert!(md.contains("generated_at: 2026-10-12T00:00:00Z"));
    assert!(md.contains(&format!("corpus sha256: {}", "a".repeat(64))));
    assert!(md.contains("| cherry | edge | 551 | 551 | 0 | 551 | 551 |"));
    assert!(md.contains("| eggins | ltct | 3 | 3 | 0 | 3 | 3 |"));
    assert!(md.contains("## failures"));
    assert!(md.contains("none"));
    assert!(md.contains("1077/1077 rows pass"));
}

#[test]
fn report_is_deterministic_apart_from_the_stamp() {
    let first = build_report(
        &corpus(),
        &sim(),
        "2026-01-01T00:00:00Z".to_string(),
        "b".repeat(64),
    );
    let second = build_report(
        &corpus(),
        &sim(),
        "2026-12-31T23:59:59Z".to_string(),
        "b".repeat(64),
    );
    let bytes_a = serde_json::to_vec_pretty(&first).unwrap();
    let bytes_b = serde_json::to_vec_pretty(&second).unwrap();
    assert_ne!(bytes_a, bytes_b);
    let mut patched = second.clone();
    patched.generated_at = first.generated_at.clone();
    assert_eq!(first, patched);
    let mut md_a = render_markdown(&first);
    let mut md_b = render_markdown(&second);
    md_a = md_a.replace("2026-01-01T00:00:00Z", "");
    md_b = md_b.replace("2026-12-31T23:59:59Z", "");
    assert_eq!(md_a, md_b);
}
