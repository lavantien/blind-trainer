use bt_core::{Alg, Cube};
use bt_scheme::memo::{apply_memo, corner_twist_state, edge_flipped, memo_from_array, state_array};
use bt_scheme::pairs::enumerate_cases;
use bt_scheme::scheme::Orbit;
use bt_scheme::scramble::{Rng, occ_is_legal, random_occ, random_state};
use proptest::prelude::*;
use std::collections::HashSet;

fn random_cube(seed: u64) -> Cube {
    let mut rng = Rng::new(seed);
    let faces = ["U", "D", "L", "R", "F", "B"];
    let suffixes = ["", "'", "2"];
    let len = 8 + rng.below(17);
    let mut tokens = Vec::new();
    let mut last = 6usize;
    for _ in 0..len {
        let mut f = rng.below(6) as usize;
        while f == last {
            f = rng.below(6) as usize;
        }
        last = f;
        tokens.push(format!("{}{}", faces[f], suffixes[rng.below(3) as usize]));
    }
    let mut cube = Cube::solved();
    cube.apply_alg(&Alg::parse(&tokens.join(" ")).unwrap());
    cube
}

#[test]
fn occ_orientation_predicates_match_bt_core() {
    for seed in 0..400u64 {
        let cube = random_cube(seed);
        let occ = state_array(&cube);
        for slot in 0..12u8 {
            assert_eq!(
                edge_flipped(&occ, slot),
                bt_core::edge_flip(&cube, slot),
                "seed {seed} slot {slot}"
            );
        }
        for slot in 0..8u8 {
            let mapped = match bt_core::corner_twist(&cube, slot) {
                0 => 0u8,
                1 => 1,
                _ => 2,
            };
            assert_eq!(
                corner_twist_state(&occ, slot),
                mapped,
                "seed {seed} slot {slot}"
            );
        }
    }
}

#[test]
fn random_occ_states_round_trip_through_memo() {
    let edge_cases: HashSet<(u8, u8)> = enumerate_cases(Orbit::Edge)
        .iter()
        .map(|c| (c.t1, c.t2))
        .collect();
    let corner_cases: HashSet<(u8, u8)> = enumerate_cases(Orbit::Corner)
        .iter()
        .map(|c| (c.t1, c.t2))
        .collect();
    for seed in 0..2000u64 {
        let occ = random_occ(seed);
        let m = memo_from_array(&occ);
        let mut work = occ;
        apply_memo(&mut work, &m);
        assert_eq!(
            work,
            std::array::from_fn(|i| i as u8),
            "seed {seed} letters {:?} {:?} residue {:?}",
            m.edge_letters,
            m.corner_letters,
            m.residue
        );
        assert_eq!(memo_from_array(&occ), m, "determinism seed {seed}");
        for case in &m.pairs.0 {
            assert!(edge_cases.contains(&(case.t1, case.t2)), "{case:?}");
        }
        for case in &m.pairs.1 {
            assert!(corner_cases.contains(&(case.t1, case.t2)), "{case:?}");
        }
        assert_eq!(m.pairs.0.len(), m.buffers.0.len());
        assert_eq!(m.pairs.1.len(), m.buffers.1.len());
        assert!(
            m.edge_letters.len() == 2 * m.pairs.0.len()
                || m.edge_letters.len() == 2 * m.pairs.0.len() + 1
        );
        assert!(
            m.corner_letters.len() == 2 * m.pairs.1.len()
                || m.corner_letters.len() == 2 * m.pairs.1.len() + 1
        );
        assert!(occ_is_legal(&occ), "seed {seed}");
    }
}

#[test]
fn random_occ_covers_orientation_and_parity_shapes() {
    let mut with_flips = 0;
    let mut with_twists = 0;
    let mut with_parity_memo = 0;
    for seed in 0..500u64 {
        let occ = random_occ(seed);
        if (0..12u8).any(|slot| edge_flipped(&occ, slot)) {
            with_flips += 1;
        }
        if (0..8u8).any(|slot| corner_twist_state(&occ, slot) != 0) {
            with_twists += 1;
        }
        if memo_from_array(&occ).residue.parity.is_some() {
            with_parity_memo += 1;
        }
    }
    assert!(with_flips > 300, "{with_flips}");
    assert!(with_twists > 200, "{with_twists}");
    assert!(with_parity_memo > 150, "{with_parity_memo}");
}

#[test]
fn random_state_cubes_are_legal_unsolved_and_center_fixed() {
    for seed in 0..2000u64 {
        let cube = random_state(seed);
        assert!(!cube.is_solved(), "seed {seed}");
        assert!(cube.centers_fixed(), "seed {seed}");
        assert_eq!(state_array(&cube), random_occ(seed), "seed {seed}");
        let twist: i32 = (0..8u8)
            .map(|corner| bt_core::corner_twist(&cube, corner) as i32)
            .sum();
        assert_eq!(twist % 3, 0, "seed {seed}");
        let flips = (0..12u8)
            .filter(|&edge| bt_core::edge_flip(&cube, edge))
            .count();
        assert!(flips.is_multiple_of(2), "seed {seed}");
    }
}

proptest::prelude::proptest! {

    #![proptest_config(proptest::prelude::ProptestConfig::with_cases(512))]

    #[test]
    fn prop_random_occ_is_legal_and_round_trips(seed in proptest::num::u64::ANY) {
        let occ = random_occ(seed);
        prop_assert!(occ_is_legal(&occ));
        prop_assert_ne!(occ, std::array::from_fn(|i| i as u8));
        let m = memo_from_array(&occ);
        let mut work = occ;
        apply_memo(&mut work, &m);
        prop_assert_eq!(work, std::array::from_fn(|i| i as u8));
        prop_assert_eq!(memo_from_array(&occ), m.clone());
        prop_assert_eq!(m.pairs.0.len(), m.buffers.0.len());
        prop_assert_eq!(m.pairs.1.len(), m.buffers.1.len());
        prop_assert!(m.edge_letters.len() == 2 * m.pairs.0.len()
            || m.edge_letters.len() == 2 * m.pairs.0.len() + 1);
        prop_assert!(m.corner_letters.len() == 2 * m.pairs.1.len()
            || m.corner_letters.len() == 2 * m.pairs.1.len() + 1);
    }
}
