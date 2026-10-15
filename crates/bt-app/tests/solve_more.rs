#[allow(dead_code)]
mod views_common;

use bevy::prelude::*;
use bt_app::logic::NetState;
use bt_app::persist::{SolveRecord, Stores};
use bt_app::states::{AppState, SolvePhase};
use bt_app::views::memo::StreamToken;
use bt_app::views::solve::{
    SolveFinished, SolveOutcome, SolveScrambleOverride, SolveSession, SolveZone, SolveZoneKind,
    bind_text, execute_residue, review_lines, simulate,
};
use bt_core::{Alg, Cube};
use bt_data::corpus::{Corpus, Row, SheetClass, Solver};
use bt_scheme::memo::memo as memo_of;
use bt_scheme::{
    EDGE_BUFFER_STICKER, Orbit, Parity, Residue, Twist, apply_residue, apply_sticker_cycles,
    ideal_cycle_at, state_array, sticker_of,
};
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

fn single_pair_scramble(first: char, second: char) -> Cube {
    let t1 = sticker_of(Orbit::Edge, first);
    let t2 = sticker_of(Orbit::Edge, second);
    let mut occ = state_array(&Cube::solved());
    let inverse = ideal_cycle_at(Orbit::Edge, EDGE_BUFFER_STICKER, t2, t1);
    apply_sticker_cycles(&mut occ, &inverse);
    Cube::from_occ(occ)
}

fn row(sheet: SheetClass, solver: Solver, position: &str, alg: &str) -> Row {
    Row {
        solver,
        sheet,
        rowid: 1,
        position: position.to_string(),
        alg: Alg::parse(alg).expect("fixture alg parses"),
        alg_text: alg.to_string(),
        commutator: String::new(),
        recon_link: String::new(),
        youtube_link: String::new(),
    }
}

fn alg_cube(alg: &str) -> Cube {
    let mut cube = Cube::solved();
    cube.apply_alg(&Alg::parse(alg).expect("fixture alg parses"));
    cube
}

fn same_state(left: &Cube, right: &Cube) -> bool {
    state_array(left) == state_array(right)
}

fn run_residue(corpus: &Corpus, residue: &Residue, cube: &mut Cube) -> Vec<String> {
    let mut broken = Vec::new();
    let mut moves = Vec::new();
    execute_residue(corpus, residue, &mut broken, &mut moves);
    for mv in &moves {
        cube.apply(*mv);
    }
    broken
}

fn residue_case() -> Residue {
    Residue {
        parity: None,
        flips: vec![26, 27],
        twists: Vec::new(),
    }
}

fn twist_pair_residue() -> Residue {
    Residue {
        parity: None,
        flips: Vec::new(),
        twists: vec![
            Twist {
                corner: 0,
                quarter: 1,
            },
            Twist {
                corner: 21,
                quarter: 2,
            },
        ],
    }
}

fn parity_residue() -> Residue {
    Residue {
        parity: Some(Parity {
            edge: (26, 24),
            corner: (2, 1),
        }),
        flips: Vec::new(),
        twists: Vec::new(),
    }
}

fn ltct_residue() -> Residue {
    Residue {
        twists: vec![
            Twist {
                corner: 0,
                quarter: 1,
            },
            Twist {
                corner: 21,
                quarter: 2,
            },
        ],
        ..parity_residue()
    }
}

fn scramble_from_residue(residue: &Residue) -> Cube {
    let mut occ = state_array(&Cube::solved());
    apply_residue(&mut occ, residue);
    Cube::from_occ(occ)
}

#[test]
fn flip_pair_residue_applies_the_covered_row_alg() {
    let corpus = Corpus::from_rows(vec![row(
        SheetClass::Flips,
        Solver::Cherry,
        "UF & UL",
        "R U R'",
    )]);
    let mut cube = Cube::solved();
    let broken = run_residue(&corpus, &residue_case(), &mut cube);
    assert!(broken.is_empty());
    assert!(same_state(&cube, &alg_cube("R U R'")));
}

#[test]
fn flip_pair_residue_reports_the_position_when_uncovered() {
    let corpus = Corpus::from_rows(Vec::new());
    let mut cube = Cube::solved();
    let broken = run_residue(&corpus, &residue_case(), &mut cube);
    assert_eq!(broken, vec!["UF & UL uncovered".to_string()]);
    assert!(cube.is_solved());
}

#[test]
fn twist_pair_residue_prefers_the_canonical_row() {
    let corpus = Corpus::from_rows(vec![
        row(SheetClass::Twists, Solver::Cherry, "DFR & UBL", "F2 D2"),
        row(SheetClass::Twists, Solver::Cherry, "UBL & DFR", "L' B' L"),
    ]);
    let mut cube = Cube::solved();
    let broken = run_residue(&corpus, &twist_pair_residue(), &mut cube);
    assert!(broken.is_empty());
    assert!(same_state(&cube, &alg_cube("L' B' L")));
}

#[test]
fn twist_pair_residue_falls_back_to_the_swapped_row_when_canonical_is_missing() {
    let corpus = Corpus::from_rows(vec![row(
        SheetClass::Twists,
        Solver::Cherry,
        "DFR & UBL",
        "F2 D2",
    )]);
    let mut cube = Cube::solved();
    let broken = run_residue(&corpus, &twist_pair_residue(), &mut cube);
    assert!(broken.is_empty());
    assert!(same_state(&cube, &alg_cube("F2 D2")));
}

#[test]
fn twist_pair_residue_reports_canonical_when_both_orders_are_missing() {
    let corpus = Corpus::from_rows(Vec::new());
    let mut cube = Cube::solved();
    let broken = run_residue(&corpus, &twist_pair_residue(), &mut cube);
    assert_eq!(broken, vec!["UBL & DFR uncovered".to_string()]);
}

#[test]
fn parity_residue_applies_the_covered_row_alg() {
    let corpus = Corpus::from_rows(vec![row(
        SheetClass::Parity,
        Solver::Cherry,
        "UF-UB, UFR-UBR",
        "R U R' U'",
    )]);
    let mut cube = Cube::solved();
    let broken = run_residue(&corpus, &parity_residue(), &mut cube);
    assert!(broken.is_empty());
    assert!(same_state(&cube, &alg_cube("R U R' U'")));
}

#[test]
fn parity_residue_reports_the_position_when_uncovered() {
    let corpus = Corpus::from_rows(Vec::new());
    let mut cube = Cube::solved();
    let broken = run_residue(&corpus, &parity_residue(), &mut cube);
    assert_eq!(broken, vec!["UF-UB, UFR-UBR uncovered".to_string()]);
}

#[test]
fn ltct_residue_applies_the_bracketed_position_alg() {
    let corpus = Corpus::from_rows(vec![row(
        SheetClass::Ltct,
        Solver::Cherry,
        "UF-UB, UFR-UBR[UBL]",
        "B2 L2",
    )]);
    let mut cube = Cube::solved();
    let broken = run_residue(&corpus, &ltct_residue(), &mut cube);
    assert!(broken.is_empty());
    assert!(same_state(&cube, &alg_cube("B2 L2")));
}

#[test]
fn ltct_residue_reports_the_bracketed_position_when_uncovered() {
    let corpus = Corpus::from_rows(Vec::new());
    let mut cube = Cube::solved();
    let broken = run_residue(&corpus, &ltct_residue(), &mut cube);
    assert_eq!(broken, vec!["UF-UB, UFR-UBR[UBL] uncovered".to_string()]);
}

#[test]
fn twist_triple_residue_is_reported_as_unsupported() {
    let residue = Residue {
        parity: None,
        flips: Vec::new(),
        twists: vec![
            Twist {
                corner: 0,
                quarter: 1,
            },
            Twist {
                corner: 3,
                quarter: 1,
            },
            Twist {
                corner: 21,
                quarter: 1,
            },
        ],
    };
    let corpus = Corpus::from_rows(Vec::new());
    let mut cube = Cube::solved();
    let broken = run_residue(&corpus, &residue, &mut cube);
    assert_eq!(
        broken,
        vec!["residue unsupported: TwistTriple(0, 3, 21)".to_string()]
    );
}

#[test]
fn compound_residue_is_reported_as_unsupported() {
    let residue = Residue {
        parity: None,
        flips: vec![26, 27, 25],
        twists: Vec::new(),
    };
    let corpus = Corpus::from_rows(Vec::new());
    let mut cube = Cube::solved();
    let broken = run_residue(&corpus, &residue, &mut cube);
    assert_eq!(broken, vec!["residue unsupported: Compound".to_string()]);
}

#[test]
fn simulate_prefers_the_user_alg_over_a_vendor_row_for_the_same_position() {
    let scramble = single_pair_scramble('A', 'B');
    let memo = memo_of(&scramble);
    assert_eq!(memo.pairs.0.len(), 1);
    let overlay_wins = Corpus::from_rows(vec![
        row(SheetClass::Edge, Solver::Cherry, "UF-UB-UR", "R U R'"),
        row(SheetClass::Edge, Solver::User, "UF-UB-UR", COVERED_ALG),
    ]);
    let outcome = simulate(&overlay_wins, &scramble, &memo);
    assert!(outcome.success, "broken: {:?}", outcome.broken);

    let vendor_only = Corpus::from_rows(vec![row(
        SheetClass::Edge,
        Solver::Cherry,
        "UF-UB-UR",
        "R U R'",
    )]);
    let outcome = simulate(&vendor_only, &scramble, &memo);
    assert!(!outcome.success);
    assert!(outcome.broken.is_empty());
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

fn finish_fast(app: &mut App) {
    press(app, KeyCode::Space);
    pump(app, 2);
    assert_eq!(phase(app), SolvePhase::Memo);
    press(app, KeyCode::Space);
    pump(app, 2);
    assert_eq!(phase(app), SolvePhase::Exec);
    press(app, KeyCode::Space);
    pump(app, 2);
    assert_eq!(phase(app), SolvePhase::Review);
}

#[test]
fn zero_cadence_streams_the_whole_token_list_in_one_frame() {
    let _guard = guard();
    let fixture = fixture("cadence-zero");
    let scramble = single_pair_scramble('A', 'B');
    let mut app = enter_solve_with(clone_paths(&fixture), scramble);
    app.world_mut()
        .resource_mut::<Stores>()
        .settings
        .memo_cadence_ms = 0;

    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), SolvePhase::Memo);
    step(&mut app, 100);
    let session = app.world().resource::<SolveSession>();
    assert_eq!(session.tokens.len(), 1);
    assert_eq!(session.shown, session.tokens.len());

    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    step(&mut app, 250);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), SolvePhase::Review);
    let stores = app.world().resource::<Stores>();
    assert_eq!(stores.solves.solves.len(), 1);
    assert!(stores.solves.solves[0].success);
    assert_eq!(stores.solves.solves[0].memo_ms, 100);
    assert_eq!(stores.solves.solves[0].exec_ms, 250);
}

#[test]
fn token_stream_counts_up_at_the_configured_cadence() {
    let _guard = guard();
    let fixture = fixture("cadence-steps");
    let scramble = single_pair_scramble('A', 'B');
    let mut app = enter_solve_with(clone_paths(&fixture), scramble);
    let net = app.world().resource::<NetState>();
    assert!(same_state(&net.cube, &scramble));
    assert_eq!(net.buffer_sticker, None);
    assert_eq!(net.targets, [None, None]);

    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), SolvePhase::Memo);
    step(&mut app, 300);
    step(&mut app, 300);
    assert_eq!(app.world().resource::<SolveSession>().shown, 0);
    step(&mut app, 300);
    let session = app.world().resource::<SolveSession>();
    assert_eq!(session.shown, 1);
    assert_eq!((session.memo_secs * 1000.0).round() as i64, 900);

    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    step(&mut app, 250);
    step(&mut app, 250);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), SolvePhase::Review);
    let record = app.world().resource::<Stores>().solves.solves[0];
    assert!(record.success);
    assert_eq!(record.memo_ms, 900);
    assert_eq!(record.exec_ms, 500);
}

#[test]
fn twist_triple_scramble_fails_with_an_unsupported_residue_report() {
    let _guard = guard();
    let fixture = fixture("twist-triple");
    let mut twists = Vec::new();
    for corner in [0u8, 3, 21] {
        twists.push(Twist { corner, quarter: 1 });
    }
    let residue = Residue {
        parity: None,
        flips: Vec::new(),
        twists,
    };
    let scramble = scramble_from_residue(&residue);
    let mut app = enter_solve_with(clone_paths(&fixture), scramble);
    finish_fast(&mut app);
    let session = app.world().resource::<SolveSession>();
    let finished = session.finished.as_ref().expect("finished");
    assert!(!finished.outcome.success);
    assert_eq!(finished.outcome.broken.len(), 1);
    assert!(finished.outcome.broken[0].starts_with("residue unsupported: TwistTriple("));
    let lines = review_lines(session);
    assert_eq!(lines.first().map(String::as_str), Some("failed"));
    assert!(
        lines
            .iter()
            .any(|line| line.starts_with("residue unsupported: TwistTriple("))
    );
    assert!(!app.world().resource::<Stores>().solves.solves[0].success);
}

#[test]
fn flip_pair_scramble_reports_the_flip_pair_position_as_broken() {
    let _guard = guard();
    let fixture = fixture("flip-pair");
    let mut occ = state_array(&Cube::solved());
    occ.swap(26, 32);
    occ.swap(27, 28);
    let scramble = Cube::from_occ(occ);
    let mut app = enter_solve_with(clone_paths(&fixture), scramble);
    finish_fast(&mut app);
    let session = app.world().resource::<SolveSession>();
    let finished = session.finished.as_ref().expect("finished");
    assert!(!finished.outcome.success);
    assert_eq!(
        finished.outcome.broken,
        vec!["UF & UL uncovered".to_string()]
    );
}

#[test]
fn parity_scramble_reports_the_parity_position_as_broken() {
    let _guard = guard();
    let fixture = fixture("parity");
    let scramble = scramble_from_residue(&parity_residue());
    let mut app = enter_solve_with(clone_paths(&fixture), scramble);
    finish_fast(&mut app);
    let session = app.world().resource::<SolveSession>();
    let finished = session.finished.as_ref().expect("finished");
    assert!(!finished.outcome.success);
    assert_eq!(
        finished.outcome.broken,
        vec!["UF-UB, UFR-UBR uncovered".to_string()]
    );
}

#[test]
fn user_overlay_alg_beats_the_vendor_row_during_the_solve_replay() {
    let _guard = guard();
    let fixture = bare_fixture("overlay");
    add_edge_row(
        &fixture.base,
        COVERED_POSITION,
        "R U R'",
        COVERED_COMMUTATOR,
    );
    let algs_dir = fixture.user_root().join("algs");
    std::fs::create_dir_all(&algs_dir).expect("algs dir");
    std::fs::write(
        algs_dir.join("user-algs.json"),
        serde_json::json!({
            "entries": {
                "edge/UF-UB-UR": {"alg": COVERED_ALG}
            }
        })
        .to_string(),
    )
    .expect("user algs file");
    let scramble = single_pair_scramble('A', 'B');
    let mut app = enter_solve_with(clone_paths(&fixture), scramble);
    finish_fast(&mut app);
    let session = app.world().resource::<SolveSession>();
    let finished = session.finished.as_ref().expect("finished");
    assert!(
        finished.outcome.success,
        "broken: {:?}",
        finished.outcome.broken
    );
    assert!(app.world().resource::<Stores>().solves.solves[0].success);
}

#[test]
fn escape_mid_memo_returns_to_menu_and_a_reopen_restarts_ready() {
    let _guard = guard();
    let fixture = fixture("esc");
    let scramble = single_pair_scramble('A', 'B');
    let mut app = enter_solve_with(clone_paths(&fixture), scramble);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), SolvePhase::Memo);

    press(&mut app, KeyCode::Escape);
    pump(&mut app, 3);
    assert_eq!(current_state(&app), AppState::Menu);

    app.world_mut().resource_mut::<SolveScrambleOverride>().0 =
        Some(single_pair_scramble('A', 'B'));
    open(&mut app, AppState::Solve);
    pump(&mut app, 2);
    assert_eq!(phase(&app), SolvePhase::Ready);
    assert!(
        app.world().resource::<SolveScrambleOverride>().0.is_none(),
        "enter consumes the override"
    );
    assert!(app.world().resource::<SolveSession>().finished.is_none());
    finish_fast(&mut app);
    let stores = app.world().resource::<Stores>();
    assert_eq!(stores.solves.solves.len(), 1);
    assert!(stores.solves.solves[0].success);
}

fn zone_session() -> SolveSession {
    SolveSession {
        seed: 7,
        tokens: vec![StreamToken {
            orbit: Orbit::Edge,
            letters: ['A', 'B'],
            syllable: None,
            image: None,
        }],
        shown: 0,
        memo_secs: 2.5,
        exec_secs: 1.2,
        finished: Some(SolveFinished {
            record: SolveRecord {
                started_ms: 1,
                memo_ms: 900,
                exec_ms: 500,
                success: true,
            },
            outcome: SolveOutcome {
                success: true,
                broken: vec![
                    "UF & UL uncovered".to_string(),
                    "UBL & DFR uncovered".to_string(),
                ],
            },
        }),
        ..SolveSession::default()
    }
}

fn zone_app() -> App {
    let mut app = App::new();
    app.add_plugins(MinimalPlugins)
        .add_plugins(bevy::state::app::StatesPlugin)
        .init_state::<AppState>()
        .add_sub_state::<SolvePhase>()
        .insert_resource(zone_session())
        .add_systems(Update, bind_text);
    for kind in [
        SolveZoneKind::Phase,
        SolveZoneKind::Focal,
        SolveZoneKind::Timer,
        SolveZoneKind::Result,
        SolveZoneKind::Splits,
        SolveZoneKind::Broken,
        SolveZoneKind::Hint,
    ] {
        app.world_mut()
            .spawn((SolveZone(kind), Text::default(), TextColor::default()));
    }
    app
}

fn frames(app: &mut App, count: usize) {
    for _ in 0..count {
        app.update();
    }
}

fn zone_view(app: &mut App, kind: SolveZoneKind) -> (String, Color) {
    let mut query = app.world_mut().query::<(&SolveZone, &Text, &TextColor)>();
    let (text, color) = query
        .iter(app.world())
        .find(|(zone, _, _)| zone.0 == kind)
        .map(|(_, text, color)| (text.0.clone(), color.0))
        .expect("zone spawned");
    (text, color)
}

fn set_phase(app: &mut App, target: SolvePhase) {
    app.world_mut()
        .resource_mut::<NextState<SolvePhase>>()
        .set(target);
    frames(app, 2);
}

#[test]
fn bind_text_drives_every_zone_through_each_phase() {
    let mut app = zone_app();
    app.world_mut()
        .resource_mut::<NextState<AppState>>()
        .set(AppState::Solve);
    frames(&mut app, 3);

    assert_eq!(zone_view(&mut app, SolveZoneKind::Phase).0, "ready, seed 7");
    assert_eq!(zone_view(&mut app, SolveZoneKind::Focal).0, "");
    assert_eq!(zone_view(&mut app, SolveZoneKind::Timer).0, "1 pairs");
    assert_eq!(
        zone_view(&mut app, SolveZoneKind::Hint).0,
        "space to start memo"
    );
    assert_eq!(
        zone_view(&mut app, SolveZoneKind::Splits).0,
        "memo 0.9s\nexec 0.5s\ntotal 1.4s"
    );
    assert_eq!(
        zone_view(&mut app, SolveZoneKind::Broken).0,
        "UF & UL uncovered\nUBL & DFR uncovered"
    );
    let (result, color) = zone_view(&mut app, SolveZoneKind::Result);
    assert_eq!(result, "solved");
    assert_eq!(color, bt_app::theme::ok());

    set_phase(&mut app, SolvePhase::Memo);
    assert_eq!(zone_view(&mut app, SolveZoneKind::Focal).0, "0/1");
    assert_eq!(zone_view(&mut app, SolveZoneKind::Timer).0, "memo 2.5s");
    assert_eq!(
        zone_view(&mut app, SolveZoneKind::Hint).0,
        "space to finish memo"
    );

    set_phase(&mut app, SolvePhase::Exec);
    assert_eq!(zone_view(&mut app, SolveZoneKind::Focal).0, "");
    assert_eq!(zone_view(&mut app, SolveZoneKind::Timer).0, "exec 1.2s");
    assert_eq!(zone_view(&mut app, SolveZoneKind::Hint).0, "space to stop");

    set_phase(&mut app, SolvePhase::Review);
    assert_eq!(zone_view(&mut app, SolveZoneKind::Focal).0, "");
    assert_eq!(zone_view(&mut app, SolveZoneKind::Timer).0, "");
    assert_eq!(
        zone_view(&mut app, SolveZoneKind::Hint).0,
        "space next solve, v 3d replay"
    );
    assert_eq!(zone_view(&mut app, SolveZoneKind::Result).0, "solved");

    app.world_mut()
        .resource_mut::<SolveSession>()
        .finished
        .as_mut()
        .expect("finished")
        .outcome = SolveOutcome {
        success: false,
        broken: Vec::new(),
    };
    frames(&mut app, 1);
    let (result, color) = zone_view(&mut app, SolveZoneKind::Result);
    assert_eq!(result, "failed");
    assert_eq!(color, bt_app::theme::danger());
    assert_eq!(zone_view(&mut app, SolveZoneKind::Broken).0, "");
}
