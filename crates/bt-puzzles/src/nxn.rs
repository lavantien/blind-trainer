use crate::geometry::{ApplyError, FaceletFace, FaceletGrid, Geometry, ParseError};

pub const MIN_N: usize = 2;
pub const MAX_N: usize = 7;
const STRIDE: usize = MAX_N;
const FACELET_LEN: usize = 6 * STRIDE * STRIDE;

const NORMAL: [[i32; 3]; 6] = [
    [0, 1, 0],
    [-1, 0, 0],
    [0, 0, 1],
    [1, 0, 0],
    [0, 0, -1],
    [0, -1, 0],
];
const ROW: [[i32; 3]; 6] = [
    [0, 0, 1],
    [0, -1, 0],
    [0, -1, 0],
    [0, -1, 0],
    [0, -1, 0],
    [0, 0, -1],
];
const COL: [[i32; 3]; 6] = [
    [1, 0, 0],
    [0, 0, 1],
    [1, 0, 0],
    [0, 0, -1],
    [-1, 0, 0],
    [1, 0, 0],
];
const AXIS: [usize; 6] = [1, 0, 2, 0, 2, 1];
const SIGN: [i32; 6] = [1, -1, 1, 1, -1, -1];

const FACE_LETTER: [char; 6] = ['U', 'L', 'F', 'R', 'B', 'D'];
const ROTATION_LETTER: [(usize, char); 3] = [(3, 'x'), (0, 'y'), (2, 'z')];
const SLICE_LETTER: [(usize, char); 3] = [(1, 'M'), (5, 'E'), (2, 'S')];

#[derive(Clone, Copy, PartialEq, Eq)]
enum Kind {
    Face,
    Rotation,
    Slice,
}

const NOTATION: [(char, u8, Kind); 12] = [
    ('U', 0, Kind::Face),
    ('D', 5, Kind::Face),
    ('L', 1, Kind::Face),
    ('R', 3, Kind::Face),
    ('F', 2, Kind::Face),
    ('B', 4, Kind::Face),
    ('x', 3, Kind::Rotation),
    ('y', 0, Kind::Rotation),
    ('z', 2, Kind::Rotation),
    ('M', 1, Kind::Slice),
    ('E', 5, Kind::Slice),
    ('S', 2, Kind::Slice),
];

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct NxnMove {
    face: u8,
    depth_from: u8,
    depth_to: u8,
    quarter: u8,
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct NxnAlg {
    moves: Vec<NxnMove>,
}

impl NxnAlg {
    pub fn moves(&self) -> &[NxnMove] {
        &self.moves
    }

    pub fn is_empty(&self) -> bool {
        self.moves.is_empty()
    }
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct NxnState {
    n: usize,
    facelet: [u8; FACELET_LEN],
}

pub struct Nxn {
    n: usize,
}

impl Nxn {
    pub fn new(n: usize) -> Option<Nxn> {
        if (MIN_N..=MAX_N).contains(&n) {
            Some(Nxn { n })
        } else {
            None
        }
    }

    pub fn n(&self) -> usize {
        self.n
    }

    fn middle(n: usize) -> u8 {
        n.div_ceil(2) as u8
    }

    fn find_notation(letter: char) -> Option<(u8, Kind)> {
        let mut k = 0;
        while k < NOTATION.len() {
            if NOTATION[k].0 == letter {
                return Some((NOTATION[k].1, NOTATION[k].2));
            }
            k += 1;
        }
        None
    }

    fn lookup_letter(table: [(usize, char); 3], face: u8) -> Option<char> {
        let mut k = 0;
        while k < table.len() {
            if table[k].0 == face as usize {
                return Some(table[k].1);
            }
            k += 1;
        }
        None
    }

    fn parse_token(&self, token: &str) -> Result<NxnMove, ParseError> {
        let bytes = token.as_bytes();
        let reject = || ParseError::UnknownToken(token.to_string());
        let mut i = 0;
        let mut digit = 0u8;
        if let Some(&first) = bytes.first().filter(|b| b.is_ascii_digit()) {
            digit = first - b'0';
            if !(2..=7).contains(&digit) {
                return Err(reject());
            }
            i = 1;
        }
        let Some(&letter) = bytes.get(i) else {
            return Err(reject());
        };
        i += 1;
        let mut entry = Self::find_notation(letter as char);
        let mut wide = false;
        if entry.is_none()
            && letter.is_ascii_lowercase()
            && let Some(upper) = Self::find_notation((letter - b'a' + b'A') as char)
            && upper.1 == Kind::Face
        {
            entry = Some(upper);
            wide = true;
        }
        let Some((face, kind)) = entry else {
            return Err(reject());
        };
        if digit != 0 && kind != Kind::Face {
            return Err(reject());
        }
        if !wide && kind == Kind::Face && bytes.get(i) == Some(&b'w') {
            wide = true;
            i += 1;
        }
        let mut halves = 0;
        let mut primes = 0;
        while i < bytes.len() {
            match bytes[i] {
                b'2' => halves += 1,
                b'\'' => primes += 1,
                _ => return Err(reject()),
            }
            i += 1;
        }
        if halves > 1 || primes > 1 {
            return Err(reject());
        }
        let quarter = if halves == 1 {
            2
        } else if primes == 1 {
            3
        } else {
            1
        };
        let middle = Self::middle(self.n);
        let (from, to) = match kind {
            Kind::Rotation => (1, self.n as u8),
            Kind::Slice => (middle, middle),
            Kind::Face if wide => (1, if digit != 0 { digit } else { 2 }),
            Kind::Face => {
                let depth = if digit != 0 { digit } else { 1 };
                (depth, depth)
            }
        };
        if from < 1 || from > to || to > self.n as u8 {
            return Err(reject());
        }
        Ok(NxnMove {
            face,
            depth_from: from,
            depth_to: to,
            quarter,
        })
    }

    fn format_token(&self, mv: NxnMove) -> String {
        let (from, to) = (mv.depth_from, mv.depth_to);
        let middle = Self::middle(self.n);
        let special = if from == 1 && to == self.n as u8 {
            Self::lookup_letter(ROTATION_LETTER, mv.face)
        } else if from == middle && to == middle {
            Self::lookup_letter(SLICE_LETTER, mv.face)
        } else {
            None
        };
        let face = FACE_LETTER[mv.face as usize];
        let body = match special {
            Some(letter) => letter.to_string(),
            None if from == 1 && to > 1 => {
                if to > 2 {
                    format!("{to}{face}w")
                } else {
                    format!("{face}w")
                }
            }
            None if from == to => {
                if from > 1 {
                    format!("{from}{face}")
                } else {
                    face.to_string()
                }
            }
            None => unreachable!(),
        };
        match mv.quarter {
            2 => format!("{body}2"),
            3 => format!("{body}'"),
            _ => body,
        }
    }

    fn apply_move(&self, state: &mut NxnState, mv: NxnMove) {
        let src = state.facelet;
        let face = mv.face as usize;
        let turned = NORMAL[face];
        let axis = AXIS[face];
        let h = self.n as i32 - 1;
        let u_lo = h - 2 * (mv.depth_to as i32 - 1);
        let u_hi = h - 2 * (mv.depth_from as i32 - 1);
        let steps = ((-SIGN[face] * mv.quarter as i32).rem_euclid(4)) as usize;
        let mut f = 0;
        while f < 6 {
            let mut r = 0;
            while r < self.n {
                let mut c = 0;
                while c < self.n {
                    let mut center = sticker_center(self.n, f, r, c);
                    let mut normal = NORMAL[f];
                    let u = dot(center, turned);
                    let mut k = 0;
                    while k < if u >= u_lo && u <= u_hi { steps } else { 0 } {
                        center = rot90(center, axis);
                        normal = rot90(normal, axis);
                        k += 1;
                    }
                    let dst = face_of_normal(normal);
                    let (dr, dc) = cell_of(center, dst, self.n);
                    state.facelet[index(dst, dr, dc)] = src[index(f, r, c)];
                    c += 1;
                }
                r += 1;
            }
            f += 1;
        }
    }
}

const fn index(face: usize, row: usize, col: usize) -> usize {
    face * STRIDE * STRIDE + row * STRIDE + col
}

const fn dot(a: [i32; 3], b: [i32; 3]) -> i32 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}

const fn rot90(v: [i32; 3], axis: usize) -> [i32; 3] {
    let (x, y, z) = (v[0], v[1], v[2]);
    match axis {
        0 => [x, -z, y],
        1 => [z, y, -x],
        _ => [-y, x, z],
    }
}

const fn sticker_center(n: usize, face: usize, row: usize, col: usize) -> [i32; 3] {
    let h = n as i32 - 1;
    let along_row = 2 * row as i32 - h;
    let along_col = 2 * col as i32 - h;
    [
        NORMAL[face][0] * h + ROW[face][0] * along_row + COL[face][0] * along_col,
        NORMAL[face][1] * h + ROW[face][1] * along_row + COL[face][1] * along_col,
        NORMAL[face][2] * h + ROW[face][2] * along_row + COL[face][2] * along_col,
    ]
}

const fn face_of_normal(normal: [i32; 3]) -> usize {
    if normal[0] != 0 {
        if normal[0] > 0 { 3 } else { 1 }
    } else if normal[1] != 0 {
        if normal[1] > 0 { 0 } else { 5 }
    } else if normal[2] > 0 {
        2
    } else {
        4
    }
}

const fn cell_of(center: [i32; 3], face: usize, n: usize) -> (usize, usize) {
    let h = n as i32 - 1;
    let along_row = dot(center, ROW[face]);
    let along_col = dot(center, COL[face]);
    (
        ((along_row + h) / 2) as usize,
        ((along_col + h) / 2) as usize,
    )
}

impl Geometry for Nxn {
    type Move = NxnMove;
    type Alg = NxnAlg;
    type State = NxnState;

    fn parse_move(&self, token: &str) -> Result<NxnMove, ParseError> {
        self.parse_token(token)
    }

    fn parse_alg(&self, text: &str) -> Result<NxnAlg, ParseError> {
        let mut moves = Vec::new();
        for token in text.split_whitespace() {
            moves.push(self.parse_token(token)?);
        }
        Ok(NxnAlg { moves })
    }

    fn format_alg(&self, alg: &NxnAlg) -> String {
        alg.moves
            .iter()
            .map(|mv| self.format_token(*mv))
            .collect::<Vec<String>>()
            .join(" ")
    }

    fn invert_alg(&self, alg: &NxnAlg) -> NxnAlg {
        NxnAlg {
            moves: alg
                .moves
                .iter()
                .rev()
                .map(|mv| NxnMove {
                    quarter: 4 - mv.quarter,
                    ..*mv
                })
                .collect(),
        }
    }

    fn apply_alg(&self, state: &mut NxnState, alg: &NxnAlg) -> Result<(), ApplyError> {
        for mv in &alg.moves {
            self.apply_move(state, *mv);
        }
        Ok(())
    }

    fn solved_state(&self) -> NxnState {
        let mut facelet = [0u8; FACELET_LEN];
        let mut face = 0;
        while face < 6 {
            let mut i = 0;
            while i < STRIDE * STRIDE {
                facelet[face * STRIDE * STRIDE + i] = face as u8;
                i += 1;
            }
            face += 1;
        }
        NxnState { n: self.n, facelet }
    }

    fn is_solved(&self, state: &NxnState) -> bool {
        let mut face = 0;
        while face < 6 {
            let mut row = 0;
            while row < self.n {
                let mut col = 0;
                while col < self.n {
                    if state.facelet[index(face, row, col)] != face as u8 {
                        return false;
                    }
                    col += 1;
                }
                row += 1;
            }
            face += 1;
        }
        true
    }

    fn facelets(&self, state: &NxnState) -> FaceletGrid {
        FaceletGrid {
            faces: (0..6)
                .map(|face| FaceletFace {
                    rows: (0..self.n)
                        .map(|row| {
                            (0..self.n)
                                .map(|col| state.facelet[index(face, row, col)])
                                .collect()
                        })
                        .collect(),
                })
                .collect(),
        }
    }
}
