use bt_data::seedpack::normalize_alg;
use bt_puzzles::sq1::{self, Move, Sq1, State};
use bt_puzzles::{Geometry, verify_tier1};

const UNITS: usize = 12;
const UNIT_TOTAL: usize = 24;

fn state_with_units(units: [u8; UNIT_TOTAL]) -> State {
    State {
        units,
        slice_flipped: false,
    }
}

fn seq() -> [u8; UNIT_TOTAL] {
    let mut units = [0u8; UNIT_TOTAL];
    for (i, slot) in units.iter_mut().enumerate() {
        *slot = i as u8;
    }
    units
}

fn apply_raw(state: &mut State, top: i32, bottom: i32) {
    sq1::apply(state, Move::Twist { top, bottom }).expect("apply raw twist");
}

fn main() {
    let mut defects: Vec<String> = Vec::new();

    println!("== 1. full 24-unit state identity, (0,-6) vs (0,6) and (-6,0) vs (6,0)");
    for (ta, tb) in [(-6i32, 0i32), (0, -6)] {
        let canon = (-ta, -tb);
        let mut a = state_with_units(seq());
        let mut b = state_with_units(seq());
        apply_raw(&mut a, ta, tb);
        apply_raw(&mut b, canon.0, canon.1);
        if a != b {
            defects.push(format!(
                "state mismatch for ({ta},{tb}) vs ({},{})",
                canon.0, canon.1
            ));
        }
        println!(
            "({ta},{tb}) vs ({},{}): units equal={} slice_flipped equal={}",
            canon.0,
            canon.1,
            a.units == b.units,
            a.slice_flipped == b.slice_flipped
        );
    }

    println!("== 2. exhaustive engine alias map, every (t,b) in -8..=8 except (0,0)");
    let ring_canon = |v: i32| {
        let r = v.rem_euclid(UNITS as i32);
        if r > 6 { r - UNITS as i32 } else { r }
    };
    for t in -8i32..=8 {
        for b in -8i32..=8 {
            if t == 0 && b == 0 {
                continue;
            }
            let (ct, cb) = (ring_canon(t), ring_canon(b));
            let mut raw = state_with_units(seq());
            let mut canon_state = state_with_units(seq());
            apply_raw(&mut raw, t, b);
            apply_raw(&mut canon_state, ct, cb);
            if raw != canon_state && !(ct == 0 && cb == 0) {
                defects.push(format!("engine alias broken for ({t},{b})"));
            }
            let aliases = (t, b) != (ct, cb);
            if aliases {
                println!("engine-level alias: ({t},{b}) == ({ct},{cb}) as permutations");
            }
        }
    }

    println!("== 3. loader domain, every text tuple over t,b in -8..=8 plus odd spellings");
    let mut accepted: Vec<(i64, i64)> = Vec::new();
    for t in -8i64..=8 {
        for b in -8i64..=8 {
            let text = format!("({t},{b})");
            let out = normalize_alg("sq1", &text);
            let both_zero = t == 0 && b == 0;
            let in_domain = (-6..=6).contains(&t) && (-6..=6).contains(&b) && !both_zero;
            match (&out, in_domain) {
                (Ok(_), true) => accepted.push((t, b)),
                (Ok(_), false) => defects.push(format!("loader accepted out-of-domain {text}")),
                (Err(_), true) => defects.push(format!("loader rejected in-domain {text}")),
                (Err(_), false) => {}
            }
            if let Ok(out) = out {
                let has_wrap = out.notes.iter().any(|n| n.starts_with("wraparound:"));
                let should_wrap = t == -6 || b == -6;
                if has_wrap != should_wrap {
                    defects.push(format!(
                        "wrap note wrong for {text}: {has_wrap} vs {should_wrap}"
                    ));
                }
            }
        }
    }
    println!(
        "loader accepted {} tuples, all in -6..=6 minus (0,0)",
        accepted.len()
    );

    println!("== 4. stored token domain equals engine parse domain");
    let engine = Sq1;
    for &(t, b) in &accepted {
        let out = normalize_alg("sq1", &format!("({t},{b})")).expect("accepted above");
        if engine.parse_alg(&out.alg).is_err() {
            defects.push(format!("stored alg {} unparsable by engine", out.alg));
        }
    }
    for text in [
        "(7,0)", "(0,7)", "(8,1)", "(6,7)", "(1,-7)", "(-7,0)", "(0,-7)", "(18,0)", "(0,18)",
        "(-18,0)", "(6,-6)", "(-6,6)", "(-6,-6)", "(0,0)",
    ] {
        let loader_rejects = normalize_alg("sq1", text).is_err();
        let engine_rejects = engine.parse_alg(text).is_err();
        println!("{text}: loader_rejects={loader_rejects} engine_rejects={engine_rejects}");
        if !loader_rejects && engine_rejects {
            defects.push(format!(
                "loader accepts {text} while engine rejects the same text"
            ));
        }
    }

    println!("== 5. odd spellings the engine rejects");
    for text in [
        "(06,0)", "(-0,3)", "(+6,0)", "(6 ,0)", "( 6,0)", "(6,)", "(,6)",
    ] {
        let out = normalize_alg("sq1", text);
        let engine_rejects = engine.parse_alg(text).is_err();
        match out {
            Ok(out) => println!(
                "{text}: loader ACCEPTS -> alg={:?} notes={:?} engine_rejects_raw={}",
                out.alg, out.notes, engine_rejects
            ),
            Err(_) => println!("{text}: loader rejects, engine_rejects_raw={engine_rejects}"),
        }
    }

    println!("== 6. real ep.96 row end to end");
    let raw = std::fs::read_to_string("../../ref_ground/packs/sq1_ep.tsv").expect("tsv");
    let line = raw
        .lines()
        .find(|l| l.starts_with("ep.96\t"))
        .expect("ep.96 row");
    let cols: Vec<&str> = line.trim_end_matches('\r').split('\t').collect();
    let source_alg = cols[1];
    println!("source: {source_alg}");
    let out = normalize_alg("sq1", source_alg).expect("ep.96 normalizes");
    println!("stored: {}", out.alg);
    println!("notes: {:?}", out.notes);
    let alg = engine.parse_alg(&out.alg).expect("engine parses stored");
    let mut state = engine.solved_state();
    engine.apply_alg(&mut state, &alg).expect("apply stored");
    let mut solved_check = state;
    for mv in engine.invert_alg(&alg).iter().rev() {
        // apply inverse move by move, invert_alg output is in reverse order already
        let _ = mv;
    }
    let mut round = state;
    engine
        .apply_alg(&mut round, &engine.invert_alg(&alg))
        .expect("apply inverse");
    if !engine.is_solved(&round) {
        defects.push("ep.96 alg + engine inverse does not round trip".into());
    }
    let _ = &mut solved_check;
    match verify_tier1(&Sq1, &out.alg) {
        Ok(()) => println!("tier1 verify: PASS"),
        Err(e) => defects.push(format!("tier1 verify failed: {e}")),
    }
    let minus_six_text = out.alg.replace("(0,6)", "(0,-6)");
    if minus_six_text != out.alg {
        let mut manual = engine.solved_state();
        let alg2 = engine.parse_alg(&minus_six_text);
        match alg2 {
            Err(_) => println!("engine rejects the raw -6 spelling, as pinned"),
            Ok(a2) => {
                let _ = a2;
                defects.push("engine accepted (0,-6)".into());
            }
        }
        let parsed = engine.parse_alg(&out.alg).expect("stored parses");
        let mut via_stored = engine.solved_state();
        engine.apply_alg(&mut via_stored, &parsed).expect("apply");
        let mut via_raw = engine.solved_state();
        for mv in parsed.iter() {
            if let Move::Twist { top, bottom } = mv {
                let alt = if *top == 0 && *bottom == 6 {
                    Move::Twist { top: 0, bottom: -6 }
                } else {
                    *mv
                };
                sq1::apply(&mut via_raw, alt).expect("apply alt");
            } else {
                sq1::apply(&mut via_raw, *mv).expect("apply mv");
            }
        }
        println!(
            "ep.96 full 24-unit state, -6 path == +6 path: {}",
            via_stored == via_raw
        );
        if via_stored != via_raw {
            defects.push("ep.96 diverges between -6 and +6 on the full alg".into());
        }
    }

    println!("== 7. bare tuple branch wrap");
    for text in ["/ -6,0 /", "-6,-6", "(1,0) -6,5 (2,2)"] {
        let out = normalize_alg("sq1", text);
        println!("{text}: {out:?}");
    }

    println!();
    if defects.is_empty() {
        println!("VERDICT INPUT CHECKS: no defects found");
    } else {
        println!("DEFECTS:");
        for d in &defects {
            println!("  - {d}");
        }
    }
}
