use bt_core::{Alg, Cube, Move};

const NOT_FOUND: &str = "Not found.";
const MAX_DEPTH: u32 = 1;

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Expanded {
    pub setup: Option<Alg>,
    pub a: Alg,
    pub b: Alg,
    pub b_first: bool,
}

impl Expanded {
    fn expansion(&self) -> Alg {
        let mut moves = Vec::new();
        if let Some(setup) = &self.setup {
            moves.extend_from_slice(setup.moves());
        }
        let (first, second) = if self.b_first {
            (&self.b, &self.a)
        } else {
            (&self.a, &self.b)
        };
        moves.extend_from_slice(first.moves());
        moves.extend_from_slice(second.moves());
        moves.extend_from_slice(first.inverse().moves());
        moves.extend_from_slice(second.inverse().moves());
        if let Some(setup) = &self.setup {
            moves.extend_from_slice(setup.inverse().moves());
        }
        Alg::new(moves)
    }
}

enum Core {
    Comm(Alg, Alg),
    Sum(Vec<Unit>),
}

struct Unit {
    setup: Option<Alg>,
    core: Core,
}

impl Unit {
    fn expansion(&self) -> Alg {
        let mut moves = Vec::new();
        if let Some(setup) = &self.setup {
            moves.extend_from_slice(setup.moves());
        }
        match &self.core {
            Core::Comm(a, b) => {
                moves.extend_from_slice(a.moves());
                moves.extend_from_slice(b.moves());
                moves.extend_from_slice(a.inverse().moves());
                moves.extend_from_slice(b.inverse().moves());
            }
            Core::Sum(units) => {
                for unit in units {
                    moves.extend_from_slice(unit.expansion().moves());
                }
            }
        }
        if let Some(setup) = &self.setup {
            moves.extend_from_slice(setup.inverse().moves());
        }
        Alg::new(moves)
    }
}

pub fn decompose(text: &str) -> Option<Vec<Expanded>> {
    let trimmed = text.trim();
    if trimmed.is_empty() || trimmed == NOT_FOUND {
        return None;
    }
    let units = parse_expr(trimmed, 0)?;
    let mut expanded = Vec::new();
    flatten(&units, None, &mut expanded);
    Some(expanded)
}

pub fn decompose_verified(text: &str, alg: &Alg) -> Option<Vec<Expanded>> {
    let mut expanded = decompose(text)?;
    let mut reference = Cube::solved();
    reference.apply_alg(alg);
    for b_first in [false, true] {
        let mut cube = Cube::solved();
        for unit in &expanded {
            let candidate = Expanded {
                setup: unit.setup.clone(),
                a: unit.a.clone(),
                b: unit.b.clone(),
                b_first,
            };
            cube.apply_alg(&candidate.expansion());
        }
        if cube == reference {
            for unit in &mut expanded {
                unit.b_first = b_first;
            }
            return Some(expanded);
        }
    }
    None
}

fn flatten(units: &[Unit], inherited: Option<&Alg>, out: &mut Vec<Expanded>) {
    for unit in units {
        let setup = match (inherited, &unit.setup) {
            (Some(pre), Some(own)) => {
                let mut moves = pre.moves().to_vec();
                moves.extend_from_slice(own.moves());
                Some(Alg::new(moves))
            }
            (Some(pre), None) => Some(pre.clone()),
            (None, own) => own.clone(),
        };
        match &unit.core {
            Core::Comm(a, b) => out.push(Expanded {
                setup,
                a: a.clone(),
                b: b.clone(),
                b_first: false,
            }),
            Core::Sum(sub) => flatten(sub, setup.as_ref(), out),
        }
    }
}

fn parse_expr(text: &str, depth: u32) -> Option<Vec<Unit>> {
    let parts = split_depth_zero(text, '+')?;
    parts
        .iter()
        .map(|part| parse_unit(part.trim(), depth))
        .collect()
}

fn parse_unit(text: &str, depth: u32) -> Option<Unit> {
    if text.is_empty() {
        return None;
    }
    let (setup, core_text) = match colon_depth_zero(text) {
        Some(index) => {
            let setup = Alg::parse(text[..index].trim()).ok()?;
            if setup.is_empty() {
                return None;
            }
            (Some(setup), &text[index + 1..])
        }
        None => (None, text),
    };
    let core = parse_core(core_text.trim(), depth)?;
    Some(Unit { setup, core })
}

fn parse_core(text: &str, depth: u32) -> Option<Core> {
    if !text.starts_with('[') || !text.ends_with(']') {
        return None;
    }
    let content = &text[1..text.len() - 1];
    if content.is_empty() {
        return None;
    }
    let parts = split_depth_zero(content, ',')?;
    match parts.len() {
        2 => {
            let a = parse_slot(parts[0].trim(), depth)?;
            let b = parse_slot(parts[1].trim(), depth)?;
            if a.is_empty() || b.is_empty() {
                return None;
            }
            Some(Core::Comm(a, b))
        }
        1 => {
            if !content.contains('[') || depth >= MAX_DEPTH {
                return None;
            }
            Some(Core::Sum(parse_expr(content, depth + 1)?))
        }
        _ => None,
    }
}

fn parse_slot(text: &str, depth: u32) -> Option<Alg> {
    if text.contains('[') {
        if depth >= MAX_DEPTH {
            return None;
        }
        let units = parse_expr(text, depth + 1)?;
        let mut moves: Vec<Move> = Vec::new();
        for unit in units {
            moves.extend_from_slice(unit.expansion().moves());
        }
        if moves.is_empty() {
            return None;
        }
        return Some(Alg::new(moves));
    }
    let alg = Alg::parse(text).ok()?;
    if alg.is_empty() {
        return None;
    }
    Some(alg)
}

fn split_depth_zero(text: &str, sep: char) -> Option<Vec<&str>> {
    let mut depth = 0usize;
    let mut parts = Vec::new();
    let mut start = 0usize;
    for (index, ch) in text.char_indices() {
        match ch {
            '[' => depth += 1,
            ']' => {
                if depth == 0 {
                    return None;
                }
                depth -= 1;
            }
            _ if ch == sep && depth == 0 => {
                parts.push(&text[start..index]);
                start = index + ch.len_utf8();
            }
            _ => {}
        }
    }
    if depth != 0 {
        return None;
    }
    parts.push(&text[start..]);
    Some(parts)
}

fn colon_depth_zero(text: &str) -> Option<usize> {
    let mut depth = 0usize;
    for (index, ch) in text.char_indices() {
        match ch {
            '[' => depth += 1,
            ']' if depth > 0 => depth -= 1,
            ':' if depth == 0 => return Some(index),
            _ => {}
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    fn alg(text: &str) -> Alg {
        Alg::parse(text).unwrap()
    }

    fn setup_text(unit: &Expanded) -> String {
        unit.setup.as_ref().map(|s| s.format()).unwrap_or_default()
    }

    fn concatenation(units: &[Expanded]) -> Alg {
        let mut moves = Vec::new();
        for unit in units {
            moves.extend_from_slice(unit.expansion().moves());
        }
        Alg::new(moves)
    }

    fn render(units: &[Expanded]) -> String {
        units
            .iter()
            .map(|u| {
                let (first, second) = if u.b_first {
                    (&u.b, &u.a)
                } else {
                    (&u.a, &u.b)
                };
                let core = format!("[{},{}]", first.format(), second.format());
                match &u.setup {
                    Some(s) => format!("{}:{}", s.format(), core),
                    None => core,
                }
            })
            .collect::<Vec<String>>()
            .join("+")
    }

    #[test]
    fn setup_form_parses_and_reports_structure() {
        let units = decompose("R2 U':[S,R2]").unwrap();
        assert_eq!(units.len(), 1);
        assert_eq!(setup_text(&units[0]), "R2 U'");
        assert_eq!(units[0].a.format(), "S");
        assert_eq!(units[0].b.format(), "R2");
        assert!(!units[0].b_first);
    }

    #[test]
    fn setup_form_verifies_against_its_row_alg() {
        let units = decompose_verified("R2 U':[S,R2]", &alg("R2 U' S R2 S' R2 U R2")).unwrap();
        assert_eq!(units.len(), 1);
        assert!(!units[0].b_first);
    }

    #[test]
    fn pure_multi_move_slots_parse_and_verify() {
        let units = decompose("[L E2 L2 E L,U]").unwrap();
        assert_eq!(units.len(), 1);
        assert_eq!(setup_text(&units[0]), "");
        assert_eq!(units[0].a.format(), "L E2 L2 E L");
        assert_eq!(units[0].b.format(), "U");
        let verified =
            decompose_verified("[L E2 L2 E L,U]", &alg("L E2 L2 E L U L' E' L2 E2 L' U'")).unwrap();
        assert!(!verified[0].b_first);
    }

    #[test]
    fn combo_units_parse_and_verify() {
        let units = decompose("[M',U2]+U':[R' F' R,S]").unwrap();
        assert_eq!(units.len(), 2);
        assert_eq!(setup_text(&units[0]), "");
        assert_eq!(units[0].a.format(), "M'");
        assert_eq!(units[0].b.format(), "U2");
        assert_eq!(setup_text(&units[1]), "U'");
        assert_eq!(units[1].a.format(), "R' F' R");
        assert_eq!(units[1].b.format(), "S");
        let verified = decompose_verified(
            "[M',U2]+U':[R' F' R,S]",
            &alg("M' U2 M U R' F' R S R' F R S' U"),
        )
        .unwrap();
        assert_eq!(verified.len(), 2);
        assert!(!verified.iter().any(|u| u.b_first));
    }

    #[test]
    fn combo_with_setup_verifies() {
        decompose_verified("[U',M]+M U:[U,M']", &alg("U' M U' M' U' M U' M'")).unwrap();
    }

    #[test]
    fn nested_sum_distributes_outer_setup() {
        let units = decompose("U:[[R' E' R,U2]+U2:[S,R2]]").unwrap();
        assert_eq!(units.len(), 2);
        assert_eq!(setup_text(&units[0]), "U");
        assert_eq!(units[0].a.format(), "R' E' R");
        assert_eq!(units[0].b.format(), "U2");
        assert_eq!(setup_text(&units[1]), "U U2");
        assert_eq!(units[1].a.format(), "S");
        assert_eq!(units[1].b.format(), "R2");
        decompose_verified(
            "U:[[R' E' R,U2]+U2:[S,R2]]",
            &alg("U R' E' R U2 R' E R S R2 S' R2 U"),
        )
        .unwrap();
    }

    #[test]
    fn nested_bare_sum_keeps_setup_on_both_units() {
        let units = decompose("S R':[[E,R2]+[F2,R S' R']]").unwrap();
        assert_eq!(units.len(), 2);
        assert_eq!(setup_text(&units[0]), "S R'");
        assert_eq!(setup_text(&units[1]), "S R'");
        assert_eq!(units[1].b.format(), "R S' R'");
        decompose_verified(
            "S R':[[E,R2]+[F2,R S' R']]",
            &alg("S R' E R2 E' R2 F2 R S' R' F2 R"),
        )
        .unwrap();
    }

    #[test]
    fn b_first_order_is_chosen_by_verification() {
        let units = decompose_verified("[R,U]", &alg("U R U' R'")).unwrap();
        assert_eq!(units.len(), 1);
        assert!(units[0].b_first);
        let plain = decompose("[R,U]").unwrap();
        assert!(!plain[0].b_first);
    }

    #[test]
    fn b_first_applies_to_every_unit_in_a_combo() {
        let units = decompose_verified("[R,U]+[F,D]", &alg("U R U' R' D F D' F'")).unwrap();
        assert_eq!(units.len(), 2);
        assert!(units.iter().all(|u| u.b_first));
    }

    #[test]
    fn mismatched_alg_returns_none() {
        assert!(decompose_verified("[R,U]", &alg("F")).is_none());
        assert!(decompose_verified("[R,U]", &alg("R U R' U' F")).is_none());
        assert!(decompose_verified("R2 U':[S,R2]", &alg("R2 U' S R2 S' R2 U R2 F2")).is_none());
    }

    #[test]
    fn not_found_empty_and_whitespace_return_none() {
        assert!(decompose("Not found.").is_none());
        assert!(decompose("").is_none());
        assert!(decompose("   ").is_none());
        assert!(decompose(" \t \n ").is_none());
        assert!(decompose_verified("Not found.", &alg("R U")).is_none());
        assert!(decompose_verified("", &alg("R U")).is_none());
    }

    #[test]
    fn garbage_returns_none() {
        let bad = [
            "N/A",
            "-",
            "R U R'",
            "R U R' U'",
            "[,]",
            "[R,]",
            "[,R]",
            "[:R,U]",
            "[R,U",
            "R,U]",
            "[R,U]]",
            "[[R,U]",
            "U::[R,U]",
            "[Q9,R]",
            "[R,U]+",
            "+[R,U]",
            "[R,U]+junk",
            "junk+[R,U]",
            "U:[[R,U]]]",
            "U:[[R,U]+[S,D]]]",
            "[[A,B],C],D]",
            "[R,U]:[S,D]",
        ];
        for text in bad {
            assert!(decompose(text).is_none(), "{text:?}");
        }
    }

    #[test]
    fn slot_nesting_expands_the_inner_commutator() {
        let units = decompose("[[R,U],S]").unwrap();
        assert_eq!(units.len(), 1);
        assert_eq!(units[0].a.format(), "R U R' U'");
        assert_eq!(units[0].b.format(), "S");
        decompose_verified("[[R,U],S]", &alg("R U R' U' S U R U' R' S'")).unwrap();
    }

    #[test]
    fn tolerates_padding_around_separators() {
        let units = decompose("  [ R , U2 ] + U' : [ F , D ]  ").unwrap();
        assert_eq!(units.len(), 2);
        assert_eq!(setup_text(&units[1]), "U'");
        assert_eq!(units[1].a.format(), "F");
    }

    #[test]
    fn round_trip_through_render_is_stable() {
        let texts = [
            "R2 U':[S,R2]",
            "[L E2 L2 E L,U]",
            "[M',U2]+U':[R' F' R,S]",
            "U:[[R' E' R,U2]+U2:[S,R2]]",
            "S R':[[E,R2]+[F2,R S' R']]",
            "[U',M]+M U:[U,M']",
            "[R,U]+[F,D]",
        ];
        for text in texts {
            let once = decompose(text).unwrap();
            let twice = decompose(&render(&once)).unwrap();
            assert_eq!(once, twice, "{text}");
            let third = decompose(&render(&twice)).unwrap();
            assert_eq!(twice, third, "{text}");
        }
        let verified = decompose_verified("[R,U]+[F,D]", &alg("U R U' R' D F D' F'")).unwrap();
        let text = render(&verified);
        assert_eq!(text, "[U,R]+[D,F]");
        let again = decompose_verified(&text, &alg("U R U' R' D F D' F'")).unwrap();
        assert_eq!(concatenation(&again), concatenation(&verified));
        assert_eq!(render(&again), text);
    }

    #[test]
    fn deep_nesting_is_rejected() {
        assert!(decompose("U:[[[R,U],S],D]").is_none());
        assert!(decompose("[[[R,U],S],D]").is_none());
    }
}
