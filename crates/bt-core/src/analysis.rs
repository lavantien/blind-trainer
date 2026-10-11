use crate::cube::Cube;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Piece {
    Corner(u8),
    Edge(u8),
}

const CORNER_STICKERS: [[u8; 3]; 8] = [
    [1, 16, 13],
    [0, 17, 4],
    [2, 9, 12],
    [3, 8, 5],
    [22, 19, 14],
    [23, 18, 7],
    [21, 10, 15],
    [20, 11, 6],
];

const EDGE_STICKERS: [[u8; 2]; 12] = [
    [24, 40],
    [25, 36],
    [26, 32],
    [27, 28],
    [41, 31],
    [43, 37],
    [33, 39],
    [35, 29],
    [46, 42],
    [45, 38],
    [44, 34],
    [47, 30],
];

const fn piece_lookup() -> [u8; 48] {
    let mut table = [255u8; 48];
    let mut piece = 0;
    while piece < 8 {
        let mut i = 0;
        while i < 3 {
            table[CORNER_STICKERS[piece][i] as usize] = piece as u8;
            i += 1;
        }
        piece += 1;
    }
    let mut piece = 0;
    while piece < 12 {
        let mut i = 0;
        while i < 2 {
            table[EDGE_STICKERS[piece][i] as usize] = piece as u8;
            i += 1;
        }
        piece += 1;
    }
    table
}

const PIECE_OF_STICKER: [u8; 48] = piece_lookup();

pub fn piece_of(sticker: u8) -> Piece {
    assert!(sticker < 48, "sticker {} is not a cubie sticker", sticker);
    if sticker < 24 {
        Piece::Corner(PIECE_OF_STICKER[sticker as usize])
    } else {
        Piece::Edge(PIECE_OF_STICKER[sticker as usize])
    }
}

pub fn head_sticker(piece: Piece) -> u8 {
    match piece {
        Piece::Corner(corner) => CORNER_STICKERS[corner as usize][0],
        Piece::Edge(edge) => EDGE_STICKERS[edge as usize][0],
    }
}

pub fn corner_stickers(corner: u8) -> [u8; 3] {
    assert!(corner < 8, "corner {} out of range", corner);
    CORNER_STICKERS[corner as usize]
}

pub fn edge_stickers(edge: u8) -> [u8; 2] {
    assert!(edge < 12, "edge {} out of range", edge);
    EDGE_STICKERS[edge as usize]
}

pub fn sticker_cycles(cube: &Cube) -> Vec<Vec<u8>> {
    let mut visited = [false; 48];
    let mut cycles = Vec::new();
    for start in 0..48u8 {
        if visited[start as usize] {
            continue;
        }
        let mut cycle = Vec::new();
        let mut current = start;
        while !visited[current as usize] {
            visited[current as usize] = true;
            cycle.push(current);
            current = cube.position_of(current);
        }
        if cycle.len() > 1 {
            let min = cycle.iter().copied().min().unwrap();
            let split = cycle.iter().position(|&s| s == min).unwrap();
            cycle.rotate_left(split);
            cycles.push(cycle);
        }
    }
    cycles.sort_by_key(|cycle| cycle[0]);
    cycles
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::alg::Alg;

    fn apply(text: &str) -> Cube {
        let mut cube = Cube::solved();
        cube.apply_alg(&Alg::parse(text).unwrap());
        cube
    }

    #[test]
    fn piece_tables_partition_the_sticker_space() {
        let mut corner_slots = [0u8; 8];
        let mut edge_slots = [0u8; 12];
        for sticker in 0..48u8 {
            match piece_of(sticker) {
                Piece::Corner(piece) => {
                    assert!(sticker < 24);
                    assert!(corner_stickers(piece).contains(&sticker));
                    corner_slots[piece as usize] += 1;
                }
                Piece::Edge(piece) => {
                    assert!(sticker >= 24);
                    assert!(edge_stickers(piece).contains(&sticker));
                    edge_slots[piece as usize] += 1;
                }
            }
        }
        assert!(corner_slots.iter().all(|&count| count == 3));
        assert!(edge_slots.iter().all(|&count| count == 2));
    }

    #[test]
    fn head_stickers_are_ud_or_equator_fb() {
        for corner in 0..8u8 {
            let head = head_sticker(Piece::Corner(corner));
            assert!(head < 24 && head / 4 == corner_stickers(corner)[0] / 4);
            assert!(head / 4 == 0 || head / 4 == 5);
        }
        for edge in 0..12u8 {
            let head = head_sticker(Piece::Edge(edge));
            let face = (head - 24) / 4;
            assert!(face == 0 || face == 5 || face == 2 || face == 4);
        }
    }

    #[test]
    fn cycles_follow_position_of_in_sorted_order() {
        for text in ["R", "F2", "x'", "u", "M E S"] {
            let cube = apply(text);
            let cycles = sticker_cycles(&cube);
            assert!(!cycles.is_empty());
            let mut firsts = Vec::new();
            for cycle in &cycles {
                assert_eq!(*cycle.iter().min().unwrap(), cycle[0]);
                firsts.push(cycle[0]);
                for pair in cycle.windows(2) {
                    assert_eq!(cube.position_of(pair[0]), pair[1]);
                }
                assert_eq!(cube.position_of(*cycle.last().unwrap()), cycle[0]);
            }
            let mut sorted = firsts.clone();
            sorted.sort();
            assert_eq!(firsts, sorted);
        }
    }

    #[test]
    fn cycle_count_matches_nonfixed_stickers() {
        for text in ["U", "R2", "M", "z2"] {
            let cube = apply(text);
            let moved = (0..48u8).filter(|&s| cube.position_of(s) != s).count();
            let cycled: usize = sticker_cycles(&cube).iter().map(|cycle| cycle.len()).sum();
            assert_eq!(moved, cycled, "{}", text);
        }
    }
}
