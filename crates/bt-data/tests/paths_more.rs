use bt_data::paths::{
    PathsError, UserAlgs, config_path_from, db_file_name, db_path_at, packs_root, read_json,
    user_root_from, vendor_root_from, workspace_root, write_json_atomic,
};
use std::path::{Path, PathBuf};
use std::sync::Mutex;

static ENV_LOCK: Mutex<()> = Mutex::new(());

fn absolute_env_value() -> String {
    if cfg!(windows) {
        "C:/bt/rg-override".to_string()
    } else {
        "/bt/rg-override".to_string()
    }
}

#[test]
fn vendor_root_env_wins_over_config_pointer() {
    let config = serde_json::json!({"paths": {"ref_ground": "from-config"}});
    assert_eq!(
        vendor_root_from(Some(&absolute_env_value()), &config).unwrap(),
        PathBuf::from(absolute_env_value())
    );
}

#[test]
fn vendor_root_relative_env_joins_the_workspace() {
    let config = serde_json::json!({"paths": {"ref_ground": "from-config"}});
    let resolved = vendor_root_from(Some("rel/rg"), &config).unwrap();
    assert!(resolved.ends_with(PathBuf::from("rel/rg")));
    assert!(resolved.is_absolute());
}

#[test]
fn vendor_root_empty_env_falls_back_to_config_pointer() {
    let config = serde_json::json!({"paths": {"ref_ground": "ref_ground"}});
    let resolved = vendor_root_from(Some(""), &config).unwrap();
    assert!(resolved.ends_with("ref_ground"));
}

#[test]
fn vendor_root_missing_or_non_string_pointer_is_a_missing_section() {
    for config in [
        serde_json::json!({}),
        serde_json::json!({"paths": {}}),
        serde_json::json!({"paths": {"ref_ground": 7}}),
        serde_json::json!({"paths": {"ref_ground": null}}),
    ] {
        match vendor_root_from(None, &config) {
            Err(PathsError::Missing("paths.ref_ground")) => {}
            other => panic!("{config}: {other:?}"),
        }
        assert_eq!(
            vendor_root_from(None, &config).unwrap_err().to_string(),
            "config section missing: paths.ref_ground"
        );
    }
}

#[test]
fn config_path_empty_env_falls_back_to_the_workspace_default() {
    assert!(
        config_path_from(Some("")).ends_with(std::path::Path::new("config").join("config.json"))
    );
    assert!(config_path_from(None).ends_with(std::path::Path::new("config").join("config.json")));
}

#[test]
fn user_root_ignores_empty_env_values_in_precedence_order() {
    assert_eq!(
        user_root_from(Some(""), Some("/xdg/data"), None),
        PathBuf::from("/xdg/data").join("blind-trainer")
    );
    assert_eq!(
        user_root_from(Some(""), Some(""), Some("/home/u")),
        PathBuf::from("/home/u/.local/share").join("blind-trainer")
    );
    assert_eq!(
        user_root_from(Some(""), Some(""), Some("")),
        std::env::temp_dir().join("blind-trainer")
    );
}

#[test]
fn paths_error_display_pins_write_serialize_deserialize() {
    let write = PathsError::Write {
        path: PathBuf::from("out/state.json"),
        source: std::io::Error::other("disk full"),
    };
    assert_eq!(write.to_string(), "write out/state.json: disk full");
    let serde_err = serde_json::from_str::<u8>("nope").unwrap_err();
    let serialize = PathsError::Serialize {
        path: PathBuf::from("out/state.json"),
        source: serde_err,
    };
    assert!(
        serialize
            .to_string()
            .starts_with("serialize out/state.json: ")
    );
    let other_err = serde_json::from_str::<u8>("nope").unwrap_err();
    let deserialize = PathsError::Deserialize {
        path: PathBuf::from("out/state.json"),
        source: other_err,
    };
    assert!(
        deserialize
            .to_string()
            .starts_with("deserialize out/state.json: ")
    );
}

#[test]
fn read_json_deserialize_error_carries_the_path() {
    let dir = std::env::temp_dir().join(format!("bt-paths-more-{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let target = dir.join("broken.json");
    std::fs::write(&target, "{ not json").unwrap();
    match read_json::<serde_json::Value>(&target) {
        Err(PathsError::Deserialize { path, .. }) => assert_eq!(path, target),
        other => panic!("{other:?}"),
    }
    std::fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn foreign_json_layout_loads_as_user_algs() {
    let dir = std::env::temp_dir().join(format!("bt-paths-foreign-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(dir.join("algs")).unwrap();
    std::fs::write(
        dir.join("algs").join("user-algs.json"),
        r#"{"entries":{"edge/UF-UB-UL":{"alg":"M2 U M U2 M' U M2"},"flips/UF & UB":{"alg":"M' U M' U M' U2 M U M U M U2"}}}"#,
    )
    .unwrap();
    let loaded = UserAlgs::load_at(&dir).unwrap();
    assert_eq!(
        loaded.alg_for(bt_data::corpus::SheetClass::Edge, "UF-UB-UL"),
        Some("M2 U M U2 M' U M2")
    );
    assert_eq!(
        loaded.alg_for(bt_data::corpus::SheetClass::Flips, "UF & UB"),
        Some("M' U M' U M' U2 M U M U M U2")
    );
    assert_eq!(loaded.len(), 2);
    std::fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn sequential_atomic_writes_leave_no_temp_files_behind() {
    let dir = std::env::temp_dir().join(format!("bt-paths-atomic-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&dir);
    let first = dir.join("a").join("one.json");
    let second = dir.join("a").join("two.json");
    write_json_atomic(&first, &serde_json::json!({"n": 1})).unwrap();
    write_json_atomic(&second, &serde_json::json!({"n": 2})).unwrap();
    write_json_atomic(&first, &serde_json::json!({"n": 3})).unwrap();
    assert_eq!(
        read_json::<serde_json::Value>(&first).unwrap().unwrap(),
        serde_json::json!({"n": 3})
    );
    let names: Vec<String> = std::fs::read_dir(dir.join("a"))
        .unwrap()
        .map(|e| e.unwrap().file_name().to_string_lossy().to_string())
        .collect();
    assert_eq!(names, vec!["one.json".to_string(), "two.json".to_string()]);
    std::fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn db_paths_follow_the_configured_db_file_and_join_the_root() {
    let _env = ENV_LOCK.lock().unwrap();
    let stash = std::env::var("BT_CONFIG").ok();
    let temp = std::env::temp_dir().join(format!("bt-paths-db-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&temp);
    std::fs::create_dir_all(&temp).unwrap();
    let mut config: serde_json::Value = serde_json::from_str(
        &std::fs::read_to_string(workspace_root().join("config").join("config.json"))
            .expect("repo config"),
    )
    .expect("repo config json");
    config["paths"]["db_file"] = serde_json::json!("custom.db");
    let fixture = temp.join("config.json");
    std::fs::write(&fixture, config.to_string()).expect("fixture");
    unsafe {
        std::env::set_var("BT_CONFIG", &fixture);
    }
    assert_eq!(db_file_name(), "custom.db");
    assert_eq!(
        db_path_at(Path::new("root-dir")),
        PathBuf::from("root-dir").join("custom.db")
    );
    unsafe {
        match stash {
            Some(value) => std::env::set_var("BT_CONFIG", value),
            None => std::env::remove_var("BT_CONFIG"),
        }
    }
    let _ = std::fs::remove_dir_all(&temp);
}

#[test]
fn packs_root_env_override_joins_packs_on_the_absolute_base() {
    let _env = ENV_LOCK.lock().unwrap();
    let stash_config = std::env::var("BT_CONFIG").ok();
    let stash_ground = std::env::var("BT_REF_GROUND").ok();
    let temp = std::env::temp_dir().join(format!("bt-paths-packs-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&temp);
    std::fs::create_dir_all(&temp).unwrap();
    let fixture = temp.join("config.json");
    std::fs::write(&fixture, serde_json::json!({"paths": {}}).to_string()).unwrap();
    unsafe {
        std::env::set_var("BT_CONFIG", &fixture);
        std::env::set_var("BT_REF_GROUND", &temp);
    }
    let resolved = packs_root().unwrap();
    unsafe {
        match stash_ground {
            Some(value) => std::env::set_var("BT_REF_GROUND", value),
            None => std::env::remove_var("BT_REF_GROUND"),
        }
        match stash_config {
            Some(value) => std::env::set_var("BT_CONFIG", value),
            None => std::env::remove_var("BT_CONFIG"),
        }
    }
    assert_eq!(resolved, temp.join("packs"));
    let _ = std::fs::remove_dir_all(&temp);
}
