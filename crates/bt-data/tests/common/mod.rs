#![allow(dead_code)]

use std::path::PathBuf;

pub const SHEETS: [(&str, &str, usize); 12] = [
    ("cherry", "edge.tsv", 551),
    ("cherry", "corner.tsv", 328),
    ("cherry", "flips.tsv", 25),
    ("cherry", "twists.tsv", 20),
    ("cherry", "parity.tsv", 26),
    ("cherry", "ltct.tsv", 8),
    ("eggins", "edge.tsv", 71),
    ("eggins", "corner.tsv", 38),
    ("eggins", "flips.tsv", 0),
    ("eggins", "twists.tsv", 5),
    ("eggins", "parity.tsv", 2),
    ("eggins", "ltct.tsv", 3),
];

pub fn workspace_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .and_then(|p| p.parent())
        .expect("workspace root from manifest dir")
        .to_path_buf()
}

pub fn ref_ground() -> PathBuf {
    match std::env::var("BT_REF_GROUND") {
        Ok(raw) if !raw.is_empty() => {
            let path = PathBuf::from(raw);
            if path.is_absolute() {
                path
            } else {
                workspace_root().join(path)
            }
        }
        _ => workspace_root().join("ref_ground"),
    }
}

pub fn config_path() -> PathBuf {
    match std::env::var("BT_CONFIG") {
        Ok(raw) if !raw.is_empty() => {
            let path = PathBuf::from(raw);
            if path.is_absolute() {
                path
            } else {
                workspace_root().join(path)
            }
        }
        _ => workspace_root().join("config/config.json"),
    }
}

pub fn sheet_text(solver_dir: &str, file: &str) -> String {
    let path = ref_ground().join(solver_dir).join(file);
    std::fs::read_to_string(&path).unwrap_or_else(|e| panic!("read {}: {}", path.display(), e))
}

pub fn simcheck_records() -> Vec<[String; 8]> {
    let text = std::fs::read_to_string(ref_ground().join("simcheck.tsv")).expect("simcheck");
    text.split('\n')
        .filter(|l| !l.trim().is_empty())
        .map(|l| l.trim_end_matches('\r'))
        .skip(1)
        .map(|l| {
            let cols: Vec<String> = l.split('\t').map(str::to_string).collect();
            cols.try_into().expect("simcheck row has 8 columns")
        })
        .collect()
}
