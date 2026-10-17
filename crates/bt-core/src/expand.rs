use crate::alg::{Alg, parse_token};
use crate::moves::Move;

const REPEAT_LIMIT: usize = 99;
const EXPAND_LIMIT: usize = 100_000;
const MAX_DEPTH: usize = 4;

pub fn expand(text: &str) -> Option<Alg> {
    let tokens = tokenize(text)?;
    let mut parser = Parser { tokens, index: 0 };
    let moves = parser.sum(0, Stop::End)?;
    if moves.len() > EXPAND_LIMIT {
        return None;
    }
    let cancelled = cancel(&Alg::new(moves));
    (!cancelled.is_empty()).then_some(cancelled)
}

pub fn cancel(alg: &Alg) -> Alg {
    let mut out: Vec<Move> = Vec::with_capacity(alg.len());
    for mv in alg.moves() {
        match out.last_mut() {
            Some(top) if same_base(*top, *mv) => {
                let quarter = (top.quarter() + mv.quarter()) % 4;
                if quarter == 0 {
                    out.pop();
                } else {
                    *top = with_quarter(*top, quarter);
                }
            }
            _ => out.push(*mv),
        }
    }
    Alg::new(out)
}

fn same_base(a: Move, b: Move) -> bool {
    with_quarter(a, 1) == with_quarter(b, 1)
}

fn with_quarter(mv: Move, quarter: u8) -> Move {
    match mv {
        Move::Face { face, .. } => Move::Face { face, quarter },
        Move::Wide { face, .. } => Move::Wide { face, quarter },
        Move::Slice { slice, .. } => Move::Slice { slice, quarter },
        Move::Inner { face, .. } => Move::Inner { face, quarter },
        Move::Rotation { axis, .. } => Move::Rotation { axis, quarter },
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Tok {
    Mv(Move),
    LParen,
    RParen,
    LBracket,
    RBracket,
    Comma,
    Colon,
    Plus,
    Count(usize),
}

fn tokenize(text: &str) -> Option<Vec<Tok>> {
    let chars: Vec<char> = text.chars().collect();
    let mut tokens = Vec::new();
    let mut word = String::new();
    let mut index = 0;
    while index < chars.len() {
        let ch = chars[index];
        match ch {
            ' ' | '\t' | '\n' | '\r' => {
                flush_word(&mut tokens, &mut word)?;
                index += 1;
            }
            '[' | ']' | '(' | ')' | ',' | ':' | '+' => {
                flush_word(&mut tokens, &mut word)?;
                tokens.push(match ch {
                    '[' => Tok::LBracket,
                    ']' => Tok::RBracket,
                    '(' => Tok::LParen,
                    ')' => Tok::RParen,
                    ',' => Tok::Comma,
                    ':' => Tok::Colon,
                    _ => Tok::Plus,
                });
                index += 1;
            }
            '0'..='9' if word.is_empty() && tokens.last() == Some(&Tok::RParen) => {
                let start = index;
                while index < chars.len() && chars[index].is_ascii_digit() {
                    index += 1;
                }
                let digits: String = chars[start..index].iter().collect();
                tokens.push(Tok::Count(digits.parse::<usize>().ok()?));
            }
            _ => {
                word.push(ch);
                index += 1;
            }
        }
    }
    flush_word(&mut tokens, &mut word)?;
    Some(tokens)
}

fn flush_word(tokens: &mut Vec<Tok>, word: &mut String) -> Option<()> {
    if word.is_empty() {
        return Some(());
    }
    tokens.push(Tok::Mv(parse_token(word).ok()?));
    word.clear();
    Some(())
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum Stop {
    End,
    Paren,
    Bracket,
}

struct Parser {
    tokens: Vec<Tok>,
    index: usize,
}

impl Parser {
    fn peek(&self) -> Option<&Tok> {
        self.tokens.get(self.index)
    }

    fn sum(&mut self, depth: usize, stop: Stop) -> Option<Vec<Move>> {
        let mut moves = self.unit(depth, stop)?;
        while self.peek() == Some(&Tok::Plus) {
            self.index += 1;
            moves.extend(self.unit(depth, stop)?);
        }
        Some(moves)
    }

    fn unit(&mut self, depth: usize, stop: Stop) -> Option<Vec<Move>> {
        let setup = self.seq(depth, stop)?;
        if self.peek() == Some(&Tok::Colon) {
            self.index += 1;
            let core = self.unit(depth, stop)?;
            let mut moves = setup.clone();
            moves.extend(core);
            moves.extend(Alg::new(setup).inverse().moves());
            return Some(moves);
        }
        Some(setup)
    }

    fn seq(&mut self, depth: usize, stop: Stop) -> Option<Vec<Move>> {
        let mut moves = Vec::new();
        loop {
            match self.peek() {
                None | Some(Tok::Plus) | Some(Tok::Colon) => break,
                Some(Tok::Comma) | Some(Tok::RBracket) if stop == Stop::Bracket => break,
                Some(Tok::RParen) if stop == Stop::Paren => break,
                Some(Tok::Comma) | Some(Tok::RBracket) | Some(Tok::RParen)
                | Some(Tok::Count(_)) => {
                    return None;
                }
                Some(Tok::LParen) => {
                    if depth >= MAX_DEPTH {
                        return None;
                    }
                    self.index += 1;
                    let inner = self.sum(depth + 1, Stop::Paren)?;
                    if self.peek() != Some(&Tok::RParen) {
                        return None;
                    }
                    self.index += 1;
                    let repeat = match self.peek() {
                        Some(Tok::Count(count)) => {
                            let count = *count;
                            self.index += 1;
                            count
                        }
                        _ => 1,
                    };
                    if repeat == 0 || repeat > REPEAT_LIMIT {
                        return None;
                    }
                    if inner.len().saturating_mul(repeat) > EXPAND_LIMIT {
                        return None;
                    }
                    for _ in 0..repeat {
                        moves.extend(inner.iter().copied());
                    }
                }
                Some(Tok::LBracket) => {
                    if depth >= MAX_DEPTH {
                        return None;
                    }
                    self.index += 1;
                    let first = self.sum(depth + 1, Stop::Bracket)?;
                    match self.peek() {
                        Some(Tok::Comma) => {
                            self.index += 1;
                            let second = self.sum(depth + 1, Stop::Bracket)?;
                            if self.peek() != Some(&Tok::RBracket) {
                                return None;
                            }
                            self.index += 1;
                            let mut comm = first.clone();
                            comm.extend(second.iter().copied());
                            comm.extend(Alg::new(first).inverse().moves());
                            comm.extend(Alg::new(second).inverse().moves());
                            moves.extend(comm);
                        }
                        Some(Tok::RBracket) => {
                            self.index += 1;
                            moves.extend(first);
                        }
                        _ => return None,
                    }
                }
                Some(Tok::Mv(mv)) => {
                    moves.push(*mv);
                    self.index += 1;
                }
            }
        }
        (!moves.is_empty()).then_some(moves)
    }
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
        for text in ["R R'", "R' R", "U U'", "x x'", "R U U' R'"] {
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
        assert_eq!(fmt("(U':[R,U])2"), "U' R U R' U' R U R'");
        assert_eq!(
            state_of_text("(U':[R,U])2"),
            repeated_ref("U' R U R' U' U", 2)
        );
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
        assert!(expand("((((R U)2)2)2)2").is_some());
        assert!(expand("(((((R U)2)2)2)2)2").is_none());
        assert!(
            expand("((((R U R')2)2)2)2").is_none(),
            "involution nesting collapses"
        );
        let wide = format!("(R U R' F){}", 99);
        assert!(expand(&wide).is_some());
        let runaway = "((R U R' F B L D S E M2 U')99)99";
        assert!(expand(runaway).is_none(), "projected length must cap");
        assert!(
            expand("(R)101").is_none(),
            "count past the cap must not expand"
        );
        assert!(
            expand("(R U)100").is_none(),
            "projected length past the cap must not expand"
        );
    }

    #[test]
    fn the_move_cap_is_exclusive_at_both_levels() {
        let exact: String = vec!["R U"; 50_000].join(" ");
        assert_eq!(Alg::parse(&exact).unwrap().len(), 100_000);
        assert!(expand(&exact).is_some(), "exactly the cap still expands");
        let over = format!("{exact} U U");
        assert!(
            expand(&over).is_none(),
            "two moves past the cap is rejected"
        );
        let unit: String = ["R U F D L B"; 4].join(" ");
        let unit = format!("{unit} R");
        assert_eq!(Alg::parse(&unit).unwrap().len(), 25);
        let mid = format!("({unit})50");
        assert!(expand(&mid).is_some());
        let projected = format!("({mid})80");
        assert!(
            expand(&projected).is_some(),
            "projected exactly the cap expands"
        );
        let projected_over = format!("({mid})81");
        assert!(expand(&projected_over).is_none());
    }

    #[test]
    fn bracket_nesting_is_bounded_like_parens() {
        assert!(expand("[[[[R,U],S],D],F]").is_some(), "four levels parse");
        assert!(
            expand("[[[[[R,U],S],D],F],E]").is_none(),
            "five levels are rejected"
        );
        assert!(
            expand("[R,[R,[R,[R,U]]]]").is_some(),
            "four right nested levels parse"
        );
        assert!(
            expand("[R,[R,[R,[R,[R,U]]]]]").is_none(),
            "five right nested levels are rejected"
        );
    }

    #[test]
    fn stray_separators_at_top_level_return_none() {
        for text in ["R,U", "R)", "R]", "(R)2,"] {
            assert!(expand(text).is_none(), "{text:?}");
        }
    }
}
