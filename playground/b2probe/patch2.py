import io

p = 'src/main.rs'
s = io.open(p, encoding='utf-8').read()

anchor = '''            println!(
                "   {} n={n} wings_bad={wings_bad} lines={lines}",
                row.case_key
            );'''
replacement = '''            let projected: Vec<String> = row
                .alg
                .split_whitespace()
                .filter_map(|token| {
                    let bytes = token.as_bytes();
                    if matches!(bytes.first(), Some(b'x' | b'y' | b'z')) {
                        return Some(token.to_string());
                    }
                    let wide = bytes.iter().any(|b| *b == b'w')
                        || token.chars().next().is_some_and(|c| c.is_ascii_lowercase());
                    if wide {
                        let letter = token.chars().find(|c| c.is_ascii_alphabetic()).unwrap().to_ascii_uppercase();
                        let suffix: String = token.chars().skip_while(|c| c.is_ascii_alphabetic() || c.is_ascii_digit() || *c == 'w').collect();
                        return Some(format!("{letter}{suffix}"));
                    }
                    if bytes[0].is_ascii_digit() {
                        return None;
                    }
                    Some(token.to_string())
                })
                .collect();
            let proj_text = projected.join(" ");
            let proj = Alg::parse(&proj_text).map_err(|e| e.to_string());
            let proj_report = match proj {
                Ok(alg) => {
                    let mut cube = Cube::solved();
                    cube.apply_alg(&alg);
                    let mut aligned = cube;
                    for cand in rotation_candidates() {
                        if cand.is_empty() {
                            continue;
                        }
                        let mut probe = cube;
                        probe.apply_alg(&Alg::parse(&cand).unwrap());
                        if probe.centers_fixed() {
                            aligned = probe;
                            break;
                        }
                    }
                    if !aligned.centers_fixed() {
                        "unalignable".to_string()
                    } else {
                        let cw: Vec<u8> = (0u8..24).filter(|s| aligned.sticker_at(*s) != *s).collect();
                        let ew: Vec<u8> = (24u8..48).filter(|s| aligned.sticker_at(*s) != *s).collect();
                        format!("core proj corners={} edges={}", cw.len(), ew.len())
                    }
                }
                Err(e) => format!("proj parse {e}"),
            };
            println!(
                "   {} n={n} wings_bad={wings_bad} lines={lines} {proj_report}",
                row.case_key
            );'''
assert anchor in s
s = s.replace(anchor, replacement)

s = s.replace('''    let mut fails = Vec::new();
    for row in rows {
        if let Err(e) = verify_tier1(&engine, &row.alg) {
            fails.push(format!("{} tier1 {e}", row.case_key));
            continue;
        }
        let mut state = engine.solved_state();
        engine.apply_alg(&mut state, &engine.invert_alg(&parsed)).unwrap();''', '''    let mut fails = Vec::new();
    let mut union_disturbed: BTreeSet<usize> = BTreeSet::new();
    for row in rows {
        if let Err(e) = verify_tier1(&engine, &row.alg) {
            fails.push(format!("{} tier1 {e}", row.case_key));
            continue;
        }
        let mut state = engine.solved_state();
        engine.apply_alg(&mut state, &engine.invert_alg(&parsed)).unwrap();''')

s = s.replace('''        let bad: Vec<usize> = (0..mega::FACELETS)
            .filter(|i| {
                let face = (*i / per_face) as u8;
                state.colors_slot(*i) != face
            })
            .collect();''', '''        let bad: Vec<usize> = (0..mega::FACELETS)
            .filter(|i| {
                let face = (*i / per_face) as u8;
                state.colors_slot(*i) != face
            })
            .collect();
        union_disturbed.extend(bad.iter().copied());''')

s = s.replace('''    println!("   tier2 fails={fails:?}");
}

trait MegaSlot''', '''    let untouched: Vec<String> = (0..mega::FACELETS)
        .filter(|i| !union_disturbed.contains(i))
        .map(|i| format!("f{}:{}", i / per_face, i % per_face))
        .collect();
    println!("   mega untouched slots: {untouched:?}");
    println!("   tier2 fails={fails:?}");
}

trait MegaSlot''')

s = s.replace('''        let bad: Vec<usize> = (0..72)
            .filter(|i| state.facelets[*i] != (i / 9) as u8)
            .collect();''', '''        let bad: Vec<usize> = (0..72)
            .filter(|i| state.facelets[*i] != (i / 9) as u8)
            .collect();
        union_disturbed.extend(bad.iter().copied());''')

s = s.replace('''        let outside: Vec<usize> = bad.iter().copied().filter(|i| !u_layer.contains(i)).collect();
        if !outside.is_empty() && (row.case_key == "l4t.05" || row.case_key == "l4t.09" || row.case_key == "l4t.36") {
            for i in &outside {
                println!("   fto {} non-U: face {} local {} color {}", row.case_key, i / 9, i % 9, state.facelets[*i]);
            }
        }''', '''        let outside: Vec<usize> = bad.iter().copied().filter(|i| !u_layer.contains(i)).collect();
        let _ = &outside;''')

s = s.replace('''    println!("   tier2 fails={fails:?}");
}

fn skewb_rows''', '''    let untouched: Vec<String> = (0..72)
        .filter(|i| !union_disturbed.contains(i))
        .map(|i| format!("f{}:{}", i / 9, i % 9))
        .collect();
    println!("   fto untouched slots: {untouched:?}");
    println!("   tier2 fails={fails:?}");
}

fn skewb_rows''')

s = s.replace('''    let mut fails = Vec::new();
    for row in rows {
        if let Err(e) = verify_tier1(&engine, &row.alg) {
            fails.push(format!("{} tier1 {e}", row.case_key));
            continue;
        }
        let parsed = engine.parse_alg(&row.alg).unwrap();
        let mut state = engine.solved_state();
        engine.apply_alg(&mut state, &engine.invert_alg(&parsed)).unwrap();
        let case_colors: Vec<u8> = engine''', '''    let mut fails = Vec::new();
    let mut union_disturbed: BTreeSet<usize> = BTreeSet::new();
    for row in rows {
        if let Err(e) = verify_tier1(&engine, &row.alg) {
            fails.push(format!("{} tier1 {e}", row.case_key));
            continue;
        }
        let parsed = engine.parse_alg(&row.alg).unwrap();
        let mut state = engine.solved_state();
        engine.apply_alg(&mut state, &engine.invert_alg(&parsed)).unwrap();
        let case_colors: Vec<u8> = engine''')

s = s.replace('''        let outside: Vec<usize> = disturbed.iter().copied().filter(|s| !edge_slots.contains(s)).collect();
        let edges_hit: BTreeSet<usize> = disturbed.iter().copied().filter(|s| edge_slots.contains(s)).collect();
        let _ = &edges_hit;''', '''        let outside: Vec<usize> = disturbed.iter().copied().filter(|s| !edge_slots.contains(s)).collect();
        union_disturbed.extend(disturbed.iter().copied());''')

s = s.replace('''    println!("   tier2 fails={fails:?}");
}

fn mega_4lll''', '''    let untouched: Vec<String> = (0..36)
        .filter(|i| !union_disturbed.contains(i))
        .map(|i| format!("f{}:{}", i / 9, i % 9))
        .collect();
    println!("   pyra untouched slots: {untouched:?}");
    println!("   tier2 fails={fails:?}");
}

fn mega_4lll''')

io.open(p, 'w', encoding='utf-8').write(s)
print('ok')
