use bt_core::{Alg, Cube, Piece, corner_stickers, piece_of};
use bt_data::corpus::SheetClass;
use bt_data::verify::verify;

fn applied(alg: &str) -> Cube {
    let mut cube = Cube::solved();
    cube.apply_alg(&Alg::parse(alg).unwrap());
    cube
}

fn main() {
    let text = "R U R' D' R U' R' D";
    let alg = Alg::parse(text).unwrap();
    let cube = applied(text);
    let start = (0..24u8).find(|&s| cube.position_of(s) != s).unwrap();
    let second = cube.position_of(start);
    let third = cube.position_of(second);
    println!("sticker cycle: {} {} {}", start, second, third);
    println!("names: {} {} {}", bt_scheme::name_of(start), bt_scheme::name_of(second), bt_scheme::name_of(third));
    let mut pool = Vec::new();
    for s in [start, second, third] {
        if let Piece::Corner(i) = piece_of(s) {
            pool.extend(corner_stickers(i));
        }
    }
    for a in &pool {
        for b in &pool {
            for c in &pool {
                if a == b || b == c || a == c { continue; }
                let cand = format!("{}-{}-{}", bt_scheme::name_of(*a), bt_scheme::name_of(*b), bt_scheme::name_of(*c));
                if verify(&alg, &cand, SheetClass::Corner).ok() {
                    println!("OK {}", cand);
                }
            }
        }
    }
}
