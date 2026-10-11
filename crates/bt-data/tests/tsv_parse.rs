mod common;

use bt_data::tsv::{SheetRow, TsvError, parse_sheet};
use common::{SHEETS, sheet_text};

#[test]
fn all_twelve_sheets_parse_with_expected_row_counts() {
    for (solver_dir, file, expected) in SHEETS {
        let rows = parse_sheet(&sheet_text(solver_dir, file)).unwrap();
        assert_eq!(rows.len(), expected, "{solver_dir}/{file}");
    }
}

#[test]
fn rowids_are_physical_line_numbers_starting_at_two() {
    let rows = parse_sheet(&sheet_text("cherry", "edge.tsv")).unwrap();
    for (offset, (rowid, _)) in rows.iter().enumerate() {
        assert_eq!(*rowid, offset + 2);
    }
    let empty = parse_sheet(&sheet_text("eggins", "flips.tsv")).unwrap();
    assert!(empty.is_empty());
}

#[test]
fn cherry_edge_first_row_columns_extract_verbatim() {
    let rows = parse_sheet(&sheet_text("cherry", "edge.tsv")).unwrap();
    let (_, row) = rows[0].clone();
    assert_eq!(row.position, "UF-UB-UL");
    assert_eq!(row.alg, "U M2 U M' U2 M U M2 U'");
    assert_eq!(row.commutator, "U M2 U:[M',U2]");
    assert_eq!(row.recon_link, "https://bit.ly/2ZWpEPq");
    assert_eq!(row.youtube_link, "3BLD: 16.04 Former WB Ao12");
}

#[test]
fn special_class_headers_and_positions_extract() {
    let twists = parse_sheet(&sheet_text("cherry", "twists.tsv")).unwrap();
    assert_eq!(twists[0].1.position, "UFR & UBR");
    assert_eq!(twists[0].1.alg, "R' D' R D R' D' R U R' D R D' R' D R U'");
    let parity = parse_sheet(&sheet_text("cherry", "parity.tsv")).unwrap();
    assert_eq!(parity[0].1.position, "UF-UL, UFR-UFL");
    let ltct = parse_sheet(&sheet_text("cherry", "ltct.tsv")).unwrap();
    assert_eq!(ltct[0].1.position, "UF-UR, UBR-LUB[FUR]");
    assert_eq!(ltct[0].1.commutator, "Not found.");
}

#[test]
fn crlf_input_parses_identically() {
    let lf = sheet_text("cherry", "flips.tsv");
    let crlf = lf.replace('\n', "\r\n");
    let a = parse_sheet(&lf).unwrap();
    let b = parse_sheet(&crlf).unwrap();
    assert_eq!(a.len(), b.len());
    for ((ra, rowa), (rb, rowb)) in a.iter().zip(b.iter()) {
        assert_eq!(ra, rb);
        assert_eq!(rowa, rowb);
    }
}

#[test]
fn header_padding_and_trailing_empty_columns_are_tolerated() {
    let text = "Position          \tAlg         \tCommutator\tReconstruction Link\tYoutube Link\t\t\n\
                A-B-C\tU R U'\tNot found.\thttps://x\tnote\t\n";
    let rows = parse_sheet(text).unwrap();
    assert_eq!(rows.len(), 1);
    assert_eq!(rows[0].1.position, "A-B-C");
    assert_eq!(rows[0].1.alg, "U R U'");
}

#[test]
fn readme_content_is_rejected_as_a_bad_header() {
    let text = sheet_text("cherry", "Readme.tsv");
    match parse_sheet(&text) {
        Ok(rows) => panic!("readme parsed as {} rows", rows.len()),
        Err(err) => assert_eq!(err, TsvError::BadHeader { line: 1 }),
    }
}

#[test]
fn short_row_reports_its_line_number() {
    let base = sheet_text("cherry", "edge.tsv");
    let mut lines: Vec<&str> = base.split('\n').collect();
    lines[3] = "UF-UB-UR\tR2 U' S";
    let joined = lines.join("\n");
    match parse_sheet(&joined) {
        Ok(rows) => panic!("short row parsed as {} rows", rows.len()),
        Err(err) => assert_eq!(err, TsvError::ColumnCount { line: 4, found: 2 }),
    }
}

#[test]
fn extra_nonempty_column_is_rejected() {
    let text = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n\
                A-B-C\tU R U'\tNot found.\thttps://x\tnote\textra\n";
    assert_eq!(
        parse_sheet(text).unwrap_err(),
        TsvError::ColumnCount { line: 2, found: 6 }
    );
}

#[test]
fn empty_position_or_alg_reports_field_and_line() {
    let text = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n\
                \tU R U'\tNot found.\thttps://x\tnote\n";
    assert_eq!(
        parse_sheet(text).unwrap_err(),
        TsvError::EmptyField {
            line: 2,
            column: "Position"
        }
    );
    let text = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n\
                A-B-C\t\tNot found.\thttps://x\tnote\n";
    assert_eq!(
        parse_sheet(text).unwrap_err(),
        TsvError::EmptyField {
            line: 2,
            column: "Alg"
        }
    );
}

#[test]
fn blank_lines_are_skipped_and_empty_input_is_empty() {
    let text = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n\
                \n\
                A-B-C\tU R U'\tNot found.\thttps://x\tnote\n\
                \n";
    let rows = parse_sheet(text).unwrap();
    assert_eq!(rows.len(), 1);
    assert_eq!(rows[0].0, 3);
    assert!(parse_sheet("").unwrap().is_empty());
}

#[test]
fn sheet_row_is_cloneable_and_comparable() {
    let rows = parse_sheet(&sheet_text("eggins", "parity.tsv")).unwrap();
    let cloned: Vec<(usize, SheetRow)> = rows.to_vec();
    assert_eq!(rows.len(), cloned.len());
    assert_eq!(rows[0].1, cloned[0].1);
}
