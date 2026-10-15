use crate::persist::{self, MemoMode, PersistenceRoot, Settings, Stores};
use crate::states::{AppState, DrillPhase, MemoPhase, SolvePhase};
use bevy::input::ButtonState;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
use bevy::state::app::StatesPlugin;
use bt_core::Cube;
use bt_data::corpus::Corpus;
use bt_data::fsrs::FsrsConfig;
use bt_data::paths;
use bt_scheme::EDGE_BUFFER_STICKER;
use serde::Deserialize;
use std::path::{Path, PathBuf};

const DEFAULT_WINDOW_WIDTH: u32 = 1280;
const DEFAULT_WINDOW_HEIGHT: u32 = 800;
const DEFAULT_WINDOW_TITLE: &str = "blind trainer";
const DEFAULT_MEMO_CADENCE_MS: u32 = 800;
const DEFAULT_MEMO_MODE: &str = "per-orbit";
const DEFAULT_DRILL_REPS: u32 = 6;
const DEFAULT_SMOKE_SEED: u64 = 20261012;
const DEFAULT_REF_GROUND: &str = "ref_ground";
const DEFAULT_ASSETS_ROOT: &str = "assets";

#[derive(Clone, Debug, Resource)]
pub struct RuntimePaths {
    pub config_path: PathBuf,
    pub vendor_root: PathBuf,
    pub user_root: PathBuf,
    pub assets_root: PathBuf,
}

impl RuntimePaths {
    pub fn from_env() -> RuntimePaths {
        let exe_dir = std::env::current_exe()
            .ok()
            .and_then(|exe| exe.parent().map(|dir| dir.to_path_buf()));
        RuntimePaths::resolve(RuntimePaths::workspace_layout(), exe_dir.as_deref())
    }

    fn workspace_layout() -> RuntimePaths {
        let config_path = paths::config_path();
        let config = load_config_value(&config_path).unwrap_or(serde_json::Value::Null);
        let vendor_root =
            paths::vendor_root_from(std::env::var("BT_REF_GROUND").ok().as_deref(), &config)
                .unwrap_or_else(|_| paths::workspace_root().join(DEFAULT_REF_GROUND));
        let assets_root = config
            .pointer("/paths/fonts")
            .and_then(|value| value.as_str())
            .and_then(|fonts| Path::new(fonts).parent())
            .map(|assets| paths::workspace_root().join(assets))
            .unwrap_or_else(|| paths::workspace_root().join(DEFAULT_ASSETS_ROOT));
        RuntimePaths {
            config_path,
            vendor_root,
            user_root: paths::user_root(),
            assets_root,
        }
    }

    pub fn resolve(primary: RuntimePaths, exe_dir: Option<&Path>) -> RuntimePaths {
        let Some(dir) = exe_dir else {
            return primary;
        };
        RuntimePaths {
            config_path: resolve_root(
                primary.config_path,
                &[dir.join("config").join("config.json")],
            ),
            vendor_root: resolve_root(primary.vendor_root, &[dir.join(DEFAULT_REF_GROUND)]),
            assets_root: resolve_root(primary.assets_root, &[dir.join(DEFAULT_ASSETS_ROOT)]),
            user_root: primary.user_root,
        }
    }
}

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

fn resolve_root(primary: PathBuf, candidates: &[PathBuf]) -> PathBuf {
    if primary.exists() {
        return primary;
    }
    candidates
        .iter()
        .find(|candidate| candidate.exists())
        .cloned()
        .unwrap_or(primary)
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

#[derive(Clone, Debug, Resource, Default)]
pub struct FatalReason(pub String);

#[derive(Clone, Debug, Resource, Default)]
pub struct BootWarnings(pub Vec<String>);

#[derive(Clone, Debug, Resource)]
pub struct Fsrs(pub FsrsConfig);

#[derive(Debug, Resource)]
pub struct VendorCorpus(pub Corpus);

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum HighlightKind {
    Buffer,
    Target,
}

#[derive(Clone, Debug, Resource)]
pub struct NetState {
    pub cube: Cube,
    pub buffer_sticker: Option<u8>,
    pub targets: [Option<u8>; 2],
}

impl Default for NetState {
    fn default() -> NetState {
        NetState {
            cube: Cube::solved(),
            buffer_sticker: Some(EDGE_BUFFER_STICKER),
            targets: [None, None],
        }
    }
}

impl NetState {
    pub fn highlight_kind(&self, sticker: u8) -> Option<HighlightKind> {
        if self.buffer_sticker == Some(sticker) {
            Some(HighlightKind::Buffer)
        } else if self.targets.contains(&Some(sticker)) {
            Some(HighlightKind::Target)
        } else {
            None
        }
    }
}

#[derive(Message, Clone, Copy, Debug)]
pub enum NavIntent {
    ToMenu,
    Open(AppState),
}

#[derive(Message, Clone, Debug)]
pub struct SettingsChanged(pub Settings);

#[derive(Message, Clone, Copy, Debug)]
pub struct SaveStores;

#[derive(Clone, Copy, PartialEq, Eq, Debug, Default, Resource)]
pub struct LastSave {
    pub settings_ok: bool,
    pub srs_ok: bool,
    pub solves_ok: bool,
    pub user_algs_ok: bool,
}

impl LastSave {
    pub fn all_ok(&self) -> bool {
        self.settings_ok && self.srs_ok && self.solves_ok && self.user_algs_ok
    }
}

#[derive(Debug)]
struct Booted {
    config: AppConfig,
    fsrs: FsrsConfig,
    corpus: Corpus,
    stores: Stores,
    warnings: Vec<String>,
}

fn default_settings(config: &AppConfig) -> Result<Settings, String> {
    let memo_mode = MemoMode::parse(&config.memo.mode)
        .ok_or_else(|| format!("config app.memo.mode invalid: {}", config.memo.mode))?;
    Ok(Settings {
        memo_cadence_ms: config.memo.cadence_ms.clamp(1, 10_000),
        memo_mode,
        drill_reps: config.drill.reps.clamp(1, 100),
    })
}

fn boot(paths: &RuntimePaths) -> Result<Booted, String> {
    let value = load_config_value(&paths.config_path)?;
    let config = app_config_from(&value)?;
    let fsrs =
        FsrsConfig::from_value(&value).map_err(|err| format!("config srs section: {err}"))?;
    let stores = Stores::load(&paths.user_root, default_settings(&config)?, &fsrs)?;
    let (corpus, warnings) = if paths.vendor_root.is_dir() {
        let loaded = Corpus::load(&paths.vendor_root)
            .map_err(|err| format!("vendor corpus at {}: {err}", paths.vendor_root.display()))?;
        let merged = loaded
            .with_overlay(&stores.user_algs)
            .map_err(|err| format!("user algs overlay: {err}"))?;
        (merged, Vec::new())
    } else {
        (
            Corpus::from_rows(Vec::new()),
            vec![format!(
                "vendor data missing at {}, running on user data only",
                paths.vendor_root.display()
            )],
        )
    };
    Ok(Booted {
        config,
        fsrs,
        corpus,
        stores,
        warnings,
    })
}

fn bootstrap_load(world: &mut World) {
    let paths = world.resource::<RuntimePaths>().clone();
    match boot(&paths) {
        Ok(booted) => {
            world.insert_resource(booted.config);
            world.insert_resource(Fsrs(booted.fsrs));
            world.insert_resource(VendorCorpus(booted.corpus));
            world.insert_resource(PersistenceRoot(paths.user_root.clone()));
            world.insert_resource(booted.stores);
            world.resource_mut::<BootWarnings>().0 = booted.warnings;
            world
                .resource_mut::<NextState<AppState>>()
                .set(AppState::Menu);
        }
        Err(reason) => {
            world.resource_mut::<FatalReason>().0 = reason;
            world
                .resource_mut::<NextState<AppState>>()
                .set(AppState::Fatal);
        }
    }
}

fn keyboard_intents(
    state: Res<State<AppState>>,
    mut focus: Option<ResMut<bevy::input_focus::InputFocus>>,
    editables: Query<&bevy::text::EditableText>,
    mut inputs: MessageReader<KeyboardInput>,
    mut intents: MessageWriter<NavIntent>,
) {
    let current = *state.get();
    if current == AppState::Bootstrap || current == AppState::Fatal {
        inputs.clear();
        return;
    }
    for input in inputs.read() {
        if input.state != ButtonState::Pressed {
            continue;
        }
        if input.key_code == KeyCode::Escape {
            let typing = focus
                .as_deref()
                .and_then(|focus| focus.get())
                .is_some_and(|entity| editables.get(entity).is_ok());
            if typing && let Some(focus) = focus.as_deref_mut() {
                focus.clear();
                continue;
            }
            intents.write(NavIntent::ToMenu);
            continue;
        }
        if current == AppState::Menu
            && let Some((_, target)) = AppState::MENU_KEYS
                .iter()
                .find(|(key, _)| *key == input.key_code)
        {
            intents.write(NavIntent::Open(*target));
        }
    }
}

fn apply_nav_intent(
    mut commands: Commands,
    state: Res<State<AppState>>,
    mut intents: MessageReader<NavIntent>,
) {
    for intent in intents.read() {
        match *intent {
            NavIntent::ToMenu => {
                if *state.get() != AppState::Menu {
                    commands.queue(|world: &mut World| {
                        world
                            .resource_mut::<NextState<AppState>>()
                            .set(AppState::Menu);
                    });
                }
            }
            NavIntent::Open(target) => {
                commands.queue(move |world: &mut World| {
                    world.resource_mut::<NextState<AppState>>().set(target);
                });
            }
        }
    }
}

fn apply_settings_change(
    root: Res<PersistenceRoot>,
    mut stores: ResMut<Stores>,
    mut last: ResMut<LastSave>,
    mut changes: MessageReader<SettingsChanged>,
) {
    for change in changes.read() {
        stores.settings = change.0;
        last.settings_ok = persist::save_settings(&root.0, &stores.settings).is_ok();
    }
}

fn save_stores_on_request(
    root: Res<PersistenceRoot>,
    stores: Res<Stores>,
    mut requests: MessageReader<SaveStores>,
    mut last: ResMut<LastSave>,
) {
    if requests.read().next().is_none() {
        return;
    }
    *last = LastSave {
        settings_ok: persist::save_settings(&root.0, &stores.settings).is_ok(),
        srs_ok: persist::save_srs(&root.0, &stores.srs).is_ok(),
        solves_ok: persist::save_solves(&root.0, &stores.solves).is_ok(),
        user_algs_ok: persist::save_user_algs(&root.0, &stores.user_algs).is_ok(),
    };
}

#[derive(Default)]
pub struct AppLogicPlugin {
    pub paths: Option<RuntimePaths>,
}

impl AppLogicPlugin {
    pub fn with_paths(paths: RuntimePaths) -> AppLogicPlugin {
        AppLogicPlugin { paths: Some(paths) }
    }
}

impl Plugin for AppLogicPlugin {
    fn build(&self, app: &mut App) {
        let paths = self.paths.clone().unwrap_or_else(RuntimePaths::from_env);
        if !app.is_plugin_added::<StatesPlugin>() {
            app.add_plugins(StatesPlugin);
        }
        app.insert_resource(paths)
            .init_state::<AppState>()
            .add_sub_state::<DrillPhase>()
            .add_sub_state::<SolvePhase>()
            .add_sub_state::<MemoPhase>()
            .init_resource::<FatalReason>()
            .init_resource::<BootWarnings>()
            .init_resource::<NetState>()
            .init_resource::<LastSave>()
            .add_message::<NavIntent>()
            .add_message::<SettingsChanged>()
            .add_message::<SaveStores>()
            .add_message::<KeyboardInput>()
            .add_systems(OnEnter(AppState::Bootstrap), bootstrap_load)
            .add_systems(
                Update,
                (apply_settings_change, save_stores_on_request)
                    .chain()
                    .run_if(resource_exists::<Stores>),
            )
            .add_systems(Update, (keyboard_intents, apply_nav_intent).chain());
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use bevy::ecs::message::Messages;
    use bevy::ecs::schedule::{IntoScheduleConfigs, Schedule};
    use bevy::ecs::system::ScheduleSystem;
    use bevy::input::keyboard::Key;
    use bevy::input_focus::InputFocus;
    use bt_data::srs::Card;
    use std::sync::atomic::{AtomicU64, Ordering};

    static DIR_COUNTER: AtomicU64 = AtomicU64::new(0);

    fn temp_paths(tag: &str) -> RuntimePaths {
        let base = std::env::temp_dir()
            .join("bt-app-logic-tests")
            .join(format!(
                "{tag}-{}-{}",
                std::process::id(),
                DIR_COUNTER.fetch_add(1, Ordering::Relaxed)
            ));
        std::fs::create_dir_all(&base).expect("temp dirs");
        write_config(&base);
        RuntimePaths {
            config_path: base.join("config").join("config.json"),
            vendor_root: base.join("vendor"),
            user_root: base.join("user"),
            assets_root: base.join("assets"),
        }
    }

    fn write_config(base: &Path) {
        let dir = base.join("config");
        std::fs::create_dir_all(&dir).expect("config dir");
        std::fs::write(
            dir.join("config.json"),
            serde_json::json!({
                "app": {
                    "window": {"width": 640, "height": 480, "title": "t"},
                    "memo": {"cadence_ms": 500, "mode": "images"},
                    "drill": {"reps": 3}
                },
                "srs": {"fsrs": {"weights": [0.40255, 1.18385, 3.173, 15.69105, 7.1949, 0.5345, 1.4604, 0.0046, 1.54575, 0.1192, 1.01925, 1.9395, 0.11, 0.29605, 2.2698, 0.2315, 2.9898, 0.51655, 0.6621], "request_retention": 0.9, "interval_clamp_days": [1, 365]}}
            })
            .to_string(),
        )
        .expect("write config");
    }

    #[test]
    fn resolve_swaps_missing_roots_to_the_install_layout() {
        let primary = temp_paths("resolve-missing");
        let install = std::env::temp_dir()
            .join("bt-app-logic-tests")
            .join(format!("resolve-install-{}", std::process::id()));
        std::fs::create_dir_all(install.join("config")).expect("install config dir");
        std::fs::create_dir_all(install.join("ref_ground")).expect("install vendor dir");
        std::fs::create_dir_all(install.join("assets")).expect("install assets dir");
        std::fs::write(
            install.join("config").join("config.json"),
            serde_json::json!({}).to_string(),
        )
        .expect("install config file");
        let resolved = RuntimePaths::resolve(
            RuntimePaths {
                config_path: primary.config_path.with_extension("absent"),
                vendor_root: primary.vendor_root.join("absent"),
                user_root: primary.user_root.clone(),
                assets_root: primary.assets_root.join("absent"),
            },
            Some(&install),
        );
        assert_eq!(
            resolved.config_path,
            install.join("config").join("config.json")
        );
        assert_eq!(resolved.vendor_root, install.join("ref_ground"));
        assert_eq!(resolved.assets_root, install.join("assets"));
        assert_eq!(resolved.user_root, primary.user_root);
    }

    #[test]
    fn resolve_keeps_existing_workspace_roots() {
        let primary = temp_paths("resolve-keeps");
        std::fs::create_dir_all(&primary.vendor_root).expect("vendor exists");
        let resolved = RuntimePaths::resolve(primary.clone(), Some(&std::env::temp_dir()));
        assert_eq!(resolved.config_path, primary.config_path);
        assert_eq!(resolved.vendor_root, primary.vendor_root);
        assert_eq!(resolved.assets_root, primary.assets_root);
    }

    #[test]
    fn resolve_returns_primary_when_nothing_exists() {
        let primary = temp_paths("resolve-none");
        let resolved = RuntimePaths::resolve(
            RuntimePaths {
                config_path: primary.config_path.with_extension("absent"),
                vendor_root: primary.vendor_root.join("absent"),
                user_root: primary.user_root.clone(),
                assets_root: primary.assets_root.join("absent"),
            },
            Some(&primary.vendor_root),
        );
        assert_eq!(
            resolved.config_path,
            primary.config_path.with_extension("absent")
        );
        assert_eq!(resolved.vendor_root, primary.vendor_root.join("absent"));
        assert_eq!(resolved.assets_root, primary.assets_root.join("absent"));
    }

    #[test]
    fn resolve_without_exe_dir_is_identity() {
        let primary = temp_paths("resolve-identity");
        let resolved = RuntimePaths::resolve(primary.clone(), None);
        assert_eq!(resolved.config_path, primary.config_path);
        assert_eq!(resolved.vendor_root, primary.vendor_root);
    }

    #[test]
    fn boot_reaches_menu_payload_without_vendor() {
        let paths = temp_paths("menu");
        let booted = boot(&paths).expect("boot");
        assert!(booted.corpus.is_empty());
        assert_eq!(booted.warnings.len(), 1);
        assert!(booted.warnings[0].contains("vendor data missing"));
        assert_eq!(booted.stores.settings.memo_mode, MemoMode::Images);
        assert_eq!(booted.stores.settings.drill_reps, 3);
        assert_eq!(booted.config.window.width, 640);
    }

    #[test]
    fn boot_fatals_on_partial_vendor() {
        let paths = temp_paths("partialvendor");
        std::fs::create_dir_all(&paths.vendor_root).expect("vendor dir");
        let err = boot(&paths).expect_err("must fatal");
        assert!(err.contains("vendor corpus"), "unexpected: {err}");
        assert!(err.contains("edge.tsv"));
    }

    #[test]
    fn boot_fatals_on_missing_config_with_reason() {
        let mut paths = temp_paths("noconfig");
        paths.config_path = paths.config_path.with_extension("missing");
        let err = boot(&paths).expect_err("must fatal");
        assert!(err.contains("cannot read config"), "unexpected: {err}");
        assert!(err.contains("config.missing"));
    }

    #[test]
    fn boot_fatals_on_missing_app_section() {
        let paths = temp_paths("noapp");
        std::fs::write(
            &paths.config_path,
            serde_json::json!({"srs": {"fsrs": {"weights": [0.40255, 1.18385, 3.173, 15.69105, 7.1949, 0.5345, 1.4604, 0.0046, 1.54575, 0.1192, 1.01925, 1.9395, 0.11, 0.29605, 2.2698, 0.2315, 2.9898, 0.51655, 0.6621], "request_retention": 0.9, "interval_clamp_days": [1, 365]}}}).to_string(),
        )
        .expect("rewrite config");
        let err = boot(&paths).expect_err("must fatal");
        assert_eq!(err, "config app section missing");
    }

    #[test]
    fn boot_fatals_on_invalid_memo_mode() {
        let paths = temp_paths("badmode");
        std::fs::write(
            &paths.config_path,
            serde_json::json!({
                "app": {
                    "window": {"width": 640, "height": 480, "title": "t"},
                    "memo": {"cadence_ms": 500, "mode": " Morse "},
                    "drill": {"reps": 3}
                },
                "srs": {"fsrs": {"weights": [0.40255, 1.18385, 3.173, 15.69105, 7.1949, 0.5345, 1.4604, 0.0046, 1.54575, 0.1192, 1.01925, 1.9395, 0.11, 0.29605, 2.2698, 0.2315, 2.9898, 0.51655, 0.6621], "request_retention": 0.9, "interval_clamp_days": [1, 365]}}
            })
            .to_string(),
        )
        .expect("rewrite config");
        let err = boot(&paths).expect_err("must fatal");
        assert!(err.contains("app.memo.mode"), "unexpected: {err}");
    }

    #[test]
    fn default_config_matches_fallbacks() {
        let default = AppConfig::default();
        assert_eq!(default.window.width, 1280);
        assert_eq!(default.window.height, 800);
        assert_eq!(default.window.title, "blind trainer");
        assert_eq!(default.memo.cadence_ms, 800);
        assert_eq!(default.memo.mode, "per-orbit");
        assert_eq!(default.drill.reps, 6);
    }

    #[test]
    fn highlight_kind_covers_both_target_slots() {
        let mut net = NetState::default();
        assert_eq!(
            net.highlight_kind(EDGE_BUFFER_STICKER),
            Some(HighlightKind::Buffer)
        );
        assert_eq!(net.highlight_kind(5), None);
        net.targets = [Some(5), Some(9)];
        assert_eq!(net.highlight_kind(5), Some(HighlightKind::Target));
        assert_eq!(net.highlight_kind(9), Some(HighlightKind::Target));
        assert_eq!(net.highlight_kind(7), None);
    }

    #[test]
    fn display_path_relativizes_only_inside_the_workspace() {
        let inside = paths::workspace_root()
            .join("crates")
            .join("bt-app")
            .join("Cargo.toml");
        let expected = Path::new("crates")
            .join("bt-app")
            .join("Cargo.toml")
            .display()
            .to_string();
        assert_eq!(display_path(&inside), expected);
        let outside = Path::new("/definitely-not-the-workspace/config.json");
        assert_eq!(display_path(outside), outside.display().to_string());
    }

    #[test]
    fn load_config_value_reports_read_and_parse_errors() {
        let valid = temp_paths("loadcfg-valid");
        let value = load_config_value(&valid.config_path).expect("valid config");
        assert_eq!(
            value.pointer("/app/memo/mode").and_then(|v| v.as_str()),
            Some("images")
        );
        let mut missing = temp_paths("loadcfg-missing");
        missing.config_path = missing.config_path.with_extension("absent");
        let err = load_config_value(&missing.config_path).expect_err("read error");
        assert!(err.contains("cannot read config"), "unexpected: {err}");
        let broken = temp_paths("loadcfg-broken");
        std::fs::write(&broken.config_path, b"{not json").expect("rewrite config");
        let err = load_config_value(&broken.config_path).expect_err("parse error");
        assert!(err.contains("cannot parse config"), "unexpected: {err}");
    }

    #[test]
    fn app_config_from_reads_the_section_and_defaults_the_seed() {
        let value = serde_json::json!({
            "app": {
                "window": {"width": 100, "height": 200, "title": "w"},
                "memo": {"cadence_ms": 250, "mode": "syllables"},
                "drill": {"reps": 2}
            }
        });
        let config = app_config_from(&value).expect("parses");
        assert_eq!(config.window.width, 100);
        assert_eq!(config.window.height, 200);
        assert_eq!(config.window.title, "w");
        assert_eq!(config.memo.cadence_ms, 250);
        assert_eq!(config.memo.mode, "syllables");
        assert_eq!(config.drill.reps, 2);
        assert_eq!(config.smoke_seed, DEFAULT_SMOKE_SEED);
        let pinned = serde_json::json!({
            "app": {
                "window": {"width": 1, "height": 2, "title": "w"},
                "memo": {"cadence_ms": 1, "mode": "images"},
                "drill": {"reps": 1},
                "smoke_seed": 99
            }
        });
        assert_eq!(app_config_from(&pinned).expect("parses").smoke_seed, 99);
    }

    #[test]
    fn app_config_from_rejects_malformed_and_missing_sections() {
        let malformed = serde_json::json!({"app": {"window": null}});
        let err = app_config_from(&malformed).expect_err("malformed");
        assert!(
            err.starts_with("config app section malformed"),
            "unexpected: {err}"
        );
        assert_eq!(
            app_config_from(&serde_json::json!({})).expect_err("missing"),
            "config app section missing"
        );
    }

    #[test]
    fn last_save_all_ok_requires_every_store() {
        assert!(!LastSave::default().all_ok());
        let full = LastSave {
            settings_ok: true,
            srs_ok: true,
            solves_ok: true,
            user_algs_ok: true,
        };
        assert!(full.all_ok());
        let no_algs = LastSave {
            settings_ok: true,
            srs_ok: true,
            solves_ok: true,
            user_algs_ok: false,
        };
        assert!(!no_algs.all_ok());
    }

    fn rewrite_config(paths: &RuntimePaths, cadence_ms: u32, reps: u32) {
        std::fs::write(
            &paths.config_path,
            serde_json::json!({
                "app": {
                    "window": {"width": 640, "height": 480, "title": "t"},
                    "memo": {"cadence_ms": cadence_ms, "mode": "images"},
                    "drill": {"reps": reps}
                },
                "srs": {"fsrs": {"weights": [0.40255, 1.18385, 3.173, 15.69105, 7.1949, 0.5345, 1.4604, 0.0046, 1.54575, 0.1192, 1.01925, 1.9395, 0.11, 0.29605, 2.2698, 0.2315, 2.9898, 0.51655, 0.6621], "request_retention": 0.9, "interval_clamp_days": [1, 365]}}
            })
            .to_string(),
        )
        .expect("rewrite config");
    }

    #[test]
    fn boot_clamps_out_of_range_settings() {
        let low = temp_paths("clamp-low");
        rewrite_config(&low, 0, 0);
        let booted = boot(&low).expect("boot");
        assert_eq!(booted.stores.settings.memo_cadence_ms, 1);
        assert_eq!(booted.stores.settings.drill_reps, 1);
        let high = temp_paths("clamp-high");
        rewrite_config(&high, 99_999, 999);
        let booted = boot(&high).expect("boot");
        assert_eq!(booted.stores.settings.memo_cadence_ms, 10_000);
        assert_eq!(booted.stores.settings.drill_reps, 100);
    }

    fn input_world(state: AppState) -> World {
        let mut world = World::new();
        world.insert_resource(State::<AppState>::new(state));
        world.init_resource::<Messages<KeyboardInput>>();
        world.init_resource::<Messages<NavIntent>>();
        world
    }

    fn key_input(key_code: KeyCode, logical: Key, pressed: bool) -> KeyboardInput {
        KeyboardInput {
            key_code,
            logical_key: logical,
            state: if pressed {
                ButtonState::Pressed
            } else {
                ButtonState::Released
            },
            text: None,
            repeat: false,
            window: Entity::PLACEHOLDER,
        }
    }

    fn nav_intents(world: &mut World) -> Vec<NavIntent> {
        world
            .resource_mut::<Messages<NavIntent>>()
            .drain()
            .collect()
    }

    fn next_pending(world: &World) -> Option<AppState> {
        match world.resource::<NextState<AppState>>() {
            NextState::Unchanged => None,
            NextState::Pending(state) | NextState::PendingIfDifferent(state) => Some(*state),
        }
    }

    fn system_schedule<M>(systems: impl IntoScheduleConfigs<ScheduleSystem, M>) -> Schedule {
        let mut schedule = Schedule::default();
        schedule.add_systems(systems);
        schedule
    }

    #[test]
    fn menu_keys_open_their_views_only_from_menu() {
        let mut world = input_world(AppState::Menu);
        let mut schedule = system_schedule(keyboard_intents);
        for (key, target) in AppState::MENU_KEYS {
            world
                .resource_mut::<Messages<KeyboardInput>>()
                .write(key_input(key, Key::Character("1".into()), true));
            schedule.run(&mut world);
            let intents = nav_intents(&mut world);
            assert!(
                matches!(intents.as_slice(), [NavIntent::Open(open)] if *open == target),
                "key {key:?} should open {target:?}, got {intents:?}"
            );
        }
        let mut other = input_world(AppState::Drill);
        other
            .resource_mut::<Messages<KeyboardInput>>()
            .write(key_input(KeyCode::Digit1, Key::Character("1".into()), true));
        system_schedule(keyboard_intents).run(&mut other);
        assert!(nav_intents(&mut other).is_empty());
    }

    #[test]
    fn escape_leaves_a_view_but_released_keys_do_nothing() {
        let mut world = input_world(AppState::Drill);
        let mut schedule = system_schedule(keyboard_intents);
        world
            .resource_mut::<Messages<KeyboardInput>>()
            .write(key_input(KeyCode::Escape, Key::Escape, true));
        schedule.run(&mut world);
        assert!(matches!(
            nav_intents(&mut world).as_slice(),
            [NavIntent::ToMenu]
        ));
        world
            .resource_mut::<Messages<KeyboardInput>>()
            .write(key_input(
                KeyCode::Digit1,
                Key::Character("1".into()),
                false,
            ));
        schedule.run(&mut world);
        assert!(nav_intents(&mut world).is_empty());
    }

    #[test]
    fn bootstrap_and_fatal_swallow_buffered_keys() {
        let mut world = input_world(AppState::Bootstrap);
        let mut schedule = system_schedule(keyboard_intents);
        world
            .resource_mut::<Messages<KeyboardInput>>()
            .write(key_input(KeyCode::Escape, Key::Escape, true));
        world
            .resource_mut::<Messages<KeyboardInput>>()
            .write(key_input(KeyCode::Digit2, Key::Character("2".into()), true));
        schedule.run(&mut world);
        assert!(nav_intents(&mut world).is_empty());
        world.insert_resource(State::<AppState>::new(AppState::Menu));
        schedule.run(&mut world);
        assert!(
            nav_intents(&mut world).is_empty(),
            "keys buffered during bootstrap must not fire later"
        );
    }

    #[test]
    fn escape_while_typing_clears_focus_instead_of_navigating() {
        let mut world = input_world(AppState::Algs);
        let editor = world.spawn(bevy::text::EditableText::new("query")).id();
        world.insert_resource(InputFocus::from_entity(editor));
        world
            .resource_mut::<Messages<KeyboardInput>>()
            .write(key_input(KeyCode::Escape, Key::Escape, true));
        system_schedule(keyboard_intents).run(&mut world);
        assert!(nav_intents(&mut world).is_empty());
        assert_eq!(world.resource::<InputFocus>().get(), None);
    }

    #[test]
    fn nav_to_menu_is_skipped_when_already_there() {
        let mut world = input_world(AppState::Menu);
        world.insert_resource(NextState::<AppState>::default());
        world
            .resource_mut::<Messages<NavIntent>>()
            .write(NavIntent::ToMenu);
        system_schedule(apply_nav_intent).run(&mut world);
        assert_eq!(next_pending(&world), None);
        let mut moving = input_world(AppState::Drill);
        moving.insert_resource(NextState::<AppState>::default());
        moving
            .resource_mut::<Messages<NavIntent>>()
            .write(NavIntent::ToMenu);
        system_schedule(apply_nav_intent).run(&mut moving);
        assert_eq!(next_pending(&moving), Some(AppState::Menu));
        let mut opening = input_world(AppState::Menu);
        opening.insert_resource(NextState::<AppState>::default());
        opening
            .resource_mut::<Messages<NavIntent>>()
            .write(NavIntent::Open(AppState::Stats));
        system_schedule(apply_nav_intent).run(&mut opening);
        assert_eq!(next_pending(&opening), Some(AppState::Stats));
    }

    fn sample_stores() -> Stores {
        Stores {
            settings: Settings {
                memo_cadence_ms: 300,
                memo_mode: MemoMode::Syllables,
                drill_reps: 5,
            },
            srs: persist::SrsStore::default(),
            solves: persist::SolveLog::default(),
            user_algs: paths::UserAlgs::new(),
        }
    }

    #[test]
    fn settings_change_persists_to_the_user_root() {
        let paths = temp_paths("apply-settings");
        let mut world = input_world(AppState::Menu);
        world.insert_resource(PersistenceRoot(paths.user_root.clone()));
        world.insert_resource(sample_stores());
        world.insert_resource(LastSave::default());
        world.init_resource::<Messages<SettingsChanged>>();
        let expected = Settings {
            memo_cadence_ms: 640,
            memo_mode: MemoMode::PerOrbit,
            drill_reps: 6,
        };
        world
            .resource_mut::<Messages<SettingsChanged>>()
            .write(SettingsChanged(expected));
        system_schedule(apply_settings_change).run(&mut world);
        assert_eq!(world.resource::<Stores>().settings, expected);
        assert!(world.resource::<LastSave>().settings_ok);
        let reloaded = Stores::load(
            &paths.user_root,
            sample_settings_default(),
            &FsrsConfig::defaults(),
        )
        .expect("reload");
        assert_eq!(reloaded.settings, expected);
    }
    fn sample_settings_default() -> Settings {
        Settings {
            memo_cadence_ms: 1,
            memo_mode: MemoMode::Images,
            drill_reps: 1,
        }
    }
    #[test]
    fn save_stores_request_persists_every_store() {
        let paths = temp_paths("save-stores");
        let mut stores = sample_stores();
        stores.srs.cards.insert(
            "edge/UF".to_string(),
            Card {
                memory: None,
                interval_days: 3,
                due_day: 4,
                reps: 1,
                lapses: 0,
                reviews: vec![],
            },
        );
        stores.solves.solves.push(persist::SolveRecord {
            started_ms: 9,
            memo_ms: 1000,
            exec_ms: 2000,
            success: false,
        });
        let mut world = input_world(AppState::Menu);
        world.insert_resource(PersistenceRoot(paths.user_root.clone()));
        world.insert_resource(stores);
        world.insert_resource(LastSave::default());
        world.init_resource::<Messages<SaveStores>>();
        let mut schedule = system_schedule(save_stores_on_request);
        schedule.run(&mut world);
        assert_eq!(*world.resource::<LastSave>(), LastSave::default());
        world
            .resource_mut::<Messages<SaveStores>>()
            .write(SaveStores);
        schedule.run(&mut world);
        let last = *world.resource::<LastSave>();
        assert!(last.all_ok());
        let reloaded = Stores::load(
            &paths.user_root,
            sample_settings_default(),
            &FsrsConfig::defaults(),
        )
        .expect("reload");
        assert_eq!(reloaded.srs.cards.len(), 1);
        assert_eq!(reloaded.solves.solves.len(), 1);
        assert_eq!(reloaded.settings.drill_reps, 5);
    }
}
