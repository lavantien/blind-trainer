mod common;

use bt_data::corpus::Corpus;
use bt_gendata::build::{build, manifest};
use bt_gendata::hash::{CorpusDigest, digest_corpus};
use bt_gendata::sha256_hex;
use common::ref_ground;

fn loaded() -> Corpus {
    Corpus::load(&ref_ground()).expect("corpus loads from ref_ground")
}

fn digest() -> CorpusDigest {
    digest_corpus(&ref_ground()).expect("corpus digest")
}

#[test]
fn repeated_loads_and_builds_are_byte_identical() {
    let first = build(&loaded());
    let second = build(&loaded());
    let cases_a = serde_json::to_vec_pretty(&first.cases).unwrap();
    let cases_b = serde_json::to_vec_pretty(&second.cases).unwrap();
    assert_eq!(cases_a, cases_b);
    let specials_a = serde_json::to_vec_pretty(&first.specials).unwrap();
    let specials_b = serde_json::to_vec_pretty(&second.specials).unwrap();
    assert_eq!(specials_a, specials_b);
    let stamp = "2026-10-12T00:00:00Z".to_string();
    let digest = digest();
    let manifest_a = manifest(
        &first,
        &digest,
        &sha256_hex(&cases_a),
        &sha256_hex(&specials_a),
        stamp.clone(),
    );
    let manifest_b = manifest(
        &second,
        &digest,
        &sha256_hex(&cases_b),
        &sha256_hex(&specials_b),
        stamp,
    );
    assert_eq!(
        serde_json::to_vec_pretty(&manifest_a).unwrap(),
        serde_json::to_vec_pretty(&manifest_b).unwrap()
    );
}

#[test]
fn timestamp_is_the_only_varying_manifest_field() {
    let built = build(&loaded());
    let cases = serde_json::to_vec_pretty(&built.cases).unwrap();
    let specials = serde_json::to_vec_pretty(&built.specials).unwrap();
    let digest = digest();
    let early = manifest(
        &built,
        &digest,
        &sha256_hex(&cases),
        &sha256_hex(&specials),
        "2026-01-01T00:00:00Z".to_string(),
    );
    let mut late = manifest(
        &built,
        &digest,
        &sha256_hex(&cases),
        &sha256_hex(&specials),
        "2026-12-31T23:59:59Z".to_string(),
    );
    assert_ne!(
        serde_json::to_vec_pretty(&early).unwrap(),
        serde_json::to_vec_pretty(&late).unwrap()
    );
    late.generated_at = early.generated_at.clone();
    assert_eq!(early, late);
}

#[test]
fn digest_corpus_is_stable_and_complete() {
    let digest = digest();
    assert_eq!(digest.files.len(), 12);
    assert_eq!(digest.rows, 1077);
    let again = digest_corpus(&ref_ground()).expect("corpus digest");
    assert_eq!(digest.sha256, again.sha256);
    for file in &digest.files {
        assert_eq!(file.sha256.len(), 64);
        assert!(file.sha256.chars().all(|c| c.is_ascii_hexdigit()));
    }
}

#[test]
fn digest_corpus_tracks_file_contents() {
    let digest = digest();
    let tmp = std::env::temp_dir().join(format!("bt-gendata-digest-{}", std::process::id()));
    for solver in ["cherry", "eggins"] {
        std::fs::create_dir_all(tmp.join(solver)).unwrap();
        for sheet in ["edge", "corner", "flips", "twists", "parity", "ltct"] {
            let name = format!("{sheet}.tsv");
            std::fs::copy(
                ref_ground().join(solver).join(&name),
                tmp.join(solver).join(&name),
            )
            .unwrap();
        }
    }
    let copied = digest_corpus(&tmp).expect("digest of copy");
    assert_eq!(copied.sha256, digest.sha256);
    assert_eq!(copied.rows, digest.rows);
    let sheet_path = tmp.join("cherry").join("edge.tsv");
    let text = std::fs::read_to_string(&sheet_path).unwrap();
    std::fs::write(&sheet_path, text.replacen("U M2", "U  M2", 1)).unwrap();
    let mutated = digest_corpus(&tmp).expect("digest of mutation");
    assert_ne!(mutated.sha256, digest.sha256);
    assert_ne!(mutated.files[0].sha256, digest.files[0].sha256);
    assert_eq!(mutated.rows, digest.rows);
    for (a, b) in mutated
        .files
        .iter()
        .skip(1)
        .zip(digest.files.iter().skip(1))
    {
        assert_eq!(a.sha256, b.sha256);
    }
    std::fs::remove_dir_all(&tmp).ok();
}
