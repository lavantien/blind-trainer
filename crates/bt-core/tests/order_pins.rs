mod common;

use bt_core::{Alg, Cube};
use proptest::prelude::*;

fn order(alg: &Alg) -> u32 {
    let mut cube = Cube::solved();
    let mut n = 0;
    loop {
        cube.apply_alg(alg);
        n += 1;
        assert!(n < 2000, "order runaway for {:?}", alg.format());
        if cube.is_solved() {
            return n;
        }
    }
}

#[test]
fn sexy_move_has_order_6() {
    assert_eq!(order(&Alg::parse("R U R' U'").unwrap()), 6);
}

#[test]
fn r_u_has_order_105() {
    assert_eq!(order(&Alg::parse("R U").unwrap()), 105);
}

#[test]
fn slices_have_order_4() {
    for token in ["M", "E", "S"] {
        assert_eq!(order(&Alg::parse(token).unwrap()), 4, "{}", token);
    }
}

#[test]
fn rotations_have_order_4() {
    for token in ["x", "y", "z"] {
        assert_eq!(order(&Alg::parse(token).unwrap()), 4, "{}", token);
    }
}

#[test]
fn centers_stay_fixed_under_all_vendor_algs() {
    let rows = common::vendor_rows();
    assert_eq!(rows.len(), 1077);
    for (rel, text) in &rows {
        let alg = Alg::parse(text).unwrap();
        let mut cube = Cube::solved();
        cube.apply_alg(&alg);
        assert!(cube.centers_fixed(), "{} alg {:?}", rel, text);
    }
}

proptest! {
    #[test]
    fn inverse_law(alg_text in common::alg_text()) {
        let alg = Alg::parse(&alg_text).unwrap();
        let mut cube = Cube::solved();
        cube.apply_alg(&alg);
        cube.apply_alg(&alg.inverse());
        prop_assert!(cube.is_solved());
    }
}
