use bevy::ecs::message::Messages;
use bevy::prelude::*;
use bt_app::logic::{
    AppLogicPlugin, FatalReason, LastSave, NavIntent, RuntimePaths, SaveStores, SettingsChanged,
    VendorCorpus,
};
use bt_app::persist::{MemoMode, Settings, Stores};
use bt_app::states::AppState;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Mutex, OnceLock};

static DIR_COUNTER: AtomicU64 = AtomicU64::new(0);

fn lock() -> &'static Mutex<()> {
    static LOCK: OnceLock<Mutex<()>> = OnceLock::new();
    LOCK.get_or_init(|| Mutex::new(()))
}

fn temp_paths(tag: &str) -> RuntimePaths {
    let base = std::env::temp_dir().join("bt-app-e2e").join(format!(
        "{tag}-{}-{}",
        std::process::id(),
        DIR_COUNTER.fetch_add(1, Ordering::Relaxed)
    ));
    std::fs::create_dir_all(&base).expect("temp dirs");
    let config_dir = base.join("config");
    std::fs::create_dir_all(&config_dir).expect("config dir");
    std::fs::write(
        config_dir.join("config.json"),
        serde_json::json!({
            "app": {
                "window": {"width": 640, "height": 480, "title": "e2e"},
                "memo": {"cadence_ms": 700, "mode": "syllables"},
                "drill": {"reps": 4}
            },
            "srs": {"sm2": {"again": 2, "hard": 3, "good": 4, "easy": 5, "interval_clamp_days": [1, 365]}}
        })
        .to_string(),
    )
    .expect("write config");
    RuntimePaths {
        config_path: config_dir.join("config.json"),
        vendor_root: base.join("vendor"),
        user_root: base.join("user"),
        assets_root: base.join("assets"),
    }
}

fn headless_app(paths: RuntimePaths) -> App {
    let mut app = App::new();
    app.add_plugins(MinimalPlugins)
        .add_plugins(AppLogicPlugin::with_paths(paths));
    app
}

fn pump(app: &mut App, frames: usize) {
    for _ in 0..frames {
        app.update();
    }
}

fn current_state(app: &App) -> AppState {
    *app.world().resource::<State<AppState>>().get()
}

fn settings_file(root: &Path) -> PathBuf {
    root.join("settings").join("settings.json")
}

#[test]
fn bootstrap_reaches_menu_headless() {
    let _guard = lock().lock().expect("test lock");
    let mut app = headless_app(temp_paths("menu"));
    pump(&mut app, 6);
    assert_eq!(current_state(&app), AppState::Menu);
    let settings = app.world().resource::<Stores>().settings;
    assert_eq!(settings.memo_cadence_ms, 700);
    assert_eq!(settings.memo_mode, MemoMode::Syllables);
    assert_eq!(settings.drill_reps, 4);
    assert!(app.world().resource::<VendorCorpus>().0.is_empty());
}

#[test]
fn settings_mutation_persists_across_reload() {
    let _guard = lock().lock().expect("test lock");
    let paths = temp_paths("persist");
    let mutated = Settings {
        memo_cadence_ms: 1234,
        memo_mode: MemoMode::Images,
        drill_reps: 9,
    };
    let mut first = headless_app(temp_paths_clone(&paths));
    pump(&mut first, 6);
    assert_eq!(current_state(&first), AppState::Menu);
    first
        .world_mut()
        .resource_mut::<Messages<SettingsChanged>>()
        .write(SettingsChanged(mutated));
    pump(&mut first, 2);
    assert!(first.world().resource::<LastSave>().settings_ok);
    assert_eq!(first.world().resource::<Stores>().settings, mutated);
    let saved = std::fs::read_to_string(settings_file(&paths.user_root)).expect("settings file");
    assert!(saved.contains("1234"));
    assert!(saved.contains("images"));

    let mut second = headless_app(paths);
    pump(&mut second, 6);
    assert_eq!(current_state(&second), AppState::Menu);
    assert_eq!(second.world().resource::<Stores>().settings, mutated);
}

#[test]
fn per_orbit_mode_survives_reload() {
    let _guard = lock().lock().expect("test lock");
    let paths = temp_paths("perorbit");
    let mut first = headless_app(temp_paths_clone(&paths));
    pump(&mut first, 6);
    assert_eq!(current_state(&first), AppState::Menu);
    assert_eq!(
        first.world().resource::<Stores>().settings.memo_mode,
        MemoMode::Syllables
    );
    let mutated = Settings {
        memo_mode: MemoMode::PerOrbit,
        ..first.world().resource::<Stores>().settings
    };
    first
        .world_mut()
        .resource_mut::<Messages<SettingsChanged>>()
        .write(SettingsChanged(mutated));
    pump(&mut first, 2);
    assert!(first.world().resource::<LastSave>().settings_ok);
    assert_eq!(first.world().resource::<Stores>().settings, mutated);
    let saved = std::fs::read_to_string(settings_file(&paths.user_root)).expect("settings file");
    assert!(saved.contains("\"per-orbit\""), "saved: {saved}");

    let mut second = headless_app(paths);
    pump(&mut second, 6);
    assert_eq!(current_state(&second), AppState::Menu);
    assert_eq!(second.world().resource::<Stores>().settings, mutated);
}

#[test]
fn save_stores_writes_all_files() {
    let _guard = lock().lock().expect("test lock");
    let paths = temp_paths("savestores");
    let mut app = headless_app(temp_paths_clone(&paths));
    pump(&mut app, 6);
    app.world_mut()
        .resource_mut::<Messages<SaveStores>>()
        .write(SaveStores);
    pump(&mut app, 2);
    let last = *app.world().resource::<LastSave>();
    assert!(last.all_ok(), "save outcome: {last:?}");
    assert!(settings_file(&paths.user_root).is_file());
    assert!(paths.user_root.join("srs").join("srs.json").is_file());
    assert!(paths.user_root.join("solves").join("solves.json").is_file());
    assert!(
        paths
            .user_root
            .join("algs")
            .join("user-algs.json")
            .is_file()
    );
}

#[test]
fn fatal_on_missing_config_shows_reason() {
    let _guard = lock().lock().expect("test lock");
    let mut paths = temp_paths("fatal");
    paths.config_path = paths.config_path.with_extension("absent");
    let mut app = headless_app(paths.clone());
    pump(&mut app, 6);
    assert_eq!(current_state(&app), AppState::Fatal);
    let reason = &app.world().resource::<FatalReason>().0;
    assert!(reason.contains("cannot read config"), "reason: {reason}");
    assert!(reason.contains(&paths.config_path.to_string_lossy().to_string()));
}

#[test]
fn nav_intents_drive_state_machine() {
    let _guard = lock().lock().expect("test lock");
    let mut app = headless_app(temp_paths("nav"));
    pump(&mut app, 6);
    assert_eq!(current_state(&app), AppState::Menu);
    for target in [
        AppState::Drill,
        AppState::Memo,
        AppState::Solve,
        AppState::Algs,
        AppState::Stats,
        AppState::Settings,
    ] {
        app.world_mut()
            .resource_mut::<Messages<NavIntent>>()
            .write(NavIntent::Open(target));
        pump(&mut app, 3);
        assert_eq!(current_state(&app), target);
        app.world_mut()
            .resource_mut::<Messages<NavIntent>>()
            .write(NavIntent::ToMenu);
        pump(&mut app, 3);
        assert_eq!(current_state(&app), AppState::Menu);
    }
}

fn temp_paths_clone(paths: &RuntimePaths) -> RuntimePaths {
    RuntimePaths {
        config_path: paths.config_path.clone(),
        vendor_root: paths.vendor_root.clone(),
        user_root: paths.user_root.clone(),
        assets_root: paths.assets_root.clone(),
    }
}
