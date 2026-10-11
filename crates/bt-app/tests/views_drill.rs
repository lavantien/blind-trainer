#[allow(dead_code)]
mod views_common;

use bevy::prelude::*;
use bt_app::logic::NetState;
use bt_app::states::{AppState, DrillPhase};
use bt_app::views::PendingDrill;
use bt_app::views::drill::{DrillDraft, DrillSession, DrillVerdict};
use bt_scheme::{CaseId, EDGE_BUFFER_STICKER, Orbit};
use views_common::*;

fn edge_case(first: char, second: char) -> CaseId {
    CaseId {
        orbit: Orbit::Edge,
        t1: bt_scheme::sticker_of(Orbit::Edge, first),
        t2: bt_scheme::sticker_of(Orbit::Edge, second),
    }
}

fn enter_drill_with(fixture: &Fixture, case: CaseId) -> App {
    let mut app = headless_app(temp_paths(fixture));
    boot_to_menu(&mut app);
    app.world_mut().resource_mut::<PendingDrill>().0 = Some(case);
    open(&mut app, AppState::Drill);
    assert_eq!(current_state(&app), AppState::Drill);
    app
}

fn temp_paths(fixture: &Fixture) -> RuntimePaths {
    RuntimePaths {
        config_path: fixture.paths.config_path.clone(),
        vendor_root: fixture.paths.vendor_root.clone(),
        user_root: fixture.paths.user_root.clone(),
        assets_root: fixture.paths.assets_root.clone(),
    }
}

fn phase(app: &App) -> DrillPhase {
    *app.world().resource::<State<DrillPhase>>().get()
}

#[test]
fn drill_rejects_a_wrong_alg_with_named_problems() {
    let _guard = guard();
    let fixture = fixture("wrong");
    let mut app = enter_drill_with(&fixture, edge_case('A', 'B'));
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Ready);
    let session = app.world().resource::<DrillSession>();
    assert_eq!(session.position, "UF-UB-UR");
    assert_eq!(session.letters(), "AB");

    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Memo);

    app.world_mut().resource_mut::<DrillDraft>().0 = "R U R' U'".to_string();
    pump(&mut app, 1);
    press(&mut app, KeyCode::Enter);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Exec);
    let session = app.world().resource::<DrillSession>();
    let verdict = session.verdict.as_ref().expect("verdict recorded");
    assert!(!verdict.is_ok(), "wrong alg must not pass");
    let lines = verdict.lines();
    assert!(!lines.is_empty());
    assert!(
        lines.iter().any(|line| line.contains("should sit on")
            || line.contains("moved to")
            || line.contains("centers")),
        "named problems expected, got {lines:?}"
    );
}

#[test]
fn drill_accepts_the_verified_alg_and_writes_an_srs_review() {
    let _guard = guard();
    let fixture = fixture("accept");
    let mut app = enter_drill_with(&fixture, edge_case('A', 'B'));
    pump(&mut app, 2);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Memo);

    app.world_mut().resource_mut::<DrillDraft>().0 = COVERED_ALG.to_string();
    pump(&mut app, 1);
    press(&mut app, KeyCode::Enter);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Exec);
    let session = app.world().resource::<DrillSession>();
    assert_eq!(session.verdict, Some(DrillVerdict::Ok));

    press(&mut app, KeyCode::Digit3);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Review);
    let stores = app.world().resource::<bt_app::persist::Stores>();
    let card = stores
        .srs
        .cards
        .get("edge/UF-UB-UR")
        .expect("review card written");
    assert_eq!(card.reps, 1);
    assert_eq!(card.reviews.last().expect("review logged").quality, 4);
    assert!(
        fixture.user_root().join("srs").join("srs.json").is_file(),
        "srs store persisted"
    );
}

#[test]
fn drill_highlights_buffer_and_targets_on_the_net() {
    let _guard = guard();
    let fixture = fixture("net");
    let mut app = enter_drill_with(&fixture, edge_case('A', 'B'));
    pump(&mut app, 2);
    let net = app.world().resource::<NetState>();
    assert_eq!(net.buffer_sticker, Some(EDGE_BUFFER_STICKER));
    let case = edge_case('A', 'B');
    assert!(
        net.target_sticker == Some(case.t1) || net.target_sticker == Some(case.t2),
        "one of the two targets highlighted, got {:?}",
        net.target_sticker
    );
    assert!(net.cube.is_solved());
}
