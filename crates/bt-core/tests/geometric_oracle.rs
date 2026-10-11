use bt_core::{Axis, Face, Move, Slice};

const NORMALS: [[i32; 3]; 6] = [
    [0, 1, 0],
    [-1, 0, 0],
    [0, 0, 1],
    [1, 0, 0],
    [0, 0, -1],
    [0, -1, 0],
];
const AXES: [usize; 6] = [1, 0, 2, 0, 2, 1];
const SIGNS: [i32; 6] = [1, -1, 1, 1, -1, -1];
const FACE_LETTERS: [char; 6] = ['U', 'L', 'F', 'R', 'B', 'D'];

fn grid(face: usize, r: i32, c: i32) -> [i32; 3] {
    match face {
        0 => [c - 1, 1, r - 1],
        1 => [-1, 1 - r, c - 1],
        2 => [c - 1, 1 - r, 1],
        3 => [1, 1 - r, 1 - c],
        4 => [1 - c, 1 - r, -1],
        _ => [c - 1, -1, 1 - r],
    }
}

fn rot(mut v: [i32; 3], axis: usize, times: usize) -> [i32; 3] {
    for _ in 0..times {
        let (x, y, z) = (v[0], v[1], v[2]);
        v = match axis {
            0 => [x, -z, y],
            1 => [z, y, -x],
            _ => [-y, x, z],
        };
    }
    v
}

fn corner_pos(r: usize, c: usize) -> usize {
    match (r, c) {
        (0, 0) => 0,
        (0, 2) => 1,
        (2, 2) => 2,
        _ => 3,
    }
}

fn edge_pos(r: usize, c: usize) -> usize {
    match (r, c) {
        (0, 1) => 0,
        (1, 2) => 1,
        (2, 1) => 2,
        _ => 3,
    }
}

fn sticker_id(face: usize, r: usize, c: usize) -> usize {
    if r == 1 && c == 1 {
        48 + face
    } else if r.is_multiple_of(2) && c.is_multiple_of(2) {
        face * 4 + corner_pos(r, c)
    } else {
        24 + face * 4 + edge_pos(r, c)
    }
}

fn face_of_normal(n: [i32; 3]) -> usize {
    (0..6)
        .find(|&f| NORMALS[f] == n)
        .unwrap_or_else(|| panic!("no face for normal {:?}", n))
}

fn cell_of(face: usize, p: [i32; 3]) -> (usize, usize) {
    for r in 0..3 {
        for c in 0..3 {
            if grid(face, r as i32, c as i32) == p {
                return (r, c);
            }
        }
    }
    panic!("no cell on face {} for {:?}", FACE_LETTERS[face], p)
}

struct Sticker {
    pos: [i32; 3],
    n: [i32; 3],
}

fn stickers() -> Vec<Sticker> {
    let mut out = Vec::new();
    for (face, &n) in NORMALS.iter().enumerate() {
        for r in 0..3 {
            for c in 0..3 {
                out.push(Sticker {
                    pos: grid(face, r, c),
                    n,
                });
            }
        }
    }
    out
}

fn turn_layer(s: &mut [Sticker], face: usize, depth: i32, whole: bool, quarter: usize) {
    let n = NORMALS[face];
    let applied = if SIGNS[face] < 0 {
        quarter
    } else {
        (4 - quarter) % 4
    };
    for st in s.iter_mut() {
        let d = st.pos[0] * n[0] + st.pos[1] * n[1] + st.pos[2] * n[2];
        if whole || d == depth {
            st.pos = rot(st.pos, AXES[face], applied);
            st.n = rot(st.n, AXES[face], applied);
        }
    }
}

type Layer = (usize, i32, bool);

fn oracle_perm(layers: &[Layer], quarter: usize) -> [u8; 54] {
    let mut s = stickers();
    for &(face, depth, whole) in layers {
        turn_layer(&mut s, face, depth, whole, quarter);
    }
    let mut perm = [0u8; 54];
    for face in 0..6 {
        for r in 0..3 {
            for c in 0..3 {
                let id = sticker_id(face, r, c);
                let st = &s[face * 9 + r * 3 + c];
                let dst = face_of_normal(st.n);
                let cell = cell_of(dst, st.pos);
                perm[id] = sticker_id(dst, cell.0, cell.1) as u8;
            }
        }
    }
    perm
}

fn face_of(idx: usize) -> Face {
    Face::ALL[idx]
}

#[test]
fn face_tables_match_pysim_geometry() {
    for (f, &letter) in FACE_LETTERS.iter().enumerate() {
        for q in 1..=3usize {
            let mv = Move::Face {
                face: face_of(f),
                quarter: q as u8,
            };
            assert_eq!(
                mv.table(),
                oracle_perm(&[(f, 1, false)], q),
                "{} quarter {}",
                letter,
                q
            );
        }
    }
}

#[test]
fn inner_tables_match_slice_geometry() {
    for (f, &letter) in FACE_LETTERS.iter().enumerate() {
        for q in 1..=3usize {
            let mv = Move::Inner {
                face: face_of(f),
                quarter: q as u8,
            };
            assert_eq!(
                mv.table(),
                oracle_perm(&[(f, 0, false)], q),
                "2{} quarter {}",
                letter,
                q
            );
        }
    }
}

#[test]
fn slice_tables_match_following_face_geometry() {
    let follows = [(Slice::M, 1usize), (Slice::E, 5), (Slice::S, 2)];
    for (slice, f) in follows {
        for q in 1..=3usize {
            let mv = Move::Slice {
                slice,
                quarter: q as u8,
            };
            assert_eq!(
                mv.table(),
                oracle_perm(&[(f, 0, false)], q),
                "{:?} quarter {}",
                slice,
                q
            );
        }
    }
}

#[test]
fn wide_tables_match_face_plus_inner_geometry() {
    for (f, &letter) in FACE_LETTERS.iter().enumerate() {
        for q in 1..=3usize {
            let mv = Move::Wide {
                face: face_of(f),
                quarter: q as u8,
            };
            assert_eq!(
                mv.table(),
                oracle_perm(&[(f, 1, false), (f, 0, false)], q),
                "{} quarter {}",
                letter.to_ascii_lowercase(),
                q
            );
        }
    }
}

#[test]
fn rotation_tables_match_whole_cube_geometry() {
    let follows = [(Axis::X, 3usize), (Axis::Y, 0), (Axis::Z, 2)];
    for (axis, f) in follows {
        for q in 1..=3usize {
            let mv = Move::Rotation {
                axis,
                quarter: q as u8,
            };
            assert_eq!(
                mv.table(),
                oracle_perm(&[(f, 0, true)], q),
                "{:?} quarter {}",
                axis,
                q
            );
        }
    }
}
