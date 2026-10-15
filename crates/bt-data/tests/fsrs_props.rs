use bt_data::fsrs::{DEFAULT_WEIGHTS, FsrsConfig, Grade, MemoryState, next_interval_days};
use bt_data::srs::ConfigError;
use proptest::prelude::*;

fn golden_config() -> FsrsConfig {
    FsrsConfig {
        weights: DEFAULT_WEIGHTS,
        request_retention: 0.9,
        interval_clamp_days: [1, 36500],
    }
}

fn app_config() -> FsrsConfig {
    FsrsConfig {
        weights: DEFAULT_WEIGHTS,
        request_retention: 0.9,
        interval_clamp_days: [1, 365],
    }
}

#[test]
fn weight_count_is_nineteen() {
    assert_eq!(DEFAULT_WEIGHTS.len(), 19);
    assert_eq!(Grade::ALL.len(), 4);
}

fn hub_section_text(weights: &str, retention: &str, clamps: &str) -> String {
    format!(
        "{{\"srs\":{{\"fsrs\":{{\"weights\":{weights},\"request_retention\":{retention},\"interval_clamp_days\":{clamps}}}}}}}"
    )
}

fn approved_json() -> String {
    hub_section_text(
        "[0.40255, 1.18385, 3.173, 15.69105, 7.1949, 0.5345, 1.4604, 0.0046, 1.54575, 0.1192, 1.01925, 1.9395, 0.11, 0.29605, 2.2698, 0.2315, 2.9898, 0.51655, 0.6621]",
        "0.9",
        "[1, 365]",
    )
}

fn config_from_text(text: &str) -> Result<FsrsConfig, ConfigError> {
    let value: serde_json::Value = serde_json::from_str(text).unwrap_or(serde_json::Value::Null);
    FsrsConfig::from_value(&value)
}

#[test]
fn from_value_parses_the_approved_hub_section() {
    let cfg = config_from_text(&approved_json()).expect("approved section parses");
    assert_eq!(cfg.weights, DEFAULT_WEIGHTS);
    assert_eq!(cfg.request_retention, 0.9);
    assert_eq!(cfg.interval_clamp_days, [1, 365]);
}

#[test]
fn from_value_rejects_missing_or_malformed_sections() {
    assert!(matches!(
        FsrsConfig::from_value(&serde_json::json!({})),
        Err(ConfigError::Missing("srs.fsrs"))
    ));
    assert!(matches!(
        FsrsConfig::from_value(&serde_json::json!({"srs": {}})),
        Err(ConfigError::Missing("srs.fsrs"))
    ));
    let short_weights = hub_section_text("[0.40255]", "0.9", "[1, 365]");
    assert!(matches!(
        config_from_text(&short_weights),
        Err(ConfigError::Malformed(_))
    ));
    let one_clamp = hub_section_text(&format!("{:?}", DEFAULT_WEIGHTS), "0.9", "[1]");
    assert!(matches!(
        config_from_text(&one_clamp),
        Err(ConfigError::Malformed(_))
    ));
    let text_weights = hub_section_text(
        "[\"x\", 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0]",
        "0.9",
        "[1, 365]",
    );
    assert!(matches!(
        config_from_text(&text_weights),
        Err(ConfigError::Malformed(_))
    ));
}

#[test]
fn from_value_rejects_non_finite_and_out_of_range_retention() {
    for retention in ["1e999", "-1e999", "0.0", "-0.5", "1.5", "2"] {
        let text = hub_section_text(&format!("{:?}", DEFAULT_WEIGHTS), retention, "[1, 365]");
        assert!(
            config_from_text(&text).is_err(),
            "retention {retention} must be rejected"
        );
    }
}

#[test]
fn from_value_rejects_non_finite_weights_end_to_end() {
    let mut weights = format!("{:?}", DEFAULT_WEIGHTS);
    weights.replace_range(0..8, "[1e999,");
    let text = hub_section_text(&weights, "0.9", "[1, 365]");
    assert!(config_from_text(&text).is_err());
    let mut negative = format!("{:?}", DEFAULT_WEIGHTS);
    negative.replace_range(0..8, "[-1e999,");
    let text = hub_section_text(&negative, "0.9", "[1, 365]");
    assert!(config_from_text(&text).is_err());
}

#[test]
fn from_value_rejects_bad_interval_clamps() {
    for clamps in ["[0, 365]", "[10, 5]", "[-1, 365]", "[100, 99]"] {
        let text = hub_section_text(&format!("{:?}", DEFAULT_WEIGHTS), "0.9", clamps);
        let outcome = config_from_text(&text);
        assert!(
            matches!(outcome, Err(ConfigError::Invalid(_))),
            "clamps {clamps} must be Invalid, got {outcome:?}"
        );
    }
    let wide = hub_section_text(&format!("{:?}", DEFAULT_WEIGHTS), "0.9", "[365, 3650]");
    let cfg = config_from_text(&wide).expect("raised floor with wide cap parses");
    assert_eq!(cfg.interval_clamp_days, [365, 3650]);
}

#[test]
fn from_value_accepts_retention_one_and_equal_clamp_bounds() {
    let retention_one = hub_section_text(&format!("{:?}", DEFAULT_WEIGHTS), "1.0", "[1, 365]");
    let cfg = config_from_text(&retention_one).expect("retention 1.0 is inside (0, 1]");
    assert_eq!(cfg.request_retention, 1.0);
    let equal_clamps = hub_section_text(&format!("{:?}", DEFAULT_WEIGHTS), "0.9", "[7, 7]");
    let cfg = config_from_text(&equal_clamps).expect("equal bounds satisfy cap at least floor");
    assert_eq!(cfg.interval_clamp_days, [7, 7]);
}

#[test]
fn state_validity_rejects_non_finite_and_out_of_domain_values() {
    let cfg = golden_config();
    for grade in Grade::ALL {
        assert!(MemoryState::initial(grade, &cfg).is_valid());
    }
    let bad_stability = [0.0, -1.0, f64::INFINITY, f64::NAN, -f64::INFINITY];
    for stability in bad_stability {
        assert!(
            !MemoryState {
                stability,
                difficulty: 5.0
            }
            .is_valid(),
            "stability {stability} must be invalid"
        );
    }
    let bad_difficulty = [f64::NAN, f64::INFINITY, 0.5, 10.5, -1.0];
    for difficulty in bad_difficulty {
        assert!(
            !MemoryState {
                stability: 3.0,
                difficulty
            }
            .is_valid(),
            "difficulty {difficulty} must be invalid"
        );
    }
}

#[test]
fn config_error_messages_are_distinct() {
    assert_eq!(
        ConfigError::Missing("srs.fsrs").to_string(),
        "config section missing: srs.fsrs"
    );
    let malformed = config_from_text(&hub_section_text("[0.40255]", "0.9", "[1, 365]"))
        .expect_err("short weights are malformed");
    assert!(
        malformed
            .to_string()
            .starts_with("config section malformed: "),
        "err: {malformed}"
    );
    assert_eq!(
        ConfigError::Invalid("weights must be finite").to_string(),
        "config section invalid: weights must be finite"
    );
}

#[test]
fn defaults_expose_the_reference_settings() {
    let cfg = FsrsConfig::defaults();
    assert_eq!(cfg.weights, DEFAULT_WEIGHTS);
    assert_eq!(cfg.request_retention, 0.9);
    assert_eq!(cfg.interval_clamp_days, [1, 36500]);
    assert_eq!(cfg, golden_config());
}

#[test]
fn initial_state_uses_the_grade_weight_and_clamps_difficulty() {
    let cfg = golden_config();
    let cases = [
        (Grade::Again, 0.40255, 7.1949),
        (Grade::Hard, 1.18385, 6.488305268471453),
        (Grade::Good, 3.173, 5.282434422319005),
        (Grade::Easy, 15.69105, 3.2245015893713678),
    ];
    for (grade, stability, difficulty) in cases {
        let state = MemoryState::initial(grade, &cfg);
        assert!((state.stability - stability).abs() < 1e-12, "{grade:?}");
        assert!((state.difficulty - difficulty).abs() < 1e-12, "{grade:?}");
    }
}

#[test]
fn initial_state_floors_stability_and_clamps_difficulty_bounds() {
    let mut cfg = golden_config();
    cfg.weights[0] = 0.01;
    cfg.weights[4] = 99.0;
    let low = MemoryState::initial(Grade::Again, &cfg);
    assert_eq!(low.stability, 0.1);
    assert_eq!(low.difficulty, 10.0);
    cfg.weights[4] = -99.0;
    cfg.weights[5] = 0.0;
    let high = MemoryState::initial(Grade::Easy, &cfg);
    assert_eq!(high.difficulty, 1.0);
}

#[test]
fn retrievability_is_one_at_zero_elapsed_and_decays_with_time() {
    let cfg = golden_config();
    let state = MemoryState::initial(Grade::Good, &cfg);
    assert_eq!(state.retrievability(0.0), 1.0);
    let integer_stability = MemoryState {
        stability: 3.0,
        difficulty: state.difficulty,
    };
    let at_stability = integer_stability.retrievability(3.0);
    assert!((at_stability - 0.9).abs() < 1e-12, "R(3, 3) is 0.9");
    assert!(state.retrievability(100.0) < at_stability);
    assert!(state.retrievability(100.0) > 0.0);
    assert!(state.retrievability(1_000_000.0) > 0.0);
}

#[test]
fn negative_elapsed_clamps_to_full_retrievability() {
    let cfg = golden_config();
    let state = MemoryState::initial(Grade::Good, &cfg);
    assert_eq!(state.retrievability(-5.0), 1.0);
}

#[test]
fn same_day_reviews_scale_stability_without_retrievability_input() {
    let cfg = golden_config();
    let state = MemoryState::initial(Grade::Good, &cfg);
    let again = state.review(Grade::Again, 0.0, &cfg);
    let hard = state.review(Grade::Hard, 0.0, &cfg);
    let good = state.review(Grade::Good, 0.0, &cfg);
    let easy = state.review(Grade::Easy, 0.0, &cfg);
    let scale =
        |grade: Grade| (cfg.weights[17] * ((grade as i32 as f64) - 3.0 + cfg.weights[18])).exp();
    assert!((again.stability - state.stability * scale(Grade::Again)).abs() < 1e-9);
    assert!((hard.stability - state.stability * scale(Grade::Hard)).abs() < 1e-9);
    assert!((good.stability - state.stability * scale(Grade::Good)).abs() < 1e-9);
    assert!((easy.stability - state.stability * scale(Grade::Easy)).abs() < 1e-9);
}

#[test]
fn long_gap_good_raises_stability_and_forget_lapses_drop_it() {
    let cfg = golden_config();
    let state = MemoryState::initial(Grade::Good, &cfg);
    let reviewed = state.review(Grade::Good, 7.0, &cfg);
    assert!(reviewed.stability > state.stability);
    let lapsed = reviewed.review(Grade::Again, 30.0, &cfg);
    assert!(lapsed.stability < reviewed.stability);
    assert!(lapsed.stability > 0.0);
}

#[test]
fn post_lapse_stability_never_exceeds_the_short_term_ceiling() {
    let cfg = golden_config();
    let state = MemoryState {
        stability: 400.0,
        difficulty: 1.0,
    };
    let lapsed = state.review(Grade::Again, 1000.0, &cfg);
    let ceiling = 400.0 / (cfg.weights[17] * cfg.weights[18]).exp();
    assert!(lapsed.stability <= ceiling + 1e-9);
}

#[test]
fn difficulty_stays_inside_bounds_under_extreme_walks() {
    let cfg = golden_config();
    let mut state = MemoryState::initial(Grade::Again, &cfg);
    for _ in 0..50 {
        state = state.review(Grade::Again, 1.0, &cfg);
        assert!((1.0..=10.0).contains(&state.difficulty));
    }
    let mut easy = MemoryState::initial(Grade::Easy, &cfg);
    for _ in 0..50 {
        easy = easy.review(Grade::Easy, 1.0, &cfg);
        assert!((1.0..=10.0).contains(&easy.difficulty));
    }
}

#[test]
fn interval_clamps_at_both_bounds() {
    let cfg = app_config();
    assert_eq!(next_interval_days(0.01, &cfg), 1);
    assert_eq!(next_interval_days(1_000_000.0, &cfg), 365);
    assert_eq!(next_interval_days(120.4, &cfg), 120);
    let wide = golden_config();
    assert_eq!(next_interval_days(1_000_000.0, &wide), 36500);
}

#[test]
fn interval_respects_a_raised_floor_and_lets_the_cap_win_last() {
    let cfg = FsrsConfig {
        weights: DEFAULT_WEIGHTS,
        request_retention: 0.9,
        interval_clamp_days: [7, 365],
    };
    assert_eq!(next_interval_days(0.01, &cfg), 7);
    assert_eq!(next_interval_days(3.0, &cfg), 7);
    assert_eq!(next_interval_days(10.0, &cfg), 10);
    let inverted = FsrsConfig {
        weights: DEFAULT_WEIGHTS,
        request_retention: 0.9,
        interval_clamp_days: [10, 5],
    };
    assert_eq!(next_interval_days(3.0, &inverted), 5);
    assert_eq!(next_interval_days(400.0, &inverted), 5);
}

#[test]
fn interval_rounds_ties_to_even_like_the_reference() {
    let cfg = golden_config();
    assert_eq!(next_interval_days(2.5, &cfg), 2);
    assert_eq!(next_interval_days(3.5, &cfg), 4);
    assert_eq!(next_interval_days(8.5, &cfg), 8);
    assert_eq!(next_interval_days(9.5, &cfg), 10);
}

#[test]
fn fractional_elapsed_floors_to_whole_days_like_the_reference() {
    let cfg = golden_config();
    let state = MemoryState::initial(Grade::Good, &cfg);
    let same_day = state.review(Grade::Good, 0.0, &cfg);
    for elapsed in [0.0, 0.5, -0.5] {
        let next = state.review(Grade::Good, elapsed, &cfg);
        assert_eq!(next, same_day, "elapsed {elapsed} must act as same-day");
        assert_eq!(state.retrievability(elapsed), 1.0);
    }
    let one_day = state.review(Grade::Good, 1.0, &cfg);
    let one_and_half = state.review(Grade::Good, 1.5, &cfg);
    assert_eq!(one_and_half, one_day);
    assert!((one_day.stability - 5.869141822641378).abs() < 1e-12);
    let nearly_three = state.review(Grade::Good, 2.9, &cfg);
    assert!((nearly_three.stability - 8.384375357393525).abs() < 1e-12);
    assert_eq!(next_interval_days(nearly_three.stability, &cfg), 8);
}

#[test]
fn config_and_state_round_trip_through_serde_json() {
    let cfg = golden_config();
    let text = serde_json::to_string_pretty(&cfg).unwrap();
    let back: FsrsConfig = serde_json::from_str(&text).unwrap();
    assert_eq!(back, cfg);
    assert!(text.contains("\"request_retention\""));
    let state = MemoryState::initial(Grade::Good, &cfg);
    let state_text = serde_json::to_string(&state).unwrap();
    let state_back: MemoryState = serde_json::from_str(&state_text).unwrap();
    assert_eq!(state_back, state);
    let weights_text = serde_json::to_string(&DEFAULT_WEIGHTS).unwrap();
    let weights_back: [f64; 19] = serde_json::from_str(&weights_text).unwrap();
    assert_eq!(weights_back, DEFAULT_WEIGHTS);
}

proptest! {
    #[test]
    fn good_reviews_grow_stability_monotonically(
        first in 1u8..=4,
        elapsed in 1f64..3650.0,
        rounds in 1usize..10,
    ) {
        let cfg = golden_config();
        let grade = match first { 1 => Grade::Again, 2 => Grade::Hard, 3 => Grade::Good, _ => Grade::Easy };
        let mut state = MemoryState::initial(grade, &cfg);
        for _ in 0..rounds {
            let next = state.review(Grade::Good, elapsed, &cfg);
            prop_assert!(next.stability > state.stability);
            state = next;
        }
    }

    #[test]
    fn intervals_respect_clamps_and_grow_with_stability(
        stability_a in 0.1f64..100.0,
        growth in 1.0f64..10.0,
        retention in 0.7f64..0.99,
        hi in 30i64..36500,
    ) {
        let cfg = FsrsConfig { weights: DEFAULT_WEIGHTS, request_retention: retention, interval_clamp_days: [1, hi] };
        let stability_b = stability_a * growth;
        let ivl_a = next_interval_days(stability_a, &cfg);
        let ivl_b = next_interval_days(stability_b, &cfg);
        prop_assert!(ivl_a >= 1);
        prop_assert!(ivl_a <= hi);
        prop_assert!(ivl_b >= ivl_a);
        prop_assert!(ivl_b <= hi);
    }

    #[test]
    fn random_walks_keep_state_finite_and_in_bounds(
        seed_grade in 1u8..=4,
        ratings in prop::collection::vec(1u8..=4, 1..40),
        gaps in prop::collection::vec(0f64..3650.0, 1..40),
    ) {
        let cfg = golden_config();
        let to_grade = |code: u8| match code { 1 => Grade::Again, 2 => Grade::Hard, 3 => Grade::Good, _ => Grade::Easy };
        let mut state = MemoryState::initial(to_grade(seed_grade), &cfg);
        for (i, &code) in ratings.iter().enumerate() {
            state = state.review(to_grade(code), gaps[i % gaps.len()], &cfg);
            prop_assert!(state.stability.is_finite());
            prop_assert!(state.stability > 0.0);
            prop_assert!((1.0..=10.0).contains(&state.difficulty));
            prop_assert!(state.is_valid());
            prop_assert!(state.retrievability(gaps[i % gaps.len()]) <= 1.0);
        }
    }
}
