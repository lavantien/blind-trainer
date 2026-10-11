use std::collections::{HashMap, HashSet};
use std::path::Path;

pub const VIETNAMESE_DIR: &str = "vietnamese";
pub const WORDLIST_FILE: &str = "syllables-7184-abc.txt";
pub const FREQS_FILE: &str = "vn-word-frequencies.tsv";
pub const CASE_COUNT: usize = 818;

#[derive(Clone, Copy, PartialEq, Eq, Debug, Hash)]
pub enum Tone {
    Ngang,
    Huyen,
    Sac,
    Hoi,
    Nga,
    Nang,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug, Hash)]
pub enum Coda {
    Open,
    P,
    T,
    K,
    M,
    N,
    Ng,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug, Hash)]
pub enum Nucleus {
    I,
    Uw,
    U,
    Eh,
    Or,
    Oh,
    E,
    Ah,
    A,
    Ab,
    O,
    Ia,
    Wa,
    Ua,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug, Hash)]
pub enum Onset {
    B,
    Dd,
    T,
    Th,
    Ch,
    Tr,
    K,
    M,
    N,
    Nh,
    Ng,
    Ph,
    X,
    S,
    Kh,
    H,
    V,
    Z,
    G,
    L,
    Zero,
}

const ONSETS: [Onset; 20] = [
    Onset::B,
    Onset::Dd,
    Onset::T,
    Onset::Th,
    Onset::Ch,
    Onset::Tr,
    Onset::K,
    Onset::M,
    Onset::N,
    Onset::Nh,
    Onset::Ng,
    Onset::Ph,
    Onset::X,
    Onset::S,
    Onset::Kh,
    Onset::H,
    Onset::V,
    Onset::Z,
    Onset::G,
    Onset::L,
];

const NUCLEI: [Nucleus; 14] = [
    Nucleus::I,
    Nucleus::Uw,
    Nucleus::U,
    Nucleus::Eh,
    Nucleus::Or,
    Nucleus::Oh,
    Nucleus::E,
    Nucleus::Ah,
    Nucleus::A,
    Nucleus::Ab,
    Nucleus::O,
    Nucleus::Ia,
    Nucleus::Wa,
    Nucleus::Ua,
];

const CODAS: [Coda; 7] = [
    Coda::Open,
    Coda::P,
    Coda::T,
    Coda::K,
    Coda::M,
    Coda::N,
    Coda::Ng,
];

const TONES: [Tone; 6] = [
    Tone::Ngang,
    Tone::Huyen,
    Tone::Sac,
    Tone::Hoi,
    Tone::Nga,
    Tone::Nang,
];

impl Nucleus {
    fn is_front(self) -> bool {
        matches!(self, Nucleus::I | Nucleus::E | Nucleus::Eh | Nucleus::Ia)
    }

    fn takes_velar_coda(self) -> bool {
        matches!(self, Nucleus::I | Nucleus::E | Nucleus::Eh)
    }

    fn takes_k_n_coda(self) -> bool {
        !matches!(self, Nucleus::Or)
    }

    fn takes_m_n_coda(self) -> bool {
        !matches!(self, Nucleus::Uw)
    }
}

impl Coda {
    fn is_stop(self) -> bool {
        matches!(self, Coda::P | Coda::T | Coda::K)
    }

    fn is_open(self) -> bool {
        matches!(self, Coda::Open)
    }

    fn letters(self, nucleus: Nucleus) -> &'static str {
        match self {
            Coda::Open => "",
            Coda::P => "p",
            Coda::T => "t",
            Coda::M => "m",
            Coda::N => "n",
            Coda::K => {
                if nucleus.takes_velar_coda() {
                    "ch"
                } else {
                    "c"
                }
            }
            Coda::Ng => {
                if nucleus.takes_velar_coda() {
                    "nh"
                } else {
                    "ng"
                }
            }
        }
    }
}

fn nucleus_rime(nucleus: Nucleus, coda: Coda) -> Option<(&'static str, usize)> {
    if coda.is_open() {
        let rime = match nucleus {
            Nucleus::I => "i",
            Nucleus::Uw => "ư",
            Nucleus::U => "u",
            Nucleus::Eh => "ê",
            Nucleus::Or => "ơ",
            Nucleus::Oh => "ô",
            Nucleus::E => "e",
            Nucleus::A => "a",
            Nucleus::O => "o",
            Nucleus::Ia => "ia",
            Nucleus::Wa => "ưa",
            Nucleus::Ua => "ua",
            Nucleus::Ah | Nucleus::Ab => return None,
        };
        return Some((rime, 0));
    }
    if !nucleus.takes_m_n_coda() && matches!(coda, Coda::M | Coda::N) {
        return None;
    }
    if !nucleus.takes_k_n_coda() && matches!(coda, Coda::Ng | Coda::K) {
        return None;
    }
    if matches!(nucleus, Nucleus::Ua) && matches!(coda, Coda::P) {
        return None;
    }
    let rime = match nucleus {
        Nucleus::I => "i",
        Nucleus::Uw => "ư",
        Nucleus::U => "u",
        Nucleus::Eh => "ê",
        Nucleus::Or => "ơ",
        Nucleus::Oh => "ô",
        Nucleus::E if matches!(coda, Coda::Ng | Coda::K) => "a",
        Nucleus::E => "e",
        Nucleus::Ah => "â",
        Nucleus::A => "a",
        Nucleus::Ab => "ă",
        Nucleus::O => "o",
        Nucleus::Ia => "iê",
        Nucleus::Wa => "ươ",
        Nucleus::Ua => "uô",
    };
    let mark_at = match nucleus {
        Nucleus::Ia | Nucleus::Wa | Nucleus::Ua => 1,
        _ => 0,
    };
    Some((rime, mark_at))
}

fn tone_char(base: char, tone: Tone) -> char {
    match (base, tone) {
        ('a', Tone::Huyen) => 'à',
        ('a', Tone::Sac) => 'á',
        ('a', Tone::Hoi) => 'ả',
        ('a', Tone::Nga) => 'ã',
        ('a', Tone::Nang) => 'ạ',
        ('e', Tone::Huyen) => 'è',
        ('e', Tone::Sac) => 'é',
        ('e', Tone::Hoi) => 'ẻ',
        ('e', Tone::Nga) => 'ẽ',
        ('e', Tone::Nang) => 'ẹ',
        ('i', Tone::Huyen) => 'ì',
        ('i', Tone::Sac) => 'í',
        ('i', Tone::Hoi) => 'ỉ',
        ('i', Tone::Nga) => 'ĩ',
        ('i', Tone::Nang) => 'ị',
        ('o', Tone::Huyen) => 'ò',
        ('o', Tone::Sac) => 'ó',
        ('o', Tone::Hoi) => 'ỏ',
        ('o', Tone::Nga) => 'õ',
        ('o', Tone::Nang) => 'ọ',
        ('u', Tone::Huyen) => 'ù',
        ('u', Tone::Sac) => 'ú',
        ('u', Tone::Hoi) => 'ủ',
        ('u', Tone::Nga) => 'ũ',
        ('u', Tone::Nang) => 'ụ',
        ('y', Tone::Huyen) => 'ỳ',
        ('y', Tone::Sac) => 'ý',
        ('y', Tone::Hoi) => 'ỷ',
        ('y', Tone::Nga) => 'ỹ',
        ('y', Tone::Nang) => 'ỵ',
        ('ă', Tone::Huyen) => 'ằ',
        ('ă', Tone::Sac) => 'ắ',
        ('ă', Tone::Hoi) => 'ẳ',
        ('ă', Tone::Nga) => 'ẵ',
        ('ă', Tone::Nang) => 'ặ',
        ('â', Tone::Huyen) => 'ầ',
        ('â', Tone::Sac) => 'ấ',
        ('â', Tone::Hoi) => 'ẩ',
        ('â', Tone::Nga) => 'ẫ',
        ('â', Tone::Nang) => 'ậ',
        ('ê', Tone::Huyen) => 'ề',
        ('ê', Tone::Sac) => 'ế',
        ('ê', Tone::Hoi) => 'ể',
        ('ê', Tone::Nga) => 'ễ',
        ('ê', Tone::Nang) => 'ệ',
        ('ô', Tone::Huyen) => 'ồ',
        ('ô', Tone::Sac) => 'ố',
        ('ô', Tone::Hoi) => 'ổ',
        ('ô', Tone::Nga) => 'ỗ',
        ('ô', Tone::Nang) => 'ộ',
        ('ơ', Tone::Huyen) => 'ờ',
        ('ơ', Tone::Sac) => 'ớ',
        ('ơ', Tone::Hoi) => 'ở',
        ('ơ', Tone::Nga) => 'ỡ',
        ('ơ', Tone::Nang) => 'ợ',
        ('ư', Tone::Huyen) => 'ừ',
        ('ư', Tone::Sac) => 'ứ',
        ('ư', Tone::Hoi) => 'ử',
        ('ư', Tone::Nga) => 'ữ',
        ('ư', Tone::Nang) => 'ự',
        (base, _) => base,
    }
}

fn onset_spellings(onset: Onset, nucleus: Nucleus) -> Vec<&'static str> {
    let front = nucleus.is_front();
    match onset {
        Onset::B => vec!["b"],
        Onset::Dd => vec!["đ"],
        Onset::T => vec!["t"],
        Onset::Th => vec!["th"],
        Onset::Ch => vec!["ch"],
        Onset::Tr => vec!["tr"],
        Onset::K => {
            if front {
                vec!["k"]
            } else {
                vec!["c", "k"]
            }
        }
        Onset::M => vec!["m"],
        Onset::N => vec!["n"],
        Onset::Nh => vec!["nh"],
        Onset::Ng => {
            if front {
                vec!["ngh"]
            } else {
                vec!["ng"]
            }
        }
        Onset::Ph => vec!["ph"],
        Onset::X => vec!["x"],
        Onset::S => vec!["s"],
        Onset::Kh => vec!["kh"],
        Onset::H => vec!["h"],
        Onset::V => vec!["v"],
        Onset::Z => {
            let mut forms = vec!["d", "r"];
            if matches!(
                nucleus,
                Nucleus::A | Nucleus::Ab | Nucleus::Ah | Nucleus::I | Nucleus::Ia | Nucleus::Wa
            ) {
                forms.push("gi");
            }
            forms
        }
        Onset::G => {
            if front {
                vec!["gh"]
            } else {
                vec!["g"]
            }
        }
        Onset::L => vec!["l"],
        Onset::Zero => vec![""],
    }
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct InventoryEntry {
    pub text: String,
    pub onset: Onset,
    pub nucleus: Nucleus,
    pub coda: Coda,
    pub tone: Tone,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct InventoryReport {
    pub generated: usize,
    pub kept: usize,
    pub dropped_not_in_wordlist: usize,
    pub wordlist_size: usize,
    pub wordlist_covered: usize,
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct PoolEntry {
    pub text: String,
    pub frequency: u64,
}

fn render(spelling: &str, nucleus: Nucleus, coda: Coda, tone: Tone) -> Option<String> {
    let (rime, mark_at) = nucleus_rime(nucleus, coda)?;
    let coda_text = coda.letters(nucleus);
    let prefix = if spelling == "gi" && rime.starts_with('i') {
        "g"
    } else {
        spelling
    };
    let mut out = String::from(prefix);
    for (i, base) in rime.chars().enumerate() {
        if i == mark_at {
            out.push(tone_char(base, tone));
        } else {
            out.push(base);
        }
    }
    out.push_str(coda_text);
    Some(out)
}

fn zero_onset_variants(nucleus: Nucleus, coda: Coda, tone: Tone) -> Vec<String> {
    let mut forms = Vec::new();
    if let Some(text) = render("", nucleus, coda, tone) {
        forms.push(text);
    }
    match (nucleus, coda) {
        (Nucleus::I, Coda::Open) => forms.push(tone_char('y', tone).to_string()),
        (Nucleus::Ia, Coda::Open) => forms.push(format!("y{}", tone_char('a', tone))),
        (Nucleus::I, Coda::M | Coda::P | Coda::K | Coda::Ng) => {
            forms.push(format!("{}{}", tone_char('y', tone), coda.letters(nucleus)));
        }
        _ => {}
    }
    forms
}

pub fn build_inventory(wordlist: &HashSet<String>) -> (Vec<InventoryEntry>, InventoryReport) {
    let mut entries = Vec::new();
    let mut generated = 0usize;
    let mut covered: HashSet<String> = HashSet::new();
    for &onset in ONSETS.iter().chain(std::iter::once(&Onset::Zero)) {
        for &nucleus in &NUCLEI {
            for &coda in &CODAS {
                for &tone in &TONES {
                    if coda.is_stop() && !matches!(tone, Tone::Sac | Tone::Nang) {
                        continue;
                    }
                    let spellings: Vec<String> = if onset == Onset::Zero {
                        zero_onset_variants(nucleus, coda, tone)
                    } else {
                        onset_spellings(onset, nucleus)
                            .into_iter()
                            .filter_map(|s| render(s, nucleus, coda, tone))
                            .collect()
                    };
                    for text in spellings {
                        generated += 1;
                        if let Some(hit) = wordlist.get(text.as_str()) {
                            covered.insert(hit.clone());
                            entries.push(InventoryEntry {
                                text: hit.clone(),
                                onset,
                                nucleus,
                                coda,
                                tone,
                            });
                        }
                    }
                }
            }
        }
    }
    entries.sort_by(|a, b| a.text.cmp(&b.text));
    entries.dedup_by(|a, b| a.text == b.text);
    let report = InventoryReport {
        generated,
        kept: entries.len(),
        dropped_not_in_wordlist: generated - entries.len(),
        wordlist_size: wordlist.len(),
        wordlist_covered: covered.len(),
    };
    (entries, report)
}

pub fn is_nfc(text: &str) -> bool {
    !text.chars().any(|c| ('\u{0300}'..='\u{036F}').contains(&c))
}

pub fn load_wordlist(path: &Path) -> Result<HashSet<String>, String> {
    let bytes = std::fs::read(path).map_err(|e| format!("read {}: {e}", path.display()))?;
    let text = String::from_utf8(bytes).map_err(|e| format!("utf8 {}: {e}", path.display()))?;
    let mut set = HashSet::new();
    for line in text.lines() {
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        let syllable = line.trim();
        if syllable.is_empty() {
            continue;
        }
        if !is_nfc(syllable) {
            return Err(format!("combining mark in wordlist entry {syllable:?}"));
        }
        set.insert(syllable.to_string());
    }
    Ok(set)
}

pub fn load_syllable_frequencies(path: &Path) -> Result<HashMap<String, u64>, String> {
    let bytes = std::fs::read(path).map_err(|e| format!("read {}: {e}", path.display()))?;
    let text = String::from_utf8(bytes).map_err(|e| format!("utf8 {}: {e}", path.display()))?;
    let mut freqs: HashMap<String, u64> = HashMap::new();
    for (index, line) in text.lines().enumerate() {
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        let mut columns = line.split('\t');
        let _rank = columns.next().unwrap_or_default();
        let count = columns.next().unwrap_or_default();
        let word = columns.next().unwrap_or_default();
        let Ok(parsed_count) = count.trim().parse::<u64>() else {
            return Err(format!(
                "freq row {} has non numeric count {count:?}",
                index + 1
            ));
        };
        for token in word.split_whitespace() {
            if !is_nfc(token) {
                return Err(format!("combining mark in freq token {token:?}"));
            }
            *freqs.entry(token.to_string()).or_insert(0) += parsed_count;
        }
    }
    Ok(freqs)
}

pub fn build_pool<'a>(
    entries: impl Iterator<Item = &'a InventoryEntry>,
    freqs: &HashMap<String, u64>,
    top_k: usize,
) -> Vec<PoolEntry> {
    let mut ranked: Vec<PoolEntry> = entries
        .filter_map(|entry| {
            freqs.get(entry.text.as_str()).map(|&frequency| PoolEntry {
                text: entry.text.clone(),
                frequency,
            })
        })
        .collect();
    ranked.sort_by(|a, b| {
        b.frequency
            .cmp(&a.frequency)
            .then_with(|| a.text.cmp(&b.text))
    });
    ranked.truncate(top_k);
    ranked
}

pub fn config_pool_top_k(config: &serde_json::Value) -> Result<usize, String> {
    config
        .pointer("/ga/pool_top_k")
        .and_then(|v| v.as_u64())
        .map(|v| v as usize)
        .ok_or_else(|| "config section missing: ga.pool_top_k".to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn wordlist_from(entries: &[&str]) -> HashSet<String> {
        entries.iter().map(|s| s.to_string()).collect()
    }

    fn ref_doc_relative() -> std::path::PathBuf {
        let config = bt_data::paths::load_config().expect("config loads");
        std::path::PathBuf::from(
            config
                .pointer("/paths/ref_doc")
                .and_then(|v| v.as_str())
                .expect("paths.ref_doc"),
        )
    }

    fn full_wordlist() -> HashSet<String> {
        let root = bt_data::paths::workspace_root()
            .join(ref_doc_relative())
            .join(VIETNAMESE_DIR);
        let set = load_wordlist(&root.join(WORDLIST_FILE)).expect("wordlist present");
        assert!(set.len() > 7000, "wordlist too small: {}", set.len());
        set
    }

    #[test]
    fn stop_coda_syllables_take_only_sac_or_nang() {
        let (entries, _) = build_inventory(&full_wordlist());
        assert!(!entries.is_empty());
        for entry in &entries {
            if entry.coda.is_stop() {
                assert!(
                    matches!(entry.tone, Tone::Sac | Tone::Nang),
                    "{}",
                    entry.text
                );
            }
        }
    }

    #[test]
    fn renderings_are_pinned() {
        assert_eq!(
            render("kh", Nucleus::Oh, Coda::Ng, Tone::Ngang).unwrap(),
            "không"
        );
        assert_eq!(
            render("t", Nucleus::Ia, Coda::Open, Tone::Sac).unwrap(),
            "tía"
        );
        assert_eq!(
            render("c", Nucleus::Ua, Coda::Open, Tone::Hoi).unwrap(),
            "của"
        );
        assert_eq!(
            render("gi", Nucleus::Ia, Coda::Open, Tone::Nang).unwrap(),
            "gịa"
        );
        assert_eq!(render("m", Nucleus::U, Coda::T, Tone::Sac).unwrap(), "mút");
        assert_eq!(
            render("ngh", Nucleus::Ia, Coda::M, Tone::Nang).unwrap(),
            "nghiệm"
        );
        assert_eq!(render("", Nucleus::E, Coda::Ng, Tone::Sac).unwrap(), "ánh");
        assert_eq!(
            render("b", Nucleus::Ab, Coda::K, Tone::Nang).unwrap(),
            "bặc"
        );
        assert_eq!(
            render("b", Nucleus::Ia, Coda::Ng, Tone::Ngang).unwrap(),
            "biêng"
        );
        assert!(nucleus_rime(Nucleus::Or, Coda::Ng).is_none());
        assert!(nucleus_rime(Nucleus::Uw, Coda::M).is_none());
        assert!(nucleus_rime(Nucleus::Ah, Coda::Open).is_none());
        assert!(nucleus_rime(Nucleus::Ab, Coda::Open).is_none());
        assert!(nucleus_rime(Nucleus::Ua, Coda::P).is_none());
    }

    #[test]
    fn zero_onset_forms_include_y_variants() {
        let forms = zero_onset_variants(Nucleus::Ia, Coda::Open, Tone::Nang);
        assert!(forms.contains(&"ịa".to_string()));
        assert!(forms.contains(&"yạ".to_string()));
        let forms = zero_onset_variants(Nucleus::I, Coda::Open, Tone::Ngang);
        assert!(forms.contains(&"i".to_string()));
        assert!(forms.contains(&"y".to_string()));
        let forms = zero_onset_variants(Nucleus::I, Coda::K, Tone::Sac);
        assert!(forms.contains(&"ích".to_string()));
        assert!(forms.contains(&"ých".to_string()));
    }

    #[test]
    fn inventory_filters_against_wordlist_and_reports() {
        let wordlist = wordlist_from(&["không", "tía", "của", "zzzz"]);
        let (entries, report) = build_inventory(&wordlist);
        let texts: Vec<&str> = entries.iter().map(|e| e.text.as_str()).collect();
        assert!(texts.contains(&"không"));
        assert!(texts.contains(&"tía"));
        assert!(texts.contains(&"của"));
        assert!(!texts.contains(&"zzzz"));
        assert_eq!(report.wordlist_size, 4);
        assert_eq!(report.wordlist_covered, 3);
        assert_eq!(report.kept, entries.len());
        assert_eq!(
            report.dropped_not_in_wordlist,
            report.generated - report.kept
        );
    }

    #[test]
    fn build_inventory_is_deterministic() {
        let wordlist = full_wordlist();
        let (a, ra) = build_inventory(&wordlist);
        let (b, rb) = build_inventory(&wordlist);
        assert_eq!(a, b);
        assert_eq!(ra, rb);
    }

    #[test]
    fn generated_entries_carry_no_combining_marks() {
        let (entries, _) = build_inventory(&full_wordlist());
        for entry in &entries {
            assert!(is_nfc(&entry.text), "{}", entry.text);
        }
    }

    #[test]
    fn pool_ranks_by_frequency_and_stays_in_wordlist() {
        let wordlist = wordlist_from(&["ma", "má", "mà"]);
        let (entries, _) = build_inventory(&wordlist);
        let mut freqs = HashMap::new();
        freqs.insert("má".to_string(), 500);
        freqs.insert("ma".to_string(), 900);
        freqs.insert("mà".to_string(), 900);
        freqs.insert("zzz".to_string(), 99999);
        let pool = build_pool(entries.iter(), &freqs, 2);
        let texts: Vec<&str> = pool.iter().map(|p| p.text.as_str()).collect();
        assert_eq!(texts, vec!["ma", "mà"]);
        for entry in &pool {
            assert!(wordlist.contains(&entry.text));
        }
    }

    #[test]
    fn real_pool_size_meets_case_count() {
        let config = bt_data::paths::load_config().expect("config loads");
        let top_k = config_pool_top_k(&config).expect("pool_top_k");
        let wordlist = full_wordlist();
        let (entries, report) = build_inventory(&wordlist);
        let freqs = load_syllable_frequencies(
            &bt_data::paths::workspace_root()
                .join(ref_doc_relative())
                .join(VIETNAMESE_DIR)
                .join(FREQS_FILE),
        )
        .expect("freqs load");
        let pool = build_pool(entries.iter(), &freqs, top_k);
        assert!(
            pool.len() >= CASE_COUNT,
            "pool {} < {CASE_COUNT} (kept {}, wordlist {} freq {})",
            pool.len(),
            report.kept,
            report.wordlist_size,
            freqs.len()
        );
        let mut seen = HashSet::new();
        for entry in &pool {
            assert!(seen.insert(entry.text.clone()), "duplicate {}", entry.text);
            assert!(wordlist.contains(&entry.text));
            assert!(entry.frequency > 0);
        }
        assert!(
            report.wordlist_covered > 1000,
            "coverage {} too low",
            report.wordlist_covered
        );
    }
}
