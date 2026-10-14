use bt_data::srs::{Card, Rating, Sm2Config};

fn config(qualities: [u32; 4], clamp: [i64; 2]) -> Sm2Config {
    let [again, hard, good, easy] = qualities;
    Sm2Config::from_value(&serde_json::json!({
        "srs": {"sm2": {
            "again": again,
            "hard": hard,
            "good": good,
            "easy": easy,
            "interval_clamp_days": clamp
        }}
    }))
    .unwrap()
}

fn fixture() -> Sm2Config {
    config([2, 3, 4, 5], [1, 365])
}

fn ladder(cfg: &Sm2Config, rating: Rating, rounds: usize) -> Vec<i64> {
    let mut card = Card::new(0);
    for day in 0..rounds as i64 {
        card.review(rating, day, cfg);
    }
    card.reviews
        .iter()
        .map(|entry| entry.scheduled_days)
        .collect()
}

#[test]
fn rating_all_lists_the_four_ratings_in_order() {
    assert_eq!(
        Rating::ALL,
        [Rating::Again, Rating::Hard, Rating::Good, Rating::Easy]
    );
}

#[test]
fn pass_threshold_comes_from_the_mapped_quality() {
    let low_hard = config([2, 2, 4, 5], [1, 365]);
    let mut card = Card::new(0);
    card.review(Rating::Hard, 0, &low_hard);
    assert_eq!(card.reps, 0);
    assert_eq!(card.lapses, 1);
    assert_eq!(card.interval_days, 1);
    let pass_hard = config([2, 3, 4, 5], [1, 365]);
    let mut other = Card::new(0);
    other.review(Rating::Hard, 0, &pass_hard);
    assert_eq!(other.reps, 1);
    assert_eq!(other.lapses, 0);
}

#[test]
fn lower_clamp_raises_short_intervals() {
    let cfg = config([2, 3, 4, 5], [10, 365]);
    assert_eq!(ladder(&cfg, Rating::Good, 3), vec![10, 10, 25]);
    let mut card = Card::new(0);
    card.review(Rating::Good, 0, &cfg);
    assert_eq!(card.due_day, 10);
}

#[test]
fn upper_clamp_caps_the_ladder_early() {
    let cfg = config([2, 3, 4, 5], [1, 5]);
    assert_eq!(ladder(&cfg, Rating::Good, 4), vec![1, 5, 5, 5]);
}

#[test]
fn quality_five_grows_ease_by_a_tenth_each_pass() {
    let cfg = config([2, 3, 5, 5], [1, 365]);
    assert_eq!(ladder(&cfg, Rating::Easy, 3), vec![1, 6, 16]);
    let mut card = Card::new(0);
    for day in 0..3 {
        card.review(Rating::Easy, day, &cfg);
    }
    assert!((card.ease - 2.8).abs() < 1e-5);
}

#[test]
fn again_never_touches_ease_even_at_quality_zero() {
    let cfg = config([0, 3, 4, 5], [1, 365]);
    let mut card = Card::new(0);
    for round in 0..4 {
        card.review(Rating::Again, round, &cfg);
        assert_eq!(card.ease, 2.5);
        assert_eq!(card.reps, 0);
        assert_eq!(card.interval_days, 1);
    }
    assert_eq!(card.lapses, 4);
    assert_eq!(card.reviews.len(), 4);
    assert!(card.reviews.iter().all(|entry| entry.quality == 0));
}

#[test]
fn malformed_sections_are_rejected() {
    for bad in [
        serde_json::json!({"srs": {"sm2": {"again": -1, "hard": 3, "good": 4, "easy": 5, "interval_clamp_days": [1, 365]}}}),
        serde_json::json!({"srs": {"sm2": {"again": "2", "hard": 3, "good": 4, "easy": 5, "interval_clamp_days": [1, 365]}}}),
        serde_json::json!({"srs": {"sm2": {"again": 2, "hard": 3, "good": 4, "easy": 5, "interval_clamp_days": [1, 2, 365]}}}),
        serde_json::json!({"srs": {"sm2": {"hard": 3, "good": 4, "easy": 5, "interval_clamp_days": [1, 365]}}}),
    ] {
        assert!(Sm2Config::from_value(&bad).is_err(), "{bad}");
    }
}

#[test]
fn good_then_easy_then_hard_ladder_is_derivable_by_hand() {
    let cfg = fixture();
    let mut card = Card::new(0);
    card.review(Rating::Good, 0, &cfg);
    card.review(Rating::Easy, 1, &cfg);
    card.review(Rating::Hard, 2, &cfg);
    let intervals: Vec<i64> = card
        .reviews
        .iter()
        .map(|entry| entry.scheduled_days)
        .collect();
    assert_eq!(intervals, vec![1, 6, 16]);
    assert!((card.ease - (2.5 + 0.1 - 0.14)).abs() < 1e-5);
    assert_eq!(card.due_day, 2 + 16);
}
