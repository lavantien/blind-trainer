use crate::model::CorpusFileEntry;
use bt_data::corpus::{SheetClass, Solver};
use bt_data::tsv::parse_sheet;
use sha2::{Digest, Sha256};
use std::path::Path;

#[derive(Debug)]
pub struct CorpusDigest {
    pub sha256: String,
    pub rows: usize,
    pub files: Vec<CorpusFileEntry>,
}

fn hex(digest: impl AsRef<[u8]>) -> String {
    let mut out = String::with_capacity(digest.as_ref().len() * 2);
    for byte in digest.as_ref() {
        out.push_str(&format!("{byte:02x}"));
    }
    out
}

pub fn sha256_hex(bytes: &[u8]) -> String {
    let mut hasher = Sha256::new();
    hasher.update(bytes);
    hex(hasher.finalize())
}

pub fn corpus_sha256_hex(parts: &[&[u8]]) -> String {
    let mut hasher = Sha256::new();
    for part in parts {
        hasher.update(part);
    }
    hex(hasher.finalize())
}

pub fn digest_corpus(root: &Path) -> Result<CorpusDigest, String> {
    let mut texts = Vec::new();
    let mut files = Vec::new();
    let mut rows = 0;
    for solver in Solver::VENDOR {
        for sheet in SheetClass::ALL {
            let path = root
                .join(solver.dir())
                .join(format!("{}.tsv", sheet.file()));
            let text = std::fs::read_to_string(&path)
                .map_err(|e| format!("read {}: {e}", path.display()))?;
            let count = parse_sheet(&text)
                .map_err(|e| format!("parse {}: {e}", path.display()))?
                .len();
            rows += count;
            files.push(CorpusFileEntry {
                solver: solver.to_string(),
                sheet: sheet.to_string(),
                rows: count,
                sha256: sha256_hex(text.as_bytes()),
            });
            texts.push(text);
        }
    }
    let parts: Vec<&[u8]> = texts.iter().map(|t| t.as_bytes()).collect();
    Ok(CorpusDigest {
        sha256: corpus_sha256_hex(&parts),
        rows,
        files,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn empty_input_matches_nist_vector() {
        assert_eq!(
            sha256_hex(b""),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
        );
    }

    #[test]
    fn short_input_matches_nist_vector() {
        assert_eq!(
            sha256_hex(b"abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        );
    }

    #[test]
    fn two_block_input_matches_nist_vector() {
        assert_eq!(
            sha256_hex(b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"
        );
    }

    #[test]
    fn corpus_hash_is_the_ordered_concatenation() {
        assert_eq!(corpus_sha256_hex(&[b"a", b"bc"]), sha256_hex(b"abc"));
        assert_eq!(corpus_sha256_hex(&[]), sha256_hex(b""));
    }

    #[test]
    fn digest_corpus_walks_vendor_sheets_in_canonical_order() {
        let root = std::env::temp_dir().join(format!("bt-gendata-digest-{}", std::process::id()));
        let mut texts = Vec::new();
        for (index, solver) in Solver::VENDOR.iter().enumerate() {
            for (slot, sheet) in SheetClass::ALL.iter().enumerate() {
                let data_rows = index * SheetClass::ALL.len() + slot + 1;
                let mut text =
                    String::from("Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n");
                for row in 0..data_rows {
                    text.push_str(&format!("p{index}-{slot}-{row}\tR U R' U'\t\t\t\n"));
                }
                let path = root
                    .join(solver.dir())
                    .join(format!("{}.tsv", sheet.file()));
                std::fs::create_dir_all(path.parent().unwrap()).unwrap();
                std::fs::write(&path, &text).unwrap();
                texts.push(text);
            }
        }
        let digest = digest_corpus(&root).expect("synthetic corpus digests");
        let parts: Vec<&[u8]> = texts.iter().map(|text| text.as_bytes()).collect();
        assert_eq!(digest.sha256, corpus_sha256_hex(&parts));
        assert_eq!(digest.rows, (1..=12usize).sum::<usize>());
        assert_eq!(digest.files.len(), 12);
        for (slot, entry) in digest.files.iter().enumerate() {
            let solver = Solver::VENDOR[slot / SheetClass::ALL.len()];
            let sheet = SheetClass::ALL[slot % SheetClass::ALL.len()];
            assert_eq!(entry.solver, solver.to_string());
            assert_eq!(entry.sheet, sheet.to_string());
            assert_eq!(entry.rows, slot + 1);
            assert_eq!(entry.sha256, sha256_hex(texts[slot].as_bytes()));
        }
        std::fs::remove_dir_all(&root).ok();
    }

    #[test]
    fn digest_corpus_reports_a_missing_sheet() {
        let root =
            std::env::temp_dir().join(format!("bt-gendata-digest-miss-{}", std::process::id()));
        std::fs::create_dir_all(&root).unwrap();
        let err = digest_corpus(&root).unwrap_err();
        assert!(err.starts_with("read "), "{err}");
        std::fs::remove_dir_all(&root).ok();
    }

    #[test]
    fn digest_corpus_rejects_a_malformed_sheet() {
        let root =
            std::env::temp_dir().join(format!("bt-gendata-digest-bad-{}", std::process::id()));
        let path = root.join("cherry").join("edge.tsv");
        std::fs::create_dir_all(path.parent().unwrap()).unwrap();
        std::fs::write(
            &path,
            "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\nonly\ttwo\n",
        )
        .unwrap();
        let err = digest_corpus(&root).unwrap_err();
        assert!(err.starts_with("parse "), "{err}");
        std::fs::remove_dir_all(&root).ok();
    }
}
