use bt_ga::hash::{sha256, sha256_hex};

pub fn site_equivalences() -> (u32, u32, u32, Vec<String>) {
    let mut ch_violations = 0u32;
    let mut maj_pair_violations = 0u32;
    let mut maj_total_violations = 0u32;
    let mut notes = Vec::new();
    for a in 0..2u8 {
        for b in 0..2u8 {
            for c in 0..2u8 {
                let ch_left = a & b;
                let ch_right = (!a) & c;
                if (ch_left ^ ch_right) != (ch_left | ch_right) {
                    ch_violations += 1;
                    notes.push(format!("ch site differs at e={a} f={b} g={c}"));
                }
                let pair_xor = (a & b) ^ (a & c);
                let pair_or = (a & b) | (a & c);
                if pair_xor != pair_or {
                    maj_pair_violations += 1;
                    notes.push(format!(
                        "maj left pair differs at a={a} b={b} c={c}: xor {pair_xor} or {pair_or}"
                    ));
                }
                let m1 = a & b;
                let m2 = a & c;
                let m3 = b & c;
                if (m1 ^ m2 ^ m3) != (m1 | m2 | m3) {
                    maj_total_violations += 1;
                    notes.push(format!("maj total differs at a={a} b={b} c={c}"));
                }
            }
        }
    }
    (ch_violations, maj_pair_violations, maj_total_violations, notes)
}

pub fn sha256_against_vectors(text: &str) -> (usize, usize, Vec<String>) {
    let mut ok = 0usize;
    let mut bad = 0usize;
    let mut fails = Vec::new();
    for line in text.lines() {
        if line.is_empty() {
            continue;
        }
        let cols: Vec<&str> = line.split('\t').collect();
        let expected = cols[1];
        let data: Vec<u8> = cols[2]
            .split(',')
            .filter(|s| !s.is_empty())
            .map(|v| v.parse().unwrap())
            .collect();
        let got = sha256_hex(&data);
        if got == expected {
            ok += 1;
        } else {
            bad += 1;
            if fails.len() < 5 {
                fails.push(format!("len {} expected {expected} got {got}", cols[0]));
            }
        }
        let raw = sha256(&data);
        if raw.len() != 32 {
            bad += 1;
        }
    }
    (ok, bad, fails)
}
