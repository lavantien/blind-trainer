#[allow(dead_code)]
mod views_common;

use bevy::prelude::*;
use bt_app::states::{AppState, SolvePhase};
use bt_app::views::solve::{SolveScrambleOverride, SolveSession};
use bt_core::Cube;
use bt_scheme::{Orbit, apply_sticker_cycles, ideal_cycle_at, state_array};
use views_common::*;

fn phase(app: &App) -> SolvePhase {
    *app.world().resource::<State<SolvePhase>>().get()
}

fn clone_paths(fixture: &Fixture) -> RuntimePaths {
    RuntimePaths {
        config_path: fixture.paths.config_path.clone(),
        vendor_root: fixture.paths.vendor_root.clone(),
        user_root: fixture.paths.user_root.clone(),
        assets_root: fixture.paths.assets_root.clone(),
    }
}

fn single_pair_scramble(orbit: Orbit, first: char, second: char) -> Cube {
    let t1 = bt_scheme::sticker_of(orbit, first);
    let t2 = bt_scheme::sticker_of(orbit, second);
    let buffer = match orbit {
        Orbit::Edge => bt_scheme::EDGE_BUFFER_STICKER,
        Orbit::Corner => bt_scheme::CORNER_BUFFER_STICKER,
    };
    let mut occ = state_array(&Cube::solved());
    let inverse = ideal_cycle_at(orbit, buffer, t2, t1);
    apply_sticker_cycles(&mut occ, &inverse);
    Cube::from_occ(occ)
}

fn enter_solve_with(paths: RuntimePaths, scramble: Cube) -> App {
    let mut app = headless_app(paths);
    boot_to_menu(&mut app);
    app.world_mut().resource_mut::<SolveScrambleOverride>().0 = Some(scramble);
    open(&mut app, AppState::Solve);
    pump(&mut app, 2);
    assert_eq!(phase(&app), SolvePhase::Ready);
    app
}

#[test]
fn solve_cycle_records_exact_splits_and_a_passing_outcome() {
    let _guard = guard();
    let fixture = fixture("pass");
    let scramble = single_pair_scramble(Orbit::Edge, 'A', 'B');
    let mut app = enter_solve_with(clone_paths(&fixture), scramble);

    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), SolvePhase::Memo);
    step(&mut app, 300);
    step(&mut app, 300);
    step(&mut app, 300);

    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), SolvePhase::Exec);
    step(&mut app, 250);
    step(&mut app, 250);

    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), SolvePhase::Review);
    let stores = app.world().resource::<bt_app::persist::Stores>();
    assert_eq!(stores.solves.solves.len(), 1);
    let record = stores.solves.solves[0];
    assert!(record.success, "covered vendor alg solves the memo");
    assert_eq!(record.memo_ms, 900);
    assert_eq!(record.exec_ms, 500);
    assert_eq!(record.memo_ms + record.exec_ms, 1400);
    assert!(record.started_ms > 0);
    let session = app.world().resource::<SolveSession>();
    let finished = session.finished.as_ref().expect("finished");
    assert!(finished.outcome.success);
    assert!(finished.outcome.broken.is_empty());
    let lines = bt_app::views::solve::review_lines(session);
    assert!(lines.contains(&"solved".to_string()));
    assert!(lines.contains(&"memo 0.9s".to_string()));
    assert!(lines.contains(&"exec 0.5s".to_string()));
    assert!(lines.contains(&"total 1.4s".to_string()));
    assert!(
        fixture
            .user_root()
            .join("solves")
            .join("solves.json")
            .is_file(),
        "solve log persisted"
    );
}

#[test]
fn solve_reports_the_pair_that_broke_it_when_uncovered() {
    let _guard = guard();
    let fixture = fixture("broke");
    let scramble = single_pair_scramble(Orbit::Corner, 'A', 'B');
    let mut app = enter_solve_with(clone_paths(&fixture), scramble);

    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), SolvePhase::Review);
    let session = app.world().resource::<SolveSession>();
    let finished = session.finished.as_ref().expect("finished");
    assert!(!finished.outcome.success);
    assert_eq!(
        finished.outcome.broken,
        vec!["UFR-UBL-UBR uncovered".to_string()]
    );
    let stores = app.world().resource::<bt_app::persist::Stores>();
    assert!(!stores.solves.solves[0].success);
}

#[test]
fn solve_review_space_starts_the_next_scramble() {
    let _guard = guard();
    let fixture = fixture("next");
    let scramble = single_pair_scramble(Orbit::Edge, 'A', 'B');
    let mut app = enter_solve_with(clone_paths(&fixture), scramble);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), SolvePhase::Review);
    let first_seed = app.world().resource::<SolveSession>().seed;

    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), SolvePhase::Ready);
    let session = app.world().resource::<SolveSession>();
    assert_ne!(session.seed, first_seed);
    assert!(session.finished.is_none());
    assert!(!session.scramble.is_solved());
}
