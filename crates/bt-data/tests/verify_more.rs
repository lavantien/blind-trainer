use bt_core::Alg;
use bt_data::corpus::SheetClass;
use bt_data::verify::{DefectKind, Note, Problem, verify};

const X2: &str = "R' D' R D R' D' R D";
const X4: &str = "R' D' R D R' D' R D R' D' R D R' D' R D";
const TPERM: &str = "R U R' U' R' F R2 U' R' U' R U R' F'";
const FLIP: &str = "M' U M' U M' U2 M U M U M U2";
const FLIP_UR_UL: &str = "U M' U M' U M' U2 M U M U M U2 U'";
const EDGE_CYCLE: &str = "M2 U M U2 M' U M2";
const CORNER_CYCLE: &str = "R U R' D' R U' R' D";

fn t3() -> String {
    format!("{X2} U {X2} U {X2} U2")
}

fn twist_pair() -> String {
    format!("{X2} U {X4} U'")
}

fn alg(text: &str) -> Alg {
    Alg::parse(text).unwrap()
}

fn twist_notes(verdict: &bt_data::verify::Verdict) -> Vec<(String, i8)> {
    verdict
        .notes
        .iter()
        .filter_map(|note| match note {
            Note::Twist { piece, twist } => Some((piece.clone(), *twist)),
            Note::Flip { .. } => None,
        })
        .collect()
}

#[test]
fn constructed_edge_and_corner_cycles_verify_ok() {
    let edge = verify(&alg(EDGE_CYCLE), "UF-UR-UL", SheetClass::Edge);
    assert!(edge.ok(), "{edge}");
    assert!(edge.notes.is_empty());
    let corner = verify(&alg(CORNER_CYCLE), "UFR-RDF-BDR", SheetClass::Corner);
    assert!(corner.ok(), "{corner}");
    assert!(corner.notes.is_empty());
}

#[test]
fn inflated_alg_with_cancelling_tail_still_verifies() {
    let inflated = verify(
        &alg(&format!("{EDGE_CYCLE} M2 M2")),
        "UF-UR-UL",
        SheetClass::Edge,
    );
    assert!(inflated.ok(), "{inflated}");
}

#[test]
fn three_style_disturbing_unnamed_pieces_reports_residue() {
    let verdict = verify(
        &alg(&format!("{EDGE_CYCLE} D")),
        "UF-UR-UL",
        SheetClass::Edge,
    );
    assert!(!verdict.ok(), "{verdict}");
    let residue: Vec<&Problem> = verdict
        .problems
        .iter()
        .filter(|p| matches!(p, Problem::NonFixedResidue { .. }))
        .collect();
    assert!(!residue.is_empty());
    assert!(
        verdict
            .problems
            .iter()
            .all(|p| matches!(p, Problem::NonFixedResidue { .. }))
    );
}

#[test]
fn opposite_direction_cycle_fails_the_original_claim() {
    let verdict = verify(&alg("M2 U' M' U2 M U' M2"), "UF-UR-UL", SheetClass::Edge);
    assert!(!verdict.ok());
    assert!(verdict.problems.contains(&Problem::WrongCycle {
        sticker: "UF".to_string(),
        expected: "UR".to_string(),
        actual: "UF".to_string()
    }));
    assert!(verdict.problems.contains(&Problem::WrongCycle {
        sticker: "UL".to_string(),
        expected: "UF".to_string(),
        actual: "UB".to_string()
    }));
}

#[test]
fn flip_pair_alg_verifies_with_no_notes() {
    let verdict = verify(&alg(FLIP), "UF & UB", SheetClass::Flips);
    assert!(verdict.ok(), "{verdict}");
    assert!(verdict.notes.is_empty());
    let conjugated = verify(&alg(FLIP_UR_UL), "UR & UL", SheetClass::Flips);
    assert!(conjugated.ok(), "{conjugated}");
    assert!(conjugated.notes.is_empty());
}

#[test]
fn twist_pair_alg_verifies_with_opposite_sign_notes() {
    let verdict = verify(&alg(&twist_pair()), "UFR & UBR", SheetClass::Twists);
    assert!(verdict.ok(), "{verdict}");
    assert_eq!(
        twist_notes(&verdict),
        vec![("UFR".to_string(), 1), ("UBR".to_string(), -1)]
    );
}

#[test]
fn t_perm_verifies_as_parity_without_orientation_notes() {
    let verdict = verify(&alg(TPERM), "UR-UL, UFR-UBR", SheetClass::Parity);
    assert!(verdict.ok(), "{verdict}");
    assert!(verdict.notes.is_empty());
}

#[test]
fn parity_with_flipped_swaps_reports_flip_notes_only() {
    let text = format!("{TPERM} {FLIP_UR_UL}");
    let verdict = verify(&alg(&text), "UR-UL, UFR-UBR", SheetClass::Parity);
    assert!(verdict.ok(), "{verdict}");
    assert_eq!(
        verdict.notes,
        vec![
            Note::Flip {
                piece: "UR".to_string()
            },
            Note::Flip {
                piece: "UL".to_string()
            },
        ]
    );
}

#[test]
fn ltct_composite_verifies_with_three_twist_notes() {
    let text = format!("{TPERM} {}", t3());
    let verdict = verify(&alg(&text), "UR-UL, UFR-RUB[LUB]", SheetClass::Ltct);
    assert!(verdict.ok(), "{verdict}");
    assert_eq!(
        twist_notes(&verdict),
        vec![
            ("UFR".to_string(), 1),
            ("UBR".to_string(), 1),
            ("UBL".to_string(), 1),
        ]
    );
}

#[test]
fn ltct_with_flipped_edge_swaps_reports_unexpected_flip() {
    let text = format!("{TPERM} {FLIP_UR_UL} {}", t3());
    let verdict = verify(&alg(&text), "UR-UL, UFR-RUB[LUB]", SheetClass::Ltct);
    assert!(!verdict.ok());
    assert_eq!(
        verdict.problems,
        vec![
            Problem::UnexpectedDefect {
                piece: "UR".to_string(),
                defect: DefectKind::Flip,
            },
            Problem::UnexpectedDefect {
                piece: "UL".to_string(),
                defect: DefectKind::Flip,
            },
        ]
    );
    assert_eq!(
        twist_notes(&verdict),
        vec![
            ("UFR".to_string(), 1),
            ("UBR".to_string(), 1),
            ("UBL".to_string(), 1),
        ]
    );
    assert_eq!(
        verdict.to_string(),
        "piece UR is flipped but should be intact\npiece UL is flipped but should be intact"
    );
}

#[test]
fn solved_cube_fails_every_named_edge_cycle_slot() {
    let verdict = verify(&alg(""), "UF-UB-UL", SheetClass::Edge);
    assert!(!verdict.ok());
    assert_eq!(verdict.problems.len(), 6);
    assert!(verdict.problems.contains(&Problem::WrongCycle {
        sticker: "UF".to_string(),
        expected: "UB".to_string(),
        actual: "UF".to_string()
    }));
    assert!(verdict.problems.contains(&Problem::WrongCycle {
        sticker: "UB".to_string(),
        expected: "UL".to_string(),
        actual: "UB".to_string()
    }));
    assert!(verdict.problems.contains(&Problem::WrongCycle {
        sticker: "UL".to_string(),
        expected: "UF".to_string(),
        actual: "UL".to_string()
    }));
}

#[test]
fn edge_sheet_flags_unnamed_piece_disturbance() {
    let verdict = verify(&alg("M2"), "UF-UB-UL", SheetClass::Edge);
    assert!(!verdict.ok());
    assert_eq!(verdict.problems.len(), 11);
    assert!(verdict.problems.contains(&Problem::CentersMoved));
    let wrong = verdict
        .problems
        .iter()
        .filter(|p| matches!(p, Problem::WrongCycle { .. }))
        .count();
    assert_eq!(wrong, 6);
    let residue = verdict
        .problems
        .iter()
        .filter(|p| matches!(p, Problem::NonFixedResidue { .. }))
        .count();
    assert_eq!(residue, 4);
    assert!(verdict.problems.contains(&Problem::WrongCycle {
        sticker: "UL".to_string(),
        expected: "UF".to_string(),
        actual: "UL".to_string()
    }));
    let extra = verify(
        &alg(&format!("{EDGE_CYCLE} {CORNER_CYCLE}")),
        "UF-UR-UL",
        SheetClass::Edge,
    );
    assert!(!extra.ok());
    let corner_residue = extra
        .problems
        .iter()
        .filter(|p| matches!(p, Problem::NonFixedResidue { .. }))
        .count();
    assert_eq!(corner_residue, 9);
}

#[test]
fn solved_cube_reports_missing_flips_and_twists_with_zero_notes() {
    let flips = verify(&alg(""), "UF & UB", SheetClass::Flips);
    assert_eq!(
        flips.problems,
        vec![
            Problem::MissingDefect {
                piece: "UF".to_string(),
                defect: DefectKind::Flip,
            },
            Problem::MissingDefect {
                piece: "UB".to_string(),
                defect: DefectKind::Flip,
            },
        ]
    );
    assert!(flips.notes.is_empty());
    let twists = verify(&alg(""), "UFR & UBR", SheetClass::Twists);
    assert_eq!(
        twists.problems,
        vec![
            Problem::MissingDefect {
                piece: "UFR".to_string(),
                defect: DefectKind::Twist,
            },
            Problem::MissingDefect {
                piece: "UBR".to_string(),
                defect: DefectKind::Twist,
            },
        ]
    );
    assert_eq!(
        twist_notes(&twists),
        vec![("UFR".to_string(), 0), ("UBR".to_string(), 0)]
    );
}

#[test]
fn solved_cube_reports_both_parity_swaps_displaced() {
    let verdict = verify(&alg(""), "UR-UL, UFR-UBR", SheetClass::Parity);
    assert!(!verdict.ok());
    assert_eq!(verdict.problems.len(), 4);
    assert!(
        verdict
            .problems
            .iter()
            .all(|p| matches!(p, Problem::Displaced { .. }))
    );
    assert!(verdict.problems.contains(&Problem::Displaced {
        piece: "UR".to_string(),
        expected_slot: "UL".to_string(),
        actual_slot: "UR".to_string()
    }));
    assert!(verdict.problems.contains(&Problem::Displaced {
        piece: "UBR".to_string(),
        expected_slot: "UFR".to_string(),
        actual_slot: "UBR".to_string()
    }));
    assert!(verdict.notes.is_empty());
}

#[test]
fn solved_cube_reports_ltct_swaps_displaced_and_bracket_untwisted() {
    let verdict = verify(&alg(""), "UR-UL, UFR-UBR[UBL]", SheetClass::Ltct);
    assert_eq!(verdict.problems.len(), 5);
    assert_eq!(
        verdict.problems.last(),
        Some(&Problem::MissingDefect {
            piece: "UBL".to_string(),
            defect: DefectKind::Twist,
        })
    );
    assert_eq!(twist_notes(&verdict), vec![("UBL".to_string(), 0)]);
}

#[test]
fn three_equal_twists_fail_the_pair_claim_with_residue() {
    let verdict = verify(&alg(&t3()), "UFR & UBR", SheetClass::Twists);
    assert!(!verdict.ok());
    assert!(verdict.problems.contains(&Problem::TwistsNotOpposite {
        first: "UFR".to_string(),
        second: "UBR".to_string()
    }));
    let residue = verdict
        .problems
        .iter()
        .filter(|p| matches!(p, Problem::NonFixedResidue { .. }))
        .count();
    assert_eq!(residue, 3);
    assert_eq!(
        twist_notes(&verdict),
        vec![("UFR".to_string(), 1), ("UBR".to_string(), 1)]
    );
}

#[test]
fn t_perm_on_twists_sheet_reports_displaced_and_missing_twists() {
    let verdict = verify(&alg(TPERM), "UFR & UBR", SheetClass::Twists);
    assert!(!verdict.ok());
    assert!(verdict.problems.contains(&Problem::Displaced {
        piece: "UFR".to_string(),
        expected_slot: "UFR".to_string(),
        actual_slot: "UBR".to_string()
    }));
    assert!(verdict.problems.contains(&Problem::MissingDefect {
        piece: "UFR".to_string(),
        defect: DefectKind::Twist,
    }));
    let residue = verdict
        .problems
        .iter()
        .filter(|p| matches!(p, Problem::NonFixedResidue { .. }))
        .count();
    assert_eq!(residue, 4);
}

#[test]
fn ltct_bracket_on_untwisted_corner_reports_sum_and_missing_defect() {
    let text = format!("{TPERM} {}", t3());
    let verdict = verify(&alg(&text), "UR-UL, UFR-RUB[UFL]", SheetClass::Ltct);
    assert!(!verdict.ok());
    assert!(
        verdict
            .problems
            .contains(&Problem::TwistSumNotZero { sum: 2 })
    );
    assert!(verdict.problems.contains(&Problem::MissingDefect {
        piece: "UFL".to_string(),
        defect: DefectKind::Twist,
    }));
    assert_eq!(
        twist_notes(&verdict),
        vec![
            ("UFR".to_string(), 1),
            ("UBR".to_string(), 1),
            ("UFL".to_string(), 0),
        ]
    );
}

#[test]
fn bad_three_style_positions_report_their_reason() {
    let cases = [
        ("UF-UB", "expected 3 tokens"),
        ("UF-UB-UL-UR", "expected 3 tokens"),
        ("UF-UB-UFR", "orbit"),
        ("UF-UB-UF", "twice"),
        ("GARBAGE-UB-UL", "unknown sticker"),
        ("", "unknown sticker"),
    ];
    for (position, needle) in cases {
        let verdict = verify(&alg(EDGE_CYCLE), position, SheetClass::Edge);
        assert_eq!(
            verdict.problems.len(),
            1,
            "{position}: {:?}",
            verdict.problems
        );
        match &verdict.problems[0] {
            Problem::BadPosition { detail } => {
                assert!(detail.contains(needle), "{position}: {detail}")
            }
            other => panic!("{position}: {other:?}"),
        }
    }
    let corner_orbit = verify(&alg(CORNER_CYCLE), "UF-UB-UL", SheetClass::Corner);
    assert!(matches!(
        corner_orbit.problems.as_slice(),
        [Problem::BadPosition { detail }] if detail.contains("orbit")
    ));
}

#[test]
fn bad_pair_positions_report_their_reason() {
    let flips = [
        ("UF & UB & UL", "expected 2 names"),
        ("UF-UB", "expected 2 names"),
        ("UFR & UBR", "orbit"),
        ("UF & GARBAGE", "unknown sticker"),
    ];
    for (position, needle) in flips {
        let verdict = verify(&alg(FLIP), position, SheetClass::Flips);
        assert!(
            matches!(&verdict.problems.as_slice(), [Problem::BadPosition { detail }] if detail.contains(needle)),
            "{position}: {:?}",
            verdict.problems
        );
    }
    let twists = verify(&alg(&twist_pair()), "UF & UB", SheetClass::Twists);
    assert!(matches!(
        twists.problems.as_slice(),
        [Problem::BadPosition { detail }] if detail.contains("orbit")
    ));
}

#[test]
fn bad_parity_positions_report_their_reason() {
    let cases = [
        ("UF-UL UFR-UBR", "expected an edge pair"),
        ("UF-UL, UFR", "needs 2 edges and 2 corners"),
        ("UF-UL-UR, UFR-UBR", "needs 2 edges and 2 corners"),
        ("UF-UL, UFR-UFL-UBL", "needs 2 edges and 2 corners"),
        ("UF-UL, UF-UB", "orbit"),
    ];
    for (position, needle) in cases {
        let verdict = verify(&alg(TPERM), position, SheetClass::Parity);
        assert!(
            matches!(&verdict.problems.as_slice(), [Problem::BadPosition { detail }] if detail.contains(needle)),
            "{position}: {:?}",
            verdict.problems
        );
    }
}

#[test]
fn bad_ltct_positions_report_their_reason() {
    let cases = [
        ("UF-UR, UBR-LUB[FUR", "unclosed bracket"),
        ("UF-UR, UBR-LUB", "bracketed twist corner"),
        ("UF-UR-UB, UBR-LUB[FUR]", "2 swapped edges"),
        ("UF-UR, UBR-LUB-FUR[UFL]", "2 swapped corners"),
        ("UF-UR, UBR-LUB[UF]", "orbit"),
        ("UF-UR, UBR-LUB[GARBAGE]", "unknown sticker"),
    ];
    for (position, needle) in cases {
        let verdict = verify(
            &alg(&format!("{TPERM} {}", t3())),
            position,
            SheetClass::Ltct,
        );
        assert!(
            matches!(&verdict.problems.as_slice(), [Problem::BadPosition { detail }] if detail.contains(needle)),
            "{position}: {:?}",
            verdict.problems
        );
    }
}
