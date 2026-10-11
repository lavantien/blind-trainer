use bt_core::{Alg, Cube, Piece, piece_of};
use bt_scheme::memo::{Memo, Residue, apply_memo, ideal_cycle_at, memo, state_array};
use bt_scheme::pairs::enumerate_cases;
use bt_scheme::scheme::{Orbit, head_sticker, letter_of, sticker_of};
use bt_scheme::scramble::Rng;
use proptest::prelude::*;
use std::collections::{HashMap, HashSet};
use std::path::PathBuf;

fn ref_ground() -> PathBuf {
    let raw = std::env::var("BT_REF_GROUND")
        .unwrap_or_else(|_| panic!("BT_REF_GROUND is unset: run tests through make"));
    let path = PathBuf::from(raw);
    if path.is_absolute() {
        path
    } else {
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("../..")
            .join(path)
    }
}

fn sheet_lines(rel: &str) -> Vec<String> {
    let path = ref_ground().join(rel);
    let text =
        std::fs::read_to_string(&path).unwrap_or_else(|e| panic!("read {}: {}", path.display(), e));
    text.split('\n')
        .map(|l| l.trim_end_matches('\r').to_string())
        .collect()
}

fn simcheck_rows() -> Vec<Vec<String>> {
    sheet_lines("simcheck.tsv")
        .iter()
        .skip(1)
        .filter(|l| !l.is_empty())
        .map(|l| l.split('\t').map(str::to_string).collect())
        .collect()
}

fn sticker_of_name() -> HashMap<String, u8> {
    let mut map = HashMap::new();
    for line in sheet_lines("position_map.tsv").iter().skip(1) {
        if line.is_empty() {
            continue;
        }
        let cols: Vec<&str> = line.split('\t').collect();
        map.insert(cols[3].to_string(), cols[1].parse().unwrap());
    }
    assert_eq!(map.len(), 48);
    map
}

fn alg_of_row(solver: &str, sheet: &str, rowid: usize) -> Alg {
    let lines = sheet_lines(&format!("{solver}/{sheet}.tsv"));
    let text = lines[rowid - 1].split('\t').nth(1).unwrap();
    Alg::parse(text).unwrap()
}

fn forward_state(alg: &Alg) -> Cube {
    let mut cube = Cube::solved();
    cube.apply_alg(alg);
    cube
}

fn solved_by_state(alg: &Alg) -> Cube {
    let mut cube = Cube::solved();
    cube.apply_alg(&alg.inverse());
    cube
}

fn random_cube(seed: u64) -> Cube {
    let mut rng = Rng::new(seed);
    let faces = ["U", "D", "L", "R", "F", "B"];
    let suffixes = ["", "'", "2"];
    let len = 8 + rng.below(17);
    let mut tokens = Vec::new();
    let mut last = 6usize;
    for _ in 0..len {
        let mut f = rng.below(6) as usize;
        while f == last {
            f = rng.below(6) as usize;
        }
        last = f;
        tokens.push(format!("{}{}", faces[f], suffixes[rng.below(3) as usize]));
    }
    let mut cube = Cube::solved();
    cube.apply_alg(&Alg::parse(&tokens.join(" ")).unwrap());
    cube
}

fn assert_solves(cube: &Cube) -> Memo {
    let m = memo(cube);
    let mut work = state_array(cube);
    apply_memo(&mut work, &m);
    assert_eq!(
        work,
        std::array::from_fn(|i| i as u8),
        "round trip failed for letters {:?} {:?} residue {:?}",
        m.edge_letters,
        m.corner_letters,
        m.residue
    );
    m
}

fn piece_of_sticker(sticker: u8) -> (Orbit, u8) {
    match piece_of(sticker) {
        Piece::Corner(p) => (Orbit::Corner, p),
        Piece::Edge(p) => (Orbit::Edge, p),
    }
}

fn covered_pieces(m: &Memo) -> HashSet<(Orbit, u8)> {
    let mut covered = HashSet::new();
    for orbit in [Orbit::Edge, Orbit::Corner] {
        let (pairs, buffers) = match orbit {
            Orbit::Edge => (&m.pairs.0, &m.buffers.0),
            Orbit::Corner => (&m.pairs.1, &m.buffers.1),
        };
        for case in pairs {
            covered.insert(piece_of_sticker(case.t1));
            covered.insert(piece_of_sticker(case.t2));
        }
        for &b in buffers {
            covered.insert(piece_of_sticker(b));
        }
    }
    if let Some(parity) = m.residue.parity {
        covered.insert(piece_of_sticker(parity.edge.0));
        covered.insert(piece_of_sticker(parity.edge.1));
        covered.insert(piece_of_sticker(parity.corner.0));
        covered.insert(piece_of_sticker(parity.corner.1));
    }
    for &f in &m.residue.flips {
        covered.insert(piece_of_sticker(f));
    }
    for t in &m.residue.twists {
        covered.insert(piece_of_sticker(t.corner));
    }
    covered.insert(piece_of_sticker(sticker_of(Orbit::Edge, 'C')));
    covered.insert(piece_of_sticker(sticker_of(Orbit::Corner, 'C')));
    covered
}

fn check_partition(cube: &Cube, m: &Memo) {
    let edge_cases: HashSet<(u8, u8)> = enumerate_cases(Orbit::Edge)
        .iter()
        .map(|c| (c.t1, c.t2))
        .collect();
    let corner_cases: HashSet<(u8, u8)> = enumerate_cases(Orbit::Corner)
        .iter()
        .map(|c| (c.t1, c.t2))
        .collect();
    let covered = covered_pieces(m);
    for orbit in [Orbit::Edge, Orbit::Corner] {
        let (letters, pairs, buffers) = match orbit {
            Orbit::Edge => (&m.edge_letters, &m.pairs.0, &m.buffers.0),
            Orbit::Corner => (&m.corner_letters, &m.pairs.1, &m.buffers.1),
        };
        assert_eq!(
            letters.len() - 2 * pairs.len(),
            if letters.len() % 2 == 1 { 1 } else { 0 },
            "{orbit:?} letters {:?} pairs {:?}",
            letters,
            pairs
        );
        assert_eq!(pairs.len(), buffers.len());
        for case in pairs {
            let set = match orbit {
                Orbit::Edge => &edge_cases,
                Orbit::Corner => &corner_cases,
            };
            assert!(set.contains(&(case.t1, case.t2)), "{case:?}");
        }
        if letters.len() % 2 == 1 {
            let last = sticker_of(orbit, *letters.last().unwrap());
            let head = bt_core::head_sticker(piece_of(last));
            let owned = m.residue.parity.is_some()
                || m.residue.flips.contains(&head)
                || m.residue.twists.iter().any(|t| t.corner == head);
            assert!(owned, "trailing letter {} unowned", letter_of(last));
        }
    }
    for sticker in 0..48u8 {
        if cube.position_of(sticker) != sticker {
            let piece = piece_of_sticker(sticker);
            assert!(covered.contains(&piece), "orphan piece {piece:?}");
        }
    }
}

#[test]
fn memo_round_trips_and_partitions_random_states() {
    for seed in 0..2000u64 {
        let cube = random_cube(seed);
        let m = assert_solves(&cube);
        assert_eq!(memo(&cube), m, "determinism at seed {seed}");
        check_partition(&cube, &m);
    }
}

fn alg_strategy() -> impl proptest::prelude::Strategy<Value = String> {
    use proptest::prelude::*;
    let tokens: Vec<&str> = [
        "U", "U'", "U2", "D", "D'", "D2", "L", "L'", "L2", "R", "R'", "R2",
    ]
    .into_iter()
    .chain(["F", "F'", "F2", "B", "B'", "B2"])
    .collect();
    proptest::collection::vec(proptest::sample::select(tokens), 8..24)
        .prop_map(|tokens| tokens.join(" "))
}

proptest::prelude::proptest! {

    #![proptest_config(proptest::prelude::ProptestConfig::with_cases(512))]

    #[test]
    fn prop_memo_round_trips_partitions_and_is_deterministic(alg_text in alg_strategy()) {
        let mut cube = Cube::solved();
        cube.apply_alg(&Alg::parse(&alg_text).unwrap());
        let m = assert_solves(&cube);
        prop_assert_eq!(memo(&cube), m.clone());
        check_partition(&cube, &m);
    }
}

#[test]
fn clean_vendor_rows_match_first_pair_and_ideal_cycle() {
    let names = sticker_of_name();
    let mut checked = 0;
    for r in simcheck_rows() {
        if r[6] != "CYCLE_A" || !r[7].contains("clean") {
            continue;
        }
        let orbit = if r[1] == "edge" {
            Orbit::Edge
        } else {
            assert_eq!(r[1], "corner");
            Orbit::Corner
        };
        let rowid: usize = r[2].parse().unwrap();
        let alg = alg_of_row(&r[0], &r[1], rowid);
        let tokens: Vec<&str> = r[3].split('-').collect();
        assert_eq!(tokens.len(), 3, "{}", r[3]);
        let h = names[tokens[0]];
        let t1 = names[tokens[1]];
        let t2 = names[tokens[2]];
        let state = solved_by_state(&alg);
        let m = memo(&state);
        let (pairs, buffers, letters) = match orbit {
            Orbit::Edge => (&m.pairs.0, &m.buffers.0, &m.edge_letters),
            Orbit::Corner => (&m.pairs.1, &m.buffers.1, &m.corner_letters),
        };
        let ctx = format!("{} row {} {}", r[0], rowid, r[3]);
        assert_eq!(pairs.len(), 1, "{ctx}");
        assert_eq!((pairs[0].t1, pairs[0].t2), (t1, t2), "{ctx}");
        assert_eq!(buffers[0], h, "{ctx}");
        assert_eq!(letters, &[letter_of(t1), letter_of(t2)], "{ctx}");
        assert_eq!(m.residue, Residue::default(), "{ctx}");
        match orbit {
            Orbit::Edge => {
                assert!(m.pairs.1.is_empty() && m.corner_letters.is_empty(), "{ctx}");
            }
            Orbit::Corner => {
                assert!(m.pairs.0.is_empty() && m.edge_letters.is_empty(), "{ctx}");
            }
        }
        let cycles = ideal_cycle_at(orbit, h, t1, t2);
        let forward = forward_state(&alg);
        let affected: HashSet<u8> = cycles.iter().flat_map(|c| c.iter().copied()).collect();
        for &[a, b, c] in &cycles {
            assert_eq!(forward.position_of(a), b, "{ctx}");
            assert_eq!(forward.position_of(b), c, "{ctx}");
            assert_eq!(forward.position_of(c), a, "{ctx}");
        }
        for sticker in 0..48u8 {
            if !affected.contains(&sticker) {
                assert_eq!(forward.position_of(sticker), sticker, "{ctx}");
            }
        }
        assert_solves(&state);
        checked += 1;
    }
    assert_eq!(checked, 208);
}

#[test]
fn flips_sheet_rows_leave_flip_pair_residue() {
    let mut checked = 0;
    for r in simcheck_rows() {
        if r[1] != "flips" {
            continue;
        }
        let rowid: usize = r[2].parse().unwrap();
        let alg = alg_of_row(&r[0], &r[1], rowid);
        let state = forward_state(&alg);
        let m = assert_solves(&state);
        let parts: Vec<&str> = r[3].split(" & ").collect();
        assert_eq!(parts.len(), 2, "{}", r[3]);
        let mut expected: Vec<u8> = parts.iter().map(|n| head_sticker(n)).collect();
        expected.sort_unstable();
        let mut flips = m.residue.flips.clone();
        flips.sort_unstable();
        assert_eq!(flips, expected, "{} {} row {}", r[0], r[1], rowid);
        assert!(m.pairs.0.is_empty() && m.pairs.1.is_empty(), "{}", r[3]);
        assert!(m.residue.parity.is_none());
        assert!(m.residue.twists.is_empty());
        checked += 1;
    }
    assert_eq!(checked, 25);
}

fn twist_sign(note: &str, tag: &str) -> i8 {
    for token in note.split(' ') {
        if let Some(rest) = token.strip_prefix(tag) {
            return rest.parse().unwrap();
        }
    }
    panic!("no {tag} in {note}");
}

#[test]
fn twists_sheet_rows_leave_signed_twist_pair_residue() {
    let names = sticker_of_name();
    let mut checked = 0;
    for r in simcheck_rows() {
        if r[1] != "twists" {
            continue;
        }
        let rowid: usize = r[2].parse().unwrap();
        let alg = alg_of_row(&r[0], &r[1], rowid);
        let state = forward_state(&alg);
        let m = assert_solves(&state);
        let parts: Vec<&str> = r[3].split(" & ").collect();
        assert_eq!(parts.len(), 2, "{}", r[3]);
        let note = &r[7];
        let mut expected: HashSet<(u8, u8)> = HashSet::new();
        for (idx, printed) in parts.iter().enumerate() {
            let cyc_tag = format!("cyc{}=[", idx + 1);
            let cyc_corner = note
                .split(&cyc_tag)
                .nth(1)
                .unwrap()
                .split('>')
                .next()
                .unwrap();
            assert_eq!(piece_family(cyc_corner), piece_family(printed), "{}", r[3]);
            let d = twist_sign(note, &format!("d{}=", idx + 1));
            let head = names[*printed];
            let quarter = if d == 1 { 2u8 } else { 1u8 };
            expected.insert((head, quarter));
        }
        let got: HashSet<(u8, u8)> = m
            .residue
            .twists
            .iter()
            .map(|t| (t.corner, t.quarter))
            .collect();
        assert_eq!(got, expected, "{} {} row {}", r[0], r[1], rowid);
        assert!(m.pairs.0.is_empty() && m.pairs.1.is_empty(), "{}", r[3]);
        assert!(m.residue.parity.is_none());
        assert!(m.residue.flips.is_empty());
        checked += 1;
    }
    assert_eq!(checked, 25);
}

fn piece_family(name: &str) -> Vec<u8> {
    let mut faces: Vec<u8> = name.bytes().collect();
    faces.sort_unstable();
    faces
}

#[test]
fn parity_sheet_rows_leave_parity_residue() {
    let mut checked = 0;
    for r in simcheck_rows() {
        if r[1] != "parity" {
            continue;
        }
        let rowid: usize = r[2].parse().unwrap();
        let alg = alg_of_row(&r[0], &r[1], rowid);
        let state = forward_state(&alg);
        let m = assert_solves(&state);
        let halves: Vec<&str> = r[3].split(", ").collect();
        assert_eq!(halves.len(), 2, "{}", r[3]);
        let edge_tokens: Vec<&str> = halves[0].split('-').collect();
        let corner_tokens: Vec<&str> = halves[1].split('-').collect();
        assert_eq!(edge_tokens.len(), 2);
        assert_eq!(corner_tokens.len(), 2);
        let parity = m.residue.parity.expect(&r[3]);
        assert_eq!(
            parity.edge,
            (head_sticker(edge_tokens[0]), head_sticker(edge_tokens[1])),
            "{}",
            r[3]
        );
        assert_eq!(
            parity.corner,
            (
                head_sticker(corner_tokens[0]),
                head_sticker(corner_tokens[1])
            ),
            "{}",
            r[3]
        );
        assert!(m.pairs.0.is_empty() && m.pairs.1.is_empty(), "{}", r[3]);
        assert!(
            m.edge_letters.len() <= 1 && m.corner_letters.len() <= 1,
            "{}",
            r[3]
        );
        if r[7].contains("clean") {
            assert!(
                m.residue.flips.is_empty() && m.residue.twists.is_empty(),
                "{}",
                r[3]
            );
            assert_eq!(m.residue.class(), bt_scheme::ResidueClass::Parity);
        }
        checked += 1;
    }
    assert_eq!(checked, 28);
}

#[test]
fn ltct_sheet_rows_leave_ltct_residue() {
    let mut checked = 0;
    for r in simcheck_rows() {
        if r[1] != "ltct" {
            continue;
        }
        let rowid: usize = r[2].parse().unwrap();
        let alg = alg_of_row(&r[0], &r[1], rowid);
        let state = forward_state(&alg);
        let m = assert_solves(&state);
        let halves: Vec<&str> = r[3].split(", ").collect();
        assert_eq!(halves.len(), 2, "{}", r[3]);
        let edge_tokens: Vec<&str> = halves[0].split('-').collect();
        let bracket_start = halves[1].find('[').unwrap();
        let corner_part = &halves[1][..bracket_start];
        let bracket = &halves[1][bracket_start + 1..halves[1].len() - 1];
        let corner_tokens: Vec<&str> = corner_part.split('-').collect();
        assert_eq!(edge_tokens.len(), 2);
        assert_eq!(corner_tokens.len(), 2);
        let parity = m.residue.parity.expect(&r[3]);
        assert_eq!(
            parity.edge,
            (head_sticker(edge_tokens[0]), head_sticker(edge_tokens[1])),
            "{}",
            r[3]
        );
        assert_eq!(
            parity.corner,
            (
                head_sticker(corner_tokens[0]),
                head_sticker(corner_tokens[1])
            ),
            "{}",
            r[3]
        );
        assert!(
            m.residue.twists.len() == 2 || m.residue.twists.len() == 3,
            "{}",
            r[3]
        );
        let twist_corners: HashSet<u8> = m.residue.twists.iter().map(|t| t.corner).collect();
        assert!(twist_corners.contains(&head_sticker(bracket)), "{}", r[3]);
        assert!(m.residue.flips.is_empty(), "{}", r[3]);
        assert!(m.pairs.0.is_empty() && m.pairs.1.is_empty(), "{}", r[3]);
        assert_eq!(m.residue.class(), bt_scheme::ResidueClass::Ltct);
        checked += 1;
    }
    assert_eq!(checked, 11);
}
