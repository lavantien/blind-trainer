#![allow(dead_code)]

use proptest::prelude::*;
use proptest::sample::select;
use std::collections::HashMap;
use std::path::PathBuf;

pub struct NameIndex {
    pub name_of_sticker: [String; 48],
    pub piece_of_sticker: [u8; 48],
    pub sticker_of_name: HashMap<String, u8>,
}

impl NameIndex {
    pub fn corner_stickers(&self) -> [[u8; 3]; 8] {
        let mut groups: Vec<Vec<u8>> = vec![Vec::new(); 8];
        for sticker in 0..24u8 {
            groups[self.piece_of_sticker[sticker as usize] as usize].push(sticker);
        }
        let mut out = [[0u8; 3]; 8];
        for (piece, group) in groups.into_iter().enumerate() {
            out[piece] = [group[0], group[1], group[2]];
        }
        out
    }

    pub fn edge_stickers(&self) -> [[u8; 2]; 12] {
        let mut groups: Vec<Vec<u8>> = vec![Vec::new(); 12];
        for sticker in 24..48u8 {
            groups[self.piece_of_sticker[sticker as usize] as usize].push(sticker);
        }
        let mut out = [[0u8; 2]; 12];
        for (piece, group) in groups.into_iter().enumerate() {
            out[piece] = [group[0], group[1]];
        }
        out
    }
}

pub fn position_names() -> NameIndex {
    let lines = sheet_lines("position_map.tsv");
    let header: Vec<&str> = lines[0].split('\t').collect();
    assert_eq!(
        header,
        vec![
            "orbit", "sticker", "letter", "name", "face", "row", "col", "piece"
        ]
    );
    let mut index = NameIndex {
        name_of_sticker: std::array::from_fn(|_| String::new()),
        piece_of_sticker: [255u8; 48],
        sticker_of_name: HashMap::new(),
    };
    for line in lines.iter().skip(1) {
        if line.is_empty() {
            continue;
        }
        let cols: Vec<&str> = line.split('\t').collect();
        assert_eq!(cols.len(), 8, "{:?}", line);
        let sticker: usize = cols[1].parse().unwrap();
        let piece: u8 = cols[7].parse().unwrap();
        if cols[0] == "corner" {
            assert!(sticker < 24 && piece < 8, "{:?}", line);
        } else {
            assert_eq!(cols[0], "edge", "{:?}", line);
            assert!((24..48).contains(&sticker) && piece < 12, "{:?}", line);
        }
        index.name_of_sticker[sticker] = cols[3].to_string();
        index.piece_of_sticker[sticker] = piece;
        let prev = index
            .sticker_of_name
            .insert(cols[3].to_string(), sticker as u8);
        assert!(prev.is_none(), "duplicate name {}", cols[3]);
    }
    assert_eq!(index.sticker_of_name.len(), 48);
    assert!(index.piece_of_sticker.iter().all(|&p| p != 255));
    index
}

pub const SHEETS: [(&str, usize); 12] = [
    ("cherry/edge.tsv", 551),
    ("cherry/corner.tsv", 328),
    ("cherry/flips.tsv", 25),
    ("cherry/twists.tsv", 20),
    ("cherry/parity.tsv", 26),
    ("cherry/ltct.tsv", 8),
    ("eggins/edge.tsv", 71),
    ("eggins/corner.tsv", 38),
    ("eggins/flips.tsv", 0),
    ("eggins/twists.tsv", 5),
    ("eggins/parity.tsv", 2),
    ("eggins/ltct.tsv", 3),
];

pub fn ref_ground() -> PathBuf {
    let raw = std::env::var("BT_REF_GROUND")
        .unwrap_or_else(|_| panic!("BT_REF_GROUND is unset: run tests through make"));
    let path = PathBuf::from(raw);
    if path.is_absolute() {
        return path;
    }
    let manifest = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let repo_root = manifest
        .parent()
        .and_then(|p| p.parent())
        .expect("workspace root from manifest dir");
    repo_root.join(path)
}

pub fn sheet_lines(rel: &str) -> Vec<String> {
    let path = ref_ground().join(rel);
    let text =
        std::fs::read_to_string(&path).unwrap_or_else(|e| panic!("read {}: {}", path.display(), e));
    text.split('\n')
        .map(|l| l.trim_end_matches('\r').to_string())
        .collect()
}

pub fn vendor_rows() -> Vec<(String, String)> {
    let mut rows = Vec::new();
    for (rel, expected) in SHEETS {
        let lines = sheet_lines(rel);
        assert!(
            lines[0].starts_with("Position"),
            "{} header: {:?}",
            rel,
            lines[0]
        );
        let mut count = 0;
        for line in lines.iter().skip(1) {
            if line.is_empty() {
                continue;
            }
            let cols: Vec<&str> = line.split('\t').collect();
            assert_eq!(cols.len(), 5, "{}: {:?}", rel, line);
            rows.push((rel.to_string(), cols[1].to_string()));
            count += 1;
        }
        assert_eq!(count, expected, "{}", rel);
    }
    rows
}

pub fn simcheck_rows() -> Vec<Vec<String>> {
    let lines = sheet_lines("simcheck.tsv");
    let header: Vec<&str> = lines[0].split('\t').collect();
    assert_eq!(
        header,
        vec![
            "solver", "sheet", "rowid", "position", "moves", "qtm", "match", "note"
        ]
    );
    lines
        .iter()
        .skip(1)
        .filter(|l| !l.is_empty())
        .map(|l| l.split('\t').map(str::to_string).collect())
        .collect()
}

pub fn alg_text() -> impl Strategy<Value = String> {
    let mut pool = Vec::new();
    for base in ["U", "D", "L", "R", "F", "B"] {
        for suffix in ["", "'", "2", "2'"] {
            pool.push(format!("{base}{suffix}"));
            pool.push(format!("2{base}{suffix}"));
            pool.push(format!("{}{suffix}", base.to_ascii_lowercase()));
        }
    }
    for base in ["M", "E", "S", "x", "y", "z"] {
        for suffix in ["", "'", "2", "2'"] {
            pool.push(format!("{base}{suffix}"));
        }
    }
    proptest::collection::vec(select(pool), 0..24).prop_map(|tokens| tokens.join(" "))
}
