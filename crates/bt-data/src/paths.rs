use crate::corpus::SheetClass;
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::fmt;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};

pub const APP_DIR_NAME: &str = "blind-trainer";
pub const USER_ALGS_FILE: &str = "user-algs.json";

#[derive(Debug)]
pub enum PathsError {
    Read {
        path: PathBuf,
        source: std::io::Error,
    },
    Write {
        path: PathBuf,
        source: std::io::Error,
    },
    Serialize {
        path: PathBuf,
        source: serde_json::Error,
    },
    Deserialize {
        path: PathBuf,
        source: serde_json::Error,
    },
    Missing(&'static str),
}

impl fmt::Display for PathsError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            PathsError::Read { path, source } => {
                write!(f, "read {}: {}", path.display(), source)
            }
            PathsError::Write { path, source } => {
                write!(f, "write {}: {}", path.display(), source)
            }
            PathsError::Serialize { path, source } => {
                write!(f, "serialize {}: {}", path.display(), source)
            }
            PathsError::Deserialize { path, source } => {
                write!(f, "deserialize {}: {}", path.display(), source)
            }
            PathsError::Missing(section) => write!(f, "config section missing: {section}"),
        }
    }
}

impl std::error::Error for PathsError {}

pub fn workspace_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .and_then(|p| p.parent())
        .expect("workspace root from manifest dir")
        .to_path_buf()
}

pub fn config_path_from(env: Option<&str>) -> PathBuf {
    match env.filter(|value| !value.is_empty()) {
        Some(raw) => {
            let path = PathBuf::from(raw);
            if path.is_absolute() {
                path
            } else {
                workspace_root().join(path)
            }
        }
        None => workspace_root().join("config").join("config.json"),
    }
}

pub fn config_path() -> PathBuf {
    config_path_from(std::env::var("BT_CONFIG").ok().as_deref())
}

pub fn load_config() -> Result<serde_json::Value, PathsError> {
    let path = config_path();
    let text = std::fs::read_to_string(&path).map_err(|source| PathsError::Read {
        path: path.clone(),
        source,
    })?;
    serde_json::from_str(&text).map_err(|source| PathsError::Deserialize { path, source })
}

pub fn vendor_root_from(
    env: Option<&str>,
    config: &serde_json::Value,
) -> Result<PathBuf, PathsError> {
    if let Some(raw) = env.filter(|value| !value.is_empty()) {
        let path = PathBuf::from(raw);
        return Ok(if path.is_absolute() {
            path
        } else {
            workspace_root().join(path)
        });
    }
    let relative = config
        .pointer("/paths/ref_ground")
        .and_then(|value| value.as_str())
        .ok_or(PathsError::Missing("paths.ref_ground"))?;
    Ok(workspace_root().join(relative))
}

pub fn vendor_root() -> Result<PathBuf, PathsError> {
    vendor_root_from(
        std::env::var("BT_REF_GROUND").ok().as_deref(),
        &load_config()?,
    )
}

pub fn user_root_from(
    env_appdata: Option<&str>,
    env_xdg: Option<&str>,
    env_home: Option<&str>,
) -> PathBuf {
    let base = env_appdata
        .filter(|value| !value.is_empty())
        .map(PathBuf::from)
        .or_else(|| env_xdg.filter(|value| !value.is_empty()).map(PathBuf::from))
        .or_else(|| {
            env_home
                .filter(|value| !value.is_empty())
                .map(|home| PathBuf::from(home).join(".local").join("share"))
        })
        .unwrap_or_else(std::env::temp_dir);
    base.join(APP_DIR_NAME)
}

pub fn user_root() -> PathBuf {
    user_root_from(
        std::env::var("APPDATA").ok().as_deref(),
        std::env::var("XDG_DATA_HOME").ok().as_deref(),
        std::env::var("HOME").ok().as_deref(),
    )
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum UserDir {
    Algs,
    Srs,
    Solves,
    Settings,
}

impl UserDir {
    pub fn name(self) -> &'static str {
        match self {
            UserDir::Algs => "algs",
            UserDir::Srs => "srs",
            UserDir::Solves => "solves",
            UserDir::Settings => "settings",
        }
    }
}

pub fn user_dir_path_at(root: &Path, kind: UserDir) -> PathBuf {
    root.join(kind.name())
}

pub fn user_dir_path(kind: UserDir) -> PathBuf {
    user_dir_path_at(&user_root(), kind)
}

pub fn user_algs_path_at(root: &Path) -> PathBuf {
    root.join(UserDir::Algs.name()).join(USER_ALGS_FILE)
}

pub fn user_algs_path() -> PathBuf {
    user_algs_path_at(&user_root())
}

pub fn read_json<T: serde::de::DeserializeOwned>(path: &Path) -> Result<Option<T>, PathsError> {
    match std::fs::read(path) {
        Ok(bytes) => {
            serde_json::from_slice(&bytes)
                .map(Some)
                .map_err(|source| PathsError::Deserialize {
                    path: path.to_path_buf(),
                    source,
                })
        }
        Err(err) if err.kind() == std::io::ErrorKind::NotFound => Ok(None),
        Err(source) => Err(PathsError::Read {
            path: path.to_path_buf(),
            source,
        }),
    }
}

static TEMP_COUNTER: AtomicU64 = AtomicU64::new(0);

pub fn write_json_atomic<T: Serialize>(path: &Path, value: &T) -> Result<(), PathsError> {
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent).map_err(|source| PathsError::Write {
            path: parent.to_path_buf(),
            source,
        })?;
    }
    let bytes = serde_json::to_vec_pretty(value).map_err(|source| PathsError::Serialize {
        path: path.to_path_buf(),
        source,
    })?;
    let file_name = path
        .file_name()
        .map(|n| n.to_string_lossy().to_string())
        .unwrap_or_else(|| "data".to_string());
    let sequence = TEMP_COUNTER.fetch_add(1, Ordering::Relaxed);
    let temp = path.with_file_name(format!("{file_name}.tmp-{}-{sequence}", std::process::id()));
    std::fs::write(&temp, &bytes).map_err(|source| PathsError::Write {
        path: temp.clone(),
        source,
    })?;
    match std::fs::rename(&temp, path) {
        Ok(()) => Ok(()),
        Err(source) => {
            std::fs::remove_file(&temp).ok();
            Err(PathsError::Write {
                path: path.to_path_buf(),
                source,
            })
        }
    }
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct UserAlg {
    pub alg: String,
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize, Default)]
pub struct UserAlgs {
    pub entries: BTreeMap<String, UserAlg>,
}

fn key(sheet: SheetClass, position: &str) -> String {
    format!("{}/{position}", sheet.file())
}

impl UserAlgs {
    pub fn new() -> UserAlgs {
        UserAlgs::default()
    }

    pub fn load_at(user_root: &Path) -> Result<UserAlgs, PathsError> {
        Ok(read_json(&user_algs_path_at(user_root))?.unwrap_or_default())
    }

    pub fn load() -> Result<UserAlgs, PathsError> {
        Ok(read_json(&user_algs_path())?.unwrap_or_default())
    }

    pub fn save_at(&self, user_root: &Path) -> Result<(), PathsError> {
        write_json_atomic(&user_algs_path_at(user_root), self)
    }

    pub fn save(&self) -> Result<(), PathsError> {
        write_json_atomic(&user_algs_path(), self)
    }

    pub fn set(&mut self, sheet: SheetClass, position: &str, alg: &str) {
        self.entries.insert(
            key(sheet, position),
            UserAlg {
                alg: alg.to_string(),
            },
        );
    }

    pub fn alg_for(&self, sheet: SheetClass, position: &str) -> Option<&str> {
        self.entries
            .get(&key(sheet, position))
            .map(|entry| entry.alg.as_str())
    }

    pub fn remove(&mut self, sheet: SheetClass, position: &str) -> bool {
        self.entries.remove(&key(sheet, position)).is_some()
    }

    pub fn len(&self) -> usize {
        self.entries.len()
    }

    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }
}
