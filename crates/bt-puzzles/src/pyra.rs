#![allow(dead_code)]

use crate::geometry::{ApplyError, FaceletFace, FaceletGrid, Geometry, ParseError};

pub const FACELET_COUNT: usize = 36;
const FACE_COUNT: usize = 4;
const STICKERS_PER_FACE: usize = 9;

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum Vertex {
    U,
    L,
    R,
    B,
}

impl Vertex {
    fn index(self) -> usize {
        match self {
            Vertex::U => 0,
            Vertex::L => 1,
            Vertex::R => 2,
            Vertex::B => 3,
        }
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum Layers {
    Twin,
    Tip,
}

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum Sense {
    Clock,
    Counter,
}

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub struct PyraMove {
    pub vertex: Vertex,
    pub layers: Layers,
    pub sense: Sense,
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct PyraAlg {
    pub moves: Vec<PyraMove>,
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct PyraState {
    facelets: [u8; FACELET_COUNT],
}

#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub struct Pyraminx;

const CYCLES: [[[usize; 3]; 4]; 4] = [
    [[0, 9, 18], [1, 10, 19], [2, 11, 20], [3, 12, 21]],
    [[4, 35, 17], [5, 34, 16], [1, 33, 15], [6, 30, 12]],
    [[8, 31, 22], [7, 32, 23], [3, 33, 24], [6, 28, 19]],
    [[27, 26, 13], [29, 25, 14], [15, 28, 21], [10, 30, 24]],
];

fn spin(facelets: &mut [u8; FACELET_COUNT], cycle: &[usize; 3], sense: Sense) {
    let [a, b, c] = *cycle;
    let carried = facelets[a];
    match sense {
        Sense::Clock => {
            facelets[a] = facelets[c];
            facelets[c] = facelets[b];
            facelets[b] = carried;
        }
        Sense::Counter => {
            facelets[a] = facelets[b];
            facelets[b] = facelets[c];
            facelets[c] = carried;
        }
    }
}

fn token_text(mv: PyraMove) -> String {
    let base = match (mv.vertex, mv.layers) {
        (Vertex::U, Layers::Twin) => 'U',
        (Vertex::U, Layers::Tip) => 'u',
        (Vertex::L, Layers::Twin) => 'L',
        (Vertex::L, Layers::Tip) => 'l',
        (Vertex::R, Layers::Twin) => 'R',
        (Vertex::R, Layers::Tip) => 'r',
        (Vertex::B, Layers::Twin) => 'B',
        (Vertex::B, Layers::Tip) => 'b',
    };
    match mv.sense {
        Sense::Clock => base.to_string(),
        Sense::Counter => format!("{base}'"),
    }
}

fn flip(mv: PyraMove) -> PyraMove {
    let sense = match mv.sense {
        Sense::Clock => Sense::Counter,
        Sense::Counter => Sense::Clock,
    };
    PyraMove { sense, ..mv }
}

impl Geometry for Pyraminx {
    type Move = PyraMove;
    type Alg = PyraAlg;
    type State = PyraState;

    fn parse_move(&self, token: &str) -> Result<PyraMove, ParseError> {
        let mut chars = token.chars();
        let Some(base) = chars.next() else {
            return Err(ParseError::UnknownToken(token.to_string()));
        };
        let (vertex, layers) = match base {
            'U' => (Vertex::U, Layers::Twin),
            'u' => (Vertex::U, Layers::Tip),
            'L' => (Vertex::L, Layers::Twin),
            'l' => (Vertex::L, Layers::Tip),
            'R' => (Vertex::R, Layers::Twin),
            'r' => (Vertex::R, Layers::Tip),
            'B' => (Vertex::B, Layers::Twin),
            'b' => (Vertex::B, Layers::Tip),
            _ => return Err(ParseError::UnknownToken(token.to_string())),
        };
        let sense = match chars.next() {
            None => Sense::Clock,
            Some('\'') => Sense::Counter,
            Some(_) => return Err(ParseError::UnknownToken(token.to_string())),
        };
        if chars.next().is_some() {
            return Err(ParseError::UnknownToken(token.to_string()));
        }
        Ok(PyraMove {
            vertex,
            layers,
            sense,
        })
    }

    fn parse_alg(&self, text: &str) -> Result<PyraAlg, ParseError> {
        let mut moves = Vec::new();
        for token in text.split_whitespace() {
            moves.push(self.parse_move(token)?);
        }
        Ok(PyraAlg { moves })
    }

    fn format_alg(&self, alg: &PyraAlg) -> String {
        alg.moves
            .iter()
            .map(|mv| token_text(*mv))
            .collect::<Vec<String>>()
            .join(" ")
    }

    fn invert_alg(&self, alg: &PyraAlg) -> PyraAlg {
        PyraAlg {
            moves: alg.moves.iter().rev().map(|mv| flip(*mv)).collect(),
        }
    }

    fn apply_alg(&self, state: &mut PyraState, alg: &PyraAlg) -> Result<(), ApplyError> {
        for mv in &alg.moves {
            let table = &CYCLES[mv.vertex.index()];
            let depth = match mv.layers {
                Layers::Twin => table.len(),
                Layers::Tip => 1,
            };
            for cycle in &table[..depth] {
                spin(&mut state.facelets, cycle, mv.sense);
            }
        }
        Ok(())
    }

    fn solved_state(&self) -> PyraState {
        let mut facelets = [0; FACELET_COUNT];
        for (i, slot) in facelets.iter_mut().enumerate() {
            *slot = (i / STICKERS_PER_FACE) as u8;
        }
        PyraState { facelets }
    }

    fn is_solved(&self, state: &PyraState) -> bool {
        *state == self.solved_state()
    }

    fn facelets(&self, state: &PyraState) -> FaceletGrid {
        FaceletGrid {
            faces: (0..FACE_COUNT)
                .map(|face| {
                    let base = face * STICKERS_PER_FACE;
                    FaceletFace {
                        rows: vec![
                            vec![state.facelets[base]],
                            vec![
                                state.facelets[base + 1],
                                state.facelets[base + 2],
                                state.facelets[base + 3],
                            ],
                            vec![
                                state.facelets[base + 4],
                                state.facelets[base + 5],
                                state.facelets[base + 6],
                                state.facelets[base + 7],
                                state.facelets[base + 8],
                            ],
                        ],
                    }
                })
                .collect(),
        }
    }
}
