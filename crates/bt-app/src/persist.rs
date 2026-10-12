use bevy::prelude::*;
use bt_data::paths::{self, UserAlgs, UserDir};
use bt_data::srs::Card;
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

#[derive(Clone, Copy, PartialEq, Eq, Debug, Default, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum MemoMode {
    Syllables,
    Images,
    #[default]
    #[serde(rename = "per-orbit")]
    PerOrbit,
}

impl MemoMode {
    pub const LABELS: [(&'static str, MemoMode); 3] = [
        ("syllables", MemoMode::Syllables),
        ("images", MemoMode::Images),
        ("per-orbit", MemoMode::PerOrbit),
    ];

    pub fn parse(text: &str) -> Option<MemoMode> {
        MemoMode::LABELS
            .iter()
            .find(|(label, _)| *label == text)
            .map(|(_, mode)| *mode)
    }

    pub fn label(self) -> &'static str {
        MemoMode::LABELS
            .iter()
            .find(|(_, mode)| *mode == self)
            .map(|(label, _)| *label)
            .expect("every mode has a label")
    }

    pub fn index(self) -> usize {
        MemoMode::LABELS
            .iter()
            .position(|(_, mode)| *mode == self)
            .expect("every mode has an index")
    }

    pub fn from_index(index: usize) -> Option<MemoMode> {
        MemoMode::LABELS.get(index).map(|(_, mode)| *mode)
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct Settings {
    pub memo_cadence_ms: u32,
    pub memo_mode: MemoMode,
    pub drill_reps: u32,
}

#[derive(Clone, PartialEq, Debug, Default, Serialize, Deserialize)]
pub struct SrsStore {
    pub cards: BTreeMap<String, Card>,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct SolveRecord {
    pub started_ms: i64,
    pub memo_ms: i64,
    pub exec_ms: i64,
    pub success: bool,
}

#[derive(Clone, PartialEq, Debug, Default, Serialize, Deserialize)]
pub struct SolveLog {
    pub solves: Vec<SolveRecord>,
}

#[derive(Clone, Debug, Resource)]
pub struct PersistenceRoot(pub PathBuf);

#[derive(Clone, Debug, Resource)]
pub struct Stores {
    pub settings: Settings,
    pub srs: SrsStore,
    pub solves: SolveLog,
    pub user_algs: UserAlgs,
}

fn settings_path(root: &Path) -> PathBuf {
    paths::user_dir_path_at(root, UserDir::Settings).join("settings.json")
}

fn srs_path(root: &Path) -> PathBuf {
    paths::user_dir_path_at(root, UserDir::Srs).join("srs.json")
}

fn solves_path(root: &Path) -> PathBuf {
    paths::user_dir_path_at(root, UserDir::Solves).join("solves.json")
}

fn load_json<T: serde::de::DeserializeOwned>(path: &Path) -> Result<Option<T>, String> {
    paths::read_json(path).map_err(|err| format!("{}: {err}", path.display()))
}

fn save_json<T: Serialize>(path: &Path, value: &T) -> Result<(), String> {
    paths::write_json_atomic(path, value).map_err(|err| format!("{}: {err}", path.display()))
}

impl Stores {
    pub fn load(root: &Path, default_settings: Settings) -> Result<Stores, String> {
        let settings = load_json(&settings_path(root))?.unwrap_or(default_settings);
        let srs = load_json(&srs_path(root))?.unwrap_or_default();
        let solves = load_json(&solves_path(root))?.unwrap_or_default();
        let user_algs = UserAlgs::load_at(root).map_err(|err| err.to_string())?;
        Ok(Stores {
            settings,
            srs,
            solves,
            user_algs,
        })
    }
}

pub fn save_settings(root: &Path, settings: &Settings) -> Result<(), String> {
    save_json(&settings_path(root), settings)
}

pub fn save_srs(root: &Path, srs: &SrsStore) -> Result<(), String> {
    save_json(&srs_path(root), srs)
}

pub fn save_solves(root: &Path, solves: &SolveLog) -> Result<(), String> {
    save_json(&solves_path(root), solves)
}

pub fn save_user_algs(root: &Path, user_algs: &UserAlgs) -> Result<(), String> {
    user_algs.save_at(root).map_err(|err| err.to_string())
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct SaveOutcome {
    pub settings_ok: bool,
    pub srs_ok: bool,
    pub solves_ok: bool,
    pub user_algs_ok: bool,
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU64, Ordering};

    static DIR_COUNTER: AtomicU64 = AtomicU64::new(0);

    fn temp_root(tag: &str) -> PathBuf {
        let dir = std::env::temp_dir()
            .join("bt-app-persist-tests")
            .join(format!(
                "{tag}-{}-{}",
                std::process::id(),
                DIR_COUNTER.fetch_add(1, Ordering::Relaxed)
            ));
        std::fs::create_dir_all(&dir).expect("temp dir");
        dir
    }

    fn sample_settings() -> Settings {
        Settings {
            memo_cadence_ms: 900,
            memo_mode: MemoMode::Images,
            drill_reps: 7,
        }
    }

    #[test]
    fn missing_files_fall_back_to_defaults() {
        let root = temp_root("defaults");
        let default = sample_settings();
        let stores = Stores::load(&root, default).expect("load");
        assert_eq!(stores.settings, default);
        assert!(stores.srs.cards.is_empty());
        assert!(stores.solves.solves.is_empty());
        assert!(stores.user_algs.is_empty());
    }

    #[test]
    fn settings_round_trip() {
        let root = temp_root("settings");
        let stores = Stores::load(&root, sample_settings()).expect("load");
        save_settings(&root, &stores.settings).expect("save");
        let reloaded = Stores::load(
            &root,
            Settings {
                memo_cadence_ms: 1,
                memo_mode: MemoMode::Syllables,
                drill_reps: 1,
            },
        )
        .expect("reload");
        assert_eq!(reloaded.settings, sample_settings());
    }

    #[test]
    fn memo_mode_per_orbit_defaults_and_round_trips() {
        assert_eq!(MemoMode::default(), MemoMode::PerOrbit);
        assert_eq!(MemoMode::PerOrbit.label(), "per-orbit");
        let json = serde_json::to_string(&Settings {
            memo_cadence_ms: 640,
            memo_mode: MemoMode::PerOrbit,
            drill_reps: 6,
        })
        .expect("ser");
        assert!(json.contains("\"per-orbit\""), "json: {json}");
        let back: Settings = serde_json::from_str(&json).expect("de");
        assert_eq!(back.memo_mode, MemoMode::PerOrbit);
        assert_eq!(back.drill_reps, 6);
    }

    #[test]
    fn memo_mode_legacy_values_still_parse() {
        for (text, mode) in [
            ("syllables", MemoMode::Syllables),
            ("images", MemoMode::Images),
            ("per-orbit", MemoMode::PerOrbit),
        ] {
            assert_eq!(MemoMode::parse(text), Some(mode), "parse {text}");
            let from_json: MemoMode = serde_json::from_str(&format!("\"{text}\"")).expect("de");
            assert_eq!(from_json, mode, "serde {text}");
        }
        assert_eq!(MemoMode::parse("perorbit"), None);
        assert_eq!(MemoMode::parse(""), None);
    }

    #[test]
    fn stored_per_orbit_settings_load_verbatim() {
        let root = temp_root("per-orbit-stored");
        let path = settings_path(&root);
        std::fs::create_dir_all(path.parent().expect("parent")).expect("dirs");
        std::fs::write(
            &path,
            serde_json::json!({
                "memo_cadence_ms": 640,
                "memo_mode": "per-orbit",
                "drill_reps": 6
            })
            .to_string(),
        )
        .expect("write");
        let stores = Stores::load(&root, sample_settings()).expect("load");
        assert_eq!(stores.settings.memo_mode, MemoMode::PerOrbit);
        assert_eq!(stores.settings.memo_cadence_ms, 640);
        assert_eq!(stores.settings.drill_reps, 6);
    }

    #[test]
    fn srs_and_solves_round_trip() {
        let root = temp_root("srs");
        let mut stores = Stores::load(&root, sample_settings()).expect("load");
        stores.srs.cards.insert(
            "edge/UF".to_string(),
            Card {
                ease: 2.5,
                interval_days: 6,
                due_day: 10,
                reps: 2,
                lapses: 0,
                reviews: vec![],
            },
        );
        stores.solves.solves.push(SolveRecord {
            started_ms: 5,
            memo_ms: 40_000,
            exec_ms: 25_000,
            success: true,
        });
        save_srs(&root, &stores.srs).expect("save srs");
        save_solves(&root, &stores.solves).expect("save solves");
        let reloaded = Stores::load(&root, sample_settings()).expect("reload");
        assert_eq!(reloaded.srs.cards.len(), 1);
        assert_eq!(
            reloaded
                .srs
                .cards
                .get("edge/UF")
                .map(|card| card.interval_days),
            Some(6)
        );
        assert_eq!(reloaded.solves.solves.len(), 1);
        assert_eq!(reloaded.solves.solves[0].memo_ms, 40_000);
    }

    #[test]
    fn corrupt_settings_reports_path() {
        let root = temp_root("corrupt");
        let path = settings_path(&root);
        std::fs::create_dir_all(path.parent().expect("parent")).expect("dirs");
        std::fs::write(&path, b"{not json").expect("write");
        let err = Stores::load(&root, sample_settings()).expect_err("must fail");
        assert!(err.contains("settings.json"), "unexpected error: {err}");
    }
}
