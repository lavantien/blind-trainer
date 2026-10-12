use std::path::PathBuf;

use bt_core::{Alg, Piece, corner_stickers, edge_stickers, head_sticker};
use bt_scheme::{
    Classification, Orbit, Parity, Residue, ResidueClass, Twist, apply_memo, apply_residue,
    classify, classify_alg, memo_from_array, occ_is_legal, piece_perm, random_state, state_array,
    twist_sum,
};

fn identity_occ() -> [u8; 54] {
    std::array::from_fn(|i| i as u8)
}

fn simcheck_rows() -> Vec<(String, String, String)> {
    let raw =
        std::env::var("BT_REF_GROUND").expect("BT_REF_GROUND is unset: run tests through make");
    let base = PathBuf::from(raw);
    let base = if base.is_absolute() {
        base
    } else {
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("../..")
            .join(base)
    };
    let text = std::fs::read_to_string(base.join("rows.tsv")).expect("rows.tsv readable");
    let mut rows = Vec::new();
    for line in text.lines().skip(1).filter(|l| !l.trim().is_empty()) {
        let cols: Vec<&str> = line.split('\t').collect();
        assert_eq!(cols.len(), 5, "rows row width: {line}");
        rows.push((
            cols[1].to_string(),
            cols[4].to_string(),
            cols[2].to_string(),
        ));
    }
    rows
}

fn build_occ(
    edge_perm: &[u8; 12],
    edge_flip: &[bool; 12],
    corner_perm: &[u8; 8],
    corner_twist: &[u8; 8],
) -> [u8; 54] {
    let mut occ = identity_occ();
    for slot in 0..12usize {
        let [p0, p1] = edge_stickers(edge_perm[slot]);
        let [s0, s1] = edge_stickers(slot as u8);
        if edge_flip[slot] {
            occ[s0 as usize] = p1;
            occ[s1 as usize] = p0;
        } else {
            occ[s0 as usize] = p0;
            occ[s1 as usize] = p1;
        }
    }
    for slot in 0..8usize {
        let piece = corner_stickers(corner_perm[slot]);
        let positions = corner_stickers(slot as u8);
        let w = corner_twist[slot] as usize % 3;
        for j in 0..3 {
            occ[positions[(j + w) % 3] as usize] = piece[j];
        }
    }
    occ
}

fn swap_pair<const N: usize>(identity: [u8; N], a: usize, b: usize) -> [u8; N] {
    let mut perm = identity;
    perm.swap(a, b);
    perm
}

fn cycle_three<const N: usize>(mut perm: [u8; N], a: usize, b: usize, c: usize) -> [u8; N] {
    let (pa, pb, pc) = (perm[a], perm[b], perm[c]);
    perm[a] = pb;
    perm[b] = pc;
    perm[c] = pa;
    perm
}

fn residue(twists: Vec<Twist>, flips: Vec<u8>, parity: Option<Parity>) -> Residue {
    Residue {
        parity,
        flips,
        twists,
    }
}

fn cube_of(occ: [u8; 54]) -> bt_core::Cube {
    bt_core::Cube::from_occ(occ)
}

fn assert_round_trip(occ: [u8; 54]) {
    assert!(occ_is_legal(&occ), "test state must be legal");
    let memo = memo_from_array(&occ);
    let mut work = occ;
    apply_memo(&mut work, &memo);
    assert_eq!(work, identity_occ(), "memo must restore identity");
}

#[test]
fn classification_shapes_are_pinned() {
    let edges: [u8; 12] = std::array::from_fn(|i| i as u8);
    let corners: [u8; 8] = std::array::from_fn(|i| i as u8);
    let no_flips = [false; 12];
    let no_twists = [0u8; 8];

    let solved = classify(&cube_of(build_occ(&edges, &no_flips, &corners, &no_twists)));
    assert_eq!(solved.class, Classification::Solved);

    let edge_cycle = classify(&cube_of(build_occ(
        &cycle_three(edges, 5, 6, 7),
        &no_flips,
        &corners,
        &no_twists,
    )));
    let Classification::ThreeStyle { orbit, cases } = edge_cycle.class else {
        panic!("edge cycle must be three style: {:?}", edge_cycle.class)
    };
    assert_eq!(orbit, Orbit::Edge);
    assert_eq!(cases.len(), 1);

    let corner_cycle = classify(&cube_of(build_occ(
        &edges,
        &no_flips,
        &cycle_three(corners, 5, 6, 7),
        &no_twists,
    )));
    let Classification::ThreeStyle { orbit, cases } = corner_cycle.class else {
        panic!("corner cycle must be three style: {:?}", corner_cycle.class)
    };
    assert_eq!(orbit, Orbit::Corner);
    assert_eq!(cases.len(), 1);

    let both_orbits = classify(&cube_of(build_occ(
        &cycle_three(edges, 5, 6, 7),
        &no_flips,
        &cycle_three(corners, 5, 6, 7),
        &no_twists,
    )));
    assert!(
        matches!(both_orbits.class, Classification::Unclassified(_)),
        "{:?}",
        both_orbits.class
    );

    let mut single_flip = no_flips;
    single_flip[5] = true;
    let one_flip = classify(&cube_of(build_occ(
        &edges,
        &single_flip,
        &corners,
        &no_twists,
    )));
    assert!(
        matches!(one_flip.class, Classification::Unclassified(_)),
        "{:?}",
        one_flip.class
    );

    let mut flip_pair = no_flips;
    flip_pair[4] = true;
    flip_pair[7] = true;
    let flips = classify(&cube_of(build_occ(
        &edges, &flip_pair, &corners, &no_twists,
    )));
    let Classification::FlipPair { first, second } = flips.class else {
        panic!("two flips must be a flip pair: {:?}", flips.class)
    };
    assert!(
        (first, second) == (head_sticker(Piece::Edge(4)), head_sticker(Piece::Edge(7)))
            || (first, second) == (head_sticker(Piece::Edge(7)), head_sticker(Piece::Edge(4))),
        "{first} {second}"
    );

    let mut twist_state = identity_occ();
    apply_residue(
        &mut twist_state,
        &residue(
            vec![
                Twist {
                    corner: head_sticker(Piece::Corner(3)),
                    quarter: 1,
                },
                Twist {
                    corner: head_sticker(Piece::Corner(4)),
                    quarter: 2,
                },
            ],
            vec![],
            None,
        ),
    );
    let twists = classify(&cube_of(twist_state));
    assert_eq!(
        twists.class,
        Classification::TwistPair {
            first: head_sticker(Piece::Corner(3)),
            second: head_sticker(Piece::Corner(4)),
        }
    );

    let mut twist_with_edges_state = twist_state;
    let [ea0, ea1] = edge_stickers(3);
    let [eb0, eb1] = edge_stickers(4);
    twist_with_edges_state.swap(ea0 as usize, eb0 as usize);
    twist_with_edges_state.swap(ea1 as usize, eb1 as usize);
    let twist_with_edges = classify(&cube_of(twist_with_edges_state));
    assert!(
        matches!(twist_with_edges.class, Classification::Unclassified(_)),
        "{:?}",
        twist_with_edges.class
    );

    let mut parity_state = identity_occ();
    apply_residue(
        &mut parity_state,
        &residue(
            vec![],
            vec![],
            Some(Parity {
                edge: (head_sticker(Piece::Edge(3)), head_sticker(Piece::Edge(4))),
                corner: (
                    head_sticker(Piece::Corner(3)),
                    head_sticker(Piece::Corner(4)),
                ),
            }),
        ),
    );
    let parity = classify(&cube_of(parity_state));
    assert_eq!(
        parity.class,
        Classification::Parity {
            e1: head_sticker(Piece::Edge(3)),
            e2: head_sticker(Piece::Edge(4)),
            c1: head_sticker(Piece::Corner(3)),
            c2: head_sticker(Piece::Corner(4)),
        }
    );

    let mut swap_plus_cycle = build_occ(
        &cycle_three(edges, 5, 6, 7),
        &no_flips,
        &corners,
        &no_twists,
    );
    let [a0, a1] = edge_stickers(3);
    let [b0, b1] = edge_stickers(4);
    swap_plus_cycle.swap(a0 as usize, b0 as usize);
    swap_plus_cycle.swap(a1 as usize, b1 as usize);
    let slot3 = corner_stickers(3);
    let slot4 = corner_stickers(4);
    let swap_plus_cycle_plus_corner_swap = {
        let mut occ = swap_plus_cycle;
        for i in 0..3 {
            occ.swap(slot3[i] as usize, slot4[i] as usize);
        }
        occ
    };
    let classified = classify(&cube_of(swap_plus_cycle_plus_corner_swap));
    assert!(
        matches!(classified.class, Classification::Unclassified(_)),
        "{:?}",
        classified.class
    );

    let swap_plus_corner_cycle = {
        let mut occ = build_occ(
            &edges,
            &no_flips,
            &cycle_three(corners, 5, 6, 7),
            &no_twists,
        );
        let [a0, a1] = edge_stickers(3);
        let [b0, b1] = edge_stickers(4);
        occ.swap(a0 as usize, b0 as usize);
        occ.swap(a1 as usize, b1 as usize);
        occ
    };
    let edge_swap_corner_cycle = classify(&cube_of(swap_plus_corner_cycle));
    assert!(
        matches!(
            edge_swap_corner_cycle.class,
            Classification::Unclassified(_)
        ),
        "{:?}",
        edge_swap_corner_cycle.class
    );

    let mut twisted_swap_state = identity_occ();
    apply_residue(
        &mut twisted_swap_state,
        &residue(
            vec![
                Twist {
                    corner: head_sticker(Piece::Corner(3)),
                    quarter: 1,
                },
                Twist {
                    corner: head_sticker(Piece::Corner(4)),
                    quarter: 2,
                },
            ],
            vec![],
            Some(Parity {
                edge: (head_sticker(Piece::Edge(3)), head_sticker(Piece::Edge(4))),
                corner: (
                    head_sticker(Piece::Corner(3)),
                    head_sticker(Piece::Corner(4)),
                ),
            }),
        ),
    );
    let twisted_swap = classify(&cube_of(twisted_swap_state));
    assert!(
        matches!(twisted_swap.class, Classification::Parity { .. }),
        "{:?}",
        twisted_swap.class
    );
    assert_eq!(twisted_swap.defects.twisted_pieces().len(), 2);

    let mut edge_swap_twist_pair = identity_occ();
    let [pa0, pa1] = edge_stickers(3);
    let [pb0, pb1] = edge_stickers(4);
    edge_swap_twist_pair.swap(pa0 as usize, pb0 as usize);
    edge_swap_twist_pair.swap(pa1 as usize, pb1 as usize);
    apply_residue(
        &mut edge_swap_twist_pair,
        &residue(
            vec![
                Twist {
                    corner: head_sticker(Piece::Corner(3)),
                    quarter: 1,
                },
                Twist {
                    corner: head_sticker(Piece::Corner(4)),
                    quarter: 2,
                },
            ],
            vec![],
            None,
        ),
    );
    let swap_with_twists = classify(&cube_of(edge_swap_twist_pair));
    assert!(
        matches!(swap_with_twists.class, Classification::Unclassified(_)),
        "{:?}",
        swap_with_twists.class
    );
}

#[test]
fn defects_list_flipped_and_twisted_pieces() {
    let edges: [u8; 12] = std::array::from_fn(|i| i as u8);
    let corners: [u8; 8] = std::array::from_fn(|i| i as u8);
    let mut flips = [false; 12];
    flips[4] = true;
    flips[7] = true;
    let report = classify(&cube_of(build_occ(&edges, &flips, &corners, &[0u8; 8])));
    assert_eq!(
        report.defects.flipped_pieces(),
        vec![head_sticker(Piece::Edge(4)), head_sticker(Piece::Edge(7))]
    );

    let mut twisted = identity_occ();
    apply_residue(
        &mut twisted,
        &residue(
            vec![
                Twist {
                    corner: head_sticker(Piece::Corner(3)),
                    quarter: 1,
                },
                Twist {
                    corner: head_sticker(Piece::Corner(4)),
                    quarter: 2,
                },
            ],
            vec![],
            None,
        ),
    );
    let twisted_report = classify(&cube_of(twisted));
    let twisted_pieces = twisted_report.defects.twisted_pieces();
    assert_eq!(twisted_pieces.len(), 2);
    assert!(
        twisted_pieces
            .iter()
            .any(|&(head, _)| head == head_sticker(Piece::Corner(3)))
    );
    assert!(
        twisted_pieces
            .iter()
            .any(|&(head, _)| head == head_sticker(Piece::Corner(4)))
    );
}

#[test]
fn residue_classes_are_pinned() {
    let c3 = head_sticker(Piece::Corner(3));
    let c4 = head_sticker(Piece::Corner(4));
    let c5 = head_sticker(Piece::Corner(5));
    let e3 = head_sticker(Piece::Edge(3));
    let e4 = head_sticker(Piece::Edge(4));

    assert_eq!(Residue::default().class(), ResidueClass::None);

    assert_eq!(
        residue(vec![], vec![e3, e4], None).class(),
        ResidueClass::FlipPair(e3, e4)
    );
    assert_eq!(
        residue(vec![], vec![e3, e4, head_sticker(Piece::Edge(5))], None).class(),
        ResidueClass::Compound
    );

    assert_eq!(
        residue(
            vec![
                Twist {
                    corner: c3,
                    quarter: 1
                },
                Twist {
                    corner: c4,
                    quarter: 2
                }
            ],
            vec![],
            None
        )
        .class(),
        ResidueClass::TwistPair(c3, c4)
    );
    assert_eq!(
        residue(
            vec![
                Twist {
                    corner: c3,
                    quarter: 1
                },
                Twist {
                    corner: c4,
                    quarter: 1
                }
            ],
            vec![],
            None
        )
        .class(),
        ResidueClass::Compound
    );
    assert_eq!(
        residue(
            vec![
                Twist {
                    corner: c3,
                    quarter: 1
                },
                Twist {
                    corner: c4,
                    quarter: 1
                },
                Twist {
                    corner: c5,
                    quarter: 1
                },
            ],
            vec![],
            None
        )
        .class(),
        ResidueClass::TwistTriple(c3, c4, c5)
    );
    assert_eq!(
        residue(
            vec![
                Twist {
                    corner: c3,
                    quarter: 1
                },
                Twist {
                    corner: c4,
                    quarter: 1
                },
                Twist {
                    corner: c5,
                    quarter: 2
                },
            ],
            vec![],
            None
        )
        .class(),
        ResidueClass::Compound
    );

    let parity = Parity {
        edge: (e3, e4),
        corner: (c3, c4),
    };
    assert_eq!(
        residue(vec![], vec![], Some(parity)).class(),
        ResidueClass::Parity
    );
    assert_eq!(
        residue(vec![], vec![e3, e4], Some(parity)).class(),
        ResidueClass::Compound
    );
    assert_eq!(
        residue(
            vec![
                Twist {
                    corner: c3,
                    quarter: 1
                },
                Twist {
                    corner: c4,
                    quarter: 2
                }
            ],
            vec![],
            Some(parity)
        )
        .class(),
        ResidueClass::Parity
    );
    assert_eq!(
        residue(
            vec![
                Twist {
                    corner: c5,
                    quarter: 1
                },
                Twist {
                    corner: c3,
                    quarter: 2
                }
            ],
            vec![],
            Some(parity)
        )
        .class(),
        ResidueClass::Ltct
    );
}

#[test]
fn scramble_legality_helpers_are_pinned() {
    let edges: [u8; 12] = std::array::from_fn(|i| i as u8);
    let corners: [u8; 8] = std::array::from_fn(|i| i as u8);

    let perm_state = build_occ(
        &cycle_three(swap_pair(edges, 0, 9), 3, 4, 5),
        &[false; 12],
        &cycle_three(corners, 1, 6, 7),
        &[0u8; 8],
    );
    let edge_perm = piece_perm(&perm_state, Orbit::Edge).unwrap();
    assert_eq!(
        edge_perm,
        vec![
            edges[9], edges[1], edges[2], edges[4], edges[5], edges[3], edges[6], edges[7],
            edges[8], edges[0], edges[10], edges[11]
        ]
    );
    let corner_perm = piece_perm(&perm_state, Orbit::Corner).unwrap();
    assert_eq!(
        corner_perm,
        vec![
            corners[0], corners[6], corners[2], corners[3], corners[4], corners[5], corners[7],
            corners[1]
        ]
    );

    let mut edge_swap_only = build_occ(&edges, &[false; 12], &corners, &[0u8; 8]);
    let [a0, a1] = edge_stickers(3);
    let [b0, b1] = edge_stickers(4);
    edge_swap_only.swap(a0 as usize, b0 as usize);
    edge_swap_only.swap(a1 as usize, b1 as usize);
    assert!(!occ_is_legal(&edge_swap_only), "parity mismatch must fail");

    let mut twisted = identity_occ();
    apply_residue(
        &mut twisted,
        &residue(
            vec![Twist {
                corner: head_sticker(Piece::Corner(5)),
                quarter: 1,
            }],
            vec![],
            None,
        ),
    );
    assert_eq!(twist_sum(&identity_occ()), 0);
    assert_ne!(twist_sum(&twisted) % 3, 0);
    assert!(!occ_is_legal(&twisted), "twist sum must vanish mod 3");

    let mut duplicate = identity_occ();
    duplicate[11] = duplicate[10];
    assert!(!occ_is_legal(&duplicate), "duplicate sticker must fail");

    let mut out_of_range = identity_occ();
    out_of_range[11] = 60;
    assert!(!occ_is_legal(&out_of_range), "sticker above 53 must fail");

    let mut non_cohesive = identity_occ();
    let slot3 = corner_stickers(3);
    let slot4 = corner_stickers(4);
    non_cohesive.swap(slot3[0] as usize, slot4[1] as usize);
    assert!(!occ_is_legal(&non_cohesive), "split corner piece must fail");

    let mut mixed_type = identity_occ();
    mixed_type.swap(slot3[0] as usize, 24);
    assert!(piece_perm(&mixed_type, Orbit::Edge).is_none());
    assert!(piece_perm(&mixed_type, Orbit::Corner).is_none());
}

#[test]
fn scramble_edge_sampling_stays_uniform() {
    let mut counts = [0u32; 12];
    for seed in 0..20000u64 {
        let occ = state_array(&random_state(seed));
        let perm = piece_perm(&occ, Orbit::Edge).unwrap();
        counts[perm[0] as usize] += 1;
    }
    for (piece, &count) in counts.iter().enumerate() {
        assert!(
            (1200..2200).contains(&count),
            "edge piece {piece} landed in slot 0 {count} times"
        );
    }
}

#[test]
fn memo_restores_synthetic_legal_states() {
    let edges: [u8; 12] = std::array::from_fn(|i| i as u8);
    let corners: [u8; 8] = std::array::from_fn(|i| i as u8);
    let mut flips = [false; 12];
    flips[4] = true;
    flips[7] = true;

    assert_round_trip(build_occ(&edges, &[false; 12], &corners, &[0u8; 8]));
    assert_round_trip(build_occ(
        &cycle_three(edges, 5, 6, 7),
        &[false; 12],
        &corners,
        &[0u8; 8],
    ));
    assert_round_trip(build_occ(
        &cycle_three(edges, 2, 5, 9),
        &[false; 12],
        &corners,
        &[0u8; 8],
    ));
    assert_round_trip(build_occ(&edges, &flips, &corners, &[0u8; 8]));

    let mut twist_pair = identity_occ();
    apply_residue(
        &mut twist_pair,
        &residue(
            vec![
                Twist {
                    corner: head_sticker(Piece::Corner(3)),
                    quarter: 1,
                },
                Twist {
                    corner: head_sticker(Piece::Corner(4)),
                    quarter: 2,
                },
            ],
            vec![],
            None,
        ),
    );
    assert_round_trip(twist_pair);

    let mut parity_state = identity_occ();
    apply_residue(
        &mut parity_state,
        &residue(
            vec![],
            vec![],
            Some(Parity {
                edge: (head_sticker(Piece::Edge(3)), head_sticker(Piece::Edge(4))),
                corner: (
                    head_sticker(Piece::Corner(3)),
                    head_sticker(Piece::Corner(4)),
                ),
            }),
        ),
    );
    assert_round_trip(parity_state);

    let mut ltct_state = identity_occ();
    apply_residue(
        &mut ltct_state,
        &residue(
            vec![
                Twist {
                    corner: head_sticker(Piece::Corner(5)),
                    quarter: 1,
                },
                Twist {
                    corner: head_sticker(Piece::Corner(3)),
                    quarter: 2,
                },
            ],
            vec![],
            Some(Parity {
                edge: (head_sticker(Piece::Edge(3)), head_sticker(Piece::Edge(4))),
                corner: (
                    head_sticker(Piece::Corner(3)),
                    head_sticker(Piece::Corner(4)),
                ),
            }),
        ),
    );
    assert_round_trip(ltct_state);

    let mut mixed = build_occ(&cycle_three(edges, 2, 5, 9), &flips, &corners, &[0u8; 8]);
    apply_residue(
        &mut mixed,
        &residue(
            vec![],
            vec![],
            Some(Parity {
                edge: (head_sticker(Piece::Edge(3)), head_sticker(Piece::Edge(4))),
                corner: (
                    head_sticker(Piece::Corner(3)),
                    head_sticker(Piece::Corner(4)),
                ),
            }),
        ),
    );
    assert_round_trip(mixed);

    for seed in 0..400u64 {
        let occ = state_array(&random_state(seed));
        let memo = memo_from_array(&occ);
        let mut work = occ;
        apply_memo(&mut work, &memo);
        assert_eq!(work, identity_occ(), "seed {seed}");
    }
}

#[test]
fn vendor_twist_pairs_are_opposite() {
    let mut twist_rows = 0;
    for (sheet, moves, rowid) in simcheck_rows() {
        let alg = Alg::parse(&moves).expect("simcheck alg parses");
        let report = classify_alg(&alg);
        let Classification::TwistPair { first, second } = report.class else {
            if sheet == "twists" {
                panic!("twists row {rowid} classified {:?}", report.class);
            }
            continue;
        };
        let t1 = report.defects.twist(first).rem_euclid(3);
        let t2 = report.defects.twist(second).rem_euclid(3);
        if sheet == "twists" {
            assert_eq!((t1 + t2) % 3, 0, "twists row {rowid} must be opposite");
            twist_rows += 1;
        }
        assert_ne!(
            (t1, t2),
            (1, 1),
            "tie pair reached on row {rowid} sheet {sheet}"
        );
        assert_ne!(
            (t1, t2),
            (2, 2),
            "tie pair reached on row {rowid} sheet {sheet}"
        );
    }
    assert!(twist_rows >= 20, "expected the vendor twists sheet");
}
