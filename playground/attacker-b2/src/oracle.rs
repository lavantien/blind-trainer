use bt_core::{Alg, Cube, Move};

pub struct MyUnit {
    pub setup: Option<Alg>,
    pub a: Alg,
    pub b: Alg,
}

fn compose(first: Option<&Alg>, second: Option<&Alg>) -> Option<Alg> {
    let mut moves: Vec<Move> = Vec::new();
    if let Some(alg) = first {
        moves.extend_from_slice(alg.moves());
    }
    if let Some(alg) = second {
        moves.extend_from_slice(alg.moves());
    }
    if moves.is_empty() {
        None
    } else {
        Some(Alg::new(moves))
    }
}

fn split_top(text: &str, sep: char) -> Option<Vec<&str>> {
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
            c if c == sep && depth == 0 => {
                parts.push(&text[start..index]);
                start = index + c.len_utf8();
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

fn colon_at_top(text: &str) -> Option<usize> {
    let mut depth = 0usize;
    for (index, ch) in text.char_indices() {
        match ch {
            '[' => depth += 1,
            ']' => {
                if depth == 0 {
                    return None;
                }
                depth -= 1;
            }
            ':' if depth == 0 => return Some(index),
            _ => {}
        }
    }
    if depth != 0 {
        return None;
    }
    None
}

pub fn my_expansion(unit: &MyUnit, b_first: bool) -> Alg {
    let mut moves: Vec<Move> = Vec::new();
    if let Some(setup) = &unit.setup {
        moves.extend_from_slice(setup.moves());
    }
    let (first, second) = if b_first {
        (&unit.b, &unit.a)
    } else {
        (&unit.a, &unit.b)
    };
    moves.extend_from_slice(first.moves());
    moves.extend_from_slice(second.moves());
    for mv in first.moves().iter().rev() {
        moves.push(mv.invert());
    }
    for mv in second.moves().iter().rev() {
        moves.push(mv.invert());
    }
    if let Some(setup) = &unit.setup {
        for mv in setup.moves().iter().rev() {
            moves.push(mv.invert());
        }
    }
    Alg::new(moves)
}

pub fn my_concat(units: &[MyUnit], b_first: bool) -> Alg {
    let mut moves: Vec<Move> = Vec::new();
    for unit in units {
        moves.extend_from_slice(my_expansion(unit, b_first).moves());
    }
    Alg::new(moves)
}

pub fn my_perm(units: &[MyUnit], b_first: bool) -> [u8; 54] {
    let mut cube = Cube::solved();
    cube.apply_alg(&my_concat(units, b_first));
    let mut occ = [0u8; 54];
    for pos in 0..54u8 {
        occ[pos as usize] = cube.sticker_at(pos);
    }
    occ
}

pub fn alg_perm(alg: &Alg) -> [u8; 54] {
    let mut cube = Cube::solved();
    cube.apply_alg(alg);
    let mut occ = [0u8; 54];
    for pos in 0..54u8 {
        occ[pos as usize] = cube.sticker_at(pos);
    }
    occ
}

fn slot_alg(text: &str, depth: u32) -> Option<Alg> {
    let trimmed = text.trim();
    if trimmed.contains('[') {
        if depth >= 1 {
            return None;
        }
        let units = my_parse_at(trimmed, depth + 1)?;
        let mut moves: Vec<Move> = Vec::new();
        for unit in &units {
            moves.extend_from_slice(my_expansion(unit, false).moves());
        }
        if moves.is_empty() {
            return None;
        }
        return Some(Alg::new(moves));
    }
    let alg = Alg::parse(trimmed).ok()?;
    if alg.is_empty() {
        return None;
    }
    Some(alg)
}

pub fn my_parse(text: &str) -> Option<Vec<MyUnit>> {
    let trimmed = text.trim();
    if trimmed.is_empty() || trimmed == "Not found." {
        return None;
    }
    my_parse_at(trimmed, 0)
}

fn my_parse_at(text: &str, depth: u32) -> Option<Vec<MyUnit>> {
    let parts = split_top(text, '+')?;
    let mut out = Vec::new();
    for part in parts {
        let part = part.trim();
        if part.is_empty() {
            return None;
        }
        let (setup, core_text) = match colon_at_top(part) {
            Some(index) => {
                let setup = Alg::parse(part[..index].trim()).ok()?;
                if setup.is_empty() {
                    return None;
                }
                (Some(setup), &part[index + 1..])
            }
            None => (None, part),
        };
        let core = core_text.trim();
        if !core.starts_with('[') || !core.ends_with(']') || core.len() < 2 {
            return None;
        }
        let inner = &core[1..core.len() - 1];
        if inner.is_empty() {
            return None;
        }
        let slots = split_top(inner, ',')?;
        match slots.len() {
            2 => {
                let a = slot_alg(slots[0], depth)?;
                let b = slot_alg(slots[1], depth)?;
                out.push(MyUnit { setup, a, b });
            }
            1 => {
                if !inner.contains('[') || depth >= 1 {
                    return None;
                }
                let sub = my_parse_at(inner, depth + 1)?;
                for unit in sub {
                    out.push(MyUnit {
                        setup: compose(setup.as_ref(), unit.setup.as_ref()),
                        a: unit.a,
                        b: unit.b,
                    });
                }
            }
            _ => return None,
        }
    }
    Some(out)
}

pub fn my_verify(text: &str, alg: &Alg) -> Option<(Vec<MyUnit>, bool, bool)> {
    let units = my_parse(text)?;
    let reference = alg_perm(alg);
    let false_ok = my_perm(&units, false) == reference;
    let true_ok = my_perm(&units, true) == reference;
    if false_ok || true_ok {
        Some((units, false_ok, true_ok))
    } else {
        None
    }
}
