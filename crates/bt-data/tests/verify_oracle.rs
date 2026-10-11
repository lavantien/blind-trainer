mod common;

use bt_core::Alg;
use bt_data::corpus::{Corpus, SheetClass, Solver};
use bt_data::verify::{Note, Problem, verify};
use common::ref_ground;

fn loaded() -> Corpus {
    Corpus::load(&ref_ground()).expect("corpus")
}

#[test]
fn every_vendor_row_verifies_ok() {
    let corpus = loaded();
    let mut failures = Vec::new();
    for row in corpus.rows() {
        let verdict = verify(&row.alg, &row.position, row.sheet);
        if !verdict.ok() {
            failures.push(format!(
                "{}/{}/{} {}: {}",
                row.solver, row.sheet, row.rowid, row.position, verdict
            ));
        }
    }
    assert!(
        failures.is_empty(),
        "{} of 1077 rows failed:\n{}",
        failures.len(),
        failures.join("\n")
    );
}

fn drop_move(text: &str) -> Option<String> {
    let tokens: Vec<&str> = text.split_whitespace().collect();
    if tokens.len() < 2 {
        return None;
    }
    let index = tokens.len() / 2;
    let mut mutated = tokens.clone();
    mutated.remove(index);
    Some(mutated.join(" "))
}

fn invert_move(text: &str) -> Option<String> {
    let tokens: Vec<&str> = text.split_whitespace().collect();
    let index = tokens
        .iter()
        .position(|token| !token.ends_with('2') && token.len() >= 1)?;
    let mut mutated = tokens.clone();
    let token = mutated[index].clone();
    let inverted = if let Some(stripped) = token.strip_suffix('\'') {
        stripped.to_string()
    } else {
        format!("{token}'")
    };
    mutated[index] = &inverted;
    Some(mutated.join(" "))
}

fn swap_face(letter: char) -> char {
    match letter {
        'U' => 'D',
        'D' => 'U',
        'L' => 'R',
        'R' => 'L',
        'F' => 'B',
        'B' => 'F',
        'u' => 'd',
        'd' => 'u',
        'l' => 'r',
        'r' => 'l',
        'f' => 'b',
        'b' => 'f',
        'M' => 'E',
        'E' => 'M',
        'x' => 'y',
        'y' => 'z',
        'z' => 'x',
        other => other,
    }
}

fn swap_face_move(text: &str) -> Option<String> {
    let tokens: Vec<&str> = text.split_whitespace().collect();
    let index = tokens
        .iter()
        .position(|token| token.chars().any(|c| c.is_ascii_alphabetic()))?;
    let mut mutated = tokens.clone();
    let token = mutated[index].clone();
    let swapped: String = token
        .chars()
        .map(|c| {
            if c.is_ascii_alphabetic() {
                swap_face(c)
            } else {
                c
            }
        })
        .collect();
    mutated[index] = &swapped;
    Some(mutated.join(" "))
}

#[test]
fn corrupted_algs_fail_verification_with_named_problems() {
    let corpus = loaded();
    let mut checked = 0;
    let mut misses = Vec::new();
    for row in corpus.rows() {
        if row.rowid > 6 {
            continue;
        }
        for mutated in [
            drop_move(&row.alg_text),
            invert_move(&row.alg_text),
            swap_face_move(&row.alg_text),
        ]
        .into_iter()
        .flatten()
        {
            let Ok(alg) = Alg::parse(&mutated) else {
                continue;
            };
            let verdict = verify(&alg, &row.position, row.sheet);
            checked += 1;
            if verdict.ok() {
                misses.push(format!(
                    "{}/{}/{} accepted corruption: {} -> {}",
                    row.solver, row.sheet, row.rowid, row.alg_text, mutated
                ));
            }
        }
    }
    assert!(checked >= 30, "only {checked} negative cases exercised");
    assert!(misses.is_empty(), "{}", misses.join("\n"));
}

#[test]
fn wrong_alg_for_a_position_fails() {
    let corpus = loaded();
    let row = corpus
        .row(Solver::Cherry, SheetClass::Edge, 2)
        .expect("row 2");
    assert_eq!(row.position, "UF-UB-UL");
    let verdict = verify(&row.alg, "UF-UB-UR", SheetClass::Edge);
    assert!(!verdict.ok());
    assert!(verdict.to_string().contains("should sit on"));
}

#[test]
fn appended_rotation_reports_centers_moved() {
    let corpus = loaded();
    for (sheet, rowid) in [
        (SheetClass::Edge, 2usize),
        (SheetClass::Flips, 2),
        (SheetClass::Twists, 2),
        (SheetClass::Parity, 2),
        (SheetClass::Ltct, 2),
    ] {
        let row = corpus.row(Solver::Cherry, sheet, rowid).expect("row");
        let rotated = format!("{} x", row.alg_text);
        let alg = Alg::parse(&rotated).unwrap();
        let verdict = verify(&alg, &row.position, sheet);
        assert!(
            verdict.problems.contains(&Problem::CentersMoved),
            "{sheet:?} {rowid}: {:?}",
            verdict.problems
        );
    }
}

#[test]
fn twists_verdict_reports_opposite_simulated_signs() {
    let corpus = loaded();
    let row = corpus
        .row(Solver::Cherry, SheetClass::Twists, 2)
        .expect("row 2");
    let verdict = verify(&row.alg, &row.position, SheetClass::Twists);
    assert!(verdict.ok(), "{verdict}");
    let twists: Vec<(String, i8)> = verdict
        .notes
        .iter()
        .filter_map(|note| match note {
            Note::Twist { piece, twist } => Some((piece.clone(), *twist)),
            _ => None,
        })
        .collect();
    assert_eq!(twists.len(), 2);
    assert_ne!(twists[0].0, twists[1].0);
    assert_eq!(twists[0].1, -twists[1].1);
    assert!(twists[0].1 != 0 && twists[1].1 != 0);
}

#[test]
fn parity_verdict_reports_orientation_defects_but_never_fails_on_them() {
    let corpus = loaded();
    let clean = corpus
        .row(Solver::Cherry, SheetClass::Parity, 2)
        .expect("row 2");
    let clean_verdict = verify(&clean.alg, &clean.position, SheetClass::Parity);
    assert!(clean_verdict.ok(), "{clean_verdict}");
    assert!(clean_verdict.notes.is_empty());

    let twisted = corpus
        .row(Solver::Cherry, SheetClass::Parity, 3)
        .expect("row 3");
    let twisted_verdict = verify(&twisted.alg, &twisted.position, SheetClass::Parity);
    assert!(twisted_verdict.ok(), "{twisted_verdict}");
    let twist_notes: Vec<&Note> = twisted_verdict
        .notes
        .iter()
        .filter(|note| matches!(note, Note::Twist { .. }))
        .collect();
    assert_eq!(twist_notes.len(), 2);
    assert!(
        twisted_verdict
            .notes
            .iter()
            .all(|note| !matches!(note, Note::Flip { .. }))
    );
}

#[test]
fn ltct_verdict_reports_the_bracket_twist_and_swap_twists() {
    let corpus = loaded();
    let row = corpus
        .row(Solver::Cherry, SheetClass::Ltct, 2)
        .expect("row 2");
    let verdict = verify(&row.alg, &row.position, SheetClass::Ltct);
    assert!(verdict.ok(), "{verdict}");
    let twists: Vec<(String, i8)> = verdict
        .notes
        .iter()
        .filter_map(|note| match note {
            Note::Twist { piece, twist } => Some((piece.clone(), *twist)),
            _ => None,
        })
        .collect();
    assert_eq!(twists.len(), 3);
    assert!(
        verdict
            .notes
            .iter()
            .all(|note| !matches!(note, Note::Flip { .. }))
    );
}

#[test]
fn defect_carrying_three_style_rows_verify_and_clean_ones_do_too() {
    let corpus = loaded();
    let defective = corpus
        .row(Solver::Cherry, SheetClass::Edge, 5)
        .expect("row 5");
    assert_eq!(defective.position, "UF-UB-LB");
    let verdict = verify(&defective.alg, &defective.position, SheetClass::Edge);
    assert!(verdict.ok(), "{verdict}");
    let clean = corpus
        .row(Solver::Cherry, SheetClass::Edge, 2)
        .expect("row 2");
    let verdict = verify(&clean.alg, &clean.position, SheetClass::Edge);
    assert!(verdict.ok(), "{verdict}");
}

#[test]
fn malformed_positions_report_bad_position() {
    let corpus = loaded();
    let row = corpus
        .row(Solver::Cherry, SheetClass::Edge, 2)
        .expect("row 2");
    for (position, sheet) in [
        ("GARBAGE", SheetClass::Edge),
        ("UF-UB", SheetClass::Edge),
        ("UF-UB-UL-DR", SheetClass::Edge),
        ("UF-UB-UFR", SheetClass::Edge),
        ("UF-UB-UF", SheetClass::Edge),
        ("UFR & UBR", SheetClass::Flips),
        ("UF-UL, UFR", SheetClass::Parity),
        ("UF-UL", SheetClass::Parity),
        ("UF-UR, UBR-LUB", SheetClass::Ltct),
        ("", SheetClass::Edge),
    ] {
        let verdict = verify(&row.alg, position, sheet);
        assert!(
            verdict
                .problems
                .iter()
                .any(|problem| matches!(problem, Problem::BadPosition { .. })),
            "{position} / {sheet:?}: {:?}",
            verdict.problems
        );
    }
}
