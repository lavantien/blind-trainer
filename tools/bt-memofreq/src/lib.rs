use bt_ga::dist::{CaseCount, FreqData, PairCount};
use bt_scheme::{CaseId, Orbit, enumerate_cases, memo_from_array, random_occ};
use rayon::prelude::*;

pub const EDGE_COUNT: usize = 440;
pub const CORNER_COUNT: usize = 378;
pub const TOTAL_CASES: usize = EDGE_COUNT + CORNER_COUNT;

pub fn crate_id() -> &'static str {
    "bt-memofreq"
}

pub fn draw_seed(seed: u64, index: u64) -> u64 {
    let mut z = seed.wrapping_add(index.wrapping_mul(0x9E37_79B9_7F4A_7C15));
    z = (z ^ (z >> 30)).wrapping_mul(0xBF58_476D_1CE4_E5B9);
    z = (z ^ (z >> 27)).wrapping_mul(0x94D0_49BB_1331_11EB);
    z ^ (z >> 31)
}

fn triangle(size: usize) -> usize {
    size * (size + 1) / 2
}

fn pair_index(a: usize, b: usize) -> usize {
    let (lo, hi) = if a <= b { (a, b) } else { (b, a) };
    hi * (hi + 1) / 2 + lo
}

#[derive(Clone)]
struct OrbitTables {
    index: Vec<i32>,
    cases: Vec<CaseId>,
}

impl OrbitTables {
    fn build() -> OrbitTables {
        let mut index = vec![-1i32; 48 * 48];
        let mut cases = Vec::new();
        for (orbit, offset) in [(Orbit::Edge, 0usize), (Orbit::Corner, EDGE_COUNT)] {
            for (i, case) in enumerate_cases(orbit).into_iter().enumerate() {
                index[case.t1 as usize * 48 + case.t2 as usize] = (offset + i) as i32;
                cases.push(case);
            }
        }
        OrbitTables { index, cases }
    }
}

#[derive(Clone)]
struct DrawCounts {
    case: Vec<u64>,
    edge_cooc: Vec<u64>,
    corner_cooc: Vec<u64>,
    edge_adj: Vec<u64>,
    corner_adj: Vec<u64>,
}

impl DrawCounts {
    fn new() -> DrawCounts {
        DrawCounts {
            case: vec![0; TOTAL_CASES],
            edge_cooc: vec![0; triangle(EDGE_COUNT)],
            corner_cooc: vec![0; triangle(CORNER_COUNT)],
            edge_adj: vec![0; triangle(EDGE_COUNT)],
            corner_adj: vec![0; triangle(CORNER_COUNT)],
        }
    }

    fn merge(mut self, other: DrawCounts) -> DrawCounts {
        for (a, b) in self.case.iter_mut().zip(other.case.iter()) {
            *a += b;
        }
        for (a, b) in self.edge_cooc.iter_mut().zip(other.edge_cooc.iter()) {
            *a += b;
        }
        for (a, b) in self.corner_cooc.iter_mut().zip(other.corner_cooc.iter()) {
            *a += b;
        }
        for (a, b) in self.edge_adj.iter_mut().zip(other.edge_adj.iter()) {
            *a += b;
        }
        for (a, b) in self.corner_adj.iter_mut().zip(other.corner_adj.iter()) {
            *a += b;
        }
        self
    }

    fn add_draw(&mut self, tables: &OrbitTables, seed: u64) {
        let occ = random_occ(seed);
        let memo = memo_from_array(&occ);
        let edge: Vec<usize> = memo
            .pairs
            .0
            .iter()
            .map(|c| tables.index[c.t1 as usize * 48 + c.t2 as usize] as usize)
            .collect();
        let corner: Vec<usize> = memo
            .pairs
            .1
            .iter()
            .map(|c| tables.index[c.t1 as usize * 48 + c.t2 as usize] as usize)
            .collect();
        let DrawCounts {
            case,
            edge_cooc,
            corner_cooc,
            edge_adj,
            corner_adj,
        } = self;
        add_stream(case, 0, edge_adj, edge_cooc, &edge);
        add_stream(case, EDGE_COUNT, corner_adj, corner_cooc, &corner);
    }
}

fn add_stream(case: &mut [u64], base: usize, adj: &mut [u64], cooc: &mut [u64], indices: &[usize]) {
    for &c in indices {
        case[c] += 1;
    }
    for window in indices.windows(2) {
        adj[pair_index(window[0] - base, window[1] - base)] += 1;
    }
    for i in 0..indices.len() {
        for j in (i + 1)..indices.len() {
            cooc[pair_index(indices[i] - base, indices[j] - base)] += 1;
        }
    }
}

fn drain_pairs(counts: Vec<u64>, base: usize) -> Vec<PairCount> {
    counts
        .into_iter()
        .enumerate()
        .filter(|(_, count)| *count > 0)
        .map(|(index, count)| PairCount {
            from: base as u32 + row_of(index) as u32,
            to: base as u32 + col_of(index) as u32,
            count,
        })
        .collect()
}

fn row_of(index: usize) -> usize {
    let mut hi = 0usize;
    while triangle(hi + 1) <= index {
        hi += 1;
    }
    hi
}

fn col_of(index: usize) -> usize {
    index - triangle(row_of(index))
}

pub fn count_frequencies(samples: u64, seed: u64) -> FreqData {
    let tables = OrbitTables::build();
    let counts = (0..samples)
        .into_par_iter()
        .fold(DrawCounts::new, |mut acc, draw| {
            acc.add_draw(&tables, draw_seed(seed, draw));
            acc
        })
        .reduce(DrawCounts::new, DrawCounts::merge);
    let cases = tables
        .cases
        .iter()
        .zip(counts.case.iter())
        .map(|(case, &count)| {
            let (l1, l2) = case.letters();
            CaseCount {
                orbit: match case.orbit {
                    Orbit::Edge => "edge".to_string(),
                    Orbit::Corner => "corner".to_string(),
                },
                letters: format!("{l1}{l2}"),
                count,
            }
        })
        .collect();
    FreqData {
        cases,
        adjacent: [
            drain_pairs(counts.edge_adj, 0),
            drain_pairs(counts.corner_adj, EDGE_COUNT),
        ]
        .concat(),
        cooccurring: [
            drain_pairs(counts.edge_cooc, 0),
            drain_pairs(counts.corner_cooc, EDGE_COUNT),
        ]
        .concat(),
        total_draws: samples,
        seed,
    }
}

pub fn zero_hit_cases(data: &FreqData) -> Vec<String> {
    data.cases
        .iter()
        .filter(|case| case.count == 0)
        .map(|case| format!("{}/{}", case.orbit, case.letters))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn draw_seed_is_splitmix_stable() {
        assert_eq!(draw_seed(20261012, 0), 16176770643486239525);
        assert_eq!(draw_seed(20261012, 1), 10310797173907234146);
        assert_eq!(draw_seed(0, 42), 1391454601869358542);
    }

    #[test]
    fn pair_index_covers_triangle_without_gaps() {
        let mut seen = vec![false; triangle(5)];
        for a in 0..5 {
            for b in 0..5 {
                seen[pair_index(a, b)] = true;
            }
        }
        assert!(seen.iter().all(|&s| s));
        assert_eq!(pair_index(3, 1), pair_index(1, 3));
        assert_eq!(pair_index(2, 2), 5);
    }

    #[test]
    fn counts_match_a_sequential_replay() {
        let samples = 250u64;
        let seed = 777u64;
        let data = count_frequencies(samples, seed);
        let tables = OrbitTables::build();
        let mut case_counts = vec![0u64; TOTAL_CASES];
        let mut edge_adj = vec![0u64; triangle(EDGE_COUNT)];
        let mut corner_adj = vec![0u64; triangle(CORNER_COUNT)];
        let mut edge_cooc = vec![0u64; triangle(EDGE_COUNT)];
        let mut corner_cooc = vec![0u64; triangle(CORNER_COUNT)];
        for draw in 0..samples {
            let occ = random_occ(draw_seed(seed, draw));
            let memo = memo_from_array(&occ);
            for (orbit, base, adj, cooc) in [
                (&memo.pairs.0, 0usize, &mut edge_adj, &mut edge_cooc),
                (&memo.pairs.1, EDGE_COUNT, &mut corner_adj, &mut corner_cooc),
            ] {
                let indices: Vec<usize> = orbit
                    .iter()
                    .map(|c| tables.index[c.t1 as usize * 48 + c.t2 as usize] as usize)
                    .collect();
                for &case in &indices {
                    case_counts[case] += 1;
                }
                for window in indices.windows(2) {
                    adj[pair_index(window[0] - base, window[1] - base)] += 1;
                }
                for i in 0..indices.len() {
                    for j in (i + 1)..indices.len() {
                        cooc[pair_index(indices[i] - base, indices[j] - base)] += 1;
                    }
                }
            }
        }
        for (recorded, expected) in data.cases.iter().zip(case_counts.iter()) {
            assert_eq!(recorded.count, *expected, "{}", recorded.letters);
        }
        assert_eq!(
            data.adjacent
                .iter()
                .map(|p| (p.from, p.to, p.count))
                .collect::<Vec<_>>(),
            [
                drain_pairs(edge_adj, 0),
                drain_pairs(corner_adj, EDGE_COUNT)
            ]
            .concat()
            .iter()
            .map(|p| (p.from, p.to, p.count))
            .collect::<Vec<_>>()
        );
        assert_eq!(
            data.cooccurring
                .iter()
                .map(|p| (p.from, p.to, p.count))
                .collect::<Vec<_>>(),
            [
                drain_pairs(edge_cooc, 0),
                drain_pairs(corner_cooc, EDGE_COUNT)
            ]
            .concat()
            .iter()
            .map(|p| (p.from, p.to, p.count))
            .collect::<Vec<_>>()
        );
    }

    #[test]
    fn output_is_byte_identical_across_runs() {
        let a = count_frequencies(400, 20261012);
        let b = count_frequencies(400, 20261012);
        let bytes_a = serde_json::to_vec(&a).unwrap();
        let bytes_b = serde_json::to_vec(&b).unwrap();
        assert_eq!(bytes_a, bytes_b);
        assert_eq!(
            bt_ga::hash::sha256_hex(&bytes_a),
            bt_ga::hash::sha256_hex(&bytes_b)
        );
    }

    #[test]
    fn every_case_is_hit_over_a_long_run() {
        let data = count_frequencies(30000, 20261012);
        let zeros = zero_hit_cases(&data);
        assert!(zeros.is_empty(), "zero hit cases: {zeros:?}");
        assert_eq!(data.cases.len(), TOTAL_CASES);
        assert_eq!(data.total_draws, 30000);
    }

    #[test]
    fn zero_hit_checker_lists_misses() {
        let data = FreqData {
            cases: vec![
                CaseCount {
                    orbit: "edge".to_string(),
                    letters: "AB".to_string(),
                    count: 5,
                },
                CaseCount {
                    orbit: "edge".to_string(),
                    letters: "AD".to_string(),
                    count: 0,
                },
                CaseCount {
                    orbit: "corner".to_string(),
                    letters: "AB".to_string(),
                    count: 0,
                },
            ],
            adjacent: vec![],
            cooccurring: vec![],
            total_draws: 10,
            seed: 1,
        };
        assert_eq!(zero_hit_cases(&data), vec!["edge/AD", "corner/AB"]);
    }

    #[test]
    fn crate_id_matches_package() {
        assert_eq!(crate_id(), "bt-memofreq");
    }

    #[test]
    fn orbit_counts_match_the_scheme_enumeration() {
        assert_eq!(EDGE_COUNT, enumerate_cases(Orbit::Edge).len());
        assert_eq!(CORNER_COUNT, enumerate_cases(Orbit::Corner).len());
        assert_eq!(TOTAL_CASES, EDGE_COUNT + CORNER_COUNT);
    }

    #[test]
    fn draw_indices_never_alias_within_a_run() {
        let drawn: std::collections::HashSet<u64> =
            (0..128u64).map(|i| draw_seed(20261014, i)).collect();
        assert_eq!(drawn.len(), 128);
        let other_seed: std::collections::HashSet<u64> =
            (0..128u64).map(|i| draw_seed(1, i)).collect();
        assert_eq!(other_seed.len(), 128);
    }

    #[test]
    fn add_stream_counts_a_hand_built_stream_exactly() {
        let mut case = vec![0u64; 8];
        let mut adj = vec![0u64; triangle(8)];
        let mut cooc = vec![0u64; triangle(8)];
        add_stream(&mut case, 0, &mut adj, &mut cooc, &[0, 2, 1, 2]);
        assert_eq!(case, vec![1, 1, 2, 0, 0, 0, 0, 0]);
        let mut want_adj = vec![0u64; triangle(8)];
        want_adj[pair_index(0, 2)] = 1;
        want_adj[pair_index(1, 2)] = 2;
        assert_eq!(adj, want_adj);
        let mut want_cooc = vec![0u64; triangle(8)];
        want_cooc[pair_index(0, 1)] = 1;
        want_cooc[pair_index(0, 2)] = 2;
        want_cooc[pair_index(1, 2)] = 2;
        want_cooc[pair_index(2, 2)] = 1;
        assert_eq!(cooc, want_cooc);
    }

    #[test]
    fn add_stream_subtracts_the_base_before_indexing_pairs() {
        let mut case = vec![0u64; 20];
        let mut adj = vec![0u64; triangle(20)];
        let mut cooc = vec![0u64; triangle(20)];
        add_stream(&mut case, 10, &mut adj, &mut cooc, &[10, 13, 10]);
        let mut want_case = vec![0u64; 20];
        want_case[10] = 2;
        want_case[13] = 1;
        assert_eq!(case, want_case);
        assert_eq!(adj[pair_index(0, 3)], 2);
        assert_eq!(cooc[pair_index(0, 3)], 2);
        assert_eq!(cooc[pair_index(0, 0)], 1);
    }

    #[test]
    fn merge_sums_every_table_and_new_is_the_identity() {
        let mut a = DrawCounts::new();
        a.case[3] = 5;
        a.edge_cooc[pair_index(2, 2)] = 7;
        a.corner_adj[pair_index(1, 4)] = 9;
        let mut b = DrawCounts::new();
        b.case[3] = 1;
        b.edge_cooc[pair_index(2, 2)] = 2;
        b.corner_adj[pair_index(1, 4)] = 3;
        let merged = a.clone().merge(b.clone());
        assert_eq!(merged.case[3], 6);
        assert_eq!(merged.edge_cooc[pair_index(2, 2)], 9);
        assert_eq!(merged.corner_adj[pair_index(1, 4)], 12);
        let untouched = a.merge(DrawCounts::new());
        assert_eq!(untouched.case[3], 5);
        let zeroed = DrawCounts::new().merge(b);
        assert_eq!(zeroed.corner_adj[pair_index(1, 4)], 3);
    }

    #[test]
    fn row_and_col_invert_pair_index_over_the_triangle() {
        for hi in 0..40usize {
            for lo in 0..=hi {
                let index = pair_index(lo, hi);
                assert_eq!(row_of(index), hi);
                assert_eq!(col_of(index), lo);
            }
        }
        assert_eq!(row_of(triangle(4) - 1), 3);
        assert_eq!(col_of(triangle(4) - 1), 3);
    }

    #[test]
    fn drain_pairs_offsets_indices_and_drops_zero_cells() {
        let mut counts = vec![0u64; triangle(4)];
        counts[pair_index(0, 0)] = 2;
        counts[pair_index(1, 3)] = 7;
        let drained = drain_pairs(counts, 5);
        assert_eq!(
            drained,
            vec![
                PairCount {
                    from: 5,
                    to: 5,
                    count: 2,
                },
                PairCount {
                    from: 8,
                    to: 6,
                    count: 7,
                },
            ]
        );
        assert!(drain_pairs(vec![0; triangle(4)], 9).is_empty());
    }

    #[test]
    fn zero_samples_reports_an_all_zero_catalog() {
        let data = count_frequencies(0, 7);
        assert_eq!(data.total_draws, 0);
        assert_eq!(data.seed, 7);
        assert_eq!(data.cases.len(), TOTAL_CASES);
        assert!(data.cases.iter().all(|case| case.count == 0));
        assert!(data.adjacent.is_empty());
        assert!(data.cooccurring.is_empty());
        let misses = zero_hit_cases(&data);
        assert_eq!(misses.len(), TOTAL_CASES);
        assert_eq!(misses[0], "edge/AB");
        assert_eq!(misses[EDGE_COUNT], "corner/AB");
        assert_eq!(misses[TOTAL_CASES - 1], "corner/XW");
    }

    #[test]
    fn case_catalog_follows_the_scheme_enumeration_order() {
        let data = count_frequencies(3, 5);
        let expected: Vec<(String, String)> = enumerate_cases(Orbit::Edge)
            .into_iter()
            .chain(enumerate_cases(Orbit::Corner))
            .map(|case| {
                let (l1, l2) = case.letters();
                (
                    match case.orbit {
                        Orbit::Edge => "edge".to_string(),
                        Orbit::Corner => "corner".to_string(),
                    },
                    format!("{l1}{l2}"),
                )
            })
            .collect();
        assert_eq!(expected.len(), TOTAL_CASES);
        let got: Vec<(String, String)> = data
            .cases
            .iter()
            .map(|case| (case.orbit.clone(), case.letters.clone()))
            .collect();
        assert_eq!(got, expected);
        assert_eq!(data.total_draws, 3);
    }
}
