use bt_core::{Alg, Cube, Piece, corner_stickers, piece_of};

fn applied(alg: &str) -> Cube {
    let mut cube = Cube::solved();
    cube.apply_alg(&Alg::parse(alg).unwrap());
    cube
}

fn invert(text: &str) -> String {
    let alg = Alg::parse(text).unwrap();
    alg.inverse().format()
}

fn main() {
    let setups = [
        "R U R'", "R U' R'", "R' U R", "R' U' R", "L U L'", "L U' L'", "L' U L", "L' U' L",
        "F U F'", "F U' F'", "R D R'", "R D' R'",
    ];
    let triggers = ["D", "D'", "D2", "U", "E", "M"];
    for setup in setups {
        for trigger in triggers {
            let trigger_inv = invert(trigger);
            let alg_text = format!("{setup} {trigger} {} {trigger_inv}", invert(setup));
            let Ok(alg) = Alg::parse(&alg_text) else {
                continue;
            };
            let report = bt_scheme::classify_alg(&alg);
            if !matches!(
                report.class,
                bt_scheme::Classification::ThreeStyle {
                    orbit: bt_scheme::Orbit::Corner,
                    ..
                }
            ) {
                continue;
            }
            let cube = applied(&alg_text);
            let Some(start) = (0..24u8).find(|&s| cube.position_of(s) != s) else {
                continue;
            };
            let b = cube.position_of(start);
            let c = cube.position_of(b);
            if cube.position_of(c) != start {
                continue;
            }
            let mut pool = Vec::new();
            for s in [start, b, c] {
                if let Piece::Corner(i) = piece_of(s) {
                    pool.extend(corner_stickers(i));
                }
            }
            let pairs: Vec<(u8, u8)> = vec![
                (start, b),
                (b, c),
                (c, start),
                (b, start),
                (c, b),
                (start, c),
            ];
            let interesting = pairs.iter().any(|&(f, t)| diverges(f, t));
            println!("{alg_text} -> cycle {start} {b} {c} divergent={interesting}");
        }
    }
}

fn diverges(from: u8, to: u8) -> bool {
    let p = match piece_of(from) {
        Piece::Corner(i) => i,
        _ => return false,
    };
    let q = match piece_of(to) {
        Piece::Corner(i) => i,
        _ => return false,
    };
    let base = placement(p, from, q, to, false);
    placement(p, from, q, to, true) != base
}

fn placement(p: u8, frm: u8, q: u8, to: u8, mutant: bool) -> Vec<(u8, u8)> {
    let ps = rotate_to(corner_stickers(p), frm);
    let mut ss = rotate_to(corner_stickers(q), to);
    if chirality(&ps, mutant) != chirality(&ss, mutant) {
        ss.swap(1, 2);
    }
    ps.iter().zip(ss.iter()).map(|(a, b)| (*a, *b)).collect()
}

fn rotate_to(mut triple: [u8; 3], sticker: u8) -> [u8; 3] {
    let i = triple.iter().position(|&s| s == sticker).unwrap();
    triple.rotate_left(i);
    triple
}

fn chirality(triple: &[u8; 3], mutant: bool) -> i32 {
    let a = normal(triple[0]);
    let b = normal(triple[1]);
    let c = normal(triple[2]);
    let mut cross = [0i32; 3];
    for i in 0..3 {
        let (j, k) = [(1usize, 2usize), (2, 0), (0, 1)][i];
        let t1 = b[j] * c[k];
        let t2 = if mutant { b[k] + c[j] } else { b[k] * c[j] };
        cross[i] = t1 - t2;
    }
    (a[0] * cross[0] + a[1] * cross[1] + a[2] * cross[2]).signum()
}

fn normal(sticker: u8) -> [i32; 3] {
    let f = (sticker / 4) as usize;
    const N: [[i32; 3]; 6] = [
        [0, 1, 0],
        [-1, 0, 0],
        [0, 0, 1],
        [1, 0, 0],
        [0, 0, -1],
        [0, -1, 0],
    ];
    N[f]
}
