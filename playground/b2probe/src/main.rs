use bt_core::{Alg, Cube, Move};
use bt_data::paths::packs_root;
use bt_data::seedpack::{load_pack, load_registry, NSpec};
use bt_puzzles::fto::Fto;
use bt_puzzles::mega;
use bt_puzzles::nxn::Nxn;
use bt_puzzles::pyra::Pyraminx;
use bt_puzzles::skewb::Skewb;
use bt_puzzles::sq1;
use bt_puzzles::{verify_tier1, Geometry};
use std::collections::BTreeMap;
use std::collections::BTreeSet;

fn face_of(letter: char) -> bt_core::Face {
    match letter {
        'U' => bt_core::Face::U,
        'L' => bt_core::Face::L,
        'F' => bt_core::Face::F,
        'R' => bt_core::Face::R,
        'B' => bt_core::Face::B,
        _ => bt_core::Face::D,
    }
}

fn core_moved(letter: char) -> BTreeSet<u8> {
    let mv = Move::Face {
        face: face_of(letter),
        quarter: 1,
    };
    let mut cube = Cube::solved();
    cube.apply(mv);
    (0u8..54).filter(|s| cube.sticker_at(*s) != *s).collect()
}

fn rotation_candidates() -> Vec<String> {
    let single = ["x", "x2", "x'", "y", "y2", "y'", "z", "z2", "z'"];
    let axis = |t: &str| t.chars().next().unwrap();
    let mut out = vec![String::new()];
    for a in single {
        out.push(a.to_string());
    }
    for a in single {
        for b in single {
            if axis(a) != axis(b) {
                out.push(format!("{a} {b}"));
            }
        }
    }
    for a in single {
        for b in single {
            if axis(a) == axis(b) {
                continue;
            }
            for c in single {
                if axis(b) != axis(c) {
                    out.push(format!("{a} {b} {c}"));
                }
            }
        }
    }
    out
}

fn core_tier1(alg: &str) -> Result<(), String> {
    let parsed = Alg::parse(alg).map_err(|e| e.to_string())?;
    let mut fwd = Cube::solved();
    fwd.apply_alg(&parsed);
    fwd.apply_alg(&parsed.inverse());
    if !fwd.is_solved() {
        return Err("fwd round trip".into());
    }
    let mut rev = Cube::solved();
    rev.apply_alg(&parsed.inverse());
    rev.apply_alg(&parsed);
    if !rev.is_solved() {
        return Err("rev round trip".into());
    }
    Ok(())
}

fn core_case(alg: &str) -> Result<Cube, String> {
    let parsed = Alg::parse(alg).map_err(|e| e.to_string())?;
    let mut state = Cube::solved();
    state.apply_alg(&parsed.inverse());
    for cand in rotation_candidates() {
        let mut probe = state;
        if !cand.is_empty() {
            let rot = Alg::parse(&cand).map_err(|e| e.to_string())?;
            probe.apply_alg(&rot);
        }
        if probe.centers_fixed() {
            return Ok(probe);
        }
    }
    Ok(state)
}

fn strip_rotations(alg: &str) -> String {
    alg.split_whitespace()
        .filter(|token| !matches!(token.chars().next(), Some('x' | 'y' | 'z')))
        .collect::<Vec<_>>()
        .join(" ")
}

fn core_text(alg: &str) -> String {
    alg.split_whitespace()
        .map(|token| {
            let bytes = token.as_bytes();
            if bytes.len() >= 2
                && (b'2'..=b'7').contains(&bytes[0])
                && bytes[1].is_ascii_uppercase()
            {
                token.to_string()
            } else if bytes.len() >= 2 && bytes[1] == b'w' && bytes[0].is_ascii_uppercase() {
                format!("{}{}", bytes[0].to_ascii_lowercase() as char, &token[2..])
            } else {
                token.to_string()
            }
        })
        .collect::<Vec<_>>()
        .join(" ")
}

fn core_slot_name(s: u8) -> String {
    let faces = "ULFRBD";
    if s < 24 {
        format!(
            "corner {} pos {}",
            faces.as_bytes()[(s / 4) as usize] as char,
            s % 4
        )
    } else if s < 48 {
        format!(
            "edge {} pos {}",
            faces.as_bytes()[((s - 24) / 4) as usize] as char,
            (s - 24) % 4
        )
    } else {
        format!("center {}", faces.as_bytes()[(s - 48) as usize] as char)
    }
}

#[derive(Clone)]
struct Grid {
    faces: Vec<Vec<Vec<u8>>>,
}

impl Grid {
    fn at(&self, f: usize, r: usize, c: usize) -> u8 {
        self.faces[f][r][c]
    }
    fn n(&self) -> usize {
        self.faces[0].len()
    }
    fn face_cells(&self, f: usize) -> Vec<(usize, usize, usize)> {
        let n = self.n();
        let mut out = Vec::new();
        for r in 0..n {
            for c in 0..n {
                out.push((f, r, c));
            }
        }
        out
    }
    fn changed_vs_solved(&self, solved: &Grid) -> Vec<(usize, usize, usize)> {
        let n = self.n();
        let mut out = Vec::new();
        for f in 0..6 {
            for r in 0..n {
                for c in 0..n {
                    if self.at(f, r, c) != solved.at(f, r, c) {
                        out.push((f, r, c));
                    }
                }
            }
        }
        out
    }
}

fn nxn_grid(n: usize, alg: &str) -> Result<Grid, String> {
    let engine = Nxn::new(n).ok_or("bad n")?;
    let parsed = engine.parse_alg(alg).map_err(|e| e.to_string())?;
    let mut state = engine.solved_state();
    engine
        .apply_alg(&mut state, &parsed)
        .map_err(|e| e.to_string())?;
    Ok(Grid {
        faces: engine
            .facelets(&state)
            .faces
            .iter()
            .map(|face| face.rows.clone())
            .collect(),
    })
}

fn nxn_solved(n: usize) -> Grid {
    let engine = Nxn::new(n).unwrap();
    Grid {
        faces: engine
            .facelets(&engine.solved_state())
            .faces
            .iter()
            .map(|face| face.rows.clone())
            .collect(),
    }
}

fn nxn_changed_by(n: usize, move_token: &str) -> BTreeSet<(usize, usize, usize)> {
    let after = nxn_grid(n, move_token).unwrap();
    let solved = nxn_solved(n);
    after.changed_vs_solved(&solved).into_iter().collect()
}

fn nxn_case(n: usize, alg: &str) -> Result<Grid, String> {
    let engine = Nxn::new(n).ok_or("bad n")?;
    let parsed = engine.parse_alg(alg).map_err(|e| e.to_string())?;
    let mut state = engine.solved_state();
    engine
        .apply_alg(&mut state, &engine.invert_alg(&parsed))
        .map_err(|e| e.to_string())?;
    let raw = Grid {
        faces: engine
            .facelets(&state)
            .faces
            .iter()
            .map(|face| face.rows.clone())
            .collect(),
    };
    for cand in rotation_candidates() {
        if cand.is_empty() {
            continue;
        }
        let mut probe = state.clone();
        let rot = engine.parse_alg(&cand).map_err(|e| e.to_string())?;
        engine
            .apply_alg(&mut probe, &rot)
            .map_err(|e| e.to_string())?;
        let grid = Grid {
            faces: engine
                .facelets(&probe)
                .faces
                .iter()
                .map(|face| face.rows.clone())
                .collect(),
        };
        let centers_ok =
            (0..6).all(|f| (1..n - 1).all(|r| (1..n - 1).all(|c| grid.at(f, r, c) == f as u8)));
        if centers_ok {
            return Ok(grid);
        }
    }
    Ok(raw)
}

const CORNER_POS: [(usize, usize); 4] = [(0, 0), (0, 2), (2, 2), (2, 0)];
const EDGE_POS: [(usize, usize); 4] = [(0, 1), (1, 2), (2, 1), (1, 0)];

fn core_color_grid(alg: &str) -> Vec<u8> {
    let mut cube = Cube::solved();
    let parsed = Alg::parse(alg).unwrap();
    cube.apply_alg(&parsed);
    let home = |sticker: u8| -> u8 {
        if sticker < 24 {
            sticker / 4
        } else if sticker < 48 {
            (sticker - 24) / 4
        } else {
            sticker - 48
        }
    };
    let mut out = vec![0u8; 54];
    for f in 0..6usize {
        for pos in 0..4usize {
            let (r, c) = CORNER_POS[pos];
            out[f * 9 + r * 3 + c] = home(cube.sticker_at((f * 4 + pos) as u8));
            let (er, ec) = EDGE_POS[pos];
            out[f * 9 + er * 3 + ec] = home(cube.sticker_at((24 + f * 4 + pos) as u8));
        }
        out[f * 9 + 4] = home(cube.sticker_at((48 + f) as u8));
    }
    out
}

fn nxn_color_grid(n: usize, alg: &str) -> Vec<u8> {
    let grid = nxn_grid(n, alg).unwrap();
    let mut out = Vec::new();
    for f in 0..6 {
        for r in 0..n {
            for c in 0..n {
                out.push(grid.at(f, r, c));
            }
        }
    }
    out
}

fn corner_pieces() -> Vec<BTreeSet<u8>> {
    let mut pieces = Vec::new();
    for a in ['U', 'L', 'F', 'R', 'B', 'D'] {
        for b in ['U', 'L', 'F', 'R', 'B', 'D'] {
            for c in ['U', 'L', 'F', 'R', 'B', 'D'] {
                if a >= b || b >= c {
                    continue;
                }
                let ma = core_moved(a);
                let mb = core_moved(b);
                let mc = core_moved(c);
                let piece: BTreeSet<u8> = (0u8..24)
                    .filter(|s| ma.contains(s) && mb.contains(s) && mc.contains(s))
                    .collect();
                if piece.len() == 3 {
                    pieces.push(piece);
                }
            }
        }
    }
    pieces
}

fn corner_cycles(alg: &str) -> String {
    let pieces = corner_pieces();
    let piece_of_slot = |slot: u8| -> usize {
        pieces
            .iter()
            .position(|p| p.contains(&slot))
            .unwrap_or(usize::MAX)
    };
    let parsed = Alg::parse(alg).unwrap();
    let mut cube = Cube::solved();
    cube.apply_alg(&parsed);
    let mut seen = vec![false; pieces.len()];
    let mut cycles = Vec::new();
    for i in 0..pieces.len() {
        if seen[i] {
            continue;
        }
        let mut orbit = Vec::new();
        let mut cur = i;
        while !seen[cur] {
            seen[cur] = true;
            orbit.push(cur);
            let probe_slot = *pieces[cur].iter().next().unwrap();
            let sticker = cube.sticker_at(probe_slot);
            cur = piece_of_slot(sticker);
        }
        if orbit.len() > 1 {
            cycles.push(orbit.clone());
        }
    }
    format!("{:?}", cycles)
}

fn main() {
    for (name, alg) in [
        ("l2e.01 proj", "R' L U' R' U R' F R F' R L'"),
        (
            "oll4 proj",
            "R U2 x R U2 R U2 R' U2 L U2 R' U2 R U2 R' U2 R'",
        ),
        ("oll4 noX", "R U2 R U2 R U2 R' U2 L U2 R' U2 R U2 R' U2 R'"),
        ("pll4 wide", "R2 U2 R2 U2 R2 U2"),
        ("tperm", "R U R' U' R' F R2 U' R' U' R U R' F'"),
        ("l2e.02", "R' U' R' U R' F R F' R"),
        ("l2e.03", "R U' R' U R' F R F'"),
        ("l2e.07", "F2 R U2 R U2' R' F2 R' U2 R' U2' R U2 R' U2' R2"),
        (
            "oll4 x'",
            "R U2 x' R U2 R U2 R' U2 L U2 R' U2 R U2 R' U2 R'",
        ),
        (
            "oll4flip inner",
            "2R' U2 2L F2 2L' F2 2R2 U2 2R U2 2R' U2 F2 2R2 F2",
        ),
        ("pll.ja full", "x R2 F R F' R U2 r' U r U2"),
        ("pll.aa full", "x L2 D2 L' U' L D2 L' U L'"),
        ("pll.z full", "M' U M2 U M2 U M' U2 M2"),
        ("pll.ua full", "M2 U M U2 M' U M2"),
    ] {
        if name.starts_with("pll.") {
            let parsed = Alg::parse(alg).unwrap();
            let mut cube = Cube::solved();
            cube.apply_alg(&parsed.inverse());
            for cand in ["", "x", "x'", "x2", "y", "y'", "y2", "z", "z'", "z2"] {
                let mut probe = cube;
                if !cand.is_empty() {
                    probe.apply_alg(&Alg::parse(cand).unwrap());
                }
                let centers_ok = probe.centers_fixed();
                let u_layer: BTreeSet<u8> = core_moved('U');
                let outside: Vec<u8> = (0u8..54)
                    .filter(|s| probe.sticker_at(*s) != *s && !u_layer.contains(s))
                    .collect();
                println!(
                    "norm {name} +'{cand}': centers={centers_ok} outside-U={} corners={}",
                    outside.len(),
                    (0u8..24).filter(|s| probe.sticker_at(*s) != *s).count()
                );
            }
            continue;
        }
        println!("corner cycles {name}: {}", corner_cycles(alg));
    }
    for probe in [
        "R",
        "U'",
        "F2",
        "r",
        "Rw",
        "2R",
        "M",
        "x",
        "y",
        "z",
        "Rw U2 x Rw U2",
    ] {
        let core_alg = probe.replace("Rw", "r").replace("Lw", "l");
        let core = core_color_grid(&core_alg);
        let nxn = nxn_color_grid(3, probe);
        let diffs = core.iter().zip(&nxn).filter(|(a, b)| a != b).count();
        println!("probe {probe}: diffs={diffs}");
        if diffs > 0 {
            for (i, (a, b)) in core.iter().zip(&nxn).enumerate() {
                if a != b {
                    println!("  slot {i}: core {a} nxn {b}");
                }
            }
        }
    }
    for (name, alg, n) in [
        (
            "l2e.05 x->x'",
            "Rw U2 x' Rw U2 Rw U2' Rw' U2 Lw U2 3Rw' U2' Rw U2 Rw' U2' Rw",
            5usize,
        ),
        (
            "l2e.05 x->noop",
            "Rw U2 Rw U2 Rw U2' Rw' U2 Lw U2 3Rw' U2' Rw U2 Rw' U2' Rw",
            5,
        ),
        ("pll4 inner", "2R2 U2 2R2 Uw2 2R2 2U2", 4),
        (
            "oll4flip inner",
            "2R' U2 2L F2 2L' F2 2R2 U2 2R U2 2R' U2 F2 2R2 F2",
            4,
        ),
    ] {
        let grid = nxn_case(n, alg).unwrap();
        let u_moved: BTreeSet<(usize, usize, usize)> = nxn_changed_by(n, "U");
        let mut centers_bad = 0;
        let mut corners_bad = 0;
        let mut non_u_bad = 0;
        let mut u_band = 0;
        for f in 0..6 {
            for r in 0..n {
                for c in 0..n {
                    if grid.at(f, r, c) == f as u8 {
                        continue;
                    }
                    let border = r == 0 || r == n - 1 || c == 0 || c == n - 1;
                    let corner = (r == 0 || r == n - 1) && (c == 0 || c == n - 1);
                    if corner {
                        corners_bad += 1;
                    } else if !border {
                        centers_bad += 1;
                    } else if !u_moved.contains(&(f, r, c)) && f != 0 {
                        non_u_bad += 1;
                    } else {
                        u_band += 1;
                    }
                }
            }
        }
        println!("variant {name}: centers={centers_bad} corners={corners_bad} non_u={non_u_bad} u_band={u_band}");
    }
    let root = packs_root().expect("packs root");
    let entries = load_registry(&root).expect("registry");
    for entry in &entries {
        let rows = load_pack(&root, entry).unwrap_or_else(|e| panic!("{}: {e}", entry.file));
        println!(
            "== {} [{} / {} / {}] rows={}",
            entry.file,
            entry.event,
            entry.method,
            entry.stage,
            rows.len()
        );
        match (entry.event.as_str(), entry.stage.as_str()) {
            ("3x3" | "oh", "pll") => cfop_class(&rows, "pll"),
            ("3x3" | "oh", "oll") => cfop_class(&rows, "oll"),
            ("3x3" | "oh", "f2l") => cfop_class(&rows, "f2l"),
            ("3bld", "concept") => bld_tags(&rows),
            ("3bld", _) => tier1_only_core(&rows),
            ("2x2", _) => cll(&rows),
            ("bigcube", _) => bigcube(entry, &rows),
            ("sq1", "cubeshape") => sq1_cubeshape(&rows),
            ("sq1", _) => sq1_start_state(&rows),
            ("pyraminx", _) => pyra_l4e(&rows),
            ("megaminx", _) => mega_4lll(&rows),
            ("fto", _) => fto_l4t(&rows),
            ("skewb", _) => skewb_rows(&rows),
            _ => println!("   !! unmatched"),
        }
    }
}

fn tier1_only_core(rows: &[bt_data::seedpack::SeedRow]) {
    let mut fails = Vec::new();
    let mut prose = 0;
    for row in rows {
        if row.alg.is_empty() {
            prose += 1;
            continue;
        }
        if let Err(e) = core_tier1(&row.alg) {
            fails.push(format!("{}: {e}", row.case_key));
        }
    }
    println!("   tier1 fails={fails:?} prose={prose}");
}

fn axis_of(face: usize) -> usize {
    match face {
        0 | 5 => 0,
        1 | 3 => 1,
        _ => 2,
    }
}

fn char_axis(h: char) -> usize {
    match h {
        'U' | 'D' => 0,
        'L' | 'R' => 1,
        _ => 2,
    }
}

fn sticker_home(sticker: u8) -> usize {
    if sticker < 24 {
        (sticker / 4) as usize
    } else if sticker < 48 {
        ((sticker - 24) / 4) as usize
    } else {
        (sticker - 48) as usize
    }
}

fn slot_home(s: u8) -> usize {
    sticker_home(s)
}

fn layer_frame_ok(case: &Cube, h: char) -> (bool, bool) {
    let layer: BTreeSet<u8> = core_moved(h);
    let axis = char_axis(h);
    let mut contained = true;
    let mut misoriented = false;
    for s in 0u8..54 {
        if case.sticker_at(s) != s && !layer.contains(&s) {
            contained = false;
        }
        if layer.contains(&s)
            && (axis_of(sticker_home(case.sticker_at(s))) == axis)
                != (axis_of(slot_home(s)) == axis)
        {
            misoriented = true;
        }
    }
    (contained, misoriented)
}

fn cfop_class(rows: &[bt_data::seedpack::SeedRow], stage: &str) {
    let mut fails = Vec::new();
    let mut leak_shown = BTreeSet::new();
    for row in rows {
        if let Err(e) = core_tier1(&row.alg) {
            fails.push(format!("{} tier1 {e}", row.case_key));
            continue;
        }
        if row.alg.is_empty() {
            continue;
        }
        let case = core_case(&row.alg).unwrap();
        let disturbed: Vec<u8> = (0u8..54).filter(|s| case.sticker_at(*s) != *s).collect();
        let frames: Vec<(char, bool, bool)> = ['U', 'D', 'L', 'F', 'R', 'B']
            .into_iter()
            .map(|h| {
                let (contained, misoriented) = layer_frame_ok(&case, h);
                (h, contained, misoriented)
            })
            .collect();
        match stage {
            "pll" => {
                if !frames.iter().any(|(_, contained, _)| *contained) {
                    fails.push(format!("{} pll no single-layer frame", row.case_key));
                }
                if !frames
                    .iter()
                    .any(|(_, contained, misori)| *contained && !misori)
                {
                    fails.push(format!(
                        "{} pll not orientation-clean in any frame",
                        row.case_key
                    ));
                }
            }
            "oll" => {
                if !frames.iter().any(|(_, contained, _)| *contained) {
                    fails.push(format!("{} oll no single-layer frame", row.case_key));
                }
                if !frames
                    .iter()
                    .any(|(_, contained, misori)| *contained && *misori)
                {
                    fails.push(format!("{} oll shows no misorientation", row.case_key));
                }
            }
            "f2l" => {
                let slot: String = serde_json::from_str::<serde_json::Value>(&row.front_json)
                    .unwrap()
                    .get("slot")
                    .and_then(|v| v.as_str())
                    .unwrap_or_default()
                    .to_string();
                let chars: Vec<char> = slot.chars().collect();
                let mut slot_pieces: BTreeSet<u8> = BTreeSet::new();
                if chars.len() == 2 {
                    let a = core_moved(chars[0]);
                    let b = core_moved(chars[1]);
                    let d = core_moved('D');
                    for s in 0u8..54 {
                        if s < 24 && a.contains(&s) && b.contains(&s) && d.contains(&s) {
                            slot_pieces.insert(s);
                        }
                        if (24..48).contains(&s) && a.contains(&s) && b.contains(&s) {
                            slot_pieces.insert(s);
                        }
                    }
                }
                let mut best: Option<(char, Vec<u8>)> = None;
                for (h, contained, _) in &frames {
                    if !contained {
                        continue;
                    }
                    let layer: BTreeSet<u8> = core_moved(*h);
                    let mut allowed = layer;
                    allowed.extend(slot_pieces.iter().copied());
                    let outside: Vec<u8> = disturbed
                        .iter()
                        .copied()
                        .filter(|s| !allowed.contains(s))
                        .collect();
                    if best.as_ref().is_none_or(|(_, o)| outside.len() < o.len()) {
                        best = Some((*h, outside));
                    }
                }
                let Some((h, outside)) = best else {
                    fails.push(format!(
                        "{} f2l slot {slot} no single-layer frame",
                        row.case_key
                    ));
                    continue;
                };
                if !outside.is_empty() {
                    if leak_shown.insert(slot.clone()) {
                        println!(
                            "   f2l leak {} slot {slot} frame {h}: {:?} stickers {:?}",
                            row.case_key,
                            outside
                                .iter()
                                .map(|s| core_slot_name(*s))
                                .collect::<Vec<_>>(),
                            outside
                                .iter()
                                .map(|s| core_slot_name(case.sticker_at(*s)))
                                .collect::<Vec<_>>()
                        );
                    }
                    fails.push(format!(
                        "{} f2l slot {slot} leaks {}",
                        row.case_key,
                        outside.len()
                    ));
                }
            }
            _ => {}
        }
    }
    println!("   tier2[{stage}] fails={fails:?}");
}

fn piece_slots(name: &str) -> BTreeSet<u8> {
    let band: Vec<u8> = if name.len() == 2 {
        (24u8..48).collect()
    } else {
        (0u8..24).collect()
    };
    let mut set: BTreeSet<u8> = band.into_iter().collect();
    for ch in name.chars() {
        let moved = core_moved(ch);
        set.retain(|s| moved.contains(s));
    }
    set
}

const TPERM: &str = "R U R' U' R' F R2 U' R' U' R U R' F'";

fn edge_orbit_perm(case: &Cube) -> (Vec<u8>, usize) {
    let mut wrong: Vec<u8> = Vec::new();
    for s in 24u8..48 {
        if case.sticker_at(s) != s {
            wrong.push(s);
        }
    }
    let mut transposition = false;
    if wrong.len() == 4 || wrong.len() == 2 {
        let stickers: BTreeSet<u8> = wrong.iter().map(|s| case.sticker_at(*s)).collect();
        transposition = stickers.iter().all(|st| wrong.contains(st));
    }
    (wrong, if transposition { 1 } else { 0 })
}

fn bld_tags(rows: &[bt_data::seedpack::SeedRow]) {
    let mut fails = Vec::new();
    for row in rows {
        if row.alg.is_empty() {
            continue;
        }
        let text = core_text(&row.alg);
        if let Err(e) = core_tier1(&text) {
            fails.push(format!("{} tier1 {e}", row.case_key));
            continue;
        }
        let tag: serde_json::Value = serde_json::from_str(&row.front_json).unwrap();
        let kind = tag.get("kind").and_then(|v| v.as_str()).unwrap_or("");
        let case = core_case(&text).unwrap();
        if kind == "edge swap" || kind == "corner swap" {
            let buffer = tag.get("buffer").and_then(|v| v.as_str()).unwrap_or("");
            let target = tag.get("target").and_then(|v| v.as_str()).unwrap_or("");
            let b = piece_slots(buffer);
            let t = piece_slots(target);
            let swap_ok = b.iter().all(|s| t.contains(&case.sticker_at(*s)))
                && t.iter().all(|s| b.contains(&case.sticker_at(*s)));
            if !swap_ok {
                fails.push(format!(
                    "{} {kind} {buffer}->{target} not a clean swap",
                    row.case_key
                ));
            }
        } else if kind == "setup example" {
            let target = tag.get("target").and_then(|v| v.as_str()).unwrap_or("");
            let t = piece_slots(target);
            let b = piece_slots("UR");
            let swap_ok = b.iter().all(|s| t.contains(&case.sticker_at(*s)))
                && t.iter().all(|s| b.contains(&case.sticker_at(*s)));
            if !swap_ok {
                fails.push(format!(
                    "{} setup {target} not a clean UR swap",
                    row.case_key
                ));
            }
        } else if kind == "buffer edge flip" {
            let flips = tag.get("flips").and_then(|v| v.as_str()).unwrap_or("");
            let names: Vec<&str> = flips.split('+').collect();
            let mut flip_ok = names.len() == 2;
            for name in &names {
                let slots = piece_slots(name);
                let swapped = slots
                    .iter()
                    .all(|s| slots.contains(&case.sticker_at(*s)) && case.sticker_at(*s) != *s);
                flip_ok &= swapped;
            }
            let mut others_ok = true;
            for s in 24u8..48 {
                if case.sticker_at(s) != s && !names.iter().any(|n| piece_slots(n).contains(&s)) {
                    others_ok = false;
                }
            }
            if !flip_ok || !others_ok {
                fails.push(format!("{} flip {flips} not a clean flip", row.case_key));
            }
        } else if tag.get("letter").is_some() {
            let setup = &text;
            let inverse = Alg::parse(setup).unwrap().inverse().format();
            let full = format!("{setup} {TPERM} {inverse}");
            let conj = core_case(&full).unwrap();
            let (wrong, transpositions) = edge_orbit_perm(&conj);
            let corners_wrong: Vec<u8> = (0u8..24).filter(|s| conj.sticker_at(*s) != *s).collect();
            if wrong.len() != 4 || transpositions != 1 {
                fails.push(format!(
                    "{} letter setup not a single edge transposition (wrong {})",
                    row.case_key,
                    wrong.len()
                ));
            }
            if corners_wrong.len() != 6 {
                fails.push(format!(
                    "{} letter setup corner collateral {}",
                    row.case_key,
                    corners_wrong.len()
                ));
            }
        }
    }
    println!("   tier2 fails={fails:?}");
}

fn cll(rows: &[bt_data::seedpack::SeedRow]) {
    let n = 2;
    let solved = nxn_solved(n);
    let d_moved: BTreeSet<(usize, usize, usize)> = nxn_changed_by(n, "D");
    let mut bottom: BTreeSet<(usize, usize, usize)> = d_moved;
    for cell in solved.face_cells(5) {
        bottom.insert(cell);
    }
    let mut fails = Vec::new();
    for row in rows {
        if let Err(e) = verify_tier1(&Nxn::new(n).unwrap(), &row.alg) {
            fails.push(format!("{} tier1 {e}", row.case_key));
            continue;
        }
        let mut hit = false;
        let inverse = bt_core::Alg::parse(&row.alg)
            .map(|a| a.inverse().format())
            .unwrap_or_else(|_| row.alg.clone());
        for cand in rotation_candidates() {
            let alg = if cand.is_empty() {
                inverse.clone()
            } else {
                format!("{inverse} {cand}")
            };
            let Ok(grid) = nxn_grid(n, &alg) else {
                continue;
            };
            let solved_all = bottom
                .iter()
                .all(|(f, r, c)| grid.at(*f, *r, *c) == *f as u8);
            if solved_all {
                hit = true;
                break;
            }
        }
        if !hit {
            fails.push(format!(
                "{} no rotation leaves the bottom layer solved",
                row.case_key
            ));
        }
    }
    println!("   tier2 fails={fails:?}");
}

fn bigcube(entry: &bt_data::seedpack::PackEntry, rows: &[bt_data::seedpack::SeedRow]) {
    let sizes: Vec<usize> = match &entry.n {
        NSpec::Sizes(v) => v.iter().map(|x| *x as usize).collect(),
        NSpec::Fixed(v) => vec![*v as usize],
        NSpec::Unset => vec![],
    };
    let mut fails = Vec::new();
    for row in rows {
        let tag: serde_json::Value = serde_json::from_str(&row.front_json).unwrap();
        let case = tag.get("case").and_then(|v| v.as_str()).unwrap_or("");
        let row_ns: Vec<usize> = if entry.method == "yau" {
            let digits: BTreeSet<char> = case.chars().filter(|c| c.is_ascii_digit()).collect();
            if case == "wing5" {
                vec![4, 5, 6, 7]
            } else {
                digits
                    .iter()
                    .map(|d| d.to_digit(10).unwrap() as usize)
                    .collect()
            }
        } else {
            vec![5]
        };
        for n in row_ns {
            if let Err(e) = verify_tier1(&Nxn::new(n).unwrap(), &row.alg) {
                fails.push(format!("{} n={n} tier1 {e}", row.case_key));
                continue;
            }
            let grid = nxn_case(n, &row.alg).unwrap();
            let u_moved: BTreeSet<(usize, usize, usize)> = nxn_changed_by(n, "U");
            let mut centers_bad = 0;
            let mut corners_bad = 0;
            let mut non_u_bad = 0;
            let mut corners_u = 0;
            let mut wing_bad = 0;
            for f in 0..6 {
                for r in 0..n {
                    for c in 0..n {
                        if grid.at(f, r, c) == f as u8 {
                            continue;
                        }
                        let border = r == 0 || r == n - 1 || c == 0 || c == n - 1;
                        let corner = (r == 0 || r == n - 1) && (c == 0 || c == n - 1);
                        if corner {
                            corners_bad += 1;
                            if u_moved.contains(&(f, r, c)) || f == 0 {
                                corners_u += 1;
                            }
                        } else if !border {
                            centers_bad += 1;
                        } else if !u_moved.contains(&(f, r, c)) && f != 0 {
                            non_u_bad += 1;
                        } else {
                            wing_bad += 1;
                        }
                    }
                }
            }
            println!(
                "   {} n={n} centers={centers_bad} corners={corners_bad}(u{corners_u}) non_u_band={non_u_bad} u_band={wing_bad}",
                row.case_key
            );
        }
    }
    println!("   tier2 fails={fails:?}");
}

fn sq1_start_state(rows: &[bt_data::seedpack::SeedRow]) {
    let engine = sq1::Sq1;
    let mut fails = Vec::new();
    for row in rows {
        let parsed = match engine.parse_alg(&row.alg) {
            Ok(p) => p,
            Err(e) => {
                fails.push(format!("{} parse {e}", row.case_key));
                continue;
            }
        };
        let mut state = engine.solved_state();
        let inv = engine.invert_alg(&parsed);
        if let Err(e) = engine.apply_alg(&mut state, &inv) {
            fails.push(format!("{} inverse apply {e}", row.case_key));
            continue;
        }
        if let Err(e) = engine.apply_alg(&mut state, &parsed) {
            fails.push(format!("{} {e}", row.case_key));
            continue;
        }
        if !engine.is_solved(&state) {
            fails.push(format!("{} round trip", row.case_key));
        }
    }
    println!("   tier1(start-state) fails={fails:?}");
}

fn mirrored(widths: &[u8]) -> Vec<u8> {
    let reversed: Vec<u8> = widths.iter().rev().copied().collect();
    let count = reversed.len();
    (0..count)
        .map(|rotation| {
            (0..count)
                .map(|unit| reversed[(rotation + unit) % count])
                .collect::<Vec<u8>>()
        })
        .min()
        .unwrap()
}

fn famkey(widths: &[u8]) -> Vec<u8> {
    let mirror = mirrored(widths);
    if mirror.as_slice() < widths {
        mirror
    } else {
        widths.to_vec()
    }
}

fn sq1_cubeshape(rows: &[bt_data::seedpack::SeedRow]) {
    let engine = sq1::Sq1;
    let mut famkey_by_name: BTreeMap<String, Vec<u8>> = BTreeMap::new();
    let mut swapped = Vec::new();
    let mut broken = Vec::new();
    let mut classes_by_key: BTreeMap<String, (Vec<u8>, Vec<u8>, String, String)> = BTreeMap::new();
    for row in rows {
        let parsed = match engine.parse_alg(&row.alg) {
            Ok(p) => p,
            Err(e) => {
                broken.push(format!("{} parse {e}", row.case_key));
                continue;
            }
        };
        let mut state = engine.solved_state();
        let inv = engine.invert_alg(&parsed);
        if let Err(e) = engine.apply_alg(&mut state, &inv) {
            broken.push(format!("{} {e}", row.case_key));
            continue;
        }
        let top = sq1::canonical_shape(&state, 0);
        let bottom = sq1::canonical_shape(&state, 1);
        if let Err(e) = engine.apply_alg(&mut state, &parsed) {
            broken.push(format!("{} {e}", row.case_key));
            continue;
        }
        if !engine.is_solved(&state) {
            broken.push(format!("{} round trip", row.case_key));
        }
        let tag: serde_json::Value = serde_json::from_str(&row.front_json).unwrap();
        let name = tag.get("case").and_then(|v| v.as_str()).unwrap_or("");
        let parts: Vec<&str> = name.split(" / ").collect();
        if parts.len() != 2 {
            broken.push(format!("{} name {name}", row.case_key));
            continue;
        }
        classes_by_key.insert(
            row.case_key.clone(),
            (top, bottom, parts[0].to_string(), parts[1].to_string()),
        );
    }
    let keys: Vec<String> = classes_by_key.keys().cloned().collect();
    for key in &keys {
        let (top, bottom, n0, n1) = &classes_by_key[key];
        let observed = (famkey(top), famkey(bottom));
        let known = (famkey_by_name.get(n0), famkey_by_name.get(n1));
        let direct =
            known.0.is_none_or(|k| *k == observed.0) && known.1.is_none_or(|k| *k == observed.1);
        let crossed =
            known.0.is_none_or(|k| *k == observed.1) && known.1.is_none_or(|k| *k == observed.0);
        if !direct && !crossed {
            broken.push(format!("{key} name mismatch {n0} / {n1}"));
            continue;
        }
        if direct {
            famkey_by_name
                .entry(n0.clone())
                .or_insert_with(|| observed.0.clone());
            famkey_by_name
                .entry(n1.clone())
                .or_insert_with(|| observed.1.clone());
        } else {
            swapped.push(key.clone());
            famkey_by_name
                .entry(n0.clone())
                .or_insert_with(|| observed.1.clone());
            famkey_by_name
                .entry(n1.clone())
                .or_insert_with(|| observed.0.clone());
        }
    }
    let mut direct_fails = Vec::new();
    for key in &keys {
        if swapped.contains(key) {
            continue;
        }
        let (top, bottom, n0, n1) = &classes_by_key[key];
        if famkey(top) != *famkey_by_name.get(n0).unwrap()
            || famkey(bottom) != *famkey_by_name.get(n1).unwrap()
        {
            direct_fails.push(key.clone());
        }
    }
    println!("   broken={broken:?}");
    println!("   swapped(8 expected)={swapped:?}");
    println!("   direct_fails={direct_fails:?}");
}

fn pyra_l4e(rows: &[bt_data::seedpack::SeedRow]) {
    let engine = Pyraminx;
    let colors = |alg: &str| -> Vec<u8> {
        let parsed = engine.parse_alg(alg).unwrap();
        let mut state = engine.solved_state();
        engine.apply_alg(&mut state, &parsed).unwrap();
        engine
            .facelets(&state)
            .faces
            .iter()
            .flat_map(|f| f.rows.iter().flat_map(|r| r.iter().copied()))
            .collect()
    };
    let solved = colors("");
    let changed = |alg: &str| -> BTreeSet<usize> {
        colors(alg)
            .iter()
            .zip(&solved)
            .enumerate()
            .filter(|(_, (a, b))| a != b)
            .map(|(i, _)| i)
            .collect()
    };
    let mut band_counts = BTreeMap::new();
    for band in ["U", "L", "R", "B"] {
        for slot in changed(band) {
            *band_counts.entry(slot).or_insert(0) += 1;
        }
    }
    let edge_slots: BTreeSet<usize> = band_counts
        .iter()
        .filter(|(_, c)| **c >= 2)
        .map(|(s, _)| *s)
        .collect();
    println!("   edge slots: {:?} (expect 12)", edge_slots.len());
    let mut fails = Vec::new();
    let mut union_disturbed: BTreeSet<usize> = BTreeSet::new();
    for row in rows {
        if let Err(e) = verify_tier1(&engine, &row.alg) {
            fails.push(format!("{} tier1 {e}", row.case_key));
            continue;
        }
        let parsed = engine.parse_alg(&row.alg).unwrap();
        let mut state = engine.solved_state();
        engine
            .apply_alg(&mut state, &engine.invert_alg(&parsed))
            .unwrap();
        let case_colors: Vec<u8> = engine
            .facelets(&state)
            .faces
            .iter()
            .flat_map(|f| f.rows.iter().flat_map(|r| r.iter().copied()))
            .collect();
        let disturbed: Vec<usize> = case_colors
            .iter()
            .zip(&solved)
            .enumerate()
            .filter(|(_, (a, b))| a != b)
            .map(|(i, _)| i)
            .collect();
        let mut normalized = false;
        let mut norm_disturbed: Vec<usize> = disturbed.clone();
        for fixer in [
            "", "U", "U'", "U U", "L", "L'", "L L", "R", "R'", "R R", "B", "B'", "B B",
        ] {
            let mut probe = state.clone();
            if !fixer.is_empty() {
                let fix_alg = engine.parse_alg(fixer).unwrap();
                engine.apply_alg(&mut probe, &fix_alg).unwrap();
            }
            let fixed_colors: Vec<u8> = engine
                .facelets(&probe)
                .faces
                .iter()
                .flat_map(|f| f.rows.iter().flat_map(|r| r.iter().copied()))
                .collect();
            let bad: Vec<usize> = fixed_colors
                .iter()
                .zip(&solved)
                .enumerate()
                .filter(|(_, (a, b))| a != b)
                .map(|(i, _)| i)
                .collect();
            if bad.iter().all(|i| edge_slots.contains(i)) {
                normalized = true;
                norm_disturbed = bad;
                break;
            }
        }
        if !normalized {
            fails.push(format!(
                "{} not vertex-normalizable to edge-only",
                row.case_key
            ));
        }
        union_disturbed.extend(norm_disturbed.iter().copied());
    }
    let untouched: Vec<String> = (0..36)
        .filter(|i| !union_disturbed.contains(i))
        .map(|i| format!("f{}:{}", i / 9, i % 9))
        .collect();
    println!("   pyra untouched slots: {untouched:?}");
    println!("   tier2 fails={fails:?}");
}

fn mega_4lll(rows: &[bt_data::seedpack::SeedRow]) {
    let engine = mega::Mega;
    let mut union_disturbed: BTreeSet<usize> = BTreeSet::new();
    let per_face = mega::FACELETS / mega::FACES;
    let mut u_layer: BTreeSet<usize> = (0..per_face).collect();
    {
        let mut turn = engine.solved_state();
        let u_alg = engine.parse_alg("U").unwrap();
        engine.apply_alg(&mut turn, &u_alg).unwrap();
        for i in 0..mega::FACELETS {
            let face = (i / per_face) as u8;
            if state_color(&engine, &turn, i) != face {
                u_layer.insert(i);
            }
        }
    }
    println!("   u layer slots {}", u_layer.len());
    let mut fails = Vec::new();
    for row in rows {
        if let Err(e) = verify_tier1(&engine, &row.alg) {
            fails.push(format!("{} tier1 {e}", row.case_key));
            continue;
        }
        let parsed = engine.parse_alg(&row.alg).unwrap();
        let mut state = engine.solved_state();
        engine
            .apply_alg(&mut state, &engine.invert_alg(&parsed))
            .unwrap();
        let spin = engine.parse_alg("y").unwrap();
        let mut aligned = None;
        let mut probe = state.clone();
        for _ in 0..5 {
            let centers_ok =
                (0..mega::FACES).all(|f| probe.color(f as u8, mega::CENTER) == f as u8);
            if centers_ok {
                aligned = Some(probe.clone());
                break;
            }
            engine.apply_alg(&mut probe, &spin).unwrap();
        }
        let Some(state) = aligned else {
            fails.push(format!("{} cannot align spin", row.case_key));
            continue;
        };
        let bad: Vec<usize> = (0..mega::FACELETS)
            .filter(|i| {
                let face = (*i / per_face) as u8;
                state.colors_slot(*i) != face
            })
            .collect();
        union_disturbed.extend(bad.iter().copied());
        let outside: Vec<usize> = bad
            .iter()
            .copied()
            .filter(|i| !u_layer.contains(i))
            .collect();
        if !outside.is_empty()
            && (row.case_key == "4lll.01"
                || row.case_key == "4lll.04"
                || row.case_key == "4lll.15"
                || row.case_key == "4lll.26")
        {
            for i in &outside {
                let face = (*i / per_face) as u8;
                let slot = (*i % per_face) as u8;
                let kind = if slot == mega::CENTER as u8 {
                    "center"
                } else if slot < 6 {
                    "corner"
                } else {
                    "edge"
                };
                println!(
                    "   mega {} non-U: face {face} {kind} slot {slot} color {}",
                    row.case_key,
                    state.colors_slot(*i)
                );
            }
        }
        if !outside.is_empty() {
            let fwd_parsed = engine.parse_alg(&row.alg).unwrap();
            let mut fwd = engine.solved_state();
            engine.apply_alg(&mut fwd, &fwd_parsed).unwrap();
            let mut fwd_spin = fwd.clone();
            for _ in 0..5 {
                let ok = (0..mega::FACES).all(|f| fwd_spin.color(f as u8, mega::CENTER) == f as u8);
                if ok {
                    break;
                }
                engine.apply_alg(&mut fwd_spin, &spin).unwrap();
            }
            let fwd_outside: Vec<usize> = (0..mega::FACELETS)
                .filter(|i| {
                    let face = (*i / per_face) as u8;
                    state_color(&engine, &fwd_spin, *i) != face && !u_layer.contains(i)
                })
                .collect();
            let mirrored: String = row
                .alg
                .split_whitespace()
                .map(|token| {
                    let base = token.trim_end_matches('\'').trim_end_matches("2");
                    if base.len() == token.len() {
                        if token.ends_with('2') {
                            token.to_string()
                        } else {
                            format!("{token}'")
                        }
                    } else if token.ends_with('2') {
                        token.to_string()
                    } else {
                        base.to_string()
                    }
                })
                .collect::<Vec<_>>()
                .join(" ");
            let mir_alg = engine.parse_alg(&mirrored).unwrap();
            let mut mir = engine.solved_state();
            engine
                .apply_alg(&mut mir, &engine.invert_alg(&mir_alg))
                .unwrap();
            for _ in 0..5 {
                let ok = (0..mega::FACES).all(|f| mir.color(f as u8, mega::CENTER) == f as u8);
                if ok {
                    break;
                }
                engine.apply_alg(&mut mir, &spin).unwrap();
            }
            let mir_outside = (0..mega::FACELETS)
                .filter(|i| {
                    let face = (*i / per_face) as u8;
                    state_color(&engine, &mir, *i) != face && !u_layer.contains(i)
                })
                .count();
            println!(
                "   mega {} case_nonu={} fwd_nonu={} mirror_nonu={}",
                row.case_key,
                outside.len(),
                fwd_outside.len(),
                mir_outside
            );
            fails.push(format!(
                "{} disturbs {} non-U slots",
                row.case_key,
                outside.len()
            ));
        }
    }
    let untouched: Vec<String> = (0..mega::FACELETS)
        .filter(|i| !union_disturbed.contains(i))
        .map(|i| format!("f{}:{}", i / per_face, i % per_face))
        .collect();
    println!("   mega untouched slots: {untouched:?}");
    println!("   tier2 fails={fails:?}");
}

fn state_color(engine: &mega::Mega, state: &mega::MegaState, i: usize) -> u8 {
    let per_face = mega::FACELETS / mega::FACES;
    state.color((i / per_face) as u8, (i % per_face) as u8)
}

trait MegaSlot {
    fn colors_slot(&self, i: usize) -> u8;
}

impl MegaSlot for mega::MegaState {
    fn colors_slot(&self, i: usize) -> u8 {
        let per_face = mega::FACELETS / mega::FACES;
        self.color((i / per_face) as u8, (i % per_face) as u8)
    }
}

fn fto_l4t(rows: &[bt_data::seedpack::SeedRow]) {
    let engine = Fto::new();
    let mut union_disturbed: BTreeSet<usize> = BTreeSet::new();
    let solved = engine.solved_state();
    let apply = |alg: &str| -> Result<bt_puzzles::fto::State, String> {
        let parsed = engine.parse_alg(alg).map_err(|e| e.to_string())?;
        let mut state = engine.solved_state();
        engine
            .apply_alg(&mut state, &parsed)
            .map_err(|e| e.to_string())?;
        Ok(state)
    };
    let after_u = apply("U").unwrap();
    let u_layer: BTreeSet<usize> = (0..9)
        .chain((0..72).filter(|i| after_u.facelets[*i] != solved.facelets[*i]))
        .collect();
    println!("   u layer slots {}", u_layer.len());
    let mut fails = Vec::new();
    let mut stage_counts: BTreeMap<String, usize> = BTreeMap::new();
    for row in rows {
        if let Err(e) = verify_tier1(&engine, &row.alg) {
            fails.push(format!("{} tier1 {e}", row.case_key));
            continue;
        }
        let tag: serde_json::Value = serde_json::from_str(&row.front_json).unwrap();
        let case = tag.get("case").and_then(|v| v.as_str()).unwrap_or("");
        let stage = case.split('.').next().unwrap_or("").to_string();
        *stage_counts.entry(stage.clone()).or_insert(0) += 1;
        if stage != "Last Layer" {
            continue;
        }
        let parsed = engine.parse_alg(&row.alg).unwrap();
        let mut state = engine.solved_state();
        engine
            .apply_alg(&mut state, &engine.invert_alg(&parsed))
            .unwrap();
        let bad: Vec<usize> = (0..72)
            .filter(|i| state.facelets[*i] != (i / 9) as u8)
            .collect();
        union_disturbed.extend(bad.iter().copied());
        let outside: Vec<usize> = bad
            .iter()
            .copied()
            .filter(|i| !u_layer.contains(i))
            .collect();
        if !outside.is_empty() {
            println!(
                "   fto {} non-U: {:?}",
                row.case_key,
                outside
                    .iter()
                    .map(|i| format!("f{}:{}", i / 9, i % 9))
                    .collect::<Vec<_>>()
            );
            fails.push(format!(
                "{} disturbs {} non-U slots",
                row.case_key,
                outside.len()
            ));
        }
    }
    println!("   stage counts: {stage_counts:?}");
    let untouched: Vec<String> = (0..72)
        .filter(|i| !union_disturbed.contains(i))
        .map(|i| format!("f{}:{}", i / 9, i % 9))
        .collect();
    println!("   fto untouched slots: {untouched:?}");
    println!("   tier2 fails={fails:?}");
}

fn skewb_rows(rows: &[bt_data::seedpack::SeedRow]) {
    let engine = Skewb::new();
    let mut fails = Vec::new();
    for row in rows {
        let stripped = strip_rotations(&row.alg);
        if let Err(e) = verify_tier1(&engine, &stripped) {
            fails.push(format!("{} tier1 {e}", row.case_key));
        }
    }
    println!("   tier1 fails={fails:?}");
}
