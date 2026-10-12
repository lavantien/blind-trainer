use crate::geom::{self, EDGE_GROUPS, CORNER_GROUPS};
use bt_scheme::{Memo, Orbit, apply_sticker_cycles, ideal_cycle_at, sticker_of};

pub struct Env {
    pub edge_floating: Vec<usize>,
    pub corner_floating: Vec<usize>,
}

impl Env {
    pub fn new() -> Env {
        let edge_floating = bt_scheme::EDGE_FLOATING
            .iter()
            .map(|n| group_of_name(false, n))
            .collect();
        let corner_floating = bt_scheme::CORNER_FLOATING
            .iter()
            .map(|n| group_of_name(true, n))
            .collect();
        Env {
            edge_floating,
            corner_floating,
        }
    }

    pub fn floating(&self, orbit: Orbit) -> &Vec<usize> {
        match orbit {
            Orbit::Edge => &self.edge_floating,
            Orbit::Corner => &self.corner_floating,
        }
    }

    pub fn group_of_sticker(&self, orbit: Orbit, s: u8) -> usize {
        match orbit {
            Orbit::Edge => geom::edge_of_sticker(s),
            Orbit::Corner => geom::corner_of_sticker(s),
        }
    }

    pub fn floating_rank(&self, orbit: Orbit, g: usize) -> Option<usize> {
        self.floating(orbit).iter().position(|&x| x == g)
    }
}

pub fn group_of_name(corner: bool, name: &str) -> usize {
    let mask = geom::face_mask_of_name(name);
    let n = if corner { 8 } else { 12 };
    for g in 0..n {
        if geom::face_set_of_group(corner, g) == mask {
            return g;
        }
    }
    panic!("no group for name {name}");
}

pub fn aligned_swap(occ: &mut [u8; 54], orbit: Orbit, g1: usize, g2: usize) {
    match orbit {
        Orbit::Edge => {
            let (a, b) = (geom::edge_cell_roles(g1), geom::edge_cell_roles(g2));
            for i in 0..2 {
                occ.swap(a[i] as usize, b[i] as usize);
            }
        }
        Orbit::Corner => {
            let (a, b) = (geom::corner_cell_roles(g1), geom::corner_cell_roles(g2));
            for i in 0..3 {
                occ.swap(a[i] as usize, b[i] as usize);
            }
        }
    }
}

fn orbit_pairs(m: &Memo, orbit: Orbit) -> &Vec<bt_scheme::CaseId> {
    match orbit {
        Orbit::Edge => &m.pairs.0,
        Orbit::Corner => &m.pairs.1,
    }
}

fn orbit_buffers(m: &Memo, orbit: Orbit) -> &Vec<u8> {
    match orbit {
        Orbit::Edge => &m.buffers.0,
        Orbit::Corner => &m.buffers.1,
    }
}

fn orbit_letters(m: &Memo, orbit: Orbit) -> &Vec<char> {
    match orbit {
        Orbit::Edge => &m.edge_letters,
        Orbit::Corner => &m.corner_letters,
    }
}

fn group_members(orbit: Orbit, g: usize) -> Vec<u8> {
    match orbit {
        Orbit::Edge => EDGE_GROUPS[g].to_vec(),
        Orbit::Corner => CORNER_GROUPS[g].to_vec(),
    }
}

pub fn audit(occ: &[u8; 54], m: &Memo, env: &Env) -> (Vec<String>, usize) {
    let mut fails = Vec::new();
    let mut dup_letters = 0usize;
    for orbit in [Orbit::Edge, Orbit::Corner] {
        let letters = orbit_letters(m, orbit);
        let pairs = orbit_pairs(m, orbit);
        let buffers = orbit_buffers(m, orbit);
        let stickers: Vec<u8> = letters
            .iter()
            .map(|&l| sticker_of(orbit, l))
            .collect();
        let mut sorted = stickers.clone();
        sorted.sort_unstable();
        sorted.dedup();
        if sorted.len() != stickers.len() {
            dup_letters += stickers.len() - sorted.len();
        }
        let trailing = letters.len().saturating_sub(2 * pairs.len());
        if trailing > 1 {
            fails.push(format!("{orbit:?} trailing letters {trailing}"));
        }
        if letters.len() < 2 * pairs.len() {
            fails.push(format!("{orbit:?} letters shorter than pairs"));
        }
        for (i, p) in pairs.iter().enumerate() {
            if p.orbit != orbit {
                fails.push(format!("{orbit:?} pair {i} wrong orbit"));
                continue;
            }
            if 2 * i + 1 >= letters.len() {
                continue;
            }
            if sticker_of(orbit, letters[2 * i]) != p.t1
                || sticker_of(orbit, letters[2 * i + 1]) != p.t2
            {
                fails.push(format!(
                    "{orbit:?} pair {i} targets ({},{}) disagree with letters ({},{})",
                    p.t1, p.t2, letters[2 * i], letters[2 * i + 1]
                ));
            }
        }
        for &s in &stickers {
            let g = env.group_of_sticker(orbit, s);
            if geom::in_place(occ, orbit == Orbit::Corner, g) {
                fails.push(format!("{orbit:?} letter {s} touches in-place piece {g}"));
            }
            if geom::solved_group(occ, orbit == Orbit::Corner, g) {
                fails.push(format!("{orbit:?} letter {s} touches solved piece {g}"));
            }
        }
        let mut replay = *occ;
        let mut prev_rank: Option<usize> = None;
        for (i, (p, &b)) in pairs.iter().zip(buffers.iter()).enumerate() {
            let bg = env.group_of_sticker(orbit, b);
            if geom::head_of_group(orbit == Orbit::Corner, bg) != b {
                fails.push(format!("{orbit:?} buffer {b} at pair {i} is not a head sticker"));
            }
            if geom::solved_group(&replay, orbit == Orbit::Corner, bg)
                || geom::in_place(&replay, orbit == Orbit::Corner, bg)
            {
                fails.push(format!("{orbit:?} buffer {b} at pair {i} is solved or in-place"));
            }
            if let Some(rank) = env.floating_rank(orbit, bg) {
                if let Some(pr) = prev_rank {
                    if rank < pr {
                        fails.push(format!(
                            "{orbit:?} buffer rank regressed {pr} -> {rank} at pair {i}"
                        ));
                    }
                }
                prev_rank = Some(rank);
            } else {
                fails.push(format!("{orbit:?} buffer {b} at pair {i} not a floating piece"));
            }
            let cycles = ideal_cycle_at(orbit, b, p.t1, p.t2);
            apply_sticker_cycles(&mut replay, &cycles);
        }
        let parity_hit = match orbit {
            Orbit::Edge => m.residue.parity.map(|p| p.edge),
            Orbit::Corner => m.residue.parity.map(|p| p.corner),
        };
        match (trailing, parity_hit) {
            (1, None) => fails.push(format!("{orbit:?} trailing letter without parity")),
            (0, Some(_)) => fails.push(format!("{orbit:?} parity without trailing letter")),
            _ => {}
        }
        if let (1, Some(pair)) = (trailing, parity_hit) {
            let last = stickers[stickers.len() - 1];
            let tg = env.group_of_sticker(orbit, last);
            let pg = env.group_of_sticker(orbit, pair.1);
            if tg != pg {
                fails.push(format!(
                    "{orbit:?} trailing piece {tg} differs from parity partner {pg}"
                ));
            }
        }
    }
    let mut replay = *occ;
    for (p, b) in orbit_pairs(m, Orbit::Edge)
        .iter()
        .zip(orbit_buffers(m, Orbit::Edge))
    {
        let cycles = ideal_cycle_at(Orbit::Edge, *b, p.t1, p.t2);
        apply_sticker_cycles(&mut replay, &cycles);
    }
    for (p, b) in orbit_pairs(m, Orbit::Corner)
        .iter()
        .zip(orbit_buffers(m, Orbit::Corner))
    {
        let cycles = ideal_cycle_at(Orbit::Corner, *b, p.t1, p.t2);
        apply_sticker_cycles(&mut replay, &cycles);
    }
    if let Some(parity) = m.residue.parity {
        aligned_swap(
            &mut replay,
            Orbit::Edge,
            env.group_of_sticker(Orbit::Edge, parity.edge.0),
            env.group_of_sticker(Orbit::Edge, parity.edge.1),
        );
        aligned_swap(
            &mut replay,
            Orbit::Corner,
            env.group_of_sticker(Orbit::Corner, parity.corner.0),
            env.group_of_sticker(Orbit::Corner, parity.corner.1),
        );
    }
    let mut flip_groups: Vec<usize> = m
        .residue
        .flips
        .iter()
        .map(|&s| env.group_of_sticker(Orbit::Edge, s))
        .collect();
    let mut expected_flips: Vec<usize> = (0..12)
        .filter(|&g| geom::in_place(&replay, false, g) && !geom::solved_group(&replay, false, g))
        .collect();
    flip_groups.sort_unstable();
    expected_flips.sort_unstable();
    if flip_groups != expected_flips {
        fails.push(format!(
            "residue flips {flip_groups:?} != in-place-unsolved edges {expected_flips:?}"
        ));
    }
    let mut twist_groups: Vec<usize> = m
        .residue
        .twists
        .iter()
        .map(|t| env.group_of_sticker(Orbit::Corner, t.corner))
        .collect();
    let mut expected_twists: Vec<usize> = (0..8)
        .filter(|&g| geom::in_place(&replay, true, g) && !geom::solved_group(&replay, true, g))
        .collect();
    twist_groups.sort_unstable();
    expected_twists.sort_unstable();
    if twist_groups != expected_twists {
        fails.push(format!(
            "residue twists {twist_groups:?} != in-place-unsolved corners {expected_twists:?}"
        ));
    }
    let quarter_sum: u32 = m.residue.twists.iter().map(|t| t.quarter as u32).sum();
    if quarter_sum % 3 != 0 {
        fails.push(format!("residue twist quarters sum {quarter_sum} not 0 mod 3"));
    }
    for t in &m.residue.twists {
        if t.quarter == 0 || t.quarter > 2 {
            fails.push(format!("residue twist quarter {} out of range", t.quarter));
        }
    }
    if m.residue.flips.len() % 2 != 0 {
        fails.push("residue flips odd".to_string());
    }
    for &f in &m.residue.flips {
        if geom::head_of_group(false, env.group_of_sticker(Orbit::Edge, f)) != f {
            fails.push(format!("residue flip {f} not a head sticker"));
        }
    }
    for t in &m.residue.twists {
        if geom::head_of_group(true, env.group_of_sticker(Orbit::Corner, t.corner)) != t.corner {
            fails.push(format!("residue twist {} not a head sticker", t.corner));
        }
    }
    (fails, dup_letters)
}
