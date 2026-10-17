use crate::cube::Face;
use crate::moves::{Axis, Move, Slice};
use std::fmt;

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Alg {
    moves: Vec<Move>,
}

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

impl Alg {
    pub fn new(moves: Vec<Move>) -> Alg {
        Alg { moves }
    }

    pub fn parse(text: &str) -> Result<Alg, ParseError> {
        let mut moves = Vec::new();
        for token in text.split_whitespace() {
            moves.push(parse_token(token)?);
        }
        Ok(Alg { moves })
    }

    pub fn format(&self) -> String {
        self.moves
            .iter()
            .map(format_move)
            .collect::<Vec<String>>()
            .join(" ")
    }

    pub fn inverse(&self) -> Alg {
        Alg {
            moves: self.moves.iter().map(|mv| mv.invert()).rev().collect(),
        }
    }

    pub fn qtm(&self) -> u32 {
        self.moves
            .iter()
            .map(|mv| match mv {
                Move::Rotation { .. } => 0,
                _ if mv.quarter() == 2 => 2,
                _ => 1,
            })
            .sum()
    }

    pub fn htm(&self) -> u32 {
        self.moves
            .iter()
            .filter(|mv| !matches!(mv, Move::Rotation { .. }))
            .count() as u32
    }

    pub fn moves(&self) -> &[Move] {
        &self.moves
    }

    pub fn len(&self) -> usize {
        self.moves.len()
    }

    pub fn is_empty(&self) -> bool {
        self.moves.is_empty()
    }
}

pub(crate) fn parse_token(token: &str) -> Result<Move, ParseError> {
    let chars: Vec<char> = token.chars().collect();
    let mut idx = 0;
    let inner = if chars.first() == Some(&'2') {
        idx = 1;
        true
    } else {
        false
    };
    let base = match chars.get(idx) {
        Some(&ch) => ch,
        None => return Err(ParseError::UnknownToken(token.to_string())),
    };
    idx += 1;
    let quarter = match chars.get(idx..) {
        Some([]) => 1,
        Some(['2']) => 2,
        Some(['\'']) => 3,
        Some(['2', '\'']) => 2,
        _ => return Err(ParseError::UnknownToken(token.to_string())),
    };
    match base {
        'U' | 'D' | 'L' | 'R' | 'F' | 'B' => {
            let face = face_from_char(base).expect("narrowed face char");
            if inner {
                Ok(Move::Inner { face, quarter })
            } else {
                Ok(Move::Face { face, quarter })
            }
        }
        'u' | 'd' | 'l' | 'r' | 'f' | 'b' => {
            if inner {
                return Err(ParseError::UnknownToken(token.to_string()));
            }
            let face = face_from_char(base.to_ascii_uppercase()).expect("narrowed face char");
            Ok(Move::Wide { face, quarter })
        }
        'M' | 'E' | 'S' => {
            if inner {
                return Err(ParseError::UnknownToken(token.to_string()));
            }
            let slice = match base {
                'M' => Slice::M,
                'E' => Slice::E,
                _ => Slice::S,
            };
            Ok(Move::Slice { slice, quarter })
        }
        'x' | 'y' | 'z' => {
            if inner {
                return Err(ParseError::UnknownToken(token.to_string()));
            }
            let axis = match base {
                'x' => Axis::X,
                'y' => Axis::Y,
                _ => Axis::Z,
            };
            Ok(Move::Rotation { axis, quarter })
        }
        _ => Err(ParseError::UnknownToken(token.to_string())),
    }
}

fn format_move(mv: &Move) -> String {
    let prefix = match *mv {
        Move::Face { face, .. } => face_char(face).to_string(),
        Move::Wide { face, .. } => face_char(face).to_ascii_lowercase().to_string(),
        Move::Inner { face, .. } => format!("2{}", face_char(face)),
        Move::Slice { slice, .. } => match slice {
            Slice::M => "M".to_string(),
            Slice::E => "E".to_string(),
            Slice::S => "S".to_string(),
        },
        Move::Rotation { axis, .. } => match axis {
            Axis::X => "x".to_string(),
            Axis::Y => "y".to_string(),
            Axis::Z => "z".to_string(),
        },
    };
    let quarter = mv.quarter();
    let suffix = match quarter {
        2 => "2",
        3 => "'",
        _ => "",
    };
    format!("{}{}", prefix, suffix)
}

fn face_from_char(ch: char) -> Option<Face> {
    match ch {
        'U' => Some(Face::U),
        'L' => Some(Face::L),
        'F' => Some(Face::F),
        'R' => Some(Face::R),
        'B' => Some(Face::B),
        'D' => Some(Face::D),
        _ => None,
    }
}

fn face_char(face: Face) -> char {
    match face {
        Face::U => 'U',
        Face::L => 'L',
        Face::F => 'F',
        Face::R => 'R',
        Face::B => 'B',
        Face::D => 'D',
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_every_token_kind() {
        let alg = Alg::parse("U D L R F B u d l r f b 2U 2D 2L 2R 2F 2B M E S x y z").unwrap();
        assert_eq!(alg.len(), 24);
        assert_eq!(alg.htm(), 21);
        assert_eq!(alg.moves().len(), 24);
    }

    #[test]
    fn parses_and_canonicalizes_suffixes() {
        let alg =
            Alg::parse("U U' U2 U2' r r' r2 r2' 2F 2F' 2F2 2F2' M M' M2 M2' x x' x2 x2'").unwrap();
        assert_eq!(alg.len(), 20);
        assert_eq!(
            alg.format(),
            "U U' U2 U2 r r' r2 r2 2F 2F' 2F2 2F2 M M' M2 M2 x x' x2 x2"
        );
    }

    #[test]
    fn empty_and_whitespace_parse_to_empty() {
        for text in ["", " ", "   "] {
            let alg = Alg::parse(text).unwrap();
            assert!(alg.is_empty());
            assert_eq!(alg.len(), 0);
            assert_eq!(alg.format(), "");
            assert_eq!(alg.qtm(), 0);
            assert_eq!(alg.htm(), 0);
        }
    }

    #[test]
    fn metrics() {
        assert_eq!(Alg::parse("U").unwrap().qtm(), 1);
        assert_eq!(Alg::parse("U2").unwrap().qtm(), 2);
        assert_eq!(Alg::parse("U'").unwrap().qtm(), 1);
        assert_eq!(Alg::parse("x").unwrap().qtm(), 0);
        assert_eq!(Alg::parse("x2").unwrap().qtm(), 0);
        assert_eq!(Alg::parse("R U R' U'").unwrap().qtm(), 4);
        assert_eq!(Alg::parse("R U R' U'").unwrap().htm(), 4);
        assert_eq!(Alg::parse("U M2 U M' U2 M U M2 U'").unwrap().qtm(), 12);
        assert_eq!(Alg::parse("U M2 U M' U2 M U M2 U'").unwrap().htm(), 9);
        assert_eq!(Alg::parse("x y2 z' U").unwrap().qtm(), 1);
        assert_eq!(Alg::parse("x y2 z' U").unwrap().htm(), 1);
    }

    #[test]
    fn inverse_format() {
        let alg = Alg::parse("R U r' 2F M2 x").unwrap();
        assert_eq!(alg.inverse().format(), "x' M2 2F' r U' R'");
        let empty = Alg::parse("").unwrap();
        assert_eq!(empty.inverse().format(), "");
    }

    #[test]
    fn rejects_garbage_tokens() {
        let bad = [
            "V", "U3", "Uu", "2u", "2", "2W", "2M", "2x", "m", "e", "s", "X", "U''", "U2x", "2UU",
            "2U''", "U5", "r3", "Mx", "x2''", "U-", "R T", "U ok", "2U 2q",
        ];
        for text in bad {
            assert!(Alg::parse(text).is_err(), "{:?}", text);
        }
    }

    #[test]
    fn parse_error_displays_the_token() {
        let err = Alg::parse("U wut").unwrap_err();
        assert_eq!(err, ParseError::UnknownToken("wut".to_string()));
        assert_eq!(err.to_string(), "unknown token: wut");
    }
}
