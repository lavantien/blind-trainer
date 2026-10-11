use crate::model::CorpusFileEntry;
use bt_data::corpus::{SheetClass, Solver};
use bt_data::tsv::parse_sheet;
use sha2::{Digest, Sha256};
use std::path::Path;

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
}
