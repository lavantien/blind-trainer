fn to_linear(c: f32) -> f32 {
    if c <= 0.04045 {
        c / 12.92
    } else {
        ((c + 0.055) / 1.055).powf(2.4)
    }
}

fn main() {
    let shades: [(f32, f32, f32); 12] = [
        (0.95, 0.95, 0.95),
        (0.95, 0.60, 0.15),
        (0.20, 0.75, 0.35),
        (0.85, 0.20, 0.20),
        (0.20, 0.45, 0.90),
        (0.95, 0.85, 0.15),
        (0.15, 0.85, 0.90),
        (0.90, 0.35, 0.80),
        (0.55, 0.30, 0.85),
        (0.60, 0.40, 0.20),
        (0.55, 0.58, 0.62),
        (0.10, 0.55, 0.55),
    ];
    let linear: Vec<(f32, f32, f32)> = shades
        .iter()
        .map(|&(r, g, b)| (to_linear(r), to_linear(g), to_linear(b)))
        .collect();
    let mut min = f32::MAX;
    let mut min_pair = (0usize, 0usize);
    for i in 0..12 {
        for j in (i + 1)..12 {
            let (a, b) = (linear[i], linear[j]);
            let d = ((a.0 - b.0).powi(2) + (a.1 - b.1).powi(2) + (a.2 - b.2).powi(2)).sqrt();
            if d < min {
                min = d;
                min_pair = (i, j);
            }
        }
    }
    println!("min pairwise linear distance {:.4} at {:?}", min, min_pair);
    for (i, l) in linear.iter().enumerate() {
        println!("{i}: {:.4} {:.4} {:.4}", l.0, l.1, l.2);
    }
}
