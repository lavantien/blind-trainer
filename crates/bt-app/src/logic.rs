use crate::persist::{self, MemoMode, PersistenceRoot, Settings, Stores};
use crate::states::{AppState, DrillPhase, MemoPhase, SolvePhase};
use bevy::input::ButtonState;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
use bevy::state::app::StatesPlugin;
use bt_core::Cube;
use bt_data::corpus::Corpus;
use bt_data::paths;
use bt_data::srs::Sm2Config;
use bt_scheme::EDGE_BUFFER_STICKER;
use serde::Deserialize;
use std::path::{Path, PathBuf};

const DEFAULT_WINDOW_WIDTH: u32 = 1280;
const DEFAULT_WINDOW_HEIGHT: u32 = 800;
const DEFAULT_WINDOW_TITLE: &str = "blind trainer";
const DEFAULT_MEMO_CADENCE_MS: u32 = 800;
const DEFAULT_MEMO_MODE: &str = "syllables";
const DEFAULT_DRILL_REPS: u32 = 5;
const DEFAULT_REF_GROUND: &str = "ref_ground";

#[derive(Clone, Debug, Resource)]
pub struct RuntimePaths {
    pub config_path: PathBuf,
    pub vendor_root: PathBuf,
    pub user_root: PathBuf,
}

impl RuntimePaths {
    pub fn from_env() -> RuntimePaths {
        let config_path = paths::config_path();
        let vendor_root = paths::vendor_root()
            .unwrap_or_else(|_| paths::workspace_root().join(DEFAULT_REF_GROUND));
        let user_root = paths::user_root();
        RuntimePaths {
            config_path,
            vendor_root,
            user_root,
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
        }
    }
}

pub fn load_config_value(path: &Path) -> Result<serde_json::Value, String> {
    let text = std::fs::read_to_string(path)
        .map_err(|err| format!("cannot read config {}: {err}", path.display()))?;
    serde_json::from_str(&text)
        .map_err(|err| format!("cannot parse config {}: {err}", path.display()))
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
pub struct Sm2(pub Sm2Config);

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
    pub target_sticker: Option<u8>,
}

impl Default for NetState {
    fn default() -> NetState {
        NetState {
            cube: Cube::solved(),
            buffer_sticker: Some(EDGE_BUFFER_STICKER),
            target_sticker: None,
        }
    }
}

impl NetState {
    pub fn highlight_kind(&self, sticker: u8) -> Option<HighlightKind> {
        if self.buffer_sticker == Some(sticker) {
            Some(HighlightKind::Buffer)
        } else if self.target_sticker == Some(sticker) {
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
    sm2: Sm2Config,
    corpus: Corpus,
    stores: Stores,
    warnings: Vec<String>,
}

fn default_settings(config: &AppConfig) -> Result<Settings, String> {
    let memo_mode = MemoMode::parse(&config.memo.mode)
        .ok_or_else(|| format!("config app.memo.mode invalid: {}", config.memo.mode))?;
    Ok(Settings {
        memo_cadence_ms: config.memo.cadence_ms,
        memo_mode,
        drill_reps: config.drill.reps,
    })
}

fn boot(paths: &RuntimePaths) -> Result<Booted, String> {
    let value = load_config_value(&paths.config_path)?;
    let config = app_config_from(&value)?;
    let sm2 = Sm2Config::from_value(&value).map_err(|err| format!("config srs section: {err}"))?;
    let stores = Stores::load(&paths.user_root, default_settings(&config)?)?;
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
        sm2,
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
            world.insert_resource(Sm2(booted.sm2));
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
        app.add_plugins(StatesPlugin)
            .insert_resource(paths)
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
                "srs": {"sm2": {"again": 2, "hard": 3, "good": 4, "easy": 5, "interval_clamp_days": [1, 365]}}
            })
            .to_string(),
        )
        .expect("write config");
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
            serde_json::json!({"srs": {"sm2": {"again": 2, "hard": 3, "good": 4, "easy": 5, "interval_clamp_days": [1, 365]}}}).to_string(),
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
                "srs": {"sm2": {"again": 2, "hard": 3, "good": 4, "easy": 5, "interval_clamp_days": [1, 365]}}
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
        assert_eq!(default.memo.mode, "syllables");
        assert_eq!(default.drill.reps, 5);
    }
}
