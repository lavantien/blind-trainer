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
