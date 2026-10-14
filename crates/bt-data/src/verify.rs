use crate::corpus::SheetClass;
use bt_core::{
    Alg, Cube, Piece, corner_stickers, corner_twist, edge_flip, edge_stickers, head_sticker,
    piece_of,
};
use std::fmt;

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Verdict {
    pub problems: Vec<Problem>,
    pub notes: Vec<Note>,
}

impl Verdict {
    pub fn ok(&self) -> bool {
        self.problems.is_empty()
    }
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub enum Problem {
    BadPosition {
        detail: String,
    },
    CentersMoved,
    WrongCycle {
        sticker: String,
        expected: String,
        actual: String,
    },
    Displaced {
        piece: String,
        expected_slot: String,
        actual_slot: String,
    },
    UnexpectedDefect {
        piece: String,
        defect: DefectKind,
    },
    MissingDefect {
        piece: String,
        defect: DefectKind,
    },
    NonFixedResidue {
        sticker: String,
        position: String,
    },
    TwistsNotOpposite {
        first: String,
        second: String,
    },
    TwistSumNotZero {
        sum: i32,
    },
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum DefectKind {
    Flip,
    Twist,
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub enum Note {
    Flip { piece: String },
    Twist { piece: String, twist: i8 },
}

impl fmt::Display for Problem {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Problem::BadPosition { detail } => write!(f, "bad position: {detail}"),
            Problem::CentersMoved => write!(f, "centers moved"),
            Problem::WrongCycle {
                sticker,
                expected,
                actual,
            } => write!(
                f,
                "sticker {sticker} should sit on {expected} but sits on {actual}"
            ),
            Problem::Displaced {
                piece,
                expected_slot,
                actual_slot,
            } => write!(
                f,
                "piece {piece} should sit in slot {expected_slot} but sits in {actual_slot}"
            ),
            Problem::UnexpectedDefect { piece, defect } => {
                write!(f, "piece {piece} is {defect} but should be intact")
            }
            Problem::MissingDefect { piece, defect } => {
                write!(f, "piece {piece} should be {defect} but is intact")
            }
            Problem::NonFixedResidue { sticker, position } => {
                write!(
                    f,
                    "sticker {sticker} moved to {position} outside the named pieces"
                )
            }
            Problem::TwistsNotOpposite { first, second } => {
                write!(f, "twists of {first} and {second} are not opposite")
            }
            Problem::TwistSumNotZero { sum } => {
                write!(f, "corner twist sum is {sum}, not 0 mod 3")
            }
        }
    }
}

impl fmt::Display for DefectKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DefectKind::Flip => write!(f, "flipped"),
            DefectKind::Twist => write!(f, "twisted"),
        }
    }
}

impl fmt::Display for Verdict {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.ok() {
            return f.write_str("ok");
        }
        for (index, problem) in self.problems.iter().enumerate() {
            if index > 0 {
                writeln!(f)?;
            }
            write!(f, "{problem}")?;
        }
        Ok(())
    }
}

struct Named {
    stickers: Vec<u8>,
    pieces: Vec<Piece>,
}

fn sticker_by_name(name: &str) -> Option<u8> {
    (0..48u8).find(|&sticker| bt_scheme::name_of(sticker) == name)
}

fn resolve(name: &str) -> Result<u8, String> {
    sticker_by_name(name).ok_or_else(|| format!("unknown sticker name {name}"))
}

fn piece_index(piece: Piece) -> u8 {
    match piece {
        Piece::Corner(index) => index,
        Piece::Edge(index) => index,
    }
}

fn is_edge(piece: Piece) -> bool {
    matches!(piece, Piece::Edge(_))
}

fn piece_label(piece: Piece) -> String {
    bt_scheme::name_of(head_sticker(piece)).to_string()
}

fn sticker_set(piece: Piece) -> Vec<u8> {
    match piece {
        Piece::Corner(index) => corner_stickers(index).to_vec(),
        Piece::Edge(index) => edge_stickers(index).to_vec(),
    }
}

fn parse_named(token: &str, expect_edge: bool) -> Result<u8, String> {
    let sticker = resolve(token.trim())?;
    let edge = sticker >= 24;
    if edge != expect_edge {
        return Err(format!(
            "token {token} is in the {} orbit",
            if edge { "edge" } else { "corner" }
        ));
    }
    Ok(sticker)
}

fn named_all(position: &str, expect_edge: bool) -> Result<Named, String> {
    let mut stickers = Vec::new();
    let mut pieces = Vec::new();
    for token in position.split('-') {
        let sticker = parse_named(token, expect_edge)?;
        let piece = piece_of(sticker);
        if pieces.contains(&piece) {
            return Err(format!("piece {token} named twice"));
        }
        stickers.push(sticker);
        pieces.push(piece);
    }
    Ok(Named { stickers, pieces })
}

fn apply_to_solved(alg: &Alg) -> Cube {
    let mut cube = Cube::solved();
    cube.apply_alg(alg);
    cube
}

fn slot_of(cube: &Cube, piece: Piece) -> u8 {
    let anchor = sticker_set(piece)[0];
    let position = cube.position_of(anchor);
    match piece {
        Piece::Corner(_) => (0..8u8)
            .find(|&slot| corner_stickers(slot).contains(&position))
            .expect("corner slot always exists"),
        Piece::Edge(_) => (0..12u8)
            .find(|&slot| edge_stickers(slot).contains(&position))
            .expect("edge slot always exists"),
    }
}

pub fn verify(alg: &Alg, position: &str, sheet: SheetClass) -> Verdict {
    let mut cube = apply_to_solved(alg);
    let mut verdict = Verdict {
        problems: Vec::new(),
        notes: Vec::new(),
    };
    if !cube.centers_fixed() {
        verdict.problems.push(Problem::CentersMoved);
    }
    match sheet {
        SheetClass::Edge => verify_three_style(&mut cube, &mut verdict, position, true),
        SheetClass::Corner => verify_three_style(&mut cube, &mut verdict, position, false),
        SheetClass::Flips => verify_flip_pair(&mut cube, &mut verdict, position),
        SheetClass::Twists => verify_twist_pair(&mut cube, &mut verdict, position),
        SheetClass::Parity => verify_parity(&mut cube, &mut verdict, position),
        SheetClass::Ltct => verify_ltct(&mut cube, &mut verdict, position),
    }
    verdict
}

fn fail(verdict: &mut Verdict, detail: String) {
    verdict.problems.push(Problem::BadPosition { detail });
}

fn residue_problems(cube: &Cube, verdict: &mut Verdict, exempt: &[Piece]) {
    for sticker in 0..48u8 {
        let piece = piece_of(sticker);
        if exempt.contains(&piece) {
            continue;
        }
        let actual = cube.position_of(sticker);
        if actual != sticker {
            verdict.problems.push(Problem::NonFixedResidue {
                sticker: bt_scheme::name_of(sticker).to_string(),
                position: bt_scheme::name_of(actual).to_string(),
            });
        }
    }
}

fn defect_of(cube: &Cube, piece: Piece) -> Defect {
    let slot = slot_of(cube, piece);
    if is_edge(piece) {
        Defect {
            flipped: edge_flip(cube, slot),
            twist: 0,
        }
    } else {
        Defect {
            flipped: false,
            twist: corner_twist(cube, slot),
        }
    }
}

struct Defect {
    flipped: bool,
    twist: i8,
}

const NORMALS: [[i8; 3]; 6] = [
    [0, 1, 0],
    [-1, 0, 0],
    [0, 0, 1],
    [1, 0, 0],
    [0, 0, -1],
    [0, -1, 0],
];

fn face_of(sticker: u8) -> usize {
    (sticker / 4) as usize
}

fn chirality(triple: [u8; 3]) -> i32 {
    let a = NORMALS[face_of(triple[0])];
    let b = NORMALS[face_of(triple[1])];
    let c = NORMALS[face_of(triple[2])];
    let cross = [
        b[1] * c[2] - b[2] * c[1],
        b[2] * c[0] - b[0] * c[2],
        b[0] * c[1] - b[1] * c[0],
    ];
    let dot = a[0] as i32 * cross[0] as i32
        + a[1] as i32 * cross[1] as i32
        + a[2] as i32 * cross[2] as i32;
    dot.signum()
}

fn place_rigid(expected: &mut [Option<u8>; 54], from: u8, to: u8) -> Result<(), String> {
    let piece = piece_of(from);
    let slot_piece = piece_of(to);
    let mut piece_stickers = sticker_set(piece);
    let piece_split = piece_stickers
        .iter()
        .position(|&sticker| sticker == from)
        .ok_or_else(|| format!("sticker {from} not on its piece"))?;
    piece_stickers.rotate_left(piece_split);
    let mut slot_stickers = sticker_set(slot_piece);
    let slot_split = slot_stickers
        .iter()
        .position(|&sticker| sticker == to)
        .ok_or_else(|| format!("sticker {to} not on its piece"))?;
    slot_stickers.rotate_left(slot_split);
    if piece_stickers.len() == 3 {
        let piece_chi = chirality([piece_stickers[0], piece_stickers[1], piece_stickers[2]]);
        let slot_chi = chirality([slot_stickers[0], slot_stickers[1], slot_stickers[2]]);
        if piece_chi != slot_chi {
            slot_stickers.swap(1, 2);
        }
    }
    for (sticker, destination) in piece_stickers.iter().zip(slot_stickers.iter()) {
        expected[*destination as usize] = Some(*sticker);
    }
    Ok(())
}

fn expected_position(expected: &[Option<u8>; 54], sticker: u8) -> Option<u8> {
    expected
        .iter()
        .position(|slot| matches!(slot, Some(occupant) if *occupant == sticker))
        .map(|position| position as u8)
}

fn verify_three_style(cube: &mut Cube, verdict: &mut Verdict, position: &str, edge: bool) {
    let named = match named_all(position, edge) {
        Ok(named) => named,
        Err(detail) => return fail(verdict, detail),
    };
    if named.stickers.len() != 3 {
        return fail(
            verdict,
            format!("expected 3 tokens, found {}", named.stickers.len()),
        );
    }
    let mut expected: [Option<u8>; 54] =
        std::array::from_fn(|index| if index >= 48 { Some(index as u8) } else { None });
    let cycle = [
        (named.stickers[0], named.stickers[1]),
        (named.stickers[1], named.stickers[2]),
        (named.stickers[2], named.stickers[0]),
    ];
    for (from, to) in cycle {
        if let Err(detail) = place_rigid(&mut expected, from, to) {
            return fail(verdict, detail);
        }
    }
    for sticker in 0..48u8 {
        let want = expected_position(&expected, sticker).unwrap_or(sticker);
        let got = cube.position_of(sticker);
        if want == got {
            continue;
        }
        if named.pieces.contains(&piece_of(sticker)) {
            verdict.problems.push(Problem::WrongCycle {
                sticker: bt_scheme::name_of(sticker).to_string(),
                expected: bt_scheme::name_of(want).to_string(),
                actual: bt_scheme::name_of(got).to_string(),
            });
        } else {
            verdict.problems.push(Problem::NonFixedResidue {
                sticker: bt_scheme::name_of(sticker).to_string(),
                position: bt_scheme::name_of(got).to_string(),
            });
        }
    }
}

fn pair_tokens(position: &str) -> Result<[&str; 2], String> {
    let parts: Vec<&str> = position.split(" & ").collect();
    if parts.len() != 2 {
        return Err(format!(
            "expected 2 names joined by ' & ', found {position}"
        ));
    }
    Ok([parts[0], parts[1]])
}

fn comma_parts(position: &str) -> Result<[&str; 2], String> {
    let parts: Vec<&str> = position.split(',').map(str::trim).collect();
    if parts.len() != 2 {
        return Err(format!(
            "expected an edge pair and a corner pair, found {position}"
        ));
    }
    Ok([parts[0], parts[1]])
}

fn check_in_place(cube: &Cube, verdict: &mut Verdict, piece: Piece) {
    let home = piece_index(piece);
    let slot = slot_of(cube, piece);
    if slot != home {
        verdict.problems.push(Problem::Displaced {
            piece: piece_label(piece),
            expected_slot: slot_name(piece, home),
            actual_slot: slot_name(piece, slot),
        });
    }
}

fn slot_name(piece: Piece, slot: u8) -> String {
    if is_edge(piece) {
        bt_scheme::name_of(edge_stickers(slot)[0]).to_string()
    } else {
        bt_scheme::name_of(corner_stickers(slot)[0]).to_string()
    }
}

fn check_swapped(cube: &Cube, verdict: &mut Verdict, first: Piece, second: Piece) {
    let slot_first = slot_of(cube, first);
    let slot_second = slot_of(cube, second);
    let ok = slot_first == piece_index(second) && slot_second == piece_index(first);
    if !ok {
        verdict.problems.push(Problem::Displaced {
            piece: piece_label(first),
            expected_slot: slot_name(first, piece_index(second)),
            actual_slot: slot_name(first, slot_first),
        });
        verdict.problems.push(Problem::Displaced {
            piece: piece_label(second),
            expected_slot: slot_name(second, piece_index(first)),
            actual_slot: slot_name(second, slot_second),
        });
    }
}

fn verify_flip_pair(cube: &mut Cube, verdict: &mut Verdict, position: &str) {
    let tokens = match pair_tokens(position) {
        Ok(tokens) => tokens,
        Err(detail) => return fail(verdict, detail),
    };
    let first = match parse_named(tokens[0], true) {
        Ok(sticker) => sticker,
        Err(detail) => return fail(verdict, detail),
    };
    let second = match parse_named(tokens[1], true) {
        Ok(sticker) => sticker,
        Err(detail) => return fail(verdict, detail),
    };
    let pieces = [piece_of(first), piece_of(second)];
    residue_problems(cube, verdict, &pieces);
    for piece in pieces {
        check_in_place(cube, verdict, piece);
        if !defect_of(cube, piece).flipped {
            verdict.problems.push(Problem::MissingDefect {
                piece: piece_label(piece),
                defect: DefectKind::Flip,
            });
        }
    }
}

fn verify_twist_pair(cube: &mut Cube, verdict: &mut Verdict, position: &str) {
    let tokens = match pair_tokens(position) {
        Ok(tokens) => tokens,
        Err(detail) => return fail(verdict, detail),
    };
    let first = match parse_named(tokens[0], false) {
        Ok(sticker) => sticker,
        Err(detail) => return fail(verdict, detail),
    };
    let second = match parse_named(tokens[1], false) {
        Ok(sticker) => sticker,
        Err(detail) => return fail(verdict, detail),
    };
    let pieces = [piece_of(first), piece_of(second)];
    residue_problems(cube, verdict, &pieces);
    for piece in pieces {
        check_in_place(cube, verdict, piece);
    }
    let twists: Vec<i8> = pieces
        .iter()
        .map(|&piece| defect_of(cube, piece).twist)
        .collect();
    for (twist, piece) in twists.iter().zip(pieces.iter()) {
        verdict.notes.push(Note::Twist {
            piece: piece_label(*piece),
            twist: *twist,
        });
        if *twist == 0 {
            verdict.problems.push(Problem::MissingDefect {
                piece: piece_label(*piece),
                defect: DefectKind::Twist,
            });
        }
    }
    if twists[0] + twists[1] != 0 {
        verdict.problems.push(Problem::TwistsNotOpposite {
            first: piece_label(pieces[0]),
            second: piece_label(pieces[1]),
        });
    }
}

fn note_orientation(cube: &Cube, verdict: &mut Verdict, piece: Piece) {
    let defect = defect_of(cube, piece);
    if is_edge(piece) {
        if defect.flipped {
            verdict.notes.push(Note::Flip {
                piece: piece_label(piece),
            });
        }
    } else if defect.twist != 0 {
        verdict.notes.push(Note::Twist {
            piece: piece_label(piece),
            twist: defect.twist,
        });
    }
}

fn verify_parity(cube: &mut Cube, verdict: &mut Verdict, position: &str) {
    let parts = match comma_parts(position) {
        Ok(parts) => parts,
        Err(detail) => return fail(verdict, detail),
    };
    let edge = match named_all(parts[0], true) {
        Ok(named) => named,
        Err(detail) => return fail(verdict, detail),
    };
    let corner = match named_all(parts[1], false) {
        Ok(named) => named,
        Err(detail) => return fail(verdict, detail),
    };
    if edge.stickers.len() != 2 || corner.stickers.len() != 2 {
        return fail(
            verdict,
            format!("parity needs 2 edges and 2 corners: {position}"),
        );
    }
    let mut exempt = edge.pieces.clone();
    exempt.extend(corner.pieces.iter().copied());
    residue_problems(cube, verdict, &exempt);
    check_swapped(cube, verdict, edge.pieces[0], edge.pieces[1]);
    check_swapped(cube, verdict, corner.pieces[0], corner.pieces[1]);
    for piece in exempt.iter().copied() {
        note_orientation(cube, verdict, piece);
    }
}

fn verify_ltct(cube: &mut Cube, verdict: &mut Verdict, position: &str) {
    let parts = match comma_parts(position) {
        Ok(parts) => parts,
        Err(detail) => return fail(verdict, detail),
    };
    let edge = match named_all(parts[0], true) {
        Ok(named) => named,
        Err(detail) => return fail(verdict, detail),
    };
    let corner_part = parts[1];
    let (swap_text, bracket) = match corner_part.split_once('[') {
        Some((swap_text, rest)) => match rest.strip_suffix(']') {
            Some(bracket) => (swap_text, Some(bracket)),
            None => return fail(verdict, format!("unclosed bracket in {position}")),
        },
        None => (corner_part, None),
    };
    if edge.stickers.len() != 2 {
        return fail(verdict, format!("ltct needs 2 swapped edges: {position}"));
    }
    let Some(bracket) = bracket else {
        return fail(
            verdict,
            format!("ltct needs a bracketed twist corner: {position}"),
        );
    };
    let corner = match named_all(swap_text, false) {
        Ok(named) => named,
        Err(detail) => return fail(verdict, detail),
    };
    if corner.stickers.len() != 2 {
        return fail(verdict, format!("ltct needs 2 swapped corners: {position}"));
    }
    let bracket_sticker = match parse_named(bracket, false) {
        Ok(sticker) => sticker,
        Err(detail) => return fail(verdict, detail),
    };
    let bracket_piece = piece_of(bracket_sticker);
    let mut exempt = edge.pieces.clone();
    exempt.extend(corner.pieces.iter().copied());
    exempt.push(bracket_piece);
    if exempt.len() != 5 {
        return fail(verdict, format!("ltct names a piece twice: {position}"));
    }
    residue_problems(cube, verdict, &exempt);
    check_swapped(cube, verdict, edge.pieces[0], edge.pieces[1]);
    for piece in edge.pieces {
        if defect_of(cube, piece).flipped {
            verdict.problems.push(Problem::UnexpectedDefect {
                piece: piece_label(piece),
                defect: DefectKind::Flip,
            });
        }
    }
    check_swapped(cube, verdict, corner.pieces[0], corner.pieces[1]);
    note_orientation(cube, verdict, corner.pieces[0]);
    note_orientation(cube, verdict, corner.pieces[1]);
    check_in_place(cube, verdict, bracket_piece);
    let bracket_twist = defect_of(cube, bracket_piece).twist;
    verdict.notes.push(Note::Twist {
        piece: piece_label(bracket_piece),
        twist: bracket_twist,
    });
    if bracket_twist == 0 {
        verdict.problems.push(Problem::MissingDefect {
            piece: piece_label(bracket_piece),
            defect: DefectKind::Twist,
        });
    }
    let mut sum = bracket_twist as i32;
    for piece in corner.pieces {
        sum += defect_of(cube, piece).twist as i32;
    }
    if sum.rem_euclid(3) != 0 {
        verdict.problems.push(Problem::TwistSumNotZero { sum });
    }
}
