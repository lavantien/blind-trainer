use std::fmt;

#[derive(Clone, PartialEq, Eq, Debug)]
pub enum ParseError {
    UnknownToken(String),
}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ParseError::UnknownToken(token) => write!(f, "unknown token: {}", token),
        }
    }
}

impl std::error::Error for ParseError {}

#[derive(Clone, PartialEq, Eq, Debug)]
pub enum ApplyError {
    IllegalMove(String),
}

impl fmt::Display for ApplyError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ApplyError::IllegalMove(token) => write!(f, "illegal move: {}", token),
        }
    }
}

impl std::error::Error for ApplyError {}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct FaceletFace {
    pub rows: Vec<Vec<u8>>,
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct FaceletGrid {
    pub faces: Vec<FaceletFace>,
}

pub trait Geometry {
    type Move: Copy + Eq;
    type Alg: Clone;
    type State;

    fn parse_move(&self, token: &str) -> Result<Self::Move, ParseError>;
    fn parse_alg(&self, text: &str) -> Result<Self::Alg, ParseError>;
    fn format_alg(&self, alg: &Self::Alg) -> String;
    fn invert_alg(&self, alg: &Self::Alg) -> Self::Alg;
    fn apply_alg(&self, state: &mut Self::State, alg: &Self::Alg) -> Result<(), ApplyError>;
    fn solved_state(&self) -> Self::State;
    fn is_solved(&self, state: &Self::State) -> bool;
    fn facelets(&self, state: &Self::State) -> FaceletGrid;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_error_displays_the_token() {
        let err = ParseError::UnknownToken("wut".to_string());
        assert_eq!(err.to_string(), "unknown token: wut");
    }

    #[test]
    fn apply_error_displays_the_move() {
        let err = ApplyError::IllegalMove("/".to_string());
        assert_eq!(err.to_string(), "illegal move: /");
    }
}
