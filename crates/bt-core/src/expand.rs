use crate::alg::Alg;

pub fn expand(text: &str) -> Option<Alg> {
    Alg::parse(text).ok()
}

pub fn cancel(alg: &Alg) -> Alg {
    alg.clone()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::cube::Cube;

    fn fmt(text: &str) -> String {
        expand(text).expect("expansible text").format()
    }

    fn state_of(alg: &Alg) -> Cube {
        let mut cube = Cube::solved();
        cube.apply_alg(alg);
        cube
    }

    fn state_of_text(text: &str) -> Cube {
        state_of(&expand(text).expect("expansible text"))
    }

    fn repeated_ref(text: &str, count: usize) -> Cube {
        let alg = Alg::parse(text).expect("flat reference parses");
        let mut cube = Cube::solved();
        for _ in 0..count {
            cube.apply_alg(&alg);
        }
        cube
    }

    #[test]
    fn cancellation_drops_inverse_pairs() {
        for text in ["R R'", "R' R", "U U'", "x x'", "M2 M'", "R U U' R'"] {
            assert_eq!(cancel(&Alg::parse(text).unwrap()).format(), "", "{text}");
        }
    }

    #[test]
    fn cancellation_merges_same_base_quarters() {
        let cases = [
            ("R R", "R2"),
            ("R' R'", "R2"),
            ("R2 R", "R'"),
            ("R2 R'", "R"),
            ("R R2 R'", "R2"),
            ("U2 U", "U'"),
            ("U2 U'", "U"),
            ("x2 x", "x'"),
            ("M M M", "M'"),
        ];
        for (text, want) in cases {
            assert_eq!(cancel(&Alg::parse(text).unwrap()).format(), want, "{text}");
        }
    }

    #[test]
    fn cancellation_never_crosses_other_moves_or_move_kinds() {
        let fixed = [
            "R U R U",
            "R U R' U'",
            "R r",
            "r R",
            "R 2R",
            "M 2L",
            "U u",
            "R U2 R' U' R U R' U'",
        ];
        for text in fixed {
            assert_eq!(cancel(&Alg::parse(text).unwrap()).format(), text, "{text}");
        }
    }

    #[test]
    fn cancellation_preserves_cube_state() {
        let cases = [
            "R U U' R'",
            "R2 R' U U2",
            "x x' R R R",
            "M2 M U U' U",
            "R U R' U' R R' R2",
        ];
        for text in cases {
            let original = Alg::parse(text).unwrap();
            assert_eq!(state_of(&cancel(&original)), state_of(&original), "{text}");
        }
    }

    #[test]
    fn cancellation_is_idempotent() {
        for text in ["R U U' R'", "R2 R' U U2", "R R2 R'", "M M M", "x2 x x'"] {
            let once = cancel(&Alg::parse(text).unwrap());
            assert_eq!(cancel(&once).format(), once.format(), "{text}");
        }
    }

    #[test]
    fn flat_algs_pass_through_unchanged() {
        for text in [
            "R U R' U'",
            "U M2 U M' U2 M U M2 U'",
            "2L 2R' S2 y'",
            "M' U2 M U R' F' R S R' F R S' U",
        ] {
            assert_eq!(fmt(text), text, "{text}");
            assert_eq!(state_of_text(text), state_of(&Alg::parse(text).unwrap()));
        }
    }

    #[test]
    fn the_pinned_repeat_example_cancels_to_a_half_turn() {
        assert_eq!(fmt("(R U R')2"), "R U2 R'");
        assert_eq!(state_of_text("(R U R')2"), repeated_ref("R U R'", 2));
        assert_eq!(fmt("(R U R' )2"), "R U2 R'");
        assert_eq!(fmt("( R U R')2"), "R U2 R'");
    }

    #[test]
    fn repeat_counts_fold_into_quarter_arithmetic() {
        assert_eq!(fmt("(R)2"), "R2");
        assert_eq!(fmt("(R)3"), "R'");
        assert_eq!(fmt("(R)5"), "R");
        assert_eq!(fmt("(R)99"), "R'");
        assert_eq!(state_of_text("(R)99"), repeated_ref("R", 99));
        assert!(expand("(R)4").is_none(), "identity collapse shows no line");
        assert!(expand("(R2)2").is_none(), "half turn pair collapses too");
    }

    #[test]
    fn nested_and_bare_groups_unfold() {
        assert_eq!(fmt("((R U)2)2"), "R U R U R U R U");
        assert_eq!(state_of_text("((R U)2)2"), repeated_ref("R U R U", 2));
        assert_eq!(fmt("(R U R')"), "R U R'");
        assert_eq!(fmt("(L' U) (R U') (L U2')"), "L' U R U' L U2");
        assert_eq!(fmt("[R]"), "R");
        assert_eq!(fmt("[[R,U]+[F,D]]"), "R U R' U' F D F' D'");
    }

    #[test]
    fn commutator_spans_decompose_in_written_order() {
        assert_eq!(fmt("[R,U]"), "R U R' U'");
        assert_eq!(fmt("[L E2 L2 E L,U]"), "L E2 L2 E L U L' E' L2 E2 L' U'");
        assert_eq!(fmt("[[R,U],S]"), "R U R' U' S U R U' R' S'");
        assert_eq!(fmt("[U':[R,U],S]"), "U' R U R' S R U' R' U S'");
    }

    #[test]
    fn setups_wrap_and_sums_concatenate() {
        assert_eq!(fmt("R2 U':[S,R2]"), "R2 U' S R2 S' R2 U R2");
        assert_eq!(fmt("U M2 U:[M',U2]"), "U M2 U M' U2 M U M2 U'");
        assert_eq!(fmt("[R,U]+[F,D]"), "R U R' U' F D F' D'");
        assert_eq!(
            fmt("[M',U2]+U':[R' F' R,S]"),
            "M' U2 M U R' F' R S R' F R S' U"
        );
        assert_eq!(
            fmt("U:[[R' E' R,U2]+U2:[S,R2]]"),
            "U R' E' R U2 R' E R S R2 S' R2 U"
        );
    }

    #[test]
    fn repeats_containing_commutators_cancel_across_copies() {
        assert_eq!(fmt("([R,U])2"), "R U R' U' R U R' U'");
        assert_eq!(state_of_text("([R,U])2"), repeated_ref("R U R' U'", 2));
        assert_eq!(fmt("(U':[R,U])2"), "U' R U2 R' U");
    }

    #[test]
    fn vendor_forms_stay_state_true_against_the_reference_cube() {
        let cases = [
            ("[R,U]", "R U R' U'"),
            ("R2 U':[S,R2]", "R2 U' S R2 S' R2 U R2"),
            ("[M',U2]+U':[R' F' R,S]", "M' U2 M U R' F' R S R' F R S' U"),
            ("[L E2 L2 E L,U]", "L E2 L2 E L U L' E' L2 E2 L' U'"),
            (
                "U:[[R' E' R,U2]+U2:[S,R2]]",
                "U R' E' R U2 R' E R S R2 S' R2 U",
            ),
            ("[[R,U],S]", "R U R' U' S U R U' R' S'"),
            ("[U':[R,U],S]", "U' R U R' U' U S U' U R U' R' U S'"),
            ("U M2 U:[M',U2]", "U M2 U M' U2 M U M2 U'"),
        ];
        for (text, reference) in cases {
            let want = state_of(&Alg::parse(reference).unwrap());
            assert_eq!(state_of_text(text), want, "{text}");
        }
    }

    #[test]
    fn expansion_reaches_a_fixed_point() {
        let texts = [
            "(R U R')2",
            "(R)99",
            "((R U)2)2",
            "[R,U]",
            "R2 U':[S,R2]",
            "[M',U2]+U':[R' F' R,S]",
            "U:[[R' E' R,U2]+U2:[S,R2]]",
            "[[R,U],S]",
            "[U':[R,U],S]",
            "(U':[R,U])2",
            "U M2 U:[M',U2]",
        ];
        for text in texts {
            let once = expand(text).expect("expansible text");
            let twice = expand(&once.format()).expect("expanded form reparses");
            assert_eq!(twice, once, "{text}");
            assert_eq!(expand(&twice.format()).unwrap(), twice, "{text}");
        }
    }

    #[test]
    fn expansion_is_deterministic() {
        for text in ["(R U R')2", "[M',U2]+U':[R' F' R,S]", "(U':[R,U])2"] {
            assert_eq!(expand(text), expand(text), "{text}");
        }
    }

    #[test]
    fn unparsable_and_non_alg_input_returns_none() {
        let bad = [
            "",
            " ",
            "   ",
            "Not found.",
            " Not found. ",
            "junk text",
            "N/A",
            "-",
            "R Q",
            "U wut",
            "R T",
            "Rw U",
            "U-",
        ];
        for text in bad {
            assert!(expand(text).is_none(), "{text:?}");
        }
    }

    #[test]
    fn malformed_structure_returns_none() {
        let bad = [
            "(R",
            "R)",
            "((R)",
            "[R,U",
            "[[R,U]",
            "[R,U]]",
            "[,]",
            "[R,]",
            "[,R]",
            "[:R,U]",
            ":[R,U]",
            "U::[R,U]",
            "U:[R,U]]",
            "[R,U,U]",
            "[R,U,,F]",
            "()",
            "( )",
            "(R, U)",
            "(R)0",
            "(R)100",
            "(R)99999999999999999999",
            "2[R]",
            "R 2",
            "U:[[R,U]]]",
            "((((((R U R')2)2)2)2)2)2",
        ];
        for text in bad {
            assert!(expand(text).is_none(), "{text:?}");
        }
    }

    #[test]
    fn unbalanced_and_runaway_inputs_return_none() {
        assert!(expand("((((R U R')2)2)2)2").is_some());
        assert!(expand("(((((R U R')2)2)2)2)2").is_none());
        let wide = format!("(R U R' F){}", 99);
        assert!(expand(&wide).is_some());
        let runaway = "((R U R' F B L D S E M2 U')99)99";
        assert!(expand(runaway).is_none(), "projected length must cap");
    }
}
