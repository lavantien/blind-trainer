use bt_core::{
    Alg, Axis, Cube, Move, corner_stickers, corner_twist, edge_flip, edge_stickers, piece_of,
};

fn identity_occ() -> [u8; 54] {
    std::array::from_fn(|i| i as u8)
}

fn all_corners_twisted(w: usize) -> [u8; 54] {
    let mut occ = identity_occ();
    for k in 0..8u8 {
        let triple = corner_stickers(k);
        for j in 0..3 {
            occ[triple[(j + w) % 3] as usize] = triple[j];
        }
    }
    occ
}

fn all_edges_flipped() -> [u8; 54] {
    let mut occ = identity_occ();
    for e in 0..12u8 {
        let [a, b] = edge_stickers(e);
        occ[a as usize] = b;
        occ[b as usize] = a;
    }
    occ
}

fn single_corner_twisted(k: u8, w: usize) -> [u8; 54] {
    let mut occ = identity_occ();
    let triple = corner_stickers(k);
    for j in 0..3 {
        occ[triple[(j + w) % 3] as usize] = triple[j];
    }
    occ
}

fn single_edge_flipped(e: u8) -> [u8; 54] {
    let mut occ = identity_occ();
    let [a, b] = edge_stickers(e);
    occ[a as usize] = b;
    occ[b as usize] = a;
    occ
}

fn states() -> Vec<Cube> {
    let mut out = vec![Cube::solved()];
    for text in [
        "R U R' F2",
        "M E S",
        "R U",
        "F B' D2",
        "R U R' U' R U R' U'",
        "2B l' x S2",
    ] {
        let mut cube = Cube::solved();
        cube.apply_alg(&Alg::parse(text).unwrap());
        out.push(cube);
    }
    for occ in [
        all_corners_twisted(1),
        all_corners_twisted(2),
        all_edges_flipped(),
        single_corner_twisted(0, 1),
        single_corner_twisted(5, 2),
        single_edge_flipped(3),
        single_edge_flipped(9),
    ] {
        out.push(Cube::from_occ(occ));
    }
    out
}

fn rotations() -> Vec<(Move, [u8; 54])> {
    let mut out = Vec::new();
    for axis in [Axis::X, Axis::Y, Axis::Z] {
        for quarter in 1..=3u8 {
            let mv = Move::Rotation { axis, quarter };
            out.push((mv, mv.table()));
        }
    }
    out
}

fn edge_slot_of(sticker: u8) -> u8 {
    match piece_of(sticker) {
        bt_core::Piece::Edge(e) => e,
        _ => panic!("not an edge sticker {sticker}"),
    }
}

fn corner_slot_of(sticker: u8) -> u8 {
    match piece_of(sticker) {
        bt_core::Piece::Corner(c) => c,
        _ => panic!("not a corner sticker {sticker}"),
    }
}

#[test]
fn is_empty_tracks_len() {
    assert!(Alg::parse("").unwrap().is_empty());
    assert_eq!(Alg::parse("").unwrap().len(), 0);
    let nonempty = Alg::parse("U").unwrap();
    assert!(!nonempty.is_empty());
    assert_eq!(nonempty.len(), 1);
    let many = Alg::parse("R U r' 2F M2 x").unwrap();
    assert!(!many.is_empty());
    assert_eq!(many.len(), many.moves().len());
}

#[test]
fn orientation_is_rotation_invariant() {
    for state in states() {
        for (rotation, table) in rotations() {
            let mut rotated = state;
            rotated.apply(rotation);
            for e in 0..12u8 {
                let [a, _] = edge_stickers(e);
                let image = edge_slot_of(table[a as usize]);
                assert_eq!(
                    edge_flip(&rotated, image),
                    edge_flip(&state, e),
                    "edge {e} under {:?}",
                    rotation
                );
            }
            for k in 0..8u8 {
                let triple = corner_stickers(k);
                let image = corner_slot_of(table[triple[0] as usize]);
                assert_eq!(
                    corner_twist(&rotated, image),
                    corner_twist(&state, k),
                    "corner {k} under {:?}",
                    rotation
                );
            }
        }
    }
}
