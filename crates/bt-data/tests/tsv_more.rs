use bt_data::tsv::{TsvError, parse_sheet};

const HEADER: &str = "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link";

fn row(position: &str, alg: &str, commutator: &str, recon: &str, youtube: &str) -> String {
    format!("{position}\t{alg}\t{commutator}\t{recon}\t{youtube}")
}

#[test]
fn quoted_cells_pass_through_verbatim() {
    let text = format!(
        "{HEADER}\n{}\n",
        row(
            "UF-UB-UL",
            "U M2 U \"M'\" U2",
            "say \"setup:insert\" here",
            "https://x?q=\"1 2\"",
            "\"quoted\" note"
        )
    );
    let rows = parse_sheet(&text).unwrap();
    assert_eq!(rows.len(), 1);
    let parsed = &rows[0].1;
    assert_eq!(parsed.position, "UF-UB-UL");
    assert_eq!(parsed.alg, "U M2 U \"M'\" U2");
    assert_eq!(parsed.commutator, "say \"setup:insert\" here");
    assert_eq!(parsed.recon_link, "https://x?q=\"1 2\"");
    assert_eq!(parsed.youtube_link, "\"quoted\" note");
}

#[test]
fn ragged_rows_report_their_found_width() {
    for (cells, found) in [
        ("UF-UB-UL", 1),
        ("UF-UB-UL\tU M2 U'", 2),
        ("UF-UB-UL\tU M2 U'\tx", 3),
    ] {
        let text = format!("{HEADER}\n{cells}\n");
        assert_eq!(
            parse_sheet(&text).unwrap_err(),
            TsvError::ColumnCount { line: 2, found },
            "{cells}"
        );
    }
}

#[test]
fn wrong_middle_header_cell_is_a_bad_header() {
    for header in [
        "Position\tMove\tCommutator\tReconstruction Link\tYoutube Link",
        "Position\tAlg\tCommutators\tReconstruction Link\tYoutube Link",
        "Position\tAlg\tCommutator\tReconstruction\tYoutube Link",
        "Position\tAlg\tCommutator\tReconstruction Link\tLink",
    ] {
        assert_eq!(
            parse_sheet(&format!("{header}\n{}", row("A-B-C", "R U R'", "", "", ""))).unwrap_err(),
            TsvError::BadHeader { line: 1 },
            "{header}"
        );
    }
}

#[test]
fn padded_header_cells_are_tolerated() {
    let header = " Position \t Alg \t Commutator \t Reconstruction Link \t Youtube Link ";
    let rows = parse_sheet(&format!("{header}\n{}", row("A-B-C", "R U R'", "", "", ""))).unwrap();
    assert_eq!(rows.len(), 1);
    assert_eq!(rows[0].1.alg, "R U R'");
}

#[test]
fn leading_blank_lines_shift_the_header_line_number() {
    let text = "\n \nnope\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n";
    assert_eq!(
        parse_sheet(text).unwrap_err(),
        TsvError::BadHeader { line: 3 }
    );
}

#[test]
fn header_only_file_yields_no_rows() {
    assert!(parse_sheet(HEADER).unwrap().is_empty());
    assert!(parse_sheet(&format!("{HEADER}\n")).unwrap().is_empty());
    assert!(
        parse_sheet(&format!("{HEADER}\n\t\n \n"))
            .unwrap()
            .is_empty()
    );
}

#[test]
fn crlf_rows_report_physical_line_numbers() {
    let text = format!(
        "{HEADER}\r\n{}\r\n\r\n{}\r\n",
        row("A-B-C", "R U R'", "", "", ""),
        row("D-E-F", "R2", "", "", "")
    );
    let rows = parse_sheet(&text).unwrap();
    assert_eq!(rows.len(), 2);
    assert_eq!(rows[0].0, 2);
    assert_eq!(rows[1].0, 4);
    assert_eq!(rows[1].1.position, "D-E-F");
}

#[test]
fn trailing_empty_columns_on_data_rows_are_trimmed() {
    let text = format!("{HEADER}\n{}\t\t\n", row("A-B-C", "R U R'", "", "", ""));
    let rows = parse_sheet(&text).unwrap();
    assert_eq!(rows.len(), 1);
    assert_eq!(rows[0].1.youtube_link, "");
}

#[test]
fn whitespace_only_position_or_alg_is_still_empty() {
    let text = format!("{HEADER}\n{}\n", row("   ", "R U R'", "", "", ""));
    assert_eq!(
        parse_sheet(&text).unwrap_err(),
        TsvError::EmptyField {
            line: 2,
            column: "Position"
        }
    );
    let text = format!("{HEADER}\n{}\n", row("A-B-C", " \t ", "", "", ""));
    assert_eq!(
        parse_sheet(&text).unwrap_err(),
        TsvError::EmptyField {
            line: 2,
            column: "Alg"
        }
    );
}

#[test]
fn later_bad_row_stops_the_parse_at_its_line() {
    let text = format!(
        "{HEADER}\n{}\nnope\n{}\n",
        row("A-B-C", "R U R'", "", "", ""),
        row("D-E-F", "R2", "", "", "")
    );
    assert_eq!(
        parse_sheet(&text).unwrap_err(),
        TsvError::ColumnCount { line: 3, found: 1 }
    );
}
