use bt_core::{Alg, Face, Move, Slice};
use bt_data::corpus::SheetClass;
use bt_scheme::{
    CORNER_BUFFER_STICKER, EDGE_BUFFER_STICKER, Orbit, apply_sticker_cycles, ideal_cycle_at,
    name_of, same_piece,
};
use std::cmp::Ordering;

use crate::model::{CasesFile, VendorAlg};
use crate::scoring::{ScoringCfg, rank_cmp};

pub struct SynthLimits {
    pub max_setup: usize,
    pub max_insert: usize,
    pub node_budget: usize,
}

#[derive(Clone)]
pub struct SynthHit {
    pub alg: Alg,
    pub commutator: String,
}

fn solved_occ() -> [u8; 54] {
    std::array::from_fn(|i| i as u8)
}

fn apply_move_occ(occ: &mut [u8; 54], mv: Move) {
    let table = mv.table();
    let mut next = *occ;
    for (i, &sticker) in occ.iter().enumerate() {
        next[table[i] as usize] = sticker;
    }
    *occ = next;
}

fn apply_seq_occ(occ: &mut [u8; 54], moves: &[Move]) {
    for &mv in moves {
        apply_move_occ(occ, mv);
    }
}

fn occ_equals(left: &[u8; 54], right: &[u8; 54]) -> bool {
    left.iter().zip(right).all(|(a, b)| a == b)
}

fn face_turns() -> Vec<Move> {
    let mut moves = Vec::with_capacity(18);
    for face in Face::ALL {
        for quarter in [1u8, 2, 3] {
            moves.push(Move::Face { face, quarter });
        }
    }
    moves
}

fn single_turns() -> Vec<Move> {
    let mut moves = face_turns();
    for slice in [Slice::M, Slice::E, Slice::S] {
        for quarter in [1u8, 2, 3] {
            moves.push(Move::Slice { slice, quarter });
        }
    }
    moves
}

fn layer_of(mv: Move) -> u8 {
    match mv {
        Move::Face { face, .. } | Move::Wide { face, .. } | Move::Inner { face, .. } => {
            face.index() as u8
        }
        Move::Slice { slice, .. } => match slice {
            Slice::M => 6,
            Slice::E => 7,
            Slice::S => 8,
        },
        Move::Rotation { .. } => 9,
    }
}

fn canonical_extension(prefix: &[Move], mv: Move) -> bool {
    match prefix.last() {
        Some(&last) => layer_of(last) != layer_of(mv),
        None => true,
    }
}

fn setups_by_depth(alphabet: &[Move], max_setup: usize) -> Vec<Vec<Move>> {
    let mut frontier: Vec<Vec<Move>> = vec![Vec::new()];
    let mut all = vec![Vec::<Move>::new()];
    for _ in 0..max_setup {
        let mut next = Vec::new();
        for prefix in &frontier {
            for &mv in alphabet {
                if !canonical_extension(prefix, mv) {
                    continue;
                }
                let mut extended = prefix.clone();
                extended.push(mv);
                next.push(extended);
            }
        }
        all.extend(next.iter().cloned());
        frontier = next;
    }
    all
}

fn cycle_slots(cube_s: &[u8; 54], t_s: &[u8; 54], buffer_slot: usize) -> Option<[usize; 3]> {
    let mut slots = [buffer_slot, 0, 0];
    let mut current = buffer_slot;
    for index in 1..3 {
        let sticker = t_s[current];
        let from = cube_s
            .iter()
            .position(|&s| s == sticker)
            .unwrap_or(usize::MAX);
        if from == usize::MAX || from == buffer_slot || slots[..index].contains(&from) {
            return None;
        }
        slots[index] = from;
        current = from;
    }
    let sticker = t_s[current];
    let back = cube_s
        .iter()
        .position(|&s| s == sticker)
        .unwrap_or(usize::MAX);
    if back != buffer_slot {
        return None;
    }
    Some(slots)
}

struct Insertion {
    moves: Vec<Move>,
}

struct Budget {
    left: usize,
}

fn insertions_reaching(
    cube_s: &[u8; 54],
    alphabet: &[Move],
    max_insert: usize,
    targets: &[usize; 2],
    buffer: u8,
    budget: &mut Budget,
    sink: &mut Vec<Insertion>,
) {
    let mut stack: Vec<Move> = Vec::new();
    let mut state = *cube_s;
    recurse_insertions(
        &mut state, &mut stack, alphabet, max_insert, targets, buffer, budget, sink,
    );
    sink.sort_by_key(|insertion| insertion.moves.len());
}

#[allow(clippy::too_many_arguments)]
fn recurse_insertions(
    state: &mut [u8; 54],
    stack: &mut Vec<Move>,
    alphabet: &[Move],
    max_insert: usize,
    targets: &[usize; 2],
    buffer: u8,
    budget: &mut Budget,
    sink: &mut Vec<Insertion>,
) {
    if stack.len() == max_insert || budget.left == 0 {
        return;
    }
    for &mv in alphabet {
        if !canonical_extension(stack, mv) {
            continue;
        }
        apply_move_occ(state, mv);
        stack.push(mv);
        budget.left -= 1;
        let reaches = targets.iter().any(|&slot| same_piece(state[slot], buffer));
        if reaches {
            sink.push(Insertion {
                moves: stack.clone(),
            });
        }
        recurse_insertions(
            state, stack, alphabet, max_insert, targets, buffer, budget, sink,
        );
        stack.pop();
        apply_move_occ(state, mv.invert());
        if budget.left == 0 {
            return;
        }
    }
}

fn invert_seq(seq: &[Move]) -> Vec<Move> {
    seq.iter().rev().map(|mv| mv.invert()).collect()
}

fn commutator_of(setup: &[Move], first: &Alg, second: &Alg) -> (Alg, String) {
    let mut moves = Vec::new();
    moves.extend_from_slice(setup);
    moves.extend_from_slice(first.moves());
    moves.extend_from_slice(second.moves());
    let first_inv = first.inverse();
    let second_inv = second.inverse();
    moves.extend_from_slice(first_inv.moves());
    moves.extend_from_slice(second_inv.moves());
    moves.extend_from_slice(&invert_seq(setup));
    let core = format!("[{},{}]", first.format(), second.format());
    let text = if setup.is_empty() {
        core
    } else {
        format!("{}:{core}", Alg::new(setup.to_vec()).format())
    };
    (Alg::new(moves), text)
}

fn probe_moves(
    setup: &[Move],
    setup_inv: &[Move],
    first: &[Move],
    first_inv: &[Move],
    interchange: Move,
    interchange_first: bool,
) -> Vec<Move> {
    let mut moves = Vec::with_capacity(setup.len() * 2 + first.len() * 2 + 2);
    moves.extend_from_slice(setup);
    if interchange_first {
        moves.push(interchange);
        moves.extend_from_slice(first);
        moves.push(interchange.invert());
        moves.extend_from_slice(first_inv);
    } else {
        moves.extend_from_slice(first);
        moves.push(interchange);
        moves.extend_from_slice(first_inv);
        moves.push(interchange.invert());
    }
    moves.extend_from_slice(setup_inv);
    moves
}

fn search_with(target: &[u8; 54], buffer: u8, limits: &SynthLimits, wide: bool) -> Vec<SynthHit> {
    let setup_alphabet = if wide { single_turns() } else { face_turns() };
    let insert_alphabet = if wide { single_turns() } else { face_turns() };
    let interchanges = single_turns();
    let mut budget = Budget {
        left: limits.node_budget,
    };
    let mut hits: Vec<SynthHit> = Vec::new();
    let mut plateau: Option<u32> = None;
    for setup in setups_by_depth(&setup_alphabet, limits.max_setup) {
        if budget.left == 0 {
            break;
        }
        let mut cube_s = solved_occ();
        apply_seq_occ(&mut cube_s, &setup);
        let mut t_s = *target;
        apply_seq_occ(&mut t_s, &setup);
        let Some(buffer_slot) = cube_s.iter().position(|&s| s == buffer) else {
            continue;
        };
        let Some(slots) = cycle_slots(&cube_s, &t_s, buffer_slot) else {
            continue;
        };
        let targets = [slots[1], slots[2]];
        let usable: Vec<Move> = interchanges
            .iter()
            .copied()
            .filter(|mv| {
                let table = mv.table();
                table[targets[0]] != targets[0] as u8 && table[targets[1]] != targets[1] as u8
            })
            .collect();
        if usable.is_empty() {
            continue;
        }
        let setup_inv = invert_seq(&setup);
        let mut found = Vec::new();
        insertions_reaching(
            &cube_s,
            &insert_alphabet,
            limits.max_insert,
            &targets,
            buffer,
            &mut budget,
            &mut found,
        );
        for insertion in &found {
            let variants = [insertion.moves.clone(), invert_seq(&insertion.moves)];
            for first in &variants {
                let first_inv = invert_seq(first);
                for &interchange in &usable {
                    for interchange_first in [false, true] {
                        let moves = probe_moves(
                            &setup,
                            &setup_inv,
                            first,
                            &first_inv,
                            interchange,
                            interchange_first,
                        );
                        let mut probe = solved_occ();
                        apply_seq_occ(&mut probe, &moves);
                        if occ_equals(&probe, target) {
                            let first_alg = Alg::new(first.clone());
                            let second_alg = Alg::new(vec![interchange]);
                            let (alg, text) = if interchange_first {
                                commutator_of(&setup, &second_alg, &first_alg)
                            } else {
                                commutator_of(&setup, &first_alg, &second_alg)
                            };
                            let hit = SynthHit {
                                alg,
                                commutator: text,
                            };
                            match plateau {
                                None => plateau = Some(hit.alg.qtm()),
                                Some(q0) => {
                                    if hit.alg.qtm() > q0 {
                                        continue;
                                    }
                                }
                            }
                            hits.push(hit);
                        }
                    }
                }
            }
        }
        if plateau.is_some() {
            break;
        }
    }
    hits
}

pub fn synth_candidates(orbit: Orbit, t1: u8, t2: u8, limits: &SynthLimits) -> Vec<SynthHit> {
    let buffer = match orbit {
        Orbit::Edge => EDGE_BUFFER_STICKER,
        Orbit::Corner => CORNER_BUFFER_STICKER,
    };
    let mut target = solved_occ();
    apply_sticker_cycles(&mut target, &ideal_cycle_at(orbit, buffer, t1, t2));
    let hits = search_with(&target, buffer, limits, false);
    if hits.is_empty() {
        return search_with(&target, buffer, limits, true);
    }
    hits
}

pub fn synth_case(
    orbit: Orbit,
    t1: u8,
    t2: u8,
    limits: &SynthLimits,
    scoring: &ScoringCfg,
) -> Option<SynthHit> {
    synth_candidates(orbit, t1, t2, limits)
        .into_iter()
        .reduce(|kept, next| {
            if rank_cmp(next.alg.moves(), kept.alg.moves(), orbit, scoring) == Ordering::Less {
                next
            } else {
                kept
            }
        })
}

fn note_text(note: &bt_data::verify::Note) -> String {
    match note {
        bt_data::verify::Note::Flip { piece } => format!("flip:{piece}"),
        bt_data::verify::Note::Twist { piece, twist } => format!("twist:{piece}:{twist:+}"),
    }
}

pub fn synthesize(
    cases: &mut CasesFile,
    limits: &SynthLimits,
    scoring: &ScoringCfg,
) -> (usize, usize) {
    let mut added = (0, 0);
    for entry in cases.cases.iter_mut() {
        if !entry.vendor_algs.is_empty() {
            continue;
        }
        let orbit = if entry.orbit == "edge" {
            Orbit::Edge
        } else {
            Orbit::Corner
        };
        let Some(hit) = synth_case(orbit, entry.t1, entry.t2, limits, scoring) else {
            continue;
        };
        let buffer = match orbit {
            Orbit::Edge => EDGE_BUFFER_STICKER,
            Orbit::Corner => CORNER_BUFFER_STICKER,
        };
        let sheet = match orbit {
            Orbit::Edge => SheetClass::Edge,
            Orbit::Corner => SheetClass::Corner,
        };
        let position = format!(
            "{}-{}-{}",
            name_of(buffer),
            name_of(entry.t1),
            name_of(entry.t2)
        );
        let verdict = bt_data::verify::verify(&hit.alg, &position, sheet);
        entry.vendor_algs.push(VendorAlg {
            solver: "generated".to_string(),
            rowid: 0,
            position,
            alg: hit.alg.format(),
            qtm: hit.alg.qtm(),
            htm: hit.alg.htm(),
            ok: verdict.ok(),
            problems: verdict.problems.iter().map(|p| p.to_string()).collect(),
            notes: verdict.notes.iter().map(note_text).collect(),
            commutator: hit.commutator,
            recon_link: String::new(),
            youtube_link: String::new(),
        });
        entry.covered = true;
        match orbit {
            Orbit::Edge => added.0 += 1,
            Orbit::Corner => added.1 += 1,
        }
    }
    added
}

#[cfg(test)]
mod tests {
    use super::*;
    use bt_core::Cube;
    use bt_data::commutator::decompose_verified;
    use bt_scheme::{enumerate_cases, letter_of};
    use std::cmp::Ordering;

    fn limits() -> SynthLimits {
        SynthLimits {
            max_setup: 2,
            max_insert: 4,
            node_budget: 100_000_000,
        }
    }

    fn test_scoring() -> crate::scoring::ScoringCfg {
        crate::scoring::reference_scoring()
    }

    fn case_of(orbit: Orbit, first: char, second: char) -> (u8, u8) {
        enumerate_cases(orbit)
            .into_iter()
            .find(|case| {
                let (l1, l2) = (letter_of(case.t1), letter_of(case.t2));
                l1 == first && l2 == second
            })
            .map(|case| (case.t1, case.t2))
            .expect("case exists in the pair space")
    }

    fn assert_hit_matches_the_ideal_cycle(orbit: Orbit, t1: u8, t2: u8, hit: &SynthHit) {
        let buffer = match orbit {
            Orbit::Edge => EDGE_BUFFER_STICKER,
            Orbit::Corner => CORNER_BUFFER_STICKER,
        };
        let mut target = std::array::from_fn(|i| i as u8);
        apply_sticker_cycles(&mut target, &ideal_cycle_at(orbit, buffer, t1, t2));
        let mut cube = Cube::solved();
        cube.apply_alg(&hit.alg);
        for (pos, &wanted) in target.iter().enumerate() {
            assert_eq!(
                cube.sticker_at(pos as u8),
                wanted,
                "sticker {pos} differs for {} {}-{}",
                hit.alg.format(),
                name_of(t1),
                name_of(t2)
            );
        }
        assert!(cube.centers_fixed());
        assert!(hit.alg.qtm() <= 24);
        assert!(decompose_verified(&hit.commutator, &hit.alg).is_some());
    }

    #[test]
    fn edge_head_pair_gets_a_verified_commutator() {
        let (t1, t2) = case_of(Orbit::Edge, 'A', 'B');
        let hit = synth_case(Orbit::Edge, t1, t2, &limits(), &test_scoring())
            .expect("edge AB synthesizes");
        assert_hit_matches_the_ideal_cycle(Orbit::Edge, t1, t2, &hit);
    }

    #[test]
    fn corner_head_pair_gets_a_verified_commutator() {
        let (t1, t2) = case_of(Orbit::Corner, 'A', 'B');
        let hit = synth_case(Orbit::Corner, t1, t2, &limits(), &test_scoring())
            .expect("corner AB synthesizes");
        assert_hit_matches_the_ideal_cycle(Orbit::Corner, t1, t2, &hit);
    }

    #[test]
    fn edge_tail_pair_gets_a_verified_commutator() {
        let (t1, t2) = case_of(Orbit::Edge, 'E', 'B');
        let hit = synth_case(Orbit::Edge, t1, t2, &limits(), &test_scoring())
            .expect("edge EB synthesizes");
        assert_hit_matches_the_ideal_cycle(Orbit::Edge, t1, t2, &hit);
    }

    #[test]
    fn corner_tail_pair_gets_a_verified_commutator() {
        let (t1, t2) = case_of(Orbit::Corner, 'E', 'B');
        let hit = synth_case(Orbit::Corner, t1, t2, &limits(), &test_scoring())
            .expect("corner EB synthesizes");
        assert_hit_matches_the_ideal_cycle(Orbit::Corner, t1, t2, &hit);
    }

    #[test]
    fn synthesis_is_deterministic() {
        let (t1, t2) = case_of(Orbit::Edge, 'A', 'B');
        let once = synth_case(Orbit::Edge, t1, t2, &limits(), &test_scoring()).unwrap();
        let twice = synth_case(Orbit::Edge, t1, t2, &limits(), &test_scoring()).unwrap();
        assert_eq!(once.alg.format(), twice.alg.format());
        assert_eq!(once.commutator, twice.commutator);
    }

    #[test]
    fn best_plateau_selection_picks_the_rank_minimum() {
        let (t1, t2) = case_of(Orbit::Corner, 'A', 'B');
        let cfg = test_scoring();
        let candidates = synth_candidates(Orbit::Corner, t1, t2, &limits());
        assert!(!candidates.is_empty());
        let hit =
            synth_case(Orbit::Corner, t1, t2, &limits(), &cfg).expect("corner AB synthesizes");
        let mut expected = &candidates[0];
        for candidate in &candidates[1..] {
            if rank_cmp(
                candidate.alg.moves(),
                expected.alg.moves(),
                Orbit::Corner,
                &cfg,
            ) == Ordering::Less
            {
                expected = candidate;
            }
        }
        assert_eq!(hit.alg.format(), expected.alg.format());
        assert_eq!(hit.commutator, expected.commutator);
    }

    #[test]
    fn plateau_stays_inside_the_first_matching_setup() {
        fn setup_of(text: &str) -> Option<&str> {
            text.split_once(':').map(|(head, _)| head)
        }
        let (t1, t2) = case_of(Orbit::Corner, 'A', 'K');
        let candidates = synth_candidates(Orbit::Corner, t1, t2, &limits());
        assert!(candidates.len() >= 2);
        let q0 = candidates[0].alg.qtm();
        let setup = setup_of(&candidates[0].commutator);
        for candidate in &candidates {
            assert!(
                candidate.alg.qtm() <= q0,
                "{} exceeds the first-hit qtm {q0}",
                candidate.alg.format()
            );
            assert_eq!(
                setup_of(&candidate.commutator),
                setup,
                "{} leaves the first matching setup",
                candidate.commutator
            );
        }
    }

    #[test]
    fn scoring_flips_the_selected_alg_for_the_pinned_case() {
        let (t1, t2) = case_of(Orbit::Corner, 'A', 'K');
        let mut inverted = test_scoring();
        inverted.preferred = [false; 9];
        for lane in [Face::L.index(), Face::F.index(), Face::B.index()] {
            inverted.preferred[lane] = true;
        }
        inverted.preferred_edge = inverted.preferred;
        let cfg = test_scoring();
        let candidates = synth_candidates(Orbit::Corner, t1, t2, &limits());
        assert!(candidates.len() >= 2);
        let pick = |scoring: &crate::scoring::ScoringCfg| {
            candidates
                .clone()
                .into_iter()
                .reduce(|kept, next| {
                    if rank_cmp(next.alg.moves(), kept.alg.moves(), Orbit::Corner, scoring)
                        == Ordering::Less
                    {
                        next
                    } else {
                        kept
                    }
                })
                .expect("candidates select")
        };
        let base = pick(&cfg);
        let alt = pick(&inverted);
        assert_ne!(base.alg.format(), alt.alg.format());
        for hit in [base, alt] {
            assert_hit_matches_the_ideal_cycle(Orbit::Corner, t1, t2, &hit);
        }
    }

    #[test]
    fn generated_entries_fill_only_uncovered_cases() {
        let mut cases = CasesFile {
            cases: vec![
                crate::model::CaseEntry {
                    orbit: "edge".to_string(),
                    t1: 24,
                    t2: 25,
                    t1_letter: 'A',
                    t2_letter: 'B',
                    t1_name: "UB".to_string(),
                    t2_name: "UR".to_string(),
                    covered: true,
                    vendor_algs: vec![VendorAlg {
                        solver: "cherry".to_string(),
                        rowid: 2,
                        position: "UF-UB-UR".to_string(),
                        alg: "R2 U' S R2 S' R2 U R2".to_string(),
                        qtm: 12,
                        htm: 9,
                        ok: true,
                        problems: Vec::new(),
                        notes: Vec::new(),
                        commutator: "R2 U':[S,R2]".to_string(),
                        recon_link: String::new(),
                        youtube_link: String::new(),
                    }],
                },
                crate::model::CaseEntry {
                    orbit: "edge".to_string(),
                    t1: 29,
                    t2: 30,
                    t1_letter: 'F',
                    t2_letter: 'G',
                    t1_name: "LF".to_string(),
                    t2_name: "LD".to_string(),
                    covered: false,
                    vendor_algs: Vec::new(),
                },
            ],
        };
        let (edge, corner) = synthesize(&mut cases, &limits(), &test_scoring());
        assert_eq!(edge, 1);
        assert_eq!(corner, 0);
        assert_eq!(cases.cases[0].vendor_algs.len(), 1);
        assert_eq!(cases.cases[0].vendor_algs[0].solver, "cherry");
        assert!(cases.cases[0].covered);
        let generated = &cases.cases[1].vendor_algs[0];
        assert_eq!(generated.solver, "generated");
        assert_eq!(generated.position, "UF-LF-LD");
        assert!(generated.ok);
        assert!(generated.problems.is_empty());
        assert!(cases.cases[1].covered);
        let alg = Alg::parse(&generated.alg).unwrap();
        assert_eq!(alg.format(), generated.alg);
        assert!(decompose_verified(&generated.commutator, &alg).is_some());
    }
}
