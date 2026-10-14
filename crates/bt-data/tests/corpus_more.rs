use bt_data::corpus::{Corpus, LoadError, OverlayError, SheetClass, Solver};
use bt_data::paths::UserAlgs;
use bt_data::tsv::parse_sheet;
use bt_data::verify::verify;
use std::path::PathBuf;

const HEADER: &str = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link";
const X2: &str = "R' D' R D R' D' R D";
const TPERM: &str = "R U R' U' R' F R2 U' R' U' R U R' F'";
const EDGE_ALG: &str = "M2 U M U2 M' U M2";
const CORNER_ALG: &str = "R U R' D' R U' R' D";
const FLIP_ALG: &str = "M' U M' U M' U2 M U M U M U2";

fn twist_alg() -> String {
    format!("{X2} U {X2} {X2} U'")
}

fn ltct_alg() -> String {
    format!("{TPERM} {X2} U {X2} U {X2} U2")
}

fn line(position: &str, alg: &str) -> String {
    format!("{position}\t{alg}\tNot found.\t\tnote\n")
}

fn sheet_text(rows: &[String]) -> String {
    let mut text = String::from(HEADER);
    for row in rows {
        text.push('\n');
        text.push_str(row);
    }
    text
}

fn mini_root(label: &str) -> PathBuf {
    let root = std::env::temp_dir().join(format!("bt-corpus-more-{label}-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&root);
    for solver in ["cherry", "eggins"] {
        std::fs::create_dir_all(root.join(solver)).unwrap();
        for sheet in SheetClass::ALL {
            std::fs::write(
                root.join(solver).join(format!("{}.tsv", sheet.file())),
                sheet_text(&[]),
            )
            .unwrap();
        }
    }
    let cherry = [
        (
            SheetClass::Edge,
            vec![line("UF-UR-UL", EDGE_ALG), line("UF-UR-UL", EDGE_ALG)],
        ),
        (SheetClass::Corner, vec![line("UFR-RDF-BDR", CORNER_ALG)]),
        (SheetClass::Flips, vec![line("UF & UB", FLIP_ALG)]),
        (SheetClass::Twists, vec![line("UFR & UBR", &twist_alg())]),
        (SheetClass::Parity, vec![line("UR-UL, UFR-UBR", TPERM)]),
        (
            SheetClass::Ltct,
            vec![line("UR-UL, UFR-RUB[LUB]", &ltct_alg())],
        ),
    ];
    for (sheet, rows) in cherry {
        std::fs::write(
            root.join("cherry").join(format!("{}.tsv", sheet.file())),
            sheet_text(&rows),
        )
        .unwrap();
    }
    std::fs::write(
        root.join("eggins").join("edge.tsv"),
        sheet_text(&[line("UF-UB-UL", "U M2 U M' U2 M U M2 U'")]),
    )
    .unwrap();
    root
}

#[test]
fn mini_corpus_loads_and_every_row_verifies() {
    let root = mini_root("load");
    let corpus = Corpus::load(&root).unwrap();
    assert_eq!(corpus.total_rows(), 8);
    assert!(!corpus.is_empty());
    for row in corpus.rows() {
        let verdict = verify(&row.alg, &row.position, row.sheet);
        assert!(
            verdict.ok(),
            "{}/{}/{}: {verdict}",
            row.solver,
            row.sheet,
            row.rowid
        );
    }
    let first = &corpus.rows()[0];
    assert_eq!(first.alg.qtm(), 10);
    assert_eq!(first.alg.htm(), 7);
    assert_eq!(first.alg_text, EDGE_ALG);
    assert_eq!(first.recon_link, "");
    assert_eq!(first.youtube_link, "note");
    assert_eq!(first.commutator, "Not found.");
    std::fs::remove_dir_all(&root).unwrap();
}

#[test]
fn mini_corpus_indexing_and_queries() {
    let root = mini_root("index");
    let corpus = Corpus::load(&root).unwrap();
    assert!(corpus.row(Solver::Cherry, SheetClass::Edge, 2).is_some());
    assert!(corpus.row(Solver::Cherry, SheetClass::Edge, 3).is_none());
    assert!(corpus.row(Solver::User, SheetClass::Edge, 1).is_none());
    assert_eq!(corpus.rows()[1].position, "UF-UR-UL");
    assert_eq!(corpus.sheet_rows(Solver::Cherry, SheetClass::Edge).len(), 2);
    assert_eq!(
        corpus.sheet_rows(Solver::Eggins, SheetClass::Corner).len(),
        0
    );
    let group = corpus.rows_for_position(SheetClass::Edge, "UF-UR-UL");
    assert_eq!(group.len(), 2);
    assert!(group.iter().all(|row| row.solver == Solver::Cherry));
    assert_eq!(corpus.rows_for_position(SheetClass::Ltct, "nope").len(), 0);
    assert_eq!(
        corpus.distinct_positions(Solver::Cherry, SheetClass::Edge),
        1
    );
    assert_eq!(
        corpus.distinct_positions(Solver::Eggins, SheetClass::Edge),
        1
    );
    assert_eq!(
        corpus
            .rows_for_position(SheetClass::Ltct, "UR-UL, UFR-RUB[LUB]")
            .len(),
        1
    );
    std::fs::remove_dir_all(&root).unwrap();
}

#[test]
fn missing_sheet_file_reports_the_path() {
    let root = mini_root("missing");
    let missing = root.join("eggins").join("twists.tsv");
    std::fs::remove_file(&missing).unwrap();
    match Corpus::load(&root) {
        Err(LoadError::Read { file, .. }) => assert_eq!(file, missing),
        other => panic!("{other:?}"),
    }
    let text = Corpus::load(&root).unwrap_err().to_string();
    assert!(text.contains("twists.tsv"), "{text}");
    assert!(text.starts_with("read "), "{text}");
    std::fs::remove_dir_all(&root).unwrap();
}

#[test]
fn corrupt_sheet_reports_parse_error_with_line() {
    let root = mini_root("parse");
    std::fs::write(root.join("eggins").join("parity.tsv"), "junk\n").unwrap();
    match Corpus::load(&root) {
        Err(LoadError::Parse { file, source }) => {
            assert!(file.ends_with("parity.tsv"));
            assert_eq!(
                source.to_string(),
                "line 1: header must be Position/Alg/Commutator/Reconstruction Link/Youtube Link"
            );
        }
        other => panic!("{other:?}"),
    }
    std::fs::remove_dir_all(&root).unwrap();
}

#[test]
fn unparsable_alg_reports_file_and_rowid() {
    let root = mini_root("badalg");
    std::fs::write(
        root.join("eggins").join("flips.tsv"),
        sheet_text(&[line("UF & UB", "Q9")]),
    )
    .unwrap();
    match Corpus::load(&root) {
        Err(LoadError::Alg { file, rowid, .. }) => {
            assert!(file.ends_with("flips.tsv"));
            assert_eq!(rowid, 2);
        }
        other => panic!("{other:?}"),
    }
    let text = Corpus::load(&root).unwrap_err().to_string();
    assert!(text.contains("rowid 2"), "{text}");
    std::fs::remove_dir_all(&root).unwrap();
}

#[test]
fn overlay_rows_join_the_mini_corpus() {
    let root = mini_root("overlay");
    let corpus = Corpus::load(&root).unwrap();
    let untouched = corpus.with_overlay(&UserAlgs::new()).unwrap();
    assert_eq!(untouched.total_rows(), corpus.total_rows());
    let mut overlay = UserAlgs::new();
    overlay.set(SheetClass::Edge, "UF-UR-UL", EDGE_ALG);
    overlay.set(SheetClass::Flips, "UF & UB", FLIP_ALG);
    let merged = corpus.with_overlay(&overlay).unwrap();
    assert_eq!(merged.total_rows(), corpus.total_rows() + 2);
    let group = merged.rows_for_position(SheetClass::Edge, "UF-UR-UL");
    assert_eq!(group.len(), 3);
    assert_eq!(group[2].solver, Solver::User);
    assert_eq!(group[2].rowid, 1);
    assert_eq!(group[2].commutator, "");
    assert_eq!(group[2].recon_link, "");
    assert_eq!(group[2].youtube_link, "");
    let verdict = verify(&group[2].alg, &group[2].position, SheetClass::Edge);
    assert!(verdict.ok(), "{verdict}");
    std::fs::remove_dir_all(&root).unwrap();
}

#[test]
fn overlay_rejects_unknown_sheet_keys_and_bad_algs() {
    let root = mini_root("badoverlay");
    let corpus = Corpus::load(&root).unwrap();
    let dir = root.join("user");
    std::fs::create_dir_all(dir.join("algs")).unwrap();
    std::fs::write(
        dir.join("algs").join("user-algs.json"),
        r#"{"entries":{"bogus/UF-UR-UL":{"alg":"R U R'"},"noslash":{"alg":"R U R'"}}}"#,
    )
    .unwrap();
    let overlay = UserAlgs::load_at(&dir).unwrap();
    match corpus.with_overlay(&overlay) {
        Err(OverlayError::UnknownSheet { key }) => assert_eq!(key, "bogus/UF-UR-UL"),
        other => panic!("{other:?}"),
    }
    let mut bad_alg = UserAlgs::new();
    bad_alg.set(SheetClass::Corner, "UFR-RDF-BDR", "Q9");
    match corpus.with_overlay(&bad_alg) {
        Err(OverlayError::BadAlg { position, .. }) => assert_eq!(position, "UFR-RDF-BDR"),
        other => panic!("{other:?}"),
    }
    let text = corpus.with_overlay(&bad_alg).unwrap_err().to_string();
    assert!(text.contains("bad alg for position UFR-RDF-BDR"), "{text}");
    std::fs::remove_dir_all(&root).unwrap();
}

#[test]
fn solver_and_sheet_parsing_round_trip() {
    for solver in [Solver::Cherry, Solver::Eggins, Solver::User] {
        assert_eq!(Solver::parse(&solver.to_string()), Some(solver));
    }
    assert_eq!(Solver::VENDOR, [Solver::Cherry, Solver::Eggins]);
    assert!(Solver::parse("Cherry").is_none());
    assert!(Solver::parse("").is_none());
    assert_eq!(SheetClass::ALL.len(), 6);
    for sheet in SheetClass::ALL {
        assert_eq!(SheetClass::parse(sheet.file()), Some(sheet));
        assert_eq!(sheet.to_string(), sheet.file());
    }
    assert!(SheetClass::parse("edges").is_none());
    assert!(SheetClass::parse("").is_none());
}

#[test]
fn mini_sheets_parse_with_expected_counts() {
    let root = mini_root("tsv");
    let expected = [
        ("cherry", SheetClass::Edge, 2),
        ("cherry", SheetClass::Corner, 1),
        ("cherry", SheetClass::Flips, 1),
        ("cherry", SheetClass::Twists, 1),
        ("cherry", SheetClass::Parity, 1),
        ("cherry", SheetClass::Ltct, 1),
        ("eggins", SheetClass::Edge, 1),
        ("eggins", SheetClass::Corner, 0),
        ("eggins", SheetClass::Flips, 0),
        ("eggins", SheetClass::Twists, 0),
        ("eggins", SheetClass::Parity, 0),
        ("eggins", SheetClass::Ltct, 0),
    ];
    for (solver, sheet, count) in expected {
        let text = std::fs::read_to_string(root.join(solver).join(format!("{}.tsv", sheet.file())))
            .unwrap();
        let rows = parse_sheet(&text).unwrap();
        assert_eq!(rows.len(), count, "{solver}/{}", sheet.file());
    }
    std::fs::remove_dir_all(&root).unwrap();
}
