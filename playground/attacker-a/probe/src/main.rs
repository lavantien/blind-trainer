use bt_core::{Alg, Axis, Cube, Face, Move, Slice};
use bt_scheme::{
    CORNER_BUFFER_STICKER, EDGE_BUFFER_STICKER, Orbit, enumerate_cases, head_sticker, letter_of,
    name_of, same_piece, sticker_of,
};
use std::panic::{AssertUnwindSafe, catch_unwind};
use std::sync::atomic::{AtomicU64, Ordering};

static FAILURES: AtomicU64 = AtomicU64::new(0);
static CHECKS: AtomicU64 = AtomicU64::new(0);

fn fail(msg: String) {
    FAILURES.fetch_add(1, Ordering::Relaxed);
    println!("FAIL {}", msg);
}

fn passes(name: &str, count: usize) {
    println!("PASS {} {}", name, count);
}

fn expect(cond: bool, msg: String) {
    CHECKS.fetch_add(1, Ordering::Relaxed);
    if !cond {
        fail(msg);
    }
}

fn read_lines(path: &str) -> Vec<String> {
    let raw = std::fs::read_to_string(path).unwrap_or_else(|e| panic!("cannot read {}: {}", path, e));
    raw.replace("\r\n", "\n").split('\n').filter(|l| !l.is_empty()).map(|l| l.to_string()).collect()
}

fn parse_csv54(field: &str) -> [u8; 54] {
    let mut out = [0u8; 54];
    for (i, part) in field.split(',').enumerate() {
        out[i] = part.parse().unwrap();
    }
    out
}

fn occ_of_alg(alg: &Alg) -> [u8; 54] {
    let mut cube = Cube::solved();
    cube.apply_alg(alg);
    let mut out = [0u8; 54];
    for pos in 0..54u8 {
        out[pos as usize] = cube.sticker_at(pos);
    }
    out
}

fn table_of_alg(alg: &Alg) -> [u8; 54] {
    let occ = occ_of_alg(alg);
    let mut table = [0u8; 54];
    for (pos, &sticker) in occ.iter().enumerate() {
        table[sticker as usize] = pos as u8;
    }
    table
}

fn face_of_str(s: &str) -> Face {
    match s {
        "U" => Face::U,
        "L" => Face::L,
        "F" => Face::F,
        "R" => Face::R,
        "B" => Face::B,
        _ => Face::D,
    }
}

struct XorShift(u64);

impl XorShift {
    fn next(&mut self) -> u64 {
        let mut x = self.0;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        self.0 = x;
        x
    }

    fn below(&mut self, n: usize) -> usize {
        (self.next() % n as u64) as usize
    }
}

fn check_generators(root: &str) {
    let lines = read_lines(&format!("{}/playground/attacker-a/gen/expected_generators.tsv", root));
    let mut count = 0;
    let mut centers_fixed_count = 0;
    for line in &lines[1..] {
        let mut parts = line.split('\t');
        let token = parts.next().unwrap();
        let expected = parse_csv54(parts.next().unwrap());
        let alg = Alg::parse(token).unwrap_or_else(|e| panic!("token {} rejected: {}", token, e));
        expect(alg.len() == 1, format!("token {} len {}", token, alg.len()));
        let table = table_of_alg(&alg);
        expect(table == expected, format!("generator table mismatch {}", token));
        let mv_table = alg.moves()[0].table();
        expect(mv_table == expected, format!("Move::table mismatch {}", token));
        let mut cube = Cube::solved();
        cube.apply_alg(&alg);
        let oracle_centers_fixed = (48..54).all(|i| expected[i] == i as u8);
        expect(
            cube.centers_fixed() == oracle_centers_fixed,
            format!("centers_fixed disagreement on {}", token),
        );
        if oracle_centers_fixed {
            centers_fixed_count += 1;
        }
        count += 1;
    }
    passes("generator tables vs oracle", count);
    println!("INFO oracle says {} of {} tokens keep centers fixed", centers_fixed_count, count);
}

fn check_vendor_algs(root: &str) {
    let lines = read_lines(&format!("{}/playground/attacker-a/gen/expected_algs.tsv", root));
    let mut count = 0;
    let mut qtm_ok = 0;
    let mut htm_diff = 0;
    for line in &lines[1..] {
        let parts: Vec<&str> = line.split('\t').collect();
        let (solver, sheet, rowid, position, alg_text, _moves, qtm, occ_field) = (
            parts[0], parts[1], parts[2], parts[3], parts[4], parts[5], parts[6], parts[7],
        );
        let expected = parse_csv54(occ_field);
        let alg = match Alg::parse(alg_text) {
            Ok(alg) => alg,
            Err(e) => {
                fail(format!("vendor alg rejected {} {}/{}/{}/{}: {}", solver, sheet, rowid, position, alg_text, e));
                count += 1;
                continue;
            }
        };
        let occ = occ_of_alg(&alg);
        if occ != expected {
            let mut diffs = Vec::new();
            for i in 0..54usize {
                if occ[i] != expected[i] {
                    diffs.push(format!("pos{} crate={} oracle={}", i, occ[i], expected[i]));
                }
            }
            fail(format!(
                "vendor alg state mismatch {} sheet {} alg [{}] first diffs: {}",
                solver,
                sheet,
                alg_text,
                diffs.iter().take(4).cloned().collect::<Vec<_>>().join(" ")
            ));
        }
        let qtm_expected: u32 = qtm.parse().unwrap();
        if alg.qtm() == qtm_expected {
            qtm_ok += 1;
        } else {
            fail(format!(
                "qtm mismatch {} {}/{}/{} alg [{}] crate={} oracle={}",
                solver,
                sheet,
                rowid,
                position,
                alg_text,
                alg.qtm(),
                qtm_expected
            ));
        }
        let moves_expected: u32 = _moves.parse().unwrap();
        if alg.htm() != moves_expected {
            htm_diff += 1;
        }
        count += 1;
    }
    passes("vendor alg states vs oracle", count);
    passes("vendor qtm vs simcheck", qtm_ok);
    println!("INFO vendor htm differs from simcheck token-count column on {} of {} algs (rotations excluded from htm)", htm_diff, count);
}

fn check_random_algs(root: &str) {
    let lines = read_lines(&format!("{}/playground/attacker-a/gen/expected_random.tsv", root));
    let mut count = 0;
    let mut roundtrip_ok = 0;
    let mut inverse_ok = 0;
    let mut state_api_ok = 0;
    for line in &lines[1..] {
        let parts: Vec<&str> = line.split('\t').collect();
        let alg_text = parts[1];
        let expected = parse_csv54(parts[2]);
        let alg = Alg::parse(alg_text).unwrap();
        let occ = occ_of_alg(&alg);
        if occ != expected {
            let mut diffs = Vec::new();
            for i in 0..54usize {
                if occ[i] != expected[i] {
                    diffs.push(format!("pos{} crate={} oracle={}", i, occ[i], expected[i]));
                }
            }
            fail(format!(
                "random alg state mismatch [{}] diffs: {}",
                alg_text,
                diffs.iter().take(4).cloned().collect::<Vec<_>>().join(" ")
            ));
        }
        let reparsed = Alg::parse(&alg.format()).unwrap();
        if reparsed.moves() == alg.moves() {
            roundtrip_ok += 1;
        } else {
            fail(format!("format round trip changed moves [{}]->[{}]", alg_text, alg.format()));
        }
        let mut cube = Cube::solved();
        cube.apply_alg(&alg);
        cube.apply_alg(&alg.inverse());
        if cube.is_solved() {
            inverse_ok += 1;
        } else {
            fail(format!("alg then inverse not solved [{}]", alg_text));
        }
        let mut forward = Cube::solved();
        forward.apply_alg(&alg);
        let mut backward = Cube::solved();
        backward.apply_alg(&alg.inverse());
        let state_ok = forward.inverse_state() == backward
            && (0..54u8).all(|s| forward.sticker_at(forward.position_of(s)) == s)
            && (0..54u8).all(|p| forward.position_of(forward.sticker_at(p)) == p);
        if state_ok {
            state_api_ok += 1;
        } else {
            fail(format!("state api properties broke on [{}]", alg_text));
        }
        let mut rebuilt = [0u8; 54];
        for sticker in 0..54u8 {
            rebuilt[forward.position_of(sticker) as usize] = sticker;
        }
        let rebuilt_cube = Cube::from_occ(rebuilt);
        if rebuilt_cube != forward {
            fail(format!("from_occ rebuild diverges on [{}]", alg_text));
        }
        count += 1;
    }
    passes("random full-dialect states vs oracle", count);
    passes("random format round trip", roundtrip_ok);
    passes("random inverse restores solved", inverse_ok);
    passes("inverse_state position_of from_occ on rotated states", state_api_ok);
}

fn expected_head(piece_name: &str, lines: &[String]) -> u8 {
    let mut members: Vec<(u8, String)> = Vec::new();
    for line in &lines[1..] {
        let parts: Vec<&str> = line.split('\t').collect();
        let sticker: u8 = parts[1].parse().unwrap();
        let name = parts[3].to_string();
        let mut a = piece_name.as_bytes().to_vec();
        a.sort_unstable();
        let mut b = name.as_bytes().to_vec();
        b.sort_unstable();
        if a == b {
            members.push((sticker, name));
        }
    }
    let ud: Vec<u8> = members
        .iter()
        .filter(|(_, n)| matches!(n.as_bytes()[0], b'U' | b'D'))
        .map(|(s, _)| *s)
        .collect();
    if ud.len() == 1 {
        return ud[0];
    }
    let fb: Vec<u8> = members
        .iter()
        .filter(|(_, n)| matches!(n.as_bytes()[0], b'F' | b'B'))
        .map(|(s, _)| *s)
        .collect();
    fb[0]
}

fn check_scheme(root: &str) {
    let lines = read_lines(&format!("{}/ref_ground/position_map.tsv", root));
    let mut count = 0;
    for line in &lines[1..] {
        let parts: Vec<&str> = line.split('\t').collect();
        let orbit = parts[0];
        let sticker: u8 = parts[1].parse().unwrap();
        let letter = parts[2].chars().next().unwrap();
        let name = parts[3];
        expect(
            letter_of(sticker) == letter,
            format!("letter_of({}) = {} oracle {}", sticker, letter_of(sticker), letter),
        );
        expect(
            name_of(sticker) == name,
            format!("name_of({}) = {} oracle {}", sticker, name_of(sticker), name),
        );
        let orb = if orbit == "corner" { Orbit::Corner } else { Orbit::Edge };
        expect(
            sticker_of(orb, letter) == sticker,
            format!("sticker_of({:?},{}) = {} oracle {}", orbit, letter, sticker_of(orb, letter), sticker),
        );
        count += 1;
    }
    passes("scheme letters names ids vs position_map", count);

    let inv = read_lines(&format!("{}/ref_ground/position_map_inverse.tsv", root));
    let mut icount = 0;
    for line in &inv[1..] {
        let parts: Vec<&str> = line.split('\t').collect();
        let name = parts[0];
        let sticker: u8 = parts[2].parse().unwrap();
        expect(
            name_of(sticker) == name,
            format!("inverse map name_of({}) = {} oracle {}", sticker, name_of(sticker), name),
        );
        icount += 1;
    }
    passes("scheme inverse map", icount);

    let mut piece_groups: Vec<((String, u8), Vec<u8>)> = Vec::new();
    for line in &lines[1..] {
        let parts: Vec<&str> = line.split('\t').collect();
        let sticker: u8 = parts[1].parse().unwrap();
        let piece: u8 = parts[7].parse().unwrap();
        let key = (parts[0].to_string(), piece);
        match piece_groups.iter_mut().find(|(p, _)| *p == key) {
            Some((_, v)) => v.push(sticker),
            None => piece_groups.push((key, vec![sticker])),
        }
    }
    let mut gcount = 0;
    for (_, group) in &piece_groups {
        for &a in group {
            for &b in group {
                expect(same_piece(a, b), format!("same_piece({},{}) false, same oracle piece", a, b));
                gcount += 1;
            }
        }
    }
    let mut neg = 0;
    for (pa, ga) in &piece_groups {
        for (pb, gb) in &piece_groups {
            if pa == pb {
                continue;
            }
            for &a in ga {
                for &b in gb {
                    expect(!same_piece(a, b), format!("same_piece({},{}) true, different oracle pieces", a, b));
                    neg += 1;
                }
            }
        }
    }
    passes("same_piece partition vs oracle pieces", gcount + neg);

    expect(EDGE_BUFFER_STICKER == 26, "edge buffer sticker".into());
    expect(CORNER_BUFFER_STICKER == 2, "corner buffer sticker".into());
    expect(letter_of(26) == 'C' && name_of(26) == "UF", "edge buffer letter/name".into());
    expect(letter_of(2) == 'C' && name_of(2) == "UFR", "corner buffer letter/name".into());

    let mut head_count = 0;
    for line in &lines[1..] {
        let parts: Vec<&str> = line.split('\t').collect();
        let name = parts[3];
        let expected = expected_head(name, &lines);
        let got = head_sticker(name);
        expect(got == expected, format!("head_sticker({}) = {} oracle rule says {}", name, got, expected));
        head_count += 1;
    }
    passes("head sticker rule vs oracle derivation", head_count);

    let edge_floating = bt_scheme::EDGE_FLOATING;
    let corner_floating = bt_scheme::CORNER_FLOATING;
    let doc_edges = ["UF", "UB", "UR", "UL", "FR", "FL", "DF", "DB", "DR", "DL", "BR", "BL"];
    let doc_corners = ["UFR", "UFL", "UBR", "UBL", "DFR", "DFL", "DBR", "DBL"];
    expect(edge_floating == doc_edges, format!("EDGE_FLOATING order {:?}", edge_floating));
    expect(corner_floating == doc_corners, format!("CORNER_FLOATING order {:?}", corner_floating));
    let heads = bt_scheme::edge_floating_heads();
    for (i, name) in doc_edges.iter().enumerate() {
        expect(
            heads[i] == expected_head(name, &lines),
            format!("edge floating head {} = {}", name, heads[i]),
        );
    }
    let cheads = bt_scheme::corner_floating_heads();
    for (i, name) in doc_corners.iter().enumerate() {
        expect(
            cheads[i] == expected_head(name, &lines),
            format!("corner floating head {} = {}", name, cheads[i]),
        );
    }
    passes("floating arrays and heads", doc_edges.len() + doc_corners.len());
}

fn check_pairs(root: &str) {
    for (orbit, path) in [
        (Orbit::Edge, format!("{}/ref_ground/pairs_edge2.tsv", root)),
        (Orbit::Corner, format!("{}/ref_ground/pairs_corner2.tsv", root)),
    ] {
        let lines = read_lines(&path);
        let cases = enumerate_cases(orbit);
        expect(
            cases.len() == lines.len() - 1,
            format!("{:?} case count {} oracle {}", orbit, cases.len(), lines.len() - 1),
        );
        let mut mism = 0;
        for (line, case) in lines[1..].iter().zip(cases.iter()) {
            let parts: Vec<&str> = line.split('\t').collect();
            let t1: u8 = parts[0].parse().unwrap();
            let t2: u8 = parts[1].parse().unwrap();
            let letters = parts[2];
            let (l1, l2) = case.letters();
            if case.t1 != t1 || case.t2 != t2 || format!("{}{}", l1, l2) != letters {
                mism += 1;
                if mism <= 5 {
                    fail(format!(
                        "{:?} case mismatch oracle ({},{},{}) crate ({},{},{},{})",
                        orbit, t1, t2, letters, case.t1, case.t2, l1, l2
                    ));
                }
            }
        }
        if mism == 0 {
            passes(&format!("{:?} pair space exact", orbit), cases.len());
        } else {
            fail(format!("{:?} pair space mismatches {}", orbit, mism));
        }
    }
}

fn check_long_algs(root: &str) {
    let lines = read_lines(&format!("{}/playground/attacker-a/gen/expected_long.tsv", root));
    let mut count = 0;
    for line in &lines[1..] {
        let parts: Vec<&str> = line.split('\t').collect();
        let alg_text = parts[1];
        let expected = parse_csv54(parts[2]);
        let alg = Alg::parse(alg_text).unwrap();
        let occ = occ_of_alg(&alg);
        if occ != expected {
            let mut diffs = Vec::new();
            for i in 0..54usize {
                if occ[i] != expected[i] {
                    diffs.push(format!("pos{} crate={} oracle={}", i, occ[i], expected[i]));
                }
            }
            fail(format!(
                "long alg state mismatch len {} first diffs: {}",
                alg.len(),
                diffs.iter().take(4).cloned().collect::<Vec<_>>().join(" ")
            ));
        }
        count += 1;
    }
    passes("long alg states vs oracle", count);
}

fn check_parse_adversarial() {
    let accepted: Vec<(&str, &str)> = vec![
        ("U", "U"),
        ("U2", "U2"),
        ("U'", "U'"),
        ("U2'", "U2"),
        ("r2'", "r2"),
        ("2F2'", "2F2"),
        ("M2'", "M2"),
        ("x2'", "x2"),
        ("2U'", "2U'"),
        ("E2'", "E2"),
        ("z'", "z'"),
        ("l", "l"),
        ("2U2'", "2U2"),
        ("U\u{00a0}", "U"),
        (" U", "U"),
        ("U ", "U"),
        ("\u{2028}U", "U"),
    ];
    for (input, canonical) in &accepted {
        let alg = Alg::parse(input);
        expect(alg.is_ok(), format!("parse({:?}) rejected", input));
        if let Ok(alg) = alg {
            expect(
                alg.format() == *canonical,
                format!("format(parse({:?})) = {} expected {}", input, alg.format(), canonical),
            );
        }
    }
    passes("canonical acceptance table", accepted.len());

    let rejected = [
        "V", "U3", "Uu", "2u", "2", "2W", "2M", "2x", "m", "e", "s", "X", "Y", "Z", "U''", "U'''",
        "U2x", "2UU", "2U''", "U5", "r3", "Mx", "x2''", "U-", "U+", "U*2", "U2U", "U'2",
        "U\u{2019}", "U\u{2032}", "22", "0U", "U0", "w", "N", "u2'u", "2f'", "P", "Q", "T",
        "U,", ".U", "U.", "\u{FF25}", "\u{FF55}", "\u{FF38}", "\u{2161}", "\u{30C6}", "\u{00Dc}",
        "\u{00FC}", "\u{016C}", "U\u{0301}",
    ];
    let mut rcount = 0;
    for input in &rejected {
        let alg = Alg::parse(input);
        expect(alg.is_err(), format!("parse({:?}) accepted, expected reject", input));
        rcount += 1;
    }
    passes("rejection table", rcount);

    let empty_inputs = ["", " ", "\t", "  \n\r ", "\u{00a0}", "\u{2003}\u{2028}"];
    for input in &empty_inputs {
        let alg = Alg::parse(input);
        expect(
            alg.as_ref().map(|a| a.is_empty()).unwrap_or(false),
            format!("whitespace-only {:?} not empty alg", input),
        );
        expect(
            alg.map(|a| a.format().is_empty()).unwrap_or(false),
            format!("whitespace-only {:?} format not empty", input),
        );
    }
    passes("empty and whitespace handling", empty_inputs.len());

    let sep_inputs = ["R  U", "R\tU", "R\nU", "R\u{000b}U", "R\u{00a0}U", "R\u{2003}U"];
    for input in &sep_inputs {
        let alg = Alg::parse(input);
        expect(alg.is_ok() && alg.unwrap().len() == 2, format!("separator handling {:?}", input));
    }
    passes("whitespace separators", sep_inputs.len());

    let default_hook = std::panic::take_hook();
    std::panic::set_hook(Box::new(|_| {}));
    let mut rng = XorShift(0x9E3779B97F4A7C15);
    let alphabet: Vec<char> = "UDLRFBudlrfbMESxyz'2\"03589 \t\u{00a0}\u{2019}-[](),".chars().collect();
    let mut fuzz_count = 0;
    let mut accepted_count = 0;
    for _ in 0..200000 {
        let len = rng.below(24);
        let s: String = (0..len).map(|_| alphabet[rng.below(alphabet.len())]).collect();
        let outcome = catch_unwind(AssertUnwindSafe(|| Alg::parse(&s)));
        match outcome {
            Err(_) => fail(format!("parse panicked on {:?}", s)),
            Ok(Err(_)) => {}
            Ok(Ok(alg)) => {
                accepted_count += 1;
                let re = Alg::parse(&alg.format());
                expect(
                    re.is_ok() && re.unwrap().moves() == alg.moves(),
                    format!("round trip failed on {:?}", s),
                );
            }
        }
        fuzz_count += 1;
    }
    std::panic::set_hook(default_hook);
    passes("parse fuzz no panic + round trip", fuzz_count);
    println!("INFO fuzz accepted {} of {} strings", accepted_count, fuzz_count);
}

fn all_moves() -> Vec<Move> {
    let mut out = Vec::new();
    for base in "UDLRFB".chars() {
        let face = face_of_str(&base.to_string());
        for q in 1..=3u8 {
            out.push(Move::Face { face, quarter: q });
            out.push(Move::Wide { face, quarter: q });
            out.push(Move::Inner { face, quarter: q });
        }
    }
    for slice in [Slice::M, Slice::E, Slice::S] {
        for q in 1..=3u8 {
            out.push(Move::Slice { slice, quarter: q });
        }
    }
    for axis in [Axis::X, Axis::Y, Axis::Z] {
        for q in 1..=3u8 {
            out.push(Move::Rotation { axis, quarter: q });
        }
    }
    out
}

fn check_constructed_roundtrip() {
    let moves = all_moves();
    let mut count = 0;
    let mut rng = XorShift(0xDEADBEEFCAFEF00D);
    for _ in 0..20000 {
        let n = 1 + rng.below(8);
        let list: Vec<Move> = (0..n).map(|_| moves[rng.below(moves.len())]).collect();
        let alg = Alg::new(list);
        let re = Alg::parse(&alg.format());
        expect(
            re.is_ok() && re.unwrap().moves() == alg.moves(),
            format!("constructed round trip failed on {}", alg.format()),
        );
        let mut cube = Cube::solved();
        cube.apply_alg(&alg);
        cube.apply_alg(&alg.inverse());
        expect(cube.is_solved(), format!("constructed inverse failed on {}", alg.format()));
        count += 1;
    }
    passes("constructed move list round trip and inverse", count);
}

fn check_crate_pins(root: &str) {
    let lines = read_lines(&format!("{}/playground/attacker-a/gen/expected_generators.tsv", root));
    let mut oracle = std::collections::HashMap::new();
    for line in &lines[1..] {
        let mut parts = line.split('\t');
        let token = parts.next().unwrap().to_string();
        let table = parse_csv54(parts.next().unwrap());
        oracle.insert(token, table);
    }
    let pins: Vec<(&str, Vec<&str>)> = vec![
        ("z", vec!["F", "S", "B'"]),
        ("z2", vec!["F2", "S2", "B2"]),
        ("z'", vec!["F'", "S'", "B"]),
        ("x", vec!["R", "M'", "L'"]),
        ("x2", vec!["R2", "M2", "L2"]),
        ("x'", vec!["R'", "M", "L"]),
        ("y", vec!["U", "E'", "D'"]),
        ("y2", vec!["U2", "E2", "D2"]),
        ("y'", vec!["U'", "E", "D"]),
        ("r", vec!["R", "2R"]),
        ("u", vec!["U", "2U"]),
        ("l", vec!["L", "2L"]),
        ("f", vec!["F", "2F"]),
        ("M", vec!["2L"]),
        ("E", vec!["2D"]),
        ("S", vec!["2F"]),
    ];
    let mut count = 0;
    for (single, parts) in &pins {
        let mut list: Vec<Move> = Vec::new();
        for p in parts {
            list.extend_from_slice(Alg::parse(p).unwrap().moves());
        }
        let table = table_of_alg(&Alg::new(list));
        let expected = oracle[*single];
        expect(table == expected, format!("crate pin {} != {}", single, parts.join(" ")));
        count += 1;
    }
    passes("crate composition pins vs oracle single-move tables", count);
}

fn check_invalid_quarters() {
    let default_hook = std::panic::take_hook();
    std::panic::set_hook(Box::new(|_| {}));
    let outcomes = [
        catch_unwind(AssertUnwindSafe(|| Move::Face { face: Face::U, quarter: 0 }.table())),
        catch_unwind(AssertUnwindSafe(|| Move::Face { face: Face::U, quarter: 4 }.table())),
        catch_unwind(AssertUnwindSafe(|| Move::Slice { slice: Slice::M, quarter: 7 }.table())),
        catch_unwind(AssertUnwindSafe(|| Move::Rotation { axis: Axis::Y, quarter: 255 }.table())),
    ];
    std::panic::set_hook(default_hook);
    let labels = ["Face quarter 0", "Face quarter 4", "Slice quarter 7", "Rotation quarter 255"];
    let mut panics = 0;
    for (i, o) in outcomes.iter().enumerate() {
        if o.is_err() {
            panics += 1;
        } else {
            println!("INFO invalid quarter [{}] produced a table without panic", labels[i]);
        }
    }
    println!("INFO invalid quarters via public constructors: {} of 4 panic", panics);
}

fn main() {
    let root = std::env::args().nth(1).expect("repo root argument");
    check_generators(&root);
    check_vendor_algs(&root);
    check_random_algs(&root);
    check_long_algs(&root);
    check_scheme(&root);
    check_pairs(&root);
    check_parse_adversarial();
    check_constructed_roundtrip();
    check_crate_pins(&root);
    check_invalid_quarters();
    let failures = FAILURES.load(Ordering::Relaxed);
    let checks = CHECKS.load(Ordering::Relaxed);
    println!("DONE checks={} failures={}", checks, failures);
    if failures > 0 {
        std::process::exit(1);
    }
}
