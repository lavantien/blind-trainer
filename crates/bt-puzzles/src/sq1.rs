use crate::geometry::{ApplyError, FaceletFace, FaceletGrid, Geometry, ParseError};

pub const UNITS: usize = 12;
pub const LAYERS: usize = 2;
pub const PIECES_PER_LAYER: usize = 8;
pub const TWISTS: usize = UNITS * UNITS - 1;
pub const SQUARE_SHAPE: [u8; PIECES_PER_LAYER] = [1, 2, 1, 2, 1, 2, 1, 2];
const UNIT_TOTAL: usize = LAYERS * UNITS;
const CUT_HALF: usize = UNITS / LAYERS;
const TURN_MIN: i32 = -5;
const TURN_MAX: i32 = 6;
const RADIX: i32 = 10;
const OPEN: u8 = b'(';
const CLOSE: u8 = b')';
const COMMA: u8 = b',';
const SLASH: u8 = b'/';
const MINUS: u8 = b'-';
const ZERO: u8 = b'0';

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Move {
    Twist { top: i32, bottom: i32 },
    Slash,
}

pub type Alg = Vec<Move>;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct State {
    pub units: [u8; UNIT_TOTAL],
    pub slice_flipped: bool,
}

#[derive(Clone, Copy, Debug)]
pub struct Sq1;

pub fn solved_state() -> State {
    let mut units = [0; UNIT_TOTAL];
    for layer in 0..LAYERS {
        let mut unit = 0;
        for piece in 0..PIECES_PER_LAYER {
            let width = if piece % LAYERS == layer { 2 } else { 1 };
            let id = (layer * PIECES_PER_LAYER + piece) as u8;
            for _ in 0..width {
                units[layer * UNITS + unit] = id;
                unit += 1;
            }
        }
    }
    State {
        units,
        slice_flipped: false,
    }
}

pub fn can_slash(state: &State) -> bool {
    (0..LAYERS).all(|layer| {
        let units = &state.units[layer * UNITS..(layer + 1) * UNITS];
        units[0] != units[UNITS - 1] && units[CUT_HALF - 1] != units[CUT_HALF]
    })
}

fn rotate(units: &mut [u8], turn: i32) {
    let mut rotated = [0; UNITS];
    for (unit, slot) in rotated.iter_mut().enumerate() {
        let source = (unit as i32 - turn).rem_euclid(UNITS as i32) as usize;
        *slot = units[source];
    }
    units.copy_from_slice(&rotated);
}

pub fn apply(state: &mut State, mv: Move) -> Result<(), ApplyError> {
    match mv {
        Move::Slash => {
            if !can_slash(state) {
                return Err(ApplyError::IllegalMove(format_move(Move::Slash)));
            }
            for unit in CUT_HALF..UNITS {
                state.units.swap(unit, UNITS + unit);
            }
            state.slice_flipped = !state.slice_flipped;
        }
        Move::Twist { top, bottom } => {
            rotate(&mut state.units[..UNITS], top);
            rotate(&mut state.units[UNITS..], bottom);
        }
    }
    Ok(())
}

pub fn is_solved(state: &State) -> bool {
    *state == solved_state()
}

pub fn format_move(mv: Move) -> String {
    match mv {
        Move::Slash => "/".to_string(),
        Move::Twist { top, bottom } => format!("({top},{bottom})"),
    }
}

fn twist_from(top: i32, bottom: i32) -> Option<Move> {
    let in_range = |turn: i32| (TURN_MIN..=TURN_MAX).contains(&turn);
    (in_range(top) && in_range(bottom) && !(top == 0 && bottom == 0))
        .then_some(Move::Twist { top, bottom })
}

fn parse_int(bytes: &[u8]) -> Option<(i32, usize)> {
    let negative = bytes.first() == Some(&MINUS);
    let digits = if negative { &bytes[1..] } else { bytes };
    let mut value = 0;
    let mut used = 0;
    for byte in digits {
        if !byte.is_ascii_digit() || value > TURN_MAX {
            break;
        }
        value = value * RADIX + i32::from(byte - ZERO);
        used += 1;
    }
    if used == 0 || (used > 1 && digits[0] == ZERO) || (negative && value == 0) {
        return None;
    }
    let signed = if negative { -value } else { value };
    Some((signed, used + usize::from(negative)))
}

fn parse_paren(segment: &[u8]) -> Result<Move, ParseError> {
    let err = || ParseError::UnknownToken(String::from_utf8_lossy(segment).into_owned());
    let inner = &segment[1..segment.len() - 1];
    let Some((top, used)) = parse_int(inner) else {
        return Err(err());
    };
    if inner.get(used) != Some(&COMMA) {
        return Err(err());
    }
    let Some((bottom, used_bottom)) = parse_int(&inner[used + 1..]) else {
        return Err(err());
    };
    if used + 1 + used_bottom != inner.len() {
        return Err(err());
    }
    twist_from(top, bottom).ok_or_else(err)
}

fn parse_bare(fragment: &[u8]) -> Result<(Move, usize), ParseError> {
    let err_token = || {
        let end = fragment
            .iter()
            .position(u8::is_ascii_whitespace)
            .unwrap_or(fragment.len());
        ParseError::UnknownToken(String::from_utf8_lossy(&fragment[..end]).into_owned())
    };
    let Some((top, used)) = parse_int(fragment) else {
        return Err(err_token());
    };
    if fragment.get(used) != Some(&COMMA) {
        return Err(err_token());
    }
    let Some((bottom, used_bottom)) = parse_int(&fragment[used + 1..]) else {
        return Err(err_token());
    };
    let consumed = used + 1 + used_bottom;
    let terminated = match fragment.get(consumed) {
        None | Some(&SLASH) => true,
        Some(byte) => byte.is_ascii_whitespace(),
    };
    if !terminated {
        return Err(err_token());
    }
    let mv = twist_from(top, bottom).ok_or_else(err_token)?;
    Ok((mv, consumed))
}

fn scan(text: &str) -> Result<Alg, ParseError> {
    if !text.is_ascii() {
        return Err(ParseError::UnknownToken(text.to_string()));
    }
    let bytes = text.as_bytes();
    let mut moves = Vec::new();
    let mut index = 0;
    while index < bytes.len() {
        let byte = bytes[index];
        if byte.is_ascii_whitespace() {
            index += 1;
        } else if byte == SLASH {
            moves.push(Move::Slash);
            index += 1;
        } else if byte == OPEN {
            let Some(offset) = bytes[index..].iter().position(|b| *b == CLOSE) else {
                return Err(ParseError::UnknownToken(text[index..].to_string()));
            };
            let end = index + offset;
            moves.push(parse_paren(&bytes[index..=end])?);
            index = end + 1;
        } else {
            let (mv, used) = parse_bare(&bytes[index..])?;
            moves.push(mv);
            index += used;
        }
    }
    Ok(moves)
}

fn cyclic_widths(units: &[u8]) -> Vec<u8> {
    let mut widths: Vec<u8> = Vec::new();
    for (unit, current) in units.iter().enumerate() {
        if unit > 0 && current == &units[unit - 1] {
            *widths.last_mut().expect("widths started") += 1;
        } else {
            widths.push(1);
        }
    }
    if widths.len() > 1 && units[0] == units[UNITS - 1] {
        let wrapped = widths.pop().expect("widths started");
        widths[0] += wrapped;
    }
    widths
}

pub fn canonical_shape(state: &State, layer: usize) -> Vec<u8> {
    let ring = &state.units[layer * UNITS..(layer + 1) * UNITS];
    let widths = cyclic_widths(ring);
    let count = widths.len();
    (0..count)
        .map(|rotation| {
            (0..count)
                .map(|unit| widths[(rotation + unit) % count])
                .collect::<Vec<u8>>()
        })
        .min()
        .expect("widths started")
}

impl Geometry for Sq1 {
    type Move = Move;
    type Alg = Alg;
    type State = State;

    fn parse_move(&self, token: &str) -> Result<Move, ParseError> {
        let moves = scan(token)?;
        if let [only] = moves.as_slice() {
            Ok(*only)
        } else {
            Err(ParseError::UnknownToken(token.to_string()))
        }
    }

    fn parse_alg(&self, text: &str) -> Result<Alg, ParseError> {
        scan(text)
    }

    fn format_alg(&self, alg: &Alg) -> String {
        alg.iter()
            .map(|mv| format_move(*mv))
            .collect::<Vec<String>>()
            .join(" ")
    }

    fn invert_alg(&self, alg: &Alg) -> Alg {
        let canonical = |turn: i32| if turn == -TURN_MAX { TURN_MAX } else { turn };
        alg.iter()
            .rev()
            .map(|mv| match *mv {
                Move::Slash => Move::Slash,
                Move::Twist { top, bottom } => Move::Twist {
                    top: canonical(-top),
                    bottom: canonical(-bottom),
                },
            })
            .collect()
    }

    fn apply_alg(&self, state: &mut State, alg: &Alg) -> Result<(), ApplyError> {
        for mv in alg {
            apply(state, *mv)?;
        }
        Ok(())
    }

    fn solved_state(&self) -> State {
        solved_state()
    }

    fn is_solved(&self, state: &State) -> bool {
        is_solved(state)
    }

    fn facelets(&self, state: &State) -> FaceletGrid {
        FaceletGrid {
            faces: (0..LAYERS)
                .map(|layer| FaceletFace {
                    rows: vec![state.units[layer * UNITS..(layer + 1) * UNITS].to_vec()],
                })
                .collect(),
        }
    }
}
