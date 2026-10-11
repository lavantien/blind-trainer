use bt_scheme::{
    CORNER_BUFFER_STICKER, EDGE_BUFFER_STICKER, Orbit, enumerate_cases, letter_of, same_piece,
};
use std::collections::HashSet;

mod common;

use common::ref_ground;

fn load_rows(file: &str) -> Vec<(u8, u8, String)> {
    let path = ref_ground().join(file);
    let text = std::fs::read_to_string(&path)
        .unwrap_or_else(|e| panic!("cannot read {}: {e}", path.display()));
    let mut lines = text.lines().filter(|l| !l.trim().is_empty());
    let header = lines.next().unwrap();
    assert_eq!(header, "t1\tt2\tletters");
    lines
        .map(|line| {
            let cols: Vec<&str> = line.split('\t').collect();
            assert_eq!(cols.len(), 3, "row must have 3 columns: {line}");
            (
                cols[0].parse().unwrap(),
                cols[1].parse().unwrap(),
                cols[2].to_string(),
            )
        })
        .collect()
}

fn check_against_dump(orbit: Orbit, file: &str, expected_count: usize, buffer: u8) {
    let rows = load_rows(file);
    assert_eq!(rows.len(), expected_count, "{file} row count");
    let cases = enumerate_cases(orbit);
    assert_eq!(cases.len(), expected_count, "enumeration count for {file}");

    let ours: Vec<(u8, u8)> = cases.iter().map(|c| (c.t1, c.t2)).collect();
    let theirs: Vec<(u8, u8)> = rows.iter().map(|(t1, t2, _)| (*t1, *t2)).collect();
    assert_eq!(ours, theirs, "sequence equality against {file}");

    let unique: HashSet<(u8, u8)> = ours.iter().copied().collect();
    assert_eq!(
        unique.len(),
        expected_count,
        "no duplicates in {file} space"
    );

    for (case, (t1, t2, letters)) in cases.iter().zip(rows.iter()) {
        assert_eq!((case.t1, case.t2), (*t1, *t2));
        let (l1, l2) = case.letters();
        assert_eq!(
            &format!("{l1}{l2}"),
            letters,
            "letters column for {t1}-{t2}"
        );
        assert_eq!((l1, l2), (letter_of(case.t1), letter_of(case.t2)));
        assert!(orbit.stickers().contains(&case.t1));
        assert!(orbit.stickers().contains(&case.t2));
        assert!(!same_piece(case.t1, buffer));
        assert!(!same_piece(case.t2, buffer));
        assert!(!same_piece(case.t1, case.t2));
    }
}

#[test]
fn edge_pairs_match_pairs_edge2_exactly() {
    check_against_dump(Orbit::Edge, "pairs_edge2.tsv", 440, EDGE_BUFFER_STICKER);
}

#[test]
fn corner_pairs_match_pairs_corner2_exactly() {
    check_against_dump(
        Orbit::Corner,
        "pairs_corner2.tsv",
        378,
        CORNER_BUFFER_STICKER,
    );
}
