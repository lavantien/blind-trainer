use crate::db::Db;
use crate::paths::{
    self, UserAlgs, settings_path_at, solves_path_at, srs_path_at, user_algs_path_at,
};
use crate::repo_algs;
use crate::repo_settings;
use crate::repo_solves::{self, SolveLog};
use crate::repo_srs::{self, SrsStore};
use serde::de::DeserializeOwned;
use std::path::{Path, PathBuf};

pub fn migrated_path(path: &Path) -> PathBuf {
    let mut name = path.as_os_str().to_os_string();
    name.push(".migrated");
    PathBuf::from(name)
}

pub fn run(db: &mut Db, root: &Path) -> Result<(), String> {
    let files = [
        settings_path_at(root),
        srs_path_at(root),
        solves_path_at(root),
        user_algs_path_at(root),
    ];
    if !files.iter().any(|path| path.exists()) {
        return Ok(());
    }
    if !db
        .is_fresh()
        .map_err(|err| format!("{}: {err}", paths::db_path_at(root).display()))?
    {
        return Ok(());
    }
    let settings_text = read_text(&files[0])?;
    let srs = parse_json::<SrsStore>(&files[1])?;
    let solves = parse_json::<SolveLog>(&files[2])?;
    let algs = parse_json::<UserAlgs>(&files[3])?;
    let tx = db
        .transaction()
        .map_err(|err| format!("{}: {err}", paths::db_path_at(root).display()))?;
    if let Some(text) = settings_text.as_deref() {
        repo_settings::save_tx(&tx, repo_settings::APP_KEY, text).map_err(err_at(root))?;
    }
    if let Some(store) = srs.as_ref() {
        repo_srs::save_tx(&tx, store).map_err(err_at(root))?;
    }
    if let Some(log) = solves.as_ref() {
        repo_solves::save_tx(&tx, log).map_err(err_at(root))?;
    }
    if let Some(entries) = algs.as_ref() {
        repo_algs::save_tx(&tx, entries).map_err(err_at(root))?;
    }
    tx.commit()
        .map_err(|source| crate::db::DbError::Exec {
            step: "commit import",
            source,
        })
        .map_err(err_at(root))?;
    for path in files.iter().filter(|path| path.exists()) {
        std::fs::rename(path, migrated_path(path))
            .map_err(|err| format!("rename {}: {err}", path.display()))?;
    }
    Ok(())
}

fn err_at(root: &Path) -> impl Fn(crate::db::DbError) -> String + '_ {
    move |err: crate::db::DbError| format!("{}: {err}", paths::db_path_at(root).display())
}

fn read_text(path: &Path) -> Result<Option<String>, String> {
    match std::fs::read_to_string(path) {
        Ok(text) => match serde_json::from_str::<serde_json::Value>(&text) {
            Ok(_) => Ok(Some(text)),
            Err(err) => Err(format!("{}: {err}", path.display())),
        },
        Err(err) if err.kind() == std::io::ErrorKind::NotFound => Ok(None),
        Err(err) => Err(format!("read {}: {err}", path.display())),
    }
}

fn parse_json<T: DeserializeOwned>(path: &Path) -> Result<Option<T>, String> {
    match std::fs::read(path) {
        Ok(bytes) => serde_json::from_slice(&bytes)
            .map(Some)
            .map_err(|err| format!("{}: {err}", path.display())),
        Err(err) if err.kind() == std::io::ErrorKind::NotFound => Ok(None),
        Err(err) => Err(format!("read {}: {err}", path.display())),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::paths::UserAlg;
    use crate::repo_solves::SolveRecord;
    use crate::srs::{Card, ReviewLogEntry};
    use std::collections::BTreeMap;
    use std::sync::atomic::{AtomicU64, Ordering};

    static DIR_COUNTER: AtomicU64 = AtomicU64::new(0);

    fn temp_root(tag: &str) -> PathBuf {
        let dir = std::env::temp_dir()
            .join("bt-data-import-tests")
            .join(format!(
                "{tag}-{}-{}",
                std::process::id(),
                DIR_COUNTER.fetch_add(1, Ordering::Relaxed)
            ));
        std::fs::create_dir_all(&dir).expect("temp root");
        dir
    }

    fn open(root: &Path) -> Db {
        Db::open_at(&paths::db_path_at(root)).expect("db")
    }

    fn write(path: &Path, text: &str) {
        std::fs::create_dir_all(path.parent().expect("parent")).expect("dirs");
        std::fs::write(path, text).expect("write legacy");
    }

    fn sample_srs() -> SrsStore {
        let mut cards = BTreeMap::new();
        cards.insert(
            "edge/UF".to_string(),
            Card {
                ease: 2.5,
                interval_days: 6,
                due_day: 10,
                reps: 2,
                lapses: 1,
                reviews: vec![ReviewLogEntry {
                    ts_days: 4,
                    quality: 4,
                    elapsed_days: 0,
                    scheduled_days: 6,
                }],
            },
        );
        SrsStore { cards }
    }

    fn sample_solves() -> SolveLog {
        SolveLog {
            solves: vec![SolveRecord {
                started_ms: 5,
                memo_ms: 40_000,
                exec_ms: 25_000,
                success: true,
            }],
        }
    }

    fn sample_algs() -> UserAlgs {
        let mut entries = BTreeMap::new();
        entries.insert(
            "corner/URB".to_string(),
            UserAlg {
                alg: "R U' R'".to_string(),
            },
        );
        UserAlgs { entries }
    }

    fn seed_all_legacy(root: &Path) {
        write(
            &settings_path_at(root),
            r#"{"memo_cadence_ms":640,"memo_mode":"per-orbit","drill_reps":6}"#,
        );
        write(
            &srs_path_at(root),
            &serde_json::to_string(&sample_srs()).expect("srs json"),
        );
        write(
            &solves_path_at(root),
            &serde_json::to_string(&sample_solves()).expect("solves json"),
        );
        write(
            &user_algs_path_at(root),
            &serde_json::to_string(&sample_algs()).expect("algs json"),
        );
    }

    #[test]
    fn imports_every_legacy_store_and_renames_each_file() {
        let root = temp_root("all");
        seed_all_legacy(&root);
        let mut db = open(&root);
        run(&mut db, &root).expect("import");
        assert_eq!(
            crate::repo_settings::load_text(&db, repo_settings::APP_KEY).expect("settings"),
            Some(r#"{"memo_cadence_ms":640,"memo_mode":"per-orbit","drill_reps":6}"#.to_string())
        );
        assert_eq!(crate::repo_srs::load(&db).expect("srs"), sample_srs());
        assert_eq!(
            crate::repo_solves::load(&db).expect("solves"),
            sample_solves()
        );
        assert_eq!(crate::repo_algs::load(&db).expect("algs"), sample_algs());
        for path in [
            settings_path_at(&root),
            srs_path_at(&root),
            solves_path_at(&root),
            user_algs_path_at(&root),
        ] {
            assert!(!path.exists(), "{} must be renamed", path.display());
            assert!(
                migrated_path(&path).is_file(),
                "{} missing",
                migrated_path(&path).display()
            );
        }
    }

    #[test]
    fn partial_legacy_only_imports_what_exists() {
        let root = temp_root("partial");
        write(
            &user_algs_path_at(&root),
            &serde_json::to_string(&sample_algs()).expect("algs json"),
        );
        let mut db = open(&root);
        run(&mut db, &root).expect("import");
        assert_eq!(crate::repo_algs::load(&db).expect("algs"), sample_algs());
        assert_eq!(
            crate::repo_srs::load(&db).expect("srs"),
            SrsStore::default()
        );
        assert_eq!(
            crate::repo_solves::load(&db).expect("solves"),
            SolveLog::default()
        );
        assert!(migrated_path(&user_algs_path_at(&root)).is_file());
    }

    #[test]
    fn skips_import_when_the_db_already_holds_data() {
        let root = temp_root("skip");
        seed_all_legacy(&root);
        let mut db = open(&root);
        repo_settings::save(&mut db, repo_settings::APP_KEY, "existing").expect("seed db");
        run(&mut db, &root).expect("run");
        assert_eq!(
            crate::repo_settings::load_text(&db, repo_settings::APP_KEY).expect("settings"),
            Some("existing".to_string())
        );
        assert_eq!(
            crate::repo_srs::load(&db).expect("srs"),
            SrsStore::default()
        );
        assert!(settings_path_at(&root).is_file());
        assert!(!migrated_path(&settings_path_at(&root)).exists());
    }

    #[test]
    fn corrupt_legacy_json_fails_closed_naming_the_file() {
        let root = temp_root("corrupt");
        write(&srs_path_at(&root), "{not json");
        let mut db = open(&root);
        let err = run(&mut db, &root).expect_err("must fail");
        assert!(err.contains("srs.json"), "err: {err}");
    }

    #[test]
    fn corrupt_legacy_settings_fails_closed_naming_the_file() {
        let root = temp_root("corrupt-settings");
        write(&settings_path_at(&root), "{not json");
        let mut db = open(&root);
        let err = run(&mut db, &root).expect_err("must fail");
        assert!(err.contains("settings.json"), "err: {err}");
        assert!(db.is_fresh().expect("fresh"));
    }

    #[test]
    fn failed_import_leaves_the_db_fresh_for_the_next_run() {
        let root = temp_root("atomic");
        write(
            &settings_path_at(&root),
            r#"{"memo_cadence_ms":1,"memo_mode":"images","drill_reps":1}"#,
        );
        write(&solves_path_at(&root), "{not json");
        let mut db = open(&root);
        assert!(run(&mut db, &root).is_err());
        assert!(db.is_fresh().expect("fresh"));
        assert!(settings_path_at(&root).is_file());
    }

    #[test]
    fn no_legacy_files_is_a_no_op() {
        let root = temp_root("noop");
        let mut db = open(&root);
        run(&mut db, &root).expect("run");
        assert!(db.is_fresh().expect("fresh"));
    }

    #[test]
    fn imported_solves_match_the_legacy_json_exactly() {
        let root = temp_root("roundtrip");
        let log = SolveLog {
            solves: vec![
                SolveRecord {
                    started_ms: -9,
                    memo_ms: 0,
                    exec_ms: i64::MAX / 4,
                    success: false,
                },
                SolveRecord {
                    started_ms: 77,
                    memo_ms: 12_345,
                    exec_ms: 67_890,
                    success: true,
                },
            ],
        };
        write(
            &solves_path_at(&root),
            &serde_json::to_string(&log).expect("json"),
        );
        let mut db = open(&root);
        run(&mut db, &root).expect("import");
        assert_eq!(crate::repo_solves::load(&db).expect("solves"), log);
    }
}
