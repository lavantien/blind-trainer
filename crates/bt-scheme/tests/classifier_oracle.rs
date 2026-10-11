use bt_core::{Alg, Cube};
use bt_scheme::{
    Classification, EDGE_FLOATING, Orbit, classify, classify_alg, head_sticker, name_of, trace,
};
use std::collections::BTreeSet;

mod common;

use common::ref_ground;

struct Row {
    solver: String,
    sheet: String,
    rowid: String,
    position: String,
    alg: String,
    qtm: u32,
    note: String,
}

fn load_rows() -> Vec<Row> {
    let base = ref_ground();
    let rows_text = std::fs::read_to_string(base.join("rows.tsv")).expect("rows.tsv readable");
    let sim_text = std::fs::read_to_string(base.join("simcheck.tsv")).expect("simcheck readable");
    let mut sim = std::collections::HashMap::new();
    let mut sim_lines = sim_text.lines().filter(|l| !l.trim().is_empty());
    let header = sim_lines.next().unwrap();
    assert_eq!(
        header,
        "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote"
    );
    for line in sim_lines {
        let cols: Vec<&str> = line.split('\t').collect();
        assert_eq!(cols.len(), 8, "simcheck row width: {line}");
        sim.insert(
            (cols[0], cols[1], cols[2]),
            (cols[3], cols[5].parse::<u32>().unwrap(), cols[7]),
        );
    }
    let mut rows = Vec::new();
    for line in rows_text.lines().filter(|l| !l.trim().is_empty()) {
        let cols: Vec<&str> = line.split('\t').collect();
        assert_eq!(cols.len(), 5, "rows row width: {line}");
        let (position, qtm, note) = sim
            .remove(&(cols[0], cols[1], cols[2]))
            .unwrap_or_else(|| panic!("simcheck row missing for {line}"));
        assert_eq!(position, cols[3], "position agreement {line}");
        rows.push(Row {
            solver: cols[0].to_string(),
            sheet: cols[1].to_string(),
            rowid: cols[2].to_string(),
            position: cols[3].to_string(),
            alg: cols[4].to_string(),
            qtm,
            note: note.to_string(),
        });
    }
    assert!(sim.is_empty(), "unmatched simcheck rows: {sim:?}");
    rows
}

fn applied(alg: &str) -> Cube {
    let mut cube = Cube::solved();
    cube.apply_alg(&Alg::parse(alg).unwrap_or_else(|e| panic!("parse {alg}: {e}")));
    cube
}

fn edge_status(report: &bt_scheme::Report, piece: &str) -> String {
    let flipped = report.defects.flip(head_sticker(piece));
    if flipped {
        "flip".to_string()
    } else {
        "ok".to_string()
    }
}

fn corner_status(report: &bt_scheme::Report, piece: &str) -> String {
    format!("{:+}", report.defects.twist(head_sticker(piece)))
}

#[test]
fn three_style_rows_reproduce_positions_exactly() {
    let rows = load_rows();
    let styled: Vec<&Row> = rows
        .iter()
        .filter(|r| r.sheet == "edge" || r.sheet == "corner")
        .collect();
    assert_eq!(styled.len(), 988);
    for row in &styled {
        let report = classify(&applied(&row.alg));
        let expected_orbit = if row.sheet == "edge" {
            Orbit::Edge
        } else {
            Orbit::Corner
        };
        let Classification::ThreeStyle { orbit, cases } = &report.class else {
            panic!(
                "{} {} {} {} not three style: {:?}",
                row.solver, row.sheet, row.rowid, row.position, report.class
            );
        };
        assert_eq!(*orbit, expected_orbit);
        assert_eq!(cases.len(), 1);
        assert_eq!(
            report.position().as_deref(),
            Some(row.position.as_str()),
            "{} {} {}",
            row.solver,
            row.sheet,
            row.rowid
        );
    }
}

#[test]
fn endgame_rows_class_and_named_pieces_match() {
    let rows = load_rows();
    let endgame: Vec<&Row> = rows
        .iter()
        .filter(|r| ["flips", "twists", "parity", "ltct"].contains(&r.sheet.as_str()))
        .collect();
    assert_eq!(endgame.len(), 89);
    for row in &endgame {
        let report = classify(&applied(&row.alg));
        match row.sheet.as_str() {
            "flips" => {
                let Classification::FlipPair { first, second } = &report.class else {
                    panic!(
                        "{} {} {} not flips: {:?}",
                        row.solver, row.sheet, row.rowid, report.class
                    );
                };
                let got: BTreeSet<&str> = [name_of(*first), name_of(*second)].into();
                let want: BTreeSet<&str> = row.position.split(" & ").collect();
                assert_eq!(got, want, "{} {} {}", row.solver, row.sheet, row.rowid);
            }
            "twists" => {
                assert!(
                    matches!(report.class, Classification::TwistPair { .. }),
                    "{} {} {} not twists: {:?}",
                    row.solver,
                    row.sheet,
                    row.rowid,
                    report.class
                );
            }
            "parity" | "ltct" => {
                assert_eq!(
                    report.position().as_deref(),
                    Some(row.position.as_str()),
                    "{} {} {} class {:?}",
                    row.solver,
                    row.sheet,
                    row.rowid,
                    report.class
                );
            }
            _ => unreachable!(),
        }
    }
}

#[test]
fn defect_notes_match_simcheck_on_carrying_rows() {
    let rows = load_rows();
    let carrying: Vec<&Row> = rows
        .iter()
        .filter(|r| r.note.starts_with("defects:"))
        .collect();
    assert_eq!(carrying.len(), 988 + 28);
    for row in &carrying {
        let report = classify(&applied(&row.alg));
        let body = row
            .note
            .trim_start_matches("defects:")
            .trim_end_matches(" clean");
        let expected: std::collections::BTreeMap<&str, &str> = body
            .split([',', ' '])
            .filter(|t| !t.is_empty())
            .map(|t| t.split_once('=').expect("defect token {t}"))
            .collect();
        assert!(
            !expected.is_empty(),
            "{} {} {}",
            row.solver,
            row.sheet,
            row.rowid
        );
        for piece in EDGE_FLOATING {
            let got = edge_status(&report, piece);
            let want = expected.get(piece).copied().unwrap_or("ok");
            assert_eq!(
                got, want,
                "{} {} {} {piece}",
                row.solver, row.sheet, row.rowid
            );
        }
        for piece in bt_scheme::CORNER_FLOATING {
            let got = corner_status(&report, piece);
            let want = expected.get(piece).copied().unwrap_or("+0");
            assert_eq!(
                got, want,
                "{} {} {} {piece}",
                row.solver, row.sheet, row.rowid
            );
        }
        for name in expected.keys() {
            assert!(
                EDGE_FLOATING.contains(name) || bt_scheme::CORNER_FLOATING.contains(name),
                "{} {} {} unknown piece {name}",
                row.solver,
                row.sheet,
                row.rowid
            );
        }
    }
}

#[test]
fn twist_pair_positions_and_signed_deltas_match_simcheck() {
    let rows = load_rows();
    let twists: Vec<&Row> = rows.iter().filter(|r| r.sheet == "twists").collect();
    assert_eq!(twists.len(), 25);
    for row in &twists {
        let report = classify(&applied(&row.alg));
        assert_eq!(
            report.position().as_deref(),
            Some(row.position.as_str()),
            "{} {} {}",
            row.solver,
            row.sheet,
            row.rowid
        );
        let names: Vec<&str> = row.position.split(" & ").collect();
        let mut d = [0i8; 2];
        for (i, token) in ["d1=", "d2="].iter().enumerate() {
            let at = row.note.find(token).expect("twist note has d token");
            let tail = &row.note[at + token.len()..];
            let sign = tail.chars().next().expect("sign char");
            let digits: String = tail[1..]
                .chars()
                .take_while(|c| c.is_ascii_digit())
                .collect();
            d[i] = format!("{sign}{digits}")
                .parse()
                .expect("signed twist value");
        }
        assert_eq!(report.defects.twist(head_sticker(names[0])), d[0]);
        assert_eq!(report.defects.twist(head_sticker(names[1])), d[1]);
        assert_eq!(d[0] + d[1], 0);
    }
}

#[test]
fn ltct_corner_twists_match_note_and_law_holds() {
    let rows = load_rows();
    let ltct: Vec<&Row> = rows.iter().filter(|r| r.sheet == "ltct").collect();
    assert_eq!(ltct.len(), 11);
    for row in &ltct {
        let report = classify(&applied(&row.alg));
        let Classification::Ltct {
            e1, e2, bracket, ..
        } = &report.class
        else {
            panic!(
                "{} {} {} not ltct: {:?}",
                row.solver, row.sheet, row.rowid, report.class
            );
        };
        let corners_part = row
            .note
            .split(" | corners: ")
            .nth(1)
            .expect("ltct note has corners part");
        let chunks: Vec<&str> = corners_part.split(" tw=").collect();
        assert_eq!(
            chunks.len(),
            4,
            "{} {} {}",
            row.solver,
            row.sheet,
            row.rowid
        );
        for i in 0..3 {
            let piece = chunks[i]
                .split_whitespace()
                .find(|t| t.contains("->"))
                .and_then(|t| t.split("->").next())
                .expect("ltct corner piece");
            let value: i8 = chunks[i + 1]
                .split_whitespace()
                .next()
                .expect("twist value")
                .parse()
                .expect("signed twist");
            assert_eq!(
                report.defects.twist(head_sticker(piece)),
                value,
                "{} {} {} {piece}",
                row.solver,
                row.sheet,
                row.rowid
            );
        }
        let sum: i32 = bt_scheme::CORNER_FLOATING
            .iter()
            .map(|p| report.defects.twist(head_sticker(p)) as i32)
            .sum();
        assert_eq!(sum % 3, 0, "{} {} {}", row.solver, row.sheet, row.rowid);
        assert_ne!(
            report.defects.twist(head_sticker(name_of(*bracket))),
            0,
            "{} {} {}",
            row.solver,
            row.sheet,
            row.rowid
        );
        assert!(!report.defects.flip(*e1));
        assert!(!report.defects.flip(*e2));
        let edges_part = row
            .note
            .split(" | edges: ")
            .nth(1)
            .and_then(|rest| rest.split(" |").next())
            .expect("ltct note has edges part");
        let swapped: BTreeSet<&str> = edges_part
            .split_whitespace()
            .map(|tok| tok.split("->").next().expect("edge piece"))
            .collect();
        let named: BTreeSet<&str> = [name_of(*e1), name_of(*e2)].into();
        assert_eq!(swapped, named, "{} {} {}", row.solver, row.sheet, row.rowid);
    }
}

#[test]
fn qtm_agrees_with_simcheck_on_every_row() {
    let rows = load_rows();
    assert_eq!(rows.len(), 1077);
    for row in &rows {
        let alg = Alg::parse(&row.alg).expect("alg parses");
        let report = classify_alg(&alg);
        assert_eq!(
            report.qtm,
            Some(row.qtm),
            "{} {} {}",
            row.solver,
            row.sheet,
            row.rowid
        );
    }
}

#[test]
fn every_row_trace_stays_within_its_orbit() {
    let rows = load_rows();
    for row in &rows {
        let state = applied(&row.alg);
        for orbit in [Orbit::Edge, Orbit::Corner] {
            for block in &trace(&state, orbit).blocks {
                assert!(block.iter().all(|&s| orbit.stickers().contains(&s)));
            }
        }
    }
}
