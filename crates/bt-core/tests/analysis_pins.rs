mod common;

use bt_core::{
    Alg, Cube, Piece, corner_stickers, corner_twist, edge_flip, head_sticker, piece_of,
    sticker_cycles, summarize,
};
use proptest::prelude::*;

fn apply(text: &str) -> Cube {
    let mut cube = Cube::solved();
    cube.apply_alg(&Alg::parse(text).unwrap());
    cube
}

#[test]
fn piece_of_matches_position_map_tsv_for_all_stickers() {
    let names = common::position_names();
    let mut corner_pieces = [false; 8];
    let mut edge_pieces = [false; 12];
    for sticker in 0..48u8 {
        match piece_of(sticker) {
            Piece::Corner(piece) => {
                assert!(sticker < 24, "sticker {} is not a corner", sticker);
                assert_eq!(
                    piece, names.piece_of_sticker[sticker as usize],
                    "sticker {}",
                    sticker
                );
                corner_pieces[piece as usize] = true;
            }
            Piece::Edge(piece) => {
                assert!(
                    (24..48).contains(&sticker),
                    "sticker {} is not an edge",
                    sticker
                );
                assert_eq!(
                    piece, names.piece_of_sticker[sticker as usize],
                    "sticker {}",
                    sticker
                );
                edge_pieces[piece as usize] = true;
            }
        }
    }
    assert!(corner_pieces.iter().all(|&seen| seen));
    assert!(edge_pieces.iter().all(|&seen| seen));
}

#[test]
fn head_sticker_follows_ud_else_fb_rule() {
    let names = common::position_names();
    for (idx, piece) in names.corner_stickers().into_iter().enumerate() {
        let head = head_sticker(Piece::Corner(idx as u8));
        assert_expected_head(&names, &piece, head);
    }
    for (idx, piece) in names.edge_stickers().into_iter().enumerate() {
        let head = head_sticker(Piece::Edge(idx as u8));
        assert_expected_head(&names, &piece, head);
    }
}

fn assert_expected_head(names: &common::NameIndex, piece: &[u8], head: u8) {
    let face_of = |s: u8| names.name_of_sticker[s as usize].chars().next().unwrap();
    let ud: Vec<u8> = piece
        .iter()
        .copied()
        .filter(|&s| face_of(s) == 'U' || face_of(s) == 'D')
        .collect();
    let expected = if ud.is_empty() {
        let fb: Vec<u8> = piece
            .iter()
            .copied()
            .filter(|&s| face_of(s) == 'F' || face_of(s) == 'B')
            .collect();
        assert_eq!(fb.len(), 1, "piece {:?}", piece);
        fb[0]
    } else {
        assert_eq!(ud.len(), 1, "piece {:?}", piece);
        ud[0]
    };
    assert!(piece.contains(&head), "piece {:?}", piece);
    assert_eq!(head, expected, "piece {:?}", piece);
}

#[test]
fn head_sticker_names_are_canonical_piece_names() {
    let names = common::position_names();
    let pins = [
        (0u8, "UBR"),
        (1, "UBL"),
        (2, "UFR"),
        (3, "UFL"),
        (4, "DBR"),
        (5, "DBL"),
        (6, "DFR"),
        (7, "DFL"),
    ];
    for (piece, name) in pins {
        assert_eq!(
            names.name_of_sticker[head_sticker(Piece::Corner(piece)) as usize],
            name
        );
    }
    let pins = [
        (0u8, "UB"),
        (2, "UF"),
        (4, "BL"),
        (5, "BR"),
        (6, "FR"),
        (7, "FL"),
        (8, "DB"),
        (11, "DL"),
    ];
    for (piece, name) in pins {
        assert_eq!(
            names.name_of_sticker[head_sticker(Piece::Edge(piece)) as usize],
            name
        );
    }
}

#[test]
fn sticker_cycles_on_solved_is_empty() {
    assert!(sticker_cycles(&Cube::solved()).is_empty());
}

#[test]
fn sticker_cycles_single_turn_pins() {
    assert_eq!(
        sticker_cycles(&apply("U")),
        vec![
            vec![0, 1, 2, 3],
            vec![4, 16, 12, 8],
            vec![5, 17, 13, 9],
            vec![24, 25, 26, 27],
            vec![28, 40, 36, 32],
        ]
    );
    assert_eq!(
        sticker_cycles(&apply("U'")),
        vec![
            vec![0, 3, 2, 1],
            vec![4, 8, 12, 16],
            vec![5, 9, 13, 17],
            vec![24, 27, 26, 25],
            vec![28, 32, 36, 40],
        ]
    );
    assert_eq!(
        sticker_cycles(&apply("D2")),
        vec![
            vec![6, 14],
            vec![7, 15],
            vec![10, 18],
            vec![11, 19],
            vec![20, 22],
            vec![21, 23],
            vec![30, 38],
            vec![34, 42],
            vec![44, 46],
            vec![45, 47],
        ]
    );
}

#[test]
fn sticker_cycles_rotation_pin_and_coverage() {
    for text in ["x", "y", "z", "R U R' U'", "M2", "S'"] {
        let cube = apply(text);
        let cycles = sticker_cycles(&cube);
        let mut seen = [false; 48];
        for cycle in &cycles {
            assert!(cycle.len() > 1, "{}: fixed point {:?}", text, cycle);
            let min = *cycle.iter().min().unwrap();
            assert_eq!(cycle[0], min, "{}: not rotated to smallest", text);
            for &s in cycle.iter() {
                assert!(s < 48);
                assert!(!seen[s as usize], "{}: sticker {} twice", text, s);
                seen[s as usize] = true;
            }
            for i in 0..cycle.len() {
                let next = cycle[(i + 1) % cycle.len()];
                assert_eq!(
                    cube.position_of(cycle[i]),
                    next,
                    "{}: step {} -> {}",
                    text,
                    cycle[i],
                    next
                );
            }
        }
        for cycle in cycles.windows(2) {
            assert!(cycle[0][0] < cycle[1][0], "{}: cycles unsorted", text);
        }
        if text == "x" {
            assert!(cycles.contains(&vec![26, 40, 46, 34]));
        }
        if text == "y" {
            assert!(cycles.contains(&vec![24, 25, 26, 27]));
            assert!(cycles.contains(&vec![30, 42, 38, 34]));
        }
    }
}

#[test]
fn solved_state_has_no_flips_or_twists() {
    let cube = Cube::solved();
    for edge in 0..12u8 {
        assert!(!edge_flip(&cube, edge), "edge {}", edge);
    }
    for corner in 0..8u8 {
        assert_eq!(corner_twist(&cube, corner), 0, "corner {}", corner);
    }
}

fn flipped_set(cube: &Cube) -> Vec<u8> {
    (0..12u8).filter(|&e| edge_flip(cube, e)).collect()
}

fn twist_map(cube: &Cube) -> [i8; 8] {
    std::array::from_fn(|c| corner_twist(cube, c as u8))
}

#[test]
fn fb_quarter_turns_flip_their_four_edges() {
    assert_eq!(flipped_set(&apply("F")), vec![2, 6, 7, 10]);
    assert_eq!(flipped_set(&apply("F'")), vec![2, 6, 7, 10]);
    assert_eq!(flipped_set(&apply("B")), vec![0, 4, 5, 8]);
    assert_eq!(flipped_set(&apply("B'")), vec![0, 4, 5, 8]);
    assert!(flipped_set(&apply("F2")).is_empty());
    assert!(flipped_set(&apply("B2")).is_empty());
}

#[test]
fn udlr_mes_and_wide_lrud_flip_none() {
    for text in [
        "U", "U'", "U2", "D", "D'", "D2", "L", "L'", "L2", "R", "R'", "R2", "M", "M'", "M2", "E",
        "E'", "E2", "S2", "r", "r'", "r2", "l", "l'", "l2", "u", "u'", "u2", "d", "d'", "d2",
    ] {
        assert!(flipped_set(&apply(text)).is_empty(), "{}", text);
    }
}

#[test]
fn s_flips_the_eight_fb_axis_edge_slots_center_relative() {
    assert_eq!(flipped_set(&apply("S")), vec![0, 2, 4, 5, 6, 7, 8, 10]);
    assert_eq!(flipped_set(&apply("S'")), vec![0, 2, 4, 5, 6, 7, 8, 10]);
}

#[test]
fn wide_fb_flip_the_back_four_center_relative() {
    assert_eq!(flipped_set(&apply("f")), vec![0, 4, 5, 8]);
    assert_eq!(flipped_set(&apply("f'")), vec![0, 4, 5, 8]);
    assert_eq!(flipped_set(&apply("b")), vec![2, 6, 7, 10]);
    assert_eq!(flipped_set(&apply("b'")), vec![2, 6, 7, 10]);
}

#[test]
fn rotations_flip_none_center_relative() {
    for text in ["x", "x'", "x2", "y", "y'", "y2", "z", "z'", "z2"] {
        assert!(flipped_set(&apply(text)).is_empty(), "{}", text);
    }
}

#[test]
fn ud_turns_twist_no_corners() {
    for text in ["U", "U'", "U2", "D", "D'", "D2"] {
        assert_eq!(twist_map(&apply(text)), [0i8; 8], "{}", text);
    }
}

#[test]
fn lrfb_quarter_turns_twist_their_four_corners_with_sum_zero() {
    for (text, face) in [
        ("R", 3usize),
        ("R'", 3),
        ("L", 1),
        ("L'", 1),
        ("F", 2),
        ("F'", 2),
        ("B", 4),
        ("B'", 4),
    ] {
        let cube = apply(text);
        let twists = twist_map(&cube);
        let mut sum = 0i32;
        for (slot, &tw) in twists.iter().enumerate() {
            let on_face = corner_stickers(slot as u8)
                .iter()
                .any(|&s| (s / 4) as usize == face);
            if on_face {
                assert_ne!(tw, 0, "{} slot {}", text, slot);
                assert!(tw == 1 || tw == -1, "{} slot {}", text, slot);
            } else {
                assert_eq!(tw, 0, "{} slot {}", text, slot);
            }
            sum += tw as i32;
        }
        assert_eq!(sum.rem_euclid(3), 0, "{}", text);
    }
}

#[test]
fn rotations_preserve_all_twists() {
    for text in ["x", "x'", "x2", "y", "y'", "y2", "z", "z'", "z2"] {
        assert_eq!(twist_map(&apply(text)), [0i8; 8], "{}", text);
    }
}

#[test]
fn sexy_move_returns_twists_to_zero_at_order_six() {
    let alg = Alg::parse("R U R' U'").unwrap();
    for k in 1..=6u32 {
        let mut cube = Cube::solved();
        for _ in 0..k {
            cube.apply_alg(&alg);
        }
        if k == 6 {
            assert_eq!(twist_map(&cube), [0i8; 8]);
            assert!(flipped_set(&cube).is_empty());
            assert!(cube.is_solved());
        } else {
            assert!(!cube.is_solved(), "k {}", k);
        }
    }
}

proptest! {
    #[test]
    fn random_algs_keep_flip_parity_even_and_twist_sum_zero(
        ref text in common::alg_text(),
    ) {
        let alg = Alg::parse(text).unwrap();
        let mut cube = Cube::solved();
        cube.apply_alg(&alg);
        let summary = summarize(&cube);
        let flips = summary.edges.iter().filter(|edge| edge.flipped).count();
        assert_eq!(flips % 2, 0, "{}", alg.format());
        let twist_sum: i32 = summary.corners.iter().map(|c| c.twist as i32).sum();
        assert_eq!(twist_sum.rem_euclid(3), 0, "{}", alg.format());
    }
}
