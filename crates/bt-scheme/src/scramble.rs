use crate::memo::{CORNER_SUCC, corner_twist_state, edge_flipped, edge_mate};
use crate::scheme::Orbit;
use bt_core::{Cube, Piece, edge_stickers, piece_of};

pub struct Rng {
    state: u64,
}

impl Rng {
    pub fn new(seed: u64) -> Rng {
        Rng { state: seed }
    }

    pub fn next_u64(&mut self) -> u64 {
        self.state = self.state.wrapping_add(0x9E37_79B9_7F4A_7C15);
        let mut z = self.state;
        z = (z ^ (z >> 30)).wrapping_mul(0xBF58_476D_1CE4_E5B9);
        z = (z ^ (z >> 27)).wrapping_mul(0x94D0_49BB_1331_11EB);
        z ^ (z >> 31)
    }

    pub fn below(&mut self, n: u64) -> u64 {
        (((self.next_u64() >> 32) as u128 * n as u128) >> 32) as u64
    }
}

fn fisher_yates(rng: &mut Rng, perm: &mut [u8]) {
    for i in (1..perm.len()).rev() {
        let j = rng.below(i as u64 + 1) as usize;
        perm.swap(i, j);
    }
}

fn perm_sign(perm: &[u8]) -> i8 {
    let mut seen = vec![false; perm.len()];
    let mut sign = 1i8;
    for i in 0..perm.len() {
        if seen[i] {
            continue;
        }
        let mut len = 0usize;
        let mut cur = i;
        while !seen[cur] {
            seen[cur] = true;
            cur = perm[cur] as usize;
            len += 1;
        }
        if len.is_multiple_of(2) {
            sign = -sign;
        }
    }
    sign
}

fn identity_occ() -> [u8; 54] {
    std::array::from_fn(|i| i as u8)
}

fn build_occ(occ: &mut [u8; 54], edges: &[u8], flips: &[bool], corners: &[u8], twists: &[u8]) {
    for slot in 0..12usize {
        let [p0, p1] = edge_stickers(slot as u8);
        let [o0, o1] = edge_stickers(edges[slot]);
        if flips[slot] {
            occ[p0 as usize] = o1;
            occ[p1 as usize] = o0;
        } else {
            occ[p0 as usize] = o0;
            occ[p1 as usize] = o1;
        }
    }
    for slot in 0..8usize {
        let slot_cyc = CORNER_SUCC[slot];
        let occ_cyc = CORNER_SUCC[corners[slot] as usize];
        let w = twists[slot] as usize % 3;
        for j in 0..3 {
            occ[slot_cyc[(j + w) % 3] as usize] = occ_cyc[j];
        }
    }
}

pub fn random_occ(seed: u64) -> [u8; 54] {
    let mut rng = Rng::new(seed);
    for _ in 0..8 {
        let mut edges: Vec<u8> = (0..12u8).collect();
        let mut corners: Vec<u8> = (0..8u8).collect();
        fisher_yates(&mut rng, &mut edges);
        fisher_yates(&mut rng, &mut corners);
        let mut flips: Vec<bool> = (0..12).map(|_| rng.next_u64() & 1 == 1).collect();
        let mut twists: Vec<u8> = (0..8).map(|_| rng.below(3) as u8).collect();
        let sum: u32 = twists[..7].iter().map(|&t| t as u32).sum();
        twists[7] = ((3 - sum % 3) % 3) as u8;
        if flips.iter().filter(|f| **f).count() % 2 == 1 {
            flips[11] = !flips[11];
        }
        if perm_sign(&edges) != perm_sign(&corners) {
            edges.swap(0, 1);
        }
        let mut occ = identity_occ();
        build_occ(&mut occ, &edges, &flips, &corners, &twists);
        if occ != identity_occ() {
            return occ;
        }
    }
    let mut edges: Vec<u8> = (0..12u8).collect();
    let mut corners: Vec<u8> = (0..8u8).collect();
    edges.swap(0, 1);
    corners.swap(0, 1);
    let flips = vec![false; 12];
    let twists = vec![0u8; 8];
    let mut occ = identity_occ();
    build_occ(&mut occ, &edges, &flips, &corners, &twists);
    occ
}

pub fn twist_sum(occ: &[u8; 54]) -> u32 {
    (0..8u8)
        .map(|slot| corner_twist_state(occ, slot) as u32)
        .sum()
}

pub fn flip_count(occ: &[u8; 54]) -> u32 {
    (0..12u8).filter(|&slot| edge_flipped(occ, slot)).count() as u32
}

fn slot_occupant(occ: &[u8; 54], orbit: Orbit, slot: u8) -> Option<u8> {
    let head = match orbit {
        Orbit::Corner => CORNER_SUCC[slot as usize][0],
        Orbit::Edge => edge_stickers(slot)[0],
    };
    match piece_of(occ[head as usize]) {
        Piece::Corner(p) => match orbit {
            Orbit::Corner => Some(p),
            Orbit::Edge => None,
        },
        Piece::Edge(p) => match orbit {
            Orbit::Edge => Some(p),
            Orbit::Corner => None,
        },
    }
}

pub fn piece_perm(occ: &[u8; 54], orbit: Orbit) -> Option<Vec<u8>> {
    let slots = match orbit {
        Orbit::Corner => 8,
        Orbit::Edge => 12,
    };
    let mut perm = Vec::new();
    for slot in 0..slots {
        perm.push(slot_occupant(occ, orbit, slot)?);
    }
    Some(perm)
}

pub fn occ_is_legal(occ: &[u8; 54]) -> bool {
    let mut seen = [false; 54];
    for &s in occ.iter() {
        if s >= 54 || seen[s as usize] {
            return false;
        }
        seen[s as usize] = true;
    }
    if (48..54usize).any(|i| occ[i] != i as u8) {
        return false;
    }
    for sticker in 0..48u8 {
        let pos = occ.iter().position(|&s| s == sticker).unwrap();
        if (sticker < 24) != (pos < 24) {
            return false;
        }
    }
    for slot in 0..12u8 {
        let [p0, p1] = edge_stickers(slot);
        if occ[p1 as usize] != edge_mate(occ[p0 as usize]) {
            return false;
        }
    }
    for slot in 0..8u8 {
        let cyc = CORNER_SUCC[slot as usize];
        let Piece::Corner(p) = piece_of(occ[cyc[0] as usize]) else {
            return false;
        };
        for i in 0..3 {
            match piece_of(occ[cyc[i] as usize]) {
                Piece::Corner(q) if q == p => {}
                _ => return false,
            }
        }
    }
    let Some(edge_perm) = piece_perm(occ, Orbit::Edge) else {
        return false;
    };
    let Some(corner_perm) = piece_perm(occ, Orbit::Corner) else {
        return false;
    };
    twist_sum(occ).is_multiple_of(3)
        && flip_count(occ).is_multiple_of(2)
        && perm_sign(&edge_perm) == perm_sign(&corner_perm)
}

pub fn random_state(seed: u64) -> Cube {
    Cube::from_occ(random_occ(seed))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn splitmix64_reference_vectors() {
        let mut rng = Rng::new(0);
        assert_eq!(rng.next_u64(), 16294208416658607535);
        assert_eq!(rng.next_u64(), 7960286522194355700);
        assert_eq!(rng.next_u64(), 487617019471545679);
        assert_eq!(rng.next_u64(), 17909611376780542444);
        let mut rng2 = Rng::new(0x0123_4567_89AB_CDEF);
        assert_eq!(rng2.next_u64(), 1547611027431991965);
        assert_eq!(rng2.next_u64(), 15380727978956804243);
    }

    #[test]
    fn below_stays_in_range_and_spreads() {
        let mut rng = Rng::new(42);
        let mut counts = vec![0u32; 5];
        for _ in 0..5000 {
            let v = rng.below(5);
            assert!(v < 5);
            counts[v as usize] += 1;
        }
        for c in &counts {
            assert!(*c > 800 && *c < 1200, "{counts:?}");
        }
        assert_eq!(Rng::new(7).below(1), 0);
    }

    #[test]
    fn fisher_yates_keeps_permutation_property() {
        let mut rng = Rng::new(123);
        let mut perm: Vec<u8> = (0..12u8).collect();
        fisher_yates(&mut rng, &mut perm);
        let mut sorted = perm.clone();
        sorted.sort_unstable();
        assert_eq!(sorted, (0..12u8).collect::<Vec<u8>>());
        assert_ne!(perm, (0..12u8).collect::<Vec<u8>>());
    }

    #[test]
    fn random_occ_is_legal_and_unsolved_over_thousands() {
        for seed in 0..3000u64 {
            let occ = random_occ(seed);
            assert!(occ_is_legal(&occ), "seed {seed}");
            assert_ne!(occ, identity_occ(), "seed {seed}");
            assert_eq!(twist_sum(&occ) % 3, 0, "seed {seed}");
            assert_eq!(flip_count(&occ) % 2, 0, "seed {seed}");
        }
    }

    #[test]
    fn random_occ_is_deterministic_and_spreads() {
        for seed in 0..200u64 {
            assert_eq!(random_occ(seed), random_occ(seed));
        }
        let mut distinct = std::collections::HashSet::new();
        for seed in 0..1000u64 {
            distinct.insert(random_occ(seed));
        }
        assert_eq!(distinct.len(), 1000);
    }

    #[test]
    fn perm_sign_matches_transposition_parity() {
        let mut identity: Vec<u8> = (0..12u8).collect();
        assert_eq!(perm_sign(&identity), 1);
        identity.swap(0, 1);
        assert_eq!(perm_sign(&identity), -1);
        identity.swap(2, 3);
        assert_eq!(perm_sign(&identity), 1);
        let cycle: Vec<u8> = vec![1, 2, 3, 0, 4, 5, 6, 7, 8, 9, 10, 11];
        assert_eq!(perm_sign(&cycle), -1);
    }

    #[test]
    fn identity_and_single_transposition_legality() {
        assert!(occ_is_legal(&identity_occ()));
        let mut occ = identity_occ();
        occ.swap(26, 32);
        assert!(!occ_is_legal(&occ));
    }
}
