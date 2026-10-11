use bevy::app::App;
use bevy::ecs::message::Messages;
use bevy::input::ButtonState;
use bevy::input::keyboard::Key;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
pub use bt_app::logic::RuntimePaths;
use bt_app::logic::{AppLogicPlugin, NavIntent};
use bt_app::states::AppState;
use bt_app::views::ViewsLogicPlugin;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Mutex, OnceLock};
use std::time::Duration;

pub const COVERED_ALG: &str = "R2 U' S R2 S' R2 U R2";
pub const COVERED_POSITION: &str = "UF-UB-UR";

static DIR_COUNTER: AtomicU64 = AtomicU64::new(0);

fn lock() -> &'static Mutex<()> {
    static LOCK: OnceLock<Mutex<()>> = OnceLock::new();
    LOCK.get_or_init(|| Mutex::new(()))
}

pub struct Fixture {
    pub paths: RuntimePaths,
    #[allow(dead_code)]
    pub base: PathBuf,
}

impl Fixture {
    #[allow(dead_code)]
    pub fn user_root(&self) -> &Path {
        &self.paths.user_root
    }
}

pub fn fixture(tag: &str) -> Fixture {
    let base = std::env::temp_dir()
        .join("bt-app-views-tests")
        .join(format!(
            "{tag}-{}-{}",
            std::process::id(),
            DIR_COUNTER.fetch_add(1, Ordering::Relaxed)
        ));
    std::fs::create_dir_all(&base).expect("fixture dirs");
    let data_ga = base.join("data-ga");
    std::fs::create_dir_all(&data_ga).expect("data-ga dir");
    std::fs::create_dir_all(base.join("vendor").join("cherry")).expect("cherry dir");
    std::fs::create_dir_all(base.join("vendor").join("eggins")).expect("eggins dir");
    write_config(&base, &data_ga);
    for solver in ["cherry", "eggins"] {
        for sheet in ["edge", "corner", "flips", "twists", "parity", "ltct"] {
            let path = base
                .join("vendor")
                .join(solver)
                .join(format!("{sheet}.tsv"));
            std::fs::write(&path, sheet_header()).expect("sheet file");
        }
    }
    add_edge_row(&base, COVERED_POSITION, COVERED_ALG);
    Fixture {
        paths: RuntimePaths {
            config_path: base.join("config").join("config.json"),
            vendor_root: base.join("vendor"),
            user_root: base.join("user"),
            assets_root: base.join("assets"),
        },
        base,
    }
}

fn write_config(base: &Path, data_ga: &Path) {
    let dir = base.join("config");
    std::fs::create_dir_all(&dir).expect("config dir");
    std::fs::write(
        dir.join("config.json"),
        serde_json::json!({
            "app": {
                "window": {"width": 640, "height": 480, "title": "views"},
                "memo": {"cadence_ms": 800, "mode": "syllables"},
                "drill": {"reps": 4}
            },
            "srs": {"sm2": {"again": 2, "hard": 3, "good": 4, "easy": 5, "interval_clamp_days": [1, 365]}},
            "paths": {"data_ga": data_ga.to_string_lossy()}
        })
        .to_string(),
    )
    .expect("write config");
}

fn sheet_header() -> String {
    "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n".to_string()
}

fn add_edge_row(base: &Path, position: &str, alg: &str) {
    let path = base.join("vendor").join("cherry").join("edge.tsv");
    let mut text = std::fs::read_to_string(&path).expect("edge sheet");
    text.push_str(&format!(
        "{position}\t{alg}\t\t\thttps://example.test/recon\n"
    ));
    std::fs::write(&path, text).expect("append edge row");
}

#[derive(Resource, Default)]
#[allow(dead_code)]
pub struct TestClock {
    pub ms: u64,
}

fn drive_clock(clock: ResMut<TestClock>, mut time: ResMut<Time<Virtual>>) {
    time.advance_by(Duration::from_millis(clock.ms));
}

pub fn headless_app(paths: RuntimePaths) -> App {
    let mut app = App::new();
    app.add_plugins(MinimalPlugins)
        .add_plugins(AppLogicPlugin::with_paths(paths))
        .add_plugins(ViewsLogicPlugin)
        .init_resource::<TestClock>()
        .add_systems(PreUpdate, drive_clock);
    app
}

pub fn boot_to_menu(app: &mut App) {
    for _ in 0..8 {
        app.update();
    }
    assert_eq!(current_state(app), AppState::Menu);
}

#[allow(dead_code)]
pub fn step(app: &mut App, ms: u64) {
    app.world_mut().resource_mut::<TestClock>().ms = ms;
    app.update();
}

pub fn pump(app: &mut App, frames: usize) {
    app.world_mut().resource_mut::<TestClock>().ms = 0;
    for _ in 0..frames {
        app.update();
    }
}

pub fn press(app: &mut App, key: KeyCode) {
    app.world_mut()
        .resource_mut::<Messages<KeyboardInput>>()
        .write(KeyboardInput {
            key_code: key,
            logical_key: Key::Character("x".into()),
            state: ButtonState::Pressed,
            text: None,
            repeat: false,
            window: Entity::PLACEHOLDER,
        });
}

pub fn open(app: &mut App, target: AppState) {
    app.world_mut()
        .resource_mut::<Messages<NavIntent>>()
        .write(NavIntent::Open(target));
    pump(app, 3);
}

pub fn current_state(app: &App) -> AppState {
    *app.world().resource::<State<AppState>>().get()
}

pub fn guard() -> std::sync::MutexGuard<'static, ()> {
    lock().lock().expect("test lock")
}
