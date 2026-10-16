#[path = "../src/geometry.rs"]
pub mod geometry;
#[path = "../src/sq1.rs"]
pub mod sq1;
#[path = "../src/tier1.rs"]
pub mod tier1;

use geometry::{ApplyError, Geometry, ParseError};
use sq1::{Move, SQUARE_SHAPE, Sq1, TWISTS};
use std::collections::{HashMap, HashSet};
use std::path::PathBuf;

const EXPECTED_ROWS: usize = 169;
const SOLVED_TOP: [u8; 12] = [0, 0, 1, 2, 2, 3, 4, 4, 5, 6, 6, 7];
const SOLVED_BOTTOM: [u8; 12] = [8, 9, 9, 10, 11, 11, 12, 13, 13, 14, 15, 15];
const KITE_SHAPE: [u8; 8] = [1, 1, 2, 1, 2, 2, 1, 2];

fn twist(top: i32, bottom: i32) -> Move {
    Move::Twist { top, bottom }
}

fn workspace_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .and_then(|path| path.parent())
        .expect("workspace root from manifest dir")
        .to_path_buf()
}

fn packs_dir() -> PathBuf {
    match std::env::var("BT_REF_GROUND") {
        Ok(raw) if !raw.is_empty() => {
            let path = PathBuf::from(raw);
            if path.is_absolute() {
                path
            } else {
                workspace_root().join(path)
            }
        }
        _ => workspace_root().join("ref_ground"),
    }
    .join("packs")
}

struct CaseRow {
    key: String,
    alg: String,
    names: [String; 2],
}

fn load_cubeshape_rows() -> Vec<CaseRow> {
    let path = packs_dir().join("sq1_cubeshape.tsv");
    let text = std::fs::read_to_string(&path).expect("sq1_cubeshape.tsv readable");
    let mut rows = Vec::new();
    for line in text.lines().skip(1) {
        let line = line.trim_end_matches('\r');
        if line.is_empty() {
            continue;
        }
        let cols: Vec<&str> = line.split('\t').collect();
        assert!(cols.len() >= 3, "row needs 3 columns: {}", line);
        let marker = "\"case\":\"";
        let start = cols[2].find(marker).expect("case marker") + marker.len();
        let rest = &cols[2][start..];
        let name = &rest[..rest.find('"').expect("case closing quote")];
        let parts: Vec<&str> = name.split(" / ").collect();
        assert_eq!(parts.len(), 2, "case name splits in two: {}", name);
        rows.push(CaseRow {
            key: cols[0].to_string(),
            alg: cols[1].to_string(),
            names: [parts[0].to_string(), parts[1].to_string()],
        });
    }
    rows
}

fn shape_pair(engine: &Sq1, alg_text: &str, inverse: bool) -> (Vec<u8>, Vec<u8>) {
    let alg = engine.parse_alg(alg_text).expect("alg parses");
    let applied = if inverse {
        engine.invert_alg(&alg)
    } else {
        alg
    };
    let mut state = engine.solved_state();
    engine.apply_alg(&mut state, &applied).expect("alg applies");
    (
        sq1::canonical_shape(&state, 0),
        sq1::canonical_shape(&state, 1),
    )
}

#[test]
fn solved_state_matches_the_ported_layout() {
    let engine = Sq1;
    let state = engine.solved_state();
    assert_eq!(state.units[0..12].to_vec(), SOLVED_TOP.to_vec());
    assert_eq!(state.units[12..24].to_vec(), SOLVED_BOTTOM.to_vec());
    assert!(!state.slice_flipped);
    assert!(engine.is_solved(&state));
    assert!(sq1::can_slash(&state));
    let grid = engine.facelets(&state);
    assert_eq!(grid.faces.len(), 2);
    assert_eq!(grid.faces[0].rows, vec![SOLVED_TOP.to_vec()]);
    assert_eq!(grid.faces[1].rows, vec![SOLVED_BOTTOM.to_vec()]);
    assert_eq!(
        sq1::canonical_shape(&state, 0),
        SQUARE_SHAPE.to_vec(),
        "solved layer classifies as the square shape"
    );
    assert_eq!(sq1::canonical_shape(&state, 1), SQUARE_SHAPE.to_vec());
}

#[test]
fn slash_swaps_ring_halves_and_flips_the_slice() {
    let engine = Sq1;
    let mut state = engine.solved_state();
    engine
        .apply_alg(&mut state, &engine.parse_alg("/").unwrap())
        .unwrap();
    let expected_top: [u8; 12] = [0, 0, 1, 2, 2, 3, 12, 13, 13, 14, 15, 15];
    let expected_bottom: [u8; 12] = [8, 9, 9, 10, 11, 11, 4, 4, 5, 6, 6, 7];
    assert_eq!(state.units[0..12].to_vec(), expected_top.to_vec());
    assert_eq!(state.units[12..24].to_vec(), expected_bottom.to_vec());
    assert!(state.slice_flipped);
    assert!(!engine.is_solved(&state));
    engine
        .apply_alg(&mut state, &engine.parse_alg("/").unwrap())
        .unwrap();
    assert!(engine.is_solved(&state));
}

#[test]
fn slash_legality_is_state_aware() {
    let engine = Sq1;
    let mut aligned = engine.solved_state();
    assert!(
        engine
            .apply_alg(&mut aligned, &engine.parse_alg("(1,0) /").unwrap())
            .is_ok()
    );
    for text in ["(2,0) /", "(0,1) /"] {
        let mut misaligned = engine.solved_state();
        assert_eq!(
            engine.apply_alg(&mut misaligned, &engine.parse_alg(text).unwrap()),
            Err(ApplyError::IllegalMove("/".to_string())),
            "{}",
            text
        );
    }
    assert_eq!(
        tier1::verify_tier1(&engine, "(2,0) /"),
        Err(tier1::Tier1Error::Apply(ApplyError::IllegalMove(
            "/".to_string()
        )))
    );
}

#[test]
fn twist_rotation_direction_follows_the_port() {
    let engine = Sq1;
    let mut top_only = engine.solved_state();
    engine
        .apply_alg(&mut top_only, &engine.parse_alg("(1,0)").unwrap())
        .unwrap();
    let rotated_top: [u8; 12] = [7, 0, 0, 1, 2, 2, 3, 4, 4, 5, 6, 6];
    assert_eq!(top_only.units[0..12].to_vec(), rotated_top.to_vec());
    assert_eq!(top_only.units[12..24].to_vec(), SOLVED_BOTTOM.to_vec());
    let mut bottom_only = engine.solved_state();
    engine
        .apply_alg(&mut bottom_only, &engine.parse_alg("(0,1)").unwrap())
        .unwrap();
    let rotated_bottom: [u8; 12] = [15, 8, 9, 9, 10, 11, 11, 12, 13, 13, 14, 15];
    assert_eq!(bottom_only.units[12..24].to_vec(), rotated_bottom.to_vec());
    assert_eq!(
        engine.facelets(&bottom_only).faces[1].rows,
        vec![rotated_bottom.to_vec()]
    );
}

#[test]
fn twist_enumeration_covers_exactly_143_moves() {
    let engine = Sq1;
    assert_eq!(TWISTS, 143);
    let mut distinct = HashSet::new();
    let mut count = 0;
    for top in -5..=6 {
        for bottom in -5..=6 {
            if top == 0 && bottom == 0 {
                continue;
            }
            count += 1;
            let parenthesized = format!("({top},{bottom})");
            let bare = format!("{top},{bottom}");
            let parsed = engine.parse_move(&parenthesized).unwrap();
            assert_eq!(parsed, twist(top, bottom), "{}", parenthesized);
            assert_eq!(engine.parse_move(&bare).unwrap(), parsed);
            distinct.insert(parenthesized);
            assert_eq!(tier1::verify_tier1(&engine, &bare), Ok(()));
        }
    }
    assert_eq!(count, 143);
    assert_eq!(distinct.len(), 143);
}

#[test]
fn grammar_accepts_the_pinned_token_forms() {
    let engine = Sq1;
    let cases: &[(&str, Vec<Move>)] = &[
        ("", vec![]),
        ("/", vec![Move::Slash]),
        ("//", vec![Move::Slash, Move::Slash]),
        ("(1,0)", vec![twist(1, 0)]),
        ("1,0", vec![twist(1, 0)]),
        ("  1,0   ", vec![twist(1, 0)]),
        ("(-5,-5)", vec![twist(-5, -5)]),
        ("(6,6)", vec![twist(6, 6)]),
        ("6,-3", vec![twist(6, -3)]),
        (
            "(1,0) / (-1,0)",
            vec![twist(1, 0), Move::Slash, twist(-1, 0)],
        ),
        ("(1,0)/(-1,0)", vec![twist(1, 0), Move::Slash, twist(-1, 0)]),
        (
            "(1,0)/(3,0)/(-1,0)",
            vec![
                twist(1, 0),
                Move::Slash,
                twist(3, 0),
                Move::Slash,
                twist(-1, 0),
            ],
        ),
        ("1,0/", vec![twist(1, 0), Move::Slash]),
        ("/3,3/", vec![Move::Slash, twist(3, 3), Move::Slash]),
        ("/ -1,2 /", vec![Move::Slash, twist(-1, 2), Move::Slash]),
    ];
    for (text, expected) in cases {
        assert_eq!(engine.parse_alg(text), Ok(expected.clone()), "{:?}", text);
    }
}

#[test]
fn grammar_rejects_pack_hazards_and_garbage() {
    let engine = Sq1;
    let cases: &[(&str, &str)] = &[
        ("(U)", "(U)"),
        ("(D)", "(D)"),
        ("(U2)", "(U2)"),
        ("(D2)", "(D2)"),
        ("(D')", "(D')"),
        ("(U D)", "(U D)"),
        ("(U2 D) (1,0)", "(U2 D)"),
        ("(U D)(0,-1)", "(U D)"),
        ("*", "*"),
        ("(1,0) / *", "*"),
        ("/ *", "*"),
        ("(0,0)", "(0,0)"),
        ("(7,0)", "(7,0)"),
        ("(1,7)", "(1,7)"),
        ("(-6,0)", "(-6,0)"),
        ("(05,0)", "(05,0)"),
        ("(-0,1)", "(-0,1)"),
        ("(1,0", "(1,0"),
        ("(1,2,3)", "(1,2,3)"),
        ("( 1,0 )", "( 1,0 )"),
        ("( )", "( )"),
        ("(1,0)x", "x"),
        ("x,1", "x,1"),
        ("1", "1"),
        ("1,0,2", "1,0,2"),
        ("1,0)(2,3)", "1,0)(2,3)"),
        ("1 0", "1"),
    ];
    for (text, token) in cases {
        assert_eq!(
            engine.parse_alg(text),
            Err(ParseError::UnknownToken(token.to_string())),
            "{:?}",
            text
        );
    }
}

#[test]
fn parse_move_takes_exactly_one_token() {
    let engine = Sq1;
    assert_eq!(engine.parse_move("/"), Ok(Move::Slash));
    assert_eq!(engine.parse_move("1,0"), Ok(twist(1, 0)));
    assert_eq!(engine.parse_move("(1,0)"), Ok(twist(1, 0)));
    for text in ["", "(1,0)/", "1,0 /", "(1,0) (2,0)", "(U)"] {
        assert_eq!(
            engine.parse_move(text),
            Err(ParseError::UnknownToken(text.to_string())),
            "{:?}",
            text
        );
    }
}

#[test]
fn format_is_canonical_and_inversion_negates_twists() {
    let engine = Sq1;
    let fused = engine.parse_alg("(1,0)/(-1,0)").unwrap();
    assert_eq!(engine.format_alg(&fused), "(1,0) / (-1,0)");
    let bare = engine.parse_alg("1,0 / 3,3").unwrap();
    assert_eq!(engine.format_alg(&bare), "(1,0) / (3,3)");
    let slashes = engine.parse_alg("/3,3/").unwrap();
    assert_eq!(engine.format_alg(&slashes), "/ (3,3) /");
    assert_eq!(engine.format_alg(&engine.parse_alg("").unwrap()), "");
    let inverted = engine.invert_alg(&engine.parse_alg("(3,0) /").unwrap());
    assert_eq!(inverted, vec![Move::Slash, twist(-3, 0)]);
    assert_eq!(engine.format_alg(&inverted), "/ (-3,0)");
    assert_eq!(
        engine.invert_alg(&engine.parse_alg("/").unwrap()),
        vec![Move::Slash]
    );
}

#[test]
fn tier1_round_trips_extra_algs_outside_the_pack() {
    let engine = Sq1;
    for text in ["/", "(6,6)", "(1,0)/(3,0)/(-1,0)", "/ 3,3 / 3,3 /", "5,-5"] {
        assert_eq!(tier1::verify_tier1(&engine, text), Ok(()), "{}", text);
    }
}

#[test]
fn forward_from_solved_can_be_illegal_while_the_start_state_round_trip_holds() {
    let engine = Sq1;
    let parsed = engine.parse_alg("/ 0,1 / 0,3 /").unwrap();
    let mut forward = engine.solved_state();
    assert_eq!(
        engine.apply_alg(&mut forward, &parsed),
        Err(ApplyError::IllegalMove("/".to_string()))
    );
    let mut through_start = engine.solved_state();
    let inverted = engine.invert_alg(&parsed);
    engine.apply_alg(&mut through_start, &inverted).unwrap();
    assert_eq!(
        sq1::canonical_shape(&through_start, 0),
        SQUARE_SHAPE.to_vec()
    );
    engine.apply_alg(&mut through_start, &parsed).unwrap();
    assert!(engine.is_solved(&through_start));
}

#[test]
fn cs001_inverse_yields_kite_kite() {
    let engine = Sq1;
    let (top, bottom) = shape_pair(&engine, "/", true);
    assert_eq!(top, KITE_SHAPE.to_vec());
    assert_eq!(bottom, KITE_SHAPE.to_vec());
}

fn mirrored(widths: &[u8]) -> Vec<u8> {
    let mut reversed = widths.to_vec();
    reversed.reverse();
    let count = reversed.len();
    (0..count)
        .map(|rotation| {
            (0..count)
                .map(|unit| reversed[(rotation + unit) % count])
                .collect::<Vec<u8>>()
        })
        .min()
        .unwrap()
}

fn famkey(widths: &[u8]) -> Vec<u8> {
    let mirror = mirrored(widths);
    if mirror.as_slice() < widths {
        mirror
    } else {
        widths.to_vec()
    }
}

#[test]
fn cubeshape_pack_oracle_sweeps_all_169_rows() {
    let engine = Sq1;
    let rows = load_cubeshape_rows();
    assert_eq!(rows.len(), EXPECTED_ROWS);
    let mut famkey_by_name: HashMap<String, Vec<u8>> = HashMap::new();
    let mut swapped_rows: Vec<String> = Vec::new();
    let mut defective_rows: Vec<String> = Vec::new();
    for row in &rows {
        let parsed = engine
            .parse_alg(&row.alg)
            .unwrap_or_else(|err| panic!("{} parses: {}", row.key, err));
        assert!(!parsed.is_empty(), "{} holds at least one move", row.key);
        let inverted = engine.invert_alg(&parsed);
        let mut start = engine.solved_state();
        let round_trip = (|| {
            engine.apply_alg(&mut start, &inverted)?;
            let classes = (
                sq1::canonical_shape(&start, 0),
                sq1::canonical_shape(&start, 1),
            );
            engine.apply_alg(&mut start, &parsed)?;
            assert!(
                engine.is_solved(&start),
                "{} returns to solved through its start state",
                row.key
            );
            Ok::<(Vec<u8>, Vec<u8>), geometry::ApplyError>(classes)
        })();
        let classes = match round_trip {
            Ok(classes) => classes,
            Err(err) => {
                assert_eq!(
                    err,
                    geometry::ApplyError::IllegalMove("/".to_string()),
                    "{} fails with the recorded slash illegality",
                    row.key
                );
                defective_rows.push(row.key.clone());
                continue;
            }
        };
        for (layer, class) in [&classes.0, &classes.1].iter().enumerate() {
            assert_eq!(
                class.iter().sum::<u8>(),
                12,
                "ring {} of {} sums to 12 units",
                layer,
                row.key
            );
        }
        let corners = [&classes.0, &classes.1]
            .iter()
            .flat_map(|class| class.iter())
            .filter(|width| **width == 2)
            .count();
        assert_eq!(corners, 8, "{} conserves the 8 corners", row.key);
        let observed = (famkey(&classes.0), famkey(&classes.1));
        let known = (
            famkey_by_name.get(&row.names[0]),
            famkey_by_name.get(&row.names[1]),
        );
        let direct =
            known.0.is_none_or(|k| k == &observed.0) && known.1.is_none_or(|k| k == &observed.1);
        let crossed =
            known.0.is_none_or(|k| k == &observed.1) && known.1.is_none_or(|k| k == &observed.0);
        assert!(
            direct || crossed,
            "{} reconstructs the two shape families its names declare, got {:?} for {}",
            row.key,
            observed,
            row.names.join(" / ")
        );
        if direct {
            famkey_by_name
                .entry(row.names[0].clone())
                .or_insert_with(|| observed.0.clone());
            famkey_by_name
                .entry(row.names[1].clone())
                .or_insert_with(|| observed.1.clone());
        } else {
            swapped_rows.push(row.key.clone());
            famkey_by_name
                .entry(row.names[0].clone())
                .or_insert_with(|| observed.1.clone());
            famkey_by_name
                .entry(row.names[1].clone())
                .or_insert_with(|| observed.0.clone());
        }
        if row.names[0] == row.names[1] {
            assert_eq!(classes.0, classes.1, "{} is symmetric", row.key);
        }
    }
    assert_eq!(
        defective_rows,
        ["cs.119"],
        "exactly the known source rows hold an unexecutable slash sequence"
    );
    assert_eq!(
        swapped_rows,
        [
            "cs.075", "cs.078", "cs.084", "cs.089", "cs.097", "cs.109", "cs.118", "cs.129"
        ],
        "exactly the known layer order swaps in the source labels"
    );
    assert_eq!(
        famkey_by_name.get("Square"),
        Some(&SQUARE_SHAPE.to_vec()),
        "Square is the alternating class"
    );
    assert_eq!(
        famkey_by_name.get("Kite"),
        Some(&KITE_SHAPE.to_vec()),
        "Kite matches the hand derived cs.001 class"
    );
    assert_eq!(
        famkey_by_name.get("Star"),
        Some(&vec![2, 2, 2, 2, 2, 2]),
        "Star is 6 fused corners"
    );
    assert_eq!(
        famkey_by_name.get("8 Edges"),
        Some(&vec![1, 1, 1, 1, 1, 1, 1, 1, 2, 2]),
        "8 Edges is 8 single edges plus 2 fused corners"
    );
    for (left, right) in [
        ("Left Fist", "Right Fist"),
        ("Left Pawn", "Right Pawn"),
        ("5-1 Left", "5-1 Right"),
        ("4-2 Left", "4-2 Right"),
        ("3-2-1 Left", "3-2-1 Right"),
    ] {
        assert_eq!(
            famkey_by_name.get(left),
            famkey_by_name.get(right),
            "{} and {} are one mirrored shape family",
            left,
            right
        );
    }
}

#[test]
fn inversion_stays_inside_the_parse_domain() {
    let sq1 = Sq1;
    assert_eq!(
        sq1.invert_alg(&vec![Move::Twist { top: 0, bottom: 6 }]),
        vec![Move::Twist { top: 0, bottom: 6 }]
    );
    for top in -5..=6 {
        for bottom in -5..=6 {
            if top == 0 && bottom == 0 {
                continue;
            }
            let alg = vec![Move::Twist { top, bottom }];
            let inverted = sq1.invert_alg(&alg);
            let text = sq1.format_alg(&inverted);
            let reparsed = sq1.parse_alg(&text).expect("inverse formats and reparses");
            assert_eq!(reparsed, inverted, "round trip failed for ({top},{bottom})");
        }
    }
}
#[test]
fn canonical_shape_merges_the_wrapped_run_and_pins_the_minimum_rotation() {
    let mut state = sq1::solved_state();
    state.units[..12].copy_from_slice(&[1, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1]);
    assert_eq!(
        sq1::canonical_shape(&state, 0),
        vec![1, 1, 1, 1, 1, 1, 1, 1, 1, 3]
    );
}

#[test]
fn canonical_shape_keeps_a_single_run_unmerged() {
    let mut state = sq1::solved_state();
    state.units[..12].copy_from_slice(&[1; 12]);
    assert_eq!(sq1::canonical_shape(&state, 0), vec![12]);
}

#[test]
fn parse_rejects_a_two_digit_turn_out_of_the_engine_domain() {
    let engine = Sq1;
    let err = engine
        .parse_alg("(12,3)")
        .expect_err("two digit top must fail");
    assert!(err.to_string().contains("unknown token"), "err: {err}");
}
