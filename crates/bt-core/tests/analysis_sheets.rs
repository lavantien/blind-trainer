mod common;

use bt_core::{Alg, Cube, Piece, head_sticker, piece_of, summarize};
use common::NameIndex;
use std::collections::HashMap;

type Sheets = HashMap<String, Vec<String>>;
type Swap = (String, String);
type CornerNote = (String, String, i8, Vec<Swap>);

fn sheet_alg(sheets: &mut Sheets, solver: &str, sheet: &str, rowid: usize) -> String {
    let key = format!("{}/{}.tsv", solver, sheet);
    let lines = sheets
        .entry(key.clone())
        .or_insert_with(|| common::sheet_lines(&key));
    assert!(rowid >= 1 && rowid < lines.len(), "{} row {}", key, rowid);
    lines[rowid - 1].split('\t').nth(1).unwrap().to_string()
}

fn apply(text: &str) -> Cube {
    let mut cube = Cube::solved();
    cube.apply_alg(&Alg::parse(text).unwrap());
    cube
}

fn sticker_id(names: &NameIndex, name: &str) -> u8 {
    *names
        .sticker_of_name
        .get(name)
        .unwrap_or_else(|| panic!("unknown sticker name {}", name))
}

fn piece_id(names: &NameIndex, name: &str) -> u8 {
    match piece_of(sticker_id(names, name)) {
        Piece::Corner(piece) | Piece::Edge(piece) => piece,
    }
}

fn piece_name(names: &NameIndex, piece: Piece) -> &str {
    &names.name_of_sticker[head_sticker(piece) as usize]
}

#[derive(Default)]
struct Defects {
    edge_flips: HashMap<u8, bool>,
    corner_twists: HashMap<u8, i8>,
}

impl Defects {
    fn record(&mut self, names: &NameIndex, name: &str, value: &str) {
        match value {
            "ok" => {
                self.edge_flips.insert(piece_id(names, name), false);
            }
            "flip" => {
                self.edge_flips.insert(piece_id(names, name), true);
            }
            "+0" | "-0" => {
                self.corner_twists.insert(piece_id(names, name), 0);
            }
            "+1" => {
                self.corner_twists.insert(piece_id(names, name), 1);
            }
            "-1" => {
                self.corner_twists.insert(piece_id(names, name), -1);
            }
            _ => panic!("unknown defect value {}", value),
        }
    }

    fn assert_matches(&self, cube: &Cube, names: &NameIndex, ctx: &str) {
        let summary = summarize(cube);
        for slot in 0..8usize {
            let piece = summary.corners[slot].piece;
            let expected = self.corner_twists.get(&piece).copied().unwrap_or(0);
            assert_eq!(
                summary.corners[slot].twist,
                expected,
                "{} corner slot {} piece {}",
                ctx,
                slot,
                piece_name(names, Piece::Corner(piece))
            );
        }
        for slot in 0..12usize {
            let piece = summary.edges[slot].piece;
            let expected = self.edge_flips.get(&piece).copied().unwrap_or(false);
            assert_eq!(
                summary.edges[slot].flipped,
                expected,
                "{} edge slot {} piece {}",
                ctx,
                slot,
                piece_name(names, Piece::Edge(piece))
            );
        }
    }
}

fn parse_defects(names: &NameIndex, note: &str) -> Defects {
    let rest = note.strip_prefix("defects:").unwrap_or(note);
    let mut defects = Defects::default();
    for token in rest.split_whitespace() {
        if token == "clean" {
            continue;
        }
        for entry in token.split(',') {
            if entry.is_empty() {
                continue;
            }
            let (name, value) = entry.split_once('=').expect(entry);
            defects.record(names, name, value);
        }
    }
    defects
}

fn bracket_pairs(inner: &str) -> Vec<(String, String)> {
    inner
        .split_whitespace()
        .map(|token| {
            let (from, to) = token.split_once('>').expect(token);
            (from.to_string(), to.to_string())
        })
        .collect()
}

fn after_token(tail: &str) -> &str {
    match tail.find(char::is_whitespace) {
        Some(i) => tail[i..].trim_start(),
        None => "",
    }
}

fn parse_ltct(note: &str) -> (Vec<Swap>, Vec<CornerNote>) {
    let parts: Vec<&str> = note.split(" | ").collect();
    assert_eq!(parts.len(), 3, "{}", note);
    let edges_part = parts[1].strip_prefix("edges: ").expect(note);
    let edge_swaps: Vec<Swap> = edges_part
        .split_whitespace()
        .map(|token| {
            let (from, to) = token.split_once("->").expect(token);
            (from.to_string(), to.to_string())
        })
        .collect();
    let corners_part = parts[2].strip_prefix("corners: ").expect(note);
    let mut corners = Vec::new();
    let mut rest = corners_part;
    while !rest.is_empty() {
        let arrow = rest.find("->").expect(rest);
        let piece = rest[..arrow].trim().to_string();
        let open = rest.find('[').expect(rest);
        let slot = rest[arrow + 2..open].trim().to_string();
        let close = rest.find(']').expect(rest);
        let pairs = bracket_pairs(&rest[open + 1..close]);
        let after = &rest[close + 1..];
        let tw_at = after.find("tw=").expect(after) + 3;
        let tail = &after[tw_at..];
        let value: &str = tail.split_whitespace().next().expect(tail);
        corners.push((piece, slot, value.parse::<i8>().expect(value), pairs));
        rest = after_token(tail);
    }
    (edge_swaps, corners)
}

fn parse_twists(note: &str) -> Vec<(Vec<(String, String)>, i8)> {
    let mut out = Vec::new();
    let mut rest = note;
    while let Some(open) = rest.find('[') {
        let close = rest.find(']').expect(rest);
        let pairs = bracket_pairs(&rest[open + 1..close]);
        assert_eq!(pairs.len(), 3, "{}", note);
        let after = &rest[close + 1..];
        let dir_at = after.find("d").expect(after);
        let tail = &after[dir_at..];
        let eq = tail.find('=').expect(tail) + 1;
        let tail = &tail[eq..];
        let value: &str = tail.split_whitespace().next().expect(tail);
        out.push((pairs, value.parse::<i8>().expect(value)));
        rest = after_token(tail);
    }
    out
}

#[test]
fn all_vendor_algs_keep_flip_parity_even_and_twist_sum_zero() {
    let rows = common::simcheck_rows();
    assert_eq!(rows.len(), 1077);
    let mut sheets: Sheets = HashMap::new();
    for row in &rows {
        let alg_text = sheet_alg(&mut sheets, &row[0], &row[1], row[2].parse().unwrap());
        let summary = summarize(&apply(&alg_text));
        let flips = summary.edges.iter().filter(|edge| edge.flipped).count();
        assert_eq!(flips % 2, 0, "{} row {} {}", row[0], row[1], row[2]);
        let twist_sum: i32 = summary
            .corners
            .iter()
            .map(|corner| corner.twist as i32)
            .sum();
        assert_eq!(
            twist_sum.rem_euclid(3),
            0,
            "{} row {} {}",
            row[0],
            row[1],
            row[2]
        );
    }
}

#[test]
fn flips_rows_flip_exactly_the_two_named_edges_in_place() {
    let names = common::position_names();
    let rows = common::simcheck_rows();
    let mut sheets: Sheets = HashMap::new();
    let mut checked = 0;
    for row in &rows {
        if row[1] != "flips" {
            continue;
        }
        let alg_text = sheet_alg(&mut sheets, &row[0], &row[1], row[2].parse().unwrap());
        let cube = apply(&alg_text);
        let ctx = format!("{} flips row {}", row[0], row[2]);
        assert!(cube.centers_fixed(), "{}", ctx);
        let pair: Vec<&str> = row[3].split(" & ").collect();
        assert_eq!(pair.len(), 2, "{}", ctx);
        let first = piece_id(&names, pair[0]);
        let second = piece_id(&names, pair[1]);
        assert_ne!(first, second, "{}", ctx);
        let summary = summarize(&cube);
        for slot in 0..12usize {
            assert_eq!(
                summary.edges[slot].piece as usize, slot,
                "{} edge slot {}",
                ctx, slot
            );
            let expected = slot == first as usize || slot == second as usize;
            assert_eq!(
                summary.edges[slot].flipped, expected,
                "{} edge slot {}",
                ctx, slot
            );
        }
        for slot in 0..8usize {
            assert_eq!(summary.corners[slot].piece as usize, slot, "{}", ctx);
            assert_eq!(summary.corners[slot].twist, 0, "{}", ctx);
        }
        checked += 1;
    }
    assert_eq!(checked, 25);
}

#[test]
fn twists_rows_match_simcheck_signed_directions() {
    let names = common::position_names();
    let rows = common::simcheck_rows();
    let mut sheets: Sheets = HashMap::new();
    let mut checked = 0;
    for row in &rows {
        if row[1] != "twists" {
            continue;
        }
        let alg_text = sheet_alg(&mut sheets, &row[0], &row[1], row[2].parse().unwrap());
        let cube = apply(&alg_text);
        let ctx = format!("{} twists row {} note {}", row[0], row[2], row[7]);
        assert!(cube.centers_fixed(), "{}", ctx);
        let summary = summarize(&cube);
        for slot in 0..8usize {
            assert_eq!(summary.corners[slot].piece as usize, slot, "{}", ctx);
        }
        for slot in 0..12usize {
            assert_eq!(summary.edges[slot].piece as usize, slot, "{}", ctx);
            assert!(!summary.edges[slot].flipped, "{}", ctx);
        }
        let cycles = parse_twists(&row[7]);
        assert_eq!(cycles.len(), 2, "{}", ctx);
        assert_eq!(cycles[0].1, -cycles[1].1, "{}", ctx);
        let mut twisted_slots = Vec::new();
        for (pairs, dir) in &cycles {
            let slot = piece_id(&names, &pairs[0].0) as usize;
            assert_eq!(summary.corners[slot].twist, *dir, "{}", ctx);
            twisted_slots.push(slot);
            for (from, to) in pairs {
                assert_eq!(
                    cube.position_of(sticker_id(&names, from)),
                    sticker_id(&names, to),
                    "{} {}>{}",
                    ctx,
                    from,
                    to
                );
            }
        }
        assert_ne!(twisted_slots[0], twisted_slots[1], "{}", ctx);
        for slot in 0..8usize {
            if !twisted_slots.contains(&slot) {
                assert_eq!(summary.corners[slot].twist, 0, "{} slot {}", ctx, slot);
            }
        }
        checked += 1;
    }
    assert_eq!(checked, 25);
}

#[test]
fn defect_notes_agree_with_orientation_rules() {
    let names = common::position_names();
    let rows = common::simcheck_rows();
    let mut sheets: Sheets = HashMap::new();
    let mut checked = 0;
    for row in &rows {
        if !matches!(row[1].as_str(), "edge" | "corner" | "parity")
            || !row[7].starts_with("defects")
        {
            continue;
        }
        let alg_text = sheet_alg(&mut sheets, &row[0], &row[1], row[2].parse().unwrap());
        let cube = apply(&alg_text);
        let ctx = format!("{} {} row {} note {}", row[0], row[1], row[2], row[7]);
        let defects = parse_defects(&names, &row[7]);
        defects.assert_matches(&cube, &names, &ctx);
        checked += 1;
    }
    assert_eq!(checked, 366 + 622 + 28);
}

#[test]
fn ltct_notes_agree_with_orientation_rules() {
    let names = common::position_names();
    let rows = common::simcheck_rows();
    let mut sheets: Sheets = HashMap::new();
    let mut checked = 0;
    for row in &rows {
        if row[1] != "ltct" {
            continue;
        }
        let alg_text = sheet_alg(&mut sheets, &row[0], &row[1], row[2].parse().unwrap());
        let cube = apply(&alg_text);
        let ctx = format!("{} ltct row {} note {}", row[0], row[2], row[7]);
        assert!(cube.centers_fixed(), "{}", ctx);
        let summary = summarize(&cube);
        let (edge_swaps, corners) = parse_ltct(&row[7]);
        assert_eq!(edge_swaps.len(), 2, "{}", ctx);
        assert_eq!(corners.len(), 3, "{}", ctx);
        let mut expected_pieces: HashMap<usize, usize> = HashMap::new();
        for (from, to) in &edge_swaps {
            expected_pieces.insert(
                piece_id(&names, to) as usize,
                piece_id(&names, from) as usize,
            );
        }
        for slot in 0..12usize {
            let expected = expected_pieces.get(&slot).copied().unwrap_or(slot);
            assert_eq!(
                summary.edges[slot].piece as usize, expected,
                "{} slot {}",
                ctx, slot
            );
            assert!(!summary.edges[slot].flipped, "{} slot {}", ctx, slot);
        }
        let twist_sum: i32 = corners.iter().map(|(_, _, dir, _)| *dir as i32).sum();
        assert_eq!(twist_sum.rem_euclid(3), 0, "{}", ctx);
        let mut twisted_slots = Vec::new();
        for (piece_name, slot_name, dir, moves) in &corners {
            let piece = piece_id(&names, piece_name) as usize;
            let slot = piece_id(&names, slot_name) as usize;
            assert_eq!(summary.corners[slot].piece as usize, piece, "{}", ctx);
            assert_eq!(summary.corners[slot].twist, *dir, "{}", ctx);
            twisted_slots.push(slot);
            for (from, to) in moves {
                assert_eq!(
                    cube.position_of(sticker_id(&names, from)),
                    sticker_id(&names, to),
                    "{} {}>{}",
                    ctx,
                    from,
                    to
                );
            }
        }
        for slot in 0..8usize {
            if !twisted_slots.contains(&slot) {
                assert_eq!(
                    summary.corners[slot].piece as usize, slot,
                    "{} slot {}",
                    ctx, slot
                );
                assert_eq!(summary.corners[slot].twist, 0, "{} slot {}", ctx, slot);
            }
        }
        checked += 1;
    }
    assert_eq!(checked, 11);
}

#[test]
fn parse_twists_handles_the_note_shapes() {
    let note = "cyc1=[UFR>FUR FUR>RUF RUF>UFR] d1=+1 cyc2=[UBR>BUR RUB>UBR BUR>RUB] d2=-1";
    let parsed = parse_twists(note);
    assert_eq!(parsed.len(), 2);
    assert_eq!(parsed[0].1, 1);
    assert_eq!(parsed[1].1, -1);
    assert_eq!(parsed[0].0[0], ("UFR".to_string(), "FUR".to_string()));

    let ltct = "named e=UF/UR c=UBR/LUB[FUR] | edges: UR->UF UF->UR | corners: UBL->BUR [UBL>BUR LUB>RUB BUL>UBR] tw=-1 UFR->LUB [UFR>LUB FUR>UBL RUF>BUL] tw=-1 UFR->RUF [UFR>RUF FUR>UFR RUF>FUR] tw=-1";
    let (edges, corners) = parse_ltct(ltct);
    assert_eq!(edges.len(), 2);
    assert_eq!(corners.len(), 3);
    assert_eq!(corners[0].0, "UBL");
    assert_eq!(corners[0].1, "BUR");
    assert_eq!(corners[0].2, -1);
    assert_eq!(corners[2].2, -1);
    assert_eq!(corners[0].3.len(), 3);
}
