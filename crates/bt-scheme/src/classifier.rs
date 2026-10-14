use crate::pairs::CaseId;
use crate::scheme::{CORNER_FLOATING, EDGE_FLOATING, Orbit, head_sticker, name_of, same_piece};
use bt_core::head_sticker as piece_head;
use bt_core::{Cube, Piece, summarize};

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Trace {
    pub orbit: Orbit,
    pub blocks: Vec<Vec<u8>>,
}

impl Trace {
    pub fn is_solved(&self) -> bool {
        self.blocks.is_empty()
    }

    pub fn cases(&self) -> Vec<CaseId> {
        self.blocks
            .iter()
            .filter(|b| is_style_block(b))
            .map(|b| CaseId {
                orbit: self.orbit,
                t1: b[2],
                t2: b[1],
            })
            .collect()
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Classification {
    Solved,
    ThreeStyle {
        orbit: Orbit,
        cases: Vec<CaseId>,
    },
    FlipPair {
        first: u8,
        second: u8,
    },
    TwistPair {
        first: u8,
        second: u8,
    },
    Parity {
        e1: u8,
        e2: u8,
        c1: u8,
        c2: u8,
    },
    Ltct {
        e1: u8,
        e2: u8,
        c1: u8,
        c2: u8,
        bracket: u8,
    },
    Unclassified(String),
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Defects {
    edge_flips: [(u8, bool); 12],
    corner_twists: [(u8, i8); 8],
}

impl Defects {
    pub fn flip(&self, piece: u8) -> bool {
        self.edge_flips
            .iter()
            .find(|&&(head, _)| head == piece)
            .map(|&(_, flipped)| flipped)
            .unwrap_or_else(|| panic!("not an edge head: {piece}"))
    }

    pub fn twist(&self, piece: u8) -> i8 {
        self.corner_twists
            .iter()
            .find(|&&(head, _)| head == piece)
            .map(|&(_, twist)| twist)
            .unwrap_or_else(|| panic!("not a corner head: {piece}"))
    }

    pub fn flipped_pieces(&self) -> Vec<u8> {
        self.edge_flips
            .iter()
            .filter(|&&(_, flipped)| flipped)
            .map(|&(head, _)| head)
            .collect()
    }

    pub fn twisted_pieces(&self) -> Vec<(u8, i8)> {
        self.corner_twists
            .iter()
            .filter(|&&(_, twist)| twist != 0)
            .map(|&(head, twist)| (head, twist))
            .collect()
    }
}

fn defects(state: &Cube) -> Defects {
    let summary = summarize(state);
    let edge_flips = std::array::from_fn(|slot| {
        let edge = summary.edges[slot];
        (piece_head(Piece::Edge(edge.piece)), edge.flipped)
    });
    let corner_twists = std::array::from_fn(|slot| {
        let corner = summary.corners[slot];
        (piece_head(Piece::Corner(corner.piece)), corner.twist)
    });
    Defects {
        edge_flips,
        corner_twists,
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Report {
    pub edge: Trace,
    pub corner: Trace,
    pub class: Classification,
    pub defects: Defects,
    pub qtm: Option<u32>,
}

impl Report {
    pub fn position(&self) -> Option<String> {
        match &self.class {
            Classification::Solved | Classification::Unclassified(_) => None,
            Classification::ThreeStyle { orbit, .. } => {
                let trace = match orbit {
                    Orbit::Edge => &self.edge,
                    Orbit::Corner => &self.corner,
                };
                Some(
                    trace
                        .blocks
                        .iter()
                        .filter(|b| is_style_block(b))
                        .map(|b| format!("{}-{}-{}", name_of(b[0]), name_of(b[2]), name_of(b[1])))
                        .collect::<Vec<_>>()
                        .join(" "),
                )
            }
            Classification::FlipPair { first, second } => {
                Some(format!("{} & {}", name_of(*first), name_of(*second)))
            }
            Classification::TwistPair { first, second } => {
                let twist_first = self.defects.twist(*first).rem_euclid(3);
                let twist_second = self.defects.twist(*second).rem_euclid(3);
                let first_is_plus = !(twist_second == 1 && twist_first != 1);
                let (a, b) = if first_is_plus {
                    (name_of(*first), name_of(*second))
                } else {
                    (name_of(*second), name_of(*first))
                };
                Some(format!("{a} & {b}"))
            }
            Classification::Parity { e1, e2, c1, c2 } => Some(format!(
                "{}-{}, {}-{}",
                name_of(*e1),
                name_of(*e2),
                name_of(*c1),
                name_of(*c2)
            )),
            Classification::Ltct {
                e1,
                e2,
                c1,
                c2,
                bracket,
            } => Some(format!(
                "{}-{}, {}-{}[{}]",
                name_of(*e1),
                name_of(*e2),
                name_of(*c1),
                name_of(*c2),
                name_of(*bracket)
            )),
        }
    }
}

pub fn trace(state: &Cube, orbit: Orbit) -> Trace {
    let order: &[&str] = match orbit {
        Orbit::Edge => EDGE_FLOATING.as_slice(),
        Orbit::Corner => CORNER_FLOATING.as_slice(),
    };
    let mut visited = [false; 48];
    let mut blocks = Vec::new();
    for &piece in order {
        let head = head_sticker(piece);
        if orbit
            .stickers()
            .any(|s| same_piece(s, head) && visited[s as usize])
        {
            continue;
        }
        let mut block = vec![head];
        while state.sticker_at(block[block.len() - 1]) != head {
            block.push(state.sticker_at(block[block.len() - 1]));
        }
        if block.len() > 1 {
            block.push(head);
            for &s in &block {
                visited[s as usize] = true;
            }
            blocks.push(block);
        }
    }
    Trace { orbit, blocks }
}

pub fn classify(state: &Cube) -> Report {
    let edge = trace(state, Orbit::Edge);
    let corner = trace(state, Orbit::Corner);
    let class = classify_traces(&edge, &corner);
    Report {
        edge,
        corner,
        class,
        defects: defects(state),
        qtm: None,
    }
}

pub fn classify_alg(alg: &bt_core::Alg) -> Report {
    let mut cube = Cube::solved();
    cube.apply_alg(alg);
    let mut report = classify(&cube);
    report.qtm = Some(alg.qtm());
    report
}

fn classify_traces(edge: &Trace, corner: &Trace) -> Classification {
    if edge.is_solved() && corner.is_solved() {
        return Classification::Solved;
    }
    if let Some(class) = solo_style(edge, corner, Orbit::Edge) {
        return class;
    }
    if let Some(class) = solo_style(corner, edge, Orbit::Corner) {
        return class;
    }
    if corner.is_solved()
        && edge.blocks.len() == 2
        && is_flip_block(&edge.blocks[0])
        && is_flip_block(&edge.blocks[1])
    {
        return Classification::FlipPair {
            first: edge.blocks[0][0],
            second: edge.blocks[1][0],
        };
    }
    if edge.is_solved()
        && corner.blocks.len() == 2
        && is_twist_block(&corner.blocks[0])
        && is_twist_block(&corner.blocks[1])
    {
        return Classification::TwistPair {
            first: corner.blocks[0][0],
            second: corner.blocks[1][0],
        };
    }
    if edge.blocks.len() == 1 && is_swap_block(&edge.blocks[0]) {
        if corner.blocks.len() == 1 && is_swap_block(&corner.blocks[0]) {
            return Classification::Parity {
                e1: edge.blocks[0][0],
                e2: edge.blocks[0][1],
                c1: corner.blocks[0][0],
                c2: corner.blocks[0][1],
            };
        }
        if corner.blocks.len() == 2 {
            let ordered = if is_swap_twist_block(&corner.blocks[0])
                && is_twist_block(&corner.blocks[1])
            {
                Some((&corner.blocks[0], &corner.blocks[1]))
            } else if is_twist_block(&corner.blocks[0]) && is_swap_twist_block(&corner.blocks[1]) {
                Some((&corner.blocks[1], &corner.blocks[0]))
            } else {
                None
            };
            if let Some((seven, four)) = ordered {
                return Classification::Ltct {
                    e1: edge.blocks[0][0],
                    e2: edge.blocks[0][1],
                    c1: seven[0],
                    c2: seven[5],
                    bracket: four[1],
                };
            }
        }
    }
    Classification::Unclassified(format!(
        "edge:{} corner:{}",
        shape_sig(edge),
        shape_sig(corner)
    ))
}

fn solo_style(active: &Trace, other: &Trace, orbit: Orbit) -> Option<Classification> {
    if !other.is_solved() || active.blocks.len() != 1 || !is_style_block(&active.blocks[0]) {
        return None;
    }
    Some(Classification::ThreeStyle {
        orbit,
        cases: active.cases(),
    })
}

fn shape_sig(trace: &Trace) -> String {
    if trace.blocks.is_empty() {
        "solved".to_string()
    } else {
        format!(
            "{:?}",
            trace.blocks.iter().map(|b| b.len()).collect::<Vec<_>>()
        )
    }
}

fn is_style_block(block: &[u8]) -> bool {
    block.len() == 4
        && !same_piece(block[0], block[1])
        && !same_piece(block[0], block[2])
        && !same_piece(block[1], block[2])
}

fn is_flip_block(block: &[u8]) -> bool {
    block.len() == 3 && same_piece(block[0], block[1])
}

fn is_swap_block(block: &[u8]) -> bool {
    block.len() == 3 && !same_piece(block[0], block[1])
}

fn is_twist_block(block: &[u8]) -> bool {
    block.len() == 4
        && same_piece(block[0], block[1])
        && same_piece(block[0], block[2])
        && same_piece(block[1], block[2])
}

fn is_swap_twist_block(block: &[u8]) -> bool {
    block.len() == 7
        && (0..3).all(|i| same_piece(block[2 * i], block[0]))
        && (0..3).all(|i| same_piece(block[2 * i + 1], block[1]))
        && !same_piece(block[0], block[1])
}

#[cfg(test)]
mod tests {
    use super::*;
    use bt_core::Alg;

    fn after(alg: &str) -> Cube {
        let mut cube = Cube::solved();
        cube.apply_alg(&Alg::parse(alg).unwrap());
        cube
    }

    #[test]
    fn solved_cube_classifies_solved() {
        let report = classify(&Cube::solved());
        assert_eq!(report.class, Classification::Solved);
        assert!(report.edge.is_solved());
        assert!(report.corner.is_solved());
        assert_eq!(report.position(), None);
    }

    #[test]
    fn u_turn_walk_blocks_pinned() {
        let state = after("U");
        let edge = trace(&state, Orbit::Edge);
        let corner = trace(&state, Orbit::Corner);
        assert_eq!(edge.blocks, vec![vec![26, 25, 24, 27, 26]]);
        assert_eq!(corner.blocks, vec![vec![2, 1, 0, 3, 2]]);
        assert_eq!(
            classify(&state).class,
            Classification::Unclassified("edge:[5] corner:[5]".to_string())
        );
    }

    #[test]
    fn m2_double_edge_swap_is_unclassified() {
        let state = after("M2");
        let edge = trace(&state, Orbit::Edge);
        assert_eq!(edge.blocks, vec![vec![26, 46, 26], vec![24, 44, 24]]);
        assert!(trace(&state, Orbit::Corner).is_solved());
        let class = classify(&state).class;
        assert!(matches!(class, Classification::Unclassified(_)));
    }

    #[test]
    fn walk_follows_arrival_map_and_partitions_displaced_stickers() {
        for alg in [
            "U",
            "U'",
            "U2",
            "R",
            "R U R' U'",
            "F R U R' U' F'",
            "x y z M E S",
            "l2 2B' S2",
            "E2",
        ] {
            let state = after(alg);
            for orbit in [Orbit::Edge, Orbit::Corner] {
                let walked = trace(&state, orbit);
                let mut covered: Vec<u8> = Vec::new();
                for block in &walked.blocks {
                    assert_eq!(block.first(), block.last(), "{alg} {orbit:?} {block:?}");
                    for pair in block.windows(2) {
                        assert_eq!(
                            state.sticker_at(pair[0]),
                            pair[1],
                            "{alg} {orbit:?} {block:?}"
                        );
                    }
                    assert!(
                        orbit_floating(orbit)
                            .iter()
                            .any(|&p| head_sticker(p) == block[0]),
                        "{alg} {orbit:?} {block:?}"
                    );
                    covered.extend(block[..block.len() - 1].iter());
                }
                let mut sorted = covered.clone();
                sorted.sort_unstable();
                sorted.dedup();
                assert_eq!(covered.len(), sorted.len(), "{alg} {orbit:?}");
                for piece in orbit_floating(orbit) {
                    let head = head_sticker(piece);
                    let group: Vec<u8> =
                        orbit.stickers().filter(|&s| same_piece(s, head)).collect();
                    let displaced = group.iter().any(|&s| state.sticker_at(s) != s);
                    let represented = group.iter().any(|s| covered.contains(s));
                    assert_eq!(displaced, represented, "{alg} {orbit:?} {piece}");
                }
                assert!(
                    covered.iter().all(|&s| state.sticker_at(s) != s),
                    "{alg} {orbit:?}"
                );
                if let Some(first) = walked.blocks.first() {
                    let expected_head = orbit_floating(orbit)
                        .iter()
                        .map(|&p| head_sticker(p))
                        .find(|&h| state.sticker_at(h) != h)
                        .unwrap();
                    assert_eq!(first[0], expected_head, "{alg} {orbit:?}");
                }
            }
        }
    }

    #[test]
    fn block_shape_predicates_pin_the_corpus_shapes() {
        assert!(is_style_block(&[26, 25, 24, 27]));
        assert!(!is_style_block(&[26, 32, 25, 27]));
        assert!(is_flip_block(&[26, 32, 26]));
        assert!(!is_flip_block(&[26, 25, 26]));
        assert!(is_swap_block(&[26, 25, 26]));
        assert!(!is_swap_block(&[26, 32, 26]));
        assert!(is_twist_block(&[2, 12, 9, 2]));
        assert!(!is_twist_block(&[2, 12, 3, 2]));
        assert!(!is_twist_block(&[2, 9, 2]));
        assert!(is_swap_twist_block(&[2, 16, 9, 1, 12, 13, 2]));
        assert!(!is_swap_twist_block(&[2, 16, 9, 1, 12, 25, 2]));
    }

    #[test]
    fn twist_pair_position_keeps_first_when_first_is_plus() {
        let defects = Defects {
            edge_flips: std::array::from_fn(|slot| (piece_head(Piece::Edge(slot as u8)), false)),
            corner_twists: std::array::from_fn(|slot| {
                let head = piece_head(Piece::Corner(slot as u8));
                let twist = if slot == 2 || slot == 3 { 1 } else { 0 };
                (head, twist as i8)
            }),
        };
        let report = Report {
            edge: Trace {
                orbit: Orbit::Edge,
                blocks: Vec::new(),
            },
            corner: Trace {
                orbit: Orbit::Corner,
                blocks: Vec::new(),
            },
            class: Classification::TwistPair {
                first: 2,
                second: 3,
            },
            defects,
            qtm: None,
        };
        assert_eq!(report.position().unwrap(), "UFR & UFL");
    }

    fn orbit_floating(orbit: Orbit) -> &'static [&'static str] {
        match orbit {
            Orbit::Edge => &EDGE_FLOATING,
            Orbit::Corner => &CORNER_FLOATING,
        }
    }

    #[test]
    fn solved_state_carries_no_defects() {
        let defects = classify(&Cube::solved()).defects;
        assert!(defects.flipped_pieces().is_empty());
        assert!(defects.twisted_pieces().is_empty());
        for piece in EDGE_FLOATING {
            assert!(!defects.flip(head_sticker(piece)));
        }
        for piece in CORNER_FLOATING {
            assert_eq!(defects.twist(head_sticker(piece)), 0);
        }
    }

    #[test]
    fn slice_double_swap_flips_no_edges() {
        let defects = classify(&after("M2 E2")).defects;
        assert!(defects.flipped_pieces().is_empty());
    }

    #[test]
    fn corner_twists_sum_to_zero_mod_three_and_are_nonzero_after_r() {
        let report = classify(&after("R"));
        let twisted = report.defects.twisted_pieces();
        assert_eq!(twisted.len(), 4);
        let sum: i32 = twisted.iter().map(|&(_, t)| t as i32).sum();
        assert_eq!(sum % 3, 0);
        for piece in CORNER_FLOATING {
            let t = report.defects.twist(head_sticker(piece));
            assert_eq!(t, report.defects.twist(head_sticker(piece)));
        }
    }

    #[test]
    fn classify_alg_fills_qtm_and_matches_state_classification() {
        let alg = Alg::parse("R U R' U'").unwrap();
        let report = classify_alg(&alg);
        assert_eq!(report.qtm, Some(4));
        assert_eq!(report.class, classify(&after("R U R' U'")).class);
        assert_eq!(classify(&after("R U R' U'")).qtm, None);
    }

    #[test]
    #[should_panic]
    fn flip_accessor_rejects_corner_head() {
        classify(&Cube::solved()).defects.flip(head_sticker("UFR"));
    }

    #[test]
    #[should_panic]
    fn twist_accessor_rejects_edge_head() {
        classify(&Cube::solved()).defects.twist(head_sticker("UF"));
    }

    #[test]
    fn e2_leaves_corners_solved_with_two_edge_swap_blocks() {
        let state = after("E2");
        assert!(trace(&state, Orbit::Corner).is_solved());
        let edge = trace(&state, Orbit::Edge);
        assert_eq!(edge.blocks, vec![vec![33, 41, 33], vec![35, 43, 35]]);
        assert!(matches!(
            classify(&state).class,
            Classification::Unclassified(_)
        ));
        assert_eq!(classify(&state).position(), None);
    }
}
