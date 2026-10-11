mod common;

use bt_core::{Alg, Cube, Piece, head_sticker, piece_of, sticker_cycles};

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
        let mut seen = vec![false; 48];
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
