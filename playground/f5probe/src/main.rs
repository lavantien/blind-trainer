fn main() {
    for text in ["R", "R R", "M2 M'", "R U", "[R,U]", "(R)2", ""] {
        let out = bt_core::expand(text);
        println!(
            "{:?} -> len {:?} fmt {:?}",
            text,
            out.as_ref().map(|alg| alg.len()),
            out.as_ref().map(|alg| alg.format())
        );
    }
    let parsed = bt_core::Alg::parse("R U R' U'");
    println!("parse R U R' U' -> {:?}", parsed.map(|alg| alg.format()));
}
