#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Orbit {
    Edge,
    Corner,
}

impl Orbit {
    pub fn stickers(self) -> std::ops::Range<u8> {
        match self {
            Orbit::Corner => 0..24,
            Orbit::Edge => 24..48,
        }
    }
}

pub const EDGE_BUFFER_STICKER: u8 = 26;
pub const CORNER_BUFFER_STICKER: u8 = 2;

pub const EDGE_FLOATING: [&str; 12] = [
    "UF", "UB", "UR", "UL", "FR", "FL", "DF", "DB", "DR", "DL", "BR", "BL",
];

pub const CORNER_FLOATING: [&str; 8] = ["UFR", "UFL", "UBR", "UBL", "DFR", "DFL", "DBR", "DBL"];

const EDGE_NAMES: [&str; 24] = [
    "UB", "UR", "UF", "UL", "LU", "LF", "LD", "LB", "FU", "FR", "FD", "FL", "RU", "RB", "RD", "RF",
    "BU", "BL", "BD", "BR", "DF", "DR", "DB", "DL",
];

const CORNER_NAMES: [&str; 24] = [
    "UBL", "UBR", "UFR", "UFL", "LUB", "LUF", "LDF", "LDB", "FUL", "FUR", "FDR", "FDL", "RUF",
    "RUB", "RDB", "RDF", "BUR", "BUL", "BDL", "BDR", "DFL", "DFR", "DBR", "DBL",
];

pub fn letter_of(sticker: u8) -> char {
    assert!(sticker < 48, "sticker id out of range: {sticker}");
    (b'A' + sticker % 24) as char
}

pub fn sticker_of(orbit: Orbit, letter: char) -> u8 {
    assert!(
        ('A'..='X').contains(&letter),
        "letter out of range: {letter}"
    );
    match orbit {
        Orbit::Corner => letter as u8 - b'A',
        Orbit::Edge => 24 + (letter as u8 - b'A'),
    }
}

pub fn name_of(sticker: u8) -> &'static str {
    assert!(sticker < 48, "sticker id out of range: {sticker}");
    if sticker < 24 {
        CORNER_NAMES[sticker as usize]
    } else {
        EDGE_NAMES[(sticker - 24) as usize]
    }
}

fn face_key(name: &str) -> [u8; 3] {
    let mut key = [0u8; 3];
    for (i, &b) in name.as_bytes().iter().take(3).enumerate() {
        key[i] = b;
    }
    key.sort_unstable();
    key
}

pub fn same_piece(a: u8, b: u8) -> bool {
    assert!(a < 48 && b < 48, "sticker id out of range: {a} {b}");
    if (a < 24) != (b < 24) {
        return false;
    }
    face_key(name_of(a)) == face_key(name_of(b))
}

pub fn head_sticker(piece_name: &str) -> u8 {
    let orbit = match piece_name.len() {
        2 => Orbit::Edge,
        3 => Orbit::Corner,
        n => panic!("piece name must have 2 or 3 faces: {piece_name} ({n})"),
    };
    let target = face_key(piece_name);
    let members: Vec<u8> = orbit
        .stickers()
        .filter(|&s| face_key(name_of(s)) == target)
        .collect();
    assert!(!members.is_empty(), "unknown piece name: {piece_name}");
    let heads: Vec<u8> = members
        .iter()
        .copied()
        .filter(|&s| matches!(name_of(s).as_bytes()[0], b'U' | b'D'))
        .collect();
    let chosen = if !heads.is_empty() {
        heads
    } else {
        members
            .iter()
            .copied()
            .filter(|&s| matches!(name_of(s).as_bytes()[0], b'F' | b'B'))
            .collect()
    };
    assert_eq!(chosen.len(), 1, "head is ambiguous for {piece_name}");
    chosen[0]
}

pub fn edge_floating_heads() -> [u8; 12] {
    std::array::from_fn(|i| head_sticker(EDGE_FLOATING[i]))
}

pub fn corner_floating_heads() -> [u8; 8] {
    std::array::from_fn(|i| head_sticker(CORNER_FLOATING[i]))
}

#[cfg(test)]
mod tests {
    use super::*;

    const PLAN_EDGE: [(char, &str); 24] = [
        ('A', "UB"),
        ('B', "UR"),
        ('C', "UF"),
        ('D', "UL"),
        ('E', "LU"),
        ('F', "LF"),
        ('G', "LD"),
        ('H', "LB"),
        ('I', "FU"),
        ('J', "FR"),
        ('K', "FD"),
        ('L', "FL"),
        ('M', "RU"),
        ('N', "RB"),
        ('O', "RD"),
        ('P', "RF"),
        ('Q', "BU"),
        ('R', "BL"),
        ('S', "BD"),
        ('T', "BR"),
        ('U', "DF"),
        ('V', "DR"),
        ('W', "DB"),
        ('X', "DL"),
    ];

    const PLAN_CORNER: [(char, &str); 24] = [
        ('A', "UBL"),
        ('B', "UBR"),
        ('C', "UFR"),
        ('D', "UFL"),
        ('E', "LUB"),
        ('F', "LUF"),
        ('G', "LDF"),
        ('H', "LDB"),
        ('I', "FUL"),
        ('J', "FUR"),
        ('K', "FDR"),
        ('L', "FDL"),
        ('M', "RUF"),
        ('N', "RUB"),
        ('O', "RDB"),
        ('P', "RDF"),
        ('Q', "BUR"),
        ('R', "BUL"),
        ('S', "BDL"),
        ('T', "BDR"),
        ('U', "DFL"),
        ('V', "DFR"),
        ('W', "DBR"),
        ('X', "DBL"),
    ];

    #[test]
    fn edge_scheme_matches_plan() {
        for (i, &(letter, name)) in PLAN_EDGE.iter().enumerate() {
            let sticker = 24 + i as u8;
            assert_eq!(letter_of(sticker), letter);
            assert_eq!(name_of(sticker), name);
            assert_eq!(sticker_of(Orbit::Edge, letter), sticker);
        }
    }

    #[test]
    fn corner_scheme_matches_plan() {
        for (i, &(letter, name)) in PLAN_CORNER.iter().enumerate() {
            let sticker = i as u8;
            assert_eq!(letter_of(sticker), letter);
            assert_eq!(name_of(sticker), name);
            assert_eq!(sticker_of(Orbit::Corner, letter), sticker);
        }
    }

    #[test]
    fn buffers_are_uf_and_ufr_letter_c() {
        assert_eq!(EDGE_BUFFER_STICKER, 26);
        assert_eq!(letter_of(EDGE_BUFFER_STICKER), 'C');
        assert_eq!(name_of(EDGE_BUFFER_STICKER), "UF");
        assert_eq!(CORNER_BUFFER_STICKER, 2);
        assert_eq!(letter_of(CORNER_BUFFER_STICKER), 'C');
        assert_eq!(name_of(CORNER_BUFFER_STICKER), "UFR");
    }

    #[test]
    fn floating_orders_pinned() {
        assert_eq!(
            EDGE_FLOATING,
            [
                "UF", "UB", "UR", "UL", "FR", "FL", "DF", "DB", "DR", "DL", "BR", "BL"
            ]
        );
        assert_eq!(
            CORNER_FLOATING,
            ["UFR", "UFL", "UBR", "UBL", "DFR", "DFL", "DBR", "DBL"]
        );
        assert_eq!(EDGE_FLOATING.first(), Some(&"UF"));
        assert_eq!(CORNER_FLOATING.first(), Some(&"UFR"));
    }

    #[test]
    fn floating_heads_pinned() {
        assert_eq!(
            edge_floating_heads(),
            [26, 24, 25, 27, 33, 35, 44, 46, 45, 47, 43, 41]
        );
        assert_eq!(corner_floating_heads(), [2, 3, 1, 0, 21, 20, 22, 23]);
        assert_eq!(edge_floating_heads()[0], EDGE_BUFFER_STICKER);
        assert_eq!(corner_floating_heads()[0], CORNER_BUFFER_STICKER);
    }

    #[test]
    fn head_sticker_follows_ud_then_fb_rule_on_all_pieces() {
        for sticker in 0..48u8 {
            let head = head_sticker(name_of(sticker));
            assert!(same_piece(sticker, head));
            let first = name_of(head).as_bytes()[0];
            let has_ud = (0..48u8)
                .filter(|&s| same_piece(s, sticker))
                .any(|s| matches!(name_of(s).as_bytes()[0], b'U' | b'D'));
            if has_ud {
                assert!(matches!(first, b'U' | b'D'));
            } else {
                assert!(matches!(first, b'F' | b'B'));
            }
        }
        assert_eq!(head_sticker("UF"), 26);
        assert_eq!(head_sticker("FU"), 26);
        assert_eq!(head_sticker("FR"), 33);
        assert_eq!(head_sticker("BR"), 43);
        assert_eq!(head_sticker("DFR"), 21);
        assert_eq!(head_sticker("RUF"), 2);
    }

    #[test]
    fn letter_and_sticker_round_trip() {
        for sticker in 0..48u8 {
            let letter = letter_of(sticker);
            assert!((('A'..='X').contains(&letter)));
            let orbit = if sticker < 24 {
                Orbit::Corner
            } else {
                Orbit::Edge
            };
            assert_eq!(sticker_of(orbit, letter), sticker);
        }
        for orbit in [Orbit::Edge, Orbit::Corner] {
            for letter in 'A'..='X' {
                assert_eq!(letter_of(sticker_of(orbit, letter)), letter);
            }
        }
    }

    #[test]
    fn names_are_unique_and_face_complete() {
        for orbit in [Orbit::Edge, Orbit::Corner] {
            let mut names: Vec<&str> = orbit.stickers().map(name_of).collect();
            names.sort_unstable();
            let expected = if orbit == Orbit::Edge { 2 } else { 3 };
            assert!(names.windows(2).all(|w| w[0] != w[1]));
            for name in names {
                let mut faces = name.as_bytes().to_vec();
                faces.sort_unstable();
                assert_eq!(faces.len(), expected);
                assert!(faces.windows(2).all(|w| w[0] != w[1]));
                assert!(
                    faces
                        .iter()
                        .all(|b| matches!(b, b'U' | b'D' | b'F' | b'B' | b'L' | b'R'))
                );
            }
        }
    }

    #[test]
    fn same_piece_groups_by_face_set() {
        assert!(same_piece(26, 32));
        assert!(same_piece(32, 26));
        assert!(!same_piece(26, 25));
        assert!(same_piece(2, 9));
        assert!(same_piece(2, 12));
        assert!(!same_piece(2, 3));
        assert!(!same_piece(2, 26));
        assert!(same_piece(47, 30));
        assert!(same_piece(23, 18));
        for sticker in 0..48u8 {
            assert!(same_piece(sticker, sticker));
        }
    }

    #[test]
    #[should_panic]
    fn letter_of_rejects_out_of_range() {
        letter_of(48);
    }

    #[test]
    #[should_panic]
    fn name_of_rejects_out_of_range() {
        name_of(200);
    }

    #[test]
    #[should_panic]
    fn sticker_of_rejects_out_of_range_letter() {
        sticker_of(Orbit::Edge, 'Z');
    }

    #[test]
    #[should_panic]
    fn head_sticker_rejects_unknown_piece() {
        head_sticker("UU");
    }
}
