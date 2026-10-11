use crate::inventory::{Coda, Nucleus, Tone};

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Place {
    Labial,
    Dental,
    Retroflex,
    Palatal,
    Velar,
    Glottal,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Manner {
    Stop,
    Aspirated,
    Implosive,
    Affricate,
    Nasal,
    Fricative,
    Approximant,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct OnsetFeatures {
    pub place: Place,
    pub manner: Manner,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct Features {
    pub tone: Tone,
    pub onset: Option<OnsetFeatures>,
    pub nucleus: Nucleus,
    pub coda: Coda,
}

const TONE_MARKED: [(char, char, Tone); 60] = [
    ('à', 'a', Tone::Huyen),
    ('á', 'a', Tone::Sac),
    ('ả', 'a', Tone::Hoi),
    ('ã', 'a', Tone::Nga),
    ('ạ', 'a', Tone::Nang),
    ('è', 'e', Tone::Huyen),
    ('é', 'e', Tone::Sac),
    ('ẻ', 'e', Tone::Hoi),
    ('ẽ', 'e', Tone::Nga),
    ('ẹ', 'e', Tone::Nang),
    ('ì', 'i', Tone::Huyen),
    ('í', 'i', Tone::Sac),
    ('ỉ', 'i', Tone::Hoi),
    ('ĩ', 'i', Tone::Nga),
    ('ị', 'i', Tone::Nang),
    ('ò', 'o', Tone::Huyen),
    ('ó', 'o', Tone::Sac),
    ('ỏ', 'o', Tone::Hoi),
    ('õ', 'o', Tone::Nga),
    ('ọ', 'o', Tone::Nang),
    ('ù', 'u', Tone::Huyen),
    ('ú', 'u', Tone::Sac),
    ('ủ', 'u', Tone::Hoi),
    ('ũ', 'u', Tone::Nga),
    ('ụ', 'u', Tone::Nang),
    ('ỳ', 'y', Tone::Huyen),
    ('ý', 'y', Tone::Sac),
    ('ỷ', 'y', Tone::Hoi),
    ('ỹ', 'y', Tone::Nga),
    ('ỵ', 'y', Tone::Nang),
    ('ằ', 'ă', Tone::Huyen),
    ('ắ', 'ă', Tone::Sac),
    ('ẳ', 'ă', Tone::Hoi),
    ('ẵ', 'ă', Tone::Nga),
    ('ặ', 'ă', Tone::Nang),
    ('ầ', 'â', Tone::Huyen),
    ('ấ', 'â', Tone::Sac),
    ('ẩ', 'â', Tone::Hoi),
    ('ẫ', 'â', Tone::Nga),
    ('ậ', 'â', Tone::Nang),
    ('ề', 'ê', Tone::Huyen),
    ('ế', 'ê', Tone::Sac),
    ('ể', 'ê', Tone::Hoi),
    ('ễ', 'ê', Tone::Nga),
    ('ệ', 'ê', Tone::Nang),
    ('ồ', 'ô', Tone::Huyen),
    ('ố', 'ô', Tone::Sac),
    ('ổ', 'ô', Tone::Hoi),
    ('ỗ', 'ô', Tone::Nga),
    ('ộ', 'ô', Tone::Nang),
    ('ờ', 'ơ', Tone::Huyen),
    ('ớ', 'ơ', Tone::Sac),
    ('ở', 'ơ', Tone::Hoi),
    ('ỡ', 'ơ', Tone::Nga),
    ('ợ', 'ơ', Tone::Nang),
    ('ừ', 'ư', Tone::Huyen),
    ('ứ', 'ư', Tone::Sac),
    ('ử', 'ư', Tone::Hoi),
    ('ữ', 'ư', Tone::Nga),
    ('ự', 'ư', Tone::Nang),
];

const ONSETS: &[(&str, OnsetFeatures)] = &[
    (
        "ngh",
        OnsetFeatures {
            place: Place::Velar,
            manner: Manner::Nasal,
        },
    ),
    (
        "ch",
        OnsetFeatures {
            place: Place::Palatal,
            manner: Manner::Affricate,
        },
    ),
    (
        "gh",
        OnsetFeatures {
            place: Place::Velar,
            manner: Manner::Fricative,
        },
    ),
    (
        "gi",
        OnsetFeatures {
            place: Place::Dental,
            manner: Manner::Fricative,
        },
    ),
    (
        "kh",
        OnsetFeatures {
            place: Place::Velar,
            manner: Manner::Fricative,
        },
    ),
    (
        "nh",
        OnsetFeatures {
            place: Place::Palatal,
            manner: Manner::Nasal,
        },
    ),
    (
        "ng",
        OnsetFeatures {
            place: Place::Velar,
            manner: Manner::Nasal,
        },
    ),
    (
        "ph",
        OnsetFeatures {
            place: Place::Labial,
            manner: Manner::Fricative,
        },
    ),
    (
        "th",
        OnsetFeatures {
            place: Place::Dental,
            manner: Manner::Aspirated,
        },
    ),
    (
        "tr",
        OnsetFeatures {
            place: Place::Retroflex,
            manner: Manner::Affricate,
        },
    ),
    (
        "b",
        OnsetFeatures {
            place: Place::Labial,
            manner: Manner::Implosive,
        },
    ),
    (
        "c",
        OnsetFeatures {
            place: Place::Velar,
            manner: Manner::Stop,
        },
    ),
    (
        "d",
        OnsetFeatures {
            place: Place::Dental,
            manner: Manner::Fricative,
        },
    ),
    (
        "đ",
        OnsetFeatures {
            place: Place::Dental,
            manner: Manner::Implosive,
        },
    ),
    (
        "g",
        OnsetFeatures {
            place: Place::Velar,
            manner: Manner::Fricative,
        },
    ),
    (
        "h",
        OnsetFeatures {
            place: Place::Glottal,
            manner: Manner::Fricative,
        },
    ),
    (
        "k",
        OnsetFeatures {
            place: Place::Velar,
            manner: Manner::Stop,
        },
    ),
    (
        "l",
        OnsetFeatures {
            place: Place::Dental,
            manner: Manner::Approximant,
        },
    ),
    (
        "m",
        OnsetFeatures {
            place: Place::Labial,
            manner: Manner::Nasal,
        },
    ),
    (
        "n",
        OnsetFeatures {
            place: Place::Dental,
            manner: Manner::Nasal,
        },
    ),
    (
        "r",
        OnsetFeatures {
            place: Place::Dental,
            manner: Manner::Fricative,
        },
    ),
    (
        "s",
        OnsetFeatures {
            place: Place::Retroflex,
            manner: Manner::Fricative,
        },
    ),
    (
        "t",
        OnsetFeatures {
            place: Place::Dental,
            manner: Manner::Stop,
        },
    ),
    (
        "v",
        OnsetFeatures {
            place: Place::Labial,
            manner: Manner::Fricative,
        },
    ),
    (
        "x",
        OnsetFeatures {
            place: Place::Dental,
            manner: Manner::Fricative,
        },
    ),
];

const RIMES: &[(&str, Nucleus, Coda)] = &[
    ("i", Nucleus::I, Coda::Open),
    ("y", Nucleus::I, Coda::Open),
    ("ư", Nucleus::Uw, Coda::Open),
    ("u", Nucleus::U, Coda::Open),
    ("ê", Nucleus::Eh, Coda::Open),
    ("ơ", Nucleus::Or, Coda::Open),
    ("ô", Nucleus::Oh, Coda::Open),
    ("e", Nucleus::E, Coda::Open),
    ("a", Nucleus::A, Coda::Open),
    ("o", Nucleus::O, Coda::Open),
    ("ia", Nucleus::Ia, Coda::Open),
    ("ya", Nucleus::Ia, Coda::Open),
    ("ưa", Nucleus::Wa, Coda::Open),
    ("ua", Nucleus::Ua, Coda::Open),
    ("im", Nucleus::I, Coda::M),
    ("ym", Nucleus::I, Coda::M),
    ("um", Nucleus::U, Coda::M),
    ("êm", Nucleus::Eh, Coda::M),
    ("ơm", Nucleus::Or, Coda::M),
    ("ôm", Nucleus::Oh, Coda::M),
    ("em", Nucleus::E, Coda::M),
    ("am", Nucleus::A, Coda::M),
    ("âm", Nucleus::Ah, Coda::M),
    ("ăm", Nucleus::Ab, Coda::M),
    ("om", Nucleus::O, Coda::M),
    ("iêm", Nucleus::Ia, Coda::M),
    ("ươm", Nucleus::Wa, Coda::M),
    ("uôm", Nucleus::Ua, Coda::M),
    ("in", Nucleus::I, Coda::N),
    ("un", Nucleus::U, Coda::N),
    ("ên", Nucleus::Eh, Coda::N),
    ("ơn", Nucleus::Or, Coda::N),
    ("ôn", Nucleus::Oh, Coda::N),
    ("en", Nucleus::E, Coda::N),
    ("an", Nucleus::A, Coda::N),
    ("ân", Nucleus::Ah, Coda::N),
    ("ăn", Nucleus::Ab, Coda::N),
    ("on", Nucleus::O, Coda::N),
    ("iên", Nucleus::Ia, Coda::N),
    ("ươn", Nucleus::Wa, Coda::N),
    ("uôn", Nucleus::Ua, Coda::N),
    ("inh", Nucleus::I, Coda::Ng),
    ("ynh", Nucleus::I, Coda::Ng),
    ("ung", Nucleus::U, Coda::Ng),
    ("ênh", Nucleus::Eh, Coda::Ng),
    ("ưng", Nucleus::Uw, Coda::Ng),
    ("ang", Nucleus::A, Coda::Ng),
    ("anh", Nucleus::E, Coda::Ng),
    ("âng", Nucleus::Ah, Coda::Ng),
    ("ăng", Nucleus::Ab, Coda::Ng),
    ("ong", Nucleus::O, Coda::Ng),
    ("ông", Nucleus::Oh, Coda::Ng),
    ("iêng", Nucleus::Ia, Coda::Ng),
    ("ương", Nucleus::Wa, Coda::Ng),
    ("uông", Nucleus::Ua, Coda::Ng),
    ("ip", Nucleus::I, Coda::P),
    ("yp", Nucleus::I, Coda::P),
    ("up", Nucleus::U, Coda::P),
    ("êp", Nucleus::Eh, Coda::P),
    ("ơp", Nucleus::Or, Coda::P),
    ("ôp", Nucleus::Oh, Coda::P),
    ("ep", Nucleus::E, Coda::P),
    ("ap", Nucleus::A, Coda::P),
    ("âp", Nucleus::Ah, Coda::P),
    ("ăp", Nucleus::Ab, Coda::P),
    ("op", Nucleus::O, Coda::P),
    ("iêp", Nucleus::Ia, Coda::P),
    ("ươp", Nucleus::Wa, Coda::P),
    ("it", Nucleus::I, Coda::T),
    ("ut", Nucleus::U, Coda::T),
    ("êt", Nucleus::Eh, Coda::T),
    ("ơt", Nucleus::Or, Coda::T),
    ("ôt", Nucleus::Oh, Coda::T),
    ("et", Nucleus::E, Coda::T),
    ("at", Nucleus::A, Coda::T),
    ("ât", Nucleus::Ah, Coda::T),
    ("ăt", Nucleus::Ab, Coda::T),
    ("ot", Nucleus::O, Coda::T),
    ("iêt", Nucleus::Ia, Coda::T),
    ("ươt", Nucleus::Wa, Coda::T),
    ("uôt", Nucleus::Ua, Coda::T),
    ("ich", Nucleus::I, Coda::K),
    ("ych", Nucleus::I, Coda::K),
    ("uc", Nucleus::U, Coda::K),
    ("êch", Nucleus::Eh, Coda::K),
    ("ưc", Nucleus::Uw, Coda::K),
    ("ac", Nucleus::A, Coda::K),
    ("ach", Nucleus::E, Coda::K),
    ("âc", Nucleus::Ah, Coda::K),
    ("ăc", Nucleus::Ab, Coda::K),
    ("oc", Nucleus::O, Coda::K),
    ("ôc", Nucleus::Oh, Coda::K),
    ("iêc", Nucleus::Ia, Coda::K),
    ("ươc", Nucleus::Wa, Coda::K),
    ("uôc", Nucleus::Ua, Coda::K),
];

pub fn parse(text: &str) -> Option<Features> {
    let mut tone = Tone::Ngang;
    let mut stripped = String::with_capacity(text.len());
    let mut mark_at: Option<usize> = None;
    for (i, c) in text.char_indices() {
        if let Some(&(_marked, base, mark)) =
            TONE_MARKED.iter().find(|&&(marked, _, _)| marked == c)
        {
            if tone != Tone::Ngang {
                return None;
            }
            tone = mark;
            mark_at = Some(i);
            stripped.push(base);
        } else if ('\u{0300}'..='\u{036F}').contains(&c) {
            return None;
        } else {
            stripped.push(c);
        }
    }
    let onset_match = ONSETS
        .iter()
        .filter(|(spelling, _)| stripped.starts_with(spelling))
        .max_by_key(|(spelling, _)| spelling.len());
    let onset = onset_match.map(|(_, features)| *features);
    let rime_start = onset_match.map_or(0, |(spelling, _)| spelling.len());
    let rime = &stripped[rime_start..];
    let merged_gi = rime_start == 2
        && stripped.starts_with("gi")
        && (matches!(mark_at, Some(1)) || rime.starts_with('ê'));
    let (nucleus, coda) = if merged_gi {
        match rime {
            "" => (Nucleus::I, Coda::Open),
            "a" => (Nucleus::Ia, Coda::Open),
            other => {
                let tail: String = other.chars().skip(1).collect();
                let expanded = format!("iê{tail}");
                let &(.., nucleus, coda) =
                    RIMES.iter().find(|(letters, _, _)| *letters == expanded)?;
                (nucleus, coda)
            }
        }
    } else {
        let &(.., nucleus, coda) = RIMES.iter().find(|(letters, _, _)| *letters == rime)?;
        (nucleus, coda)
    };
    Some(Features {
        tone,
        onset,
        nucleus,
        coda,
    })
}

fn tone_confusion(a: Tone, b: Tone) -> f32 {
    if a == b {
        return 1.0;
    }
    match (a, b) {
        (Tone::Hoi, Tone::Nga) | (Tone::Nga, Tone::Hoi) => 0.70,
        (Tone::Huyen, Tone::Nang) | (Tone::Nang, Tone::Huyen) => 0.60,
        (Tone::Sac, Tone::Nga) | (Tone::Nga, Tone::Sac) => 0.35,
        (Tone::Ngang, Tone::Huyen) | (Tone::Huyen, Tone::Ngang) => 0.30,
        (Tone::Hoi, Tone::Nang) | (Tone::Nang, Tone::Hoi) => 0.25,
        _ => 0.05,
    }
}

fn nucleus_confusion(a: Nucleus, b: Nucleus) -> f32 {
    if a == b {
        return 1.0;
    }
    match (a, b) {
        (Nucleus::Eh, Nucleus::E) | (Nucleus::E, Nucleus::Eh) => 0.40,
        (Nucleus::Oh, Nucleus::O) | (Nucleus::O, Nucleus::Oh) => 0.40,
        (Nucleus::Or, Nucleus::Ah) | (Nucleus::Ah, Nucleus::Or) => 0.40,
        (Nucleus::A, Nucleus::Ab) | (Nucleus::Ab, Nucleus::A) => 0.40,
        _ => 0.05,
    }
}

fn coda_place(coda: Coda) -> Option<Place> {
    match coda {
        Coda::Open => None,
        Coda::P | Coda::M => Some(Place::Labial),
        Coda::T | Coda::N => Some(Place::Dental),
        Coda::K | Coda::Ng => Some(Place::Velar),
    }
}

fn coda_confusion(a: Coda, b: Coda) -> f32 {
    if a == b {
        return 1.0;
    }
    match (a, b) {
        (Coda::P, Coda::T) | (Coda::T, Coda::P) => 0.50,
        (Coda::T, Coda::K) | (Coda::K, Coda::T) => 0.40,
        (Coda::P, Coda::K) | (Coda::K, Coda::P) => 0.20,
        (Coda::M, Coda::N) | (Coda::N, Coda::M) => 0.30,
        (Coda::N, Coda::Ng) | (Coda::Ng, Coda::N) => 0.30,
        (Coda::M, Coda::Ng) | (Coda::Ng, Coda::M) => 0.15,
        (Coda::P, Coda::M) | (Coda::M, Coda::P) => 0.20,
        (Coda::T, Coda::N) | (Coda::N, Coda::T) => 0.20,
        (Coda::K, Coda::Ng) | (Coda::Ng, Coda::K) => 0.20,
        _ => 0.10,
    }
}

fn onset_confusion(a: Option<OnsetFeatures>, b: Option<OnsetFeatures>) -> f32 {
    match (a, b) {
        (None, None) => 1.0,
        (Some(x), Some(y)) => {
            if x == y {
                return 1.0;
            }
            let same_place = x.place == y.place;
            let same_manner = x.manner == y.manner;
            if same_place && same_manner {
                0.60
            } else if same_place || same_manner {
                0.20
            } else {
                0.10
            }
        }
        (None, Some(_)) | (Some(_), None) => 0.10,
    }
}

pub fn confusion(a: &Features, b: &Features) -> f32 {
    tone_confusion(a.tone, b.tone)
        * nucleus_confusion(a.nucleus, b.nucleus)
        * coda_confusion(a.coda, b.coda)
        * onset_confusion(a.onset, b.onset)
}

pub fn flow_penalty(a: &Features, a_len: usize, b: &Features, b_len: usize) -> f32 {
    let mut penalty = 0.0f32;
    if a.tone == b.tone {
        penalty += 1.0;
    }
    if let (Some(coda_place), Some(onset)) = (coda_place(a.coda), b.onset)
        && coda_place == onset.place
    {
        penalty += 1.0;
    }
    penalty += 0.25 * a_len.abs_diff(b_len) as f32;
    penalty
}

#[cfg(test)]
mod tests {
    use super::*;

    fn f(text: &str) -> Features {
        parse(text).unwrap_or_else(|| panic!("parse failed for {text}"))
    }

    #[test]
    fn parse_segments_representative_syllables() {
        let không = f("không");
        assert_eq!(không.tone, Tone::Ngang);
        assert_eq!(
            không.onset,
            Some(OnsetFeatures {
                place: Place::Velar,
                manner: Manner::Fricative
            })
        );
        assert_eq!(không.nucleus, Nucleus::Oh);
        assert_eq!(không.coda, Coda::Ng);

        let của = f("của");
        assert_eq!(của.tone, Tone::Hoi);
        assert_eq!(
            của.onset,
            Some(OnsetFeatures {
                place: Place::Velar,
                manner: Manner::Stop
            })
        );
        assert_eq!(của.nucleus, Nucleus::Ua);
        assert_eq!(của.coda, Coda::Open);

        let tía = f("tía");
        assert_eq!(tía.tone, Tone::Sac);
        assert_eq!(tía.nucleus, Nucleus::Ia);

        let anh = f("anh");
        assert_eq!(anh.onset, None);
        assert_eq!(anh.nucleus, Nucleus::E);
        assert_eq!(anh.coda, Coda::Ng);

        let ach = f("ach");
        assert_eq!(ach.nucleus, Nucleus::E);
        assert_eq!(ach.coda, Coda::K);

        let ac = f("ac");
        assert_eq!(ac.nucleus, Nucleus::A);
        assert_eq!(ac.coda, Coda::K);

        let iêng = f("iêng");
        assert_eq!(iêng.onset, None);
        assert_eq!(iêng.nucleus, Nucleus::Ia);
        assert_eq!(iêng.coda, Coda::Ng);

        let gì = f("gì");
        assert_eq!(
            gì.onset,
            Some(OnsetFeatures {
                place: Place::Dental,
                manner: Manner::Fricative
            })
        );
        assert_eq!(gì.nucleus, Nucleus::I);
        let gịa = f("gịa");
        assert_eq!(gịa.nucleus, Nucleus::Ia);
        let già = f("già");
        assert_eq!(già.nucleus, Nucleus::A);
        let giếng = f("giếng");
        assert_eq!(giếng.nucleus, Nucleus::Ia);
        assert_eq!(giếng.coda, Coda::Ng);
        assert_eq!(giếng.tone, Tone::Sac);

        let đặt = f("đặt");
        assert_eq!(
            đặt.onset,
            Some(OnsetFeatures {
                place: Place::Dental,
                manner: Manner::Implosive
            })
        );
        assert_eq!(đặt.nucleus, Nucleus::Ab);
        assert_eq!(đặt.coda, Coda::T);

        let trăng = f("trăng");
        assert_eq!(
            trăng.onset,
            Some(OnsetFeatures {
                place: Place::Retroflex,
                manner: Manner::Affricate
            })
        );
        assert_eq!(trăng.nucleus, Nucleus::Ab);
        assert_eq!(trăng.coda, Coda::Ng);

        let y = f("y");
        assert_eq!(y.onset, None);
        assert_eq!(y.nucleus, Nucleus::I);
        assert_eq!(y.coda, Coda::Open);
    }

    #[test]
    fn parse_rejects_garbage_and_double_tones() {
        assert!(parse("xyz").is_none());
        assert!(parse("").is_none());
        assert!(parse("khôngg").is_none());
        assert!(parse("qua").is_none());
    }

    #[test]
    fn identical_and_homophonous_pairs_score_maximally() {
        let a = f("không");
        assert_eq!(confusion(&a, &a), 1.0);
        let i = f("i");
        let y = f("y");
        assert_eq!(confusion(&i, &y), 1.0);
    }

    #[test]
    fn documented_tone_confusions_rank_above_undocumented() {
        let hỏi = f("ả");
        let ngã = f("ã");
        let huyền = f("à");
        let nặng = f("ạ");
        let sắc = f("á");
        let hỏi_ngã = confusion(&hỏi, &ngã);
        let huyền_nặng = confusion(&huyền, &nặng);
        let hỏi_sắc = confusion(&hỏi, &sắc);
        assert!(hỏi_ngã > 0.6);
        assert!(huyền_nặng > 0.5);
        assert!(hỏi_ngã > hỏi_sắc);
        assert!(huyền_nặng > hỏi_sắc);
        assert!(hỏi_sắc < 0.1);
    }

    #[test]
    fn stop_coda_place_confusions_follow_kirby_ranking() {
        let p = f("áp");
        let t = f("át");
        let k = f("ác");
        let m = f("ám");
        let pt = confusion(&p, &t);
        let tk = confusion(&t, &k);
        let pk = confusion(&p, &k);
        assert!(pt > tk && tk > pk);
        let pm = confusion(&p, &m);
        assert!(pm < tk);
        assert!(confusion(&p, &k) < confusion(&k, &k));
    }

    #[test]
    fn short_long_nucleus_pairs_partial_confusion() {
        let e = f("en");
        let ê = f("ên");
        let o = f("on");
        let ô = f("ôn");
        assert!(confusion(&e, &ê) > 0.3);
        assert!(confusion(&o, &ô) > 0.3);
        let a = f("an");
        let u = f("un");
        assert!(confusion(&e, &ê) > confusion(&a, &u));
    }

    #[test]
    fn confusion_is_symmetric() {
        let words = ["không", "của", "tía", "anh", "mút", "đặt", "y", "ác", "áp"];
        let feats: Vec<Features> = words.iter().map(|w| f(w)).collect();
        for a in &feats {
            for b in &feats {
                let forward = confusion(a, b);
                let backward = confusion(b, a);
                assert!((forward - backward).abs() < f32::EPSILON);
            }
        }
    }

    #[test]
    fn flow_penalty_counts_tone_clash_coda_clash_and_length() {
        let a = f("không");
        let b = f("lông");
        assert_eq!(flow_penalty(&a, 4, &b, 4), 1.0);
        let c = f("không");
        let d = f("không");
        assert_eq!(flow_penalty(&c, 4, &d, 4), 2.0);
        let e = f("ma");
        let g = f("má");
        assert_eq!(flow_penalty(&e, 2, &g, 2), 0.0);
        let h = f("cam");
        let i = f("ma");
        assert!(flow_penalty(&h, 3, &i, 2) >= 0.25);
        let j = f("ác");
        let k = f("can");
        assert!(flow_penalty(&j, 2, &k, 3) >= 1.25);
    }
}
