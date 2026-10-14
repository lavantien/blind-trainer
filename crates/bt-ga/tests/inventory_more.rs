use bt_ga::inventory::{
    Coda, InventoryEntry, Nucleus, Onset, Tone, build_pool, config_pool_top_k,
    load_syllable_frequencies, load_wordlist,
};
use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;

fn scratch(name: &str) -> PathBuf {
    let dir = std::env::temp_dir().join(format!("bt-ga-more-{}-{name}", std::process::id()));
    fs::create_dir_all(&dir).unwrap();
    dir
}

fn entry(text: &str) -> InventoryEntry {
    InventoryEntry {
        text: text.to_string(),
        onset: Onset::M,
        nucleus: Nucleus::A,
        coda: Coda::Open,
        tone: Tone::Ngang,
    }
}

#[test]
fn wordlist_loader_trims_blanks_and_dedupes() {
    let dir = scratch("wordlist-ok");
    let path = dir.join("words.txt");
    fs::write(&path, "# note\n\n   \n  ma  \nma\nmá\n").unwrap();
    let set = load_wordlist(&path).unwrap();
    assert_eq!(set.len(), 2);
    assert!(set.contains("ma") && set.contains("má"));
    fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn wordlist_loader_rejects_missing_broken_and_marked_files() {
    let dir = scratch("wordlist-bad");
    let err = load_wordlist(&dir.join("missing.txt")).unwrap_err();
    assert!(err.contains("missing.txt"), "{err}");
    let path = dir.join("bad.txt");
    fs::write(&path, "la\nla\u{0301}\n").unwrap();
    let err = load_wordlist(&path).unwrap_err();
    assert!(err.contains("combining mark"), "{err}");
    fs::write(&path, [0xffu8, 0xfe, 0x00]).unwrap();
    let err = load_wordlist(&path).unwrap_err();
    assert!(err.contains("utf8"), "{err}");
    fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn frequency_loader_sums_tokens_across_rows() {
    let dir = scratch("freq-ok");
    let path = dir.join("freqs.tsv");
    fs::write(
        &path,
        "# rank\tcount\tword\n\n1\t500\tma\n2\t300\tmá\n3\t100\tma nhiều\n",
    )
    .unwrap();
    let freqs = load_syllable_frequencies(&path).unwrap();
    assert_eq!(freqs.len(), 3);
    assert_eq!(freqs["ma"], 600);
    assert_eq!(freqs["má"], 300);
    assert_eq!(freqs["nhiều"], 100);
    fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn frequency_loader_rejects_broken_rows() {
    let dir = scratch("freq-bad");
    let err = load_syllable_frequencies(&dir.join("missing.tsv")).unwrap_err();
    assert!(err.contains("missing.tsv"), "{err}");
    let path = dir.join("freqs.tsv");
    fs::write(&path, "1\t500\tma\n2\tx\tb\n").unwrap();
    let err = load_syllable_frequencies(&path).unwrap_err();
    assert!(err.contains("row 2"), "{err}");
    fs::write(&path, "1\t500\tma\u{0301}\n").unwrap();
    let err = load_syllable_frequencies(&path).unwrap_err();
    assert!(err.contains("combining mark"), "{err}");
    fs::write(&path, [0xffu8, 0xfe]).unwrap();
    let err = load_syllable_frequencies(&path).unwrap_err();
    assert!(err.contains("utf8"), "{err}");
    fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn pool_truncates_orders_by_frequency_then_text() {
    let entries: Vec<InventoryEntry> = ["z", "b", "a", "m"].iter().map(|t| entry(t)).collect();
    let mut freqs = HashMap::new();
    freqs.insert("z".to_string(), 1);
    freqs.insert("b".to_string(), 3);
    freqs.insert("a".to_string(), 3);
    assert!(build_pool(entries.iter(), &freqs, 0).is_empty());
    assert!(build_pool(entries.iter(), &HashMap::new(), 10).is_empty());
    let pool = build_pool(entries.iter(), &freqs, 2);
    let texts: Vec<&str> = pool.iter().map(|p| p.text.as_str()).collect();
    assert_eq!(texts, vec!["a", "b"]);
    assert_eq!(pool[0].frequency, 3);
    let pool = build_pool(entries.iter(), &freqs, 10);
    let texts: Vec<&str> = pool.iter().map(|p| p.text.as_str()).collect();
    assert_eq!(texts, vec!["a", "b", "z"]);
}

#[test]
fn pool_top_k_reads_only_whole_numbers_from_the_hub() {
    let ok = serde_json::json!({"ga": {"pool_top_k": 822}});
    assert_eq!(config_pool_top_k(&ok).unwrap(), 822);
    let err = config_pool_top_k(&serde_json::json!({"ga": {}})).unwrap_err();
    assert!(err.contains("ga.pool_top_k"), "{err}");
    for wrong in [
        serde_json::json!({"ga": {"pool_top_k": "822"}}),
        serde_json::json!({"ga": {"pool_top_k": 1.5}}),
        serde_json::json!({"ga": {"pool_top_k": -1}}),
        serde_json::json!({}),
    ] {
        assert!(config_pool_top_k(&wrong).is_err(), "{wrong}");
    }
}
