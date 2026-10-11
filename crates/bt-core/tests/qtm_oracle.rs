mod common;

use bt_core::{Alg, Move};
use std::collections::HashMap;

#[test]
fn qtm_and_htm_agree_with_simcheck() {
    let rows = common::simcheck_rows();
    assert_eq!(rows.len(), 1077);
    let mut sheets: HashMap<String, Vec<String>> = HashMap::new();
    for r in &rows {
        let key = format!("{}/{}.tsv", r[0], r[1]);
        let lines = sheets
            .entry(key.clone())
            .or_insert_with(|| common::sheet_lines(&key));
        let rowid: usize = r[2].parse().unwrap();
        let line = &lines[rowid - 1];
        let alg_text = line.split('\t').nth(1).unwrap();
        let alg = Alg::parse(alg_text).unwrap_or_else(|e| panic!("{} row {}: {}", key, rowid, e));
        let qtm: u32 = r[5].parse().unwrap();
        let moves: u32 = r[4].parse().unwrap();
        let rotations = alg
            .moves()
            .iter()
            .filter(|mv| matches!(mv, Move::Rotation { .. }))
            .count() as u32;
        assert_eq!(alg.qtm(), qtm, "{} row {} {:?}", key, rowid, alg_text);
        assert_eq!(
            alg.len() as u32,
            moves,
            "{} row {} {:?}",
            key,
            rowid,
            alg_text
        );
        assert_eq!(
            alg.htm() + rotations,
            moves,
            "{} row {} {:?}",
            key,
            rowid,
            alg_text
        );
    }
}
