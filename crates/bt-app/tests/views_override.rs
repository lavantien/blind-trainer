mod views_common;

use bevy::ecs::message::Messages;
use bevy::input::ButtonState;
use bevy::input::keyboard::Key;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
use bt_app::logic::SaveStores;
use bt_app::states::AppState;
use bt_app::views::algs::browse::RepoBrowser;
use bt_app::views::algs::detail::{OverrideDraft, OverrideStore};
use bt_data::db::Db;
use bt_data::repo_user::{CaseRef, load_all};
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

fn override_fixture(tag: &str) -> Fixture {
    let fixture = bare_fixture(tag);
    let packs = fixture.base.join("vendor").join("packs");
    std::fs::write(
        packs.join("registry.json"),
        registry_text(&[("cll.tsv", "2x2", "cll", "cll", 1)]),
    )
    .expect("registry");
    std::fs::write(packs.join("cll.tsv"), pack_text(&[("cll.1", "R U R'")])).expect("pack");
    fixture
}

fn case_key() -> CaseRef {
    CaseRef {
        event: "2x2".to_string(),
        method: "cll".to_string(),
        stage: "cll".to_string(),
        case_key: "cll.1".to_string(),
    }
}

fn db_overrides(fixture: &Fixture) -> bt_data::repo_user::Overrides {
    let db = Db::open_root(&fixture.paths.user_root).expect("user db");
    load_all(&db).expect("overrides")
}

fn walk_to_case(app: &mut App) {
    press(app, KeyCode::ArrowRight);
    pump(app, 2);
    press(app, KeyCode::ArrowDown);
    pump(app, 2);
    press(app, KeyCode::ArrowRight);
    pump(app, 2);
    press(app, KeyCode::ArrowRight);
    pump(app, 2);
    press(app, KeyCode::ArrowRight);
    pump(app, 2);
    let repo = app.world().resource::<RepoBrowser>();
    assert!(repo.repo_focused);
    assert_eq!(repo.depth, 3, "walked to the case level");
    assert_eq!(repo.node().expect("case node").label, "cll.1");
}

fn type_chars(app: &mut App, text: &str) {
    for ch in text.chars() {
        app.world_mut()
            .resource_mut::<Messages<KeyboardInput>>()
            .write(KeyboardInput {
                key_code: KeyCode::Unidentified(bevy::input::keyboard::NativeKeyCode::Unidentified),
                logical_key: Key::Character(ch.to_string().into()),
                state: ButtonState::Pressed,
                text: None,
                repeat: false,
                window: Entity::PLACEHOLDER,
            });
        pump(app, 2);
    }
}

fn backspaces(app: &mut App, count: usize) {
    for _ in 0..count {
        press(app, KeyCode::Backspace);
        pump(app, 2);
    }
}

fn open_draft_with(app: &mut App, text: &str) {
    press(app, KeyCode::KeyO);
    pump(app, 2);
    let draft = app.world().resource::<OverrideDraft>();
    assert!(draft.case.is_some(), "draft opens on a selected case");
    assert_eq!(draft.text, text, "draft prefills the effective alg");
    assert!(draft.error.is_none());
}

#[test]
fn override_edit_is_tier1_gated_and_delete_reverts_to_seed() {
    let _guard = guard();
    let fixture = override_fixture("crud");
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    walk_to_case(&mut app);
    assert!(app.world().resource::<OverrideStore>().0.is_empty());
    assert!(db_overrides(&fixture).is_empty());

    open_draft_with(&mut app, "R U R'");
    backspaces(&mut app, 6);
    type_chars(&mut app, "QQ");
    press(&mut app, KeyCode::Enter);
    pump(&mut app, 2);
    let draft = app.world().resource::<OverrideDraft>();
    assert!(draft.case.is_some(), "a rejected edit keeps the draft open");
    assert!(draft.error.is_some(), "the tier 1 failure is reported");
    assert!(app.world().resource::<OverrideStore>().0.is_empty());
    assert!(db_overrides(&fixture).is_empty());

    press(&mut app, KeyCode::Tab);
    pump(&mut app, 2);
    assert!(app.world().resource::<OverrideDraft>().case.is_none());

    open_draft_with(&mut app, "R U R'");
    backspaces(&mut app, 6);
    type_chars(&mut app, "F R F'");
    press(&mut app, KeyCode::Enter);
    pump(&mut app, 2);
    assert!(app.world().resource::<OverrideDraft>().case.is_none());
    assert_eq!(
        app.world()
            .resource::<OverrideStore>()
            .0
            .get(&case_key())
            .map(String::as_str),
        Some("F R F'")
    );
    assert_eq!(
        db_overrides(&fixture).get(&case_key()).map(String::as_str),
        Some("F R F'")
    );

    open_draft_with(&mut app, "F R F'");
    press(&mut app, KeyCode::Tab);
    pump(&mut app, 2);
    press(&mut app, KeyCode::KeyX);
    pump(&mut app, 2);
    assert!(app.world().resource::<OverrideStore>().0.is_empty());
    assert!(db_overrides(&fixture).is_empty());
}

#[test]
fn overrides_survive_a_stores_save_and_a_reboot() {
    let _guard = guard();
    let fixture = override_fixture("persist");
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    walk_to_case(&mut app);
    open_draft_with(&mut app, "R U R'");
    backspaces(&mut app, 6);
    type_chars(&mut app, "F R F'");
    press(&mut app, KeyCode::Enter);
    pump(&mut app, 2);
    assert!(
        app.world()
            .resource::<OverrideStore>()
            .0
            .contains_key(&case_key())
    );

    app.world_mut()
        .resource_mut::<Messages<SaveStores>>()
        .write(SaveStores);
    pump(&mut app, 3);
    assert!(
        app.world()
            .resource::<OverrideStore>()
            .0
            .contains_key(&case_key()),
        "a stores save must not wipe seed overrides"
    );
    assert!(db_overrides(&fixture).contains_key(&case_key()));

    let mut rebooted = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut rebooted);
    open(&mut rebooted, AppState::Algs);
    walk_to_case(&mut rebooted);
    assert_eq!(
        rebooted
            .world()
            .resource::<OverrideStore>()
            .0
            .get(&case_key())
            .map(String::as_str),
        Some("F R F'"),
        "a fresh boot reloads the persisted override"
    );
}
