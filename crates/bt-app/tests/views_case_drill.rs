mod views_common;

use bevy::prelude::*;
use bt_app::logic::SaveStores;
use bt_app::states::{AppState, CaseDrillPhase};
use bt_app::views::PendingDrill;
use bt_app::views::algs::browse::{RepoBrowser, RepoKind};
use bt_app::views::case_drill::{CaseCardStore, CaseDrillSession, PendingCaseDrill};
use bt_data::db::Db;
use bt_data::repo_cards;
use bt_data::repo_user::CaseRef;
use views_common::*;

const PACK_FRONT_JSON: &str = "{}";
const PACK_SOURCE_URL: &str = "https://example.test/pack";
const PACK_SOURCE_AT: &str = "2026-01-01";

fn temp_paths(fixture: &Fixture) -> RuntimePaths {
    RuntimePaths {
        config_path: fixture.paths.config_path.clone(),
        vendor_root: fixture.paths.vendor_root.clone(),
        packs_root: fixture.paths.packs_root.clone(),
        user_root: fixture.paths.user_root.clone(),
        assets_root: fixture.paths.assets_root.clone(),
    }
}

fn registry_text(entries: &[(&str, &str, &str, &str, usize)]) -> String {
    let body: Vec<String> = entries
        .iter()
        .map(|(file, event, method, stage, rows)| {
            format!(
                "{{\"file\":\"{file}\",\"event\":\"{event}\",\"method\":\"{method}\",\"stage\":\"{stage}\",\"n\":null,\"expected_rows\":{rows}}}"
            )
        })
        .collect();
    format!("[{}]", body.join(","))
}

fn pack_text(rows: &[(&str, &str)]) -> String {
    let mut text = String::from("case_key\talg\tfront_json\tsource_url\tsource_at\n");
    for (key, alg) in rows {
        text.push_str(&format!(
            "{key}\t{alg}\t{PACK_FRONT_JSON}\t{PACK_SOURCE_URL}\t{PACK_SOURCE_AT}\n"
        ));
    }
    text
}

fn drill_fixture(tag: &str) -> Fixture {
    let fixture = bare_fixture(tag);
    let packs = fixture.base.join("vendor").join("packs");
    std::fs::write(
        packs.join("registry.json"),
        registry_text(&[
            ("cll.tsv", "2x2", "cll", "cll", 2),
            ("bld.tsv", "3bld", "3style", "parity", 1),
        ]),
    )
    .expect("registry");
    std::fs::write(
        packs.join("cll.tsv"),
        pack_text(&[("cll.1", "R U R'"), ("cll.2", "F R F'")]),
    )
    .expect("cll pack");
    std::fs::write(packs.join("bld.tsv"), pack_text(&[("par.1", "R U R'")])).expect("bld pack");
    fixture
}

fn case_key(event: &str, method: &str, stage: &str, key: &str) -> CaseRef {
    CaseRef {
        event: event.to_string(),
        method: method.to_string(),
        stage: stage.to_string(),
        case_key: key.to_string(),
    }
}

fn db_cards(fixture: &Fixture) -> repo_cards::CaseCards {
    let db = Db::open_root(&fixture.paths.user_root).expect("user db");
    repo_cards::load(&db, &bt_data::fsrs::FsrsConfig::defaults()).expect("cards")
}

fn repo(app: &App) -> &RepoBrowser {
    app.world().resource::<RepoBrowser>()
}

fn session(app: &App) -> &CaseDrillSession {
    app.world().resource::<CaseDrillSession>()
}

fn phase(app: &App) -> CaseDrillPhase {
    *app.world().resource::<State<CaseDrillPhase>>().get()
}

fn enter_repo(app: &mut App) {
    press(app, KeyCode::ArrowRight);
    pump(app, 2);
    assert!(repo(app).repo_focused);
}

fn walk_to_case(app: &mut App, case_key: &str) {
    enter_repo(app);
    press(app, KeyCode::ArrowDown);
    pump(app, 2);
    for _ in 0..3 {
        press(app, KeyCode::ArrowRight);
        pump(app, 2);
    }
    assert_eq!(repo(app).node().expect("case node").label, case_key);
}

fn walk_to_3bld_case(app: &mut App) {
    enter_repo(app);
    press(app, KeyCode::ArrowDown);
    pump(app, 2);
    press(app, KeyCode::ArrowDown);
    pump(app, 2);
    assert_eq!(repo(app).arms[repo(app).arm].label, "3bld");
    for _ in 0..3 {
        press(app, KeyCode::ArrowRight);
        pump(app, 2);
    }
    assert_eq!(repo(app).node().expect("case node").kind, RepoKind::Case);
}

fn grade_current(app: &mut App, key: KeyCode) {
    press(app, KeyCode::Space);
    pump(app, 2);
    assert_eq!(phase(app), CaseDrillPhase::Grade, "space must reveal");
    press(app, key);
    pump(app, 3);
}

#[test]
fn d_on_a_repo_case_opens_the_case_drill_in_show_phase() {
    let _guard = guard();
    let fixture = drill_fixture("open");
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    walk_to_case(&mut app, "cll.1");
    press(&mut app, KeyCode::KeyD);
    pump(&mut app, 4);
    assert_eq!(current_state(&app), AppState::CaseDrill);
    assert_eq!(phase(&app), CaseDrillPhase::Show);
    let current = session(&app).current.as_ref().expect("current case");
    assert_eq!(current.case, case_key("2x2", "cll", "cll", "cll.1"));
    assert_eq!(current.alg, "R U R'");
    assert_eq!(session(&app).total, 1);
    assert!(session(&app).grid.is_some(), "a parseable 2x2 alg renders");
}

#[test]
fn grading_persists_the_card_and_returns_to_the_browser() {
    let _guard = guard();
    let fixture = drill_fixture("grade");
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    walk_to_case(&mut app, "cll.1");
    press(&mut app, KeyCode::KeyD);
    pump(&mut app, 4);
    assert_eq!(current_state(&app), AppState::CaseDrill);
    grade_current(&mut app, KeyCode::Digit3);
    assert_eq!(
        current_state(&app),
        AppState::Algs,
        "an exhausted queue returns to the browser"
    );
    let key = case_key("2x2", "cll", "cll", "cll.1");
    let card = app
        .world()
        .resource::<CaseCardStore>()
        .0
        .get(&key)
        .expect("graded card in memory");
    assert_eq!(card.reps, 1);
    assert_eq!(card.reviews[0].quality, 4, "good grades write quality 4");
    let stored = db_cards(&fixture);
    assert_eq!(stored.get(&key).map(|card| card.reps), Some(1));
}

#[test]
fn due_sessions_cover_the_event_and_skip_recently_graded_cases() {
    let _guard = guard();
    let fixture = drill_fixture("due");
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    enter_repo(&mut app);
    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    assert_eq!(repo(&app).arms[repo(&app).arm].label, "2x2");
    press(&mut app, KeyCode::KeyD);
    pump(&mut app, 4);
    assert_eq!(current_state(&app), AppState::CaseDrill);
    assert_eq!(
        session(&app).total,
        2,
        "nothing carded yet so all cases drill"
    );
    assert_eq!(
        session(&app)
            .current
            .as_ref()
            .expect("current")
            .case
            .case_key,
        "cll.1"
    );
    grade_current(&mut app, KeyCode::Digit4);
    assert_eq!(phase(&app), CaseDrillPhase::Show, "the queue advances");
    assert_eq!(
        session(&app).current.as_ref().expect("next").case.case_key,
        "cll.2"
    );
    grade_current(&mut app, KeyCode::Digit4);
    assert_eq!(current_state(&app), AppState::Algs);

    press(&mut app, KeyCode::Escape);
    pump(&mut app, 3);
    assert_eq!(current_state(&app), AppState::Menu);
    open(&mut app, AppState::Algs);
    enter_repo(&mut app);
    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    press(&mut app, KeyCode::KeyD);
    pump(&mut app, 4);
    assert_eq!(
        session(&app).total,
        2,
        "with nothing due the fallback drills every case again"
    );
}

#[test]
fn a_stores_save_never_wipes_the_case_cards() {
    let _guard = guard();
    let fixture = drill_fixture("persist");
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    walk_to_case(&mut app, "cll.1");
    press(&mut app, KeyCode::KeyD);
    pump(&mut app, 4);
    grade_current(&mut app, KeyCode::Digit3);
    assert_eq!(db_cards(&fixture).len(), 1);
    app.world_mut()
        .resource_mut::<Messages<SaveStores>>()
        .write(SaveStores);
    pump(&mut app, 3);
    assert_eq!(
        db_cards(&fixture).len(),
        1,
        "the 3bld srs save must spare the case cards"
    );
}

#[test]
fn letters_d_keeps_the_3bld_drill_and_the_3bld_arm_stays_inert() {
    let _guard = guard();
    let fixture = drill_fixture("letters");
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    assert!(!repo(&app).repo_focused);
    press(&mut app, KeyCode::KeyD);
    pump(&mut app, 4);
    assert_eq!(
        current_state(&app),
        AppState::Drill,
        "letters focus keeps routing to the 3bld pair drill"
    );
    assert!(
        app.world()
            .resource::<bt_app::views::drill::DrillSession>()
            .pin
            .is_some(),
        "the pinned 3bld pair drives the drill"
    );

    press(&mut app, KeyCode::Escape);
    pump(&mut app, 3);
    open(&mut app, AppState::Algs);
    walk_to_3bld_case(&mut app);
    press(&mut app, KeyCode::KeyD);
    pump(&mut app, 4);
    assert_eq!(
        current_state(&app),
        AppState::Algs,
        "the 3bld seed arm never opens the case drill"
    );
    assert_eq!(app.world().resource::<PendingDrill>().0, None);
    assert!(app.world().resource::<PendingCaseDrill>().0.is_empty());
}

#[test]
fn an_override_replaces_the_seed_alg_in_the_drill() {
    let _guard = guard();
    let fixture = drill_fixture("override");
    {
        let mut db = Db::open_root(&fixture.paths.user_root).expect("user db");
        bt_data::repo_user::upsert(&mut db, &case_key("2x2", "cll", "cll", "cll.1"), "F R F'")
            .expect("override");
    }
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    walk_to_case(&mut app, "cll.1");
    press(&mut app, KeyCode::KeyD);
    pump(&mut app, 4);
    assert_eq!(current_state(&app), AppState::CaseDrill);
    let current = session(&app).current.as_ref().expect("current case");
    assert_eq!(current.seed_alg, "R U R'");
    assert_eq!(
        current.alg, "F R F'",
        "the override drills instead of the seed"
    );
}

#[test]
fn escape_from_the_case_drill_returns_to_the_menu() {
    let _guard = guard();
    let fixture = drill_fixture("escape");
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    walk_to_case(&mut app, "cll.1");
    press(&mut app, KeyCode::KeyD);
    pump(&mut app, 4);
    assert_eq!(current_state(&app), AppState::CaseDrill);
    press(&mut app, KeyCode::Escape);
    pump(&mut app, 3);
    assert_eq!(current_state(&app), AppState::Menu);
}
