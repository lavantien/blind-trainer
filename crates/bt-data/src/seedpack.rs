use crate::db::DbError;
use serde::Deserialize;
use sha2::{Digest, Sha256};
use std::collections::BTreeSet;
use std::fmt;
use std::path::{Path, PathBuf};

pub const SQ1_EVENT: &str = "sq1";
pub const NOTES_KEY: &str = "loader_notes";
const PACK_HEADER: &str = "case_key\talg\tfront_json\tsource_url\tsource_at";
const REPEAT_LIMIT: usize = 99;
const EXPAND_LIMIT: usize = 100_000;

#[derive(Clone, PartialEq, Eq, Debug)]
pub enum NSpec {
    Unset,
    Fixed(i64),
    Sizes(Vec<i64>),
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct PackEntry {
    pub file: String,
    pub event: String,
    pub method: String,
    pub stage: String,
    pub n: NSpec,
    pub expected_rows: usize,
}

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

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Normalized {
    pub alg: String,
    pub notes: Vec<String>,
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct SeedRow {
    pub event: String,
    pub method: String,
    pub stage: String,
    pub case_key: String,
    pub alg: String,
    pub front_json: String,
    pub source_url: String,
    pub source_at: String,
    pub content_hash: String,
}

#[derive(Debug)]
pub enum NormalizeError {
    UnknownToken { alg: String, token: String },
    Unbalanced { alg: String },
    BadRepeat { alg: String },
    BadChain { alg: String },
    EmptyAlg,
}

impl fmt::Display for NormalizeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            NormalizeError::UnknownToken { alg, token } => {
                write!(f, "unknown token '{token}' in alg '{alg}'")
            }
            NormalizeError::Unbalanced { alg } => write!(f, "unbalanced parens in alg '{alg}'"),
            NormalizeError::BadRepeat { alg } => {
                write!(f, "malformed repeat marker in alg '{alg}'")
            }
            NormalizeError::BadChain { alg } => {
                write!(f, "chain marker with an empty alg in '{alg}'")
            }
            NormalizeError::EmptyAlg => write!(f, "alg is empty"),
        }
    }
}

impl std::error::Error for NormalizeError {}

#[derive(Debug)]
pub enum SeedError {
    RegistryJson {
        source: serde_json::Error,
    },
    RegistryRule {
        detail: String,
    },
    Io {
        path: PathBuf,
        source: std::io::Error,
    },
    BadHeader {
        file: String,
    },
    RaggedRow {
        file: String,
        line: usize,
        found: usize,
    },
    EmptyField {
        file: String,
        line: usize,
        column: &'static str,
    },
    BadField {
        file: String,
        case_key: String,
        field: &'static str,
    },
    DuplicateKey {
        file: String,
        case_key: String,
    },
    BadFrontJson {
        file: String,
        case_key: String,
        source: serde_json::Error,
    },
    FrontJsonShape {
        file: String,
        case_key: String,
    },
    NoteCollision {
        file: String,
        case_key: String,
    },
    CountDrift {
        file: String,
        expected: usize,
        found: usize,
    },
    Normalize {
        file: String,
        case_key: String,
        source: NormalizeError,
    },
    Drift {
        file: String,
        case_key: String,
        stored: String,
        packed: String,
    },
    Stale {
        file: String,
        case_key: String,
    },
    Db(DbError),
}

impl fmt::Display for SeedError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            SeedError::RegistryJson { source } => write!(f, "registry parse: {source}"),
            SeedError::RegistryRule { detail } => write!(f, "registry rule: {detail}"),
            SeedError::Io { path, source } => write!(f, "read {}: {source}", path.display()),
            SeedError::BadHeader { file } => {
                write!(
                    f,
                    "pack {file}: header must be case_key/alg/front_json/source_url/source_at"
                )
            }
            SeedError::RaggedRow { file, line, found } => {
                write!(
                    f,
                    "pack {file} line {line}: expected 5 columns, found {found}"
                )
            }
            SeedError::EmptyField { file, line, column } => {
                write!(f, "pack {file} line {line}: {column} column is empty")
            }
            SeedError::BadField {
                file,
                case_key,
                field,
            } => {
                write!(f, "pack {file} row {case_key}: bad {field}")
            }
            SeedError::DuplicateKey { file, case_key } => {
                write!(f, "pack {file}: duplicate case_key {case_key}")
            }
            SeedError::BadFrontJson {
                file,
                case_key,
                source,
            } => {
                write!(
                    f,
                    "pack {file} row {case_key}: front_json is not json: {source}"
                )
            }
            SeedError::FrontJsonShape { file, case_key } => {
                write!(f, "pack {file} row {case_key}: front_json is not an object")
            }
            SeedError::NoteCollision { file, case_key } => {
                write!(
                    f,
                    "pack {file} row {case_key}: front_json already holds {NOTES_KEY}"
                )
            }
            SeedError::CountDrift {
                file,
                expected,
                found,
            } => write!(
                f,
                "pack {file}: row count drift, expected {expected}, found {found}"
            ),
            SeedError::Normalize {
                file,
                case_key,
                source,
            } => {
                write!(f, "pack {file} row {case_key}: {source}")
            }
            SeedError::Drift {
                file,
                case_key,
                stored,
                packed,
            } => write!(
                f,
                "seed drift in pack {file} row {case_key}: stored hash {stored}, pack hash {packed}"
            ),
            SeedError::Stale { file, case_key } => {
                write!(f, "seed row {case_key} no longer exists in pack {file}")
            }
            SeedError::Db(source) => write!(f, "{source}"),
        }
    }
}

impl std::error::Error for SeedError {}

impl From<DbError> for SeedError {
    fn from(source: DbError) -> Self {
        SeedError::Db(source)
    }
}

pub const EXPECTED_FAILURES: &[(&str, &str, &str, &str, &str)] = &[
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.119",
        "unexecutable slash sequence: hand traced, cs.122 differs by one twist sign and executes",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.075",
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.078",
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.084",
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.089",
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.097",
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.109",
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.118",
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.129",
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
];

pub fn expected_failure(
    event: &str,
    method: &str,
    stage: &str,
    case_key: &str,
) -> Option<&'static str> {
    EXPECTED_FAILURES
        .iter()
        .find(|row| row.0 == event && row.1 == method && row.2 == stage && row.3 == case_key)
        .map(|row| row.4)
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

fn row_hash(row: &SeedRow) -> String {
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

pub fn normalize_alg(event: &str, raw: &str) -> Result<Normalized, NormalizeError> {
    if event == SQ1_EVENT {
        normalize_sq1(raw)
    } else {
        normalize_cube(raw)
    }
}

fn normalize_cube(raw: &str) -> Result<Normalized, NormalizeError> {
    let trimmed = raw.trim();
    let mut notes = Vec::new();
    let mut text = trimmed.to_string();
    if text.is_empty() {
        return Err(NormalizeError::EmptyAlg);
    }
    if let Some((left, right)) = text.split_once("->") {
        let alg_part = left.trim();
        let note_part = right.trim();
        if alg_part.is_empty() || note_part.is_empty() {
            return Err(NormalizeError::BadChain {
                alg: trimmed.into(),
            });
        }
        notes.push(format!("chain note: -> {note_part}"));
        text = alg_part.to_string();
    }
    text = expand_bracket_repeats(&text)?;
    text = expand_group_repeats(&text)?;
    text = strip_grouping_parens(&text)?;
    text = text.replace(',', " ");
    let tokens: Vec<&str> = text.split_whitespace().collect();
    if tokens.is_empty() {
        return Err(NormalizeError::EmptyAlg);
    }
    if tokens.iter().all(|token| is_prose_word(token)) {
        notes.push(format!("prose alg: {trimmed}"));
        return Ok(Normalized {
            alg: String::new(),
            notes,
        });
    }
    let mut out = Vec::with_capacity(tokens.len());
    for token in tokens {
        if token == "X" {
            notes.push("mapped X to x".to_string());
            out.push("x".to_string());
        } else if valid_cube_token(token) {
            out.push(token.to_string());
        } else {
            return Err(NormalizeError::UnknownToken {
                alg: trimmed.into(),
                token: token.to_string(),
            });
        }
    }
    Ok(Normalized {
        alg: out.join(" "),
        notes,
    })
}

fn is_prose_word(token: &str) -> bool {
    token.len() >= 2 && token.bytes().all(|byte| byte.is_ascii_lowercase())
}

fn valid_cube_token(token: &str) -> bool {
    let bytes = token.as_bytes();
    let mut index = 0;
    if bytes.len() > 1 && bytes[0].is_ascii_digit() {
        if !(b'2'..=b'7').contains(&bytes[0]) {
            return false;
        }
        index = 1;
    }
    if index >= bytes.len() || !bytes[index].is_ascii_alphabetic() {
        return false;
    }
    if bytes[index] == b'w' || bytes[index] == b'W' {
        return false;
    }
    index += 1;
    if index < bytes.len()
        && (bytes[index] == b'w'
            || (bytes[index].is_ascii_uppercase() && bytes[index - 1].is_ascii_uppercase()))
    {
        index += 1;
    }
    let (mut prime, mut two) = (false, false);
    while index < bytes.len() {
        match bytes[index] {
            b'\'' if !prime => prime = true,
            b'2' if !two => two = true,
            _ => return false,
        }
        index += 1;
    }
    true
}

fn expand_bracket_repeats(text: &str) -> Result<String, NormalizeError> {
    let mut current = text.to_string();
    while let Some(open) = current.find('[') {
        let Some(rel) = current[open + 1..].find(']') else {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        };
        let close = open + 1 + rel;
        let content = &current[open + 1..close];
        if content.contains('[') || content.contains(']') {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        }
        let tail = current[close + 1..].trim_start();
        let Some(star_tail) = tail.strip_prefix('*') else {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        };
        let digit_len = star_tail
            .bytes()
            .take_while(|byte| byte.is_ascii_digit())
            .count();
        if digit_len == 0 {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        }
        let count: usize = star_tail[..digit_len].parse().unwrap_or(0);
        if count == 0 || count > REPEAT_LIMIT {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        }
        let after = star_tail[digit_len..].trim_start();
        if !after.is_empty() {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        }
        let repeated = vec![content.trim(); count].join(" ");
        if repeated.len() > EXPAND_LIMIT {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        }
        current = format!("{} {} {}", &current[..open], repeated, after)
            .trim()
            .to_string();
    }
    Ok(current)
}

struct RepeatHit {
    open: usize,
    close: usize,
    count: usize,
    digits: usize,
}

fn find_group_repeat(text: &str) -> Result<Option<RepeatHit>, NormalizeError> {
    let bytes = text.as_bytes();
    let mut stack: Vec<usize> = Vec::new();
    let mut index = 0;
    while index < bytes.len() {
        match bytes[index] {
            b'(' => stack.push(index),
            b')' => {
                let Some(open) = stack.pop() else {
                    return Err(NormalizeError::Unbalanced { alg: text.into() });
                };
                let inner = &text[open + 1..index];
                if !inner.contains('(') && !inner.contains(')') {
                    let rest = &bytes[index + 1..];
                    let digits = rest.iter().take_while(|byte| byte.is_ascii_digit()).count();
                    if digits > 0 {
                        let count: usize = text[index + 1..index + 1 + digits].parse().unwrap_or(0);
                        let after = rest.get(digits);
                        if count == 0
                            || count > REPEAT_LIMIT
                            || after
                                .is_some_and(|byte| byte.is_ascii_alphanumeric() || *byte == b'\'')
                        {
                            return Err(NormalizeError::BadRepeat { alg: text.into() });
                        }
                        return Ok(Some(RepeatHit {
                            open,
                            close: index,
                            count,
                            digits,
                        }));
                    }
                    if rest
                        .first()
                        .is_some_and(|byte| byte.is_ascii_alphanumeric() || *byte == b'\'')
                    {
                        return Err(NormalizeError::BadRepeat { alg: text.into() });
                    }
                }
            }
            _ => {}
        }
        index += 1;
    }
    if !stack.is_empty() {
        return Err(NormalizeError::Unbalanced { alg: text.into() });
    }
    Ok(None)
}

fn expand_group_repeats(text: &str) -> Result<String, NormalizeError> {
    let mut current = text.to_string();
    while let Some(hit) = find_group_repeat(&current)? {
        let content = current[hit.open + 1..hit.close].trim();
        let repeated = vec![content; hit.count].join(" ");
        if repeated.len() > EXPAND_LIMIT {
            return Err(NormalizeError::BadRepeat { alg: text.into() });
        }
        let tail = current[hit.close + 1 + hit.digits..].trim();
        current = format!("{} {} {}", &current[..hit.open], repeated, tail)
            .trim()
            .to_string();
    }
    Ok(current)
}

fn strip_grouping_parens(text: &str) -> Result<String, NormalizeError> {
    let mut depth = 0i64;
    let mut out = String::with_capacity(text.len());
    for ch in text.chars() {
        match ch {
            '(' => depth += 1,
            ')' => {
                depth -= 1;
                if depth < 0 {
                    return Err(NormalizeError::Unbalanced { alg: text.into() });
                }
            }
            _ => out.push(ch),
        }
    }
    if depth != 0 {
        return Err(NormalizeError::Unbalanced { alg: text.into() });
    }
    Ok(out)
}

fn is_auf_inner(inner: &str) -> bool {
    let tokens: Vec<&str> = inner.split_whitespace().collect();
    (1..=2).contains(&tokens.len())
        && tokens.iter().all(|token| {
            let bytes = token.as_bytes();
            matches!(bytes.first(), Some(b'U') | Some(b'D'))
                && bytes[1..]
                    .iter()
                    .all(|byte| *byte == b'2' || *byte == b'\'')
        })
}

fn valid_int(text: &str) -> bool {
    let bytes = text.as_bytes();
    let digits = text.strip_prefix('-').unwrap_or(text);
    !digits.is_empty()
        && digits.bytes().all(|byte| byte.is_ascii_digit())
        && (digits.len() == 1 || digits.as_bytes()[0] != b'0')
        && !(text.starts_with('-') && digits == "0")
        && bytes
            .iter()
            .all(|byte| byte.is_ascii_digit() || *byte == b'-')
}

fn valid_tuple_ints(inner: &str) -> bool {
    match inner.split_once(',') {
        Some((top, bottom)) => valid_int(top) && valid_int(bottom),
        None => false,
    }
}

fn normalize_sq1(raw: &str) -> Result<Normalized, NormalizeError> {
    let trimmed = raw.trim();
    let mut notes = Vec::new();
    let mut text = trimmed;
    if text.is_empty() {
        return Err(NormalizeError::EmptyAlg);
    }
    if let Some(stripped) = text.strip_suffix('*') {
        let before = stripped.trim_end();
        if before.is_empty() {
            return Err(NormalizeError::UnknownToken {
                alg: trimmed.into(),
                token: "*".into(),
            });
        }
        text = before;
        notes.push("equator marker stripped: *".to_string());
    }
    if text.starts_with('(')
        && let Some(close) = text.find(')')
    {
        let inner = &text[1..close];
        if is_auf_inner(inner) {
            let rest = &text[close + 1..];
            if rest.is_empty() || rest.starts_with(|ch: char| ch.is_whitespace() || ch == '(') {
                notes.push(format!("auf prefix stripped: ({inner})"));
                text = rest.trim_start();
            }
        }
    }
    if text.is_empty() {
        return Err(NormalizeError::EmptyAlg);
    }
    let bytes = text.as_bytes();
    let mut index = 0;
    let mut parts: Vec<String> = Vec::new();
    while index < bytes.len() {
        let byte = bytes[index];
        if byte.is_ascii_whitespace() {
            index += 1;
        } else if byte == b'/' {
            parts.push("/".to_string());
            index += 1;
        } else if byte == b'(' {
            let Some(rel) = bytes[index + 1..].iter().position(|next| *next == b')') else {
                return Err(NormalizeError::Unbalanced {
                    alg: trimmed.into(),
                });
            };
            let close = index + 1 + rel;
            let inner = &text[index + 1..close];
            if !valid_tuple_ints(inner) {
                return Err(NormalizeError::UnknownToken {
                    alg: trimmed.into(),
                    token: text[index..=close].to_string(),
                });
            }
            parts.push(format!("({inner})"));
            index = close + 1;
        } else if byte == b'-' || byte.is_ascii_digit() {
            let start = index;
            while index < bytes.len()
                && (bytes[index] == b'-' || bytes[index] == b',' || bytes[index].is_ascii_digit())
            {
                index += 1;
            }
            let token = &text[start..index];
            if !valid_tuple_ints(token) {
                return Err(NormalizeError::UnknownToken {
                    alg: trimmed.into(),
                    token: token.to_string(),
                });
            }
            parts.push(format!("({token})"));
        } else {
            let end = bytes[index..]
                .iter()
                .position(|next| next.is_ascii_whitespace() || *next == b'/' || *next == b'(')
                .map(|offset| index + offset)
                .unwrap_or(bytes.len());
            return Err(NormalizeError::UnknownToken {
                alg: trimmed.into(),
                token: text[index..end].to_string(),
            });
        }
    }
    Ok(Normalized {
        alg: parts.join(" "),
        notes,
    })
}
