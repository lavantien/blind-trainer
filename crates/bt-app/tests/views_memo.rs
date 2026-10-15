#[allow(dead_code)]
mod views_common;

use bevy::prelude::*;
use bt_app::persist::MemoMode;
use bt_app::states::{AppState, MemoPhase};
use bt_app::views::memo::{MemoSeed, MemoSession, token_text};
use bt_scheme::Orbit;
use bt_scheme::memo::memo as memo_of;
use bt_scheme::random_state;
use views_common::*;

fn phase(app: &App) -> MemoPhase {
    *app.world().resource::<State<MemoPhase>>().get()
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

#[test]
fn memo_streaming_respects_the_configured_cadence() {
    let _guard = guard();
    let fixture = fixture("cadence");
    let mut app = enter_memo(clone_paths(&fixture), 12_345);
    let total = app.world().resource::<MemoSession>().tokens.len();
    assert!(total >= 4, "random memo has several pairs, got {total}");

    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Stream);
    assert_eq!(app.world().resource::<MemoSession>().shown, 0);

    step(&mut app, 300);
    step(&mut app, 300);
    assert_eq!(
        app.world().resource::<MemoSession>().shown,
        0,
        "600ms must not yet reveal at 800ms cadence"
    );
    step(&mut app, 300);
    assert_eq!(
        app.world().resource::<MemoSession>().shown,
        1,
        "900ms reveals exactly one token"
    );
    step(&mut app, 800);
    assert_eq!(app.world().resource::<MemoSession>().shown, 2);
}

#[test]
fn memo_tokens_fall_back_to_letters_without_syllables_json() {
    let _guard = guard();
    let fixture = fixture("fallback");
    let app = enter_memo(clone_paths(&fixture), 777);
    let session = app.world().resource::<MemoSession>();
    assert!(!session.tokens.is_empty());
    for token in &session.tokens {
        let letters: String = token.letters.iter().collect();
        assert!(
            token.syllable.is_none(),
            "no syllables.json in fixture, pair {letters} must not resolve"
        );
    }
}

#[test]
fn memo_tokens_use_syllables_json_when_present() {
    let _guard = guard();
    let fixture = fixture("syllables");
    let data_ga = fixture.base.join("data-ga");
    std::fs::write(
        data_ga.join("syllables.json"),
        serde_json::json!({
            "cases": [
                {"orbit": "edge", "letters": "AB", "syllable": "tả"},
                {"orbit": "edge", "letters": "AD", "syllable": ""}
            ]
        })
        .to_string(),
    )
    .expect("syllables fixture");
    let app = enter_memo(clone_paths(&fixture), 777);
    let book = app.world().resource::<bt_app::views::SyllableBook>();
    assert!(book.loaded);
    assert_eq!(
        book.syllable_for("edge", ['A', 'B']),
        Some("tả"),
        "populated entries resolve"
    );
    assert_eq!(
        book.syllable_for("edge", ['A', 'D']),
        None,
        "empty syllables are dropped at load"
    );
    assert_eq!(book.syllable_for("corner", ['A', 'B']), None);
}

#[test]
fn memo_rating_writes_srs_reviews_for_every_pair() {
    let _guard = guard();
    let fixture = fixture("rate");
    let seed = 12_345u64;
    let mut app = enter_memo(clone_paths(&fixture), seed);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Stream);
    let pair_count = app.world().resource::<MemoSession>().tokens.len();

    let steps = pair_count + 4;
    for _ in 0..steps {
        step(&mut app, 800);
    }
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Recall);
    press(&mut app, KeyCode::Space);
    pump(&mut app, 2);
    assert_eq!(phase(&app), MemoPhase::Review);

    press(&mut app, KeyCode::Digit2);
    pump(&mut app, 2);
    let stores = app.world().resource::<bt_app::persist::Stores>();
    assert_eq!(stores.srs.cards.len(), pair_count);
    for card in stores.srs.cards.values() {
        assert_eq!(card.reviews.len(), 1);
        assert_eq!(card.reviews[0].quality, 3);
    }
    assert_eq!(phase(&app), MemoPhase::Intro, "rating starts the next memo");
}

fn clone_paths(fixture: &Fixture) -> RuntimePaths {
    RuntimePaths {
        config_path: fixture.paths.config_path.clone(),
        vendor_root: fixture.paths.vendor_root.clone(),
        user_root: fixture.paths.user_root.clone(),
        assets_root: fixture.paths.assets_root.clone(),
    }
}

fn seed_with_edge_pair(first: char, second: char) -> u64 {
    for seed in 0..5_000u64 {
        let memo = memo_of(&random_state(seed));
        if memo
            .pairs
            .0
            .iter()
            .any(|case| case.letters() == (first, second))
        {
            return seed;
        }
    }
    panic!("no seed produced edge pair {first}{second}");
}

#[test]
fn memo_per_orbit_dispatches_edges_to_syllables_and_corners_to_images() {
    let _guard = guard();
    let fixture = fixture("per-orbit");
    std::fs::write(
        fixture.base.join("data-ga").join("syllables.json"),
        serde_json::json!({
            "cases": [
                {"orbit": "edge", "letters": "AB", "syllable": "tả"}
            ]
        })
        .to_string(),
    )
    .expect("syllables fixture");
    let seed = seed_with_edge_pair('A', 'B');
    let app = enter_memo(clone_paths(&fixture), seed);
    let session = app.world().resource::<MemoSession>();
    let ab = session
        .tokens
        .iter()
        .find(|token| token.orbit == Orbit::Edge && token.letters == ['A', 'B'])
        .expect("edge AB token");
    assert_eq!(token_text(ab, MemoMode::PerOrbit), "tả");
    for token in &session.tokens {
        let letters: String = token.letters.iter().collect();
        let expected = match token.orbit {
            Orbit::Edge => token.syllable.clone().unwrap_or(letters.clone()),
            Orbit::Corner => token.image.map(str::to_string).unwrap_or(letters.clone()),
        };
        assert_eq!(
            token_text(token, MemoMode::PerOrbit),
            expected,
            "per-orbit text for pair {letters}"
        );
    }
}

#[test]
fn config_per_orbit_mode_and_default_reps_seed_the_settings() {
    let _guard = guard();
    let fixture = fixture("per-orbit-config");
    let data_ga = fixture.base.join("data-ga");
    std::fs::write(
        fixture.paths.config_path.clone(),
        serde_json::json!({
            "app": {
                "window": {"width": 640, "height": 480, "title": "views"},
                "memo": {"cadence_ms": 800, "mode": "per-orbit"},
                "drill": {"reps": 6}
            },
            "srs": {"fsrs": {"weights": [0.40255, 1.18385, 3.173, 15.69105, 7.1949, 0.5345, 1.4604, 0.0046, 1.54575, 0.1192, 1.01925, 1.9395, 0.11, 0.29605, 2.2698, 0.2315, 2.9898, 0.51655, 0.6621], "request_retention": 0.9, "interval_clamp_days": [1, 365]}},
            "paths": {"data_ga": data_ga.to_string_lossy()}
        })
        .to_string(),
    )
    .expect("overwrite config");
    let mut app = headless_app(clone_paths(&fixture));
    boot_to_menu(&mut app);
    let stores = app.world().resource::<bt_app::persist::Stores>();
    assert_eq!(stores.settings.memo_mode, MemoMode::PerOrbit);
    assert_eq!(stores.settings.drill_reps, 6);
}

#[test]
fn stored_settings_load_per_orbit_and_legacy_modes() {
    let _guard = guard();
    for (tag, text, mode) in [
        ("per-orbit", "per-orbit", MemoMode::PerOrbit),
        ("legacy-syllables", "syllables", MemoMode::Syllables),
        ("legacy-images", "images", MemoMode::Images),
    ] {
        let fixture = fixture(tag);
        let settings_dir = fixture.paths.user_root.join("settings");
        std::fs::create_dir_all(&settings_dir).expect("settings dir");
        std::fs::write(
            settings_dir.join("settings.json"),
            serde_json::json!({
                "memo_cadence_ms": 640,
                "memo_mode": text,
                "drill_reps": 6
            })
            .to_string(),
        )
        .expect("settings file");
        let mut app = headless_app(clone_paths(&fixture));
        boot_to_menu(&mut app);
        let stores = app.world().resource::<bt_app::persist::Stores>();
        assert_eq!(stores.settings.memo_mode, mode, "stored mode {text}");
        assert_eq!(stores.settings.drill_reps, 6);
    }
}
