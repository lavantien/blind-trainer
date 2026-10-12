mod views_common;

use bevy::prelude::*;
use bt_app::states::AppState;
use bt_app::views::PendingDrill;
use bt_app::views::algs::AlgsBrowser;
use bt_app::views::drill::DrillSession;
use bt_scheme::{CaseId, Orbit};
use views_common::*;

fn edge_case(first: char, second: char) -> CaseId {
    CaseId {
        orbit: Orbit::Edge,
        t1: bt_scheme::sticker_of(Orbit::Edge, first),
        t2: bt_scheme::sticker_of(Orbit::Edge, second),
    }
}

fn temp_paths(fixture: &Fixture) -> RuntimePaths {
    RuntimePaths {
        config_path: fixture.paths.config_path.clone(),
        vendor_root: fixture.paths.vendor_root.clone(),
        user_root: fixture.paths.user_root.clone(),
        assets_root: fixture.paths.assets_root.clone(),
    }
}

#[test]
fn algs_search_routes_the_selected_case_into_drill() {
    let _guard = guard();
    let fixture = fixture("route");
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    app.world_mut()
        .resource_mut::<bt_app::views::algs::AlgsQuery>()
        .0 = "ab".to_string();
    pump(&mut app, 2);
    let browser = app.world().resource::<AlgsBrowser>();
    assert_eq!(browser.filtered.len(), 2);
    assert_eq!(browser.selected, 0);
    assert_eq!(browser.filtered[0].orbit, Orbit::Edge);

    press(&mut app, KeyCode::KeyD);
    pump(&mut app, 3);
    assert_eq!(current_state(&app), AppState::Drill);
    let pending = app.world().resource::<PendingDrill>().0;
    assert_eq!(pending, None, "drill enter consumed the pending case");
    let session = app.world().resource::<DrillSession>();
    assert_eq!(session.case, Some(edge_case('A', 'B')));
}

#[test]
fn algs_navigation_survives_window_focus_on_a_fresh_boot() {
    let _guard = guard();
    let fixture = fixture("windowfocus");
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    let window = app.world_mut().spawn_empty().id();
    app.world_mut()
        .insert_resource(bevy::input_focus::InputFocus::from_entity(window));
    open(&mut app, AppState::Algs);
    let browser = app.world().resource::<AlgsBrowser>();
    assert_eq!(browser.selected, 0);

    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    let browser = app.world().resource::<AlgsBrowser>();
    assert_eq!(
        browser.selected, 1,
        "window focus must not block navigation"
    );
    assert_eq!(current_state(&app), AppState::Algs);
}

#[test]
fn arrows_keep_navigating_while_the_filter_box_is_focused() {
    let _guard = guard();
    let fixture = fixture("focusedarrows");
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    app.world_mut()
        .insert_resource(bevy::input_focus::InputFocus::default());
    let editable = app
        .world_mut()
        .spawn(bevy::text::EditableText::default())
        .id();
    app.world_mut()
        .resource_mut::<bevy::input_focus::InputFocus>()
        .set(editable, bevy::input_focus::FocusCause::Navigated);

    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    let browser = app.world().resource::<AlgsBrowser>();
    assert_eq!(
        browser.selected, 1,
        "arrows must navigate even while typing in the filter"
    );

    press(&mut app, KeyCode::KeyD);
    pump(&mut app, 2);
    assert_eq!(
        current_state(&app),
        AppState::Algs,
        "d must stay with the filter while it is focused"
    );
}

#[test]
fn escape_releases_a_focused_text_input_before_leaving_the_view() {
    let _guard = guard();
    let fixture = fixture("escapedefocus");
    let mut app = headless_app(temp_paths(&fixture));
    boot_to_menu(&mut app);
    open(&mut app, AppState::Algs);
    app.world_mut()
        .insert_resource(bevy::input_focus::InputFocus::default());
    let editable = app
        .world_mut()
        .spawn(bevy::text::EditableText::default())
        .id();
    app.world_mut()
        .resource_mut::<bevy::input_focus::InputFocus>()
        .set(editable, bevy::input_focus::FocusCause::Navigated);

    press(&mut app, KeyCode::Escape);
    pump(&mut app, 2);
    assert_eq!(
        current_state(&app),
        AppState::Algs,
        "first escape defocuses"
    );
    assert_eq!(
        app.world()
            .resource::<bevy::input_focus::InputFocus>()
            .get(),
        None
    );

    press(&mut app, KeyCode::Escape);
    pump(&mut app, 2);
    assert_eq!(current_state(&app), AppState::Menu, "second escape exits");
}
