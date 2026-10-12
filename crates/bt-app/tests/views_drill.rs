#[allow(dead_code)]
mod views_common;

use bevy::prelude::*;
use bt_app::logic::{AppConfig, NetState};
use bt_app::present::FontBook;
use bt_app::states::{AppState, DrillPhase};
use bt_app::views::PendingDrill;
use bt_app::views::drill::{
    self, DrillSession, card_key, case_position, flip_case, mechanic_spans, pair_is_due, pick_pair,
    sheet_of,
};
use bt_app::views::drill_view::{CardPanel, DrillCardRoot, RatePanel, RepsPanel};
use bt_core::{Alg, Cube};
use bt_data::corpus::{Corpus, Row, SheetClass, Solver};
use bt_data::srs::Card;
use bt_scheme::{CaseId, EDGE_BUFFER_STICKER, Orbit, Rng};
use views_common::*;

fn edge_case(first: char, second: char) -> CaseId {
    CaseId {
        orbit: Orbit::Edge,
        t1: bt_scheme::sticker_of(Orbit::Edge, first),
        t2: bt_scheme::sticker_of(Orbit::Edge, second),
    }
}

fn enter_drill_with(fixture: &Fixture, case: Option<CaseId>) -> App {
    let mut app = headless_app(temp_paths(fixture));
    boot_to_menu(&mut app);
    if let Some(case) = case {
        app.world_mut().resource_mut::<PendingDrill>().0 = Some(case);
    }
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

fn prepared_cube(session: &DrillSession) -> Cube {
    let mut cube = Cube::solved();
    cube.apply_alg(&session.side_b.as_ref().expect("side b").alg);
    cube
}

fn run_reps(app: &mut App, presses: usize) {
    for _ in 0..presses {
        step(app, 400);
        press(app, KeyCode::Space);
        pump(app, 2);
    }
}

fn enter_rate(app: &mut App) {
    press(app, KeyCode::Space);
    pump(app, 2);
    assert_eq!(phase(app), DrillPhase::Reps);
    let target = app.world().resource::<DrillSession>().reps_target as usize;
    run_reps(app, target);
    assert_eq!(phase(app), DrillPhase::Rate);
}

#[test]
fn card_pins_the_pair_with_inverse_side_b() {
    let _guard = guard();
    let fixture = fixture("card");
    let mut app = enter_drill_with(&fixture, Some(edge_case('A', 'B')));
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Card);
    let session = app.world().resource::<DrillSession>();
    let a = session.side_a.as_ref().expect("side a");
    let b = session.side_b.as_ref().expect("side b");
    assert_eq!(a.letters(), "AB");
    assert_eq!(a.position, "UF-UB-UR");
    assert_eq!(a.solver_label, "cherry");
    assert!(a.vendor);
    assert_eq!(a.commutator, COVERED_COMMUTATOR);
    assert!(!b.vendor, "fixture covers no flip row");
    assert_eq!(b.solver_label, "inverse");
    assert_eq!(b.alg, a.alg.inverse());
    assert_eq!(session.reps_target, 4);
    assert_eq!(
        session.prepared,
        prepared_cube(session),
        "prepared must be solved plus side b"
    );
    let corpus = &app.world().resource::<bt_app::logic::VendorCorpus>().0;
    let book = app.world().resource::<bt_app::views::SyllableBook>();
    let stores = app.world().resource::<bt_app::persist::Stores>();
    let now = app.world().resource::<bt_app::views::NowDay>().0;
    let lines = drill::card_lines(session, corpus, book, stores, now);
    assert_eq!(lines[0], "AB");
    assert_eq!(lines[1], "edge UF-UB-UR");
    assert_eq!(lines[2], "memo AB", "fixture ships no syllables");
    assert_eq!(lines[3], COVERED_ALG);
    assert_eq!(lines[4], format!("commutator {COVERED_COMMUTATOR}"));
    assert_eq!(
        lines[5],
        format!("prepare BA inverse: {}", a.alg.inverse().format())
    );
    assert_eq!(lines[7], "srs AB new, BA new");
    let net = app.world().resource::<NetState>();
    assert_eq!(net.buffer_sticker, Some(EDGE_BUFFER_STICKER));
    let case = edge_case('A', 'B');
    assert_eq!(net.targets, [Some(case.t1), Some(case.t2)]);
    assert!(net.cube.is_solved(), "card shows the solved cube");
}

#[test]
fn space_opens_reps_on_the_prepared_cube() {
    let _guard = guard();
    let fixture = fixture("reps");
    let mut app = enter_drill_with(&fixture, Some(edge_case('A', 'B')));
    pump(&mut app, 2);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Reps);
    let expected = {
        let session = app.world().resource::<DrillSession>();
        assert_eq!(session.reps_done, 0);
        assert!(session.reps_side_a, "side a executes first");
        prepared_cube(session)
    };
    let net = app.world().resource::<NetState>();
    assert_eq!(net.cube, expected);
    assert!(!net.cube.is_solved(), "the drilling state is a scramble");
    let case = edge_case('A', 'B');
    assert_eq!(net.targets, [Some(case.t1), Some(case.t2)]);
}

#[test]
fn reps_alternate_sides_and_reach_rate() {
    let _guard = guard();
    let fixture = fixture("alternation");
    let mut app = enter_drill_with(&fixture, Some(edge_case('A', 'B')));
    pump(&mut app, 2);
    let expected_prepared = {
        let session = app.world().resource::<DrillSession>();
        assert_eq!(session.reps_target, 4);
        prepared_cube(session)
    };
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Reps);
    for index in 0..4 {
        let before = app.world().resource::<DrillSession>().reps_done;
        run_reps(&mut app, 1);
        let session = app.world().resource::<DrillSession>();
        assert_eq!(session.reps_done, before + 1);
        let (side_a, secs) = session.splits[index];
        assert_eq!(side_a, index % 2 == 0, "sides alternate a then b");
        assert!(
            (0.35..0.55).contains(&secs),
            "split {secs} must track the 400ms step"
        );
    }
    assert_eq!(phase(&app), DrillPhase::Rate);
    let session = app.world().resource::<DrillSession>();
    assert_eq!(session.splits.len(), 4);
    assert!(session.reps_side_a, "even reps end ready for side a again");
    let net = app.world().resource::<NetState>();
    assert_eq!(
        net.cube, expected_prepared,
        "even reps return the cube to the drilling state"
    );
    let mut solved_again = net.cube;
    let alg = session.side_a.as_ref().expect("side a").alg.clone();
    solved_again.apply_alg(&alg);
    assert!(solved_again.is_solved());
}

#[test]
fn rate_writes_both_cards_and_advances_to_the_next_pair() {
    let _guard = guard();
    let fixture = fixture("rate");
    let mut app = enter_drill_with(&fixture, Some(edge_case('A', 'B')));
    pump(&mut app, 2);
    enter_rate(&mut app);
    press(&mut app, KeyCode::Digit3);
    pump(&mut app, 2);
    assert_eq!(
        phase(&app),
        DrillPhase::Rate,
        "side b still waits for a rate"
    );
    let stores = app.world().resource::<bt_app::persist::Stores>();
    let card_a = stores
        .srs
        .cards
        .get("edge/UF-UB-UR")
        .expect("side a card written");
    assert_eq!(card_a.reps, 1);
    assert_eq!(card_a.reviews.last().expect("review logged").quality, 4);
    assert!(
        !stores.srs.cards.contains_key("edge/UF-UR-UB"),
        "side b card waits for its own prompt"
    );

    press(&mut app, KeyCode::Digit3);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Card);
    let stores = app.world().resource::<bt_app::persist::Stores>();
    let card_b = stores
        .srs
        .cards
        .get("edge/UF-UR-UB")
        .expect("side b card written");
    assert_eq!(card_b.reps, 1);
    assert!(
        fixture.user_root().join("srs").join("srs.json").is_file(),
        "srs store persisted"
    );
    let session = app.world().resource::<DrillSession>();
    assert_eq!(
        session.side_a.as_ref().expect("side a").letters(),
        "AB",
        "the fixture has one covered pair, so the next pair is the same"
    );
    assert_eq!(session.reps_done, 0);
    assert!(session.splits.is_empty());
    assert_eq!(session.rate_step, 0);
    let net = app.world().resource::<NetState>();
    assert!(net.cube.is_solved(), "the next card starts from solved");
}

#[test]
fn alternation_guard_replaces_a_broken_vendor_flip() {
    let _guard = guard();
    let fixture = fixture("guard");
    add_edge_row(&fixture.base, "UF-UR-UB", "R U R' U'", "");
    let mut app = enter_drill_with(&fixture, Some(edge_case('A', 'B')));
    pump(&mut app, 2);
    let session = app.world().resource::<DrillSession>();
    let b = session.side_b.as_ref().expect("side b");
    assert!(!b.vendor, "a flip that breaks alternation must be replaced");
    assert_eq!(b.solver_label, "inverse");
    let a = session.side_a.as_ref().expect("side a").alg.clone();
    let mut cube = Cube::solved();
    cube.apply_alg(&a);
    cube.apply_alg(&b.alg);
    assert!(cube.is_solved());
    let expected = prepared_cube(session);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(app.world().resource::<NetState>().cube, expected);
}

#[test]
fn an_empty_corpus_keeps_the_drill_idle() {
    let _guard = guard();
    let fixture = bare_fixture("empty");
    let mut app = enter_drill_with(&fixture, None);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Card);
    let session = app.world().resource::<DrillSession>();
    assert!(session.sides().is_none());
    assert_eq!(session.pin, None);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Card, "space must not open reps");
}

#[test]
fn phase_panels_spawn_and_despawn_per_phase() {
    let _guard = guard();
    let fixture = fixture("panels");
    let mut app = headless_app(temp_paths(&fixture));
    app.insert_resource(FontBook {
        regular: Handle::default(),
        bold: Handle::default(),
    })
    .add_systems(
        OnEnter(AppState::Drill),
        |mut commands: Commands, fonts: Res<FontBook>, config: Res<AppConfig>| {
            drill::spawn(&mut commands, &fonts, &config);
        },
    );
    boot_to_menu(&mut app);
    app.world_mut().resource_mut::<PendingDrill>().0 = Some(edge_case('A', 'B'));
    open(&mut app, AppState::Drill);
    pump(&mut app, 4);
    assert_eq!(count::<CardPanel>(&mut app), 1);
    assert_eq!(count::<DrillCardRoot>(&mut app), 1);
    assert!(
        count::<bt_app::net::NetView>(&mut app) > 0,
        "the frame hosts the cube net"
    );

    press(&mut app, KeyCode::Space);
    pump(&mut app, 3);
    assert_eq!(phase(&app), DrillPhase::Reps);
    assert_eq!(count::<CardPanel>(&mut app), 0, "card despawns on exit");
    assert_eq!(count::<RepsPanel>(&mut app), 1);

    let target = app.world().resource::<DrillSession>().reps_target as usize;
    run_reps(&mut app, target);
    assert_eq!(phase(&app), DrillPhase::Rate);
    assert_eq!(count::<RepsPanel>(&mut app), 0);
    assert_eq!(count::<RatePanel>(&mut app), 1);

    press(&mut app, KeyCode::Digit3);
    pump(&mut app, 2);
    press(&mut app, KeyCode::Digit3);
    pump(&mut app, 2);
    assert_eq!(phase(&app), DrillPhase::Card);
    assert_eq!(count::<RatePanel>(&mut app), 0);
    assert_eq!(
        count::<CardPanel>(&mut app),
        1,
        "the next pair respawns the card"
    );
}

fn count<M: Component>(app: &mut App) -> usize {
    let mut query = app.world_mut().query_filtered::<Entity, With<M>>();
    query.iter(app.world()).count()
}

fn corner_case(first: char, second: char) -> CaseId {
    CaseId {
        orbit: Orbit::Corner,
        t1: bt_scheme::sticker_of(Orbit::Corner, first),
        t2: bt_scheme::sticker_of(Orbit::Corner, second),
    }
}

fn corpus_row(sheet: SheetClass, position: &str) -> Row {
    Row {
        solver: Solver::Cherry,
        sheet,
        rowid: 1,
        position: position.to_string(),
        alg: Alg::parse(COVERED_ALG).expect("fixture alg parses"),
        alg_text: COVERED_ALG.to_string(),
        commutator: String::new(),
        recon_link: String::new(),
        youtube_link: String::new(),
    }
}

fn stores_with_reps(reps: u32) -> bt_app::persist::Stores {
    bt_app::persist::Stores {
        settings: bt_app::persist::Settings {
            memo_cadence_ms: 800,
            memo_mode: bt_app::persist::MemoMode::Syllables,
            drill_reps: reps,
        },
        srs: bt_app::persist::SrsStore::default(),
        solves: bt_app::persist::SolveLog::default(),
        user_algs: bt_data::paths::UserAlgs::default(),
    }
}

fn seen_card() -> Card {
    Card {
        ease: 2.5,
        interval_days: 6,
        due_day: 10,
        reps: 2,
        lapses: 0,
        reviews: Vec::new(),
    }
}

#[test]
fn pair_is_due_when_either_side_is_new_or_due() {
    let case = edge_case('A', 'B');
    let mut stores = stores_with_reps(4);
    assert!(pair_is_due(&stores, 0, &case), "no cards counts as due");
    stores
        .srs
        .cards
        .insert(card_key(SheetClass::Edge, "UF-UB-UR"), seen_card());
    assert!(pair_is_due(&stores, 0, &case), "one new side stays due");
    stores
        .srs
        .cards
        .insert(card_key(SheetClass::Edge, "UF-UR-UB"), seen_card());
    assert!(!pair_is_due(&stores, 0, &case), "both seen and ok");
    let mut due = seen_card();
    due.due_day = 0;
    stores
        .srs
        .cards
        .insert(card_key(SheetClass::Edge, "UF-UR-UB"), due);
    assert!(pair_is_due(&stores, 0, &case), "one due side is due");
}

#[test]
fn pick_pair_prefers_due_pairs_and_skips_uncovered() {
    let cases = [
        edge_case('A', 'B'),
        edge_case('A', 'D'),
        corner_case('A', 'B'),
    ];
    let rows: Vec<Row> = cases
        .iter()
        .map(|case| corpus_row(sheet_of(case.orbit), &case_position(case)))
        .collect();
    let corpus = Corpus::from_rows(rows);
    let mut stores = stores_with_reps(4);
    for case in [cases[0], flip_case(&cases[0])] {
        stores.srs.cards.insert(
            card_key(sheet_of(case.orbit), &case_position(&case)),
            seen_card(),
        );
    }
    let mut rng = Rng::new(11);
    for _ in 0..24 {
        let picked = pick_pair(&corpus, &stores, 0, &mut rng).expect("covered pair exists");
        assert_ne!(
            case_position(&picked),
            case_position(&cases[0]),
            "seen pair must wait behind due pairs"
        );
    }
    for case in cases.iter().flat_map(|case| [*case, flip_case(case)]) {
        stores.srs.cards.insert(
            card_key(sheet_of(case.orbit), &case_position(&case)),
            seen_card(),
        );
    }
    let covered: Vec<String> = cases.iter().map(case_position).collect();
    for _ in 0..24 {
        let picked = pick_pair(&corpus, &stores, 0, &mut rng).expect("covered pair exists");
        assert!(covered.contains(&case_position(&picked)));
    }
    assert_eq!(
        pick_pair(&Corpus::from_rows(Vec::new()), &stores, 0, &mut rng),
        None
    );
}

#[test]
fn mechanic_spans_color_each_role() {
    let alg = Alg::parse(COVERED_ALG).expect("alg parses");
    let spans =
        mechanic_spans(COVERED_COMMUTATOR, &alg).expect("commutator verifies against the alg");
    let joined: String = spans.iter().map(|(text, _, _)| text.as_str()).collect();
    assert_eq!(joined, "R2 U': [S, R2]");
    assert_eq!(
        spans[0],
        (
            "R2 U': ".to_string(),
            bt_app::theme::FONT_ALG,
            bt_app::theme::setup()
        )
    );
    assert_eq!(
        spans[2],
        (
            "S".to_string(),
            bt_app::theme::FONT_ALG,
            bt_app::theme::teal()
        )
    );
    assert_eq!(
        spans[4],
        (
            "R2".to_string(),
            bt_app::theme::FONT_ALG,
            bt_app::theme::interchange()
        )
    );

    let b_first = Alg::parse("R2 S R2 S'").expect("alg parses");
    let flipped = mechanic_spans("[S,R2]", &b_first).expect("b_first expansion matches");
    assert_eq!(
        flipped[1],
        (
            "R2".to_string(),
            bt_app::theme::FONT_ALG,
            bt_app::theme::interchange()
        )
    );
    assert_eq!(
        flipped[3],
        (
            "S".to_string(),
            bt_app::theme::FONT_ALG,
            bt_app::theme::teal()
        )
    );

    let junk = Alg::parse("R U R' U'").expect("alg parses");
    for text in ["", "Not found.", "]]", "[S,R2]"] {
        assert!(mechanic_spans(text, &junk).is_none(), "{text} must reject");
    }
}
