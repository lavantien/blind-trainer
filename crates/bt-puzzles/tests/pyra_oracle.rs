#[path = "../src/geometry.rs"]
mod geometry;

#[path = "../src/pyra.rs"]
mod pyra;

#[path = "../src/tier1.rs"]
mod tier1;

use geometry::Geometry;
use pyra::{Layers, PyraMove, Pyraminx, Sense, Vertex};
use std::collections::BTreeSet;
use std::path::PathBuf;
use tier1::verify_tier1;

const PYRA: Pyraminx = Pyraminx;

const TOKENS: [&str; 16] = [
    "U", "U'", "L", "L'", "R", "R'", "B", "B'", "u", "u'", "l", "l'", "r", "r'", "b", "b'",
];

const U_AFTER: [u8; 36] = [
    2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3,
    3, 3, 3, 3,
];

const L_AFTER: [u8; 36] = [
    0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 3, 1, 1, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 0, 3,
    3, 0, 0, 0,
];

const R_AFTER: [u8; 36] = [
    0, 0, 0, 2, 0, 0, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 2, 2, 3, 3, 3, 2, 2, 3, 0, 3, 3, 0,
    0, 0, 3, 3,
];

const B_AFTER: [u8; 36] = [
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 1, 2, 2, 2, 1, 1, 2, 2, 2, 3, 2, 2, 3, 3, 3, 1, 1, 1, 1, 3,
    3, 3, 3, 3,
];

fn colors(text: &str) -> Vec<u8> {
    let alg = PYRA.parse_alg(text).unwrap();
    let mut state = PYRA.solved_state();
    PYRA.apply_alg(&mut state, &alg).unwrap();
    PYRA.facelets(&state)
        .faces
        .iter()
        .flat_map(|face| face.rows.iter().flat_map(|row| row.iter().copied()))
        .collect()
}

fn solved_colors() -> Vec<u8> {
    colors("")
}

fn changed_positions(text: &str) -> Vec<usize> {
    let after = colors(text);
    let base = solved_colors();
    let mut moved: Vec<usize> = after
        .iter()
        .zip(&base)
        .enumerate()
        .filter(|(_, (a, b))| a != b)
        .map(|(i, _)| i)
        .collect();
    moved.sort_unstable();
    moved
}

fn at(face: usize, locals: &[usize]) -> Vec<usize> {
    locals.iter().map(|local| face * 9 + local).collect()
}

#[test]
fn solved_grid_shape_and_colors() {
    let grid = PYRA.facelets(&PYRA.solved_state());
    assert_eq!(grid.faces.len(), 4);
    for (face, color) in grid.faces.iter().zip(0..4) {
        let flat: Vec<u8> = face
            .rows
            .iter()
            .flat_map(|row| row.iter().copied())
            .collect();
        assert_eq!(flat, vec![color as u8; 9]);
        let widths: Vec<usize> = face.rows.iter().map(|row| row.len()).collect();
        assert_eq!(widths, vec![1, 3, 5]);
    }
    assert!(PYRA.is_solved(&PYRA.solved_state()));
}

#[test]
fn vertex_turns_pin_derived_facelets() {
    for (text, expected) in [
        ("U", U_AFTER),
        ("L", L_AFTER),
        ("R", R_AFTER),
        ("B", B_AFTER),
    ] {
        assert_eq!(colors(text), expected.to_vec(), "{text}");
    }
}

#[test]
fn counter_turns_equal_double_clockwise() {
    for (clock, counter) in [
        ("U", "U'"),
        ("L", "L'"),
        ("R", "R'"),
        ("B", "B'"),
        ("u", "u'"),
        ("l", "l'"),
        ("r", "r'"),
        ("b", "b'"),
    ] {
        assert_eq!(
            colors(counter),
            colors(&format!("{clock} {clock}")),
            "{clock}"
        );
    }
}

#[test]
fn every_move_has_order_three() {
    for token in TOKENS {
        assert_eq!(
            colors(&format!("{0} {0} {0}", token)),
            solved_colors(),
            "{token}"
        );
    }
}

#[test]
fn vertex_turns_move_exactly_two_layer_bands() {
    let cases = [
        (
            "U",
            vec![
                at(0, &[0, 1, 2, 3]),
                at(1, &[0, 1, 2, 3]),
                at(2, &[0, 1, 2, 3]),
            ],
        ),
        (
            "L",
            vec![
                at(0, &[1, 4, 5, 6]),
                at(3, &[3, 6, 7, 8]),
                at(1, &[3, 6, 7, 8]),
            ],
        ),
        (
            "R",
            vec![
                at(0, &[3, 6, 7, 8]),
                at(3, &[1, 4, 5, 6]),
                at(2, &[1, 4, 5, 6]),
            ],
        ),
        (
            "B",
            vec![
                at(3, &[0, 1, 2, 3]),
                at(1, &[1, 4, 5, 6]),
                at(2, &[3, 6, 7, 8]),
            ],
        ),
    ];
    for (text, bands) in &cases {
        let mut expected = bands.concat();
        expected.sort_unstable();
        assert_eq!(changed_positions(text), expected, "{text}");
    }
}

#[test]
fn tip_turns_touch_only_tip_stickers() {
    let cases = [
        ("u", vec![0, 9, 18]),
        ("l", vec![4, 35, 17]),
        ("r", vec![8, 31, 22]),
        ("b", vec![27, 26, 13]),
    ];
    for (text, tips) in &cases {
        let mut expected = tips.clone();
        expected.sort_unstable();
        assert_eq!(changed_positions(text), expected, "{text}");
    }
}

#[test]
fn tip_turns_pin_derived_facelets() {
    for (text, tips, values) in [
        ("u", [0, 9, 18], [2, 0, 1]),
        ("l", [4, 35, 17], [1, 0, 3]),
        ("r", [8, 31, 22], [2, 0, 3]),
        ("b", [27, 26, 13], [1, 3, 2]),
    ] {
        let after = colors(text);
        for (position, value) in tips.iter().zip(values) {
            assert_eq!(after[*position], value, "{text} at {position}");
        }
    }
}

#[test]
fn vertex_turn_carries_its_tip_along() {
    for (twin, tip, corners) in [
        ("U", "u", [0usize, 9, 18]),
        ("L", "l", [4usize, 35, 17]),
        ("R", "r", [8usize, 31, 22]),
        ("B", "b", [27usize, 26, 13]),
    ] {
        let wide = colors(twin);
        let narrow = colors(tip);
        for corner in corners {
            assert_eq!(wide[corner], narrow[corner], "{twin} vs {tip} at {corner}");
        }
    }
}

#[test]
fn single_moves_break_and_inverse_restores_solved() {
    for token in TOKENS {
        let alg = PYRA.parse_alg(token).unwrap();
        let mut state = PYRA.solved_state();
        PYRA.apply_alg(&mut state, &alg).unwrap();
        assert!(!PYRA.is_solved(&state), "{token}");
        PYRA.apply_alg(&mut state, &PYRA.invert_alg(&alg)).unwrap();
        assert!(PYRA.is_solved(&state), "{token}");
    }
}

#[test]
fn parses_every_12e_token() {
    let expected = [
        ("U", Vertex::U, Layers::Twin, Sense::Clock),
        ("U'", Vertex::U, Layers::Twin, Sense::Counter),
        ("u", Vertex::U, Layers::Tip, Sense::Clock),
        ("u'", Vertex::U, Layers::Tip, Sense::Counter),
        ("L", Vertex::L, Layers::Twin, Sense::Clock),
        ("L'", Vertex::L, Layers::Twin, Sense::Counter),
        ("l", Vertex::L, Layers::Tip, Sense::Clock),
        ("l'", Vertex::L, Layers::Tip, Sense::Counter),
        ("R", Vertex::R, Layers::Twin, Sense::Clock),
        ("R'", Vertex::R, Layers::Twin, Sense::Counter),
        ("r", Vertex::R, Layers::Tip, Sense::Clock),
        ("r'", Vertex::R, Layers::Tip, Sense::Counter),
        ("B", Vertex::B, Layers::Twin, Sense::Clock),
        ("B'", Vertex::B, Layers::Twin, Sense::Counter),
        ("b", Vertex::B, Layers::Tip, Sense::Clock),
        ("b'", Vertex::B, Layers::Tip, Sense::Counter),
    ];
    for (token, vertex, layers, sense) in expected {
        assert_eq!(
            PYRA.parse_move(token),
            Ok(PyraMove {
                vertex,
                layers,
                sense,
            }),
            "{token}"
        );
    }
}

#[test]
fn rejects_tokens_outside_12e() {
    for token in [
        "F", "F'", "D", "d", "U2", "u2", "U2'", "'U", "UU", "U''", "(", "(R", "R')", "x", "y", "M",
        "e", "s", "ü", "U ", " U", "uB", "0", "-",
    ] {
        assert!(PYRA.parse_move(token).is_err(), "{token}");
    }
    assert!(PYRA.parse_alg("(R U' R')").is_err());
    assert!(PYRA.parse_alg("U 2").is_err());
    assert!(PYRA.parse_alg("R2").is_err());
}

#[test]
fn formats_and_inverts() {
    let alg = PYRA.parse_alg("U u' L").unwrap();
    assert_eq!(PYRA.format_alg(&alg), "U u' L");
    assert_eq!(PYRA.format_alg(&PYRA.invert_alg(&alg)), "L' u U'");
    let empty = PYRA.parse_alg("").unwrap();
    assert_eq!(PYRA.format_alg(&empty), "");
    assert_eq!(PYRA.invert_alg(&empty).moves.len(), 0);
    assert_eq!(PYRA.parse_alg(&PYRA.format_alg(&alg)).unwrap(), alg);
}

#[test]
fn apply_always_succeeds() {
    let alg = PYRA.parse_alg("U' b L R' u").unwrap();
    let mut state = PYRA.solved_state();
    assert!(PYRA.apply_alg(&mut state, &alg).is_ok());
}

struct XorShift(u64);

impl XorShift {
    fn next(&mut self) -> u64 {
        let mut x = self.0;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        self.0 = x;
        x
    }

    fn below(&mut self, bound: usize) -> usize {
        (self.next() % bound as u64) as usize
    }
}

#[test]
fn random_algs_round_trip() {
    let mut rng = XorShift(0x5eed_1234_abcd_f00d);
    for _ in 0..1000 {
        let len = rng.below(21);
        let tokens: Vec<&str> = (0..len).map(|_| TOKENS[rng.below(TOKENS.len())]).collect();
        let text = tokens.join(" ");
        verify_tier1(&PYRA, &text).unwrap_or_else(|err| panic!("{text}: {err}"));
        let alg = PYRA.parse_alg(&text).unwrap();
        assert_eq!(
            PYRA.parse_alg(&PYRA.format_alg(&alg)).unwrap(),
            alg,
            "{text}"
        );
        assert_eq!(PYRA.invert_alg(&PYRA.invert_alg(&alg)), alg, "{text}");
        let mut state = PYRA.solved_state();
        PYRA.apply_alg(&mut state, &alg).unwrap();
        PYRA.apply_alg(&mut state, &PYRA.invert_alg(&alg)).unwrap();
        assert!(PYRA.is_solved(&state), "{text}");
    }
}

#[test]
fn pack_rows_pass_tier1() {
    let path =
        PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../ref_ground/packs/pyraminx_l4e.tsv");
    let text = std::fs::read_to_string(&path).expect("pyraminx_l4e.tsv must exist");
    let mut lines = text.lines();
    let header = lines.next().expect("nonempty tsv");
    assert!(header.starts_with("case_key\talg\t"));
    let mut rows = 0usize;
    let mut tokens = BTreeSet::new();
    for line in lines {
        if line.is_empty() {
            continue;
        }
        let fields: Vec<&str> = line.split('\t').collect();
        assert!(fields.len() >= 2, "{line}");
        let alg = fields[1].replace(['(', ')'], " ");
        for token in alg.split_whitespace() {
            tokens.insert(token.to_string());
        }
        verify_tier1(&PYRA, &alg).unwrap_or_else(|err| panic!("{}: {err}", fields[0]));
        rows += 1;
    }
    assert_eq!(rows, 35);
    let expected: BTreeSet<String> = ["U", "U'", "L", "L'", "R", "R'"]
        .iter()
        .map(|token| token.to_string())
        .collect();
    assert_eq!(tokens, expected);
}
