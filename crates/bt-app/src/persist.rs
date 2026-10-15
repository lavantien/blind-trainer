use bevy::prelude::*;
use bt_data::db::Db;
use bt_data::paths::{self, UserAlgs};
use bt_data::repo_algs;
use bt_data::repo_settings;
use bt_data::repo_solves;
use bt_data::repo_srs;
use serde::{Deserialize, Serialize};
use std::path::{Path, PathBuf};

pub use bt_data::repo_solves::{SolveLog, SolveRecord};
pub use bt_data::repo_srs::SrsStore;

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

#[derive(Clone, Debug, Resource)]
pub struct PersistenceRoot(pub PathBuf);

#[derive(Clone, Debug, Resource)]
pub struct Stores {
    pub settings: Settings,
    pub srs: SrsStore,
    pub solves: SolveLog,
    pub user_algs: UserAlgs,
}

fn open(root: &Path) -> Result<Db, String> {
    Db::open_root(root).map_err(|err| err.to_string())
}

fn db_shown(root: &Path) -> String {
    paths::db_path_at(root).display().to_string()
}

impl Stores {
    pub fn load(root: &Path, default_settings: Settings) -> Result<Stores, String> {
        let mut db = open(root)?;
        bt_data::import::run(&mut db, root)?;
        let settings = match repo_settings::load_text(&db, repo_settings::APP_KEY)
            .map_err(|err| format!("{}: {err}", db_shown(root)))?
        {
            Some(text) => serde_json::from_str(&text)
                .map_err(|err| format!("{} settings: {err}", db_shown(root)))?,
            None => default_settings,
        };
        let srs = repo_srs::load(&db).map_err(|err| format!("{}: {err}", db_shown(root)))?;
        let solves = repo_solves::load(&db).map_err(|err| format!("{}: {err}", db_shown(root)))?;
        let user_algs = repo_algs::load(&db).map_err(|err| format!("{}: {err}", db_shown(root)))?;
        Ok(Stores {
            settings,
            srs,
            solves,
            user_algs,
        })
    }
}

pub fn save_settings(root: &Path, settings: &Settings) -> Result<(), String> {
    let mut db = open(root)?;
    let text = serde_json::to_string(settings)
        .map_err(|err| format!("{} settings: {err}", db_shown(root)))?;
    repo_settings::save(&mut db, repo_settings::APP_KEY, &text)
        .map_err(|err| format!("{}: {err}", db_shown(root)))
}

pub fn save_srs(root: &Path, srs: &SrsStore) -> Result<(), String> {
    let mut db = open(root)?;
    repo_srs::save(&mut db, srs).map_err(|err| format!("{}: {err}", db_shown(root)))
}

pub fn save_solves(root: &Path, solves: &SolveLog) -> Result<(), String> {
    let mut db = open(root)?;
    repo_solves::save(&mut db, solves).map_err(|err| format!("{}: {err}", db_shown(root)))
}

pub fn save_user_algs(root: &Path, user_algs: &UserAlgs) -> Result<(), String> {
    let mut db = open(root)?;
    repo_algs::save(&mut db, user_algs).map_err(|err| format!("{}: {err}", db_shown(root)))
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
    use bt_data::paths::UserAlg;
    use bt_data::srs::Card;
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

    fn write_legacy(root: &Path, rel: &str, text: &str) {
        let path = root.join(rel);
        std::fs::create_dir_all(path.parent().expect("parent")).expect("dirs");
        std::fs::write(&path, text).expect("write legacy");
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
        save_settings(
            &root,
            &Settings {
                memo_cadence_ms: 640,
                memo_mode: MemoMode::PerOrbit,
                drill_reps: 6,
            },
        )
        .expect("save");
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
    fn corrupt_db_fails_closed_naming_the_file() {
        let root = temp_root("corrupt-db");
        std::fs::create_dir_all(&root).expect("dirs");
        std::fs::write(
            paths::db_path_at(&root),
            b"definitely not a sqlite database",
        )
        .expect("corrupt write");
        let err = Stores::load(&root, sample_settings()).expect_err("must fail");
        assert!(err.contains("user_version"), "unexpected error: {err}");
    }

    #[test]
    fn corrupt_settings_row_fails_closed_naming_the_db() {
        let root = temp_root("corrupt-row");
        let mut db = Db::open_root(&root).expect("db");
        repo_settings::save(&mut db, repo_settings::APP_KEY, "{not json").expect("seed row");
        drop(db);
        let err = Stores::load(&root, sample_settings()).expect_err("must fail");
        assert!(err.contains("settings"), "unexpected error: {err}");
        assert!(err.contains("blind-trainer.db"), "unexpected error: {err}");
    }

    #[test]
    fn corrupt_legacy_json_fails_closed_naming_the_file() {
        let root = temp_root("corrupt-legacy");
        write_legacy(&root, "settings/settings.json", "{not json");
        let err = Stores::load(&root, sample_settings()).expect_err("must fail");
        assert!(err.contains("settings.json"), "unexpected error: {err}");
    }

    #[test]
    fn memo_mode_index_round_trips_through_the_label_order() {
        for (index, (_, mode)) in MemoMode::LABELS.iter().enumerate() {
            assert_eq!(mode.index(), index);
            assert_eq!(MemoMode::from_index(index), Some(*mode));
        }
        assert_eq!(MemoMode::from_index(MemoMode::LABELS.len()), None);
    }

    #[test]
    fn every_store_saves_into_the_db() {
        let root = temp_root("db-file");
        save_settings(&root, &sample_settings()).expect("settings");
        save_srs(&root, &SrsStore::default()).expect("srs");
        save_solves(&root, &SolveLog::default()).expect("solves");
        save_user_algs(&root, &UserAlgs::new()).expect("user algs");
        assert!(paths::db_path_at(&root).is_file());
        assert!(!root.join("settings").exists());
        assert!(!root.join("srs").exists());
        assert!(!root.join("solves").exists());
        assert!(!root.join("algs").exists());
    }

    #[test]
    fn user_algs_round_trip_through_the_store() {
        let root = temp_root("algs-roundtrip");
        let mut algs = UserAlgs::new();
        algs.entries.insert(
            "edge/UF-UB-UR".to_string(),
            UserAlg {
                alg: "R2 U' S R2 S' R2 U R2".to_string(),
            },
        );
        save_user_algs(&root, &algs).expect("save");
        let stores = Stores::load(&root, sample_settings()).expect("load");
        assert_eq!(stores.user_algs.entries.len(), 1);
        assert_eq!(
            stores
                .user_algs
                .entries
                .get("edge/UF-UB-UR")
                .map(|entry| entry.alg.as_str()),
            Some("R2 U' S R2 S' R2 U R2")
        );
    }

    #[test]
    fn legacy_stores_import_once_then_the_db_drives_loads() {
        let root = temp_root("legacy-import");
        write_legacy(
            &root,
            "settings/settings.json",
            r#"{"memo_cadence_ms":640,"memo_mode":"per-orbit","drill_reps":6}"#,
        );
        write_legacy(
            &root,
            "algs/user-algs.json",
            r#"{"entries":{"corner/URB":{"alg":"R U' R'"}}}"#,
        );
        let stores = Stores::load(&root, sample_settings()).expect("import load");
        assert_eq!(stores.settings.memo_cadence_ms, 640);
        assert_eq!(stores.user_algs.entries.len(), 1);
        assert!(
            root.join("settings")
                .join("settings.json.migrated")
                .is_file()
        );
        assert!(root.join("algs").join("user-algs.json.migrated").is_file());
        write_legacy(
            &root,
            "settings/settings.json",
            r#"{"memo_cadence_ms":1,"memo_mode":"images","drill_reps":1}"#,
        );
        let again = Stores::load(&root, sample_settings()).expect("second load");
        assert_eq!(again.settings.memo_cadence_ms, 640);
        save_settings(&root, &sample_settings()).expect("save");
        let third = Stores::load(&root, sample_settings()).expect("third load");
        assert_eq!(third.settings, sample_settings());
    }

    #[test]
    fn saving_into_a_file_root_fails_loudly() {
        let file = temp_root("file-root").join("blocker");
        std::fs::write(&file, b"occupied").expect("write");
        assert!(save_settings(&file, &sample_settings()).is_err());
        assert!(save_srs(&file, &SrsStore::default()).is_err());
        assert!(save_solves(&file, &SolveLog::default()).is_err());
        assert!(save_user_algs(&file, &UserAlgs::new()).is_err());
    }

    #[test]
    fn settings_json_uses_the_documented_field_names() {
        let json = serde_json::to_value(sample_settings()).expect("ser");
        assert_eq!(
            json,
            serde_json::json!({
                "memo_cadence_ms": 900,
                "memo_mode": "images",
                "drill_reps": 7
            })
        );
    }
}
