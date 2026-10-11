use crate::alg::Alg;
use crate::moves::Move;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Face {
    U,
    L,
    F,
    R,
    B,
    D,
}

impl Face {
    pub const ALL: [Face; 6] = [Face::U, Face::L, Face::F, Face::R, Face::B, Face::D];

    pub fn index(self) -> usize {
        self as usize
    }
}

pub fn corner_id(face: Face, pos: u8) -> u8 {
    (face.index() * 4 + pos as usize) as u8
}

pub fn edge_id(face: Face, pos: u8) -> u8 {
    (24 + face.index() * 4 + pos as usize) as u8
}

pub fn center_id(face: Face) -> u8 {
    (48 + face.index()) as u8
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct Cube {
    occ: [u8; 54],
}

impl Cube {
    pub fn solved() -> Cube {
        Cube {
            occ: std::array::from_fn(|i| i as u8),
        }
    }

    pub fn from_occ(occ: [u8; 54]) -> Cube {
        debug_assert!({
            let mut seen = [false; 54];
            for &s in &occ {
                seen[s as usize] = true;
            }
            seen.iter().all(|&s| s)
        });
        Cube { occ }
    }

    pub fn apply(&mut self, mv: Move) {
        let table = mv.table();
        let mut next = self.occ;
        for (i, &sticker) in self.occ.iter().enumerate() {
            next[table[i] as usize] = sticker;
        }
        self.occ = next;
    }

    pub fn apply_alg(&mut self, alg: &Alg) {
        for &mv in alg.moves() {
            self.apply(mv);
        }
    }

    pub fn sticker_at(&self, pos: u8) -> u8 {
        self.occ[pos as usize]
    }

    pub fn position_of(&self, sticker: u8) -> u8 {
        self.occ
            .iter()
            .position(|&s| s == sticker)
            .expect("sticker not present") as u8
    }

    pub fn inverse_state(&self) -> Cube {
        let mut inv = [0u8; 54];
        for (i, &sticker) in self.occ.iter().enumerate() {
            inv[sticker as usize] = i as u8;
        }
        Cube { occ: inv }
    }

    pub fn is_solved(&self) -> bool {
        self.occ.iter().enumerate().all(|(i, &s)| s as usize == i)
    }

    pub fn centers_fixed(&self) -> bool {
        self.occ[48..54]
            .iter()
            .enumerate()
            .all(|(f, &s)| s as usize == 48 + f)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn solved_state_is_identity() {
        let cube = Cube::solved();
        assert!(cube.is_solved());
        assert!(cube.centers_fixed());
        assert_eq!(cube, cube.inverse_state());
        for pos in 0..54u8 {
            assert_eq!(cube.sticker_at(pos), pos);
            assert_eq!(cube.position_of(pos), pos);
        }
    }

    #[test]
    fn apply_then_inverse_alg_restores_solved() {
        let alg = Alg::parse("R U r' 2F x2 M2 S'").unwrap();
        let mut cube = Cube::solved();
        cube.apply_alg(&alg);
        assert!(!cube.is_solved());
        cube.apply_alg(&alg.inverse());
        assert!(cube.is_solved());
    }

    #[test]
    fn inverse_state_matches_inverse_alg_state() {
        let alg = Alg::parse("F B' l2 M E' 2R z").unwrap();
        let mut forward = Cube::solved();
        forward.apply_alg(&alg);
        let mut backward = Cube::solved();
        backward.apply_alg(&alg.inverse());
        assert_eq!(forward.inverse_state(), backward);
    }

    #[test]
    fn sticker_at_and_position_of_are_inverses_after_moves() {
        let mut cube = Cube::solved();
        cube.apply_alg(&Alg::parse("R U R' U' M x' 2B2").unwrap());
        for sticker in 0..54u8 {
            assert_eq!(cube.sticker_at(cube.position_of(sticker)), sticker);
        }
        for pos in 0..54u8 {
            assert_eq!(cube.position_of(cube.sticker_at(pos)), pos);
        }
    }

    #[test]
    fn only_face_turns_keep_centers_fixed() {
        for alg in ["U", "D", "L", "R", "F", "B"] {
            let mut cube = Cube::solved();
            cube.apply_alg(&Alg::parse(alg).unwrap());
            assert!(cube.centers_fixed(), "{}", alg);
        }
        for alg in ["x", "M", "l", "2F"] {
            let mut cube = Cube::solved();
            cube.apply_alg(&Alg::parse(alg).unwrap());
            assert!(!cube.centers_fixed(), "{}", alg);
        }
    }

    #[test]
    fn sticker_id_layout_matches_position_map() {
        assert_eq!(corner_id(Face::U, 2), 2);
        assert_eq!(edge_id(Face::U, 2), 26);
        assert_eq!(center_id(Face::U), 48);
        assert_eq!(corner_id(Face::D, 0), 20);
        assert_eq!(edge_id(Face::D, 0), 44);
        assert_eq!(center_id(Face::D), 53);
    }

    #[test]
    fn from_occ_rebuilds_equivalent_state() {
        let mut cube = Cube::solved();
        cube.apply_alg(&Alg::parse("R U F2 M x' 2L S2").unwrap());
        let mut occ = [0u8; 54];
        for sticker in 0..54u8 {
            occ[cube.position_of(sticker) as usize] = sticker;
        }
        let rebuilt = Cube::from_occ(occ);
        for pos in 0..54u8 {
            assert_eq!(rebuilt.sticker_at(pos), cube.sticker_at(pos));
        }
        assert_eq!(rebuilt, cube);
        assert!(Cube::from_occ(std::array::from_fn(|i| i as u8)).is_solved());
    }
}
