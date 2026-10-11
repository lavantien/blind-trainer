pub mod report;

use bt_data::corpus::{SheetClass, Solver};
use std::collections::HashMap;
use std::path::Path;

pub const SIMCHECK_HEADER: &str = "solver\tsheet\trowid\tposition\tmoves\tqtm\tmatch\tnote";

pub const SHEET_EXPECTED_ROWS: [(&str, &str, usize); 12] = [
    ("cherry", "edge", 551),
    ("cherry", "corner", 328),
    ("cherry", "flips", 25),
    ("cherry", "twists", 20),
    ("cherry", "parity", 26),
    ("cherry", "ltct", 8),
    ("eggins", "edge", 71),
    ("eggins", "corner", 38),
    ("eggins", "flips", 0),
    ("eggins", "twists", 5),
    ("eggins", "parity", 2),
    ("eggins", "ltct", 3),
];

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct SimRow {
    pub position: String,
    pub qtm: u32,
}

pub type SimMap = HashMap<(Solver, SheetClass, usize), SimRow>;

pub fn load_simcheck(root: &Path) -> Result<SimMap, String> {
    let path = root.join("simcheck.tsv");
    let text =
        std::fs::read_to_string(&path).map_err(|e| format!("read {}: {e}", path.display()))?;
    let mut lines = text.lines().filter(|l| !l.trim().is_empty());
    let header = lines
        .next()
        .ok_or_else(|| "simcheck.tsv is empty".to_string())?;
    if header.trim_end_matches('\r') != SIMCHECK_HEADER {
        return Err(format!("simcheck.tsv header changed: {header}"));
    }
    let mut map = SimMap::new();
    for line in lines {
        let cols: Vec<&str> = line.trim_end_matches('\r').split('\t').collect();
        if cols.len() != 8 {
            return Err(format!("simcheck row has {} columns: {line}", cols.len()));
        }
        let solver = Solver::parse(cols[0])
            .ok_or_else(|| format!("simcheck solver {} is unknown", cols[0]))?;
        let sheet = SheetClass::parse(cols[1])
            .ok_or_else(|| format!("simcheck sheet {} is unknown", cols[1]))?;
        let rowid: usize = cols[2]
            .parse()
            .map_err(|_| format!("simcheck rowid {} is not a number", cols[2]))?;
        let qtm: u32 = cols[5]
            .parse()
            .map_err(|_| format!("simcheck qtm {} is not a number", cols[5]))?;
        map.insert(
            (solver, sheet, rowid),
            SimRow {
                position: cols[3].to_string(),
                qtm,
            },
        );
    }
    Ok(map)
}

pub fn crate_id() -> &'static str {
    "bt-verify"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn crate_id_matches_package() {
        assert_eq!(crate_id(), "bt-verify");
    }

    #[test]
    fn sheet_expectations_sum_to_the_corpus_total() {
        assert_eq!(
            SHEET_EXPECTED_ROWS
                .iter()
                .map(|&(_, _, n)| n)
                .sum::<usize>(),
            bt_gendata::TOTAL_ROWS
        );
    }
}
