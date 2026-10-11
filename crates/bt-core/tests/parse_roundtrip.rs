mod common;

use bt_core::{Alg, Cube};
use proptest::prelude::*;

#[test]
fn m2_prime_parses_as_m2() {
    assert_eq!(Alg::parse("M2'").unwrap(), Alg::parse("M2").unwrap());
    let mut folded = Cube::solved();
    folded.apply_alg(&Alg::parse("M2'").unwrap());
    let mut plain = Cube::solved();
    plain.apply_alg(&Alg::parse("M2").unwrap());
    assert_eq!(folded, plain);
}

#[test]
fn canonical_formatting() {
    assert_eq!(Alg::parse("U2'").unwrap().format(), "U2");
    assert_eq!(Alg::parse("2L2'").unwrap().format(), "2L2");
    assert_eq!(Alg::parse("x2'").unwrap().format(), "x2");
    assert_eq!(Alg::parse("r2'").unwrap().format(), "r2");
    assert_eq!(Alg::parse("").unwrap().format(), "");
    assert_eq!(Alg::parse("  R   U  ").unwrap().format(), "R U");
}

#[test]
fn rejects_unknown_tokens() {
    let bad = [
        "V", "U3", "Uu", "2u", "2", "2W", "2M", "2x", "m", "e", "s", "X", "U''", "U2x", "2UU",
        "2U''", "U5", "r3", "Mx", "x2''", "Ü", "U-", "R T", "U ok", "2U 2q",
    ];
    for token in bad {
        assert!(Alg::parse(token).is_err(), "{:?}", token);
    }
}

proptest! {
    #[test]
    fn format_parse_round_trip(alg_text in common::alg_text()) {
        let alg = Alg::parse(&alg_text).unwrap();
        let canonical = alg.format();
        prop_assert!(!canonical.contains("2'"), "canonical form {:?}", canonical);
        let reparsed = Alg::parse(&canonical).unwrap();
        prop_assert_eq!(reparsed, alg);
    }
}
