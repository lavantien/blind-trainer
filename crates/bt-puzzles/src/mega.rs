use crate::geometry::{ApplyError, FaceletFace, FaceletGrid, Geometry, ParseError};

pub const FACES: usize = 12;
pub const FACELETS: usize = 132;
const PER_FACE: usize = FACELETS / FACES;
const RING: usize = 5;
const TOKEN_COUNT: usize = 16;

pub const U: u8 = 0;
pub const F: u8 = 1;
pub const R: u8 = 2;
pub const BR: u8 = 3;
pub const BL: u8 = 4;
pub const L: u8 = 5;
pub const D: u8 = 6;
pub const B: u8 = 7;
pub const UR: u8 = 8;
pub const DR: u8 = 9;
pub const DL: u8 = 10;
pub const UL: u8 = 11;

pub const NEIGHBOR_CW: [[u8; RING]; FACES] = [
    [F, L, BL, BR, R],
    [U, R, DR, DL, L],
    [U, BR, UR, DR, F],
    [U, BL, B, UR, R],
    [U, L, UL, B, BR],
    [U, F, DL, UL, BL],
    [B, UL, DL, DR, UR],
    [BR, BL, UL, D, UR],
    [R, BR, B, D, DR],
    [F, R, UR, D, DL],
    [F, DR, D, UL, L],
    [BL, L, DL, D, B],
];

pub const Y_PERM: [u8; FACES] = [U, L, F, R, BR, BL, D, UR, DR, DL, UL, B];

pub const CENTER: u8 = 0;
const CORNER_BASE: u8 = 1;
const EDGE_BASE: u8 = CORNER_BASE + RING as u8;

pub const fn corner_slot(k: usize) -> u8 {
    CORNER_BASE + k as u8
}

pub const fn edge_slot(k: usize) -> u8 {
    EDGE_BASE + k as u8
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum MegaMove {
    Turn { face: u8, steps: u8 },
    Spin { steps: u8 },
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct MegaAlg {
    moves: Vec<MegaMove>,
}

impl MegaAlg {
    pub fn moves(&self) -> &[MegaMove] {
        &self.moves
    }

    pub fn is_empty(&self) -> bool {
        self.moves.is_empty()
    }
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct MegaState {
    colors: [u8; FACELETS],
}

impl MegaState {
    pub fn color(&self, face: u8, slot: u8) -> u8 {
        self.colors[face as usize * PER_FACE + slot as usize]
    }
}

pub struct Mega;

const PINNED: [(&str, MegaMove); TOKEN_COUNT] = [
    ("U", MegaMove::Turn { face: U, steps: 1 }),
    ("U2", MegaMove::Turn { face: U, steps: 2 }),
    (
        "U2'",
        MegaMove::Turn {
            face: U,
            steps: RING as u8 - 2,
        },
    ),
    (
        "U'",
        MegaMove::Turn {
            face: U,
            steps: RING as u8 - 1,
        },
    ),
    ("R", MegaMove::Turn { face: R, steps: 1 }),
    ("R2", MegaMove::Turn { face: R, steps: 2 }),
    (
        "R2'",
        MegaMove::Turn {
            face: R,
            steps: RING as u8 - 2,
        },
    ),
    (
        "R'",
        MegaMove::Turn {
            face: R,
            steps: RING as u8 - 1,
        },
    ),
    ("L", MegaMove::Turn { face: L, steps: 1 }),
    (
        "L'",
        MegaMove::Turn {
            face: L,
            steps: RING as u8 - 1,
        },
    ),
    ("F", MegaMove::Turn { face: F, steps: 1 }),
    (
        "F'",
        MegaMove::Turn {
            face: F,
            steps: RING as u8 - 1,
        },
    ),
    ("BR", MegaMove::Turn { face: BR, steps: 1 }),
    (
        "BR'",
        MegaMove::Turn {
            face: BR,
            steps: RING as u8 - 1,
        },
    ),
    ("y", MegaMove::Spin { steps: 1 }),
    (
        "y'",
        MegaMove::Spin {
            steps: RING as u8 - 1,
        },
    ),
];

fn slot(face: u8, kind: u8) -> usize {
    face as usize * PER_FACE + kind as usize
}

pub fn is_pinned_move(mv: MegaMove) -> bool {
    PINNED.iter().any(|(_, pinned)| *pinned == mv)
}

fn describe_move(mv: MegaMove) -> String {
    match mv {
        MegaMove::Turn { face, steps } => format!("turn {} {}", face, steps),
        MegaMove::Spin { steps } => format!("spin {}", steps),
    }
}

fn ring_pos(g: u8, h: u8) -> usize {
    let ring = &NEIGHBOR_CW[g as usize];
    for (k, &n) in ring.iter().enumerate() {
        if n == h {
            return k;
        }
    }
    unreachable!("ring lookup")
}

fn corner_pos(g: u8, a: u8, b: u8) -> usize {
    let ring = &NEIGHBOR_CW[g as usize];
    for k in 0..RING {
        let cur = ring[k];
        let next = ring[(k + 1) % RING];
        if (cur == a && next == b) || (cur == b && next == a) {
            return k;
        }
    }
    unreachable!("corner lookup")
}

fn turn_cw(colors: &mut [u8; FACELETS], face: u8) {
    let src = *colors;
    let ring = NEIGHBOR_CW[face as usize];
    for k in 0..RING {
        let a = ring[k];
        let b = ring[(k + 1) % RING];
        let c = ring[(k + 2) % RING];
        let next = (k + 1) % RING;
        colors[slot(face, corner_slot(next))] = src[slot(face, corner_slot(k))];
        colors[slot(face, edge_slot(next))] = src[slot(face, edge_slot(k))];
        colors[slot(b, corner_slot(corner_pos(b, face, c)))] =
            src[slot(a, corner_slot(corner_pos(a, face, b)))];
        colors[slot(c, corner_slot(corner_pos(c, face, b)))] =
            src[slot(b, corner_slot(corner_pos(b, face, a)))];
        colors[slot(b, edge_slot((ring_pos(b, face) + RING - 1) % RING))] =
            src[slot(a, edge_slot((ring_pos(a, face) + RING - 1) % RING))];
    }
}

fn spin_cw(colors: &mut [u8; FACELETS]) {
    let src = *colors;
    for (g, &img) in Y_PERM.iter().enumerate() {
        let face = g as u8;
        colors[slot(img, CENTER)] = src[slot(face, CENTER)];
        for k in 0..RING {
            let h = NEIGHBOR_CW[g][k];
            let h2 = NEIGHBOR_CW[g][(k + 1) % RING];
            colors[slot(img, edge_slot(ring_pos(img, Y_PERM[h as usize])))] =
                src[slot(face, edge_slot(k))];
            colors[slot(
                img,
                corner_slot(corner_pos(img, Y_PERM[h as usize], Y_PERM[h2 as usize])),
            )] = src[slot(face, corner_slot(k))];
        }
    }
}

impl Geometry for Mega {
    type Move = MegaMove;
    type Alg = MegaAlg;
    type State = MegaState;

    fn parse_move(&self, token: &str) -> Result<MegaMove, ParseError> {
        for (text, mv) in PINNED {
            if text == token {
                return Ok(mv);
            }
        }
        Err(ParseError::UnknownToken(token.to_string()))
    }

    fn parse_alg(&self, text: &str) -> Result<MegaAlg, ParseError> {
        let mut moves = Vec::new();
        for token in text.split_whitespace() {
            moves.push(self.parse_move(token)?);
        }
        Ok(MegaAlg { moves })
    }

    fn format_alg(&self, alg: &MegaAlg) -> String {
        alg.moves
            .iter()
            .map(|mv| {
                PINNED
                    .iter()
                    .find(|(_, pinned)| *pinned == *mv)
                    .map(|(text, _)| *text)
                    .expect("pinned move")
            })
            .collect::<Vec<&str>>()
            .join(" ")
    }

    fn invert_alg(&self, alg: &MegaAlg) -> MegaAlg {
        MegaAlg {
            moves: alg
                .moves
                .iter()
                .rev()
                .map(|mv| match *mv {
                    MegaMove::Turn { face, steps } => MegaMove::Turn {
                        face,
                        steps: RING as u8 - steps,
                    },
                    MegaMove::Spin { steps } => MegaMove::Spin {
                        steps: RING as u8 - steps,
                    },
                })
                .collect(),
        }
    }

    fn apply_alg(&self, state: &mut MegaState, alg: &MegaAlg) -> Result<(), ApplyError> {
        for mv in &alg.moves {
            if !is_pinned_move(*mv) {
                return Err(ApplyError::IllegalMove(describe_move(*mv)));
            }
            match *mv {
                MegaMove::Turn { face, steps } => {
                    for _ in 0..steps {
                        turn_cw(&mut state.colors, face);
                    }
                }
                MegaMove::Spin { steps } => {
                    for _ in 0..steps {
                        spin_cw(&mut state.colors);
                    }
                }
            }
        }
        Ok(())
    }

    fn solved_state(&self) -> MegaState {
        let mut colors = [0u8; FACELETS];
        for (i, cell) in colors.iter_mut().enumerate() {
            *cell = (i / PER_FACE) as u8;
        }
        MegaState { colors }
    }

    fn is_solved(&self, state: &MegaState) -> bool {
        (0..FACELETS).all(|i| state.colors[i] as usize == i / PER_FACE)
    }

    fn facelets(&self, state: &MegaState) -> FaceletGrid {
        let view = |face: u8| -> FaceletFace {
            FaceletFace {
                rows: vec![
                    vec![state.colors[slot(face, corner_slot(0))]],
                    vec![
                        state.colors[slot(face, edge_slot(RING - 1))],
                        state.colors[slot(face, edge_slot(0))],
                    ],
                    vec![
                        state.colors[slot(face, corner_slot(RING - 1))],
                        state.colors[slot(face, CENTER)],
                        state.colors[slot(face, corner_slot(1))],
                    ],
                    vec![
                        state.colors[slot(face, edge_slot(RING - 2))],
                        state.colors[slot(face, edge_slot(2))],
                        state.colors[slot(face, edge_slot(1))],
                    ],
                    vec![
                        state.colors[slot(face, corner_slot(RING - 2))],
                        state.colors[slot(face, corner_slot(2))],
                    ],
                ],
            }
        };
        FaceletGrid {
            faces: (0..FACES).map(|f| view(f as u8)).collect(),
        }
    }
}
