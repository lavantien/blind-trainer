mod geom;
mod hashes;
mod oracle;
mod walk;

use std::collections::{HashMap, HashSet};
use walk::Env;

fn identity() -> [u8; 54] {
    std::array::from_fn(|i| i as u8)
}

fn fmt_occ(occ: &[u8; 54]) -> String {
    occ.iter().map(|v| v.to_string()).collect::<Vec<_>>().join(",")
}

struct Findings {
    items: Vec<String>,
    cap: usize,
}

impl Findings {
    fn new(cap: usize) -> Findings {
        Findings {
            items: Vec::new(),
            cap,
        }
    }
    fn push(&mut self, msg: String) {
        if self.items.len() < self.cap {
            self.items.push(msg);
        }
    }
    fn count(&self) -> usize {
        self.items.len()
    }
}

fn round_trip(occ: &[u8; 54]) -> Result<(), String> {
    let m = bt_scheme::memo_from_array(occ);
    let mut work = *occ;
    bt_scheme::apply_memo(&mut work, &m);
    if work != identity() {
        return Err(format!(
            "round trip unsolved, residue diffs at {:?}",
            (0..54u8).filter(|&i| work[i as usize] != i).collect::<Vec<_>>()
        ));
    }
    Ok(())
}

fn catch_round_trip(occ: &[u8; 54]) -> Result<(), String> {
    std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| round_trip(occ)))
        .unwrap_or_else(|p| Err(format!("panicked: {}", panic_msg(&p))))
}

fn name_of_edge(head: u8) -> String {
    let mask = geom::face_set_of_group(false, geom::edge_of_sticker(head));
    geom::FACES
        .iter()
        .enumerate()
        .filter(|(i, _)| mask & (1u8 << i) != 0)
        .map(|(_, c)| *c)
        .collect()
}

fn panic_msg(p: &Box<dyn std::any::Any + Send>) -> String {
    if let Some(s) = p.downcast_ref::<&str>() {
        s.to_string()
    } else if let Some(s) = p.downcast_ref::<String>() {
        s.clone()
    } else {
        "opaque panic".to_string()
    }
}

fn cmd_simcheck(path: &str) {
    let text = std::fs::read_to_string(path).expect("simocc readable");
    let rows = oracle::parse_simocc(&text);
    let env = Env::new();
    let mut f = Findings::new(40);
    let mut bt_bridge_diff = 0usize;
    let mut qtm_diff = 0usize;
    let mut class_diff = 0usize;
    let mut pos_diff = 0usize;
    let mut defect_diff = 0usize;
    let mut ideal_diff = 0usize;
    let mut sign_map_bt: HashMap<(usize, u8), HashSet<i8>> = HashMap::new();
    let mut bt_note_pairs: HashMap<usize, HashSet<(i8, i8)>> = HashMap::new();
    let mut geom_note_pairs: HashMap<usize, HashSet<(i8, u8)>> = HashMap::new();
    let mut head_counts: HashMap<String, usize> = HashMap::new();
    let mut bt_note_diff = 0usize;
    let mut geom_note_diff = 0usize;
    let mut bt_geom_diff = 0usize;
    let mut geom_legal_fail = 0usize;
    let mut bt_twist_sum_bad = 0usize;
    for row in &rows {
        let tag = format!("{}/{}#{}", row.solver, row.sheet, row.rowid);
        if !geom::is_bijective(&row.occ) || !geom::centers_permute(&row.occ) {
            f.push(format!("{tag}: bridge occ not a bijection"));
            continue;
        }
        let alg = bt_core::Alg::parse(&row.moves).expect("alg parses");
        let mut cube = bt_core::Cube::solved();
        cube.apply_alg(&alg);
        let mut bt_occ = identity();
        for p in 0..54u8 {
            bt_occ[p as usize] = cube.sticker_at(p);
        }
        if bt_occ != row.occ {
            bt_bridge_diff += 1;
            if bt_bridge_diff <= 3 {
                let first = (0..54u8).find(|&i| bt_occ[i as usize] != row.occ[i as usize]);
                f.push(format!(
                    "{tag}: bt-core alg state differs from pysim at {first:?}"
                ));
            }
        }
        if alg.qtm().to_string() != row.qtm {
            qtm_diff += 1;
            if qtm_diff <= 3 {
                f.push(format!(
                    "{tag}: qtm {} vs vendor {}",
                    alg.qtm(),
                    row.qtm
                ));
            }
        }
        if let Err(e) = catch_round_trip(&row.occ) {
            f.push(format!("{tag}: {e}"));
        }
        if !geom::is_legal(&row.occ) {
            geom_legal_fail += 1;
            if geom_legal_fail <= 5 {
                let ts = geom::twist_sum(&row.occ);
                let fc = geom::flip_count(&row.occ);
                f.push(format!(
                    "{tag}: geom legality rejects oracle state (twist sum {ts} flips {fc})"
                ));
            }
        }
        let cube = bt_core::Cube::from_occ(row.occ);
        let report = bt_scheme::classify(&cube);
        let bt_sum: u32 = (0..8u8)
            .map(|s| bt_scheme::corner_twist_state(&row.occ, s) as u32)
            .sum();
        if geom::is_legal(&row.occ) && bt_sum % 3 != 0 {
            bt_twist_sum_bad += 1;
            if bt_twist_sum_bad <= 3 {
                f.push(format!(
                    "{tag}: bt twist sum {bt_sum} not 0 mod 3 on legal state"
                ));
            }
        }
        let my_edge = oracle::my_trace(&row.occ, bt_scheme::Orbit::Edge, &env);
        let my_corner = oracle::my_trace(&row.occ, bt_scheme::Orbit::Corner, &env);
        let my_tag = oracle::my_classify(&my_edge, &my_corner);
        let clean = oracle::note_clean(&row.sheet, &row.note);
        let expected = oracle::expected_sheet_tag(&row.sheet, clean, &my_tag);
        let got_tag = oracle::class_tag(&report.class);
        if got_tag != expected {
            class_diff += 1;
            f.push(format!(
                "{tag}: class {got_tag} expected {expected} (note {})",
                row.note
            ));
        }
        if expected == got_tag && got_tag != "Unclassified" && got_tag != "Solved" {
            let pos = report.position().unwrap_or_default();
            if pos != row.position {
                pos_diff += 1;
                f.push(format!(
                    "{tag}: position '{pos}' vs vendor '{}'",
                    row.position
                ));
            }
        }
        if my_tag != got_tag {
            f.push(format!(
                "{tag}: differential trace {my_tag} vs bt {got_tag}"
            ));
        }
        let bt_flips: HashSet<usize> = report
            .defects
            .flipped_pieces()
            .into_iter()
            .map(|s| geom::edge_of_sticker(s))
            .collect();
        let my_flips: HashSet<usize> = (0..12)
            .filter(|&s| geom::edge_flip(&row.occ, s))
            .map(|s| geom::edge_of_sticker(row.occ[geom::EDGE_GROUPS[s][0] as usize]))
            .collect();
        if bt_flips != my_flips {
            bt_geom_diff += 1;
            if bt_geom_diff <= 3 {
                f.push(format!("{tag}: bt flips {:?} vs geom {:?}", bt_flips, my_flips));
            }
        }
        let twisted: Vec<(usize, i8)> = report
            .defects
            .twisted_pieces()
            .iter()
            .map(|(h, t)| (geom::corner_of_sticker(*h), *t))
            .collect();
        for &(piece, twist) in &twisted {
            let slot = geom::slot_of_corner_piece(&row.occ, piece);
            let t = geom::corner_twist(&row.occ, slot);
            sign_map_bt.entry((piece, t)).or_default().insert(twist);
        }
        let note_defects = oracle::parse_note(&row.sheet, &row.note);
        for (name, flipped) in &note_defects.edges {
            let g = geom::edge_group_of_name(name);
            let slot = geom::slot_of_edge_piece(&row.occ, g);
            let mine = geom::edge_flip(&row.occ, slot);
            let theirs = bt_flips.contains(&g);
            if theirs != *flipped {
                bt_note_diff += 1;
                if bt_note_diff <= 5 {
                    f.push(format!("{tag}: note flip {name}={flipped} bt {theirs}"));
                }
            }
            if mine != *flipped {
                geom_note_diff += 1;
            }
        }
        for (name, d) in &note_defects.corners {
            let g = geom::corner_group_of_name(name);
            let slot = geom::slot_of_corner_piece(&row.occ, g);
            let mine = geom::corner_twist(&row.occ, slot);
            let bt = twisted.iter().find(|(tg, _)| *tg == g).map(|&(_, t)| t);
            if *d != 0 {
                match bt {
                    Some(b) => {
                        bt_note_pairs.entry(g).or_default().insert((*d, b));
                    }
                    None => {
                        bt_note_diff += 1;
                        if bt_note_diff <= 5 {
                            f.push(format!("{tag}: note twist {name}={d} bt clean"));
                        }
                    }
                }
                if mine != 0 {
                    geom_note_pairs.entry(g).or_default().insert((*d, mine));
                } else {
                    geom_note_diff += 1;
                }
            } else {
                if bt.is_some() {
                    bt_note_diff += 1;
                    if bt_note_diff <= 5 {
                        f.push(format!("{tag}: note {name} clean but bt twisted"));
                    }
                }
                if mine != 0 {
                    geom_note_diff += 1;
                }
            }
        }
        if let Some(((n1, d1), (n2, d2))) = &note_defects.twist_pair {
            let (g1, g2) = (
                geom::corner_group_of_name(n1),
                geom::corner_group_of_name(n2),
            );
            for (g, d) in [(g1, *d1), (g2, *d2)] {
                let slot = geom::slot_of_corner_piece(&row.occ, g);
                let mine = geom::corner_twist(&row.occ, slot);
                if mine == 0 {
                    geom_note_diff += 1;
                } else {
                    geom_note_pairs.entry(g).or_default().insert((d, mine));
                }
                match twisted.iter().find(|(tg, _)| *tg == g) {
                    Some(&(_, b)) => {
                        bt_note_pairs.entry(g).or_default().insert((d, b));
                    }
                    None => {
                        bt_note_diff += 1;
                        if bt_note_diff <= 5 {
                            f.push(format!("{tag}: twists row member {g} bt clean"));
                        }
                    }
                }
            }
        }
        if row.sheet == "edge" || row.sheet == "corner" {
            let orbit = if row.sheet == "edge" {
                bt_scheme::Orbit::Edge
            } else {
                bt_scheme::Orbit::Corner
            };
            let names: Vec<&str> = row.position.split('-').collect();
            if names.len() == 3 {
                *head_counts.entry(names[0].to_string()).or_default() += 1;
                let h = oracle::sticker_of_name(names[0]);
                let case = bt_scheme::CaseId {
                    orbit,
                    t1: oracle::sticker_of_name(names[1]),
                    t2: oracle::sticker_of_name(names[2]),
                };
                let cycles = bt_scheme::ideal_cycle_at(orbit, h, case.t1, case.t2);
                let mut ideal = identity();
                bt_scheme::apply_sticker_cycles(&mut ideal, &cycles);
                if geom::twist_sum(&ideal) % 3 != 0 || geom::flip_count(&ideal) % 2 != 0 {
                    f.push(format!("{tag}: ideal cycle leaves orientation defects"));
                }
                if clean {
                    if ideal != row.occ {
                        ideal_diff += 1;
                        f.push(format!("{tag}: clean row ideal cycle differs from pysim state"));
                    }
                } else {
                    let pi = (
                        geom::occupant_perm(&ideal, true),
                        geom::occupant_perm(&ideal, false),
                    );
                    let po = (
                        geom::occupant_perm(&row.occ, true),
                        geom::occupant_perm(&row.occ, false),
                    );
                    if pi != po {
                        ideal_diff += 1;
                        f.push(format!(
                            "{tag}: defect row ideal cycle permutation differs from pysim"
                        ));
                    }
                }
            }
        }
    }
    let mut sign_violations = 0usize;
    for (key, set) in &sign_map_bt {
        if set.len() > 1 {
            sign_violations += 1;
            f.push(format!(
                "bt twist sign inconsistent for corner {} twist {}: {:?}",
                key.0, key.1, set
            ));
        }
    }
    for (g, pairs) in &bt_note_pairs {
        let signs: HashSet<i8> = pairs.iter().map(|(d, b)| d * b).collect();
        if signs.len() > 1 {
            sign_violations += 1;
            f.push(format!("bt-vs-note sign inconsistent at corner {g}: {pairs:?}"));
        }
    }
    for (g, pairs) in &geom_note_pairs {
        let mut by_geom: HashMap<u8, HashSet<i8>> = HashMap::new();
        for (d, t) in pairs {
            by_geom.entry(*t).or_default().insert(*d);
        }
        if by_geom.values().any(|s| s.len() > 1) || by_geom.len() > 2 {
            sign_violations += 1;
            f.push(format!("geom-vs-note inconsistent at corner {g}: {pairs:?}"));
        }
    }
    println!("bt twist-sum failures on legal rows {bt_twist_sum_bad}");
    println!("geom legality failures on oracle rows {geom_legal_fail}");
    let mut heads: Vec<(String, usize)> = head_counts.into_iter().collect();
    heads.sort();
    println!("position heads {heads:?}");
    println!("rows {}", rows.len());
    println!("bt-vs-bridge occ diffs {bt_bridge_diff}");
    println!("qtm diffs {qtm_diff}");
    println!("class diffs {class_diff}");
    println!("position diffs {pos_diff}");
    println!("bt-vs-note defect diffs {bt_note_diff}");
    println!("geom-vs-note defect diffs {geom_note_diff}");
    println!("bt-vs-geom defect diffs {bt_geom_diff}");
    println!("ideal cycle diffs {ideal_diff}");
    println!("sign consistency violations {sign_violations}");
    println!("findings {}", f.count());
    for item in &f.items {
        println!("FINDING {item}");
    }
}

fn cmd_random(n: u64, deep: bool) {
    let env = Env::new();
    let mut f = Findings::new(40);
    let mut legality_fail = 0u64;
    let mut identity_hits = 0u64;
    let mut seen: HashSet<[u8; 54]> = HashSet::new();
    let mut dup = 0u64;
    let mut dup_letters = 0usize;
    for seed in 0..n {
        let occ = bt_scheme::random_occ(seed);
        if !geom::is_legal(&occ) {
            legality_fail += 1;
            if legality_fail <= 3 {
                f.push(format!("seed {seed}: independent legality rejects {occ:?}"));
            }
        }
        if occ == identity() {
            identity_hits += 1;
        }
        if !seen.insert(occ) {
            dup += 1;
        }
        if let Err(e) = catch_round_trip(&occ) {
            f.push(format!("seed {seed}: {e}"));
        }
        if deep {
            let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                let m = bt_scheme::memo_from_array(&occ);
                walk::audit(&occ, &m, &env)
            }));
            match result {
                Ok((fails, dups)) => {
                    dup_letters += dups;
                    for msg in fails {
                        f.push(format!("seed {seed}: {msg}"));
                    }
                }
                Err(p) => f.push(format!("seed {seed}: walk audit panicked {}", panic_msg(&p))),
            }
        }
    }
    for seed in [0u64, 1, 42, 999] {
        if bt_scheme::random_occ(seed) != bt_scheme::random_occ(seed) {
            f.push(format!("seed {seed}: nondeterministic"));
        }
    }
    println!("seeds {n} deep {deep}");
    println!("repeated letter stickers {dup_letters}");
    println!("legality failures {legality_fail}");
    println!("identity outputs {identity_hits}");
    println!("duplicate outputs {dup}");
    println!("findings {}", f.count());
    for item in &f.items {
        println!("FINDING {item}");
    }
}

fn swap_pieces(occ: &mut [u8; 54], orbit: bt_scheme::Orbit, g1: usize, g2: usize) {
    walk::aligned_swap(occ, orbit, g1, g2);
}

fn flip_in_place(occ: &mut [u8; 54], g: usize) {
    let m = geom::EDGE_GROUPS[g];
    occ.swap(m[0] as usize, m[1] as usize);
}

fn twist_in_place(occ: &mut [u8; 54], g: usize, direction: u8) {
    let m = geom::CORNER_GROUPS[g];
    let a = occ[m[0] as usize];
    let b = occ[m[1] as usize];
    let c = occ[m[2] as usize];
    if direction == 1 {
        occ[m[0] as usize] = b;
        occ[m[1] as usize] = c;
        occ[m[2] as usize] = a;
    } else {
        occ[m[0] as usize] = c;
        occ[m[1] as usize] = a;
        occ[m[2] as usize] = b;
    }
}

fn cycle3(occ: &mut [u8; 54], orbit: bt_scheme::Orbit, g1: usize, g2: usize, g3: usize) {
    let members = |g: usize| -> Vec<u8> {
        match orbit {
            bt_scheme::Orbit::Edge => geom::EDGE_GROUPS[g].to_vec(),
            bt_scheme::Orbit::Corner => geom::CORNER_GROUPS[g].to_vec(),
        }
    };
    let (a, b, c) = (members(g1), members(g2), members(g3));
    let save_a: Vec<u8> = a.iter().map(|&p| occ[p as usize]).collect();
    for (i, &p) in a.iter().enumerate() {
        occ[p as usize] = occ[b[i] as usize];
    }
    for (i, &p) in b.iter().enumerate() {
        occ[p as usize] = occ[c[i] as usize];
    }
    for (i, &p) in c.iter().enumerate() {
        occ[p as usize] = save_a[i];
    }
}

fn with_legal_twists(base: [u8; 54], corners: &[usize]) -> Option<[u8; 54]> {
    let k = corners.len();
    let combos = 3u32.pow(k as u32);
    for mask in 0..combos {
        let mut occ = base;
        let mut m = mask;
        for &g in corners {
            let d = (m % 3) as u8 + 1;
            m /= 3;
            twist_in_place(&mut occ, g, d);
        }
        if geom::is_legal(&occ) {
            return Some(occ);
        }
    }
    None
}

fn cmd_constructs() {
    let env = Env::new();
    let mut f = Findings::new(40);
    let cases: Vec<(&str, [u8; 54], bool)> = {
        let mut v = Vec::new();
        let uf = walk::group_of_name(false, "UF");
        let ub = walk::group_of_name(false, "UB");
        let ur = walk::group_of_name(false, "UR");
        let fl = walk::group_of_name(false, "FL");
        let fr = walk::group_of_name(false, "FR");
        let ufr = walk::group_of_name(true, "UFR");
        let ufl = walk::group_of_name(true, "UFL");
        let dfr = walk::group_of_name(true, "DFR");
        let dbl = walk::group_of_name(true, "DBL");
        let dbr = walk::group_of_name(true, "DBR");
        let mut occ = identity();
        flip_in_place(&mut occ, fl);
        flip_in_place(&mut occ, fr);
        v.push(("two in-place flips", occ, true));
        if let Some(occ) = with_legal_twists(identity(), &[ufr, dbl]) {
            v.push(("two in-place twists", occ, true));
        }
        if let Some(occ) = with_legal_twists(identity(), &[ufr, dfr, dbl]) {
            v.push(("three in-place twists", occ, true));
        }
        let mut base = identity();
        swap_pieces(&mut base, bt_scheme::Orbit::Edge, uf, ub);
        swap_pieces(&mut base, bt_scheme::Orbit::Corner, ufr, ufl);
        v.push(("parity swap", base, true));
        if let Some(occ) = with_legal_twists(base, &[dfr, dbl]) {
            v.push(("ltct shape", occ, true));
        }
        if let Some(occ) = with_legal_twists(base, &[ufr, ufl]) {
            v.push(("twisty parity", occ, true));
        }
        let mut occ = identity();
        cycle3(&mut occ, bt_scheme::Orbit::Edge, uf, ub, ur);
        flip_in_place(&mut occ, fl);
        flip_in_place(&mut occ, fr);
        v.push(("edge cycle plus in-place flips", occ, true));
        let mut base = identity();
        cycle3(&mut base, bt_scheme::Orbit::Corner, ufr, ufl, dfr);
        if let Some(occ) = with_legal_twists(base, &[dbl, dbr]) {
            v.push(("corner cycle plus in-place twists", occ, true));
        }
        let mut occ = identity();
        swap_pieces(&mut occ, bt_scheme::Orbit::Edge, uf, ub);
        v.push(("single edge swap", occ, false));
        let mut occ = identity();
        swap_pieces(&mut occ, bt_scheme::Orbit::Corner, ufr, ufl);
        v.push(("single corner swap", occ, false));
        let mut occ = identity();
        flip_in_place(&mut occ, fl);
        v.push(("single flip", occ, false));
        let mut occ = identity();
        twist_in_place(&mut occ, ufr, 1);
        v.push(("single twist", occ, false));
        v
    };
    for (name, occ, legal) in &cases {
        let legal_now = geom::is_legal(occ);
        if legal_now != *legal {
            f.push(format!("{name}: legality {legal_now} expected {legal}"));
        }
        match catch_round_trip(occ) {
            Ok(()) => {}
            Err(e) => {
                if *legal {
                    f.push(format!("{name}: legal but {e}"));
                } else {
                    println!("NOTE {name}: {e}");
                }
            }
        }
        if *legal {
            let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                let m = bt_scheme::memo_from_array(occ);
                walk::audit(occ, &m, &env)
            }));
            match result {
                Ok((fails, _dups)) => {
                    for msg in fails {
                        f.push(format!("{name}: {msg}"));
                    }
                }
                Err(p) => f.push(format!("{name}: audit panicked {}", panic_msg(&p))),
            }
            if name.contains("corner cycle") {
                println!("DIAG occ {}", fmt_occ(occ));
                let cube = bt_core::Cube::from_occ(*occ);
                let report = bt_scheme::classify(&cube);
                for slot in 0..8usize {
                    let members = geom::CORNER_GROUPS[slot];
                    let my_t = geom::corner_twist(occ, slot);
                    let bt_state =
                        bt_scheme::corner_twist_state(occ, geom::bt_corner_slot(slot));
                    let piece = geom::corner_of_sticker(occ[members[0] as usize]);
                    let analysis_t = report.defects.twist(geom::bt_corner_head(piece));
                    println!(
                        "DIAG slot {slot} piece {piece} rho {my_t} memo_state {bt_state} analysis {analysis_t}"
                    );
                }
                let m = bt_scheme::memo_from_array(occ);
                println!(
                    "DIAG twists {:?} quarters sum {}",
                    m.residue.twists,
                    m.residue.twists.iter().map(|t| t.quarter as u32).sum::<u32>()
                );
            }
            let cube = bt_core::Cube::from_occ(*occ);
            let report = bt_scheme::classify(&cube);
            println!(
                "CASE {name}: class {} letters {:?}+{:?} pairs {}+{} residue flips {} twists {} parity {}",
                oracle::class_tag(&report.class),
                bt_scheme::memo_from_array(occ).edge_letters,
                bt_scheme::memo_from_array(occ).corner_letters,
                bt_scheme::memo_from_array(occ).pairs.0.len(),
                bt_scheme::memo_from_array(occ).pairs.1.len(),
                bt_scheme::memo_from_array(occ).residue.flips.len(),
                bt_scheme::memo_from_array(occ).residue.twists.len(),
                bt_scheme::memo_from_array(occ).residue.parity.is_some()
            );
        }
    }
    println!("findings {}", f.count());
    for item in &f.items {
        println!("FINDING {item}");
    }
}

fn cmd_panics() {
    let mut occ = identity();
    walk::aligned_swap(&mut occ, bt_scheme::Orbit::Edge, walk::group_of_name(false, "UF"), walk::group_of_name(false, "UB"));
    let r = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        bt_scheme::memo_from_array(&occ)
    }));
    println!(
        "single edge swap memo: {}",
        match r {
            Ok(_) => "no panic".to_string(),
            Err(p) => format!("panic {}", panic_msg(&p)),
        }
    );
    let r = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        bt_scheme::classify(&bt_core::Cube::from_occ(occ))
    }));
    println!(
        "single edge swap classify: {}",
        match r {
            Ok(rep) => oracle::class_tag(&rep.class),
            Err(p) => format!("panic {}", panic_msg(&p)),
        }
    );
    let mut occ = identity();
    walk::aligned_swap(&mut occ, bt_scheme::Orbit::Corner, walk::group_of_name(true, "UFR"), walk::group_of_name(true, "UFL"));
    let r = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        bt_scheme::memo_from_array(&occ)
    }));
    println!(
        "single corner swap memo: {}",
        match r {
            Ok(_) => "no panic".to_string(),
            Err(p) => format!("panic {}", panic_msg(&p)),
        }
    );
    for (a, b) in [(2u8, 26u8), (2, 47), (26, 50)] {
        let mut occ = identity();
        occ.swap(a as usize, b as usize);
        let r = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            bt_scheme::memo_from_array(&occ)
        }));
        println!(
            "sticker swap {a}<->{b} memo: {}",
            match r {
                Ok(_) => "no panic".to_string(),
                Err(p) => format!("panic {}", panic_msg(&p)),
            }
        );
        let r = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            bt_scheme::classify(&bt_core::Cube::from_occ(occ))
        }));
        println!(
            "sticker swap {a}<->{b} classify: {}",
            match r {
                Ok(rep) => oracle::class_tag(&rep.class),
                Err(p) => format!("panic {}", panic_msg(&p)),
            }
        );
    }
}

fn cmd_hang() {
    let mut occ = identity();
    occ[26] = 24;
    occ[24] = 25;
    occ[25] = 24;
    let cube = bt_core::Cube::from_occ(occ);
    let report = bt_scheme::classify(&cube);
    println!("classified {}", oracle::class_tag(&report.class));
}

fn cmd_hashes(path: &str) {
    let (ch, pair, total, notes) = hashes::site_equivalences();
    println!("ch site xor-vs-or violations {ch}/8");
    println!("maj left-pair xor-vs-or violations {pair}/8");
    println!("maj 3-way xor-vs-or violations {total}/8");
    for n in &notes {
        println!("SITE {n}");
    }
    let text = std::fs::read_to_string(path).expect("vectors readable");
    let (ok, bad, fails) = hashes::sha256_against_vectors(&text);
    println!("sha256 vector matches {ok} mismatches {bad}");
    for msg in fails {
        println!("SHA {msg}");
    }
}

struct Lcg(u64);

impl Lcg {
    fn next(&mut self) -> u64 {
        self.0 = self
            .0
            .wrapping_mul(6364136223846793005)
            .wrapping_add(1442695040888963407);
        self.0 >> 33
    }
    fn below(&mut self, n: u64) -> u64 {
        self.next() % n.max(1)
    }
}

fn build_cohesive(rng: &mut Lcg) -> [u8; 54] {
    let mut occ = identity();
    let mut edges: Vec<u8> = (0..12).collect();
    for i in (1..edges.len()).rev() {
        let j = rng.below(i as u64 + 1) as usize;
        edges.swap(i, j);
    }
    let mut corners: Vec<u8> = (0..8).collect();
    for i in (1..corners.len()).rev() {
        let j = rng.below(i as u64 + 1) as usize;
        corners.swap(i, j);
    }
    for slot in 0..12usize {
        let src = edges[slot] as usize;
        let a = geom::EDGE_GROUPS[slot];
        let b = geom::EDGE_GROUPS[src];
        let (o0, o1) = if rng.below(2) == 1 { (b[1], b[0]) } else { (b[0], b[1]) };
        occ[a[0] as usize] = o0;
        occ[a[1] as usize] = o1;
    }
    for slot in 0..8usize {
        let src = corners[slot] as usize;
        let a = geom::CORNER_GROUPS[slot];
        let b = geom::CORNER_GROUPS[src];
        let w = rng.below(3) as usize;
        for j in 0..3 {
            occ[a[j] as usize] = b[(j + w) % 3];
        }
    }
    occ
}

fn cmd_cohesivefuzz(n: u64) {
    let mut rng = Lcg(0xB00CBEEF);
    let mut f = Findings::new(30);
    let mut parity_panics = 0u64;
    let mut solved = 0u64;
    let mut unsolved = 0u64;
    let mut legal_solved = 0u64;
    let mut legal_bad = 0u64;
    for i in 0..n {
        let occ = build_cohesive(&mut rng);
        if !geom::cohesive(&occ) {
            f.push(format!("iter {i}: builder produced non-cohesive state"));
            continue;
        }
        let legal = geom::is_legal(&occ);
        match catch_round_trip(&occ) {
            Ok(()) => {
                solved += 1;
                if legal {
                    legal_solved += 1;
                }
            }
            Err(e) => {
                if e.contains("parity must hit both orbits") {
                    parity_panics += 1;
                    if legal {
                        legal_bad += 1;
                        f.push(format!("iter {i}: LEGAL state hit parity panic"));
                    }
                } else {
                    unsolved += 1;
                    f.push(format!("iter {i}: {e}"));
                }
            }
        }
    }
    println!("iters {n} solved {solved} parity-panics {parity_panics} other-failures {unsolved}");
    println!("legal solved {legal_solved} legal parity-panics {legal_bad}");
    println!("findings {}", f.count());
    for item in &f.items {
        println!("FINDING {item}");
    }
}

fn cmd_composed(path: &str) {
    let text = std::fs::read_to_string(path).expect("composed readable");
    let rows = oracle::parse_simocc(&text);
    let env = Env::new();
    let mut f = Findings::new(20);
    let mut illegal = 0usize;
    let mut failures = 0usize;
    for row in &rows {
        if !geom::is_legal(&row.occ) {
            illegal += 1;
            if illegal <= 3 {
                f.push(format!("{}/{}#{}: composed state not legal", row.solver, row.sheet, row.rowid));
            }
            continue;
        }
        if let Err(e) = catch_round_trip(&row.occ) {
            failures += 1;
            f.push(format!(
                "{}/{}#{} ({}): {e} occ {}",
                row.solver, row.sheet, row.rowid, row.moves, fmt_occ(&row.occ)
            ));
            continue;
        }
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            let m = bt_scheme::memo_from_array(&row.occ);
            walk::audit(&row.occ, &m, &env)
        }));
        if let Ok((fails, _)) = result {
            for msg in fails {
                f.push(format!("{}/{}#{}: {msg}", row.solver, row.sheet, row.rowid));
            }
        }
    }
    println!("composed rows {} illegal {} round-trip failures {}", rows.len(), illegal, failures);
    println!("findings {}", f.count());
    for item in &f.items {
        println!("FINDING {item}");
    }
}

fn place_piece(occ: &mut [u8; 54], slot: usize, piece: usize, w: usize) {
    let cells = geom::corner_cell_roles(slot);
    let stickers = geom::corner_cell_roles(piece);
    for k in 0..3 {
        occ[cells[k] as usize] = stickers[(k + w) % 3];
    }
}

fn cmd_sweep() {
    let mut f = Findings::new(30);
    let buffer = geom::corner_group_of_name("UFR");
    let others: Vec<usize> = (0..8).filter(|&g| g != buffer).collect();
    let mut combos = 0usize;
    let mut legal_states = 0usize;
    let mut failures = 0usize;
    let mut panics = 0usize;
    for &s1 in &others {
        for &s2 in &others {
            if s1 == s2 {
                continue;
            }
            for w0 in 0..3usize {
                for w1 in 0..3usize {
                    for w2 in 0..3usize {
                        combos += 1;
                        let mut occ = identity();
                        place_piece(&mut occ, buffer, s1, w0);
                        place_piece(&mut occ, s1, s2, w1);
                        place_piece(&mut occ, s2, buffer, w2);
                        let spare: Vec<usize> = (0..8)
                            .filter(|&g| g != buffer && g != s1 && g != s2)
                            .collect();
                        let mut fixed = geom::is_legal(&occ);
                        if !fixed {
                            'outer: for da in 1..3u8 {
                                for db in 1..3u8 {
                                    let mut cand = occ;
                                    twist_in_place(&mut cand, spare[0], da);
                                    twist_in_place(&mut cand, spare[1], db);
                                    if geom::is_legal(&cand) {
                                        occ = cand;
                                        fixed = true;
                                        break 'outer;
                                    }
                                }
                            }
                        }
                        if !fixed {
                            continue;
                        }
                        legal_states += 1;
                        match catch_round_trip(&occ) {
                            Ok(()) => {}
                            Err(e) => {
                                if e.contains("panicked") {
                                    panics += 1;
                                } else {
                                    failures += 1;
                                }
                                if f.count() < 20 {
                                    f.push(format!(
                                        "s1 {s1} s2 {s2} w ({w0},{w1},{w2}): {e} occ {}",
                                        fmt_occ(&occ)
                                    ));
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    println!("combos {combos} legal {legal_states} failures {failures} panics {panics}");
    println!("findings {}", f.count());
    for item in &f.items {
        println!("FINDING {item}");
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let cmd = args.get(1).map(String::as_str).unwrap_or("help");
    match cmd {
        "simcheck" => cmd_simcheck(args.get(2).expect("path")),
        "composed" => cmd_composed(args.get(2).expect("path")),
        "random" => {
            let n: u64 = args.get(2).expect("n").parse().unwrap();
            let deep = args.get(3).map(|s| s == "deep").unwrap_or(false);
            cmd_random(n, deep);
        }
        "constructs" => cmd_constructs(),
        "sweep" => cmd_sweep(),
        "cohesivefuzz" => {
            let n: u64 = args.get(2).expect("n").parse().unwrap();
            cmd_cohesivefuzz(n);
        }
        "panics" => cmd_panics(),
        "hang" => cmd_hang(),
        "hashes" => cmd_hashes(args.get(2).expect("path")),
        _ => println!("subcommands: simcheck <tsv>, random <n> [deep], constructs, panics, hang, hashes <tsv>"),
    }
}
