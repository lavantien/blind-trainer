use bt_ga::features::{
    Features, Manner, OnsetFeatures, Place, confusion, flow_penalty, onset_features, parse,
};
use bt_ga::ga::{GaParams, draw_cuts, tournament};
use bt_ga::inventory::{Coda, Nucleus, Onset, Tone, build_inventory, load_wordlist};
use bt_scheme::Rng;
use std::collections::HashSet;

fn feat(tone: Tone, onset: Option<OnsetFeatures>, nucleus: Nucleus, coda: Coda) -> Features {
    Features {
        tone,
        onset,
        nucleus,
        coda,
    }
}

fn tones_only(a: Tone, b: Tone) -> (Features, Features) {
    (
        feat(a, None, Nucleus::A, Coda::Open),
        feat(b, None, Nucleus::A, Coda::Open),
    )
}

fn nuclei_only(a: Nucleus, b: Nucleus) -> (Features, Features) {
    (
        feat(Tone::Ngang, None, a, Coda::Open),
        feat(Tone::Ngang, None, b, Coda::Open),
    )
}

fn codas_only(a: Coda, b: Coda) -> (Features, Features) {
    (
        feat(Tone::Ngang, None, Nucleus::A, a),
        feat(Tone::Ngang, None, Nucleus::A, b),
    )
}

fn onsets_only(a: Option<OnsetFeatures>, b: Option<OnsetFeatures>) -> (Features, Features) {
    (
        feat(Tone::Ngang, a, Nucleus::A, Coda::Open),
        feat(Tone::Ngang, b, Nucleus::A, Coda::Open),
    )
}

#[test]
fn documented_tone_confusions_are_pinned() {
    for (a, b, expected) in [
        (Tone::Hoi, Tone::Nga, 0.70f32),
        (Tone::Huyen, Tone::Nang, 0.60),
        (Tone::Sac, Tone::Nga, 0.35),
        (Tone::Ngang, Tone::Huyen, 0.30),
        (Tone::Hoi, Tone::Nang, 0.25),
        (Tone::Ngang, Tone::Sac, 0.05),
        (Tone::Ngang, Tone::Nga, 0.05),
    ] {
        let (x, y) = tones_only(a, b);
        assert_eq!(confusion(&x, &y), expected, "{a:?} {b:?}");
        assert_eq!(confusion(&y, &x), expected, "{a:?} {b:?}");
    }
    let (same, same2) = tones_only(Tone::Sac, Tone::Sac);
    assert_eq!(confusion(&same, &same2), 1.0);
}

#[test]
fn documented_nucleus_confusions_are_pinned() {
    for (a, b, expected) in [
        (Nucleus::E, Nucleus::Eh, 0.40f32),
        (Nucleus::O, Nucleus::Oh, 0.40),
        (Nucleus::Or, Nucleus::Ah, 0.40),
        (Nucleus::A, Nucleus::Ab, 0.40),
        (Nucleus::A, Nucleus::E, 0.05),
        (Nucleus::Or, Nucleus::O, 0.05),
    ] {
        let (x, y) = nuclei_only(a, b);
        assert_eq!(confusion(&x, &y), expected, "{a:?} {b:?}");
        assert_eq!(confusion(&y, &x), expected, "{a:?} {b:?}");
    }
}

#[test]
fn documented_coda_confusions_are_pinned() {
    for (a, b, expected) in [
        (Coda::P, Coda::T, 0.50f32),
        (Coda::T, Coda::K, 0.40),
        (Coda::P, Coda::K, 0.20),
        (Coda::M, Coda::N, 0.30),
        (Coda::N, Coda::Ng, 0.30),
        (Coda::M, Coda::Ng, 0.15),
        (Coda::P, Coda::M, 0.20),
        (Coda::T, Coda::N, 0.20),
        (Coda::K, Coda::Ng, 0.20),
        (Coda::P, Coda::N, 0.10),
        (Coda::Open, Coda::T, 0.10),
    ] {
        let (x, y) = codas_only(a, b);
        assert_eq!(confusion(&x, &y), expected, "{a:?} {b:?}");
        assert_eq!(confusion(&y, &x), expected, "{a:?} {b:?}");
    }
}

#[test]
fn documented_onset_confusions_are_pinned() {
    let labial_stop = Some(OnsetFeatures {
        place: Place::Labial,
        manner: Manner::Stop,
    });
    let labial_nasal = Some(OnsetFeatures {
        place: Place::Labial,
        manner: Manner::Nasal,
    });
    let dental_stop = Some(OnsetFeatures {
        place: Place::Dental,
        manner: Manner::Stop,
    });
    let dental_nasal = Some(OnsetFeatures {
        place: Place::Dental,
        manner: Manner::Nasal,
    });

    let (x, y) = onsets_only(labial_stop, labial_nasal);
    assert_eq!(confusion(&x, &y), 0.20, "same place only");
    let (x, y) = onsets_only(labial_stop, dental_stop);
    assert_eq!(confusion(&x, &y), 0.20, "same manner only");
    let (x, y) = onsets_only(labial_stop, dental_nasal);
    assert_eq!(confusion(&x, &y), 0.10, "neither shared");
    let (x, y) = onsets_only(labial_stop, labial_stop);
    assert_eq!(confusion(&x, &y), 1.0, "identical onsets");
    let (x, y) = onsets_only(None, None);
    assert_eq!(confusion(&x, &y), 1.0, "both zero onset");
    let (x, y) = onsets_only(None, labial_stop);
    assert_eq!(confusion(&x, &y), 0.10, "zero versus consonant");
}

#[test]
fn confusion_multiplies_all_feature_factors() {
    let x = feat(Tone::Hoi, None, Nucleus::E, Coda::P);
    let y = feat(Tone::Nga, None, Nucleus::Eh, Coda::T);
    assert_eq!(confusion(&x, &y), 0.70 * 0.40 * 0.50);

    let x = feat(Tone::Ngang, None, Nucleus::A, Coda::M);
    let y = feat(Tone::Huyen, None, Nucleus::Ab, Coda::Ng);
    assert_eq!(confusion(&x, &y), 0.30 * 0.40 * 0.15);
}

#[test]
fn flow_penalty_terms_are_pinned() {
    let onset = Some(OnsetFeatures {
        place: Place::Labial,
        manner: Manner::Stop,
    });
    let other_labial = Some(OnsetFeatures {
        place: Place::Labial,
        manner: Manner::Nasal,
    });
    let dental = Some(OnsetFeatures {
        place: Place::Dental,
        manner: Manner::Stop,
    });
    let x = feat(Tone::Sac, onset, Nucleus::A, Coda::P);
    let y = feat(Tone::Sac, other_labial, Nucleus::A, Coda::T);
    assert_eq!(
        flow_penalty(&x, 3, &y, 3),
        2.0,
        "same tone plus labial clash"
    );

    let x = feat(Tone::Sac, dental, Nucleus::A, Coda::T);
    let y = feat(Tone::Huyen, onset, Nucleus::A, Coda::P);
    assert_eq!(
        flow_penalty(&x, 4, &y, 3),
        0.25,
        "no tone or place term, one length step"
    );
    assert_eq!(flow_penalty(&x, 7, &y, 3), 1.0, "four length steps");
}

#[test]
fn parse_supplies_onset_features() {
    let ta = parse("ta").expect("ta parses");
    let onset = ta.onset.expect("ta has an onset");
    assert_eq!(onset.place, Place::Dental);
    assert_eq!(onset.manner, Manner::Stop);

    let ma = parse("ma").expect("ma parses");
    let onset = ma.onset.expect("ma has an onset");
    assert_eq!(onset.place, Place::Labial);
    assert_eq!(onset.manner, Manner::Nasal);

    assert_eq!(parse("ta"), parse("ta"));
    assert!(parse("ta").is_some());
}

#[test]
fn build_env_rejects_mismatched_tables() {
    let f0 = feat(Tone::Hoi, None, Nucleus::A, Coda::Open);
    let f1 = feat(Tone::Nga, None, Nucleus::A, Coda::Open);
    assert!(bt_ga::ga::build_env(2, vec![3.0, 4.0], vec![f0, f1], 2, vec![], vec![]).is_ok());
    let err = bt_ga::ga::build_env(2, vec![3.0, 4.0], vec![f0], 2, vec![], vec![]);
    assert!(err.is_err(), "feature table mismatch must fail");
    let err = bt_ga::ga::build_env(2, vec![3.0], vec![f0, f1], 2, vec![], vec![]);
    assert!(err.is_err(), "length table mismatch must fail");
    let err = bt_ga::ga::build_env(1, vec![3.0], vec![f0], 2, vec![], vec![]);
    assert!(err.is_err(), "pool below case count must fail");
}

#[test]
fn fitness_parts_weights_are_pinned() {
    let f0 = feat(Tone::Hoi, None, Nucleus::A, Coda::Open);
    let f1 = feat(Tone::Nga, None, Nucleus::A, Coda::Open);
    let env = bt_ga::ga::build_env(
        2,
        vec![3.0, 4.0],
        vec![f0, f1],
        2,
        vec![(0, 1, 1.0f32)],
        vec![(1, 0, 2.0f32)],
    )
    .unwrap();
    assert_eq!(env.conf_table[1], 0.70);
    assert_eq!(env.conf_table[2], 0.70);
    assert_eq!(env.conf_table[0], 1.0);
    assert_eq!(env.flow_table[2], 0.25);
    let parts = bt_ga::ga::fitness_parts(&env, &[0, 1]);
    assert_eq!(parts, [0.70, 0.50, 3.5]);
}

#[test]
fn onset_features_maps_every_consonant() {
    for (onset, place, manner) in [
        (Onset::B, Place::Labial, Manner::Implosive),
        (Onset::Dd, Place::Dental, Manner::Implosive),
        (Onset::T, Place::Dental, Manner::Stop),
        (Onset::Th, Place::Dental, Manner::Aspirated),
        (Onset::Ch, Place::Palatal, Manner::Affricate),
        (Onset::Tr, Place::Retroflex, Manner::Affricate),
        (Onset::K, Place::Velar, Manner::Stop),
        (Onset::M, Place::Labial, Manner::Nasal),
        (Onset::N, Place::Dental, Manner::Nasal),
        (Onset::Nh, Place::Palatal, Manner::Nasal),
        (Onset::Ng, Place::Velar, Manner::Nasal),
        (Onset::Ph, Place::Labial, Manner::Fricative),
        (Onset::X, Place::Dental, Manner::Fricative),
        (Onset::S, Place::Retroflex, Manner::Fricative),
        (Onset::Kh, Place::Velar, Manner::Fricative),
        (Onset::H, Place::Glottal, Manner::Fricative),
        (Onset::V, Place::Labial, Manner::Fricative),
        (Onset::Z, Place::Dental, Manner::Fricative),
        (Onset::G, Place::Velar, Manner::Fricative),
        (Onset::L, Place::Dental, Manner::Approximant),
    ] {
        assert_eq!(
            onset_features(onset),
            Some(OnsetFeatures { place, manner }),
            "{onset:?}"
        );
    }
    assert_eq!(onset_features(Onset::Zero), None);
}

#[test]
fn draw_cuts_stay_bounded_and_spread() {
    let mut seen: HashSet<(usize, usize)> = HashSet::new();
    for seed in 0..400u64 {
        let mut rng = Rng::new(seed);
        for n in 2usize..=12 {
            let (cut1, cut2) = draw_cuts(&mut rng, n);
            assert!(cut1 < cut2, "seed {seed} n {n} cuts ({cut1},{cut2})");
            assert!(cut2 <= n - 1, "seed {seed} n {n} cuts ({cut1},{cut2})");
            assert!(cut1 <= n - 2, "seed {seed} n {n} cuts ({cut1},{cut2})");
            if n == 8 {
                seen.insert((cut1, cut2));
            }
        }
    }
    assert!(seen.len() >= 10, "{} distinct cut pairs", seen.len());
    assert!(seen.iter().any(|&(cut1, _)| cut1 == 0));
    assert!(seen.iter().any(|&(_, cut2)| cut2 == 7));
}

#[test]
fn tournament_breaks_ties_toward_the_first_drawn_index() {
    let flat = vec![0.5f32; 8];
    for seed in 0..64u64 {
        let mut rng = Rng::new(seed);
        let a = rng.below(8) as usize;
        let mut replay = Rng::new(seed);
        replay.next_u64();
        let b = replay.below(8) as usize;
        let mut draw = Rng::new(seed);
        let winner = tournament(&mut draw, &flat);
        assert_eq!(winner, a, "seed {seed} draws ({a},{b})");
    }
}

#[test]
fn flat_landscape_reports_full_stagnation() {
    let feats: Vec<Features> = [
        (Tone::Hoi, Nucleus::A, Coda::Open),
        (Tone::Nga, Nucleus::E, Coda::P),
        (Tone::Sac, Nucleus::O, Coda::T),
        (Tone::Huyen, Nucleus::Eh, Coda::M),
    ]
    .iter()
    .map(|&(tone, nucleus, coda)| feat(tone, None, nucleus, coda))
    .collect();
    let env = bt_ga::ga::build_env(4, vec![2.5f32; 4], feats, 2, vec![], vec![]).unwrap();
    let params = GaParams {
        seed: 9,
        population: 12,
        generations: 25,
        elite: 2,
        swap: 0.2,
        replace: 0.1,
        w_conf: 1.0,
        w_flow: 0.5,
        w_cplx: 0.1,
    };
    let outcome = bt_ga::ga::run(&env, &params);
    assert!(
        outcome
            .per_gen_best
            .windows(2)
            .all(|pair| pair[0] == pair[1])
    );
    assert_eq!(outcome.stagnation_tail, 25);
    assert_eq!(outcome.max_stagnation, 25);
    assert_eq!(outcome.best_generation, 0);
    assert_eq!(
        outcome.evaluations,
        12 + 25 * (12 - 2),
        "flat landscape evaluation count"
    );
}

#[test]
fn run_trajectory_is_pinned_for_reproducible_inputs() {
    let feats: Vec<Features> = [
        (Tone::Hoi, Nucleus::A, Coda::Open),
        (Tone::Nga, Nucleus::A, Coda::Open),
        (Tone::Sac, Nucleus::E, Coda::P),
        (Tone::Huyen, Nucleus::O, Coda::T),
        (Tone::Ngang, Nucleus::Eh, Coda::M),
        (Tone::Nang, Nucleus::Oh, Coda::N),
    ]
    .iter()
    .map(|&(tone, nucleus, coda)| feat(tone, None, nucleus, coda))
    .collect();
    let env = bt_ga::ga::build_env(
        6,
        vec![3.0, 4.0, 5.0, 3.0, 4.0, 5.0],
        feats,
        3,
        vec![(0, 1, 0.5f32), (1, 2, 0.25)],
        vec![(0, 2, 0.5f32), (1, 2, 1.0)],
    )
    .unwrap();
    let params = GaParams {
        seed: 777,
        population: 12,
        generations: 25,
        elite: 2,
        swap: 0.2,
        replace: 0.1,
        w_conf: 1.0,
        w_flow: 0.5,
        w_cplx: 0.1,
    };
    let outcome = bt_ga::ga::run(&env, &params);
    assert_eq!(outcome.best, vec![4, 0, 3]);
    let mut expected = vec![0.396_645_84_f32; 4];
    expected.extend(std::iter::repeat(0.396_020_83_f32).take(22));
    assert_eq!(outcome.per_gen_best, expected);
    assert_eq!(outcome.best_generation, 4);
    assert_eq!(outcome.stagnation_tail, 21);
    assert_eq!(outcome.max_stagnation, 21);
    assert_eq!(outcome.evaluations, 262);
}

fn inventory_texts(wordlist: &[&str]) -> Vec<String> {
    let wordlist: HashSet<String> = wordlist.iter().map(|text| text.to_string()).collect();
    let (entries, _) = build_inventory(&wordlist);
    let mut texts: Vec<String> = entries.iter().map(|entry| entry.text.clone()).collect();
    texts.sort();
    texts
}

#[test]
fn stop_coda_syllables_require_sac_or_nang_tone() {
    assert_eq!(
        inventory_texts(&["át", "at", "àn"]),
        vec!["àn".to_string(), "át".to_string()]
    );
}

#[test]
fn front_nuclei_force_k_and_ngh_spellings() {
    assert_eq!(
        inventory_texts(&["ké", "ce", "nghé", "ngé", "ca"]),
        vec!["ca".to_string(), "ké".to_string(), "nghé".to_string()]
    );
}

#[test]
fn nucleus_e_keeps_its_rime_outside_velar_codas() {
    assert_eq!(
        inventory_texts(&["en", "an", "ang"]),
        vec!["an".to_string(), "ang".to_string(), "en".to_string()]
    );
}

#[test]
fn uoa_nucleus_rejects_only_the_p_coda() {
    assert_eq!(
        inventory_texts(&["uốt", "uôp", "áp"]),
        vec!["uốt".to_string(), "áp".to_string()]
    );
}

#[test]
fn load_wordlist_skips_comments_and_rejects_non_nfc_lines() {
    let dir = std::env::temp_dir().join("bt-ga-fuzzkill-wordlist");
    std::fs::create_dir_all(&dir).unwrap();
    let path = dir.join("words.txt");
    std::fs::write(&path, "bà\n# note\n\nla\n#x\n").unwrap();
    let set = load_wordlist(&path).unwrap();
    let mut texts: Vec<String> = set.into_iter().collect();
    texts.sort();
    assert_eq!(texts, vec!["bà".to_string(), "la".to_string()]);
    let marked = dir.join("marked.txt");
    std::fs::write(&marked, "la\nla\u{0301}\n").unwrap();
    let err = load_wordlist(&marked).unwrap_err();
    assert!(err.contains("combining mark"), "{err}");
}

fn boundary_env() -> bt_ga::ga::GaEnv {
    let feats: Vec<Features> = [
        (Tone::Hoi, Nucleus::A, Coda::Open),
        (Tone::Nga, Nucleus::A, Coda::Open),
        (Tone::Sac, Nucleus::E, Coda::P),
        (Tone::Huyen, Nucleus::O, Coda::T),
        (Tone::Ngang, Nucleus::Eh, Coda::M),
        (Tone::Nang, Nucleus::Oh, Coda::N),
    ]
    .iter()
    .map(|&(tone, nucleus, coda)| feat(tone, None, nucleus, coda))
    .collect();
    bt_ga::ga::build_env(
        6,
        vec![3.0, 4.0, 5.0, 3.0, 4.0, 5.0],
        feats,
        4,
        vec![(0, 1, 0.5f32), (1, 2, 0.25), (2, 3, 0.1)],
        vec![(0, 2, 0.5f32), (1, 3, 1.0), (3, 2, 0.75)],
    )
    .unwrap()
}

fn boundary_params(seed: u64, swap: f64, replace: f64) -> GaParams {
    GaParams {
        seed,
        population: 2,
        generations: 1,
        elite: 1,
        swap,
        replace,
        w_conf: 1.0,
        w_flow: 0.5,
        w_cplx: 0.1,
    }
}

#[test]
fn swap_gate_is_strict_at_the_rate_boundary() {
    let env = boundary_env();
    let at_draw = bt_ga::ga::run(&env, &boundary_params(1, 645_334.0 / 1_000_000.0, 1.0));
    let below_draw = bt_ga::ga::run(&env, &boundary_params(1, 645_333.0 / 1_000_000.0, 1.0));
    assert_eq!(at_draw.per_gen_best, below_draw.per_gen_best);
    assert_eq!(at_draw.best, below_draw.best);
    let fired = bt_ga::ga::run(&env, &boundary_params(1, 645_335.0 / 1_000_000.0, 1.0));
    assert_ne!(fired.per_gen_best, at_draw.per_gen_best);
}

#[test]
fn replace_gate_is_strict_at_the_rate_boundary() {
    let env = boundary_env();
    let at_draw = bt_ga::ga::run(&env, &boundary_params(1, 0.0, 815_350.0 / 1_000_000.0));
    let below_draw = bt_ga::ga::run(&env, &boundary_params(1, 0.0, 815_349.0 / 1_000_000.0));
    assert_eq!(at_draw.per_gen_best, below_draw.per_gen_best);
    assert_eq!(at_draw.best, below_draw.best);
    let fired = bt_ga::ga::run(&env, &boundary_params(1, 0.0, 815_351.0 / 1_000_000.0));
    assert_ne!(fired.per_gen_best, at_draw.per_gen_best);
}
