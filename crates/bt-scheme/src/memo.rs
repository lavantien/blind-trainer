use crate::pairs::CaseId;
use crate::scheme::{
    CORNER_BUFFER_STICKER, EDGE_BUFFER_STICKER, Orbit, corner_floating_heads, edge_floating_heads,
    letter_of, same_piece,
};
use bt_core::{Cube, Piece, corner_stickers, edge_stickers, head_sticker, piece_of};

pub(crate) const CORNER_SUCC: [[u8; 3]; 8] = [
    [1, 13, 16],
    [0, 17, 4],
    [2, 9, 12],
    [3, 5, 8],
    [22, 19, 14],
    [23, 7, 18],
    [21, 15, 10],
    [20, 11, 6],
];

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Memo {
    pub edge_letters: Vec<char>,
    pub corner_letters: Vec<char>,
    pub pairs: (Vec<CaseId>, Vec<CaseId>),
    pub buffers: (Vec<u8>, Vec<u8>),
    pub residue: Residue,
}

#[derive(Clone, PartialEq, Eq, Debug, Default)]
pub struct Residue {
    pub parity: Option<Parity>,
    pub flips: Vec<u8>,
    pub twists: Vec<Twist>,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct Parity {
    pub edge: (u8, u8),
    pub corner: (u8, u8),
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct Twist {
    pub corner: u8,
    pub quarter: u8,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum ResidueClass {
    None,
    FlipPair(u8, u8),
    TwistPair(u8, u8),
    TwistTriple(u8, u8, u8),
    Parity,
    Ltct,
    Compound,
}

impl Residue {
    pub fn class(&self) -> ResidueClass {
        if self.parity.is_none() {
            if self.twists.is_empty() {
                return match self.flips.as_slice() {
                    [] => ResidueClass::None,
                    [e1, e2] => ResidueClass::FlipPair(*e1, *e2),
                    _ => ResidueClass::Compound,
                };
            }
            if self.flips.is_empty() {
                let corners: Vec<u8> = self.twists.iter().map(|t| t.corner).collect();
                return match self.twists.as_slice() {
                    [a, b] if a.quarter + b.quarter == 3 => {
                        ResidueClass::TwistPair(corners[0], corners[1])
                    }
                    [a, b, c] if a.quarter == b.quarter && b.quarter == c.quarter => {
                        ResidueClass::TwistTriple(corners[0], corners[1], corners[2])
                    }
                    _ => ResidueClass::Compound,
                };
            }
            return ResidueClass::Compound;
        }
        if self.flips.is_empty() && self.twists.is_empty() {
            return ResidueClass::Parity;
        }
        if self.flips.is_empty() {
            let (c1, c2) = self.parity.unwrap().corner;
            let outside = self
                .twists
                .iter()
                .any(|t| !same_piece(t.corner, c1) && !same_piece(t.corner, c2));
            if outside {
                return ResidueClass::Ltct;
            }
            return ResidueClass::Parity;
        }
        ResidueClass::Compound
    }
}

pub fn state_array(cube: &Cube) -> [u8; 54] {
    std::array::from_fn(|i| cube.sticker_at(i as u8))
}

pub(crate) fn corner_succ(pos: u8) -> u8 {
    match piece_of(pos) {
        Piece::Corner(p) => {
            let cyc = &CORNER_SUCC[p as usize];
            let i = cyc.iter().position(|&s| s == pos).unwrap();
            cyc[(i + 1) % 3]
        }
        _ => panic!("corner_succ on non corner sticker {pos}"),
    }
}

pub(crate) fn edge_mate(pos: u8) -> u8 {
    match piece_of(pos) {
        Piece::Edge(p) => {
            let [a, b] = edge_stickers(p);
            if a == pos { b } else { a }
        }
        _ => panic!("edge_mate on non edge sticker {pos}"),
    }
}

pub(crate) fn piece_index(sticker: u8) -> u8 {
    match piece_of(sticker) {
        Piece::Corner(p) | Piece::Edge(p) => p,
    }
}

pub(crate) fn head_of(orbit: Orbit, piece: u8) -> u8 {
    match orbit {
        Orbit::Corner => head_sticker(Piece::Corner(piece)),
        Orbit::Edge => head_sticker(Piece::Edge(piece)),
    }
}

pub(crate) fn stickers_of(orbit: Orbit, piece: u8) -> Vec<u8> {
    match orbit {
        Orbit::Corner => corner_stickers(piece).to_vec(),
        Orbit::Edge => edge_stickers(piece).to_vec(),
    }
}

pub(crate) fn floating_pieces(orbit: Orbit) -> Vec<u8> {
    let heads = match orbit {
        Orbit::Corner => corner_floating_heads().to_vec(),
        Orbit::Edge => edge_floating_heads().to_vec(),
    };
    heads.iter().map(|&h| piece_index(h)).collect()
}

pub(crate) fn solved_piece(occ: &[u8; 54], orbit: Orbit, piece: u8) -> bool {
    stickers_of(orbit, piece)
        .iter()
        .all(|&p| occ[p as usize] == p)
}

pub(crate) fn in_place(occ: &[u8; 54], orbit: Orbit, piece: u8) -> bool {
    let homes = stickers_of(orbit, piece);
    let mut occupants: Vec<u8> = homes.iter().map(|&p| occ[p as usize]).collect();
    let mut expected = homes;
    occupants.sort_unstable();
    expected.sort_unstable();
    occupants == expected
}

pub fn edge_flipped(occ: &[u8; 54], slot: u8) -> bool {
    let p0 = edge_stickers(slot)[0];
    let occupant = piece_index(occ[p0 as usize]);
    occ[p0 as usize] != head_of(Orbit::Edge, occupant)
}

pub fn corner_twist_state(occ: &[u8; 54], slot: u8) -> u8 {
    let cyc = &CORNER_SUCC[slot as usize];
    let ohead = head_of(Orbit::Corner, piece_index(occ[cyc[0] as usize]));
    cyc.iter().position(|&p| occ[p as usize] == ohead).unwrap() as u8
}

fn pair_cycles(orbit: Orbit, h: u8, t1: u8, t2: u8) -> Vec<[u8; 3]> {
    match orbit {
        Orbit::Edge => vec![[h, t1, t2], [edge_mate(h), edge_mate(t1), edge_mate(t2)]],
        Orbit::Corner => {
            let (mut sh, mut s1, mut s2) = (h, t1, t2);
            let mut cycles = Vec::new();
            for _ in 0..3 {
                cycles.push([sh, s1, s2]);
                sh = corner_succ(sh);
                s1 = corner_succ(s1);
                s2 = corner_succ(s2);
            }
            cycles
        }
    }
}

pub fn ideal_cycle(case: &CaseId) -> Vec<[u8; 3]> {
    let h = match case.orbit {
        Orbit::Corner => CORNER_BUFFER_STICKER,
        Orbit::Edge => EDGE_BUFFER_STICKER,
    };
    pair_cycles(case.orbit, h, case.t1, case.t2)
}

pub fn ideal_cycle_at(orbit: Orbit, buffer: u8, t1: u8, t2: u8) -> Vec<[u8; 3]> {
    pair_cycles(orbit, buffer, t1, t2)
}

pub fn apply_sticker_cycles(occ: &mut [u8; 54], cycles: &[[u8; 3]]) {
    for &[a, b, c] in cycles {
        let (sa, sb, sc) = (occ[a as usize], occ[b as usize], occ[c as usize]);
        occ[b as usize] = sa;
        occ[c as usize] = sb;
        occ[a as usize] = sc;
    }
}

fn apply_swap(occ: &mut [u8; 54], a: u8, b: u8) {
    occ.swap(a as usize, b as usize);
}

fn apply_aligned_swap(occ: &mut [u8; 54], orbit: Orbit, h1: u8, h2: u8) {
    match orbit {
        Orbit::Edge => {
            apply_swap(occ, h1, h2);
            apply_swap(occ, edge_mate(h1), edge_mate(h2));
        }
        Orbit::Corner => {
            let (mut a, mut b) = (h1, h2);
            for _ in 0..3 {
                apply_swap(occ, a, b);
                a = corner_succ(a);
                b = corner_succ(b);
            }
        }
    }
}

fn apply_flip(occ: &mut [u8; 54], head: u8) {
    apply_swap(occ, head, edge_mate(head));
}

fn apply_twist(occ: &mut [u8; 54], head: u8, quarter: u8) {
    let cyc = CORNER_SUCC[piece_index(head) as usize];
    let (s0, s1, s2) = (
        occ[cyc[0] as usize],
        occ[cyc[1] as usize],
        occ[cyc[2] as usize],
    );
    if quarter == 1 {
        occ[cyc[1] as usize] = s0;
        occ[cyc[2] as usize] = s1;
        occ[cyc[0] as usize] = s2;
    } else {
        occ[cyc[0] as usize] = s1;
        occ[cyc[1] as usize] = s2;
        occ[cyc[2] as usize] = s0;
    }
}

pub fn apply_residue(occ: &mut [u8; 54], residue: &Residue) {
    if let Some(parity) = residue.parity {
        apply_aligned_swap(occ, Orbit::Edge, parity.edge.0, parity.edge.1);
        apply_aligned_swap(occ, Orbit::Corner, parity.corner.0, parity.corner.1);
    }
    for &head in &residue.flips {
        apply_flip(occ, head);
    }
    for twist in &residue.twists {
        apply_twist(occ, twist.corner, twist.quarter);
    }
}

pub fn apply_memo(occ: &mut [u8; 54], memo: &Memo) {
    for (case, &buffer) in memo.pairs.0.iter().zip(memo.buffers.0.iter()) {
        let cycles = pair_cycles(case.orbit, buffer, case.t1, case.t2);
        apply_sticker_cycles(occ, &cycles);
    }
    for (case, &buffer) in memo.pairs.1.iter().zip(memo.buffers.1.iter()) {
        let cycles = pair_cycles(case.orbit, buffer, case.t1, case.t2);
        apply_sticker_cycles(occ, &cycles);
    }
    apply_residue(occ, &memo.residue);
}

fn assert_case_legal(case: &CaseId) {
    let buffer = match case.orbit {
        Orbit::Corner => CORNER_BUFFER_STICKER,
        Orbit::Edge => EDGE_BUFFER_STICKER,
    };
    assert!(
        case.orbit.stickers().contains(&case.t1),
        "t1 outside orbit: {case:?}"
    );
    assert!(
        case.orbit.stickers().contains(&case.t2),
        "t2 outside orbit: {case:?}"
    );
    assert!(!same_piece(case.t1, buffer), "t1 on buffer piece: {case:?}");
    assert!(!same_piece(case.t2, buffer), "t2 on buffer piece: {case:?}");
    assert!(!same_piece(case.t1, case.t2), "t2 on t1 piece: {case:?}");
}

struct WalkOutcome {
    letters: Vec<char>,
    pairs: Vec<CaseId>,
    buffers: Vec<u8>,
    parity: Option<(u8, u8)>,
}

struct OrbitWalk<'a> {
    orbit: Orbit,
    occ: &'a mut [u8; 54],
    pieces: Vec<u8>,
    reserved: Vec<bool>,
    visited: [bool; 48],
    letters: Vec<char>,
    pairs: Vec<CaseId>,
    buffers: Vec<u8>,
    pending: Vec<u8>,
    parity: Option<(u8, u8)>,
}

impl<'a> OrbitWalk<'a> {
    fn new(orbit: Orbit, occ: &'a mut [u8; 54]) -> Self {
        let count = match orbit {
            Orbit::Corner => 8,
            Orbit::Edge => 12,
        };
        OrbitWalk {
            orbit,
            occ,
            pieces: floating_pieces(orbit),
            reserved: vec![false; count],
            visited: [false; 48],
            letters: Vec::new(),
            pairs: Vec::new(),
            buffers: Vec::new(),
            pending: Vec::new(),
            parity: None,
        }
    }

    fn main_scan(&mut self) -> Option<u8> {
        for &p in &self.pieces {
            if self.reserved[p as usize] || solved_piece(self.occ, self.orbit, p) {
                continue;
            }
            if in_place(self.occ, self.orbit, p) {
                self.reserved[p as usize] = true;
                continue;
            }
            return Some(p);
        }
        None
    }

    fn read_block(&mut self) -> Option<u8> {
        for &p in &self.pieces {
            if self.reserved[p as usize] || solved_piece(self.occ, self.orbit, p) {
                continue;
            }
            if in_place(self.occ, self.orbit, p) {
                self.reserved[p as usize] = true;
                continue;
            }
            if stickers_of(self.orbit, p)
                .iter()
                .any(|&s| self.visited[s as usize])
            {
                continue;
            }
            return Some(p);
        }
        None
    }

    fn run(self) -> WalkOutcome {
        let mut walk = self;
        let mut budget = 0u32;
        let mut spins = 0u32;
        loop {
            spins += 1;
            assert!(spins <= 200, "memo walk spin budget exceeded");
            let Some(buffer) = walk.main_scan() else {
                walk.pending.clear();
                break;
            };
            let h = head_of(walk.orbit, buffer);
            if walk
                .pending
                .first()
                .is_some_and(|&t| walk.reserved[piece_index(t) as usize])
            {
                walk.pending.clear();
            }
            let mut pos = if walk.pending.is_empty() {
                walk.visited[h as usize] = true;
                h
            } else {
                match walk.read_block() {
                    Some(q) => {
                        let qh = head_of(walk.orbit, q);
                        walk.visited[qh as usize] = true;
                        qh
                    }
                    None => {
                        let t = walk.pending[0];
                        walk.parity = Some((h, head_of(walk.orbit, piece_index(t))));
                        break;
                    }
                }
            };
            loop {
                let s = walk.occ[pos as usize];
                if same_piece(s, h) {
                    break;
                }
                walk.pending.push(s);
                walk.letters.push(letter_of(s));
                walk.visited[s as usize] = true;
                budget += 1;
                assert!(budget <= 96, "memo walk letter budget exceeded");
                if walk.pending.len() == 2 {
                    let case = CaseId {
                        orbit: walk.orbit,
                        t1: walk.pending[0],
                        t2: walk.pending[1],
                    };
                    assert_case_legal(&case);
                    walk.pairs.push(case);
                    walk.buffers.push(h);
                    let cycles = pair_cycles(walk.orbit, h, case.t1, case.t2);
                    apply_sticker_cycles(walk.occ, &cycles);
                    walk.pending.clear();
                    pos = h;
                } else {
                    pos = s;
                }
            }
        }
        WalkOutcome {
            letters: walk.letters,
            pairs: walk.pairs,
            buffers: walk.buffers,
            parity: walk.parity,
        }
    }
}

fn memo_occ(occ: &[u8; 54]) -> Memo {
    let mut work = *occ;
    let edge = OrbitWalk::new(Orbit::Edge, &mut work).run();
    let corner = OrbitWalk::new(Orbit::Corner, &mut work).run();
    let parity = match (edge.parity, corner.parity) {
        (None, None) => None,
        (Some(e), Some(c)) => {
            apply_aligned_swap(&mut work, Orbit::Edge, e.0, e.1);
            apply_aligned_swap(&mut work, Orbit::Corner, c.0, c.1);
            Some(Parity { edge: e, corner: c })
        }
        (e, c) => panic!("parity must hit both orbits: edge {e:?} corner {c:?}"),
    };
    let mut flips = Vec::new();
    for slot in 0..12u8 {
        if in_place(&work, Orbit::Edge, slot) && !solved_piece(&work, Orbit::Edge, slot) {
            flips.push(head_of(Orbit::Edge, slot));
        }
    }
    let mut twists = Vec::new();
    for slot in 0..8u8 {
        if in_place(&work, Orbit::Corner, slot) && !solved_piece(&work, Orbit::Corner, slot) {
            let quarter = corner_twist_state(&work, slot);
            twists.push(Twist {
                corner: head_of(Orbit::Corner, slot),
                quarter: if quarter == 1 { 2 } else { 1 },
            });
        }
    }
    Memo {
        edge_letters: edge.letters,
        corner_letters: corner.letters,
        pairs: (edge.pairs, corner.pairs),
        buffers: (edge.buffers, corner.buffers),
        residue: Residue {
            parity,
            flips,
            twists,
        },
    }
}

pub fn memo(state: &Cube) -> Memo {
    memo_occ(&state_array(state))
}

pub fn memo_from_array(occ: &[u8; 54]) -> Memo {
    memo_occ(occ)
}

#[cfg(test)]
mod tests {
    use super::*;
    use bt_core::{Alg, Axis, Move};

    fn apply(text: &str) -> Cube {
        let mut cube = Cube::solved();
        cube.apply_alg(&Alg::parse(text).unwrap());
        cube
    }

    fn occ_of(assignments: &[(u8, u8)]) -> [u8; 54] {
        let mut occ = std::array::from_fn(|i| i as u8);
        for &(pos, sticker) in assignments {
            occ[pos as usize] = sticker;
        }
        occ
    }

    fn round_trip(occ: &[u8; 54]) {
        let m = memo_occ(occ);
        let mut work = *occ;
        apply_memo(&mut work, &m);
        assert_eq!(work, std::array::from_fn(|i| i as u8));
    }

    #[test]
    fn corner_succ_cycles_are_pinned() {
        for cyc in CORNER_SUCC {
            assert_eq!(corner_succ(cyc[0]), cyc[1]);
            assert_eq!(corner_succ(cyc[1]), cyc[2]);
            assert_eq!(corner_succ(cyc[2]), cyc[0]);
        }
        assert_eq!(CORNER_SUCC[2], [2, 9, 12]);
        assert_eq!(CORNER_SUCC[0], [1, 13, 16]);
        assert_eq!(CORNER_SUCC[3], [3, 5, 8]);
        assert_eq!(CORNER_SUCC[5], [23, 7, 18]);
    }

    #[test]
    fn corner_succ_conjugates_under_rotations() {
        let rotations = [
            (Axis::X, 1u8),
            (Axis::X, 2),
            (Axis::X, 3),
            (Axis::Y, 1),
            (Axis::Y, 2),
            (Axis::Y, 3),
            (Axis::Z, 1),
            (Axis::Z, 2),
            (Axis::Z, 3),
        ];
        for &(axis, quarter) in &rotations {
            let mut cube = Cube::solved();
            cube.apply(Move::Rotation { axis, quarter });
            for p in 0..24u8 {
                let moved = cube.position_of(p);
                assert_eq!(
                    cube.position_of(corner_succ(p)),
                    corner_succ(moved),
                    "axis {axis:?} quarter {quarter} sticker {p}"
                );
            }
        }
    }

    #[test]
    fn ideal_cycle_structure_pinned() {
        let edge_case = CaseId {
            orbit: Orbit::Edge,
            t1: 24,
            t2: 25,
        };
        assert_eq!(ideal_cycle(&edge_case), vec![[26, 24, 25], [32, 40, 36]]);
        let corner_case = CaseId {
            orbit: Orbit::Corner,
            t1: 0,
            t2: 1,
        };
        assert_eq!(
            ideal_cycle(&corner_case),
            vec![[2, 0, 1], [9, 17, 13], [12, 4, 16]]
        );
        assert_eq!(
            ideal_cycle_at(Orbit::Edge, 24, 27, 43),
            vec![[24, 27, 43], [40, 28, 37]]
        );
    }

    #[test]
    fn memo_of_solved_is_empty() {
        let m = memo(&Cube::solved());
        assert!(m.edge_letters.is_empty());
        assert!(m.corner_letters.is_empty());
        assert!(m.pairs.0.is_empty());
        assert!(m.pairs.1.is_empty());
        assert!(m.buffers.0.is_empty());
        assert!(m.buffers.1.is_empty());
        assert_eq!(m.residue, Residue::default());
        assert_eq!(m.residue.class(), ResidueClass::None);
    }

    #[test]
    fn memo_of_four_cycles_is_pair_plus_parity_per_orbit() {
        let occ = occ_of(&[
            (26, 27),
            (32, 28),
            (24, 26),
            (40, 32),
            (25, 24),
            (36, 40),
            (27, 25),
            (28, 36),
            (3, 2),
            (5, 9),
            (8, 12),
            (0, 3),
            (17, 5),
            (4, 8),
            (1, 0),
            (13, 17),
            (16, 4),
            (2, 1),
            (9, 13),
            (12, 16),
        ]);
        let m = memo_occ(&occ);
        assert_eq!(m.edge_letters, vec!['D', 'B', 'A']);
        assert_eq!(
            m.pairs.0,
            vec![CaseId {
                orbit: Orbit::Edge,
                t1: 27,
                t2: 25
            }]
        );
        assert_eq!(m.buffers.0, vec![26]);
        assert_eq!(m.corner_letters, vec!['B', 'A', 'D']);
        assert_eq!(
            m.pairs.1,
            vec![CaseId {
                orbit: Orbit::Corner,
                t1: 1,
                t2: 0
            }]
        );
        assert_eq!(m.buffers.1, vec![2]);
        assert_eq!(
            m.residue.parity.unwrap(),
            Parity {
                edge: (26, 24),
                corner: (2, 3)
            }
        );
        assert!(m.residue.flips.is_empty());
        assert!(m.residue.twists.is_empty());
        assert_eq!(m.residue.class(), ResidueClass::Parity);
        round_trip(&occ);
    }

    #[test]
    fn memo_of_double_edge_swap_crosses_blocks() {
        let occ = occ_of(&[
            (26, 24),
            (32, 40),
            (24, 26),
            (40, 32),
            (25, 27),
            (36, 28),
            (27, 25),
            (28, 36),
        ]);
        let m = memo_occ(&occ);
        assert_eq!(m.edge_letters, vec!['A', 'D', 'B', 'D']);
        assert_eq!(
            m.pairs.0,
            vec![
                CaseId {
                    orbit: Orbit::Edge,
                    t1: 24,
                    t2: 27
                },
                CaseId {
                    orbit: Orbit::Edge,
                    t1: 25,
                    t2: 27
                }
            ]
        );
        assert_eq!(m.buffers.0, vec![26, 26]);
        assert_eq!(m.residue, Residue::default());
        assert!(m.pairs.1.is_empty());
        round_trip(&occ);
    }

    #[test]
    fn memo_of_two_flips_is_residue_only() {
        let occ = occ_of(&[(26, 32), (32, 26), (27, 28), (28, 27)]);
        let m = memo_occ(&occ);
        assert!(m.edge_letters.is_empty());
        assert!(m.pairs.0.is_empty());
        let mut flips = m.residue.flips.clone();
        flips.sort_unstable();
        assert_eq!(flips, vec![26, 27]);
        assert!(m.residue.parity.is_none());
        assert_eq!(m.residue.class(), ResidueClass::FlipPair(26, 27));
        round_trip(&occ);
    }

    #[test]
    fn memo_of_flip_mid_cycle_absorbs_into_pair() {
        let occ = occ_of(&[
            (26, 25),
            (32, 36),
            (24, 26),
            (40, 32),
            (36, 24),
            (25, 40),
            (27, 28),
            (28, 27),
        ]);
        let m = memo_occ(&occ);
        assert_eq!(m.edge_letters, vec!['B', 'Q']);
        assert_eq!(
            m.pairs.0,
            vec![CaseId {
                orbit: Orbit::Edge,
                t1: 25,
                t2: 40
            }]
        );
        let mut flips = m.residue.flips.clone();
        flips.sort_unstable();
        assert_eq!(flips, vec![26, 27]);
        assert!(m.residue.parity.is_none());
        round_trip(&occ);
    }

    #[test]
    fn memo_of_two_disjoint_cycles_floats_the_buffer() {
        let occ = occ_of(&[
            (26, 25),
            (32, 36),
            (25, 24),
            (36, 40),
            (24, 26),
            (40, 32),
            (46, 45),
            (42, 38),
            (45, 44),
            (38, 34),
            (44, 46),
            (34, 42),
        ]);
        let m = memo_occ(&occ);
        assert_eq!(m.edge_letters, vec!['B', 'A', 'W', 'V']);
        assert_eq!(
            m.pairs.0,
            vec![
                CaseId {
                    orbit: Orbit::Edge,
                    t1: 25,
                    t2: 24
                },
                CaseId {
                    orbit: Orbit::Edge,
                    t1: 46,
                    t2: 45
                }
            ]
        );
        assert_eq!(m.buffers.0, vec![26, 44]);
        assert_eq!(m.residue, Residue::default());
        round_trip(&occ);
    }

    #[test]
    fn round_trip_on_face_turns() {
        for text in ["U", "R", "F", "L2", "D'", "B2 U", "R U R' U'", "F2 D2 L2"] {
            let cube = apply(text);
            let m = memo(&cube);
            let mut work = state_array(&cube);
            apply_memo(&mut work, &m);
            assert_eq!(work, std::array::from_fn(|i| i as u8), "{text}");
        }
    }
}
