use crate::geometry::{ApplyError, Geometry, ParseError};
use std::fmt;

#[derive(Clone, PartialEq, Eq, Debug)]
pub enum Tier1Error {
    Parse(ParseError),
    Apply(ApplyError),
    ForwardRoundTrip,
    ReverseRoundTrip,
}

impl fmt::Display for Tier1Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Tier1Error::Parse(err) => write!(f, "tier 1 parse failed: {}", err),
            Tier1Error::Apply(err) => write!(f, "tier 1 apply failed: {}", err),
            Tier1Error::ForwardRoundTrip => write!(f, "tier 1 round trip failed: alg then inverse"),
            Tier1Error::ReverseRoundTrip => write!(f, "tier 1 round trip failed: inverse then alg"),
        }
    }
}

impl std::error::Error for Tier1Error {}

pub fn verify_tier1<G: Geometry>(geometry: &G, text: &str) -> Result<(), Tier1Error> {
    let alg = geometry.parse_alg(text).map_err(Tier1Error::Parse)?;
    let inverse = geometry.invert_alg(&alg);
    let mut forward = geometry.solved_state();
    geometry
        .apply_alg(&mut forward, &alg)
        .map_err(Tier1Error::Apply)?;
    geometry
        .apply_alg(&mut forward, &inverse)
        .map_err(Tier1Error::Apply)?;
    if !geometry.is_solved(&forward) {
        return Err(Tier1Error::ForwardRoundTrip);
    }
    let mut reverse = geometry.solved_state();
    geometry
        .apply_alg(&mut reverse, &inverse)
        .map_err(Tier1Error::Apply)?;
    geometry
        .apply_alg(&mut reverse, &alg)
        .map_err(Tier1Error::Apply)?;
    if !geometry.is_solved(&reverse) {
        return Err(Tier1Error::ReverseRoundTrip);
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::geometry::{FaceletFace, FaceletGrid};

    const OFF_GRID: i32 = 7;

    #[derive(Clone, Copy, PartialEq, Eq, Debug)]
    enum CycleMove {
        Clock(u8),
        Counter(u8),
        Slash,
    }

    #[derive(Clone, PartialEq, Eq, Debug)]
    struct CycleAlg {
        moves: Vec<CycleMove>,
    }

    #[derive(Clone, Copy, PartialEq, Eq, Debug)]
    struct CycleState {
        turn: i32,
    }

    #[derive(Clone, Copy, PartialEq, Eq, Debug)]
    struct Cycle {
        identity_invert: bool,
        counter_shifts_forward: bool,
        slash_needs_solved: bool,
        counter_lands_off: bool,
    }

    impl Cycle {
        fn sound() -> Cycle {
            Cycle {
                identity_invert: false,
                counter_shifts_forward: false,
                slash_needs_solved: false,
                counter_lands_off: false,
            }
        }
    }

    fn quarter_suffix(base: char, quarter: u8) -> String {
        match quarter {
            2 => format!("{base}2"),
            3 => format!("{base}'"),
            _ => base.to_string(),
        }
    }

    impl Geometry for Cycle {
        type Move = CycleMove;
        type Alg = CycleAlg;
        type State = CycleState;

        fn parse_move(&self, token: &str) -> Result<CycleMove, ParseError> {
            let mut chars = token.chars();
            let Some(base) = chars.next() else {
                return Err(ParseError::UnknownToken(token.to_string()));
            };
            if base == 's' && chars.next().is_none() {
                return Ok(CycleMove::Slash);
            }
            let quarter = match chars.next() {
                None => 1,
                Some('2') => 2,
                Some('\'') => 3,
                Some(_) => return Err(ParseError::UnknownToken(token.to_string())),
            };
            if chars.next().is_some() || (base != 'a' && base != 'b') {
                return Err(ParseError::UnknownToken(token.to_string()));
            }
            Ok(if base == 'a' {
                CycleMove::Clock(quarter)
            } else {
                CycleMove::Counter(quarter)
            })
        }

        fn parse_alg(&self, text: &str) -> Result<CycleAlg, ParseError> {
            let mut moves = Vec::new();
            for token in text.split_whitespace() {
                moves.push(self.parse_move(token)?);
            }
            Ok(CycleAlg { moves })
        }

        fn format_alg(&self, alg: &CycleAlg) -> String {
            alg.moves
                .iter()
                .map(|mv| match *mv {
                    CycleMove::Clock(quarter) => quarter_suffix('a', quarter),
                    CycleMove::Counter(quarter) => quarter_suffix('b', quarter),
                    CycleMove::Slash => "s".to_string(),
                })
                .collect::<Vec<String>>()
                .join(" ")
        }

        fn invert_alg(&self, alg: &CycleAlg) -> CycleAlg {
            if self.identity_invert {
                return alg.clone();
            }
            CycleAlg {
                moves: alg
                    .moves
                    .iter()
                    .rev()
                    .map(|mv| match *mv {
                        CycleMove::Clock(quarter) => CycleMove::Counter(quarter),
                        CycleMove::Counter(quarter) => CycleMove::Clock(quarter),
                        CycleMove::Slash => CycleMove::Slash,
                    })
                    .collect(),
            }
        }

        fn apply_alg(&self, state: &mut CycleState, alg: &CycleAlg) -> Result<(), ApplyError> {
            for mv in &alg.moves {
                match *mv {
                    CycleMove::Clock(quarter) => state.turn += quarter as i32,
                    CycleMove::Counter(quarter) => {
                        if self.counter_lands_off && state.turn == 0 {
                            state.turn = OFF_GRID;
                        } else if self.counter_shifts_forward {
                            state.turn += quarter as i32;
                        } else {
                            state.turn -= quarter as i32;
                        }
                    }
                    CycleMove::Slash => {
                        if self.slash_needs_solved && state.turn != 0 {
                            return Err(ApplyError::IllegalMove("s".to_string()));
                        }
                    }
                }
            }
            Ok(())
        }

        fn solved_state(&self) -> CycleState {
            CycleState { turn: 0 }
        }

        fn is_solved(&self, state: &CycleState) -> bool {
            state.turn == 0
        }

        fn facelets(&self, state: &CycleState) -> FaceletGrid {
            FaceletGrid {
                faces: vec![FaceletFace {
                    rows: vec![vec![state.turn as u8]],
                }],
            }
        }
    }

    #[test]
    fn accepts_valid_algs() {
        let cycle = Cycle::sound();
        for text in [
            "",
            "a",
            "b",
            "s",
            "a b",
            "a a a a",
            "a2 b' a2",
            "  a   b2  ",
            "a s b",
        ] {
            assert_eq!(verify_tier1(&cycle, text), Ok(()), "{}", text);
        }
    }

    #[test]
    fn rejects_garbage_tokens() {
        let cycle = Cycle::sound();
        for text in ["c", "a3", "a''", "2a", "s2", "A", "a z b", "-"] {
            assert!(
                matches!(verify_tier1(&cycle, text), Err(Tier1Error::Parse(_))),
                "{}",
                text
            );
        }
    }

    #[test]
    fn parse_failure_carries_the_offending_token() {
        let err = verify_tier1(&Cycle::sound(), "a z b").unwrap_err();
        assert_eq!(
            err,
            Tier1Error::Parse(ParseError::UnknownToken("z".to_string()))
        );
    }

    #[test]
    fn rejects_identity_inversion() {
        let cycle = Cycle {
            identity_invert: true,
            ..Cycle::sound()
        };
        assert_eq!(verify_tier1(&cycle, "a"), Err(Tier1Error::ForwardRoundTrip));
    }

    #[test]
    fn rejects_apply_sign_drift() {
        let cycle = Cycle {
            counter_shifts_forward: true,
            ..Cycle::sound()
        };
        assert_eq!(verify_tier1(&cycle, "a"), Err(Tier1Error::ForwardRoundTrip));
    }

    #[test]
    fn rejects_non_bijective_inverse_landing() {
        let cycle = Cycle {
            counter_lands_off: true,
            ..Cycle::sound()
        };
        assert_eq!(verify_tier1(&cycle, "a"), Err(Tier1Error::ReverseRoundTrip));
    }

    #[test]
    fn rejects_alg_legal_forward_but_illegal_reversed() {
        let cycle = Cycle {
            slash_needs_solved: true,
            ..Cycle::sound()
        };
        assert_eq!(verify_tier1(&cycle, "a b"), Ok(()));
        assert!(matches!(
            verify_tier1(&cycle, "a b s a"),
            Err(Tier1Error::Apply(ApplyError::IllegalMove(token))) if token == "s"
        ));
    }

    #[test]
    fn cycle_mock_moves_and_renders_honestly() {
        let cycle = Cycle::sound();
        let alg = cycle.parse_alg("a2 b' s").unwrap();
        assert_eq!(cycle.format_alg(&alg), "a2 b' s");
        assert_eq!(cycle.format_alg(&cycle.invert_alg(&alg)), "s a' b2");
        let mut state = cycle.solved_state();
        cycle
            .apply_alg(&mut state, &cycle.parse_alg("a").unwrap())
            .unwrap();
        assert!(!cycle.is_solved(&state));
        assert_eq!(cycle.facelets(&state).faces[0].rows[0][0], 1);
        assert_eq!(cycle.facelets(&cycle.solved_state()).faces[0].rows[0][0], 0);
    }

    #[test]
    fn tier1_error_displays_its_cause() {
        let err = verify_tier1(&Cycle::sound(), "q").unwrap_err();
        assert_eq!(err.to_string(), "tier 1 parse failed: unknown token: q");
        assert_eq!(
            Tier1Error::ForwardRoundTrip.to_string(),
            "tier 1 round trip failed: alg then inverse"
        );
        assert_eq!(
            Tier1Error::ReverseRoundTrip.to_string(),
            "tier 1 round trip failed: inverse then alg"
        );
    }
}
