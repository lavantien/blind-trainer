use crate::geometry::{ApplyError, FaceletFace, FaceletGrid, Geometry, ParseError};

pub const FACELET_COUNT: usize = 72;
pub const FACE_NAMES: [&str; 8] = ["U", "F", "R", "BR", "B", "BL", "L", "D"];

const FACE_SIGNS: [[i8; 3]; 8] = [
    [1, 1, 1],
    [1, 1, -1],
    [-1, 1, -1],
    [-1, 1, 1],
    [-1, -1, 1],
    [1, -1, 1],
    [1, -1, -1],
    [-1, -1, -1],
];
const EDGE_LOCALS: [(usize, usize); 3] = [(0, 1), (0, 2), (1, 2)];

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct Move {
    pub face: u8,
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

pub struct Fto {
    turns: [[u8; FACELET_COUNT]; 16],
}

fn vertex(face: usize, local: usize) -> [i8; 3] {
    let mut point = [0; 3];
    point[local] = FACE_SIGNS[face][local];
    point
}

fn vertex_id(point: [i8; 3]) -> usize {
    let axis = point.iter().position(|&c| c != 0).unwrap();
    axis * 2 + usize::from(point[axis] < 0)
}

fn face_mask(face: usize) -> u8 {
    (0..3).fold(0u8, |mask, local| {
        mask | 1 << vertex_id(vertex(face, local))
    })
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

fn corner_local(face: usize, corner: usize) -> usize {
    (0..3)
        .find(|&local| vertex_id(vertex(face, local)) == corner)
        .unwrap()
}

fn edge_local(face: usize, edge: (usize, usize)) -> usize {
    EDGE_LOCALS
        .iter()
        .position(|&(a, b)| {
            let want = (vertex_id(vertex(face, a)), vertex_id(vertex(face, b)));
            want == edge || (want.1, want.0) == edge
        })
        .unwrap()
}

fn shared_vertices(a: u8, b: u8) -> usize {
    (0..3)
        .filter(|&axis| FACE_SIGNS[a as usize][axis] == FACE_SIGNS[b as usize][axis])
        .count()
}

fn parse_move(token: &str) -> Result<Move, ParseError> {
    for (face, name) in FACE_NAMES.iter().enumerate() {
        if let Some(rest) = token.strip_prefix(name) {
            let quarter = match rest {
                "" | "2'" => 1,
                "'" | "2" => 2,
                _ => continue,
            };
            return Ok(Move {
                face: face as u8,
                quarter,
            });
        }
    }
    Err(ParseError::UnknownToken(token.to_string()))
}

fn format_move(mv: Move) -> String {
    let suffix = if mv.quarter == 2 { "'" } else { "" };
    format!("{}{suffix}", FACE_NAMES[mv.face as usize])
}

impl Fto {
    pub fn new() -> Fto {
        let mut turns = [[0u8; FACELET_COUNT]; 16];
        for face in 0..8usize {
            let signs = FACE_SIGNS[face];
            let anchor = vertex(face, 0);
            let quarter = if dot(cross(anchor, rotate(signs, 1, anchor)), signs) < 0 {
                1
            } else {
                2
            };
            let corner_map: [usize; 3] = core::array::from_fn(|local| {
                let image = rotate(signs, quarter, vertex(face, local));
                corner_local(face, vertex_id(image))
            });
            let face_map: [usize; 8] = core::array::from_fn(|other| {
                let mask = (0..3).fold(0u8, |mask, local| {
                    mask | 1 << vertex_id(rotate(signs, quarter, vertex(other, local)))
                });
                (0..8)
                    .find(|&candidate| face_mask(candidate) == mask)
                    .unwrap()
            });
            let mut table: [u8; FACELET_COUNT] = core::array::from_fn(|i| i as u8);
            for (local, &landing) in corner_map.iter().enumerate() {
                table[face * 9 + landing] = (face * 9 + local) as u8;
                table[face * 9 + 6 + landing] = (face * 9 + 6 + local) as u8;
            }
            for (offset, &(a, b)) in EDGE_LOCALS.iter().enumerate() {
                let image = (
                    vertex_id(rotate(signs, quarter, vertex(face, a))),
                    vertex_id(rotate(signs, quarter, vertex(face, b))),
                );
                let landing = edge_local(face, image);
                table[face * 9 + 3 + landing] = (face * 9 + 3 + offset) as u8;
            }
            for local in 0..3 {
                let home = vertex_id(vertex(face, local));
                let away = vertex_id(rotate(signs, quarter, vertex(face, local)));
                for (other, &target) in face_map.iter().enumerate() {
                    if other == face || face_mask(other) & (1 << home) == 0 {
                        continue;
                    }
                    let from = other * 9 + corner_local(other, home);
                    let to = target * 9 + corner_local(target, away);
                    table[to] = from as u8;
                }
            }
            for &(a, b) in EDGE_LOCALS.iter() {
                let home = (vertex_id(vertex(face, a)), vertex_id(vertex(face, b)));
                let away = (
                    vertex_id(rotate(signs, quarter, vertex(face, a))),
                    vertex_id(rotate(signs, quarter, vertex(face, b))),
                );
                let other = (0..8)
                    .find(|&candidate| {
                        candidate != face
                            && face_mask(candidate) & (1 << home.0) != 0
                            && face_mask(candidate) & (1 << home.1) != 0
                    })
                    .unwrap();
                let target = face_map[other];
                let from = other * 9 + 3 + edge_local(other, home);
                let to = target * 9 + 3 + edge_local(target, away);
                table[to] = from as u8;
            }
            turns[face * 2] = table;
            turns[face * 2 + 1] = core::array::from_fn(|i| table[table[i] as usize]);
        }
        Fto { turns }
    }

    pub fn turn_table(&self, face: u8, quarter: u8) -> [u8; FACELET_COUNT] {
        self.turns[face as usize * 2 + quarter as usize - 1]
    }

    pub fn neighbors(&self, face: u8) -> [u8; 3] {
        let mut found = [0u8; 3];
        let mut count = 0;
        for other in 0..8u8 {
            if other != face && shared_vertices(face, other) == 2 {
                found[count] = other;
                count += 1;
            }
        }
        found
    }

    pub fn opposite(&self, face: u8) -> u8 {
        (0..8u8)
            .find(|&other| shared_vertices(face, other) == 0)
            .unwrap()
    }

    fn table_for(&self, mv: Move) -> Result<&[u8; FACELET_COUNT], ApplyError> {
        let face = mv.face as usize;
        let quarter = mv.quarter as usize;
        if face >= 8 || quarter == 0 || quarter > 2 {
            return Err(ApplyError::IllegalMove(format_move(mv)));
        }
        Ok(&self.turns[face * 2 + quarter - 1])
    }
}

impl Default for Fto {
    fn default() -> Self {
        Self::new()
    }
}

impl Geometry for Fto {
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
                    face: mv.face,
                    quarter: 3 - mv.quarter,
                })
                .collect(),
        }
    }

    fn apply_alg(&self, state: &mut State, alg: &Alg) -> Result<(), ApplyError> {
        for mv in &alg.moves {
            let table = self.table_for(*mv)?;
            let before = state.facelets;
            for (i, from) in table.iter().enumerate() {
                state.facelets[i] = before[*from as usize];
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
            faces: (0..8)
                .map(|face| {
                    let at = |slot: usize| state.facelets[face * 9 + slot];
                    FaceletFace {
                        rows: vec![
                            vec![at(0)],
                            vec![at(3), at(6), at(4)],
                            vec![at(1), at(7), at(5), at(8), at(2)],
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
        let fto = Fto::new();
        assert!(fto.is_solved(&fto.solved_state()));
    }

    #[test]
    fn every_single_turn_breaks_and_reverts() {
        let fto = Fto::new();
        for face in 0..8u8 {
            for quarter in [1u8, 2u8] {
                let alg = Alg {
                    moves: vec![Move { face, quarter }],
                };
                let mut state = fto.solved_state();
                fto.apply_alg(&mut state, &alg).unwrap();
                assert!(!fto.is_solved(&state));
                fto.apply_alg(&mut state, &fto.invert_alg(&alg)).unwrap();
                assert!(fto.is_solved(&state));
            }
        }
    }

    #[test]
    fn move_tokens_round_trip_through_format() {
        let fto = Fto::new();
        for face in 0..8u8 {
            for quarter in [1u8, 2u8] {
                let text = format_move(Move { face, quarter });
                assert_eq!(fto.parse_move(&text), Ok(Move { face, quarter }), "{text}");
            }
        }
    }

    #[test]
    fn adjacency_closes_over_the_octahedron() {
        let fto = Fto::new();
        let expected: [[u8; 3]; 8] = [
            [1, 3, 5],
            [0, 2, 6],
            [1, 3, 7],
            [0, 2, 4],
            [3, 5, 7],
            [0, 4, 6],
            [1, 5, 7],
            [2, 4, 6],
        ];
        for face in 0..8u8 {
            assert_eq!(fto.neighbors(face), expected[face as usize]);
            for neighbor in fto.neighbors(face) {
                assert!(fto.neighbors(neighbor).contains(&face));
                assert_ne!(fto.opposite(face), neighbor);
            }
        }
        for (face, far) in [(0u8, 7u8), (1, 4), (2, 5), (3, 6)] {
            assert_eq!(fto.opposite(face), far);
            assert_eq!(fto.opposite(far), face);
        }
    }

    #[test]
    fn every_turn_is_a_bijective_order_three_three_cycle_permutation() {
        let fto = Fto::new();
        for face in 0..8u8 {
            for quarter in [1u8, 2u8] {
                let table = fto.turn_table(face, quarter);
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
                        other => panic!("face {face} quarter {quarter} cycle len {other}"),
                    }
                }
                assert_eq!((fixed, threes), (51, 7));
            }
        }
    }

    #[test]
    fn turn_recolors_only_neighbor_and_opposite_neighbor_faces() {
        let fto = Fto::new();
        for face in 0..8u8 {
            let far = fto.opposite(face);
            let mut landing = fto.neighbors(face).to_vec();
            landing.extend(fto.neighbors(far));
            for quarter in [1u8, 2u8] {
                let mut state = fto.solved_state();
                fto.apply_alg(
                    &mut state,
                    &Alg {
                        moves: vec![Move { face, quarter }],
                    },
                )
                .unwrap();
                let mut changed = 0;
                for i in 0..FACELET_COUNT {
                    let home = (i / 9) as u8;
                    if state.facelets[i] != home {
                        changed += 1;
                        assert!(
                            landing.contains(&home),
                            "face {face} quarter {quarter} moved a sticker onto face {home}"
                        );
                    }
                }
                for slot in 0..9 {
                    assert_eq!(state.facelets[face as usize * 9 + slot], face);
                    assert_eq!(state.facelets[far as usize * 9 + slot], far);
                }
                assert_eq!(changed, 12);
            }
        }
    }

    #[test]
    fn opposite_face_turns_commute() {
        let fto = Fto::new();
        for (a, b) in [(0u8, 7u8), (1, 4), (2, 5), (3, 6)] {
            let a_name = FACE_NAMES[a as usize];
            let b_name = FACE_NAMES[b as usize];
            let mut ab = fto.solved_state();
            fto.apply_alg(&mut ab, &fto.parse_alg(a_name).unwrap())
                .unwrap();
            fto.apply_alg(&mut ab, &fto.parse_alg(b_name).unwrap())
                .unwrap();
            let mut ba = fto.solved_state();
            fto.apply_alg(&mut ba, &fto.parse_alg(b_name).unwrap())
                .unwrap();
            fto.apply_alg(&mut ba, &fto.parse_alg(a_name).unwrap())
                .unwrap();
            assert_eq!(ab, ba);
        }
    }

    #[test]
    fn double_turn_token_equals_two_quarter_turns() {
        let fto = Fto::new();
        for face in FACE_NAMES {
            let mut doubled = fto.solved_state();
            fto.apply_alg(&mut doubled, &fto.parse_alg(face).unwrap())
                .unwrap();
            fto.apply_alg(&mut doubled, &fto.parse_alg(face).unwrap())
                .unwrap();
            let mut tokened = fto.solved_state();
            fto.apply_alg(&mut tokened, &fto.parse_alg(&format!("{face}2")).unwrap())
                .unwrap();
            assert_eq!(doubled, tokened, "{face}");
            let mut tripled = fto.solved_state();
            for _ in 0..3 {
                fto.apply_alg(&mut tripled, &fto.parse_alg(face).unwrap())
                    .unwrap();
            }
            assert!(fto.is_solved(&tripled), "{face}");
        }
    }

    #[test]
    fn grammar_accepts_all_faces_and_pinned_repeat_forms() {
        let fto = Fto::new();
        for face in FACE_NAMES {
            for suffix in ["", "'", "2", "2'"] {
                assert!(fto.parse_move(&format!("{face}{suffix}")).is_ok());
            }
        }
        assert_eq!(fto.format_alg(&fto.parse_alg("L2").unwrap()), "L'");
        assert_eq!(fto.format_alg(&fto.parse_alg("L2'").unwrap()), "L");
        assert_eq!(fto.format_alg(&fto.parse_alg("R2'").unwrap()), "R");
        let alg = fto.parse_alg("U BR U BR' F U F'").unwrap();
        assert_eq!(fto.parse_alg(&fto.format_alg(&alg)).unwrap(), alg);
    }

    #[test]
    fn rejects_garbage_and_cube_slice_tokens() {
        let fto = Fto::new();
        for token in [
            "M", "E", "S", "x", "y", "z", "u", "r", "Uw", "BRw", "3U", "U3", "U4", "UU", "'", "2",
            "U2''", "F2'2", "",
        ] {
            assert_eq!(
                fto.parse_move(token),
                Err(ParseError::UnknownToken(token.to_string())),
                "{token}"
            );
        }
        assert!(matches!(
            fto.parse_alg("U M").unwrap_err(),
            ParseError::UnknownToken(token) if token == "M"
        ));
    }

    #[test]
    fn solved_grid_renders_eight_triforce_faces() {
        let fto = Fto::new();
        let grid = fto.facelets(&fto.solved_state());
        assert_eq!(grid.faces.len(), 8);
        for (face, grid_face) in grid.faces.iter().enumerate() {
            assert_eq!(
                grid_face
                    .rows
                    .iter()
                    .map(|row| row.len())
                    .collect::<Vec<_>>(),
                vec![1, 3, 5]
            );
            for row in &grid_face.rows {
                assert!(row.iter().all(|&value| value as usize == face));
            }
        }
    }

    #[test]
    fn inverse_round_trips_a_deterministic_alg_sweep() {
        let fto = Fto::new();
        let mut seed = 0x2545_f491_4f6c_dd1du64;
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
                    face: (next() % 8) as u8,
                    quarter: (next() % 2) as u8 + 1,
                })
                .collect();
            let alg = Alg { moves };
            let inverse = fto.invert_alg(&alg);
            let mut forward = fto.solved_state();
            fto.apply_alg(&mut forward, &alg).unwrap();
            fto.apply_alg(&mut forward, &inverse).unwrap();
            assert!(fto.is_solved(&forward));
            let mut reverse = fto.solved_state();
            fto.apply_alg(&mut reverse, &inverse).unwrap();
            fto.apply_alg(&mut reverse, &alg).unwrap();
            assert!(fto.is_solved(&reverse));
            let text = fto.format_alg(&alg);
            assert_eq!(fto.parse_alg(&text).unwrap(), alg);
            assert_eq!(fto.invert_alg(&inverse), alg);
            assert_eq!(verify_tier1(&fto, &text), Ok(()));
        }
    }
}
