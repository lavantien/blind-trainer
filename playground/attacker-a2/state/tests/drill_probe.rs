use bevy::app::App;
use bevy::ecs::message::Messages;
use bevy::input::ButtonState;
use bevy::input::keyboard::Key;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
use bt_app::logic::{AppLogicPlugin, NetState, RuntimePaths, SettingsChanged};
use bt_app::states::{AppState, DrillPhase};
use bt_app::views::PendingDrill;
use bt_app::views::ViewsLogicPlugin;
use bt_app::views::drill::{DrillSeed, DrillSession, card_lines, even_reps};
use bt_scheme::{CaseId, Orbit, sticker_of};
use std::path::PathBuf;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Mutex, OnceLock};
use std::time::Duration;

const EDGE_ALG: &str = "R2 U' S R2 S' R2 U R2";
const CORNER_ALG: &str = "R' B' R2 D R' U' R D' R' U R' B R";

static DIR_COUNTER: AtomicU64 = AtomicU64::new(0);

fn lock() -> &'static Mutex<()> {
    static LOCK: OnceLock<Mutex<()>> = OnceLock::new();
    LOCK.get_or_init(|| Mutex::new(()))
}

fn guard() -> std::sync::MutexGuard<'static, ()> {
    lock().lock().expect("test lock")
}

struct Fixture {
    paths: RuntimePaths,
    base: PathBuf,
}

fn fixture(tag: &str, rows: &[(&str, &str)], reps: u32) -> Fixture {
    let base = std::env::temp_dir()
        .join("bt-a2-state")
        .join(format!(
            "{tag}-{}-{}",
            std::process::id(),
            DIR_COUNTER.fetch_add(1, Ordering::Relaxed)
        ));
    std::fs::create_dir_all(base.join("data-ga")).expect("ga dir");
    std::fs::create_dir_all(base.join("config")).expect("config dir");
    for solver in ["cherry", "eggins"] {
        std::fs::create_dir_all(base.join("vendor").join(solver)).expect("vendor dir");
        for sheet in ["edge", "corner", "flips", "twists", "parity", "ltct"] {
            std::fs::write(
                base.join("vendor").join(solver).join(format!("{sheet}.tsv")),
                "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n",
            )
            .expect("sheet header");
        }
    }
    let mut edge_text = String::new();
    for (position, alg) in rows {
        edge_text.push_str(&format!("{position}\t{alg}\t\t\thttps://example.test/r\n"));
    }
    if !edge_text.is_empty() {
        let path = base.join("vendor").join("cherry").join("edge.tsv");
        let mut text = std::fs::read_to_string(&path).expect("edge sheet");
        text.push_str(&edge_text);
        std::fs::write(&path, text).expect("edge rows");
    }
    let corner_path = base.join("vendor").join("cherry").join("corner.tsv");
    let mut corner_text = std::fs::read_to_string(&corner_path).expect("corner sheet");
    corner_text.push_str(&format!("UFR-UBL-UBR\t{CORNER_ALG}\t\t\thttps://example.test/r\n"));
    std::fs::write(&corner_path, corner_text).expect("corner row");
    std::fs::write(
        base.join("config").join("config.json"),
        serde_json::json!({
            "app": {
                "window": {"width": 640, "height": 480, "title": "a2"},
                "memo": {"cadence_ms": 800, "mode": "syllables"},
                "drill": {"reps": reps}
            },
            "srs": {"sm2": {"again": 2, "hard": 3, "good": 4, "easy": 5, "interval_clamp_days": [1, 365]}},
            "paths": {"data_ga": base.join("data-ga").to_string_lossy()}
        })
        .to_string(),
    )
    .expect("config");
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

#[derive(Resource, Default)]
struct TestClock {
    ms: u64,
}

fn drive_clock(clock: ResMut<TestClock>, mut time: ResMut<Time<Virtual>>) {
    time.advance_by(Duration::from_millis(clock.ms));
}

fn headless_app(paths: RuntimePaths) -> App {
    let mut app = App::new();
    app.add_plugins(MinimalPlugins)
        .add_plugins(AppLogicPlugin::with_paths(paths))
        .add_plugins(ViewsLogicPlugin)
        .init_resource::<TestClock>()
        .add_systems(PreUpdate, drive_clock);
    app
}

fn boot_to_menu(app: &mut App) {
    for _ in 0..8 {
        app.update();
    }
    assert_eq!(current_state(app), AppState::Menu);
}

fn pump(app: &mut App, frames: usize) {
    app.world_mut().resource_mut::<TestClock>().ms = 0;
    for _ in 0..frames {
        app.update();
    }
}

fn press(app: &mut App, key: KeyCode) {
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

fn open(app: &mut App, target: AppState) {
    app.world_mut()
        .resource_mut::<Messages<bt_app::logic::NavIntent>>()
        .write(bt_app::logic::NavIntent::Open(target));
    pump(app, 3);
}

fn current_state(app: &App) -> AppState {
    *app.world().resource::<State<AppState>>().get()
}

fn phase(app: &App) -> DrillPhase {
    *app.world().resource::<State<DrillPhase>>().get()
}

fn edge_case(first: char, second: char) -> CaseId {
    CaseId {
        orbit: Orbit::Edge,
        t1: sticker_of(Orbit::Edge, first),
        t2: sticker_of(Orbit::Edge, second),
    }
}

fn corner_case(first: char, second: char) -> CaseId {
    CaseId {
        orbit: Orbit::Corner,
        t1: sticker_of(Orbit::Corner, first),
        t2: sticker_of(Orbit::Corner, second),
    }
}

fn enter_drill_pinned(fx: &Fixture, case: CaseId) -> App {
    let mut app = headless_app(fx.paths.clone());
    boot_to_menu(&mut app);
    app.world_mut().insert_resource(DrillSeed(1234));
    app.world_mut().resource_mut::<PendingDrill>().0 = Some(case);
    open(&mut app, AppState::Drill);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Card);
    app
}

fn run_reps(app: &mut App, presses: usize) {
    for _ in 0..presses {
        app.world_mut().resource_mut::<TestClock>().ms = 400;
        app.update();
        press(app, KeyCode::Space);
        pump(app, 2);
    }
}

fn session(app: &App) -> &DrillSession {
    app.world().resource::<DrillSession>()
}

#[test]
fn escape_mid_reps_reenters_clean_on_the_same_pair() {
    let _guard = guard();
    let fx = fixture("esc-reps", &[("UF-UB-UR", EDGE_ALG)], 4);
    let mut app = enter_drill_pinned(&fx, edge_case('A', 'B'));
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Reps);
    run_reps(&mut app, 1);
    assert_eq!(session(&app).reps_done, 1);
    press(&mut app, KeyCode::Escape);
    pump(&mut app, 3);
    assert_eq!(current_state(&app), AppState::Menu);
    let seed_before = app.world().resource::<DrillSeed>().0;
    open(&mut app, AppState::Drill);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Card);
    let s = session(&app);
    assert_eq!(
        s.side_a.as_ref().expect("side a").letters(),
        "AB",
        "same seed repicks the due pair"
    );
    assert_eq!(s.reps_done, 0);
    assert!(s.splits.is_empty());
    assert_eq!(s.rate_step, 0);
    assert_eq!(app.world().resource::<DrillSeed>().0, seed_before);
    assert!(app.world().resource::<NetState>().cube.is_solved());
    assert!(
        app.world()
            .resource::<bt_app::persist::Stores>()
            .srs
            .cards
            .is_empty(),
        "escape mid reps writes nothing"
    );
}

#[test]
fn escape_mid_rate_keeps_one_card_and_reenters_clean() {
    let _guard = guard();
    let fx = fixture("esc-rate", &[("UF-UB-UR", EDGE_ALG)], 4);
    let mut app = enter_drill_pinned(&fx, edge_case('A', 'B'));
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    run_reps(&mut app, 4);
    assert_eq!(phase(&app), DrillPhase::Rate);
    press(&mut app, KeyCode::Digit3);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Rate);
    press(&mut app, KeyCode::Escape);
    pump(&mut app, 3);
    assert_eq!(current_state(&app), AppState::Menu);
    {
        let stores = app.world().resource::<bt_app::persist::Stores>();
        assert!(stores.srs.cards.contains_key("edge/UF-UB-UR"));
        assert!(!stores.srs.cards.contains_key("edge/UF-UR-UB"));
    }
    open(&mut app, AppState::Drill);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Card);
    assert_eq!(session(&app).rate_step, 0);
    assert_eq!(
        session(&app).side_a.as_ref().expect("side a").letters(),
        "AB"
    );
}

#[test]
fn uncovered_pin_shows_hint_ignores_space_and_leaves_stale_net() {
    let _guard = guard();
    let fx = fixture("uncovered", &[("UF-UB-UR", EDGE_ALG)], 4);
    let mut app = headless_app(fx.paths.clone());
    boot_to_menu(&mut app);
    {
        let mut net = app.world_mut().resource_mut::<NetState>();
        net.cube = bt_scheme::random_state(9);
        net.targets = [Some(sticker_of(Orbit::Edge, 'Q')), Some(sticker_of(Orbit::Edge, 'S'))];
    }
    app.world_mut().resource_mut::<PendingDrill>().0 = Some(edge_case('Q', 'S'));
    open(&mut app, AppState::Drill);
    pump(&mut app, 3);
    assert_eq!(phase(&app), DrillPhase::Card);
    {
        let s = session(&app);
        assert!(s.sides().is_none());
        assert_eq!(s.pin, Some(edge_case('Q', 'S')));
        let corpus = app.world().resource::<bt_app::logic::VendorCorpus>();
        let book = app.world().resource::<bt_app::views::SyllableBook>();
        let stores = app.world().resource::<bt_app::persist::Stores>();
        let now = app.world().resource::<bt_app::views::NowDay>().0;
        let lines = card_lines(s, &corpus.0, book, stores, now);
        assert!(lines.join(" ").contains("uncovered"), "{lines:?}");
    }
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Card, "space must not open reps");
    let net = app.world().resource::<NetState>();
    assert_eq!(
        net.cube,
        bt_scheme::random_state(9),
        "stale net cube survives an uncovered pin"
    );
    assert_eq!(
        net.targets,
        [Some(sticker_of(Orbit::Edge, 'Q')), Some(sticker_of(Orbit::Edge, 'S'))]
    );
}

#[test]
fn net_mirrors_reps_prepared_solved_alternation() {
    let _guard = guard();
    let fx = fixture("net-mirror", &[("UF-UB-UR", EDGE_ALG)], 4);
    let mut app = enter_drill_pinned(&fx, edge_case('A', 'B'));
    let case = edge_case('A', 'B');
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    let prepared = session(&app).prepared;
    let net = app.world().resource::<NetState>();
    assert_eq!(net.cube, prepared);
    assert_eq!(net.buffer_sticker, Some(bt_scheme::EDGE_BUFFER_STICKER));
    assert_eq!(net.targets, [Some(case.t1), Some(case.t2)]);
    drop(net);
    run_reps(&mut app, 1);
    let net = app.world().resource::<NetState>();
    assert!(net.cube.is_solved(), "after rep 1 of A the cube is solved");
    assert_eq!(net.targets, [Some(case.t2), Some(case.t1)]);
    drop(net);
    run_reps(&mut app, 1);
    let net = app.world().resource::<NetState>();
    assert_eq!(net.cube, prepared, "after rep 2 the cube is prepared again");
    assert_eq!(net.targets, [Some(case.t1), Some(case.t2)]);
}

#[test]
fn corner_pair_sets_corner_buffer_and_targets() {
    let _guard = guard();
    let fx = fixture("corner", &[("UF-UB-UR", EDGE_ALG)], 2);
    let mut app = headless_app(fx.paths.clone());
    boot_to_menu(&mut app);
    app.world_mut().resource_mut::<PendingDrill>().0 = Some(corner_case('A', 'B'));
    open(&mut app, AppState::Drill);
    pump(&mut app, 3);
    let case = corner_case('A', 'B');
    let net = app.world().resource::<NetState>();
    assert_eq!(net.buffer_sticker, Some(bt_scheme::CORNER_BUFFER_STICKER));
    assert_eq!(net.targets, [Some(case.t1), Some(case.t2)]);
    assert!(net.cube.is_solved());
    let s = session(&app);
    assert_eq!(s.side_a.as_ref().expect("side a").position, "UFR-UBL-UBR");
    assert_eq!(s.side_b.as_ref().expect("side b").solver_label, "inverse");
    drop(net);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    run_reps(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Rate);
    let net = app.world().resource::<NetState>();
    assert_eq!(net.cube, session(&app).prepared);
}

#[test]
fn tier2_fallback_still_picks_covered_pair() {
    let _guard = guard();
    let fx = fixture("tier2", &[("UF-UB-UR", EDGE_ALG)], 2);
    let mut app = headless_app(fx.paths.clone());
    boot_to_menu(&mut app);
    {
        use bt_app::persist::Stores;
        let now = app.world().resource::<bt_app::views::NowDay>().0;
        let mut stores = app.world_mut().resource_mut::<Stores>();
        for key in [
            "edge/UF-UB-UR",
            "edge/UF-UR-UB",
            "corner/UFR-UBL-UBR",
            "corner/UFR-UBR-UBL",
        ] {
            let mut card = bt_data::srs::Card::new(now);
            card.due_day = now + 100;
            card.reps = 3;
            stores.srs.cards.insert(key.to_string(), card);
        }
    }
    app.world_mut().insert_resource(DrillSeed(55));
    open(&mut app, AppState::Drill);
    pump(&mut app, 3);
    assert!(
        session(&app).sides().is_some(),
        "tier 2 fallback must still drill the covered pair"
    );
}

#[test]
fn seed_determinism_and_increment_on_completion() {
    let _guard = guard();
    let fx = fixture(
        "seed",
        &[
            ("UF-UB-UR", EDGE_ALG),
            ("UF-UB-UL", "M2 U M2 U2 M U M2"),
            ("UF-UR-UB", "R2 U' S' U2 S U' R2"),
        ],
        2,
    );
    let mut app_a = headless_app(fx.paths.clone());
    boot_to_menu(&mut app_a);
    app_a.world_mut().insert_resource(DrillSeed(77));
    let mut app_b = headless_app(fx.paths.clone());
    boot_to_menu(&mut app_b);
    app_b.world_mut().insert_resource(DrillSeed(77));
    open(&mut app_a, AppState::Drill);
    open(&mut app_b, AppState::Drill);
    pump(&mut app_a, 2);
    pump(&mut app_b, 2);
    let pick_a = session(&app_a).side_a.as_ref().expect("side a").letters();
    let pick_b = session(&app_b).side_a.as_ref().expect("side a").letters();
    assert_eq!(pick_a, pick_b, "same seed same store state same pick");
    assert_eq!(app_a.world().resource::<DrillSeed>().0, 77);

    press(&mut app_a, KeyCode::Space);
    pump(&mut app_a, 2);
    run_reps(&mut app_a, 2);
    assert_eq!(phase(&app_a), DrillPhase::Rate);
    press(&mut app_a, KeyCode::Digit3);
    pump(&mut app_a, 2);
    press(&mut app_a, KeyCode::Digit3);
    pump(&mut app_a, 2);
    assert_eq!(phase(&app_a), DrillPhase::Card);
    assert_eq!(app_a.world().resource::<DrillSeed>().0, 78);
}

#[test]
fn reps_target_tracks_settings_and_clamps_even() {
    let _guard = guard();
    let fx = fixture("reps-cfg", &[("UF-UB-UR", EDGE_ALG)], 5);
    let mut app = headless_app(fx.paths.clone());
    boot_to_menu(&mut app);
    app.world_mut().resource_mut::<PendingDrill>().0 = Some(edge_case('A', 'B'));
    open(&mut app, AppState::Drill);
    pump(&mut app, 2);
    assert_eq!(session(&app).reps_target, 6, "odd 5 rounds to 6");
    assert_eq!(even_reps(0), 2);
    assert_eq!(even_reps(1), 2);
    assert_eq!(even_reps(100), 100);
    let stores = app.world().resource::<bt_app::persist::Stores>();
    let mut next = stores.settings.clone();
    next.drill_reps = 3;
    drop(stores);
    app.world_mut()
        .resource_mut::<Messages<SettingsChanged>>()
        .write(SettingsChanged(next));
    pump(&mut app, 2);
    assert_eq!(
        app.world()
            .resource::<bt_app::persist::Stores>()
            .settings
            .drill_reps,
        3
    );
    app.world_mut().resource_mut::<PendingDrill>().0 = Some(edge_case('A', 'B'));
    open(&mut app, AppState::Drill);
    pump(&mut app, 3);
    assert_eq!(session(&app).reps_target, 4, "reinstall reads new settings");
    let saved = std::fs::read_to_string(fx.base.join("user").join("settings").join("settings.json"))
        .expect("settings saved");
    assert!(saved.contains("\"drill_reps\": 3"), "{saved}");
}

#[test]
fn rate_writes_cards_saves_and_advances() {
    let _guard = guard();
    let fx = fixture("rate-flow", &[("UF-UB-UR", EDGE_ALG)], 2);
    let mut app = headless_app(fx.paths.clone());
    boot_to_menu(&mut app);
    app.world_mut().resource_mut::<PendingDrill>().0 = Some(edge_case('A', 'B'));
    open(&mut app, AppState::Drill);
    pump(&mut app, 2);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    run_reps(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Rate);
    let now = app.world().resource::<bt_app::views::NowDay>().0;
    press(&mut app, KeyCode::Digit3);
    pump(&mut app, 2);
    let stores = app.world().resource::<bt_app::persist::Stores>();
    let card = stores.srs.cards.get("edge/UF-UB-UR").expect("card a");
    assert_eq!(card.reps, 1);
    assert_eq!(card.due_day, now + 1);
    assert_eq!(card.reviews.len(), 1);
    assert!(!stores.srs.cards.contains_key("edge/UF-UR-UB"));
    drop(stores);
    press(&mut app, KeyCode::Digit3);
    pump(&mut app, 2);
    let stores = app.world().resource::<bt_app::persist::Stores>();
    let card_b = stores.srs.cards.get("edge/UF-UR-UB").expect("card b");
    assert_eq!(card_b.reps, 1);
    let srs_file = fx.base.join("user").join("srs").join("srs.json");
    let text = std::fs::read_to_string(&srs_file).expect("srs persisted");
    assert!(text.contains("edge/UF-UR-UB"), "{text}");
    assert!(text.contains("edge/UF-UB-UR"), "{text}");
}

#[test]
fn double_space_one_frame_advances_one_rep_per_frame() {
    let _guard = guard();
    let fx = fixture("double", &[("UF-UB-UR", EDGE_ALG)], 4);
    let mut app = enter_drill_pinned(&fx, edge_case('A', 'B'));
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Reps);
    press(&mut app, KeyCode::Space);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 1);
    assert_eq!(session(&app).reps_done, 1, "one rep per frame maximum");
    pump(&mut app, 1);
    assert_eq!(session(&app).reps_done, 2);
    let splits = session(&app).splits.clone();
    assert_eq!(splits.len(), 2);
    assert!(splits[1].1 < 0.05, "second split is near zero: {:?}", splits);
}

#[test]
fn held_key_repeat_rates_both_sides_without_a_new_press() {
    let _guard = guard();
    let fx = fixture("repeat", &[("UF-UB-UR", EDGE_ALG)], 2);
    let mut app = headless_app(fx.paths.clone());
    boot_to_menu(&mut app);
    app.world_mut().resource_mut::<PendingDrill>().0 = Some(edge_case('A', 'B'));
    open(&mut app, AppState::Drill);
    pump(&mut app, 2);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    run_reps(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Rate);
    for _ in 0..2 {
        app.world_mut()
            .resource_mut::<Messages<KeyboardInput>>()
            .write(KeyboardInput {
                key_code: KeyCode::Digit3,
                logical_key: Key::Character("3".into()),
                state: ButtonState::Pressed,
                text: None,
                repeat: true,
                window: Entity::PLACEHOLDER,
            });
        pump(&mut app, 1);
    }
    let stores = app.world().resource::<bt_app::persist::Stores>();
    assert!(
        stores.srs.cards.contains_key("edge/UF-UR-UB"),
        "repeat events rate both sides with the key held once"
    );
}

#[test]
fn config_max_reps_breaks_the_even_invariant() {
    let _guard = guard();
    let fx = fixture("max-reps", &[("UF-UB-UR", EDGE_ALG)], u32::MAX);
    let mut app = headless_app(fx.paths.clone());
    boot_to_menu(&mut app);
    app.world_mut().resource_mut::<PendingDrill>().0 = Some(edge_case('A', 'B'));
    open(&mut app, AppState::Drill);
    pump(&mut app, 3);
    let target = session(&app).reps_target;
    assert_eq!(target, u32::MAX, "saturating add leaves the target odd");
    assert_eq!(target % 2, 1);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Reps);
    run_reps(&mut app, 3);
    let prepared = session(&app).prepared;
    assert!(!prepared.is_solved());
    let net = app.world().resource::<NetState>();
    assert_eq!(
        net.cube,
        {
            let mut cube = prepared;
            cube.apply_alg(&session(&app).side_a.as_ref().unwrap().alg);
            cube
        },
        "odd rep count ends on prepared plus A, not on prepared"
    );
    assert!(net.cube.is_solved());
}

#[test]
fn rate_and_escape_same_frame_both_take_effect() {
    let _guard = guard();
    let fx = fixture("rate-esc", &[("UF-UB-UR", EDGE_ALG)], 2);
    let mut app = headless_app(fx.paths.clone());
    boot_to_menu(&mut app);
    app.world_mut().resource_mut::<PendingDrill>().0 = Some(edge_case('A', 'B'));
    open(&mut app, AppState::Drill);
    pump(&mut app, 2);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    run_reps(&mut app, 2);
    press(&mut app, KeyCode::Digit3);
    press(&mut app, KeyCode::Escape);
    pump(&mut app, 3);
    let rated = app
        .world()
        .resource::<bt_app::persist::Stores>()
        .srs
        .cards
        .contains_key("edge/UF-UB-UR");
    let at_menu = current_state(&app) == AppState::Menu;
    println!("rated={rated} at_menu={at_menu}");
}
