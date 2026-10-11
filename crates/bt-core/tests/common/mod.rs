#![allow(dead_code)]

use proptest::prelude::*;
use proptest::sample::select;
use std::path::PathBuf;

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
