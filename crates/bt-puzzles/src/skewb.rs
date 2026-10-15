use crate::geometry::{ApplyError, FaceletFace, FaceletGrid, Geometry, ParseError};

pub const FACELET_COUNT: usize = 54;
pub const FACE_NAMES: [&str; 6] = ["U", "F", "R", "L", "B", "D"];
pub const VERTEX_NAMES: [&str; 4] = ["F", "L", "R", "B"];

const VERTEX_SIGNS: [[i8; 3]; 4] = [[1, 1, 1], [-1, 1, 1], [1, 1, -1], [1, -1, 1]];

const FACE_NORMALS: [[i8; 3]; 6] = [
    [0, 1, 0],
    [0, 0, 1],
    [1, 0, 0],
    [-1, 0, 0],
    [0, 0, -1],
    [0, -1, 0],
];
const FACE_UPS: [[i8; 3]; 6] = [
    [0, 0, -1],
    [0, 1, 0],
    [0, 1, 0],
    [0, 1, 0],
    [0, 1, 0],
    [0, 0, 1],
];
const FACE_RIGHTS: [[i8; 3]; 6] = [
    [1, 0, 0],
    [1, 0, 0],
    [0, 0, -1],
    [0, 0, 1],
    [-1, 0, 0],
    [1, 0, 0],
];

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct Move {
    pub vertex: u8,
    pub quarter: u8,
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Alg {
    pub moves: Vec<Move>,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct State {
    pub facelets: [u8; FACELET_COUNT],
}

pub struct Skewb {
    turns: [[u8; FACELET_COUNT]; 8],
}

fn rotate(signs: [i8; 3], quarter: u8, point: [i8; 3]) -> [i8; 3] {
    let framed = [
        signs[0] * point[0],
        signs[1] * point[1],
        signs[2] * point[2],
    ];
    let cycled = if quarter == 1 {
        [framed[1], framed[2], framed[0]]
    } else {
        [framed[2], framed[0], framed[1]]
    };
    [
        signs[0] * cycled[0],
        signs[1] * cycled[1],
        signs[2] * cycled[2],
    ]
}

fn dot(a: [i8; 3], b: [i8; 3]) -> i32 {
    a[0] as i32 * b[0] as i32 + a[1] as i32 * b[1] as i32 + a[2] as i32 * b[2] as i32
}

fn cross(a: [i8; 3], b: [i8; 3]) -> [i8; 3] {
    [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]
}

fn anchors() -> [[i8; 3]; FACELET_COUNT] {
    let mut table = [[0i8; 3]; FACELET_COUNT];
    for face in 0..6 {
        let normal = FACE_NORMALS[face];
        let up = FACE_UPS[face];
        let right = FACE_RIGHTS[face];
        for row in 0..3usize {
            for col in 0..3usize {
                let rise = (1 - row as i8) * 2;
                let run = (col as i8 - 1) * 2;
                let slot = face * 9 + row * 3 + col;
                table[slot] = [
                    normal[0] * 3 + up[0] * rise + right[0] * run,
                    normal[1] * 3 + up[1] * rise + right[1] * run,
                    normal[2] * 3 + up[2] * rise + right[2] * run,
                ];
            }
        }
    }
    table
}

fn slot_of(anchors: &[[i8; 3]; FACELET_COUNT], point: [i8; 3]) -> usize {
    (0..FACELET_COUNT)
        .find(|&slot| {
            anchors[slot][0] == point[0]
                && anchors[slot][1] == point[1]
                && anchors[slot][2] == point[2]
        })
        .unwrap()
}

fn parse_move(token: &str) -> Result<Move, ParseError> {
    for (vertex, name) in VERTEX_NAMES.iter().enumerate() {
        if let Some(rest) = token.strip_prefix(name) {
            let quarter = match rest {
                "" => 1,
                "'" => 2,
                _ => continue,
            };
            return Ok(Move {
                vertex: vertex as u8,
                quarter,
            });
        }
    }
    Err(ParseError::UnknownToken(token.to_string()))
}

fn format_move(mv: Move) -> String {
    let suffix = if mv.quarter == 2 { "'" } else { "" };
    format!("{}{suffix}", VERTEX_NAMES[mv.vertex as usize])
}

impl Skewb {
    pub fn new() -> Skewb {
        let anchors = anchors();
        let mut turns = [[0u8; FACELET_COUNT]; 8];
        for vertex in 0..4usize {
            let signs = VERTEX_SIGNS[vertex];
            let probe = [0, 1, 0];
            let clockwise = if dot(cross(probe, rotate(signs, 1, probe)), signs) < 0 {
                1
            } else {
                2
            };
            let mut table = core::array::from_fn(|i| i as u8);
            for slot in 0..FACELET_COUNT {
                if dot(anchors[slot], signs) > 0 {
                    let image = slot_of(&anchors, rotate(signs, clockwise, anchors[slot]));
                    table[slot] = image as u8;
                }
            }
            turns[vertex * 2] = table;
            turns[vertex * 2 + 1] = core::array::from_fn(|i| table[table[i] as usize]);
        }
        Skewb { turns }
    }

    pub fn turn_table(&self, vertex: u8, quarter: u8) -> [u8; FACELET_COUNT] {
        self.turns[vertex as usize * 2 + quarter as usize - 1]
    }

    fn table_for(&self, mv: Move) -> Result<&[u8; FACELET_COUNT], ApplyError> {
        let vertex = mv.vertex as usize;
        let quarter = mv.quarter as usize;
        if vertex >= 4 || quarter == 0 || quarter > 2 {
            return Err(ApplyError::IllegalMove(format_move(mv)));
        }
        Ok(&self.turns[vertex * 2 + quarter - 1])
    }
}

impl Default for Skewb {
    fn default() -> Self {
        Self::new()
    }
}

impl Geometry for Skewb {
    type Move = Move;
    type Alg = Alg;
    type State = State;

    fn parse_move(&self, token: &str) -> Result<Move, ParseError> {
        parse_move(token)
    }

    fn parse_alg(&self, text: &str) -> Result<Alg, ParseError> {
        let mut moves = Vec::new();
        for token in text.split_whitespace() {
            moves.push(parse_move(token)?);
        }
        Ok(Alg { moves })
    }

    fn format_alg(&self, alg: &Alg) -> String {
        alg.moves
            .iter()
            .map(|mv| format_move(*mv))
            .collect::<Vec<String>>()
            .join(" ")
    }

    fn invert_alg(&self, alg: &Alg) -> Alg {
        Alg {
            moves: alg
                .moves
                .iter()
                .rev()
                .map(|mv| Move {
                    vertex: mv.vertex,
                    quarter: 3 - mv.quarter,
                })
                .collect(),
        }
    }

    fn apply_alg(&self, state: &mut State, alg: &Alg) -> Result<(), ApplyError> {
        for mv in &alg.moves {
            let table = self.table_for(*mv)?;
            let before = state.facelets;
            for (slot, to) in table.iter().enumerate() {
                state.facelets[*to as usize] = before[slot];
            }
        }
        Ok(())
    }

    fn solved_state(&self) -> State {
        State {
            facelets: core::array::from_fn(|i| (i / 9) as u8),
        }
    }

    fn is_solved(&self, state: &State) -> bool {
        state
            .facelets
            .iter()
            .enumerate()
            .all(|(i, &facelet)| facelet as usize == i / 9)
    }

    fn facelets(&self, state: &State) -> FaceletGrid {
        FaceletGrid {
            faces: (0..6)
                .map(|face| {
                    let at = |slot: usize| state.facelets[face * 9 + slot];
                    FaceletFace {
                        rows: vec![
                            vec![at(0), at(1), at(2)],
                            vec![at(3), at(4), at(5)],
                            vec![at(6), at(7), at(8)],
                        ],
                    }
                })
                .collect(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::verify_tier1;

    #[test]
    fn solved_state_is_solved() {
        let skewb = Skewb::new();
        assert!(skewb.is_solved(&skewb.solved_state()));
    }

    #[test]
    fn every_single_turn_breaks_and_reverts() {
        let skewb = Skewb::new();
        for vertex in 0..4u8 {
            for quarter in [1u8, 2u8] {
                let alg = Alg {
                    moves: vec![Move { vertex, quarter }],
                };
                let mut state = skewb.solved_state();
                skewb.apply_alg(&mut state, &alg).unwrap();
                assert!(!skewb.is_solved(&state));
                skewb
                    .apply_alg(&mut state, &skewb.invert_alg(&alg))
                    .unwrap();
                assert!(skewb.is_solved(&state));
            }
        }
    }

    #[test]
    fn move_tokens_round_trip_through_format() {
        let skewb = Skewb::new();
        for vertex in 0..4u8 {
            for quarter in [1u8, 2u8] {
                let text = format_move(Move { vertex, quarter });
                assert_eq!(
                    skewb.parse_move(&text),
                    Ok(Move { vertex, quarter }),
                    "{text}"
                );
            }
        }
    }

    #[test]
    fn inverse_round_trips_a_deterministic_alg_sweep() {
        let skewb = Skewb::new();
        let mut seed = 0x5eed_1234_abcd_f00du64;
        let mut next = move || {
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            seed
        };
        for _ in 0..4096 {
            let length = (next() % 24) as usize;
            let moves = (0..length)
                .map(|_| Move {
                    vertex: (next() % 4) as u8,
                    quarter: (next() % 2) as u8 + 1,
                })
                .collect();
            let alg = Alg { moves };
            let inverse = skewb.invert_alg(&alg);
            let mut forward = skewb.solved_state();
            skewb.apply_alg(&mut forward, &alg).unwrap();
            skewb.apply_alg(&mut forward, &inverse).unwrap();
            assert!(skewb.is_solved(&forward));
            let mut reverse = skewb.solved_state();
            skewb.apply_alg(&mut reverse, &inverse).unwrap();
            skewb.apply_alg(&mut reverse, &alg).unwrap();
            assert!(skewb.is_solved(&reverse));
            let text = skewb.format_alg(&alg);
            assert_eq!(skewb.parse_alg(&text).unwrap(), alg);
            assert_eq!(skewb.invert_alg(&inverse), alg);
            assert_eq!(verify_tier1(&skewb, &text), Ok(()));
        }
    }
}
