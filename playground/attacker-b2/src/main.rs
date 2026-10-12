mod oracle;

use bt_core::{Alg, Cube};
use bt_data::commutator::{decompose, decompose_verified};
use oracle::{alg_perm, my_concat, my_parse, my_perm, my_verify};
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

struct Row {
    solver: &'static str,
    sheet: &'static str,
    position: String,
    alg_text: String,
    commutator: String,
}

fn read_sheet(path: &Path) -> Vec<Row> {
    let text = std::fs::read_to_string(path)
        .unwrap_or_else(|err| panic!("read {}: {err}", path.display()));
    let mut rows = Vec::new();
    let mut saw_header = false;
    for raw in text.split('\n') {
        let line = raw.strip_suffix('\r').unwrap_or(raw);
        if line.trim().is_empty() {
            continue;
        }
        if !saw_header {
            saw_header = true;
            continue;
        }
        let mut cells: Vec<&str> = line.split('\t').collect();
        while cells.len() > 5 && cells.last().is_some_and(|c| c.trim().is_empty()) {
            cells.pop();
        }
        assert_eq!(cells.len(), 5, "row cells in {}", path.display());
        let file = path.file_name().unwrap().to_string_lossy().to_string();
        let sheet = file.trim_end_matches(".tsv").to_string();
        let solver = path
            .parent()
            .unwrap()
            .file_name()
            .unwrap()
            .to_string_lossy()
            .to_string();
        rows.push(Row {
            solver: Box::leak(solver.into_boxed_str()),
            sheet: Box::leak(sheet.into_boxed_str()),
            position: cells[0].to_string(),
            alg_text: cells[1].to_string(),
            commutator: cells[2].to_string(),
        });
    }
    rows
}

fn ref_ground() -> PathBuf {
    match std::env::var("BT_REF_GROUND") {
        Ok(raw) if !raw.is_empty() => PathBuf::from(raw),
        _ => PathBuf::from("../../ref_ground"),
    }
}

fn load_all() -> Vec<Row> {
    let root = ref_ground();
    let mut rows = Vec::new();
    for solver in ["cherry", "eggins"] {
        for sheet in ["edge", "corner", "flips", "twists", "parity", "ltct"] {
            let path = root.join(solver).join(format!("{sheet}.tsv"));
            if path.is_file() {
                rows.extend(read_sheet(&path));
            }
        }
    }
    rows
}

fn expand_units(units: &[bt_data::commutator::Expanded], b_first: bool) -> Alg {
    let mut moves = Vec::new();
    for unit in units {
        let first = if b_first { &unit.b } else { &unit.a };
        let second = if b_first { &unit.a } else { &unit.b };
        if let Some(setup) = &unit.setup {
            moves.extend_from_slice(setup.moves());
        }
        moves.extend_from_slice(first.moves());
        moves.extend_from_slice(second.moves());
        moves.extend_from_slice(first.inverse().moves());
        moves.extend_from_slice(second.inverse().moves());
        if let Some(setup) = &unit.setup {
            moves.extend_from_slice(setup.inverse().moves());
        }
    }
    Alg::new(moves)
}

fn expand_units_as_returned(units: &[bt_data::commutator::Expanded]) -> Alg {
    let mut moves = Vec::new();
    for unit in units {
        let first = if unit.b_first { &unit.b } else { &unit.a };
        let second = if unit.b_first { &unit.a } else { &unit.b };
        if let Some(setup) = &unit.setup {
            moves.extend_from_slice(setup.moves());
        }
        moves.extend_from_slice(first.moves());
        moves.extend_from_slice(second.moves());
        moves.extend_from_slice(first.inverse().moves());
        moves.extend_from_slice(second.inverse().moves());
        if let Some(setup) = &unit.setup {
            moves.extend_from_slice(setup.inverse().moves());
        }
    }
    Alg::new(moves)
}

fn perm_of(alg: &Alg) -> [u8; 54] {
    alg_perm(alg)
}

fn corpus_truth() {
    let rows = load_all();
    let mut total = 0usize;
    let mut not_found = 0usize;
    let mut empty = 0usize;
    let mut parsed = 0usize;
    let mut verified = 0usize;
    let mut disagree_parse = 0usize;
    let mut disagree_verify = 0usize;
    let mut both_orders = 0usize;
    let mut true_only = 0usize;
    let mut bt_true_picked = 0usize;
    let mut mixed_possible = 0usize;
    let mut examples_amb: Vec<String> = Vec::new();
    for row in &rows {
        total += 1;
        let comm = row.commutator.trim();
        if comm == "Not found." {
            not_found += 1;
            continue;
        }
        if comm.is_empty() {
            empty += 1;
            continue;
        }
        let alg = match Alg::parse(&row.alg_text) {
            Ok(alg) => alg,
            Err(err) => panic!("row alg unparsable {}: {err}", row.alg_text),
        };
        let mine_parse = my_parse(&row.commutator).is_some();
        let bt_parse = decompose(&row.commutator).is_some();
        if mine_parse != bt_parse {
            disagree_parse += 1;
            eprintln!(
                "parse disagreement mine={mine_parse} bt={bt_parse}: {:?}",
                row.commutator
            );
        }
        if mine_parse {
            parsed += 1;
        }
        let mine = my_verify(&row.commutator, &alg);
        let bt = decompose_verified(&row.commutator, &alg);
        match (&mine, &bt) {
            (None, None) => {}
            (Some(_), None) | (None, Some(_)) => {
                disagree_verify += 1;
                eprintln!(
                    "verify disagreement mine={} bt={}: {:?} vs {:?}",
                    mine.is_some(),
                    bt.is_some(),
                    row.commutator,
                    row.alg_text
                );
            }
            (Some((units, false_ok, true_ok)), Some(bt_units)) => {
                verified += 1;
                if *false_ok && *true_ok {
                    both_orders += 1;
                    if examples_amb.len() < 5 {
                        examples_amb.push(format!(
                            "{} {} {:?}",
                            row.solver, row.position, row.commutator
                        ));
                    }
                } else if *true_ok {
                    true_only += 1;
                }
                let ref_perm = perm_of(&alg);
                let bt_perm = {
                    let mut cube = Cube::solved();
                    cube.apply_alg(&expand_units(bt_units, false));
                    let mut occ = [0u8; 54];
                    for pos in 0..54u8 {
                        occ[pos as usize] = cube.sticker_at(pos);
                    }
                    occ
                };
                if bt_perm != ref_perm {
                    eprintln!(
                        "bt expansion permutation mismatch: {:?} vs {:?}",
                        row.commutator, row.alg_text
                    );
                }
                let picked_true = bt_units.iter().all(|u| u.b_first);
                let picked_false = bt_units.iter().all(|u| !u.b_first);
                if !picked_true && !picked_false {
                    eprintln!("bt mixed b_first flags across units: {:?}", row.commutator);
                }
                if picked_true {
                    bt_true_picked += 1;
                    if !*true_ok {
                        eprintln!(
                            "bt picked b_first but only a-first matches: {:?}",
                            row.commutator
                        );
                    }
                }
                if picked_false && *false_ok && *true_ok {
                    eprintln!(
                        "ambiguous row, bt picked a-first deterministically: {:?}",
                        row.commutator
                    );
                }
                if units.len() > 1 {
                    let n = units.len().min(12);
                    let mut any_mix = false;
                    for mask in 0u32..(1u32 << n) {
                        let mut moves = Vec::new();
                        for (index, unit) in units.iter().enumerate() {
                            let flag = (mask >> index) & 1 == 1;
                            moves.extend_from_slice(oracle::my_expansion(unit, flag).moves());
                        }
                        let mut cube = Cube::solved();
                        cube.apply_alg(&Alg::new(moves));
                        let mut occ = [0u8; 54];
                        for pos in 0..54u8 {
                            occ[pos as usize] = cube.sticker_at(pos);
                        }
                        if occ == ref_perm && !(mask == 0 || mask == (1u32 << n) - 1) {
                            any_mix = true;
                        }
                    }
                    if any_mix {
                        mixed_possible += 1;
                        eprintln!(
                            "mixed per-unit order solves but uniform works too: {:?}",
                            row.commutator
                        );
                    }
                }
                let mine_uniform = *false_ok || *true_ok;
                if !mine_uniform {
                    eprintln!(
                        "my uniform verify failed but returned Some: {:?}",
                        row.commutator
                    );
                }
            }
        }
    }
    println!("corpus rows total {total}");
    println!("not found rows {not_found}");
    println!("empty commutator rows {empty}");
    println!("parse hits mine {parsed}");
    println!("verified hits {verified}");
    println!("parse disagreements {disagree_parse}");
    println!("verify disagreements {disagree_verify}");
    println!("rows where both orders verify {both_orders}");
    println!("rows where only b-first order verifies {true_only}");
    println!("rows where bt picked b-first {bt_true_picked}");
    println!("rows where a mixed per-unit order also solves {mixed_possible}");
    for example in &examples_amb {
        println!("ambiguous example {example}");
    }
}

fn alternation_truth() {
    let rows = load_all();
    for sheet in ["edge", "corner"] {
        let mut first: BTreeMap<String, String> = BTreeMap::new();
        for row in &rows {
            if row.sheet != sheet {
                continue;
            }
            first
                .entry(row.position.clone())
                .or_insert_with(|| row.alg_text.clone());
        }
        let mut covered = 0usize;
        let mut broken = 0usize;
        for (position, alg_text) in &first {
            let mut parts: Vec<&str> = position.split('-').collect();
            let n = parts.len();
            parts.swap(n - 2, n - 1);
            let flipped = parts.join("-");
            let Some(flip_alg) = first.get(&flipped) else {
                continue;
            };
            covered += 1;
            let a = Alg::parse(alg_text).expect("alg parses");
            let b = Alg::parse(flip_alg).expect("flip alg parses");
            let mut cube = Cube::solved();
            cube.apply_alg(&a);
            cube.apply_alg(&b);
            if !cube.is_solved() {
                broken += 1;
                println!("alternation broken {sheet} {position} then {flipped}");
            }
        }
        println!("{sheet} covered alternation pairs {covered} broken {broken}");
    }
}

fn case_space() {
    let edge = bt_scheme::enumerate_cases(bt_scheme::Orbit::Edge);
    let corner = bt_scheme::enumerate_cases(bt_scheme::Orbit::Corner);
    println!(
        "case space edge {} corner {} total {}",
        edge.len(),
        corner.len(),
        edge.len() + corner.len()
    );
}

fn adversarial() {
    let cases: &[(&str, bool)] = &[
        ("R2 U':[S,R2]", true),
        ("[L E2 L2 E L,U]", true),
        ("[M',U2]+U':[R' F' R,S]", true),
        ("U:[[R' E' R,U2]+U2:[S,R2]]", true),
        ("S R':[[E,R2]+[F2,R S' R']]", true),
        ("[[R,U],S]", true),
        ("[[R,U],[S,D]]", true),
        ("[U:[F,R],D]", true),
        ("  [ R , U2 ] + U' : [ F , D ]  ", true),
        ("[R,U]\u{a0}", true),
        ("R2\u{2009}U':[S,R2]", true),
        ("\u{3000}[R,U]\u{3000}", true),
        ("[R2',U]", true),
        ("[2U,R]", true),
        ("[x,R]", true),
        ("[R U,U]", true),
        ("[R,U]+[F,D]+[S,D]+[F2,R]", true),
        ("U:[[R,U]+[S,D]]", true),
        ("[[A,B],C],D]", false),
        ("U:[[[R,U],S],D]", false),
        ("[[[R,U],S],D]", false),
        ("U:[[[R,U]]]", false),
        ("[[[R,U]]]", false),
        ("[[[[R,U]]]]", false),
        ("[[[R,U],S],D]", false),
        ("[]", false),
        ("[,]", false),
        ("[ , ]", false),
        ("[R,]", false),
        ("[,R]", false),
        ("[:R,U]", false),
        ("[R,U", false),
        ("R,U]", false),
        ("[R,U]]", false),
        ("[[R,U]", false),
        ("]", false),
        ("[", false),
        ("U::[R,U]", false),
        ("[R,U]:[S,D]", false),
        (":[R,U]", false),
        ("U:", false),
        ("U: ", false),
        (": [R,U]", false),
        ("U:[]", false),
        ("[R,U]+", false),
        ("+[R,U]", false),
        ("[R,U]+junk", false),
        ("junk+[R,U]", false),
        ("[R,U]++[F,D]", false),
        ("[R,U] + + [F,D]", false),
        ("U:[[R,U]]]", false),
        ("U:[[R,U]+[S,D]]]", false),
        ("[Q9,R]", false),
        ("[R,U2", false),
        ("[R,U]extra", false),
        ("extra[R,U]", false),
        ("U:extra", false),
        ("N/A", false),
        ("-", false),
        ("R U R'", false),
        ("R U R' U'", false),
        ("not found.", false),
        ("Not found", false),
        ("Not found. ", false),
        (" Not found.", false),
        ("NOT FOUND.", false),
        ("Not found. [R,U]", false),
        ("Not found.+[R,U]", false),
        ("", false),
        ("   ", false),
        (" \t \n ", false),
        ("[R，U]", false),
        ("[R+U,S]", false),
        ("[R,U],[S,D]", false),
        ("R,U", false),
        ("U: [R,U]", true),
        ("U :[R,U]", false),
    ];
    let mut bad = 0usize;
    for (text, expected) in cases {
        let mine = my_parse(text);
        let bt = decompose(text);
        let mine_ok = mine.is_some();
        let bt_ok = bt.is_some();
        let mut status = "ok";
        if mine_ok != *expected {
            status = "MINE-DIFFERS-FROM-EXPECTATION";
            bad += 1;
        }
        if mine_ok != bt_ok {
            status = "MINE-VS-BT-DISAGREE";
            bad += 1;
        }
        if status != "ok" {
            println!("{status}: {text:?} expected={expected} mine={mine_ok} bt={bt_ok}");
        }
        if let (Some(mine), Some(bt)) = (&mine, &bt) {
            let my_perm = my_perm(mine, false);
            let mut cube = Cube::solved();
            cube.apply_alg(&expand_units(bt, false));
            let mut bt_perm = [0u8; 54];
            for pos in 0..54u8 {
                bt_perm[pos as usize] = cube.sticker_at(pos);
            }
            if my_perm != bt_perm {
                println!("EXPANSION-DISAGREE: {text:?}");
                bad += 1;
            }
            if mine.len() != bt.len() {
                println!(
                    "UNIT-COUNT-DISAGREE: {text:?} mine {} bt {}",
                    mine.len(),
                    bt.len()
                );
                bad += 1;
            }
        }
    }
    println!("adversarial cases {} disagreements {bad}", cases.len());
    let long_chain = "[R,U]".to_string() + &" + [F,D]".repeat(2000);
    assert!(decompose(&long_chain).is_some());
    assert!(my_parse(&long_chain).is_some());
    let deep_open = "[".repeat(100_000) + &"]".repeat(100_000);
    assert!(decompose(&deep_open).is_none());
    assert!(my_parse(&deep_open).is_none());
    let deep_mixed = "U:".to_string() + &"[".repeat(50_000) + "R" + &"]".repeat(50_000);
    assert!(decompose(&deep_mixed).is_none());
    println!("stress strings handled");
}

fn alg_from_moves(rng: &mut u64) -> Alg {
    let faces = [
        "R", "U", "F", "D", "L", "B", "R2", "U'", "F2", "M", "S", "E",
    ];
    let count = (next(rng) % 4) as usize + 1;
    let tokens: Vec<&str> = (0..count)
        .map(|_| faces[(next(rng) as usize) % faces.len()])
        .collect();
    Alg::parse(&tokens.join(" ")).unwrap()
}

fn next(rng: &mut u64) -> u64 {
    *rng ^= *rng << 13;
    *rng ^= *rng >> 7;
    *rng ^= *rng << 17;
    *rng
}

fn random_commutator(rng: &mut u64) -> String {
    let units = (next(rng) % 3) as usize + 1;
    let mut parts = Vec::new();
    for _ in 0..units {
        let mut unit = String::new();
        if next(rng).is_multiple_of(3) {
            unit.push_str(&alg_from_moves(rng).format());
            unit.push(':');
        }
        unit.push('[');
        let a = alg_from_moves(rng).format();
        let b = alg_from_moves(rng).format();
        if next(rng).is_multiple_of(4) {
            unit.push_str(&format!(
                "[{},{}]",
                alg_from_moves(rng).format(),
                alg_from_moves(rng).format()
            ));
        } else {
            unit.push_str(&a);
        }
        unit.push(',');
        unit.push_str(&b);
        unit.push(']');
        parts.push(unit);
    }
    parts.join("+")
}

fn mutate(text: &str, rng: &mut u64) -> String {
    let mut chars: Vec<char> = text.chars().collect();
    let edits = (next(rng) % 3) as usize + 1;
    for _ in 0..edits {
        if chars.is_empty() {
            break;
        }
        let pos = (next(rng) as usize) % chars.len();
        match next(rng) % 3 {
            0 => {
                chars.remove(pos);
            }
            1 => {
                let pool = ['[', ']', ',', ':', '+', ' ', 'R', 'U', '2', '\'', 'x'];
                chars.insert(pos, pool[(next(rng) as usize) % pool.len()]);
            }
            _ => {
                let pool = ['[', ']', ',', ':', '+', ' ', 'F', 'D', '2', '\'', 'M'];
                chars[pos] = pool[(next(rng) as usize) % pool.len()];
            }
        }
    }
    chars.into_iter().collect()
}

fn differential_fuzz(iterations: u32) {
    let alphabet: [char; 14] = [
        'R', 'U', 'F', '2', '\'', ':', ',', '[', ']', '+', ' ', 'M', 'x', 'S',
    ];
    let mut rng: u64 = 0x2545_F491_4F6C_DD1D;
    let mut accepted = 0usize;
    let mut verified_hits = 0usize;
    for _ in 0..iterations {
        let text = if next(&mut rng).is_multiple_of(2) {
            let len = (next(&mut rng) % 24) as usize + 1;
            (0..len)
                .map(|_| alphabet[(next(&mut rng) as usize) % alphabet.len()])
                .collect::<String>()
        } else {
            let base = random_commutator(&mut rng);
            if next(&mut rng).is_multiple_of(2) {
                mutate(&base, &mut rng)
            } else {
                base
            }
        };
        let mine = my_parse(&text);
        let bt = decompose(&text);
        assert_eq!(
            decompose(&text).is_some(),
            bt.is_some(),
            "decompose nondeterministic on {text:?}"
        );
        match (&mine, &bt) {
            (None, None) => {}
            (Some(_), None) => panic!("bt rejected what mine accepted: {text:?}"),
            (None, Some(_)) => panic!("mine rejected what bt accepted: {text:?}"),
            (Some(mine), Some(bt)) => {
                accepted += 1;
                assert_eq!(mine.len(), bt.len(), "unit count differs on {text:?}");
                let my_perm = my_perm(mine, false);
                let mut cube = Cube::solved();
                cube.apply_alg(&expand_units(bt, false));
                let mut bt_perm = [0u8; 54];
                for pos in 0..54u8 {
                    bt_perm[pos as usize] = cube.sticker_at(pos);
                }
                assert_eq!(my_perm, bt_perm, "expansion differs on {text:?}");
                let mut alg_moves = Vec::new();
                for unit in mine {
                    alg_moves.extend_from_slice(
                        oracle::my_expansion(unit, next(&mut rng).is_multiple_of(2)).moves(),
                    );
                }
                let reference_alg = Alg::new(alg_moves);
                let reference = perm_of(&reference_alg);
                if let Some(verified) = decompose_verified(&text, &reference_alg) {
                    verified_hits += 1;
                    let mut cube = Cube::solved();
                    cube.apply_alg(&expand_units_as_returned(&verified));
                    let mut occ = [0u8; 54];
                    for pos in 0..54u8 {
                        occ[pos as usize] = cube.sticker_at(pos);
                    }
                    assert_eq!(occ, reference, "verified result mismatch on {text:?}");
                    let all_true = verified.iter().all(|u| u.b_first);
                    let all_false = verified.iter().all(|u| !u.b_first);
                    assert!(all_true || all_false, "mixed b_first flags on {text:?}");
                }
            }
        }
    }
    println!(
        "differential fuzz {iterations} iterations, {accepted} accepted, {verified_hits} verified"
    );
}

fn verify_oracle_independence() {
    let mut rng: u64 = 991_927;
    let mut found_true_only = 0usize;
    let mut found_ambiguous = 0usize;
    for _ in 0..20_000u32 {
        let text = random_commutator(&mut rng);
        if let Some(units) = my_parse(&text) {
            if units.len() != 1 {
                continue;
            }
            let false_ok = my_perm(&units, false);
            let true_ok = my_perm(&units, true);
            if false_ok == true_ok {
                found_ambiguous += 1;
                continue;
            }
            for b_first in [false, true] {
                let alg = my_concat(&units, b_first);
                let verified = decompose_verified(&text, &alg)
                    .unwrap_or_else(|| panic!("self expansion must verify: {text:?}"));
                assert_eq!(
                    verified[0].b_first, b_first,
                    "order selection wrong for {text:?}"
                );
                if b_first {
                    found_true_only += 1;
                }
            }
        }
    }
    println!(
        "order selection: {found_true_only} b-first-required checks passed, {found_ambiguous} commuting constructions seen"
    );
}

fn ui_fallback_rows() {
    let rows = load_all();
    let mut not_found_rows = 0usize;
    let mut example = String::new();
    let mut longest_alg: Vec<(usize, String, String)> = Vec::new();
    for row in &rows {
        let comm = row.commutator.trim();
        let alg = Alg::parse(&row.alg_text).expect("alg parses");
        if comm != "Not found." && !comm.is_empty() && decompose_verified(comm, &alg).is_none() {
            println!(
                "fallback row (verify fails but commutator present): {:?} vs {:?}",
                comm, row.alg_text
            );
        }
        if comm == "Not found." {
            not_found_rows += 1;
            if example.is_empty() {
                example = format!("{} {} {}", row.solver, row.position, row.alg_text);
            }
        }
        longest_alg.push((
            row.alg_text.split_whitespace().count(),
            row.alg_text.clone(),
            row.position.clone(),
        ));
    }
    longest_alg.sort_by_key(|entry| std::cmp::Reverse(entry.0));
    println!("rows with commutator Not found. {not_found_rows}, example {example}");
    println!("longest vendor algs by token count:");
    for (count, alg, position) in longest_alg.iter().take(5) {
        println!("  {count} tokens {position} {alg}");
    }
    let mut longest_comm: Vec<(usize, String)> = rows
        .iter()
        .filter(|row| {
            let comm = row.commutator.trim();
            comm != "Not found." && !comm.is_empty()
        })
        .map(|row| (row.commutator.chars().count(), row.commutator.clone()))
        .collect();
    longest_comm.sort_by_key(|entry| std::cmp::Reverse(entry.0));
    println!("longest commutators by chars:");
    for (count, comm) in longest_comm.iter().take(5) {
        println!("  {count} chars {comm}");
    }
}

fn unicode_fuzz() {
    let ws = [
        '\u{a0}', '\u{2009}', '\u{3000}', '\u{2028}', '\u{feff}', '\u{200b}', '\u{202b}', '\t',
        '\n', '\r',
    ];
    let bases = [
        "R2 U':[S,R2]",
        "[R,U]",
        "Not found.",
        "[M',U2]+U':[R' F' R,S]",
        "[[R,U],S]",
    ];
    let mut rng: u64 = 88_171;
    let mut accepted = 0usize;
    for _ in 0..400_000u32 {
        let base = bases[(next(&mut rng) as usize) % bases.len()];
        let prefix = ws[(next(&mut rng) as usize) % ws.len()];
        let suffix = ws[(next(&mut rng) as usize) % ws.len()];
        let inner = ws[(next(&mut rng) as usize) % ws.len()];
        let pieces: Vec<String> = base.chars().map(|c| c.to_string()).collect();
        let joiner = if next(&mut rng).is_multiple_of(4) {
            inner.to_string()
        } else {
            " ".to_string()
        };
        let mut text = String::new();
        text.push(prefix);
        text.push_str(&pieces.join(&joiner));
        text.push(suffix);
        let mine = my_parse(&text);
        let bt = decompose(&text);
        assert_eq!(
            mine.is_some(),
            bt.is_some(),
            "unicode acceptance differs on {text:?}"
        );
        if bt.is_some() {
            accepted += 1;
        }
    }
    println!("unicode fuzz accepted {accepted} of 400000");
}

fn main() {
    let mode = std::env::args().nth(1).unwrap_or_default();
    match mode.as_str() {
        "corpus" => corpus_truth(),
        "alternation" => alternation_truth(),
        "cases" => case_space(),
        "adversarial" => adversarial(),
        "fuzz" => differential_fuzz(2_000_000),
        "uni" => unicode_fuzz(),
        "orders" => verify_oracle_independence(),
        "fallback" => ui_fallback_rows(),
        _ => {
            corpus_truth();
            alternation_truth();
            case_space();
            adversarial();
            verify_oracle_independence();
            ui_fallback_rows();
            differential_fuzz(500_000);
        }
    }
}
