#[allow(dead_code)]
mod views_common;

use bevy::input_focus::{FocusCause, InputFocus};
use bevy::prelude::*;
use bevy::text::EditableText;
use bt_app::logic::{BootWarnings, FatalReason, LastSave, VendorCorpus};
use bt_app::persist::{MemoMode, Stores};
use bt_app::states::AppState;
use bt_app::views::menu::warning_context;
use bt_app::views::settings::SettingsDraft;
use views_common::*;

fn clone_paths(fixture: &Fixture) -> RuntimePaths {
    RuntimePaths {
        config_path: fixture.paths.config_path.clone(),
        vendor_root: fixture.paths.vendor_root.clone(),
        packs_root: fixture.paths.packs_root.clone(),
        user_root: fixture.paths.user_root.clone(),
        assets_root: fixture.paths.assets_root.clone(),
    }
}

#[test]
fn digits_open_each_view_and_esc_returns_to_menu() {
    let _guard = guard();
    let fixture = fixture("digit-tour");
    let mut app = headless_app(clone_paths(&fixture));
    boot_to_menu(&mut app);
    let tour = [
        (KeyCode::Digit1, AppState::Drill),
        (KeyCode::Digit2, AppState::Memo),
        (KeyCode::Digit3, AppState::Solve),
        (KeyCode::Digit4, AppState::Algs),
        (KeyCode::Digit5, AppState::Stats),
        (KeyCode::Digit6, AppState::Settings),
    ];
    for (key, view) in tour {
        press(&mut app, key);
        pump(&mut app, 3);
        assert_eq!(current_state(&app), view, "key {key:?} opens its view");
        press(&mut app, KeyCode::Escape);
        pump(&mut app, 3);
        assert_eq!(
            current_state(&app),
            AppState::Menu,
            "esc returns from {view:?}"
        );
    }
}

#[test]
fn digits_stay_inert_outside_the_menu() {
    let _guard = guard();
    let fixture = fixture("digits-inert");
    let mut app = headless_app(clone_paths(&fixture));
    boot_to_menu(&mut app);
    press(&mut app, KeyCode::Digit5);
    pump(&mut app, 3);
    assert_eq!(current_state(&app), AppState::Stats);
    press(&mut app, KeyCode::Digit1);
    pump(&mut app, 3);
    assert_eq!(
        current_state(&app),
        AppState::Stats,
        "digits navigate only from the menu"
    );
}

#[test]
fn escape_in_the_menu_stays_in_the_menu() {
    let _guard = guard();
    let fixture = fixture("esc-menu");
    let mut app = headless_app(clone_paths(&fixture));
    boot_to_menu(&mut app);
    press(&mut app, KeyCode::Escape);
    pump(&mut app, 3);
    assert_eq!(current_state(&app), AppState::Menu);
}

#[test]
fn escape_drops_a_focused_input_before_leaving_settings() {
    let _guard = guard();
    let fixture = fixture("esc-settings");
    let mut app = headless_app(clone_paths(&fixture));
    boot_to_menu(&mut app);
    press(&mut app, KeyCode::Digit6);
    pump(&mut app, 3);
    assert_eq!(current_state(&app), AppState::Settings);
    app.world_mut().insert_resource(InputFocus::default());
    let editable = app.world_mut().spawn(EditableText::default()).id();
    app.world_mut()
        .resource_mut::<InputFocus>()
        .set(editable, FocusCause::Navigated);

    press(&mut app, KeyCode::Digit2);
    pump(&mut app, 2);
    assert_eq!(
        app.world().resource::<SettingsDraft>().memo_mode,
        MemoMode::Syllables,
        "mode keys stay inert while an input is focused"
    );

    press(&mut app, KeyCode::Escape);
    pump(&mut app, 2);
    assert_eq!(
        current_state(&app),
        AppState::Settings,
        "first esc defocuses"
    );
    assert_eq!(app.world().resource::<InputFocus>().get(), None);

    press(&mut app, KeyCode::Escape);
    pump(&mut app, 2);
    assert_eq!(current_state(&app), AppState::Menu, "second esc exits");
}

#[test]
fn mode_keys_apply_immediately_and_persist_per_orbit() {
    let _guard = guard();
    let fixture = fixture("mode-apply");
    let mut app = headless_app(clone_paths(&fixture));
    boot_to_menu(&mut app);
    press(&mut app, KeyCode::Digit6);
    pump(&mut app, 3);
    assert_eq!(
        app.world().resource::<SettingsDraft>().memo_mode,
        MemoMode::Syllables,
        "opening settings must not leak the menu digit into the mode keys"
    );

    press(&mut app, KeyCode::Digit2);
    pump(&mut app, 3);
    assert_eq!(
        app.world().resource::<SettingsDraft>().memo_mode,
        MemoMode::Images
    );
    assert_eq!(
        app.world().resource::<Stores>().settings.memo_mode,
        MemoMode::Images
    );

    press(&mut app, KeyCode::Digit1);
    pump(&mut app, 3);
    assert_eq!(
        app.world().resource::<Stores>().settings.memo_mode,
        MemoMode::Syllables
    );

    press(&mut app, KeyCode::Digit3);
    pump(&mut app, 3);
    assert_eq!(
        app.world().resource::<SettingsDraft>().memo_mode,
        MemoMode::PerOrbit
    );
    assert_eq!(
        app.world().resource::<Stores>().settings.memo_mode,
        MemoMode::PerOrbit
    );
    assert!(app.world().resource::<LastSave>().settings_ok);

    press(&mut app, KeyCode::Digit4);
    pump(&mut app, 2);
    assert_eq!(current_state(&app), AppState::Settings);
    assert_eq!(
        app.world().resource::<SettingsDraft>().memo_mode,
        MemoMode::PerOrbit,
        "digit 4 is not a mode key"
    );

    assert!(
        bt_data::paths::db_path_at(fixture.user_root()).is_file(),
        "settings persisted into the db"
    );

    let mut reloaded = headless_app(clone_paths(&fixture));
    boot_to_menu(&mut reloaded);
    assert_eq!(
        reloaded.world().resource::<Stores>().settings.memo_mode,
        MemoMode::PerOrbit,
        "the mode survives a restart"
    );
}

#[test]
fn settings_draft_resets_from_stores_on_reenter() {
    let _guard = guard();
    let fixture = fixture("draft-reset");
    let mut app = headless_app(clone_paths(&fixture));
    boot_to_menu(&mut app);
    press(&mut app, KeyCode::Digit6);
    pump(&mut app, 3);
    press(&mut app, KeyCode::Digit2);
    pump(&mut app, 3);
    assert_eq!(
        app.world().resource::<Stores>().settings.memo_mode,
        MemoMode::Images
    );
    press(&mut app, KeyCode::Escape);
    pump(&mut app, 3);
    assert_eq!(current_state(&app), AppState::Menu);
    press(&mut app, KeyCode::Digit6);
    pump(&mut app, 3);
    assert_eq!(
        app.world().resource::<SettingsDraft>().memo_mode,
        MemoMode::Images,
        "entering settings reloads the draft from the stores"
    );
    assert_eq!(
        *app.world().resource::<SettingsDraft>(),
        SettingsDraft::from_settings(&app.world().resource::<Stores>().settings)
    );
}

#[test]
fn missing_vendor_dir_warns_at_the_menu_and_still_boots() {
    let _guard = guard();
    let fixture = bare_fixture("vendor-missing");
    let mut paths = clone_paths(&fixture);
    paths.vendor_root = fixture.base.join("vendor-absent");
    let mut app = headless_app(paths.clone());
    boot_to_menu(&mut app);
    let warnings = &app.world().resource::<BootWarnings>().0;
    assert_eq!(warnings.len(), 1, "warnings: {warnings:?}");
    assert!(
        warnings[0].contains("vendor data missing"),
        "warning: {}",
        warnings[0]
    );
    assert!(
        warnings[0].contains(&paths.vendor_root.to_string_lossy().to_string()),
        "warning names the missing path: {}",
        warnings[0]
    );
    assert_eq!(
        warning_context(warnings),
        warnings[0],
        "the menu header carries the boot warning verbatim"
    );
    assert!(app.world().resource::<VendorCorpus>().0.is_empty());
}

#[test]
fn boot_with_vendor_data_reports_no_warnings() {
    let _guard = guard();
    let fixture = fixture("vendor-present");
    let mut app = headless_app(clone_paths(&fixture));
    boot_to_menu(&mut app);
    assert!(app.world().resource::<BootWarnings>().0.is_empty());
}

#[test]
fn fatal_screen_ignores_keys() {
    let _guard = guard();
    let fixture = bare_fixture("fatal-keys");
    let mut paths = clone_paths(&fixture);
    paths.config_path = paths.config_path.with_extension("absent");
    let mut app = headless_app(paths);
    pump(&mut app, 8);
    assert_eq!(current_state(&app), AppState::Fatal);
    let reason = app.world().resource::<FatalReason>().0.clone();
    assert!(reason.contains("cannot read config"), "reason: {reason}");
    press(&mut app, KeyCode::Digit1);
    press(&mut app, KeyCode::Digit6);
    press(&mut app, KeyCode::Escape);
    pump(&mut app, 3);
    assert_eq!(
        current_state(&app),
        AppState::Fatal,
        "the fatal screen ignores every key"
    );
}

#[test]
fn state_names_pin_the_documented_labels() {
    let expected = [
        (AppState::Bootstrap, "bootstrap", "booting"),
        (AppState::Fatal, "fatal", "fatal"),
        (AppState::Menu, "menu", "menu"),
        (AppState::Drill, "drill", "drill"),
        (AppState::Memo, "memo", "memo training"),
        (AppState::Solve, "solve", "solve"),
        (AppState::Algs, "algs", "algs"),
        (AppState::Stats, "stats", "stats"),
        (AppState::Settings, "settings", "settings"),
    ];
    for (state, slug, label) in expected {
        assert_eq!(state.slug(), slug, "slug for {state:?}");
        assert_eq!(state.label(), label, "label for {state:?}");
    }
    assert_eq!(
        AppState::MENU_KEYS.map(|(key, _)| key),
        [
            KeyCode::Digit1,
            KeyCode::Digit2,
            KeyCode::Digit3,
            KeyCode::Digit4,
            KeyCode::Digit5,
            KeyCode::Digit6,
        ]
    );
    assert_eq!(
        AppState::MENU_KEYS.map(|(_, state)| state),
        [
            AppState::Drill,
            AppState::Memo,
            AppState::Solve,
            AppState::Algs,
            AppState::Stats,
            AppState::Settings,
        ]
    );
}
