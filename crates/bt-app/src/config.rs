use bevy::prelude::Resource;
use bt_data::paths;
use serde::Deserialize;
use std::path::Path;

const DEFAULT_WINDOW_WIDTH: u32 = 1280;
const DEFAULT_WINDOW_HEIGHT: u32 = 800;
const DEFAULT_WINDOW_TITLE: &str = "blind trainer";
const DEFAULT_MEMO_CADENCE_MS: u32 = 800;
const DEFAULT_MEMO_MODE: &str = "per-orbit";
const DEFAULT_DRILL_REPS: u32 = 6;
pub(crate) const DEFAULT_SMOKE_SEED: u64 = 20261012;

#[derive(Clone, PartialEq, Eq, Debug, Deserialize)]
pub struct WindowCfg {
    pub width: u32,
    pub height: u32,
    pub title: String,
}

#[derive(Clone, PartialEq, Eq, Debug, Deserialize)]
pub struct MemoCfg {
    pub cadence_ms: u32,
    pub mode: String,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug, Deserialize)]
pub struct DrillCfg {
    pub reps: u32,
}

#[derive(Clone, PartialEq, Eq, Debug, Resource, Deserialize)]
pub struct AppConfig {
    pub window: WindowCfg,
    pub memo: MemoCfg,
    pub drill: DrillCfg,
    #[serde(default = "default_smoke_seed")]
    pub smoke_seed: u64,
}

fn default_smoke_seed() -> u64 {
    DEFAULT_SMOKE_SEED
}

impl Default for AppConfig {
    fn default() -> AppConfig {
        AppConfig {
            window: WindowCfg {
                width: DEFAULT_WINDOW_WIDTH,
                height: DEFAULT_WINDOW_HEIGHT,
                title: DEFAULT_WINDOW_TITLE.to_string(),
            },
            memo: MemoCfg {
                cadence_ms: DEFAULT_MEMO_CADENCE_MS,
                mode: DEFAULT_MEMO_MODE.to_string(),
            },
            drill: DrillCfg {
                reps: DEFAULT_DRILL_REPS,
            },
            smoke_seed: DEFAULT_SMOKE_SEED,
        }
    }
}

pub fn display_path(path: &Path) -> String {
    match path.strip_prefix(paths::workspace_root()) {
        Ok(relative) => relative.display().to_string(),
        Err(_) => path.display().to_string(),
    }
}

pub fn load_config_value(path: &Path) -> Result<serde_json::Value, String> {
    let shown = display_path(path);
    let text = std::fs::read_to_string(path)
        .map_err(|err| format!("cannot read config {shown}: {err}"))?;
    serde_json::from_str(&text).map_err(|err| format!("cannot parse config {shown}: {err}"))
}

pub fn app_config_from(value: &serde_json::Value) -> Result<AppConfig, String> {
    match value.pointer("/app") {
        Some(section) => serde_json::from_value(section.clone())
            .map_err(|err| format!("config app section malformed: {err}")),
        None => Err("config app section missing".to_string()),
    }
}
