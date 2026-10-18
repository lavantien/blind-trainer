use super::{NSpec, PackEntry, SeedError};
use serde::Deserialize;
use std::collections::BTreeSet;
use std::path::Path;

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct RawEntry {
    file: String,
    event: String,
    method: String,
    stage: String,
    n: Option<RawN>,
    expected_rows: usize,
}

#[derive(Debug, Deserialize)]
#[serde(untagged)]
enum RawN {
    Fixed(i64),
    Sizes(Vec<i64>),
}
fn valid_size(size: i64) -> bool {
    (2..=7).contains(&size)
}

pub fn parse_registry(text: &str) -> Result<Vec<PackEntry>, SeedError> {
    let raw: Vec<RawEntry> =
        serde_json::from_str(text).map_err(|source| SeedError::RegistryJson { source })?;
    let mut entries = Vec::with_capacity(raw.len());
    let mut seen: BTreeSet<(String, String, String)> = BTreeSet::new();
    for item in raw {
        if item.file.is_empty()
            || item.file.contains('/')
            || item.file.contains('\\')
            || item.file == ".."
        {
            return Err(SeedError::RegistryRule {
                detail: format!("file name '{}' is not a bare name", item.file),
            });
        }
        if item.event.is_empty() || item.method.is_empty() || item.stage.is_empty() {
            return Err(SeedError::RegistryRule {
                detail: format!(
                    "entry for '{}' holds an empty event, method, or stage",
                    item.file
                ),
            });
        }
        if item.expected_rows == 0 {
            return Err(SeedError::RegistryRule {
                detail: format!("entry for '{}' declares zero expected rows", item.file),
            });
        }
        let n = match item.n {
            None => NSpec::Unset,
            Some(RawN::Fixed(size)) => {
                if !valid_size(size) {
                    return Err(SeedError::RegistryRule {
                        detail: format!("entry for '{}' holds n {size} outside 2..=7", item.file),
                    });
                }
                NSpec::Fixed(size)
            }
            Some(RawN::Sizes(sizes)) => {
                if sizes.is_empty() || sizes.iter().any(|size| !valid_size(*size)) {
                    return Err(SeedError::RegistryRule {
                        detail: format!(
                            "entry for '{}' holds an empty or out-of-range n list",
                            item.file
                        ),
                    });
                }
                NSpec::Sizes(sizes)
            }
        };
        let triplet = (item.event.clone(), item.method.clone(), item.stage.clone());
        if !seen.insert(triplet) {
            return Err(SeedError::RegistryRule {
                detail: format!(
                    "duplicate registry key {} / {} / {}",
                    item.event, item.method, item.stage
                ),
            });
        }
        entries.push(PackEntry {
            file: item.file,
            event: item.event,
            method: item.method,
            stage: item.stage,
            n,
            expected_rows: item.expected_rows,
        });
    }
    Ok(entries)
}

pub fn load_registry(packs_root: &Path) -> Result<Vec<PackEntry>, SeedError> {
    let path = packs_root.join("registry.json");
    let text = std::fs::read_to_string(&path).map_err(|source| SeedError::Io { path, source })?;
    parse_registry(&text)
}
