#[allow(dead_code)]
mod views_common;

use bevy::ecs::message::Messages;
use bevy::input::ButtonState;
use bevy::input::keyboard::{Key, KeyboardInput};
use bevy::prelude::*;
use bt_app::persist::{MemoMode, Settings, SolveLog, SrsStore, Stores};
use bt_app::states::{AppState, MemoPhase};
use bt_app::views::SyllableBook;
use bt_app::views::memo::{
    MemoSeed, MemoSession, StreamToken, build_tokens, orbit_name, rate_pairs, rebuild,
    residue_lines, token_text, view_rows,
};
use bt_data::fsrs::FsrsConfig;
use bt_data::paths::UserAlgs;
use bt_data::srs::Rating;
use bt_scheme::memo::memo as memo_of;
use bt_scheme::{CaseId, Memo, Orbit, Parity, Residue, Twist, random_state};
use views_common::*;

fn phase(app: &App) -> MemoPhase {
    *app.world().resource::<State<MemoPhase>>().get()
}

fn clone_paths(fixture: &Fixture) -> RuntimePaths {
    RuntimePaths {
        config_path: fixture.paths.config_path.clone(),
        vendor_root: fixture.paths.vendor_root.clone(),
        user_root: fixture.paths.user_root.clone(),
        assets_root: fixture.paths.assets_root.clone(),
    }
}

fn enter_memo(paths: RuntimePaths, seed: u64) -> App {
    let mut app = headless_app(paths);
    boot_to_menu(&mut app);
    app.world_mut().resource_mut::<MemoSeed>().0 = seed;
    open(&mut app, AppState::Memo);
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Intro);
    app
}

fn send(app: &mut App, key: KeyCode, state: ButtonState) {
    app.world_mut()
        .resource_mut::<Messages<KeyboardInput>>()
        .write(KeyboardInput {
            key_code: key,
            logical_key: Key::Character("x".into()),
            state,
            text: None,
            repeat: false,
            window: Entity::PLACEHOLDER,
        });
}

fn release(app: &mut App, key: KeyCode) {
    send(app, key, ButtonState::Released);
}

fn reach_review(app: &mut App) -> usize {
    press(app, KeyCode::Space);
    pump(app, 2);
    assert_eq!(phase(app), MemoPhase::Stream);
    let total = app.world().resource::<MemoSession>().tokens.len();
    for _ in 0..total + 4 {
        step(app, 800);
    }
    pump(app, 2);
    assert_eq!(phase(app), MemoPhase::Recall);
    press(app, KeyCode::Space);
    pump(app, 2);
    assert_eq!(phase(app), MemoPhase::Review);
    total
}

fn stores_with(cadence: u32, mode: MemoMode) -> Stores {
    Stores {
        settings: Settings {
            memo_cadence_ms: cadence,
            memo_mode: mode,
            drill_reps: 6,
        },
        srs: SrsStore::default(),
        solves: SolveLog::default(),
        user_algs: UserAlgs::default(),
    }
}

fn fsrs() -> FsrsConfig {
    FsrsConfig::defaults()
}

fn memo_of_shape(pairs: (Vec<CaseId>, Vec<CaseId>), residue: Residue) -> Memo {
    Memo {
        edge_letters: Vec::new(),
        corner_letters: Vec::new(),
        pairs,
        buffers: (Vec::new(), Vec::new()),
        residue,
    }
}

const TOKEN_IMAGES: [&str; 8] = [
    "img0", "img1", "img2", "img3", "img4", "img5", "img6", "img7",
];

fn numbered_token(index: usize, orbit: Orbit) -> StreamToken {
    StreamToken {
        orbit,
        letters: [(b'A' + index as u8) as char, (b'Z' - index as u8) as char],
        syllable: Some(format!("s{index}")),
        image: Some(TOKEN_IMAGES[index]),
    }
}

fn eight_edge_tokens() -> Vec<StreamToken> {
    (0..8)
        .map(|index| numbered_token(index, Orbit::Edge))
        .collect()
}

#[test]
fn orbit_name_maps_both_orbits() {
    assert_eq!(orbit_name(Orbit::Edge), "edge");
    assert_eq!(orbit_name(Orbit::Corner), "corner");
}

#[test]
fn memo_session_defaults_are_inert() {
    let session = MemoSession::default();
    assert_eq!(session.seed, 0);
    assert_eq!(session.scramble, bt_core::Cube::solved());
    assert!(session.memo.is_none());
    assert!(session.tokens.is_empty());
    assert_eq!(session.shown, 0);
    assert_eq!(session.clock, 0.0);
}

#[test]
fn cadence_secs_converts_millis_to_seconds() {
    let session = MemoSession::default();
    assert_eq!(
        session.cadence_secs(&stores_with(800, MemoMode::PerOrbit)),
        0.8
    );
    assert_eq!(
        session.cadence_secs(&stores_with(1_250, MemoMode::PerOrbit)),
        1.25
    );
}

#[test]
fn reset_stream_zeroes_progress() {
    let mut session = MemoSession {
        shown: 5,
        clock: 3.2,
        ..Default::default()
    };
    session.reset_stream();
    assert_eq!(session.shown, 0);
    assert_eq!(session.clock, 0.0);
}

#[test]
fn rebuild_seeds_the_scramble_and_streams_every_pair() {
    let mut session = MemoSession {
        seed: 9,
        shown: 4,
        clock: 2.5,
        ..Default::default()
    };
    rebuild(&mut session, 42, &SyllableBook::default());
    let reference = memo_of(&random_state(42));
    assert_eq!(session.seed, 42);
    assert_eq!(session.scramble, random_state(42));
    assert_eq!(session.memo.as_ref(), Some(&reference));
    assert_eq!(
        session.tokens.len(),
        reference.pairs.0.len() + reference.pairs.1.len()
    );
    assert_eq!(session.shown, 0);
    assert_eq!(session.clock, 0.0);
}

#[test]
fn build_tokens_orders_edges_before_corners_and_maps_letters() {
    let memo = memo_of_shape(
        (
            vec![CaseId {
                orbit: Orbit::Edge,
                t1: 24,
                t2: 25,
            }],
            vec![CaseId {
                orbit: Orbit::Corner,
                t1: 0,
                t2: 1,
            }],
        ),
        Residue::default(),
    );
    let tokens = build_tokens(&memo, &SyllableBook::default());
    assert_eq!(tokens.len(), 2);
    assert_eq!(tokens[0].orbit, Orbit::Edge);
    assert_eq!(tokens[0].letters, ['A', 'B']);
    assert_eq!(tokens[1].orbit, Orbit::Corner);
    assert_eq!(tokens[1].letters, ['A', 'B']);
    assert!(tokens.iter().all(|token| token.syllable.is_none()));
}

#[test]
fn token_text_prefers_the_mode_source_and_falls_back_to_letters() {
    let rich = StreamToken {
        orbit: Orbit::Edge,
        letters: ['A', 'B'],
        syllable: Some("tả".to_string()),
        image: Some("Anthony Brooks"),
    };
    assert_eq!(token_text(&rich, MemoMode::Syllables), "tả");
    assert_eq!(token_text(&rich, MemoMode::Images), "Anthony Brooks");
    let bare = StreamToken {
        orbit: Orbit::Corner,
        letters: ['C', 'D'],
        syllable: None,
        image: None,
    };
    assert_eq!(token_text(&bare, MemoMode::Images), "CD");
    assert_eq!(token_text(&bare, MemoMode::Syllables), "CD");
}

#[test]
fn residue_lines_report_every_residue_kind_in_order() {
    let memo = memo_of_shape(
        (Vec::new(), Vec::new()),
        Residue {
            parity: Some(Parity {
                edge: (26, 24),
                corner: (1, 3),
            }),
            flips: vec![25, 27],
            twists: vec![Twist {
                corner: 2,
                quarter: 2,
            }],
        },
    );
    assert_eq!(
        residue_lines(&memo),
        vec![
            "parity UF-UB, UBR-UFL".to_string(),
            "flip UR & UL".to_string(),
            "twist UFR x2".to_string(),
        ]
    );
}

#[test]
fn residue_lines_drop_an_unpaired_trailing_flip() {
    let memo = memo_of_shape(
        (Vec::new(), Vec::new()),
        Residue {
            parity: None,
            flips: vec![25, 27, 24],
            twists: Vec::new(),
        },
    );
    assert_eq!(residue_lines(&memo), vec!["flip UR & UL".to_string()]);
}

#[test]
fn residue_lines_report_none_for_a_clean_memo() {
    let memo = memo_of_shape((Vec::new(), Vec::new()), Residue::default());
    assert_eq!(residue_lines(&memo), vec!["residue none".to_string()]);
}

#[test]
fn rate_pairs_write_one_review_per_pair_under_sheet_positions() {
    let mut stores = stores_with(800, MemoMode::PerOrbit);
    let session = MemoSession {
        memo: Some(memo_of_shape(
            (
                vec![CaseId {
                    orbit: Orbit::Edge,
                    t1: 24,
                    t2: 25,
                }],
                vec![CaseId {
                    orbit: Orbit::Corner,
                    t1: 0,
                    t2: 1,
                }],
            ),
            Residue::default(),
        )),
        ..Default::default()
    };
    rate_pairs(&mut stores, &fsrs(), &session, Rating::Good, 1_000);
    assert_eq!(stores.srs.cards.len(), 2);
    let edge = &stores.srs.cards["edge/UF-UB-UR"];
    assert_eq!(edge.reviews.len(), 1);
    assert_eq!(edge.reviews[0].quality, 4);
    assert_eq!(edge.reviews[0].ts_days, 1_000);
    assert_eq!(edge.reps, 1);
    let corner = &stores.srs.cards["corner/UFR-UBL-UBR"];
    assert_eq!(corner.reviews.len(), 1);
    assert_eq!(corner.reviews[0].quality, 4);
    assert_eq!(corner.reviews[0].scheduled_days, 3);
}

#[test]
fn rate_pairs_without_memo_write_nothing() {
    let mut stores = stores_with(800, MemoMode::PerOrbit);
    rate_pairs(
        &mut stores,
        &fsrs(),
        &MemoSession::default(),
        Rating::Easy,
        1_000,
    );
    assert!(stores.srs.cards.is_empty());
}

#[test]
fn intro_rows_show_seed_pair_count_mode_and_cadence() {
    let stores = stores_with(640, MemoMode::PerOrbit);
    let session = MemoSession {
        seed: 7,
        tokens: vec![numbered_token(0, Orbit::Edge); 3],
        ..Default::default()
    };
    let rows = view_rows(MemoPhase::Intro, &session, &stores);
    assert_eq!(rows.focal, "space to stream");
    assert_eq!(rows.recent, "");
    assert_eq!(
        rows.status,
        "seed 7  3 pairs  mode per-orbit  cadence 640 ms"
    );
}

#[test]
fn stream_rows_before_the_first_token_show_an_ellipsis() {
    let stores = stores_with(640, MemoMode::PerOrbit);
    let session = MemoSession {
        tokens: eight_edge_tokens(),
        shown: 0,
        ..Default::default()
    };
    let rows = view_rows(MemoPhase::Stream, &session, &stores);
    assert_eq!(rows.focal, "...");
    assert_eq!(rows.recent, "");
    assert_eq!(rows.status, "stream 0/8  cadence 640 ms");
}

#[test]
fn stream_rows_trim_recents_to_the_last_four_tokens() {
    let stores = stores_with(640, MemoMode::PerOrbit);
    let session = MemoSession {
        tokens: eight_edge_tokens(),
        shown: 8,
        ..Default::default()
    };
    let rows = view_rows(MemoPhase::Stream, &session, &stores);
    assert_eq!(rows.focal, "s7");
    assert_eq!(rows.recent, "s3 s4 s5 s6");
    assert_eq!(rows.status, "stream 8/8  cadence 640 ms");
}

#[test]
fn recall_rows_hide_every_token_and_count_the_stream() {
    let stores = stores_with(640, MemoMode::PerOrbit);
    let session = MemoSession {
        tokens: eight_edge_tokens(),
        shown: 5,
        ..Default::default()
    };
    let rows = view_rows(MemoPhase::Recall, &session, &stores);
    assert_eq!(rows.focal, "recall");
    assert_eq!(rows.recent, "");
    assert_eq!(rows.status, "5/8 streamed  space to reveal");
}

#[test]
fn review_rows_without_memo_list_only_the_rate_hint() {
    let stores = stores_with(640, MemoMode::PerOrbit);
    let session = MemoSession {
        tokens: eight_edge_tokens(),
        shown: 8,
        memo: None,
        ..Default::default()
    };
    let rows = view_rows(MemoPhase::Review, &session, &stores);
    assert_eq!(rows.focal, "rate 1..4");
    assert_eq!(rows.recent, "s0 s1 s2 s3 s4 s5 s6 s7");
    assert_eq!(rows.status, "r replay  space next  1..4 rate pairs");
}

#[test]
fn intro_space_draws_the_next_scramble() {
    let _guard = guard();
    let fixture = fixture("next-seed");
    let seed = 4_242u64;
    let mut app = enter_memo(clone_paths(&fixture), seed);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Stream);
    let session = app.world().resource::<MemoSession>();
    assert_eq!(session.seed, seed + 1);
    assert_eq!(session.scramble, random_state(seed + 1));
    let reference = memo_of(&random_state(seed + 1));
    assert_eq!(session.memo.as_ref(), Some(&reference));
}

#[test]
fn one_long_frame_reveals_one_token_per_elapsed_cadence() {
    let _guard = guard();
    let fixture = fixture("long-frame");
    let mut app = enter_memo(clone_paths(&fixture), 60_667);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Stream);
    assert_eq!(app.world().resource::<MemoSession>().shown, 0);
    step(&mut app, 1_700);
    assert_eq!(
        app.world().resource::<MemoSession>().shown,
        2,
        "1700ms at 800ms cadence reveals exactly 2 tokens"
    );
}

#[test]
fn zero_cadence_reveals_the_whole_stream_in_one_frame() {
    let _guard = guard();
    let fixture = fixture("zero-cadence");
    let mut app = enter_memo(clone_paths(&fixture), 555);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Stream);
    let total = app.world().resource::<MemoSession>().tokens.len();
    app.world_mut()
        .resource_mut::<Stores>()
        .settings
        .memo_cadence_ms = 0;
    pump(&mut app, 2);
    assert_eq!(app.world().resource::<MemoSession>().shown, total);
    assert_eq!(phase(&app), MemoPhase::Recall);
}

#[test]
fn an_empty_token_stream_never_reaches_recall() {
    let _guard = guard();
    let fixture = fixture("empty-stream");
    let mut app = enter_memo(clone_paths(&fixture), 909);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Stream);
    app.world_mut().resource_mut::<MemoSession>().tokens.clear();
    for _ in 0..3 {
        step(&mut app, 800);
    }
    assert_eq!(phase(&app), MemoPhase::Stream);
    assert_eq!(app.world().resource::<MemoSession>().shown, 0);
}

#[test]
fn review_replay_r_restarts_the_stream_from_zero() {
    let _guard = guard();
    let fixture = fixture("replay");
    let mut app = enter_memo(clone_paths(&fixture), 1_234);
    reach_review(&mut app);
    press(&mut app, KeyCode::KeyR);
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Stream);
    assert_eq!(app.world().resource::<MemoSession>().shown, 0);
    assert!(app.world().resource::<Stores>().srs.cards.is_empty());
    step(&mut app, 800);
    assert_eq!(app.world().resource::<MemoSession>().shown, 1);
}

#[test]
fn released_and_unmapped_keys_never_advance_the_flow() {
    let _guard = guard();
    let fixture = fixture("dead-keys");
    let seed = 311u64;
    let mut app = enter_memo(clone_paths(&fixture), seed);
    release(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Intro);
    assert_eq!(app.world().resource::<MemoSession>().seed, seed);
    reach_review(&mut app);
    release(&mut app, KeyCode::Digit2);
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Review);
    press(&mut app, KeyCode::KeyZ);
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Review);
    assert!(app.world().resource::<Stores>().srs.cards.is_empty());
    press(&mut app, KeyCode::Digit3);
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Intro);
    for card in app.world().resource::<Stores>().srs.cards.values() {
        assert_eq!(card.reviews.len(), 1);
        assert_eq!(card.reviews[0].quality, 4);
    }
}

#[test]
fn rating_again_lapses_every_pair_and_space_draws_the_next_seed() {
    let _guard = guard();
    let fixture = fixture("rate-again");
    let seed = 9_181u64;
    let mut app = enter_memo(clone_paths(&fixture), seed);
    let total = reach_review(&mut app);
    assert_eq!(app.world().resource::<MemoSession>().seed, seed + 1);
    press(&mut app, KeyCode::Digit1);
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Intro);
    let stores = app.world().resource::<Stores>();
    assert_eq!(stores.srs.cards.len(), total);
    for card in stores.srs.cards.values() {
        assert_eq!(card.reviews.len(), 1);
        assert_eq!(card.reviews[0].quality, 2);
        assert_eq!(card.reps, 1);
        assert_eq!(card.lapses, 1);
    }
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Stream);
    let session = app.world().resource::<MemoSession>();
    assert_eq!(session.seed, seed + 2);
    assert_eq!(session.scramble, random_state(seed + 2));
}
