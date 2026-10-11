use crate::cube::Face;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Slice {
    M,
    E,
    S,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Axis {
    X,
    Y,
    Z,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Move {
    Face { face: Face, quarter: u8 },
    Wide { face: Face, quarter: u8 },
    Slice { slice: Slice, quarter: u8 },
    Inner { face: Face, quarter: u8 },
    Rotation { axis: Axis, quarter: u8 },
}

const IDENT: [u8; 54] = {
    let mut ids = [0u8; 54];
    let mut i = 0;
    while i < 54 {
        ids[i] = i as u8;
        i += 1;
    }
    ids
};

const NORMAL: [[i32; 3]; 6] = [
    [0, 1, 0],
    [-1, 0, 0],
    [0, 0, 1],
    [1, 0, 0],
    [0, 0, -1],
    [0, -1, 0],
];
const AXIS: [usize; 6] = [1, 0, 2, 0, 2, 1];
const SIGN: [i32; 6] = [1, -1, 1, 1, -1, -1];

const fn grid(face: usize, r: usize, c: usize) -> [i32; 3] {
    let (r, c) = (r as i32, c as i32);
    match face {
        0 => [c - 1, 1, r - 1],
        1 => [-1, 1 - r, c - 1],
        2 => [c - 1, 1 - r, 1],
        3 => [1, 1 - r, 1 - c],
        4 => [1 - c, 1 - r, -1],
        _ => [c - 1, -1, 1 - r],
    }
}

const fn rot(v: [i32; 3], axis: usize, times: usize) -> [i32; 3] {
    let mut v = v;
    let mut i = 0;
    while i < times {
        let (x, y, z) = (v[0], v[1], v[2]);
        v = match axis {
            0 => [x, -z, y],
            1 => [z, y, -x],
            _ => [-y, x, z],
        };
        i += 1;
    }
    v
}

const fn home_cell(id: usize) -> (usize, usize, usize) {
    if id < 24 {
        let pos = match id % 4 {
            0 => (0, 0),
            1 => (0, 2),
            2 => (2, 2),
            _ => (2, 0),
        };
        (id / 4, pos.0, pos.1)
    } else if id < 48 {
        let e = id - 24;
        let pos = match e % 4 {
            0 => (0, 1),
            1 => (1, 2),
            2 => (2, 1),
            _ => (1, 0),
        };
        (e / 4, pos.0, pos.1)
    } else {
        (id - 48, 1, 1)
    }
}

const fn sticker_id(face: usize, r: usize, c: usize) -> usize {
    if r == 1 && c == 1 {
        48 + face
    } else if r.is_multiple_of(2) && c.is_multiple_of(2) {
        let pos = match (r, c) {
            (0, 0) => 0,
            (0, 2) => 1,
            (2, 2) => 2,
            (2, 0) => 3,
            _ => 0,
        };
        face * 4 + pos
    } else {
        let pos = match (r, c) {
            (0, 1) => 0,
            (1, 2) => 1,
            (2, 1) => 2,
            (1, 0) => 3,
            _ => 0,
        };
        24 + face * 4 + pos
    }
}

const fn face_of_normal(n: [i32; 3]) -> usize {
    let mut f = 0;
    while f < 6 {
        if NORMAL[f][0] == n[0] && NORMAL[f][1] == n[1] && NORMAL[f][2] == n[2] {
            return f;
        }
        f += 1;
    }
    6
}

const fn cell_of(face: usize, p: [i32; 3]) -> (usize, usize) {
    let mut r = 0;
    while r < 3 {
        let mut c = 0;
        while c < 3 {
            let g = grid(face, r, c);
            if g[0] == p[0] && g[1] == p[1] && g[2] == p[2] {
                return (r, c);
            }
            c += 1;
        }
        r += 1;
    }
    (9, 9)
}

const fn layer_perm(face: usize, depth: i32, quarter: usize, whole: bool) -> [u8; 54] {
    let n = NORMAL[face];
    let axis = AXIS[face];
    let applied = if SIGN[face] < 0 {
        quarter
    } else {
        (4 - quarter) % 4
    };
    let mut perm = [0u8; 54];
    let mut id = 0;
    while id < 54 {
        let (hf, r, c) = home_cell(id);
        let p = grid(hf, r, c);
        let d = p[0] * n[0] + p[1] * n[1] + p[2] * n[2];
        if whole || d == depth {
            let p2 = rot(p, axis, applied);
            let n2 = rot(NORMAL[hf], axis, applied);
            let fd = face_of_normal(n2);
            let cell = cell_of(fd, p2);
            perm[id] = sticker_id(fd, cell.0, cell.1) as u8;
        } else {
            perm[id] = id as u8;
        }
        id += 1;
    }
    perm
}

const fn compose(a: [u8; 54], b: [u8; 54]) -> [u8; 54] {
    let mut out = [0u8; 54];
    let mut i = 0;
    while i < 54 {
        out[i] = a[b[i] as usize];
        i += 1;
    }
    out
}

const fn power(base: [u8; 54], times: usize) -> [u8; 54] {
    let mut out = IDENT;
    let mut i = 0;
    while i < times {
        out = compose(base, out);
        i += 1;
    }
    out
}

const fn quarter_tables(base: [u8; 54]) -> [[u8; 54]; 3] {
    [power(base, 1), power(base, 2), power(base, 3)]
}

const fn layer_tables(depth: i32) -> [[[u8; 54]; 3]; 6] {
    [
        quarter_tables(layer_perm(0, depth, 1, false)),
        quarter_tables(layer_perm(1, depth, 1, false)),
        quarter_tables(layer_perm(2, depth, 1, false)),
        quarter_tables(layer_perm(3, depth, 1, false)),
        quarter_tables(layer_perm(4, depth, 1, false)),
        quarter_tables(layer_perm(5, depth, 1, false)),
    ]
}

static FACE_TABLES: [[[u8; 54]; 3]; 6] = layer_tables(1);
static INNER_TABLES: [[[u8; 54]; 3]; 6] = layer_tables(0);
static ROTATION_TABLES: [[[u8; 54]; 3]; 3] = [
    quarter_tables(layer_perm(3, 0, 1, true)),
    quarter_tables(layer_perm(0, 0, 1, true)),
    quarter_tables(layer_perm(2, 0, 1, true)),
];

fn slice_follows(slice: Slice) -> Face {
    match slice {
        Slice::M => Face::L,
        Slice::E => Face::D,
        Slice::S => Face::F,
    }
}

impl Move {
    pub fn table(&self) -> [u8; 54] {
        let q = self.quarter() as usize - 1;
        match *self {
            Move::Face { face, .. } => FACE_TABLES[face.index()][q],
            Move::Inner { face, .. } => INNER_TABLES[face.index()][q],
            Move::Wide { face, .. } => {
                compose(INNER_TABLES[face.index()][q], FACE_TABLES[face.index()][q])
            }
            Move::Slice { slice, .. } => INNER_TABLES[slice_follows(slice).index()][q],
            Move::Rotation { axis, .. } => ROTATION_TABLES[axis as usize][q],
        }
    }

    pub fn quarter(&self) -> u8 {
        match *self {
            Move::Face { quarter, .. }
            | Move::Wide { quarter, .. }
            | Move::Slice { quarter, .. }
            | Move::Inner { quarter, .. }
            | Move::Rotation { quarter, .. } => quarter,
        }
    }

    pub fn invert(&self) -> Move {
        let quarter = 4 - self.quarter();
        match *self {
            Move::Face { face, .. } => Move::Face { face, quarter },
            Move::Wide { face, .. } => Move::Wide { face, quarter },
            Move::Slice { slice, .. } => Move::Slice { slice, quarter },
            Move::Inner { face, .. } => Move::Inner { face, quarter },
            Move::Rotation { axis, .. } => Move::Rotation { axis, quarter },
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::cube::Cube;

    fn all_generators() -> Vec<Move> {
        let mut out = Vec::new();
        for face in Face::ALL {
            for q in 1..=3u8 {
                out.push(Move::Face { face, quarter: q });
                out.push(Move::Wide { face, quarter: q });
                out.push(Move::Inner { face, quarter: q });
            }
        }
        for slice in [Slice::M, Slice::E, Slice::S] {
            for q in 1..=3u8 {
                out.push(Move::Slice { slice, quarter: q });
            }
        }
        for axis in [Axis::X, Axis::Y, Axis::Z] {
            for q in 1..=3u8 {
                out.push(Move::Rotation { axis, quarter: q });
            }
        }
        out
    }

    #[test]
    fn all_generator_tables_are_bijections() {
        for mv in all_generators() {
            let table = mv.table();
            let mut seen = [false; 54];
            for &dst in &table {
                assert!(!seen[dst as usize], "{:?} hits {} twice", mv, dst);
                seen[dst as usize] = true;
            }
        }
    }

    #[test]
    fn face_turn_cycles_its_own_stickers_and_fixes_centers() {
        for face in Face::ALL {
            let table = Move::Face { face, quarter: 1 }.table();
            let (corner_base, edge_base) = (face.index() * 4, 24 + face.index() * 4);
            let mut corner_dsts = [false; 4];
            let mut edge_dsts = [false; 4];
            for k in 0..4usize {
                let c = table[corner_base + k] as usize;
                let e = table[edge_base + k] as usize;
                assert!(
                    c >= corner_base && c < corner_base + 4 && c != corner_base + k,
                    "{:?} corner {} -> {}",
                    face,
                    corner_base + k,
                    c
                );
                assert!(
                    e >= edge_base && e < edge_base + 4 && e != edge_base + k,
                    "{:?} edge {} -> {}",
                    face,
                    edge_base + k,
                    e
                );
                corner_dsts[c - corner_base] = true;
                edge_dsts[e - edge_base] = true;
            }
            assert!(corner_dsts.iter().all(|&s| s));
            assert!(edge_dsts.iter().all(|&s| s));
            for (center, &dst) in table.iter().enumerate().skip(48) {
                assert_eq!(dst as usize, center);
            }
        }
    }

    #[test]
    fn slices_equal_matching_inners() {
        for q in 1..=3u8 {
            assert_eq!(
                Move::Slice {
                    slice: Slice::M,
                    quarter: q
                }
                .table(),
                Move::Inner {
                    face: Face::L,
                    quarter: q
                }
                .table()
            );
            assert_eq!(
                Move::Slice {
                    slice: Slice::E,
                    quarter: q
                }
                .table(),
                Move::Inner {
                    face: Face::D,
                    quarter: q
                }
                .table()
            );
            assert_eq!(
                Move::Slice {
                    slice: Slice::S,
                    quarter: q
                }
                .table(),
                Move::Inner {
                    face: Face::F,
                    quarter: q
                }
                .table()
            );
        }
    }

    #[test]
    fn rotations_equal_face_slice_counter_face_composition() {
        for q in 1..=3u8 {
            let inv = 4 - q;
            let mut composed = Cube::solved();
            composed.apply(Move::Face {
                face: Face::R,
                quarter: q,
            });
            composed.apply(Move::Slice {
                slice: Slice::M,
                quarter: inv,
            });
            composed.apply(Move::Face {
                face: Face::L,
                quarter: inv,
            });
            let mut rotation = Cube::solved();
            rotation.apply(Move::Rotation {
                axis: Axis::X,
                quarter: q,
            });
            assert_eq!(composed, rotation, "x quarter {}", q);

            let mut composed = Cube::solved();
            composed.apply(Move::Face {
                face: Face::U,
                quarter: q,
            });
            composed.apply(Move::Slice {
                slice: Slice::E,
                quarter: inv,
            });
            composed.apply(Move::Face {
                face: Face::D,
                quarter: inv,
            });
            let mut rotation = Cube::solved();
            rotation.apply(Move::Rotation {
                axis: Axis::Y,
                quarter: q,
            });
            assert_eq!(composed, rotation, "y quarter {}", q);

            let mut composed = Cube::solved();
            composed.apply(Move::Face {
                face: Face::F,
                quarter: q,
            });
            composed.apply(Move::Slice {
                slice: Slice::S,
                quarter: q,
            });
            composed.apply(Move::Face {
                face: Face::B,
                quarter: inv,
            });
            let mut rotation = Cube::solved();
            rotation.apply(Move::Rotation {
                axis: Axis::Z,
                quarter: q,
            });
            assert_eq!(composed, rotation, "z quarter {}", q);
        }
    }

    #[test]
    fn wide_equals_face_plus_following_inner() {
        for face in Face::ALL {
            let mut composed = Cube::solved();
            composed.apply(Move::Face { face, quarter: 1 });
            composed.apply(Move::Inner { face, quarter: 1 });
            let mut wide = Cube::solved();
            wide.apply(Move::Wide { face, quarter: 1 });
            assert_eq!(composed, wide, "{:?}", face);
        }
    }

    #[test]
    fn invert_is_an_involution() {
        for mv in all_generators() {
            assert_eq!(mv.invert().invert(), mv);
            let mut cube = Cube::solved();
            cube.apply(mv);
            cube.apply(mv.invert());
            assert!(cube.is_solved(), "{:?}", mv);
        }
    }
}
