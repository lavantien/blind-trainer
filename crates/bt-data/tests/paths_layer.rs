mod common;

use bt_data::corpus::{Corpus, SheetClass, Solver};
use bt_data::paths::{
    UserAlgs, UserDir, config_path_from, read_json, user_algs_path_at, user_dir_path_at,
    user_root_from, write_json_atomic,
};
use common::ref_ground;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

fn temp_root(label: &str) -> PathBuf {
    let dir = std::env::temp_dir().join(format!(
        "bt-data-paths-{}-{}-{}",
        label,
        std::process::id(),
        line!()
    ));
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(&dir).unwrap();
    dir
}

fn tsv_digest(root: &Path) -> BTreeMap<String, u64> {
    fn hash(bytes: &[u8]) -> u64 {
        let mut h: u64 = 0xcbf29ce484222325;
        for &b in bytes {
            h ^= b as u64;
            h = h.wrapping_mul(0x100000001b3);
        }
        h
    }
    let mut map = BTreeMap::new();
    for solver in ["cherry", "eggins"] {
        let dir = root.join(solver);
        for entry in std::fs::read_dir(&dir).unwrap() {
            let path = entry.unwrap().path();
            if path.extension().is_some_and(|e| e == "tsv") {
                let bytes = std::fs::read(&path).unwrap();
                map.insert(
                    format!("{}/{}", solver, path.file_name().unwrap().to_string_lossy()),
                    hash(&bytes),
                );
            }
        }
    }
    map
}

#[test]
fn user_root_prefers_appdata_then_xdg_then_home() {
    assert_eq!(
        user_root_from(Some("C:/Users/u/AppData/Roaming"), None, None),
        PathBuf::from("C:/Users/u/AppData/Roaming").join("blind-trainer")
    );
    assert_eq!(
        user_root_from(None, Some("/home/u/.local/share"), None),
        PathBuf::from("/home/u/.local/share").join("blind-trainer")
    );
    assert_eq!(
        user_root_from(None, None, Some("/home/u")),
        PathBuf::from("/home/u/.local/share").join("blind-trainer")
    );
}

#[test]
fn user_dirs_are_named_subdirectories() {
    let root = PathBuf::from("/base");
    assert_eq!(user_dir_path_at(&root, UserDir::Algs), root.join("algs"));
    assert_eq!(user_dir_path_at(&root, UserDir::Srs), root.join("srs"));
    assert_eq!(
        user_dir_path_at(&root, UserDir::Solves),
        root.join("solves")
    );
    assert_eq!(
        user_dir_path_at(&root, UserDir::Settings),
        root.join("settings")
    );
    assert_eq!(
        user_algs_path_at(&root),
        root.join("algs").join("user-algs.json")
    );
}

#[test]
fn config_path_resolves_env_then_workspace_default() {
    let absolute = if cfg!(windows) {
        PathBuf::from("C:/etc/app.json")
    } else {
        PathBuf::from("/etc/app.json")
    };
    assert_eq!(
        config_path_from(Some(absolute.to_str().unwrap()),),
        absolute
    );
    assert_eq!(
        config_path_from(Some("config/config.json")),
        common::workspace_root().join("config/config.json")
    );
    assert!(config_path_from(None).ends_with("config/config.json"));
}

#[test]
fn atomic_write_creates_reads_and_replaces() {
    let dir = temp_root("atomic");
    let target = dir.join("settings").join("state.json");
    write_json_atomic(&target, &serde_json::json!({"mode": "a"})).unwrap();
    assert_eq!(
        read_json::<serde_json::Value>(&target).unwrap().unwrap(),
        serde_json::json!({"mode": "a"})
    );
    write_json_atomic(&target, &serde_json::json!({"mode": "b"})).unwrap();
    assert_eq!(
        read_json::<serde_json::Value>(&target).unwrap().unwrap(),
        serde_json::json!({"mode": "b"})
    );
    let leftovers: Vec<_> = std::fs::read_dir(target.parent().unwrap())
        .unwrap()
        .map(|e| e.unwrap().file_name().to_string_lossy().to_string())
        .collect();
    assert_eq!(leftovers, vec!["state.json".to_string()]);
    std::fs::remove_dir_all(&dir).ok();
}

struct Unserializable;

impl serde::Serialize for Unserializable {
    fn serialize<S: serde::Serializer>(&self, _: S) -> Result<S::Ok, S::Error> {
        Err(serde::ser::Error::custom("injected failure"))
    }
}

#[test]
fn failed_serialization_leaves_target_and_directory_untouched() {
    let dir = temp_root("badser");
    let target = dir.join("state.json");
    write_json_atomic(&target, &serde_json::json!({"ok": 1})).unwrap();
    assert!(write_json_atomic(&target, &Unserializable).is_err());
    assert_eq!(
        read_json::<serde_json::Value>(&target).unwrap().unwrap(),
        serde_json::json!({"ok": 1})
    );
    let names: Vec<String> = std::fs::read_dir(&dir)
        .unwrap()
        .map(|e| e.unwrap().file_name().to_string_lossy().to_string())
        .collect();
    assert_eq!(names, vec!["state.json".to_string()]);
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn read_json_reports_missing_as_none_and_corrupt_as_error() {
    let dir = temp_root("readjson");
    let target = dir.join("x.json");
    assert!(read_json::<serde_json::Value>(&target).unwrap().is_none());
    std::fs::write(&target, "{ not json").unwrap();
    assert!(read_json::<serde_json::Value>(&target).is_err());
    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn user_algs_round_trip_through_the_user_layer_only() {
    let user = temp_root("overlay");
    let vendor = ref_ground();
    let before = tsv_digest(&vendor);

    let mut overlay = UserAlgs::load_at(&user).unwrap();
    assert!(overlay.is_empty());
    overlay.set(SheetClass::Edge, "UF-UB-UL", "M2");
    overlay.set(SheetClass::Ltct, "UF-UR, UBR-LUB[FUR]", "U R U' R'");
    overlay.save_at(&user).unwrap();

    let path = user_algs_path_at(&user);
    assert!(path.is_file());
    assert!(path.starts_with(&user));
    assert!(!path.starts_with(&vendor));

    let mut reloaded = UserAlgs::load_at(&user).unwrap();
    assert_eq!(reloaded.alg_for(SheetClass::Edge, "UF-UB-UL"), Some("M2"));
    assert_eq!(
        reloaded.alg_for(SheetClass::Ltct, "UF-UR, UBR-LUB[FUR]"),
        Some("U R U' R'")
    );
    assert_eq!(reloaded.len(), 2);
    assert!(reloaded.remove(SheetClass::Edge, "UF-UB-UL"));
    assert!(!reloaded.remove(SheetClass::Edge, "UF-UB-UL"));
    assert_eq!(reloaded.len(), 1);

    let corpus = Corpus::load(&vendor).unwrap();
    let merged = corpus.with_overlay(&reloaded).unwrap();
    assert_eq!(merged.total_rows(), 1077 + 1);
    let group = merged.rows_for_position(SheetClass::Ltct, "UF-UR, UBR-LUB[FUR]");
    assert_eq!(group.len(), 2);
    assert_eq!(group[0].solver, Solver::Cherry);
    assert_eq!(group[1].solver, Solver::User);
    assert_eq!(group[1].alg_text, "U R U' R'");

    let mut fresh = UserAlgs::load_at(&user).unwrap();
    fresh.remove(SheetClass::Ltct, "UF-UR, UBR-LUB[FUR]");
    fresh.set(SheetClass::Edge, "UF-UB-ZZ", "R2");
    let merged_new = corpus.with_overlay(&fresh).unwrap();
    assert_eq!(merged_new.total_rows(), 1077 + 2);
    assert_eq!(
        merged_new
            .rows_for_position(SheetClass::Edge, "UF-UB-UL")
            .len(),
        4
    );
    assert_eq!(
        merged_new
            .rows_for_position(SheetClass::Edge, "UF-UB-ZZ")
            .len(),
        1
    );
    assert_eq!(
        merged_new.rows_for_position(SheetClass::Edge, "UF-UB-ZZ")[0].solver,
        Solver::User
    );

    assert_eq!(tsv_digest(&vendor), before);
    std::fs::remove_dir_all(&user).ok();
}

#[test]
fn overlay_rejects_bad_algs_and_unknown_sheets() {
    let user = temp_root("badalgs");
    let mut overlay = UserAlgs::load_at(&user).unwrap();
    overlay.set(SheetClass::Edge, "UF-UB-UL", "Q9");
    let corpus = Corpus::load(&ref_ground()).unwrap();
    let err = corpus.with_overlay(&overlay).unwrap_err();
    assert!(err.to_string().contains("UF-UB-UL"), "{}", err);
    std::fs::remove_dir_all(&user).ok();
}

#[test]
fn overlay_rows_get_unique_user_rowids_per_sheet() {
    let user = temp_root("rowids");
    let mut overlay = UserAlgs::load_at(&user).unwrap();
    overlay.set(SheetClass::Edge, "UF-UB-UL", "M2");
    overlay.set(SheetClass::Edge, "UF-BL-BD", "E2");
    let corpus = Corpus::load(&ref_ground()).unwrap();
    let merged = corpus.with_overlay(&overlay).unwrap();
    let user_edge = merged.sheet_rows(Solver::User, SheetClass::Edge);
    assert_eq!(user_edge.len(), 2);
    let rowids: Vec<usize> = user_edge.iter().map(|r| r.rowid).collect();
    assert_ne!(rowids[0], rowids[1]);
    for rowid in rowids {
        assert!(merged.row(Solver::User, SheetClass::Edge, rowid).is_some());
    }
    std::fs::remove_dir_all(&user).ok();
}
