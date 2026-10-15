use bt_puzzles::fto::{Alg, FACE_NAMES, FACELET_COUNT, Fto, Move};
use bt_puzzles::{Geometry, ParseError, verify_tier1};
use proptest::prelude::*;
use std::path::Path;

fn pack_rows() -> Vec<(String, String)> {
    let path =
        Path::new(env!("CARGO_MANIFEST_DIR")).join("../../ref_ground/packs/fto_l4t_redux.tsv");
    let text =
        std::fs::read_to_string(&path).unwrap_or_else(|err| panic!("pack unreadable: {err}"));
    text.lines()
        .skip(1)
        .filter(|line| !line.trim().is_empty())
        .map(|line| {
            let cols: Vec<&str> = line.trim_end_matches('\r').split('\t').collect();
            (cols[0].to_string(), cols[1].to_string())
        })
        .collect()
}

#[test]
fn adjacency_closes_over_the_octahedron() {
    let fto = Fto::new();
    let expected: [[u8; 3]; 8] = [
        [1, 3, 5],
        [0, 2, 6],
        [1, 3, 7],
        [0, 2, 4],
        [3, 5, 7],
        [0, 4, 6],
        [1, 5, 7],
        [2, 4, 6],
    ];
    for face in 0..8u8 {
        assert_eq!(fto.neighbors(face), expected[face as usize]);
        for neighbor in fto.neighbors(face) {
            assert!(fto.neighbors(neighbor).contains(&face));
            assert_ne!(fto.opposite(face), neighbor);
        }
    }
    for (face, far) in [(0u8, 7u8), (1, 4), (2, 5), (3, 6)] {
        assert_eq!(fto.opposite(face), far);
        assert_eq!(fto.opposite(far), face);
    }
}

#[test]
fn every_turn_is_a_bijective_order_three_seven_three_cycle_permutation() {
    let fto = Fto::new();
    for face in 0..8u8 {
        for quarter in [1u8, 2u8] {
            let table = fto.turn_table(face, quarter);
            let mut sorted = table;
            sorted.sort();
            assert_eq!(sorted, core::array::from_fn(|i| i as u8));
            for i in 0..FACELET_COUNT {
                let double = table[table[i] as usize];
                assert_eq!(table[double as usize], i as u8);
            }
            let mut seen = [false; FACELET_COUNT];
            let (mut fixed, mut threes) = (0, 0);
            for i in 0..FACELET_COUNT {
                if seen[i] {
                    continue;
                }
                let mut len = 0;
                let mut j = i;
                while !seen[j] {
                    seen[j] = true;
                    j = table[j] as usize;
                    len += 1;
                }
                match len {
                    1 => fixed += 1,
                    3 => threes += 1,
                    other => panic!("face {face} quarter {quarter} cycle len {other}"),
                }
            }
            assert_eq!((fixed, threes), (51, 7));
        }
    }
}

#[test]
fn turn_recolors_only_neighbor_and_opposite_neighbor_faces() {
    let fto = Fto::new();
    for face in 0..8u8 {
        let far = fto.opposite(face);
        let mut landing = fto.neighbors(face).to_vec();
        landing.extend(fto.neighbors(far));
        for quarter in [1u8, 2u8] {
            let mut state = fto.solved_state();
            fto.apply_alg(
                &mut state,
                &Alg {
                    moves: vec![Move { face, quarter }],
                },
            )
            .unwrap();
            let mut changed = 0;
            for i in 0..FACELET_COUNT {
                let home = (i / 9) as u8;
                if state.facelets[i] != home {
                    changed += 1;
                    assert!(
                        landing.contains(&home),
                        "face {face} quarter {quarter} moved a sticker onto face {home}"
                    );
                }
            }
            for slot in 0..9 {
                assert_eq!(state.facelets[face as usize * 9 + slot], face);
                assert_eq!(state.facelets[far as usize * 9 + slot], far);
            }
            assert_eq!(changed, 12);
        }
    }
}

#[test]
fn opposite_face_turns_commute() {
    let fto = Fto::new();
    for (a, b) in [(0u8, 7u8), (1, 4), (2, 5), (3, 6)] {
        let a_name = FACE_NAMES[a as usize];
        let b_name = FACE_NAMES[b as usize];
        let mut ab = fto.solved_state();
        fto.apply_alg(&mut ab, &fto.parse_alg(a_name).unwrap())
            .unwrap();
        fto.apply_alg(&mut ab, &fto.parse_alg(b_name).unwrap())
            .unwrap();
        let mut ba = fto.solved_state();
        fto.apply_alg(&mut ba, &fto.parse_alg(b_name).unwrap())
            .unwrap();
        fto.apply_alg(&mut ba, &fto.parse_alg(a_name).unwrap())
            .unwrap();
        assert_eq!(ab, ba);
    }
}

#[test]
fn double_turn_token_equals_two_quarter_turns() {
    let fto = Fto::new();
    for face in FACE_NAMES {
        let mut doubled = fto.solved_state();
        fto.apply_alg(&mut doubled, &fto.parse_alg(face).unwrap())
            .unwrap();
        fto.apply_alg(&mut doubled, &fto.parse_alg(face).unwrap())
            .unwrap();
        let mut tokened = fto.solved_state();
        fto.apply_alg(&mut tokened, &fto.parse_alg(&format!("{face}2")).unwrap())
            .unwrap();
        assert_eq!(doubled, tokened, "{face}");
        let mut tripled = fto.solved_state();
        for _ in 0..3 {
            fto.apply_alg(&mut tripled, &fto.parse_alg(face).unwrap())
                .unwrap();
        }
        assert!(fto.is_solved(&tripled), "{face}");
    }
}

#[test]
fn br_parity_row_parses_and_applies() {
    let fto = Fto::new();
    let row = pack_rows()
        .into_iter()
        .find(|(key, _)| key == "l4t.36")
        .expect("l4t.36 row missing from fto_l4t_redux.tsv");
    assert_eq!(row.1, "U BR U BR' F U F'");
    assert!(fto.parse_move("BR").is_ok());
    assert!(fto.parse_move("BR'").is_ok());
    assert_eq!(verify_tier1(&fto, &row.1), Ok(()));
    let alg = fto.parse_alg(&row.1).unwrap();
    let mut state = fto.solved_state();
    fto.apply_alg(&mut state, &alg).unwrap();
    assert!(!fto.is_solved(&state));
}

#[test]
fn tier1_green_over_every_pack_row() {
    let fto = Fto::new();
    let rows = pack_rows();
    assert_eq!(rows.len(), 36);
    for (key, alg) in &rows {
        assert_eq!(verify_tier1(&fto, alg), Ok(()), "{key}: {alg}");
    }
}

#[test]
fn grammar_accepts_exactly_the_pinned_token_inventory() {
    let fto = Fto::new();
    for token in [
        "BR", "BR'", "F", "F'", "L", "L'", "L2", "L2'", "R", "R'", "R2'", "U", "U'",
    ] {
        assert!(fto.parse_move(token).is_ok(), "{token}");
    }
    for face in FACE_NAMES {
        for suffix in ["", "'", "2", "2'"] {
            assert!(fto.parse_move(&format!("{face}{suffix}")).is_ok());
        }
    }
    assert_eq!(fto.format_alg(&fto.parse_alg("L2").unwrap()), "L'");
    assert_eq!(fto.format_alg(&fto.parse_alg("L2'").unwrap()), "L");
    assert_eq!(fto.format_alg(&fto.parse_alg("R2'").unwrap()), "R");
    let alg = fto.parse_alg("U BR U BR' F U F'").unwrap();
    assert_eq!(fto.parse_alg(&fto.format_alg(&alg)).unwrap(), alg);
}

#[test]
fn rejects_garbage_and_cube_slice_tokens() {
    let fto = Fto::new();
    for token in [
        "M", "E", "S", "x", "y", "z", "u", "r", "Uw", "BRw", "3U", "U3", "U4", "UU", "'", "2",
        "U2''", "F2'2", "",
    ] {
        assert_eq!(
            fto.parse_move(token),
            Err(ParseError::UnknownToken(token.to_string())),
            "{token}"
        );
    }
    assert!(matches!(
        fto.parse_alg("U M").unwrap_err(),
        ParseError::UnknownToken(token) if token == "M"
    ));
}

#[test]
fn solved_grid_renders_eight_triforce_faces() {
    let fto = Fto::new();
    let grid = fto.facelets(&fto.solved_state());
    assert_eq!(grid.faces.len(), 8);
    for (face, grid_face) in grid.faces.iter().enumerate() {
        assert_eq!(
            grid_face
                .rows
                .iter()
                .map(|row| row.len())
                .collect::<Vec<_>>(),
            vec![1, 3, 5]
        );
        for row in &grid_face.rows {
            assert!(row.iter().all(|&value| value as usize == face));
        }
    }
}

fn alg_strategy() -> impl Strategy<Value = Alg> {
    prop::collection::vec((0..8u8, 1..=2u8), 0..24).prop_map(|moves| Alg {
        moves: moves
            .into_iter()
            .map(|(face, quarter)| Move { face, quarter })
            .collect(),
    })
}

proptest! {
    #[test]
    fn inverse_round_trips_through_solved(alg in alg_strategy()) {
        let fto = Fto::new();
        let inverse = fto.invert_alg(&alg);
        let mut forward = fto.solved_state();
        fto.apply_alg(&mut forward, &alg).unwrap();
        fto.apply_alg(&mut forward, &inverse).unwrap();
        prop_assert!(fto.is_solved(&forward));
        let mut reverse = fto.solved_state();
        fto.apply_alg(&mut reverse, &inverse).unwrap();
        fto.apply_alg(&mut reverse, &alg).unwrap();
        prop_assert!(fto.is_solved(&reverse));
        let text = fto.format_alg(&alg);
        prop_assert!(fto.parse_alg(&text).unwrap() == alg);
        let inverse_text = fto.format_alg(&inverse);
        prop_assert!(fto.parse_alg(&inverse_text).unwrap() == inverse);
        prop_assert!(fto.invert_alg(&inverse) == alg);
    }
}
