use bt_scheme::{Orbit, letter_of, name_of, same_piece, sticker_of};
use std::collections::HashSet;

mod common;

use common::ref_ground;

const CORNER_GRID: [(u8, u8); 4] = [(0, 0), (0, 2), (2, 2), (2, 0)];
const EDGE_GRID: [(u8, u8); 4] = [(0, 1), (1, 2), (2, 1), (1, 0)];

#[test]
fn position_map_tsv_matches_scheme_all_48() {
    let path = ref_ground().join("position_map.tsv");
    let text = std::fs::read_to_string(&path)
        .unwrap_or_else(|e| panic!("cannot read {}: {e}", path.display()));
    let mut lines = text.lines().filter(|l| !l.trim().is_empty());
    let header = lines.next().unwrap();
    assert_eq!(
        header,
        "orbit\tsticker\tletter\tname\tface\trow\tcol\tpiece"
    );

    let mut pieces = [255u8; 48];
    let mut letters_seen = HashSet::new();
    let mut names_seen = HashSet::new();
    let mut rows = 0usize;

    for line in lines {
        let cols: Vec<&str> = line.split('\t').collect();
        assert_eq!(cols.len(), 8, "row must have 8 columns: {line}");
        let orbit = match cols[0] {
            "corner" => Orbit::Corner,
            "edge" => Orbit::Edge,
            other => panic!("unknown orbit token: {other}"),
        };
        let sticker: u8 = cols[1].parse().unwrap();
        let letter: char = cols[2].parse().unwrap();
        let name = cols[3];
        let face: u8 = cols[4].parse().unwrap();
        let row: u8 = cols[5].parse().unwrap();
        let col: u8 = cols[6].parse().unwrap();
        let piece: u8 = cols[7].parse().unwrap();

        assert_eq!(letter_of(sticker), letter);
        assert_eq!(name_of(sticker), name);
        assert_eq!(sticker_of(orbit, letter), sticker);
        assert_eq!(face, (sticker % 24) / 4);
        let grid = if orbit == Orbit::Corner {
            &CORNER_GRID
        } else {
            &EDGE_GRID
        };
        assert_eq!((row, col), grid[(sticker % 4) as usize]);

        assert!(orbit.stickers().contains(&sticker));
        pieces[sticker as usize] = piece;
        letters_seen.insert(letter);
        names_seen.insert(name);
        rows += 1;
    }

    assert_eq!(rows, 48);
    assert_eq!(letters_seen.len(), 24);
    assert_eq!(names_seen.len(), 48);

    for orbit in [Orbit::Edge, Orbit::Corner] {
        let ids: HashSet<u8> = orbit.stickers().map(|s| pieces[s as usize]).collect();
        let expected = if orbit == Orbit::Edge { 12 } else { 8 };
        assert_eq!(ids.len(), expected);
        assert_eq!(*ids.iter().min().unwrap(), 0);
        assert_eq!(*ids.iter().max().unwrap(), expected as u8 - 1);
    }

    for a in 0..48u8 {
        for b in 0..48u8 {
            let grouped = (a < 24) == (b < 24) && pieces[a as usize] == pieces[b as usize];
            assert_eq!(
                grouped,
                same_piece(a, b),
                "piece grouping mismatch for {a} vs {b}"
            );
        }
    }
}
