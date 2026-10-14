use bt_core::{Alg, Cube};
use bt_scheme::pairs::CaseId;
use bt_scheme::scheme::{
    CORNER_BUFFER_STICKER, EDGE_BUFFER_STICKER, Orbit, head_sticker, letter_of,
};
use bt_scheme::scramble::{occ_is_legal, random_occ};
use bt_scheme::{
    Classification, Parity, Residue, ResidueClass, Twist, apply_memo, apply_residue,
    apply_sticker_cycles, classify, ideal_cycle, ideal_cycle_at, memo_from_array,
};

fn identity() -> [u8; 54] {
    std::array::from_fn(|i| i as u8)
}

fn after(alg: &str) -> Cube {
    let mut cube = Cube::solved();
    cube.apply_alg(&Alg::parse(alg).unwrap());
    cube
}

fn assert_solves(occ: &[u8; 54], m: &bt_scheme::Memo) {
    let mut work = *occ;
    apply_memo(&mut work, m);
    assert_eq!(work, identity());
}

fn edge_case(t1: u8, t2: u8) -> CaseId {
    CaseId {
        orbit: Orbit::Edge,
        t1,
        t2,
    }
}

fn corner_case(t1: u8, t2: u8) -> CaseId {
    CaseId {
        orbit: Orbit::Corner,
        t1,
        t2,
    }
}

#[test]
fn ideal_cycle_matches_standard_buffer_for_both_orbits() {
    let edge = edge_case(30, 47);
    assert_eq!(
        ideal_cycle(&edge),
        ideal_cycle_at(Orbit::Edge, EDGE_BUFFER_STICKER, 30, 47)
    );
    let corner = corner_case(1, 0);
    assert_eq!(
        ideal_cycle(&corner),
        ideal_cycle_at(Orbit::Corner, CORNER_BUFFER_STICKER, 1, 0)
    );
}

#[test]
fn ideal_cycle_at_corner_buffer_pins_cycles() {
    assert_eq!(
        ideal_cycle_at(Orbit::Corner, 3, 1, 0),
        vec![[3, 1, 0], [5, 13, 17], [8, 16, 4]]
    );
    let mut occ = identity();
    apply_sticker_cycles(&mut occ, &ideal_cycle_at(Orbit::Corner, 3, 1, 0));
    let cube = Cube::from_occ(occ);
    assert_eq!(cube.position_of(3), 1);
    assert_eq!(cube.position_of(1), 0);
    assert_eq!(cube.position_of(0), 3);
    assert_eq!(cube.position_of(5), 13);
    assert_eq!(cube.position_of(13), 17);
    assert_eq!(cube.position_of(17), 5);
    assert_eq!(cube.position_of(2), 2);
    assert_eq!(cube.position_of(21), 21);
}

#[test]
fn edge_three_style_case_from_ideal_cycle_round_trips() {
    let mut occ = identity();
    apply_sticker_cycles(&mut occ, &ideal_cycle_at(Orbit::Edge, 26, 24, 25));
    assert!(occ_is_legal(&occ));
    let cube = Cube::from_occ(occ);
    assert_eq!(cube.position_of(26), 24);
    assert_eq!(cube.position_of(24), 25);
    assert_eq!(cube.position_of(25), 26);
    assert_eq!(cube.position_of(0), 0);
    assert_eq!(cube.position_of(47), 47);
    let m = memo_from_array(&occ);
    assert_eq!(m.edge_letters, vec!['B', 'A']);
    assert_eq!(m.pairs.0, vec![edge_case(25, 24)]);
    assert_eq!(m.buffers.0, vec![26]);
    assert!(m.pairs.1.is_empty() && m.corner_letters.is_empty());
    assert_eq!(m.residue, Residue::default());
    assert_solves(&occ, &m);
    match classify(&cube).class {
        Classification::ThreeStyle { orbit, cases } => {
            assert_eq!(orbit, Orbit::Edge);
            assert_eq!(cases, vec![edge_case(24, 25)]);
        }
        other => panic!("{other:?}"),
    }
    assert_eq!(classify(&cube).position().as_deref(), Some("UF-UB-UR"));
}

#[test]
fn corner_three_style_case_from_ideal_cycle_round_trips() {
    let mut occ = identity();
    apply_sticker_cycles(&mut occ, &ideal_cycle_at(Orbit::Corner, 2, 0, 1));
    assert!(occ_is_legal(&occ));
    let cube = Cube::from_occ(occ);
    assert_eq!(cube.position_of(2), 0);
    assert_eq!(cube.position_of(0), 1);
    assert_eq!(cube.position_of(1), 2);
    assert_eq!(cube.position_of(3), 3);
    let m = memo_from_array(&occ);
    assert_eq!(m.corner_letters, vec!['B', 'A']);
    assert_eq!(m.pairs.1, vec![corner_case(1, 0)]);
    assert_eq!(m.buffers.1, vec![2]);
    assert!(m.pairs.0.is_empty() && m.edge_letters.is_empty());
    assert_eq!(m.residue, Residue::default());
    assert_solves(&occ, &m);
    match classify(&cube).class {
        Classification::ThreeStyle { orbit, cases } => {
            assert_eq!(orbit, Orbit::Corner);
            assert_eq!(cases, vec![corner_case(0, 1)]);
        }
        other => panic!("{other:?}"),
    }
    assert_eq!(classify(&cube).position().as_deref(), Some("UFR-UBL-UBR"));
}

#[test]
fn edge_buffers_float_through_the_documented_order() {
    let mut occ = identity();
    apply_sticker_cycles(&mut occ, &ideal_cycle_at(Orbit::Edge, 26, 24, 25));
    apply_sticker_cycles(&mut occ, &ideal_cycle_at(Orbit::Edge, 33, 35, 44));
    apply_sticker_cycles(&mut occ, &ideal_cycle_at(Orbit::Edge, 46, 47, 43));
    assert!(occ_is_legal(&occ));
    let m = memo_from_array(&occ);
    assert_eq!(m.edge_letters, vec!['B', 'A', 'U', 'L', 'T', 'X']);
    assert_eq!(
        m.pairs.0,
        vec![edge_case(25, 24), edge_case(44, 35), edge_case(43, 47)]
    );
    assert_eq!(m.buffers.0, vec![26, 33, 46]);
    assert_eq!(m.residue, Residue::default());
    assert_solves(&occ, &m);
}

#[test]
fn corner_buffers_float_through_the_documented_order() {
    let mut occ = identity();
    apply_sticker_cycles(&mut occ, &ideal_cycle_at(Orbit::Corner, 2, 1, 0));
    apply_sticker_cycles(&mut occ, &ideal_cycle_at(Orbit::Corner, 3, 21, 22));
    assert!(occ_is_legal(&occ));
    let m = memo_from_array(&occ);
    assert_eq!(m.corner_letters, vec!['A', 'B', 'W', 'V']);
    assert_eq!(m.pairs.1, vec![corner_case(0, 1), corner_case(22, 21)]);
    assert_eq!(m.buffers.1, vec![2, 3]);
    assert_eq!(m.residue, Residue::default());
    assert_solves(&occ, &m);
    assert!(matches!(
        classify(&Cube::from_occ(occ)).class,
        Classification::Unclassified(_)
    ));
}

#[test]
fn flip_pair_state_leaves_flip_residue_and_classifies() {
    let mut occ = identity();
    apply_residue(
        &mut occ,
        &Residue {
            flips: vec![26, 24],
            ..Residue::default()
        },
    );
    assert!(occ_is_legal(&occ));
    let m = memo_from_array(&occ);
    assert!(m.edge_letters.is_empty() && m.pairs.0.is_empty());
    assert_eq!(m.residue.flips, vec![24, 26]);
    assert!(m.residue.parity.is_none() && m.residue.twists.is_empty());
    assert_eq!(m.residue.class(), ResidueClass::FlipPair(24, 26));
    assert_solves(&occ, &m);
    let report = classify(&Cube::from_occ(occ));
    assert_eq!(
        report.class,
        Classification::FlipPair {
            first: 26,
            second: 24
        }
    );
    assert_eq!(report.position().as_deref(), Some("UF & UB"));
    assert_eq!(report.defects.flipped_pieces(), vec![24, 26]);
    assert!(report.defects.flip(head_sticker("UF")));
    assert!(!report.defects.flip(head_sticker("UR")));
    assert!(report.defects.twisted_pieces().is_empty());
}

#[test]
fn twist_pair_state_leaves_twist_residue_and_classifies() {
    let mut occ = identity();
    apply_residue(
        &mut occ,
        &Residue {
            twists: vec![
                Twist {
                    corner: 2,
                    quarter: 1,
                },
                Twist {
                    corner: 21,
                    quarter: 2,
                },
            ],
            ..Residue::default()
        },
    );
    assert!(occ_is_legal(&occ));
    let m = memo_from_array(&occ);
    assert!(m.corner_letters.is_empty() && m.pairs.1.is_empty());
    let mut got: Vec<(u8, u8)> = m
        .residue
        .twists
        .iter()
        .map(|t| (t.corner, t.quarter))
        .collect();
    got.sort_unstable();
    assert_eq!(got, vec![(2, 2), (21, 1)]);
    assert_eq!(m.residue.class(), ResidueClass::TwistPair(2, 21));
    assert_solves(&occ, &m);
    let report = classify(&Cube::from_occ(occ));
    assert_eq!(
        report.class,
        Classification::TwistPair {
            first: 2,
            second: 21
        }
    );
    let position = report.position().expect("twist pair position");
    let names: Vec<&str> = position.split(" & ").collect();
    assert_eq!(names.len(), 2);
    let mut expected = vec!["UFR", "DFR"];
    expected.sort_unstable();
    let mut printed = names.clone();
    printed.sort_unstable();
    assert_eq!(printed, expected);
    assert_eq!(report.defects.twist(head_sticker(names[0])), 1);
    assert_eq!(report.defects.twist(head_sticker(names[1])), -1);
}

#[test]
fn twist_triple_state_leaves_equal_quarter_residue() {
    let mut occ = identity();
    apply_residue(
        &mut occ,
        &Residue {
            twists: vec![
                Twist {
                    corner: 2,
                    quarter: 1,
                },
                Twist {
                    corner: 3,
                    quarter: 1,
                },
                Twist {
                    corner: 21,
                    quarter: 1,
                },
            ],
            ..Residue::default()
        },
    );
    assert!(occ_is_legal(&occ));
    let m = memo_from_array(&occ);
    assert!(m.corner_letters.is_empty() && m.pairs.1.is_empty());
    assert_eq!(m.residue.class(), ResidueClass::TwistTriple(2, 3, 21));
    assert_solves(&occ, &m);
    assert!(matches!(
        classify(&Cube::from_occ(occ)).class,
        Classification::Unclassified(_)
    ));
}

#[test]
fn parity_state_leaves_parity_residue_and_classifies() {
    let mut occ = identity();
    apply_residue(
        &mut occ,
        &Residue {
            parity: Some(Parity {
                edge: (26, 24),
                corner: (2, 1),
            }),
            ..Residue::default()
        },
    );
    assert!(occ_is_legal(&occ));
    let m = memo_from_array(&occ);
    assert_eq!(m.edge_letters, vec!['A']);
    assert_eq!(m.corner_letters, vec!['B']);
    assert!(m.pairs.0.is_empty() && m.pairs.1.is_empty());
    assert_eq!(
        m.residue.parity,
        Some(Parity {
            edge: (26, 24),
            corner: (2, 1)
        })
    );
    assert!(m.residue.flips.is_empty() && m.residue.twists.is_empty());
    assert_eq!(m.residue.class(), ResidueClass::Parity);
    assert_solves(&occ, &m);
    let report = classify(&Cube::from_occ(occ));
    assert_eq!(
        report.class,
        Classification::Parity {
            e1: 26,
            e2: 24,
            c1: 2,
            c2: 1
        }
    );
    assert_eq!(report.position().as_deref(), Some("UF-UB, UFR-UBR"));
    assert!(report.defects.flipped_pieces().is_empty());
    assert!(report.defects.twisted_pieces().is_empty());
}

#[test]
fn ltct_state_leaves_parity_and_outside_twists() {
    let mut occ = identity();
    apply_residue(
        &mut occ,
        &Residue {
            parity: Some(Parity {
                edge: (26, 24),
                corner: (2, 1),
            }),
            twists: vec![
                Twist {
                    corner: 2,
                    quarter: 1,
                },
                Twist {
                    corner: 3,
                    quarter: 2,
                },
            ],
            ..Residue::default()
        },
    );
    assert!(occ_is_legal(&occ));
    let m = memo_from_array(&occ);
    assert_eq!(m.edge_letters, vec!['A']);
    assert_eq!(m.corner_letters, vec!['Q']);
    assert!(m.pairs.0.is_empty() && m.pairs.1.is_empty());
    assert_eq!(
        m.residue.parity,
        Some(Parity {
            edge: (26, 24),
            corner: (2, 1)
        })
    );
    let mut got: Vec<(u8, u8)> = m
        .residue
        .twists
        .iter()
        .map(|t| (t.corner, t.quarter))
        .collect();
    got.sort_unstable();
    assert_eq!(got, vec![(1, 2), (3, 1)]);
    assert!(m.residue.flips.is_empty());
    assert_eq!(m.residue.class(), ResidueClass::Ltct);
    assert_solves(&occ, &m);
    let report = classify(&Cube::from_occ(occ));
    assert_eq!(
        report.class,
        Classification::Ltct {
            e1: 26,
            e2: 24,
            c1: 2,
            c2: 1,
            bracket: 5
        }
    );
    assert_eq!(report.position().as_deref(), Some("UF-UB, UFR-UBR[LUF]"));
    assert!(!report.defects.flip(26) && !report.defects.flip(24));
}

#[test]
fn flips_plus_twists_state_is_compound() {
    let mut occ = identity();
    apply_residue(
        &mut occ,
        &Residue {
            flips: vec![26, 24],
            twists: vec![
                Twist {
                    corner: 2,
                    quarter: 1,
                },
                Twist {
                    corner: 21,
                    quarter: 2,
                },
            ],
            ..Residue::default()
        },
    );
    assert!(occ_is_legal(&occ));
    let m = memo_from_array(&occ);
    assert!(m.edge_letters.is_empty() && m.corner_letters.is_empty());
    assert!(!m.residue.flips.is_empty() && !m.residue.twists.is_empty());
    assert_eq!(m.residue.class(), ResidueClass::Compound);
    assert_solves(&occ, &m);
}

#[test]
fn residue_class_covers_every_arm_from_first_principles() {
    assert_eq!(Residue::default().class(), ResidueClass::None);
    assert_eq!(
        Residue {
            flips: vec![26, 24],
            ..Residue::default()
        }
        .class(),
        ResidueClass::FlipPair(26, 24)
    );
    assert_eq!(
        Residue {
            flips: vec![26, 24, 46],
            ..Residue::default()
        }
        .class(),
        ResidueClass::Compound
    );
    assert_eq!(
        Residue {
            twists: vec![
                Twist {
                    corner: 2,
                    quarter: 1
                },
                Twist {
                    corner: 21,
                    quarter: 2
                }
            ],
            ..Residue::default()
        }
        .class(),
        ResidueClass::TwistPair(2, 21)
    );
    assert_eq!(
        Residue {
            twists: vec![
                Twist {
                    corner: 2,
                    quarter: 1
                },
                Twist {
                    corner: 21,
                    quarter: 1
                }
            ],
            ..Residue::default()
        }
        .class(),
        ResidueClass::Compound
    );
    assert_eq!(
        Residue {
            twists: vec![
                Twist {
                    corner: 2,
                    quarter: 1
                },
                Twist {
                    corner: 3,
                    quarter: 1
                },
                Twist {
                    corner: 21,
                    quarter: 1
                },
            ],
            ..Residue::default()
        }
        .class(),
        ResidueClass::TwistTriple(2, 3, 21)
    );
    assert_eq!(
        Residue {
            twists: vec![
                Twist {
                    corner: 2,
                    quarter: 2
                },
                Twist {
                    corner: 3,
                    quarter: 2
                }
            ],
            ..Residue::default()
        }
        .class(),
        ResidueClass::Compound
    );
    assert_eq!(
        Residue {
            flips: vec![26],
            twists: vec![Twist {
                corner: 2,
                quarter: 1
            }],
            ..Residue::default()
        }
        .class(),
        ResidueClass::Compound
    );
    let parity = Parity {
        edge: (26, 24),
        corner: (2, 1),
    };
    assert_eq!(
        Residue {
            parity: Some(parity),
            ..Residue::default()
        }
        .class(),
        ResidueClass::Parity
    );
    assert_eq!(
        Residue {
            parity: Some(parity),
            twists: vec![
                Twist {
                    corner: 2,
                    quarter: 1
                },
                Twist {
                    corner: 1,
                    quarter: 2
                }
            ],
            ..Residue::default()
        }
        .class(),
        ResidueClass::Parity
    );
    assert_eq!(
        Residue {
            parity: Some(parity),
            twists: vec![Twist {
                corner: 3,
                quarter: 1
            }],
            ..Residue::default()
        }
        .class(),
        ResidueClass::Ltct
    );
    assert_eq!(
        Residue {
            parity: Some(parity),
            flips: vec![26, 24],
            ..Residue::default()
        }
        .class(),
        ResidueClass::Compound
    );
}

#[test]
fn u_turn_position_stays_none_when_unclassified() {
    let report = classify(&after("U"));
    assert!(matches!(report.class, Classification::Unclassified(_)));
    assert_eq!(report.position(), None);
}

#[test]
fn random_legal_states_round_trip_with_monotonic_buffers() {
    let edge_heads = bt_scheme::edge_floating_heads();
    let corner_heads = bt_scheme::corner_floating_heads();
    for seed in 0..400u64 {
        let occ = random_occ(seed);
        assert!(occ_is_legal(&occ), "seed {seed}");
        let m = memo_from_array(&occ);
        assert_solves(&occ, &m);
        assert_eq!(m.pairs.0.len(), m.buffers.0.len(), "seed {seed}");
        assert_eq!(m.pairs.1.len(), m.buffers.1.len(), "seed {seed}");
        let edge_idx: Vec<usize> = m
            .buffers
            .0
            .iter()
            .map(|&h| edge_heads.iter().position(|&e| e == h).unwrap())
            .collect();
        assert!(
            edge_idx.windows(2).all(|w| w[0] <= w[1]),
            "seed {seed} buffers {:?}",
            m.buffers.0
        );
        let corner_idx: Vec<usize> = m
            .buffers
            .1
            .iter()
            .map(|&h| corner_heads.iter().position(|&c| c == h).unwrap())
            .collect();
        assert!(
            corner_idx.windows(2).all(|w| w[0] <= w[1]),
            "seed {seed} buffers {:?}",
            m.buffers.1
        );
        for (letters, pairs) in [
            (&m.edge_letters, &m.pairs.0),
            (&m.corner_letters, &m.pairs.1),
        ] {
            let slack = letters.len() - 2 * pairs.len();
            assert!(
                slack <= 1,
                "seed {seed} letters {letters:?} pairs {pairs:?}"
            );
        }
    }
}

#[test]
fn letters_follow_arrival_order_at_each_buffer() {
    let mut occ = identity();
    apply_sticker_cycles(&mut occ, &ideal_cycle_at(Orbit::Edge, 26, 24, 25));
    apply_sticker_cycles(&mut occ, &ideal_cycle_at(Orbit::Edge, 33, 35, 44));
    let m = memo_from_array(&occ);
    for case in m.pairs.0.iter().chain(m.pairs.1.iter()) {
        assert_eq!(case.letters(), (letter_of(case.t1), letter_of(case.t2)));
    }
    assert_eq!(
        m.pairs.0.iter().map(|c| c.letters()).collect::<Vec<_>>(),
        vec![('B', 'A'), ('U', 'L')]
    );
}
