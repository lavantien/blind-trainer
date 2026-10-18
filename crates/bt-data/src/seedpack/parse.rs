use super::hashing::row_hash;
use super::normalizer::normalize_alg;
use super::{PackEntry, SeedError, SeedRow};
use std::collections::BTreeSet;
use std::path::Path;

pub const NOTES_KEY: &str = "loader_notes";
const PACK_HEADER: &str = "case_key\talg\tfront_json\tsource_url\tsource_at";

pub fn load_pack(packs_root: &Path, entry: &PackEntry) -> Result<Vec<SeedRow>, SeedError> {
    let path = packs_root.join(&entry.file);
    let text = std::fs::read_to_string(&path).map_err(|source| SeedError::Io { path, source })?;
    parse_pack(entry, &text)
}

pub fn parse_pack(entry: &PackEntry, text: &str) -> Result<Vec<SeedRow>, SeedError> {
    let file = entry.file.as_str();
    let mut rows = Vec::new();
    let mut seen: BTreeSet<String> = BTreeSet::new();
    let mut saw_header = false;
    for (index, raw_line) in text.split('\n').enumerate() {
        let line = raw_line.strip_suffix('\r').unwrap_or(raw_line);
        if line.trim().is_empty() {
            continue;
        }
        let line_number = index + 1;
        if !saw_header {
            if line != PACK_HEADER {
                return Err(SeedError::BadHeader {
                    file: file.to_string(),
                });
            }
            saw_header = true;
            continue;
        }
        let cells: Vec<&str> = line.split('\t').collect();
        if cells.len() != 5 {
            return Err(SeedError::RaggedRow {
                file: file.to_string(),
                line: line_number,
                found: cells.len(),
            });
        }
        for (column, cell) in [
            ("case_key", cells[0]),
            ("alg", cells[1]),
            ("front_json", cells[2]),
            ("source_url", cells[3]),
            ("source_at", cells[4]),
        ] {
            if cell.trim().is_empty() {
                return Err(SeedError::EmptyField {
                    file: file.to_string(),
                    line: line_number,
                    column,
                });
            }
        }
        let case_key = cells[0];
        if case_key != case_key.trim() {
            return Err(SeedError::BadField {
                file: file.to_string(),
                case_key: case_key.trim().to_string(),
                field: "case_key padding",
            });
        }
        let source_at = cells[4].trim();
        if !valid_source_at(source_at) {
            return Err(SeedError::BadField {
                file: file.to_string(),
                case_key: case_key.to_string(),
                field: "source_at date",
            });
        }
        if !seen.insert(case_key.to_string()) {
            return Err(SeedError::DuplicateKey {
                file: file.to_string(),
                case_key: case_key.to_string(),
            });
        }
        let normalized =
            normalize_alg(&entry.event, cells[1]).map_err(|source| SeedError::Normalize {
                file: file.to_string(),
                case_key: case_key.to_string(),
                source,
            })?;
        let front_json = enrich_front_json(cells[2].trim(), &normalized.notes, file, case_key)?;
        rows.push(SeedRow {
            event: entry.event.clone(),
            method: entry.method.clone(),
            stage: entry.stage.clone(),
            case_key: case_key.to_string(),
            alg: normalized.alg,
            front_json,
            source_url: cells[3].trim().to_string(),
            source_at: source_at.to_string(),
            content_hash: String::new(),
        });
    }
    if !saw_header {
        return Err(SeedError::BadHeader {
            file: file.to_string(),
        });
    }
    if rows.len() != entry.expected_rows {
        return Err(SeedError::CountDrift {
            file: file.to_string(),
            expected: entry.expected_rows,
            found: rows.len(),
        });
    }
    for row in &mut rows {
        row.content_hash = row_hash(row);
    }
    Ok(rows)
}

fn valid_source_at(text: &str) -> bool {
    let bytes = text.as_bytes();
    bytes.len() == 10
        && bytes[4] == b'-'
        && bytes[7] == b'-'
        && [0, 1, 2, 3, 5, 6, 8, 9]
            .into_iter()
            .all(|index| bytes[index].is_ascii_digit())
}

fn enrich_front_json(
    raw: &str,
    notes: &[String],
    file: &str,
    case_key: &str,
) -> Result<String, SeedError> {
    let mut value: serde_json::Value =
        serde_json::from_str(raw).map_err(|source| SeedError::BadFrontJson {
            file: file.to_string(),
            case_key: case_key.to_string(),
            source,
        })?;
    let object = value
        .as_object_mut()
        .ok_or_else(|| SeedError::FrontJsonShape {
            file: file.to_string(),
            case_key: case_key.to_string(),
        })?;
    if object.contains_key(NOTES_KEY) {
        return Err(SeedError::NoteCollision {
            file: file.to_string(),
            case_key: case_key.to_string(),
        });
    }
    if !notes.is_empty() {
        object.insert(
            NOTES_KEY.to_string(),
            serde_json::Value::Array(
                notes
                    .iter()
                    .map(|note| serde_json::Value::String(note.clone()))
                    .collect(),
            ),
        );
    }
    serde_json::to_string(&value).map_err(|source| SeedError::BadFrontJson {
        file: file.to_string(),
        case_key: case_key.to_string(),
        source,
    })
}
