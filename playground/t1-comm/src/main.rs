use bt_core::{Alg, Cube};

fn matches(candidate: &[String], target: &str) -> bool {
    let mut c = Cube::solved();
    for part in candidate {
        c.apply_alg(&Alg::parse(part).unwrap());
    }
    let mut t = Cube::solved();
    t.apply_alg(&Alg::parse(target).unwrap());
    c == t
}

fn inv(text: &str) -> String {
    Alg::parse(text).unwrap().inverse().format()
}

fn unit_forms(setup: Option<&str>, a: &str, b: &str) -> Vec<(String, String)> {
    let (ai, bi) = (inv(a), inv(b));
    let pairs: [(&str, &str, &str, &str, &str); 2] = [
        ("A-first", a, b, ai.as_str(), bi.as_str()),
        ("B-first", b, a, bi.as_str(), ai.as_str()),
    ];
    let mut out = Vec::new();
    for (order, first, second, first_inv, second_inv) in pairs {
        let bodies = [
            (format!("{first} {second} {first_inv} {second_inv}"), "full"),
            (format!("{first} {second} {first_inv}"), "noB2"),
            (format!("{first} {second}"), "ab"),
        ];
        for (body, kind) in bodies {
            match setup {
                Some(s) => {
                    out.push((format!("{s} {body} {}", inv(s)), format!("{order}/{kind}/conj")));
                    out.push((format!("{s} {body}"), format!("{order}/{kind}/open")));
                }
                None => out.push((body.clone(), format!("{order}/{kind}/bare"))),
            }
        }
    }
    out
}

fn search(name: &str, setup: Option<&str>, units: &[(Option<&str>, &str, &str)], target: &str) {
    println!("== {name} target: {target}");
    let forms: Vec<Vec<(String, String)>> =
        units.iter().map(|(s, a, b)| unit_forms(*s, a, b)).collect();
    let n = forms.len();
    let counts: Vec<usize> = forms.iter().map(|f| f.len()).collect();
    let total: usize = counts.iter().product();
    let mut found = 0;
    for code in 0..total {
        let mut idx = Vec::with_capacity(n);
        let mut rem = code;
        for &c in counts.iter().rev() {
            idx.push(rem % c);
            rem /= c;
        }
        idx.reverse();
        let mut seq: Vec<String> = Vec::new();
        if let Some(s) = setup {
            seq.push(s.to_string());
        }
        for (i, f) in forms.iter().enumerate() {
            seq.push(f[idx[i]].0.clone());
        }
        for tail in [true, false] {
            let mut candidate = seq.clone();
            if tail {
                if let Some(s) = setup {
                    candidate.push(inv(s));
                }
            } else if setup.is_none() {
                continue;
            }
            if matches(&candidate, target) {
                let desc: Vec<String> =
                    forms.iter().enumerate().map(|(i, f)| f[idx[i]].1.clone()).collect();
                println!(
                    "  MATCH outer_tail={tail} units=[{}]",
                    desc.join(", ")
                );
                found += 1;
            }
        }
    }
    if found == 0 {
        println!("  NO MATCH in search space");
    }
}

fn main() {
    search(
        "n1 flips UF&DR",
        Some("U"),
        &[(None, "R' E' R", "U2"), (Some("U2"), "S", "R2")],
        "U R' E' R U2 R' E R S R2 S' R2 U",
    );
    search(
        "n2 flips UR&FL",
        Some("S R'"),
        &[(None, "E", "R2"), (None, "F2", "R S' R'")],
        "S R' E R2 E' R2 F2 R S' R' F2 R",
    );
    search(
        "c1 combo UF-UL-BD",
        None,
        &[(None, "U'", "M"), (Some("M U"), "U", "M'")],
        "U' M U' M' U' M U' M'",
    );
    search(
        "c2 nested combo UF-RU-BD",
        Some("M'"),
        &[(None, "U'", "M"), (Some("M' U"), "U", "M")],
        "M' U' M' U' M U' M' U' M2",
    );
    search(
        "c3 nested combo UF-RF-LU",
        Some("R M U'"),
        &[(None, "M'", "U2"), (Some("U M'"), "x'", "R'")],
        "R M U' M' U2 M U' M' x' R' x",
    );
    search(
        "c4 combo UF-BD-UR",
        None,
        &[(None, "M", "U'"), (Some("U"), "U", "M")],
        "M U' M' U' M U' M' U'",
    );
    search(
        "f10 flips UF&DB",
        Some("M U"),
        &[(None, "R' F' R", "S"), (Some("U"), "M'", "U2")],
        "M U R' F' R S R' F R S' U M' U2",
    );
    search(
        "f13 flips UB&DF",
        Some("M U"),
        &[(None, "R' F' R", "S"), (Some("U"), "M'", "U2")],
        "U R' F' R S R' F R S' U M' U2 M",
    );
}
