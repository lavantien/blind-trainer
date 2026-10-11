mod common;

use bt_core::{Alg, Cube};
use std::collections::HashMap;

struct NameMap {
    sticker: HashMap<String, u8>,
    piece: [usize; 54],
}

fn sticker_names() -> NameMap {
    let lines = common::sheet_lines("position_map.tsv");
    let header: Vec<&str> = lines[0].split('\t').collect();
    assert_eq!(
        header,
        vec![
            "orbit", "sticker", "letter", "name", "face", "row", "col", "piece"
        ]
    );
    let mut map = NameMap {
        sticker: HashMap::new(),
        piece: std::array::from_fn(|i| 200 + i),
    };
    for line in lines.iter().skip(1) {
        if line.is_empty() {
            continue;
        }
        let cols: Vec<&str> = line.split('\t').collect();
        assert_eq!(cols.len(), 8, "{:?}", line);
        let sticker: usize = cols[1].parse().unwrap();
        let piece: usize = cols[7].parse().unwrap();
        map.piece[sticker] = if cols[0] == "corner" {
            piece
        } else {
            100 + piece
        };
        let prev = map.sticker.insert(cols[3].to_string(), sticker as u8);
        assert!(prev.is_none(), "duplicate name {}", cols[3]);
    }
    assert_eq!(map.sticker.len(), 48);
    map
}

#[test]
fn clean_cycle_rows_are_pure_piece_three_cycles() {
    let names = sticker_names();
    let rows = common::simcheck_rows();
    let mut sheets: HashMap<String, Vec<String>> = HashMap::new();
    let mut checked = 0;
    for r in &rows {
        if r[6] != "CYCLE_A" || !r[7].contains("clean") {
            continue;
        }
        let key = format!("{}/{}.tsv", r[0], r[1]);
        let lines = sheets
            .entry(key.clone())
            .or_insert_with(|| common::sheet_lines(&key));
        let rowid: usize = r[2].parse().unwrap();
        let alg_text = lines[rowid - 1].split('\t').nth(1).unwrap();
        let alg = Alg::parse(alg_text).unwrap_or_else(|e| panic!("{} row {}: {}", key, rowid, e));
        let tokens: Vec<&str> = r[3].split('-').collect();
        assert_eq!(tokens.len(), 3, "{} row {}", key, rowid);
        let a = names.sticker[tokens[0]] as usize;
        let b = names.sticker[tokens[1]] as usize;
        let c = names.sticker[tokens[2]] as usize;
        let (pa, pb, pc) = (names.piece[a], names.piece[b], names.piece[c]);
        assert_ne!(pa, pb);
        assert_ne!(pb, pc);
        assert_ne!(pa, pc);
        let mut cube = Cube::solved();
        cube.apply_alg(&alg);
        let ctx = format!(
            "{} row {} {:?} {}-{}-{}",
            key, rowid, alg_text, tokens[0], tokens[1], tokens[2]
        );
        assert_eq!(cube.position_of(a as u8) as usize, b, "{}", ctx);
        assert_eq!(cube.position_of(b as u8) as usize, c, "{}", ctx);
        assert_eq!(cube.position_of(c as u8) as usize, a, "{}", ctx);
        for sticker in 0..54usize {
            let dest = cube.position_of(sticker as u8) as usize;
            let expected_piece = match names.piece[sticker] {
                p if p == pa => pb,
                p if p == pb => pc,
                p if p == pc => pa,
                _ => {
                    assert_eq!(dest, sticker, "{}", ctx);
                    continue;
                }
            };
            assert_eq!(names.piece[dest], expected_piece, "{}", ctx);
        }
        checked += 1;
    }
    assert_eq!(checked, 208);
}
