use bt_ga::features::{Features, Manner, OnsetFeatures, Place, confusion, flow_penalty, parse};
use bt_ga::inventory::{Coda, Nucleus, Tone};

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
