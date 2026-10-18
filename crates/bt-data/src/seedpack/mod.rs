pub mod hashing;
pub mod normalizer;
pub mod parse;
pub mod registry;
pub mod rulings;

pub use normalizer::{NormalizeError, Normalized, SQ1_EVENT, normalize_alg};
pub use parse::{NOTES_KEY, load_pack, parse_pack};
pub use registry::{load_registry, parse_registry};
pub use rulings::{
    EXPECTED_FAILURES, FailureClass, classify_failure, expected_failure, expected_failure_class,
};

use crate::db::DbError;
use std::fmt;
use std::path::PathBuf;

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
