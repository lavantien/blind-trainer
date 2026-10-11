mod common;

use bt_data::corpus::{Corpus, SheetClass, Solver};
use common::{SHEETS, ref_ground, simcheck_records};

fn loaded() -> Corpus {
    Corpus::load(&ref_ground()).expect("corpus loads from ref_ground")
}

#[test]
fn total_row_count_is_1077() {
    let corpus = loaded();
    assert_eq!(corpus.rows().len(), 1077);
    assert_eq!(corpus.total_rows(), 1077);
    assert!(!corpus.is_empty());
}

#[test]
fn per_solver_and_sheet_counts_match_the_dumps() {
    let corpus = loaded();
    for (solver_dir, file, expected) in SHEETS {
        let solver = Solver::parse(solver_dir).expect("solver name");
        let sheet = SheetClass::parse(file.trim_end_matches(".tsv")).expect("sheet name");
        let rows = corpus.sheet_rows(solver, sheet);
        assert_eq!(rows.len(), expected, "{solver_dir}/{file}");
    }
}

#[test]
fn distinct_position_counts_match_coverage_report() {
    let corpus = loaded();
    assert_eq!(
        corpus.distinct_positions(Solver::Cherry, SheetClass::Edge),
        506
    );
    assert_eq!(
        corpus.distinct_positions(Solver::Cherry, SheetClass::Corner),
        318
    );
    assert_eq!(
        corpus.distinct_positions(Solver::Eggins, SheetClass::Edge),
        71
    );
    assert_eq!(
        corpus.distinct_positions(Solver::Eggins, SheetClass::Corner),
        38
    );
}

#[test]
fn multi_alg_positions_stay_grouped_in_rowid_order() {
    let corpus = loaded();
    let group = corpus.rows_for_position(SheetClass::Edge, "UF-UB-UL");
    assert_eq!(group.len(), 3);
    let cherry: Vec<&bt_data::corpus::Row> = group
        .iter()
        .copied()
        .filter(|r| r.solver == Solver::Cherry)
        .collect();
    assert_eq!(cherry.len(), 2);
    assert_eq!(cherry[0].rowid, 2);
    assert_eq!(cherry[1].rowid, 3);
    assert_eq!(cherry[0].alg_text, "U M2 U M' U2 M U M2 U'");
    assert_eq!(cherry[1].alg_text, "U2 R2 U' S' U2 S U' R2 U2");
    assert_eq!(group[2].solver, Solver::Eggins);
}

#[test]
fn sheet_class_and_solver_round_trip_names() {
    assert_eq!(SheetClass::ALL.len(), 6);
    for sheet in SheetClass::ALL {
        assert_eq!(SheetClass::parse(sheet.file()), Some(sheet));
    }
    assert_eq!(SheetClass::parse("nonsense"), None);
    for solver in Solver::VENDOR {
        assert_eq!(Solver::parse(solver.dir()), Some(solver));
    }
    assert_eq!(Solver::parse("nonsense"), None);
    assert_eq!(Solver::User.dir(), "user");
    assert_eq!(Solver::parse("user"), Some(Solver::User));
}

#[test]
fn rowid_lookup_hits_vendor_rows_and_misses_others() {
    let corpus = loaded();
    let row = corpus
        .row(Solver::Cherry, SheetClass::Edge, 2)
        .expect("rowid 2 exists");
    assert_eq!(row.position, "UF-UB-UL");
    assert_eq!(row.qtm(), 12);
    assert_eq!(row.htm(), 9);
    assert!(
        corpus
            .row(Solver::Cherry, SheetClass::Edge, 99999)
            .is_none()
    );
    assert!(corpus.row(Solver::User, SheetClass::Edge, 2).is_none());
}

#[test]
fn position_lookup_spans_solvers_deterministically() {
    let corpus = loaded();
    let group = corpus.rows_for_position(SheetClass::Corner, "UFR-BDR-BUL");
    assert_eq!(group.len(), 2);
    assert_eq!(group[0].solver, Solver::Cherry);
    assert_eq!(group[1].solver, Solver::Eggins);
    assert!(group.iter().all(|r| r.position == "UFR-BDR-BUL"));
    assert!(
        corpus
            .rows_for_position(SheetClass::Ltct, "NOPE")
            .is_empty()
    );
}

#[test]
fn qtm_and_position_agree_with_simcheck_for_all_rows() {
    let corpus = loaded();
    let records = simcheck_records();
    assert_eq!(records.len(), 1077);
    for record in &records {
        let solver = Solver::parse(&record[0]).expect("solver");
        let sheet = SheetClass::parse(&record[1]).expect("sheet");
        let rowid: usize = record[2].parse().expect("rowid");
        let row = corpus
            .row(solver, sheet, rowid)
            .unwrap_or_else(|| panic!("missing row {solver:?}/{sheet:?}/{rowid}"));
        assert_eq!(row.position, record[3], "{solver:?}/{sheet:?}/{rowid}");
        let expected_qtm: u32 = record[5].parse().expect("qtm");
        assert_eq!(
            row.qtm(),
            expected_qtm,
            "{solver:?}/{sheet:?}/{rowid} {}",
            row.alg_text
        );
    }
}

#[test]
fn rows_keep_link_columns_from_the_dump() {
    let corpus = loaded();
    let row = corpus
        .row(Solver::Cherry, SheetClass::Edge, 5)
        .expect("row");
    assert_eq!(row.position, "UF-UB-LB");
    assert_eq!(row.alg_text, "U' R E' R' U2 R E R' U'");
    assert_eq!(row.commutator, "U':[R E' R',U2]");
    assert_eq!(row.recon_link, "https://bit.ly/2Ymdwpw");
    assert_eq!(
        row.youtube_link,
        "3BLD: 11.89 Single, 14.52 Mo3 (With Commentary)"
    );
}

#[test]
fn missing_root_fails_with_file_context() {
    let missing = ref_ground().join("does-not-exist");
    let err = Corpus::load(&missing).unwrap_err();
    let text = err.to_string();
    assert!(text.contains("edge.tsv"), "{text}");
    assert!(
        text.contains("does-not-exist") || text.contains("cannot"),
        "{text}"
    );
}

#[test]
fn unparsable_alg_fails_with_sheet_and_rowid() {
    let dir =
        std::env::temp_dir().join(format!("bt-data-corpus-{}-{}", std::process::id(), line!()));
    let sheet_dir = dir.join("cherry");
    std::fs::create_dir_all(&sheet_dir).unwrap();
    std::fs::write(
        sheet_dir.join("edge.tsv"),
        "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\nA-B-C\tQ9\tNot found.\t\t\n",
    )
    .unwrap();
    let err = Corpus::load(&dir).unwrap_err();
    assert!(err.to_string().contains("rowid 2"), "{}", err);
    std::fs::remove_dir_all(&dir).ok();
}
