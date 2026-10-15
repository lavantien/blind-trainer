use bt_puzzles::mega::{self, Mega, MegaMove};
use bt_puzzles::{Geometry, ParseError, verify_tier1};
use proptest::prelude::*;
use std::path::PathBuf;

const PINNED_TOKENS: [&str; 16] = [
    "U", "U'", "U2", "U2'", "R", "R'", "R2", "R2'", "L", "L'", "F", "F'", "BR", "BR'", "y", "y'",
];

const REJECTED_TOKENS: &[&str] = &[
    "D", "D'", "D2", "B", "B'", "BL", "BL'", "UL", "UL'", "UR", "UR'", "DR", "DR'", "DL", "DL'",
    "L2", "L2'", "F2", "F2'", "BR2", "BR2'", "y2", "y2'", "x", "x'", "z", "u", "u'", "r", "r'",
    "f", "f'", "br", "br'", "l", "U3", "R3", "U0", "U''", "U'2", "2U", "UU", "UR'", "Rw", "Uw",
    "M", "E", "S", "e2", "-", ",", "R,", "(R", "U)", "U]", "[(R", "*2",
];

const PHI: f64 = std::f64::consts::GOLDEN_RATIO;
const FACE_VERTEX: [usize; mega::FACES] = [0, 4, 2, 3, 5, 1, 7, 11, 6, 10, 8, 9];
const VERTICES: [[(i64, i64); 3]; 12] = [
    [(0, 0), (1, 0), (0, 1)],
    [(0, 0), (-1, 0), (0, 1)],
    [(1, 0), (0, 1), (0, 0)],
    [(-1, 0), (0, 1), (0, 0)],
    [(0, 1), (0, 0), (1, 0)],
    [(0, -1), (0, 0), (1, 0)],
    [(0, 0), (1, 0), (0, -1)],
    [(0, 0), (-1, 0), (0, -1)],
    [(1, 0), (0, -1), (0, 0)],
    [(-1, 0), (0, -1), (0, 0)],
    [(0, 1), (0, 0), (-1, 0)],
    [(0, -1), (0, 0), (-1, 0)],
];

fn face_of_vertex(vertex: usize) -> usize {
    FACE_VERTEX.iter().position(|&f| f == vertex).unwrap()
}

fn coord_value(pair: (i64, i64)) -> f64 {
    pair.0 as f64 + pair.1 as f64 * PHI
}

fn vertex_xyz(index: usize) -> [f64; 3] {
    VERTICES[index].map(coord_value)
}

fn square(pair: (i64, i64)) -> (i64, i64) {
    (
        pair.0 * pair.0 + pair.1 * pair.1,
        2 * pair.0 * pair.1 + pair.1 * pair.1,
    )
}

fn exact_adjacency() -> Vec<Vec<usize>> {
    let mut out = vec![Vec::new(); 12];
    for (i, vi) in VERTICES.iter().enumerate() {
        for (j, vj) in VERTICES.iter().enumerate() {
            if i == j {
                continue;
            }
            let mut r1 = 0;
            let mut rphi = 0;
            for (ci, cj) in vi.iter().zip(vj.iter()) {
                let (a, b) = square((ci.0 - cj.0, ci.1 - cj.1));
                r1 += a;
                rphi += b;
            }
            if r1 == 4 && rphi == 0 {
                out[i].push(j);
            }
        }
    }
    out
}

fn face_adjacency() -> Vec<Vec<usize>> {
    mega::NEIGHBOR_CW
        .iter()
        .map(|ring| ring.iter().map(|&n| n as usize).collect())
        .collect()
}

fn unit(v: [f64; 3]) -> [f64; 3] {
    let norm = (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]).sqrt();
    [v[0] / norm, v[1] / norm, v[2] / norm]
}

fn rotate_cw(v: [f64; 3], axis: [f64; 3]) -> [f64; 3] {
    let rad = -(72.0f64).to_radians();
    let cos = rad.cos();
    let sin = rad.sin();
    let cross = [
        axis[1] * v[2] - axis[2] * v[1],
        axis[2] * v[0] - axis[0] * v[2],
        axis[0] * v[1] - axis[1] * v[0],
    ];
    let dot = axis[0] * v[0] + axis[1] * v[1] + axis[2] * v[2];
    [
        v[0] * cos + cross[0] * sin + axis[0] * dot * (1.0 - cos),
        v[1] * cos + cross[1] * sin + axis[1] * dot * (1.0 - cos),
        v[2] * cos + cross[2] * sin + axis[2] * dot * (1.0 - cos),
    ]
}

fn dist2(a: [f64; 3], b: [f64; 3]) -> f64 {
    let d = [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
    d[0] * d[0] + d[1] * d[1] + d[2] * d[2]
}

fn nearest(target: [f64; 3], cands: &[usize]) -> usize {
    *cands
        .iter()
        .min_by(|&&a, &&b| dist2(target, vertex_xyz(a)).total_cmp(&dist2(target, vertex_xyz(b))))
        .unwrap()
}

fn derived_ring(face: usize) -> Vec<usize> {
    let vertex = FACE_VERTEX[face];
    let axis = unit(vertex_xyz(vertex));
    let neighbors = exact_adjacency()[vertex].clone();
    let start = *neighbors
        .iter()
        .min_by_key(|&&v| face_of_vertex(v))
        .unwrap();
    let mut order = Vec::new();
    let mut cur = start;
    for _ in 0..5 {
        order.push(cur);
        cur = nearest(rotate_cw(vertex_xyz(cur), axis), &neighbors);
    }
    assert_eq!(cur, start);
    order.iter().map(|&v| face_of_vertex(v)).collect()
}

fn derived_y_perm() -> Vec<usize> {
    let axis = unit(vertex_xyz(FACE_VERTEX[mega::U as usize]));
    let all: Vec<usize> = (0..12).collect();
    (0..mega::FACES)
        .map(|f| face_of_vertex(nearest(rotate_cw(vertex_xyz(FACE_VERTEX[f]), axis), &all)))
        .collect()
}

fn face_token(face: u8) -> &'static str {
    match face {
        mega::U => "U",
        mega::R => "R",
        mega::L => "L",
        mega::F => "F",
        _ => "BR",
    }
}

fn colors_after(text: &str) -> mega::MegaState {
    let engine = Mega;
    let alg = engine.parse_alg(text).unwrap();
    let mut state = engine.solved_state();
    engine.apply_alg(&mut state, &alg).unwrap();
    state
}

fn all_slots() -> impl Iterator<Item = u8> {
    [mega::CENTER]
        .into_iter()
        .chain((0..5).map(mega::corner_slot))
        .chain((0..5).map(mega::edge_slot))
}

fn moved_slots(before: &mega::MegaState, after: &mega::MegaState, face: u8) -> Vec<u8> {
    all_slots()
        .filter(|&slot| before.color(face, slot) != after.color(face, slot))
        .collect()
}

fn ref_ground() -> PathBuf {
    match std::env::var("BT_REF_GROUND") {
        Ok(raw) if !raw.is_empty() => PathBuf::from(raw),
        _ => PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .parent()
            .unwrap()
            .parent()
            .unwrap()
            .join("ref_ground"),
    }
}

fn tsv_rows() -> Vec<[String; 5]> {
    let text = std::fs::read_to_string(ref_ground().join("packs/megaminx_4lll.tsv")).unwrap();
    text.split('\n')
        .filter(|line| !line.trim().is_empty())
        .map(|line| line.trim_end_matches('\r'))
        .skip(1)
        .map(|line| {
            line.split('\t')
                .map(str::to_string)
                .collect::<Vec<_>>()
                .try_into()
                .unwrap()
        })
        .collect()
}

fn strip_grouping(alg: &str) -> String {
    alg.chars().filter(|ch| *ch != '(' && *ch != ')').collect()
}

#[test]
fn pinned_tokens_parse_format_and_round_trip() {
    let engine = Mega;
    for token in PINNED_TOKENS {
        let mv = engine.parse_move(token).unwrap();
        let alg = engine.parse_alg(token).unwrap();
        assert_eq!(alg.moves(), &[mv], "token {}", token);
        assert_eq!(engine.format_alg(&alg), token, "token {}", token);
        assert_eq!(verify_tier1(&engine, token), Ok(()), "token {}", token);
    }
}

#[test]
fn rejects_tokens_outside_the_pinned_grammar() {
    let engine = Mega;
    for token in REJECTED_TOKENS {
        assert_eq!(
            engine.parse_move(token),
            Err(ParseError::UnknownToken(token.to_string())),
            "token {}",
            token
        );
    }
}

#[test]
fn rejects_rogers_twelve_face_vocabulary_in_algs() {
    let engine = Mega;
    for text in [
        "D", "R D R'", "B L B'", "BL U BL'", "UR2 U'", "y DR", "x U x'", "U2 D2",
    ] {
        assert!(
            matches!(engine.parse_alg(text), Err(ParseError::UnknownToken(_))),
            "text {}",
            text
        );
    }
}

#[test]
fn row_4lll_33_and_raw_grouping_are_wave_b_normalizer_scope() {
    let engine = Mega;
    let raw = "[(R U R' U) (R' U' R2 U') (R' U R' U R) U] *2";
    assert_eq!(
        engine.parse_alg(raw),
        Err(ParseError::UnknownToken("[(R".to_string()))
    );
    assert_eq!(
        engine.parse_alg("F (R U R' U') F'"),
        Err(ParseError::UnknownToken("(R".to_string()))
    );
}

#[test]
fn adjacency_closes_symmetrically() {
    let adj = face_adjacency();
    for (f, list) in adj.iter().enumerate() {
        assert_eq!(list.len(), 5, "face {} degree", f);
        let mut sorted = list.clone();
        sorted.sort_unstable();
        sorted.dedup();
        assert_eq!(sorted.len(), 5, "face {} neighbors distinct", f);
        assert!(!list.contains(&f), "face {} self adjacent", f);
        for &n in list {
            assert!(adj[n].contains(&f), "faces {} {} asymmetric", f, n);
        }
    }
}

proptest! {
    #[test]
    fn adjacency_closure_holds_for_any_face(face in 0usize..mega::FACES) {
        let adj = face_adjacency();
        prop_assert_eq!(adj[face].len(), 5);
        for &n in &adj[face] {
            prop_assert_ne!(n, face);
            prop_assert!(adj[n].contains(&face));
            prop_assert!(n < mega::FACES);
        }
    }
}

#[test]
fn adjacency_matches_the_golden_icosahedron_exactly() {
    let exact = exact_adjacency();
    for (f, ring) in mega::NEIGHBOR_CW.iter().enumerate() {
        let mut derived: Vec<usize> = exact[FACE_VERTEX[f]]
            .iter()
            .map(|&v| face_of_vertex(v))
            .collect();
        let mut pinned: Vec<usize> = ring.iter().map(|&n| n as usize).collect();
        derived.sort_unstable();
        pinned.sort_unstable();
        assert_eq!(derived, pinned, "face {} adjacency", f);
    }
}

#[test]
fn graph_distances_pin_the_antipodes() {
    let adj = face_adjacency();
    let antipode = |source: usize| -> usize {
        let mut dist = vec![None; mega::FACES];
        dist[source] = Some(0);
        let mut queue = std::collections::VecDeque::new();
        queue.push_back(source);
        while let Some(v) = queue.pop_front() {
            for &n in &adj[v] {
                if dist[n].is_none() {
                    dist[n] = Some(dist[v].unwrap() + 1);
                    queue.push_back(n);
                }
            }
        }
        let mut counts = [0usize; 4];
        for d in &dist {
            counts[d.unwrap()] += 1;
        }
        assert_eq!(counts, [1, 5, 5, 1], "distance profile from {}", source);
        dist.iter().position(|d| *d == Some(3)).unwrap()
    };
    for source in 0..mega::FACES {
        let far = antipode(source);
        assert_ne!(far, source);
        assert_eq!(antipode(far), source);
    }
    assert_eq!(antipode(mega::U as usize), mega::D as usize);
    assert_eq!(antipode(mega::F as usize), mega::B as usize);
    assert_eq!(antipode(mega::R as usize), mega::UL as usize);
    assert_eq!(antipode(mega::L as usize), mega::UR as usize);
    assert_eq!(antipode(mega::BR as usize), mega::DL as usize);
    assert_eq!(antipode(mega::BL as usize), mega::DR as usize);
}

#[test]
fn ring_orders_form_a_valid_rotation_system() {
    let adj = face_adjacency();
    for direction in [false, true] {
        let mut visited = [false; 12 * 12];
        let mut orbit_count = 0;
        for g in 0..mega::FACES {
            for &h in &adj[g] {
                let mut dart = g * 12 + h;
                if visited[dart] {
                    continue;
                }
                let mut orbit = Vec::new();
                while !visited[dart] {
                    visited[dart] = true;
                    let from = dart / 12;
                    let to = dart % 12;
                    orbit.push((from, to));
                    let ring = &mega::NEIGHBOR_CW[to];
                    let pos = ring.iter().position(|&n| n as usize == from).unwrap();
                    let next = if direction {
                        ring[(pos + 1) % 5] as usize
                    } else {
                        ring[(pos + 4) % 5] as usize
                    };
                    dart = to * 12 + next;
                }
                assert_eq!(orbit.len(), 3, "orbit direction {}", direction);
                let (a, b) = (orbit[0].0, orbit[0].1);
                let c = orbit[1].1;
                assert_ne!(c, a);
                assert_ne!(c, b);
                assert!(adj[a].contains(&b));
                assert!(adj[a].contains(&c));
                assert!(adj[b].contains(&c));
                orbit_count += 1;
            }
        }
        assert_eq!(orbit_count, 20, "corners direction {}", direction);
    }
}

#[test]
fn ring_orders_match_independent_rotation_derivation() {
    for (f, ring) in mega::NEIGHBOR_CW.iter().enumerate() {
        let derived = derived_ring(f);
        let pinned = ring.iter().map(|&n| n as usize).collect::<Vec<_>>();
        assert_eq!(derived, pinned, "face {} ring", f);
    }
    assert_eq!(
        mega::NEIGHBOR_CW[mega::U as usize],
        [mega::F, mega::L, mega::BL, mega::BR, mega::R]
    );
}

#[test]
fn y_perm_matches_independent_derivation_and_structure() {
    let derived = derived_y_perm();
    for (f, &img) in mega::Y_PERM.iter().enumerate() {
        assert_eq!(img as usize, derived[f], "y image of {}", f);
    }
    assert_eq!(mega::Y_PERM[mega::U as usize], mega::U);
    assert_eq!(mega::Y_PERM[mega::D as usize], mega::D);
    let ring = &mega::NEIGHBOR_CW[mega::U as usize];
    for k in 0..5 {
        assert_eq!(
            mega::Y_PERM[ring[k] as usize],
            ring[(k + 1) % 5],
            "y ring step {}",
            k
        );
    }
    for (g, ring) in mega::NEIGHBOR_CW.iter().enumerate() {
        let image: Vec<u8> = ring.iter().map(|&n| mega::Y_PERM[n as usize]).collect();
        let target = &mega::NEIGHBOR_CW[mega::Y_PERM[g] as usize];
        let mut shifted = false;
        for t in 0..5 {
            let candidate: Vec<u8> = (0..5).map(|k| target[(k + t) % 5]).collect();
            if candidate == image {
                shifted = true;
            }
        }
        assert!(shifted, "y rotation preserving at face {}", g);
    }
}

#[test]
fn quarter_turn_anatomy_moves_the_layer_strips() {
    let engine = Mega;
    for face in [mega::U, mega::R, mega::L, mega::F, mega::BR] {
        let text = face_token(face);
        let before = engine.solved_state();
        let after = colors_after(text);
        assert!(!engine.is_solved(&after), "face {} breaks solved", face);
        let mut total = 0;
        for probe in 0..mega::FACES as u8 {
            let moved = moved_slots(&before, &after, probe);
            total += moved.len();
            if probe == face {
                assert!(
                    moved.is_empty(),
                    "face {} stays uniform, got {:?}",
                    face,
                    moved
                );
                assert_eq!(after.color(face, mega::CENTER), face);
            } else if mega::NEIGHBOR_CW[face as usize].contains(&probe) {
                assert_eq!(moved.len(), 3, "neighbor {} of {}", probe, face);
                let ring = &mega::NEIGHBOR_CW[probe as usize];
                let pos = ring.iter().position(|&n| n == face).unwrap();
                assert!(moved.contains(&mega::edge_slot((pos + 4) % 5)));
                assert!(moved.contains(&mega::corner_slot(pos)));
                assert!(moved.contains(&mega::corner_slot((pos + 4) % 5)));
            } else {
                assert!(
                    moved.is_empty(),
                    "face {} untouched, got {:?}",
                    probe,
                    moved
                );
            }
        }
        assert_eq!(total, 15, "face {} visible strip stickers", face);
        let mut state = engine.solved_state();
        for _ in 0..5 {
            engine
                .apply_alg(&mut state, &engine.parse_alg(text).unwrap())
                .unwrap();
        }
        assert!(engine.is_solved(&state), "face {} order 5", face);
    }
}

#[test]
fn strip_donors_follow_the_cw_ring() {
    for face in [mega::U, mega::R, mega::L, mega::F, mega::BR] {
        let before = Mega.solved_state();
        let after = colors_after(face_token(face));
        let ring = &mega::NEIGHBOR_CW[face as usize];
        for k in 0..5 {
            let target = ring[(k + 1) % 5];
            let donor = ring[k];
            let moved = moved_slots(&before, &after, target);
            assert_eq!(moved.len(), 3, "target {}", target);
            for slot in moved {
                assert_eq!(
                    after.color(target, slot),
                    donor,
                    "face {} ring slot {} target {}",
                    face,
                    k,
                    target
                );
            }
        }
    }
}

#[test]
fn conjugation_by_y_moves_turns_around_the_u_ring() {
    assert_eq!(colors_after("y R y'"), colors_after("BR"));
    assert_eq!(colors_after("y' R y"), colors_after("F"));
    assert_eq!(colors_after("y' BR y"), colors_after("R"));
}

#[test]
fn y_shares_the_u_axis() {
    let engine = Mega;
    assert_eq!(colors_after("y U y'"), colors_after("U"));
    assert_eq!(colors_after("y U"), colors_after("U y"));
    let mut state = colors_after("y");
    assert!(!engine.is_solved(&state));
    for face in 0..mega::FACES as u8 {
        let head = state.color(face, mega::CENTER);
        for slot in all_slots() {
            assert_eq!(
                state.color(face, slot),
                head,
                "face {} uniform after y",
                face
            );
        }
    }
    for _ in 0..4 {
        engine
            .apply_alg(&mut state, &engine.parse_alg("y").unwrap())
            .unwrap();
    }
    assert!(engine.is_solved(&state));
}

proptest! {
    #[test]
    fn random_algs_pass_tier1_and_round_trips(
        text in prop::collection::vec(proptest::sample::select(Vec::from(PINNED_TOKENS)), 0..12)
    ) {
        let engine = Mega;
        let joined = text.join(" ");
        prop_assert_eq!(verify_tier1(&engine, &joined), Ok(()));
        let alg = engine.parse_alg(&joined).unwrap();
        prop_assert_eq!(engine.format_alg(&alg), joined);
        let inverse = engine.invert_alg(&alg);
        prop_assert_eq!(engine.invert_alg(&inverse), alg.clone());
        let mut state = engine.solved_state();
        engine.apply_alg(&mut state, &alg).unwrap();
        let mut counts = [0usize; mega::FACES];
        for face in 0..mega::FACES as u8 {
            for slot in all_slots() {
                counts[state.color(face, slot) as usize] += 1;
            }
        }
        prop_assert!(counts.iter().all(|&c| c == 11));
        engine.apply_alg(&mut state, &inverse).unwrap();
        prop_assert!(engine.is_solved(&state));
        let mut reverse = engine.solved_state();
        engine.apply_alg(&mut reverse, &inverse).unwrap();
        engine.apply_alg(&mut reverse, &alg).unwrap();
        prop_assert!(engine.is_solved(&reverse));
    }
}

#[test]
fn tsv_rows_all_pass_tier1_except_the_wave_b_row() {
    let engine = Mega;
    let rows = tsv_rows();
    assert_eq!(rows.len(), 39);
    let mut failing = Vec::new();
    for row in &rows {
        let normalized = strip_grouping(&row[1]);
        if normalized
            .split_whitespace()
            .any(|token| engine.parse_move(token).is_err())
        {
            failing.push(row[0].clone());
        }
    }
    assert_eq!(failing, ["4lll.33"]);
    for row in &rows {
        if row[0] == "4lll.33" {
            continue;
        }
        let normalized = strip_grouping(&row[1]);
        assert_eq!(
            verify_tier1(&engine, &normalized),
            Ok(()),
            "row {} alg {}",
            row[0],
            normalized
        );
    }
}

#[test]
fn tsv_row_33_bracket_tokens_are_pinned() {
    let engine = Mega;
    let rows = tsv_rows();
    let row = rows.iter().find(|r| r[0] == "4lll.33").unwrap();
    let normalized = strip_grouping(&row[1]);
    let bad: Vec<&str> = normalized
        .split_whitespace()
        .filter(|t| engine.parse_move(t).is_err())
        .collect();
    assert_eq!(bad, ["[R", "U]", "*2"]);
}

#[test]
fn facelet_grid_layout_pins_the_pentagon() {
    let engine = Mega;
    let solved = engine.facelets(&engine.solved_state());
    assert_eq!(solved.faces.len(), 12);
    for (face, view) in solved.faces.iter().enumerate() {
        let widths: Vec<usize> = view.rows.iter().map(Vec::len).collect();
        assert_eq!(widths, [1, 2, 3, 3, 2], "face {} widths", face);
        for row in &view.rows {
            assert!(row.iter().all(|&cell| cell == face as u8));
        }
    }
    let after_r = engine.facelets(&colors_after("R"));
    let top = &after_r.faces[mega::U as usize];
    assert_eq!(top.rows[0], vec![mega::U]);
    assert_eq!(top.rows[1], vec![mega::U, mega::U]);
    assert_eq!(top.rows[2], vec![mega::F, mega::U, mega::U]);
    assert_eq!(top.rows[3], vec![mega::F, mega::U, mega::U]);
    assert_eq!(top.rows[4], vec![mega::F, mega::U]);
}

#[test]
fn empty_alg_is_the_identity() {
    let engine = Mega;
    let alg = engine.parse_alg("").unwrap();
    assert!(alg.is_empty());
    assert_eq!(engine.format_alg(&alg), "");
    assert_eq!(verify_tier1(&engine, ""), Ok(()));
}

#[test]
fn move_type_reports_face_and_steps() {
    let engine = Mega;
    assert_eq!(
        engine.parse_move("BR'"),
        Ok(MegaMove::Turn {
            face: mega::BR,
            steps: 4
        })
    );
    assert_eq!(engine.parse_move("y'"), Ok(MegaMove::Spin { steps: 4 }));
    assert_eq!(
        engine.parse_move("U2'"),
        Ok(MegaMove::Turn {
            face: mega::U,
            steps: 3
        })
    );
    let inverse = engine.invert_alg(&engine.parse_alg("U2' y BR").unwrap());
    assert_eq!(engine.format_alg(&inverse), "BR' y' U2");
}

#[test]
fn only_pinned_moves_are_legal_constructions() {
    for token in PINNED_TOKENS {
        assert!(
            mega::is_pinned_move(Mega.parse_move(token).unwrap()),
            "{}",
            token
        );
    }
    for mv in [
        MegaMove::Turn {
            face: mega::D,
            steps: 1,
        },
        MegaMove::Turn {
            face: mega::UL,
            steps: 2,
        },
        MegaMove::Turn { face: 12, steps: 1 },
        MegaMove::Turn {
            face: mega::U,
            steps: 0,
        },
        MegaMove::Turn {
            face: mega::U,
            steps: 5,
        },
        MegaMove::Spin { steps: 0 },
        MegaMove::Spin { steps: 5 },
    ] {
        assert!(!mega::is_pinned_move(mv), "{:?}", mv);
    }
}
