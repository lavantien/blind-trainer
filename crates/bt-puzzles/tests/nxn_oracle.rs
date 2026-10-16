use bt_core::{Alg, Cube};
use bt_puzzles::nxn::{Nxn, NxnState};
use bt_puzzles::{FaceletFace, FaceletGrid, Geometry, verify_tier1};
use proptest::prelude::*;
use std::iter::once;

const FACES: [char; 6] = ['U', 'L', 'F', 'R', 'B', 'D'];
const SUFFIXES: [&str; 5] = ["", "'", "2", "2'", "'2"];
const CORE_SUFFIXES: [&str; 4] = ["", "'", "2", "2'"];

fn token_strategy(n: usize) -> impl Strategy<Value = String> {
    (0u8..8, 0usize..6, 2u8..=n as u8, 0usize..5).prop_map(|(class, face, digit, suffix)| {
        match class {
            0 | 1 => format!("{}{}", FACES[face], SUFFIXES[suffix]),
            2 => format!("{}w{}", FACES[face], SUFFIXES[suffix]),
            3 => format!("{digit}{}{}", FACES[face], SUFFIXES[suffix]),
            4 => format!("{digit}{}w{}", FACES[face], SUFFIXES[suffix]),
            5 => format!("{}{}", FACES[face].to_ascii_lowercase(), SUFFIXES[suffix]),
            6 => format!("{}{}", ["M", "E", "S"][face % 3], SUFFIXES[suffix]),
            _ => format!("{}{}", ["x", "y", "z"][face % 3], SUFFIXES[suffix]),
        }
    })
}

fn core_token_strategy() -> impl Strategy<Value = String> {
    (0u8..6, 0usize..6, 0usize..4).prop_map(|(class, face, suffix)| match class {
        0 | 1 => format!("{}{}", FACES[face], CORE_SUFFIXES[suffix]),
        2 => format!("2{}{}", FACES[face], CORE_SUFFIXES[suffix]),
        3 => format!(
            "{}{}",
            FACES[face].to_ascii_lowercase(),
            CORE_SUFFIXES[suffix]
        ),
        4 => format!("{}{}", ["M", "E", "S"][face % 3], CORE_SUFFIXES[suffix]),
        _ => format!("{}{}", ["x", "y", "z"][face % 3], CORE_SUFFIXES[suffix]),
    })
}

fn alg_case() -> impl Strategy<Value = (usize, Vec<String>)> {
    (2usize..=7).prop_flat_map(|n| {
        (
            Just(n),
            proptest::collection::vec(token_strategy(n), 1..=24),
        )
    })
}

fn applied(engine: &Nxn, text: &str) -> NxnState {
    let mut state = engine.solved_state();
    let alg = engine.parse_alg(text).unwrap();
    engine.apply_alg(&mut state, &alg).unwrap();
    state
}

fn slot(face: usize, r: usize, c: usize) -> usize {
    if r == 1 && c == 1 {
        48 + face
    } else if r.is_multiple_of(2) && c.is_multiple_of(2) {
        let pos = if r == 0 { c / 2 } else { 3 - c / 2 };
        face * 4 + pos
    } else {
        let pos = if r == 1 { 3 - c } else { r };
        24 + face * 4 + pos
    }
}

fn home_face(sticker: usize) -> usize {
    if sticker < 24 {
        sticker / 4
    } else if sticker < 48 {
        (sticker - 24) / 4
    } else {
        sticker - 48
    }
}

fn cube_facelets(cube: &Cube) -> FaceletGrid {
    FaceletGrid {
        faces: (0..6)
            .map(|face| FaceletFace {
                rows: (0..3)
                    .map(|r| {
                        (0..3)
                            .map(|c| {
                                home_face(cube.sticker_at(slot(face, r, c) as u8) as usize) as u8
                            })
                            .collect()
                    })
                    .collect(),
            })
            .collect(),
    }
}

fn pack_rows(file: &str) -> Vec<String> {
    let path = format!(
        "{}/../../ref_ground/packs/{file}",
        env!("CARGO_MANIFEST_DIR")
    );
    let text = std::fs::read_to_string(&path).unwrap();
    text.lines()
        .skip(1)
        .filter_map(|line| {
            let mut cols = line.split('\t');
            cols.next()?;
            let alg = cols.next()?;
            let cleaned = alg
                .split("->")
                .next()
                .unwrap_or(alg)
                .replace([',', '(', ')'], " ");
            let tokens: Vec<&str> = cleaned.split_whitespace().collect();
            if tokens.is_empty() {
                None
            } else {
                Some(tokens.join(" "))
            }
        })
        .collect()
}

fn shared_corpus() -> Vec<String> {
    let mut rows = Vec::new();
    for file in [
        "bld_3style_specials.tsv",
        "bld_m2.tsv",
        "twoby2_cll.tsv",
        "bld_oldpoch.tsv",
    ] {
        rows.extend(pack_rows(file));
    }
    rows.into_iter()
        .filter(|text| Alg::parse(text).is_ok())
        .collect()
}

#[test]
fn constructor_bounds_n_to_two_through_seven() {
    for n in [0, 1, 8, 100] {
        assert!(Nxn::new(n).is_none(), "{n}");
    }
    for n in 2..=7 {
        assert_eq!(Nxn::new(n).unwrap().n(), n);
    }
}

#[test]
fn solved_state_renders_uniform_faces_at_every_n() {
    for n in 2..=7 {
        let engine = Nxn::new(n).unwrap();
        let state = engine.solved_state();
        assert!(engine.is_solved(&state));
        let grid = engine.facelets(&state);
        assert_eq!(grid.faces.len(), 6);
        for (face, sheet) in grid.faces.iter().enumerate() {
            assert_eq!(sheet.rows.len(), n);
            assert!(
                sheet
                    .rows
                    .iter()
                    .all(|row| { row.len() == n && row.iter().all(|&v| v == face as u8) })
            );
        }
    }
}

#[test]
fn parses_every_token_class_at_n3() {
    let engine = Nxn::new(3).unwrap();
    for token in [
        "U", "U'", "U2", "U2'", "U'2", "D", "L", "F", "R", "B", "D'", "L2", "F2", "Rw", "Rw'",
        "Rw2", "Rw2'", "Rw'2", "Lw", "Uw", "Dw", "Fw", "Bw", "Uw2'", "r", "l", "u", "d", "f", "b",
        "r'", "l2", "u2'", "b2", "M", "M'", "M2", "M2'", "E", "E'", "E2", "S", "S'", "S2", "S2'",
        "x", "x'", "x2", "y", "y'", "y2", "z", "z'", "z2", "x2'", "y'2", "z2'", "2U", "2U'", "2U2",
        "2D", "2L", "2F", "2R", "2B", "2Rw", "3Rw", "3Rw'",
    ] {
        assert!(engine.parse_move(token).is_ok(), "{token}");
    }
}

#[test]
fn parses_digit_prefixes_up_to_n7() {
    let engine = Nxn::new(7).unwrap();
    for face in FACES {
        for digit in 2..=7 {
            assert!(engine.parse_move(&format!("{digit}{face}")).is_ok());
            assert!(engine.parse_move(&format!("{digit}{face}w")).is_ok());
            assert!(engine.parse_move(&format!("{digit}{face}w'")).is_ok());
        }
    }
    let small = Nxn::new(4).unwrap();
    for token in ["5R", "6R", "7R", "5Rw", "6Rw", "7Rw", "8R", "8Rw"] {
        assert!(small.parse_move(token).is_err(), "{token}");
    }
}

#[test]
fn both_prime_orders_agree_and_format_to_the_half_turn() {
    let engine = Nxn::new(3).unwrap();
    for (a, b, canonical) in [
        ("U2'", "U'2", "U2"),
        ("Rw2'", "Rw'2", "Rw2"),
        ("M2'", "M'2", "M2"),
        ("x2'", "x'2", "x2"),
        ("u2'", "u'2", "Uw2"),
        ("2R2'", "2R'2", "2R2"),
    ] {
        assert_eq!(engine.parse_move(a).unwrap(), engine.parse_move(b).unwrap());
        let alg = engine.parse_alg(a).unwrap();
        assert_eq!(engine.format_alg(&alg), canonical);
    }
}

#[test]
fn wide_aliases_parse_to_identical_moves() {
    let engine = Nxn::new(3).unwrap();
    for (a, b) in [
        ("Rw", "2Rw"),
        ("u", "Uw"),
        ("r", "Rw"),
        ("f", "Fw"),
        ("l2", "Lw2"),
        ("2r", "Rw"),
        ("3r", "3Rw"),
    ] {
        assert_eq!(
            engine.parse_move(a).unwrap(),
            engine.parse_move(b).unwrap(),
            "{a} {b}"
        );
    }
}

#[test]
fn rejects_garbage_tokens() {
    let engine = Nxn::new(3).unwrap();
    for token in [
        "", "V", "N", "P", "Q", "T", "W", "A", "I", "O", "8R", "9Rw", "1R", "0U", "2", "7", "U3",
        "U4", "U5", "U''", "U'''", "U22", "U222", "U2''", "U'2'", "rw", "Rww", "wR", "m", "e", "s",
        "2M", "2x", "3y", "X", "Y", "Z", "'", "''", "2'", "'U", "U-", "U+", "UwU", "Uw2w", "Rw3",
        "rw'", "ü", "Uw-", "(R",
    ] {
        assert!(
            matches!(
                engine.parse_move(token),
                Err(bt_puzzles::ParseError::UnknownToken(_))
            ),
            "{token}"
        );
    }
}

#[test]
fn parse_alg_splits_on_whitespace_and_accepts_empty() {
    let engine = Nxn::new(3).unwrap();
    assert!(engine.parse_alg("").unwrap().is_empty());
    let alg = engine.parse_alg("  R   U'  ").unwrap();
    assert_eq!(alg.moves().len(), 2);
    assert!(engine.parse_alg("R wut").is_err());
    assert_eq!(engine.format_alg(&engine.parse_alg("").unwrap()), "");
}

#[test]
fn formatting_is_canonical_per_token_class() {
    let engine = Nxn::new(3).unwrap();
    for (token, canonical) in [
        ("U", "U"),
        ("U'", "U'"),
        ("U2'", "U2"),
        ("u", "Uw"),
        ("2Uw", "Uw"),
        ("2U", "2U"),
        ("M", "M"),
        ("S'", "S'"),
        ("x", "x"),
        ("Rw", "Rw"),
        ("Rw2'", "Rw2"),
        ("3Rw'", "x'"),
    ] {
        let alg = engine.parse_alg(token).unwrap();
        assert_eq!(engine.format_alg(&alg), canonical, "{token}");
    }
    let tiny = Nxn::new(2).unwrap();
    for (token, canonical) in [
        ("Rw", "x"),
        ("Uw", "y"),
        ("Fw", "z"),
        ("Lw", "Lw"),
        ("M", "M"),
    ] {
        let alg = tiny.parse_alg(token).unwrap();
        assert_eq!(tiny.format_alg(&alg), canonical, "{token}");
    }
    let big = Nxn::new(4).unwrap();
    for (token, canonical) in [("3Rw'", "3Rw'"), ("4Rw", "x"), ("3R", "3R")] {
        let alg = big.parse_alg(token).unwrap();
        assert_eq!(big.format_alg(&alg), canonical, "{token}");
    }
}

proptest! {
    #[test]
    fn random_algs_round_trip_through_their_inverse_at_every_n(case in alg_case()) {
        let (n, tokens) = case;
        let engine = Nxn::new(n).unwrap();
        let text = tokens.join(" ");
        let alg = engine.parse_alg(&text).unwrap();
        let inverse = engine.invert_alg(&alg);
        let mut forward = engine.solved_state();
        engine.apply_alg(&mut forward, &alg).unwrap();
        engine.apply_alg(&mut forward, &inverse).unwrap();
        prop_assert!(engine.is_solved(&forward), "n={} {}", n, text);
        let mut reverse = engine.solved_state();
        engine.apply_alg(&mut reverse, &inverse).unwrap();
        engine.apply_alg(&mut reverse, &alg).unwrap();
        prop_assert!(engine.is_solved(&reverse), "n={} {}", n, text);
    }

    #[test]
    fn format_and_invert_are_stable_on_random_algs(case in alg_case()) {
        let (n, tokens) = case;
        let engine = Nxn::new(n).unwrap();
        let text = tokens.join(" ");
        let alg = engine.parse_alg(&text).unwrap();
        let reparsed = engine.parse_alg(&engine.format_alg(&alg)).unwrap();
        prop_assert_eq!(reparsed, alg.clone());
        prop_assert_eq!(engine.invert_alg(&engine.invert_alg(&alg)), alg);
    }

    #[test]
    fn applying_random_algs_preserves_the_color_multiset(case in alg_case()) {
        let (n, tokens) = case;
        let engine = Nxn::new(n).unwrap();
        let state = applied(&engine, &tokens.join(" "));
        let grid = engine.facelets(&state);
        for face in 0..6u8 {
            let count = grid
                .faces
                .iter()
                .flat_map(|sheet| sheet.rows.iter().flatten())
                .filter(|&&v| v == face)
                .count();
            prop_assert_eq!(count, n * n, "n={} face={}", n, face);
        }
    }
}

#[test]
fn every_quarter_move_has_order_dividing_four() {
    for n in 2..=7usize {
        let engine = Nxn::new(n).unwrap();
        let mut tokens: Vec<String> = [
            "R", "R'", "R2", "Rw", "2R", "M", "E", "S", "x", "y", "z", "u",
        ]
        .iter()
        .map(|t| t.to_string())
        .collect();
        for digit in 2..=n {
            tokens.push(format!("{digit}R"));
            tokens.push(format!("{digit}Rw"));
        }
        for token in &tokens {
            let alg = engine.parse_alg(token).unwrap();
            let mut state = engine.solved_state();
            for _ in 0..4 {
                engine.apply_alg(&mut state, &alg).unwrap();
            }
            assert!(engine.is_solved(&state), "n={n} {token}");
        }
    }
}

#[test]
fn wide_moves_turn_both_outer_layers() {
    for n in 2..=7usize {
        let engine = Nxn::new(n).unwrap();
        assert_eq!(applied(&engine, "Rw"), applied(&engine, "R 2R"), "n={n}");
        assert_eq!(applied(&engine, "u"), applied(&engine, "U 2U"), "n={n}");
        assert_ne!(applied(&engine, "Rw"), applied(&engine, "R"), "n={n}");
    }
}

#[test]
fn digit_prefixes_select_the_matching_depth_band() {
    for n in 3..=7usize {
        let engine = Nxn::new(n).unwrap();
        let singles: Vec<String> = once("R".to_string())
            .chain((2..=n).map(|d| format!("{d}R")))
            .collect();
        for depth in 3..=n {
            let wide = format!("{depth}Rw");
            let composed = singles[..depth].join(" ");
            assert_eq!(
                applied(&engine, &wide),
                applied(&engine, &composed),
                "n={n} {wide}"
            );
        }
        if n >= 4 {
            assert_ne!(applied(&engine, "3Rw"), applied(&engine, "4Rw"), "n={n}");
        }
    }
    let engine = Nxn::new(4).unwrap();
    for token in ["2R", "3R"] {
        let state = applied(&engine, token);
        assert_ne!(state, engine.solved_state(), "{token}");
        let grid = engine.facelets(&state);
        for face in [1usize, 3usize] {
            assert!(
                grid.faces[face]
                    .rows
                    .iter()
                    .all(|row| row.iter().all(|&v| v == face as u8)),
                "{token} face {face}"
            );
        }
    }
    assert_ne!(applied(&engine, "2R"), applied(&engine, "3R"));
}

#[test]
fn slice_moves_turn_only_the_middle_layer() {
    let engine = Nxn::new(3).unwrap();
    assert_eq!(applied(&engine, "M"), applied(&engine, "2L"));
    assert_eq!(applied(&engine, "E"), applied(&engine, "2D"));
    assert_eq!(applied(&engine, "S"), applied(&engine, "2F"));
    assert_ne!(applied(&engine, "M"), applied(&engine, "L"));
    let engine = Nxn::new(4).unwrap();
    assert_eq!(applied(&engine, "M"), applied(&engine, "2L"));
    assert_ne!(applied(&engine, "M"), applied(&engine, "3L"));
    let engine = Nxn::new(5).unwrap();
    assert_eq!(applied(&engine, "M"), applied(&engine, "3L"));
    assert_eq!(applied(&engine, "E"), applied(&engine, "3D"));
    assert_eq!(applied(&engine, "S"), applied(&engine, "3F"));
    assert_ne!(applied(&engine, "M"), applied(&engine, "2L"));
    let engine = Nxn::new(2).unwrap();
    assert_eq!(applied(&engine, "M"), applied(&engine, "L"));
}

#[test]
fn rotations_reorient_every_layer_together() {
    let engine = Nxn::new(3).unwrap();
    assert_eq!(applied(&engine, "x"), applied(&engine, "R M' L'"));
    assert_eq!(applied(&engine, "y"), applied(&engine, "U E' D'"));
    assert_eq!(applied(&engine, "z"), applied(&engine, "F S B'"));
    assert_ne!(applied(&engine, "x"), engine.solved_state());
    let engine = Nxn::new(2).unwrap();
    assert_eq!(applied(&engine, "x"), applied(&engine, "Rw"));
}

#[test]
fn tier1_accepts_representative_algs_at_every_n() {
    for n in 2..=7usize {
        let engine = Nxn::new(n).unwrap();
        assert_eq!(verify_tier1(&engine, "R U R' U'"), Ok(()), "n={n}");
        assert_eq!(verify_tier1(&engine, ""), Ok(()), "n={n}");
    }
    let engine = Nxn::new(4).unwrap();
    let parity = "Rw U2 x Rw U2 Rw U2 Rw' U2 Lw U2 Rw' U2 Rw U2 Rw' U2 Rw'";
    assert_eq!(verify_tier1(&engine, parity), Ok(()));
    let engine = Nxn::new(5).unwrap();
    let l2e = "Rw' U' R' U R' F R F' Rw";
    assert_eq!(verify_tier1(&engine, l2e), Ok(()));
    assert!(matches!(
        verify_tier1(&engine, "R wut"),
        Err(bt_puzzles::Tier1Error::Parse(_))
    ));
}

#[test]
fn bigcube_pack_tokens_parse_at_their_registered_n() {
    let parity_tokens = [
        "R'", "U", "3Rw'", "F2", "Lw", "R", "Rw", "Rw'", "U'", "U2", "Uw2", "l'", "l", "r'", "r",
        "r2", "u2", "x",
    ];
    for n in 4..=7usize {
        let engine = Nxn::new(n).unwrap();
        for token in parity_tokens {
            assert!(engine.parse_move(token).is_ok(), "{token} n={n}");
        }
    }
    let l2e_tokens = [
        "3Lw", "3Lw'", "3Rw'", "B2", "F", "F'", "F2", "Lw", "Lw'", "R", "R'", "Rw", "Rw'", "Rw2",
        "Rw2'", "U", "U'", "U2", "U2'", "x", "x'",
    ];
    for n in 5..=7usize {
        let engine = Nxn::new(n).unwrap();
        for token in l2e_tokens {
            assert!(engine.parse_move(token).is_ok(), "{token} n={n}");
        }
    }
    assert!(Nxn::new(4).unwrap().parse_move("X").is_err());
    assert!(Nxn::new(5).unwrap().parse_move("X,").is_err());
}

#[test]
fn n3_agrees_with_bt_core_on_the_pack_corpus() {
    let engine = Nxn::new(3).unwrap();
    let corpus = shared_corpus();
    assert!(corpus.len() >= 150, "corpus shrank to {}", corpus.len());
    for text in &corpus {
        let mut cube = Cube::solved();
        cube.apply_alg(&Alg::parse(text).unwrap());
        let state = applied(&engine, text);
        assert_eq!(engine.facelets(&state), cube_facelets(&cube), "{text}");
    }
}

proptest! {
    #[test]
    fn n3_agrees_with_bt_core_on_generated_algs(
        tokens in proptest::collection::vec(core_token_strategy(), 1..=30)
    ) {
        let engine = Nxn::new(3).unwrap();
        let text = tokens.join(" ");
        let mut cube = Cube::solved();
        cube.apply_alg(&Alg::parse(&text).unwrap());
        let state = applied(&engine, &text);
        prop_assert_eq!(engine.facelets(&state), cube_facelets(&cube), "{}", text);
    }
}
#[test]
fn quarter_one_direction_is_pinned_by_the_side_top_rows() {
    let engine = Nxn::new(3).expect("engine");
    let mut state = engine.solved_state();
    let alg = engine.parse_alg("U").expect("canonical turn");
    engine.apply_alg(&mut state, &alg).expect("apply");
    let grid = engine.facelets(&state);
    for (face, value) in [(1usize, 2u8), (2, 3), (3, 4), (4, 1)] {
        assert_eq!(
            grid.faces[face].rows[0],
            vec![value; 3],
            "face {face} top row"
        );
    }
}

#[test]
fn is_solved_detects_a_turned_state() {
    let engine = Nxn::new(3).expect("engine");
    let solved = engine.solved_state();
    assert!(engine.is_solved(&solved));
    let mut turned = engine.solved_state();
    let alg = engine.parse_alg("U").expect("canonical turn");
    engine.apply_alg(&mut turned, &alg).expect("apply");
    assert!(!engine.is_solved(&turned));
}

#[test]
fn is_empty_reflects_the_parsed_move_count() {
    let engine = Nxn::new(3).expect("engine");
    assert!(!engine.parse_alg("U").expect("parse").is_empty());
    assert!(engine.parse_alg("").expect("parse empty").is_empty());
}
