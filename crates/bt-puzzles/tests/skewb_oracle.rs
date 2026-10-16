use bt_puzzles::skewb::{Alg, FACELET_COUNT, Move, Skewb, VERTEX_NAMES};
use bt_puzzles::{Geometry, ParseError, verify_tier1};
use proptest::prelude::*;
use std::collections::BTreeSet;
use std::path::Path;

const U: usize = 0;
const F: usize = 1;
const R: usize = 2;
const L: usize = 3;
const B: usize = 4;
const D: usize = 5;

fn slot(face: usize, cell: usize) -> usize {
    face * 9 + cell
}

fn after(skewb: &Skewb, text: &str) -> [u8; FACELET_COUNT] {
    let alg = skewb.parse_alg(text).unwrap();
    let mut state = skewb.solved_state();
    skewb.apply_alg(&mut state, &alg).unwrap();
    state.facelets
}

fn changed(skewb: &Skewb, text: &str) -> Vec<usize> {
    let facelets = after(skewb, text);
    (0..FACELET_COUNT)
        .filter(|&i| facelets[i] != (i / 9) as u8)
        .collect()
}

fn expected_layer(vertex: usize) -> Vec<usize> {
    let faces: [(usize, Vec<usize>); 6] = match vertex {
        0 => [
            (U, vec![1, 2, 3, 4, 5, 6, 7, 8]),
            (F, vec![0, 1, 2, 3, 4, 5, 7, 8]),
            (R, vec![0, 1, 2, 3, 4, 5, 6, 7]),
            (L, vec![2]),
            (B, vec![0]),
            (D, vec![2]),
        ],
        1 => [
            (U, vec![0, 1, 3, 4, 5, 6, 7, 8]),
            (F, vec![0, 1, 2, 3, 4, 5, 6, 7]),
            (R, vec![0]),
            (L, vec![0, 1, 2, 3, 4, 5, 7, 8]),
            (B, vec![2]),
            (D, vec![0]),
        ],
        2 => [
            (U, vec![0, 1, 2, 3, 4, 5, 7, 8]),
            (F, vec![2]),
            (R, vec![0, 1, 2, 3, 4, 5, 7, 8]),
            (L, vec![0]),
            (B, vec![0, 1, 2, 3, 4, 5, 6, 7]),
            (D, vec![8]),
        ],
        _ => [
            (U, vec![8]),
            (F, vec![1, 2, 3, 4, 5, 6, 7, 8]),
            (R, vec![0, 1, 3, 4, 5, 6, 7, 8]),
            (L, vec![8]),
            (B, vec![6]),
            (D, vec![0, 1, 2, 3, 4, 5, 7, 8]),
        ],
    };
    let mut slots: Vec<usize> = faces
        .iter()
        .flat_map(|(face, cells)| cells.iter().map(|&cell| slot(*face, cell)))
        .collect();
    slots.sort_unstable();
    slots
}

#[test]
fn solved_grid_renders_six_square_faces() {
    let skewb = Skewb::new();
    let grid = skewb.facelets(&skewb.solved_state());
    assert_eq!(grid.faces.len(), 6);
    for (face, grid_face) in grid.faces.iter().enumerate() {
        assert_eq!(
            grid_face
                .rows
                .iter()
                .map(|row| row.len())
                .collect::<Vec<_>>(),
            vec![3, 3, 3]
        );
        for row in &grid_face.rows {
            assert!(row.iter().all(|&value| value as usize == face));
        }
    }
}

#[test]
fn every_turn_is_a_bijective_order_three_permutation() {
    let skewb = Skewb::new();
    for vertex in 0..4u8 {
        for quarter in [1u8, 2u8] {
            let table = skewb.turn_table(vertex, quarter);
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
                    other => panic!("vertex {vertex} quarter {quarter} cycle len {other}"),
                }
            }
            assert_eq!((fixed, threes), (27, 9));
        }
    }
}

#[test]
fn turns_recolor_exactly_the_derived_layer() {
    let skewb = Skewb::new();
    for (vertex, name) in VERTEX_NAMES.iter().enumerate() {
        assert_eq!(changed(&skewb, name), expected_layer(vertex), "{name}");
        assert_eq!(
            changed(&skewb, &format!("{name}'")),
            expected_layer(vertex),
            "{name}'"
        );
        assert_eq!(changed(&skewb, name).len(), 27, "{name}");
    }
}

#[test]
fn center_cycles_match_derived_geometry() {
    let skewb = Skewb::new();
    let cases = [
        (
            "F",
            [(slot(U, 4), 1u8), (slot(R, 4), 0), (slot(F, 4), 2)],
            [slot(L, 4), slot(B, 4), slot(D, 4)],
        ),
        (
            "L",
            [(slot(U, 4), 3u8), (slot(F, 4), 0), (slot(L, 4), 1)],
            [slot(R, 4), slot(B, 4), slot(D, 4)],
        ),
        (
            "R",
            [(slot(U, 4), 2u8), (slot(B, 4), 0), (slot(R, 4), 4)],
            [slot(F, 4), slot(L, 4), slot(D, 4)],
        ),
        (
            "B",
            [(slot(F, 4), 5u8), (slot(R, 4), 1), (slot(D, 4), 2)],
            [slot(U, 4), slot(L, 4), slot(B, 4)],
        ),
    ];
    for (name, moved, frozen) in &cases {
        let facelets = after(&skewb, name);
        for (position, value) in moved {
            assert_eq!(facelets[*position], *value, "{name} at {position}");
        }
        for position in frozen {
            assert_eq!(facelets[*position], (*position / 9) as u8, "{name}");
        }
    }
}

#[test]
fn crosses_travel_with_their_centers() {
    let skewb = Skewb::new();
    let facelets = after(&skewb, "F");
    for cell in [1, 3, 4, 5, 7] {
        assert_eq!(facelets[slot(R, cell)], 0, "R{cell}");
        assert_eq!(facelets[slot(F, cell)], 2, "F{cell}");
        assert_eq!(facelets[slot(U, cell)], 1, "U{cell}");
    }
}

#[test]
fn corner_stickers_match_derived_geometry() {
    let skewb = Skewb::new();
    let cases = [
        (
            "F",
            vec![
                (slot(R, 0), 0u8),
                (slot(R, 2), 0),
                (slot(R, 6), 0),
                (slot(F, 0), 2),
                (slot(F, 2), 2),
                (slot(F, 8), 2),
                (slot(U, 2), 1),
                (slot(U, 6), 1),
                (slot(U, 8), 1),
                (slot(L, 2), 5),
                (slot(B, 0), 3),
                (slot(D, 2), 4),
            ],
        ),
        (
            "L",
            vec![
                (slot(F, 0), 0u8),
                (slot(F, 6), 0),
                (slot(L, 0), 1),
                (slot(L, 2), 1),
                (slot(L, 8), 1),
                (slot(U, 0), 3),
                (slot(U, 6), 3),
                (slot(U, 8), 3),
                (slot(D, 0), 2),
                (slot(B, 2), 5),
                (slot(R, 0), 4),
            ],
        ),
        (
            "R",
            vec![
                (slot(B, 0), 0u8),
                (slot(B, 2), 0),
                (slot(B, 6), 0),
                (slot(R, 0), 4),
                (slot(R, 2), 4),
                (slot(R, 8), 4),
                (slot(U, 0), 2),
                (slot(U, 2), 2),
                (slot(U, 8), 2),
                (slot(L, 0), 1),
                (slot(D, 8), 3),
                (slot(F, 2), 5),
            ],
        ),
        (
            "B",
            vec![
                (slot(R, 0), 1u8),
                (slot(R, 6), 1),
                (slot(R, 8), 1),
                (slot(D, 0), 2),
                (slot(D, 2), 2),
                (slot(D, 8), 2),
                (slot(F, 2), 5),
                (slot(F, 6), 5),
                (slot(F, 8), 5),
                (slot(B, 6), 0),
                (slot(L, 8), 4),
                (slot(U, 8), 3),
            ],
        ),
    ];
    for (name, pins) in &cases {
        let facelets = after(&skewb, name);
        for (position, value) in pins {
            assert_eq!(facelets[*position], *value, "{name} at {position}");
        }
    }
}

#[test]
fn every_move_has_order_three_and_counter_doubles_clock() {
    let skewb = Skewb::new();
    for name in VERTEX_NAMES {
        let facelets = after(&skewb, &format!("{name} {name} {name}"));
        assert_eq!(facelets, skewb.solved_state().facelets, "{name}");
        let doubled = after(&skewb, &format!("{name} {name}"));
        assert_eq!(after(&skewb, &format!("{name}'")), doubled, "{name}");
    }
}

#[test]
fn grammar_accepts_exactly_the_sarah_alphabet() {
    let skewb = Skewb::new();
    for (vertex, name) in VERTEX_NAMES.iter().enumerate() {
        assert_eq!(
            skewb.parse_move(name),
            Ok(Move {
                vertex: vertex as u8,
                quarter: 1,
            }),
            "{name}"
        );
        assert_eq!(
            skewb.parse_move(&format!("{name}'")),
            Ok(Move {
                vertex: vertex as u8,
                quarter: 2,
            }),
            "{name}'"
        );
    }
    let alg = skewb.parse_alg("F' L B' R").unwrap();
    assert_eq!(skewb.format_alg(&alg), "F' L B' R");
    assert_eq!(skewb.format_alg(&skewb.invert_alg(&alg)), "R' B L' F");
    assert_eq!(skewb.parse_alg(&skewb.format_alg(&alg)).unwrap(), alg);
}

#[test]
fn rejects_garbage_and_cube_face_tokens() {
    let skewb = Skewb::new();
    for token in [
        "U", "U'", "D", "D'", "F2", "F2'", "f", "l'", "u", "d", "x", "y", "z", "M", "E", "S", "w",
        "wut", "", "'", "2", "FF", "F''", "0", "-", "F F", "F ",
    ] {
        assert_eq!(
            skewb.parse_move(token),
            Err(ParseError::UnknownToken(token.to_string())),
            "{token}"
        );
    }
    assert!(matches!(
        skewb.parse_alg("F U").unwrap_err(),
        ParseError::UnknownToken(token) if token == "U"
    ));
}

fn is_rotation(token: &str) -> bool {
    let mut chars = token.chars();
    matches!(chars.next(), Some('x' | 'y' | 'z')) && chars.all(|ch| ch == '\'' || ch == '2')
}

fn normalize(alg: &str) -> String {
    let core = alg.split("->").next().unwrap_or("");
    let mut out: Vec<String> = Vec::new();
    let mut plain = String::new();
    let mut group = String::new();
    let mut suffix = String::new();
    let mut depth = 0usize;
    let mut pending = false;
    for ch in core.chars().chain([' ']) {
        if pending {
            if ch.is_ascii_digit() {
                suffix.push(ch);
                continue;
            }
            let repeats = suffix.parse::<usize>().unwrap_or(1).max(1);
            let tokens: Vec<&str> = group.split_whitespace().collect();
            if !tokens.iter().all(|token| is_rotation(token)) {
                for _ in 0..repeats {
                    out.extend(tokens.iter().map(|token| token.to_string()));
                }
            }
            suffix.clear();
            pending = false;
            if ch == '(' {
                depth = 1;
                group.clear();
            } else if ch != ' ' {
                plain.push(ch);
            }
        } else if depth == 0 {
            match ch {
                '(' => {
                    depth = 1;
                    group.clear();
                }
                ' ' => {
                    if !plain.is_empty() {
                        out.push(std::mem::take(&mut plain));
                    }
                }
                _ => plain.push(ch),
            }
        } else {
            match ch {
                '(' => {
                    depth += 1;
                    group.push(ch);
                }
                ')' => {
                    depth -= 1;
                    if depth == 0 {
                        pending = true;
                    } else {
                        group.push(ch);
                    }
                }
                _ => group.push(ch),
            }
        }
    }
    out.join(" ")
}

#[test]
fn rotation_groups_arrow_chains_and_group_repeats_strip_to_move_streams() {
    assert_eq!(normalize("L F' L'"), "L F' L'");
    assert_eq!(normalize("(y2) L F' L' F"), "L F' L' F");
    assert_eq!(normalize("(y) R' F R F'"), "R' F R F'");
    assert_eq!(normalize("(y2) (F' L F L')2"), "F' L F L' F' L F L'");
    assert_eq!(normalize("R' F R F' -> cat. 1"), "R' F R F'");
    assert_eq!(normalize("L F' L' F -> cat. 1"), "L F' L' F");
    assert_eq!(
        normalize("R' F R F' (y2 x') R' F R F'"),
        "R' F R F' R' F R F'"
    );
    assert_eq!(
        normalize("R' F R F' (y2) R' F R F' (y2 x') R' F R F' (y2) R' F R F'"),
        "R' F R F' R' F R F' R' F R F' R' F R F'"
    );
    assert_eq!(
        normalize("R' F R F' (y2) R' F R F' (z2 x') R' F R F' (y2) R' F R F'"),
        "R' F R F' R' F R F' R' F R F' R' F R F'"
    );
}

#[test]
fn pack_rows_pass_tier1_after_stripping() {
    let skewb = Skewb::new();
    let path = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../ref_ground/packs/skewb_sarah.tsv");
    let text =
        std::fs::read_to_string(&path).unwrap_or_else(|err| panic!("pack unreadable: {err}"));
    let mut rows = 0usize;
    let mut tokens = BTreeSet::new();
    for line in text.lines().skip(1).filter(|line| !line.trim().is_empty()) {
        let cols: Vec<&str> = line.trim_end_matches('\r').split('\t').collect();
        let alg = normalize(cols[1]);
        for token in alg.split_whitespace() {
            tokens.insert(token.to_string());
        }
        verify_tier1(&skewb, &alg).unwrap_or_else(|err| panic!("{}: {err}", cols[0]));
        rows += 1;
    }
    assert_eq!(rows, 15);
    let expected: BTreeSet<String> = ["F", "F'", "L", "L'", "R", "R'"]
        .iter()
        .map(|token| token.to_string())
        .collect();
    assert_eq!(tokens, expected);
}

fn alg_strategy() -> impl Strategy<Value = Alg> {
    prop::collection::vec((0..4u8, 1..=2u8), 0..24).prop_map(|moves| Alg {
        moves: moves
            .into_iter()
            .map(|(vertex, quarter)| Move { vertex, quarter })
            .collect(),
    })
}

proptest! {
    #[test]
    fn inverse_round_trips_through_solved(alg in alg_strategy()) {
        let skewb = Skewb::new();
        let inverse = skewb.invert_alg(&alg);
        let mut forward = skewb.solved_state();
        skewb.apply_alg(&mut forward, &alg).unwrap();
        skewb.apply_alg(&mut forward, &inverse).unwrap();
        prop_assert!(skewb.is_solved(&forward));
        let mut reverse = skewb.solved_state();
        skewb.apply_alg(&mut reverse, &inverse).unwrap();
        skewb.apply_alg(&mut reverse, &alg).unwrap();
        prop_assert!(skewb.is_solved(&reverse));
        let text = skewb.format_alg(&alg);
        prop_assert!(skewb.parse_alg(&text).unwrap() == alg);
        let inverse_text = skewb.format_alg(&inverse);
        prop_assert!(skewb.parse_alg(&inverse_text).unwrap() == inverse);
        prop_assert!(skewb.invert_alg(&inverse) == alg);
    }
}

const DIRECTION_WITNESSES: [(&str, usize, u8); 4] =
    [("F", 2, 1), ("L", 0, 3), ("R", 0, 2), ("B", 8, 3)];

#[test]
fn quarter_one_direction_is_pinned_per_vertex() {
    let skewb = Skewb::new();
    for (index, (name, position, value)) in DIRECTION_WITNESSES.iter().enumerate() {
        let mut state = skewb.solved_state();
        let alg = skewb.parse_alg(name).expect("canonical vertex turn");
        skewb.apply_alg(&mut state, &alg).expect("apply");
        assert_eq!(state.facelets[*position], *value, "{name} quarter one");
        let table = skewb.turn_table(index as u8, 1);
        let source = table
            .iter()
            .position(|&target| target as usize == *position)
            .expect("witness position is a turn image");
        assert_eq!(source / 9, *value as usize, "{name} quarter one table");
    }
}

#[test]
fn apply_rejects_hand_built_illegal_quarters() {
    let skewb = Skewb::new();
    for quarter in [0u8, 3u8] {
        let alg = Alg {
            moves: vec![Move { vertex: 0, quarter }],
        };
        let mut state = skewb.solved_state();
        let err = skewb
            .apply_alg(&mut state, &alg)
            .expect_err("illegal quarter");
        assert!(err.to_string().contains("illegal"), "err: {err}");
    }
}
