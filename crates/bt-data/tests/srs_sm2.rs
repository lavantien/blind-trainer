mod common;

use bt_data::srs::{Card, Rating, Sm2Config};
use common::config_path;
use proptest::prelude::*;

fn fixture_config() -> Sm2Config {
    let text = std::fs::read_to_string(config_path()).expect("config file");
    let value: serde_json::Value = serde_json::from_str(&text).expect("config json");
    Sm2Config::from_value(&value).expect("srs section")
}

fn run(card: &mut Card, cfg: &Sm2Config, ratings: &[Rating], day: i64, step: i64) {
    for (i, rating) in ratings.iter().enumerate() {
        card.review(*rating, day + step * i as i64, cfg);
    }
}

#[test]
fn config_fixture_carries_expected_sm2_values() {
    let cfg = fixture_config();
    assert_eq!(cfg.quality(Rating::Again), 2);
    assert_eq!(cfg.quality(Rating::Hard), 3);
    assert_eq!(cfg.quality(Rating::Good), 4);
    assert_eq!(cfg.quality(Rating::Easy), 5);
    assert_eq!(cfg.interval_min_days(), 1);
    assert_eq!(cfg.interval_max_days(), 365);
}

#[test]
fn from_value_rejects_missing_or_bad_sections() {
    assert!(Sm2Config::from_value(&serde_json::json!({})).is_err());
    assert!(Sm2Config::from_value(&serde_json::json!({"srs": {}})).is_err());
    let one_bound = serde_json::json!({
        "srs": {"sm2": {"again": 2, "hard": 3, "good": 4, "easy": 5, "interval_clamp_days": [10]}}
    });
    assert!(Sm2Config::from_value(&one_bound).is_err());
    let valid = serde_json::json!({
        "srs": {"sm2": {"again": 2, "hard": 3, "good": 4, "easy": 5, "interval_clamp_days": [1, 365]}}
    });
    assert!(Sm2Config::from_value(&valid).is_ok());
}

#[test]
fn quality_mapping_comes_from_the_config_not_the_code() {
    let cfg = Sm2Config::from_value(&serde_json::json!({"srs": {"sm2": {
        "again": 1, "hard": 2, "good": 4, "easy": 5, "interval_clamp_days": [1, 10]
    }}}))
    .unwrap();
    assert_eq!(cfg.quality(Rating::Again), 1);
    assert_eq!(cfg.quality(Rating::Hard), 2);
    assert_eq!(cfg.interval_max_days(), 10);
}

#[test]
fn new_card_starts_at_default_ease_due_immediately() {
    let card = Card::new(100);
    assert_eq!(card.ease, 2.5);
    assert_eq!(card.interval_days, 0);
    assert_eq!(card.due_day, 100);
    assert_eq!(card.reps, 0);
    assert_eq!(card.lapses, 0);
    assert!(card.reviews.is_empty());
    assert!(card.is_due(100));
    assert!(!card.is_due(99));
}

#[test]
fn first_good_review_schedules_one_day_out() {
    let cfg = fixture_config();
    let mut card = Card::new(0);
    card.review(Rating::Good, 0, &cfg);
    assert_eq!(card.interval_days, 1);
    assert_eq!(card.due_day, 1);
    assert_eq!(card.reps, 1);
    assert_eq!(card.lapses, 0);
    assert_eq!(card.ease, 2.5);
    assert_eq!(card.reviews.len(), 1);
    assert_eq!(
        card.reviews[0],
        bt_data::srs::ReviewLogEntry {
            ts_days: 0,
            quality: 4,
            elapsed_days: 0,
            scheduled_days: 1
        }
    );
}

#[test]
fn good_ladder_is_one_six_fifteen_thirty_eight() {
    let cfg = fixture_config();
    let mut card = Card::new(0);
    run(&mut card, &cfg, &[Rating::Good; 4], 0, 1);
    let intervals: Vec<i64> = card.reviews.iter().map(|e| e.scheduled_days).collect();
    assert_eq!(intervals, vec![1, 6, 15, 38]);
}

#[test]
fn easy_ladder_grows_and_caps_at_the_configured_ceiling() {
    let cfg = fixture_config();
    let mut card = Card::new(0);
    run(&mut card, &cfg, &[Rating::Easy; 7], 0, 1);
    let intervals: Vec<i64> = card.reviews.iter().map(|e| e.scheduled_days).collect();
    assert_eq!(intervals, vec![1, 6, 16, 45, 130, 365, 365]);
    assert_eq!(card.interval_days, 365);
    assert!((card.ease - 3.2).abs() < 1e-4);
}

#[test]
fn again_resets_interval_but_keeps_ease() {
    let cfg = fixture_config();
    let mut card = Card::new(0);
    run(&mut card, &cfg, &[Rating::Good, Rating::Good], 0, 1);
    assert_eq!(card.interval_days, 6);
    let ease_before = card.ease;
    card.review(Rating::Again, 2, &cfg);
    assert_eq!(card.interval_days, 1);
    assert_eq!(card.due_day, 3);
    assert_eq!(card.reps, 0);
    assert_eq!(card.lapses, 1);
    assert_eq!(card.ease, ease_before);
    assert_eq!(
        card.reviews.last().unwrap(),
        &bt_data::srs::ReviewLogEntry {
            ts_days: 2,
            quality: 2,
            elapsed_days: 1,
            scheduled_days: 1
        }
    );
    card.review(Rating::Good, 3, &cfg);
    assert_eq!(card.interval_days, 1);
    assert_eq!(card.reps, 1);
}

#[test]
fn hard_never_resets_and_uses_one_six_then_prior_times_ease() {
    let cfg = fixture_config();
    let mut card = Card::new(0);
    run(
        &mut card,
        &cfg,
        &[Rating::Hard, Rating::Hard, Rating::Hard],
        0,
        1,
    );
    let intervals: Vec<i64> = card.reviews.iter().map(|e| e.scheduled_days).collect();
    assert_eq!(intervals, vec![1, 6, 13]);
    assert_eq!(card.lapses, 0);
    assert_eq!(card.reps, 3);
    assert!((card.ease - (2.5 - 0.14f32 * 3.0)).abs() < 1e-5);
}

#[test]
fn ease_floor_is_enforced_under_repeated_hard() {
    let cfg = fixture_config();
    let mut card = Card::new(0);
    for round in 0..20 {
        card.review(Rating::Hard, round, &cfg);
        assert!(card.ease >= 1.3, "ease {} at round {}", card.ease, round);
    }
    assert!((card.ease - 1.3).abs() < 1e-5);
}

#[test]
fn overdue_reviews_record_real_elapsed_time() {
    let cfg = fixture_config();
    let mut card = Card::new(0);
    card.review(Rating::Good, 0, &cfg);
    card.review(Rating::Good, 10, &cfg);
    let second = &card.reviews[1];
    assert_eq!(second.elapsed_days, 10);
    assert_eq!(second.ts_days, 10);
    assert_eq!(second.scheduled_days, 6);
    assert_eq!(card.due_day, 16);
}

#[test]
fn reviews_never_go_back_in_time() {
    let cfg = fixture_config();
    let mut card = Card::new(50);
    card.review(Rating::Good, 40, &cfg);
    assert_eq!(card.reviews[0].elapsed_days, 0);
    card.review(Rating::Good, 45, &cfg);
    assert_eq!(card.reviews[1].elapsed_days, 5);
}

#[test]
fn card_round_trips_through_serde_json() {
    let cfg = fixture_config();
    let mut card = Card::new(7);
    run(
        &mut card,
        &cfg,
        &[Rating::Good, Rating::Easy, Rating::Again, Rating::Hard],
        7,
        3,
    );
    let text = serde_json::to_string_pretty(&card).unwrap();
    let back: Card = serde_json::from_str(&text).unwrap();
    assert_eq!(back, card);
    assert!(text.contains("\"elapsed_days\""));
    assert!(text.contains("\"scheduled_days\""));
    assert!(text.contains("\"ts_days\""));
    assert!(text.contains("\"quality\""));
}

proptest! {
    #[test]
    fn invariants_hold_over_random_histories(
        ratings in prop::collection::vec(0u8..4, 1..40),
        start in 0i64..10_000,
        gaps in prop::collection::vec(0i64..30, 1..40),
    ) {
        let cfg = fixture_config();
        let mut card = Card::new(start);
        let mut day = start;
        for (i, &code) in ratings.iter().enumerate() {
            let rating = match code { 0 => Rating::Again, 1 => Rating::Hard, 2 => Rating::Good, _ => Rating::Easy };
            day += gaps[i % gaps.len()];
            card.review(rating, day, &cfg);
            prop_assert!(card.interval_days >= cfg.interval_min_days());
            prop_assert!(card.interval_days <= cfg.interval_max_days());
            prop_assert!(card.ease >= 1.3);
            prop_assert_eq!(card.due_day, day + card.interval_days);
            prop_assert_eq!(card.reviews.len(), i + 1);
            let entry = card.reviews.last().unwrap();
            prop_assert_eq!(entry.ts_days, day);
            prop_assert_eq!(entry.scheduled_days, card.interval_days);
            prop_assert_eq!(entry.quality, cfg.quality(rating));
            if rating == Rating::Again {
                prop_assert_eq!(card.interval_days, cfg.interval_min_days());
            }
        }
    }
}
