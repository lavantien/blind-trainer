use super::SeedRow;
use sha2::{Digest, Sha256};

pub(crate) fn row_hash(row: &SeedRow) -> String {
    use std::fmt::Write as _;
    let joined = format!(
        "{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
        row.event,
        row.method,
        row.stage,
        row.case_key,
        row.alg,
        row.front_json,
        row.source_url,
        row.source_at
    );
    let digest = Sha256::digest(joined.as_bytes());
    let mut hex = String::with_capacity(digest.len() * 2);
    for byte in digest {
        let _ = write!(hex, "{byte:02x}");
    }
    hex
}
