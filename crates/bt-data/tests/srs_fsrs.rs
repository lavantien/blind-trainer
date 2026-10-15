mod common;

use bt_data::fsrs::{DEFAULT_WEIGHTS, FsrsConfig, MemoryState};
use bt_data::srs::{Card, Rating, grade_from_quality, replay_sm2};
use common::{config_path, ref_ground};
use proptest::prelude::*;

fn golden_config() -> FsrsConfig {
    FsrsConfig {
        weights: DEFAULT_WEIGHTS,
        request_retention: 0.9,
        interval_clamp_days: [1, 36500],
    }
}

fn hub_config() -> FsrsConfig {
    let text = std::fs::read_to_string(config_path()).expect("config file");
    let value: serde_json::Value = serde_json::from_str(&text).expect("config json");
    FsrsConfig::from_value(&value).expect("srs.fsrs section")
}

fn rating_of_quality(quality: u32) -> Rating {
    grade_from_quality(quality).expect("golden qualities map")
}

fn assert_close(actual: f64, expected: f64, what: &str, case: &str, step: usize) {
    let tolerance = 1e-12 * expected.abs().max(1.0);
    assert!(
        (actual - expected).abs() <= tolerance,
        "{what} mismatch in {case} step {step}: got {actual}, expected {expected}"
    );
}

#[test]
fn unknown_quality_reports_the_offending_quality_in_its_message() {
    let err = grade_from_quality(9).expect_err("unmapped quality");
    assert_eq!(
        err.to_string(),
        "review quality 9 has no sm-2 grade mapping"
    );
}

#[test]
fn graded_walks_match_the_pinned_reference_conversions() {
    let cfg = golden_config();
    let text = std::fs::read_to_string(ref_ground().join("fsrs/sm2_migration.tsv"))
        .expect("migration vectors file");
    let lines = text.trim_end_matches('\n').split('\n').skip(1);
    let mut case = "";
    let mut card = Card::new(0);
    let mut day = 0i64;
    let mut step = 0usize;
    let mut expected_lapses = 0u32;
    for line in lines {
        let cols: Vec<&str> = line.split('\t').collect();
        assert_eq!(cols.len(), 7, "row columns in {line}");
        if cols[0] != case {
            case = cols[0];
            card = Card::new(0);
            day = 0;
            step = 0;
            expected_lapses = 0;
        }
        let quality: u32 = cols[2].parse().expect("quality");
        let elapsed: i64 = cols[3].parse().expect("elapsed");
        day += elapsed;
        card.review(rating_of_quality(quality), day, &cfg);
        let expected_s: f64 = cols[4].parse().expect("stability");
        let expected_d: f64 = cols[5].parse().expect("difficulty");
        let expected_ivl: i64 = cols[6].parse().expect("interval");
        let memory = card.memory.expect("graded card holds memory");
        assert_close(memory.stability, expected_s, "stability", case, step);
        assert_close(memory.difficulty, expected_d, "difficulty", case, step);
        assert_eq!(
            card.interval_days, expected_ivl,
            "interval in {case} step {step}"
        );
        assert_eq!(
            card.due_day,
            day + card.interval_days,
            "due in {case} step {step}"
        );
        assert_eq!(card.reps as usize, step + 1, "reps in {case} step {step}");
        if quality == 2 {
            expected_lapses += 1;
        }
        assert_eq!(card.lapses, expected_lapses, "lapses in {case} step {step}");
        let entry = card.reviews.last().expect("review entry");
        assert_eq!(entry.ts_days, day, "ts in {case} step {step}");
        assert_eq!(entry.elapsed_days, elapsed, "elapsed in {case} step {step}");
        assert_eq!(
            entry.scheduled_days, expected_ivl,
            "scheduled in {case} step {step}"
        );
        assert_eq!(
            entry.quality, quality,
            "recorded quality in {case} step {step}"
        );
        step += 1;
    }
}

#[test]
fn new_card_has_no_memory_and_is_due_immediately() {
    let card = Card::new(100);
    assert_eq!(card.memory, None);
    assert_eq!(card.interval_days, 0);
    assert_eq!(card.due_day, 100);
    assert_eq!(card.reps, 0);
    assert_eq!(card.lapses, 0);
    assert!(card.reviews.is_empty());
    assert!(card.is_due(100));
    assert!(!card.is_due(99));
}

#[test]
fn the_first_review_seeds_memory_from_the_grade() {
    for (rating, quality, stability) in [
        (Rating::Again, 2u32, 0.40255),
        (Rating::Hard, 3, 1.18385),
        (Rating::Good, 4, 3.173),
        (Rating::Easy, 5, 15.69105),
    ] {
        let mut card = Card::new(7);
        card.review(rating, 7, &golden_config());
        let memory = card.memory.expect("memory after first review");
        assert_close(
            memory.stability,
            stability,
            "initial stability",
            "first",
            quality as usize,
        );
        assert_eq!(card.reviews[0].quality, quality);
        assert_eq!(card.reviews[0].elapsed_days, 0);
    }
}

#[test]
fn again_counts_a_lapse_but_reps_keep_counting() {
    let cfg = golden_config();
    let mut card = Card::new(0);
    card.review(Rating::Good, 0, &cfg);
    card.review(Rating::Good, 1, &cfg);
    card.review(Rating::Again, 7, &cfg);
    card.review(Rating::Good, 8, &cfg);
    assert_eq!(card.reps, 4);
    assert_eq!(card.lapses, 1);
    assert!(card.memory.expect("memory").is_valid());
}

#[test]
fn overdue_reviews_record_the_real_elapsed_gap() {
    let cfg = golden_config();
    let mut card = Card::new(0);
    card.review(Rating::Good, 0, &cfg);
    card.review(Rating::Good, 10, &cfg);
    let second = &card.reviews[1];
    assert_eq!(second.elapsed_days, 10);
    assert_eq!(second.ts_days, 10);
    assert_eq!(card.due_day, 10 + card.interval_days);
}

#[test]
fn review_days_never_move_the_clock_backwards() {
    let cfg = golden_config();
    let mut card = Card::new(50);
    card.review(Rating::Good, 40, &cfg);
    assert_eq!(card.reviews[0].elapsed_days, 0);
    card.review(Rating::Good, 35, &cfg);
    assert_eq!(card.reviews[1].elapsed_days, 0);
}

#[test]
fn intervals_stay_inside_the_hub_clamps() {
    let cfg = hub_config();
    let mut card = Card::new(0);
    let mut day = 0;
    let mut hit_the_cap = false;
    for _ in 0..12 {
        day += 6;
        card.review(Rating::Easy, day, &cfg);
        assert!(
            (1..=365).contains(&card.interval_days),
            "interval {} left the hub clamps",
            card.interval_days
        );
        hit_the_cap |= card.interval_days == 365;
    }
    assert!(hit_the_cap, "a long easy run must reach the cap");
}

#[test]
fn the_hub_fixture_carries_the_approved_fsrs_values() {
    let cfg = hub_config();
    assert_eq!(cfg.weights, DEFAULT_WEIGHTS);
    assert_eq!(cfg.request_retention, 0.9);
    assert_eq!(cfg.interval_clamp_days, [1, 365]);
}

#[test]
fn card_round_trips_through_serde_json() {
    let cfg = golden_config();
    let mut card = Card::new(7);
    card.review(Rating::Good, 7, &cfg);
    card.review(Rating::Easy, 10, &cfg);
    card.review(Rating::Again, 16, &cfg);
    card.review(Rating::Hard, 17, &cfg);
    let text = serde_json::to_string_pretty(&card).unwrap();
    let back: Card = serde_json::from_str(&text).unwrap();
    assert_eq!(back, card);
    assert!(text.contains("\"memory\""));
    assert!(text.contains("\"stability\""));
    assert!(text.contains("\"difficulty\""));
    assert!(text.contains("\"ts_days\""));
    assert!(text.contains("\"quality\""));
}

#[test]
fn a_forgotten_memory_is_rebuilt_from_the_recorded_log() {
    let cfg = golden_config();
    let graded = {
        let mut graded = Card::new(0);
        graded.review(Rating::Good, 0, &cfg);
        graded.review(Rating::Easy, 1, &cfg);
        graded.review(Rating::Hard, 11, &cfg);
        graded
    };
    let mut forgotten = graded.clone();
    forgotten.memory = None;
    forgotten.review(Rating::Good, 21, &cfg);
    let mut expected_log = graded.reviews.clone();
    expected_log.push(forgotten.reviews.last().unwrap().clone());
    assert_eq!(
        forgotten.memory,
        replay_sm2(&expected_log, &cfg).expect("replay")
    );
}

#[test]
fn a_memory_without_any_reviews_seeds_from_the_grade() {
    let cfg = golden_config();
    let mut orphan = Card {
        memory: Some(MemoryState {
            stability: 90.0,
            difficulty: 5.0,
        }),
        interval_days: 90,
        due_day: 90,
        reps: 0,
        lapses: 0,
        reviews: Vec::new(),
    };
    orphan.review(Rating::Good, 0, &cfg);
    assert_eq!(
        orphan.memory,
        Some(MemoryState::initial(Rating::Good, &cfg))
    );
}

#[test]
fn legacy_card_json_without_memory_defaults_to_none() {
    let text = r#"{"ease":2.5,"interval_days":6,"due_day":10,"reps":2,"lapses":1,"reviews":[]}"#;
    let card: Card = serde_json::from_str(text).expect("legacy shape parses");
    assert_eq!(card.memory, None);
    assert_eq!(card.interval_days, 6);
    assert_eq!(card.due_day, 10);
}

#[test]
fn a_card_without_memory_walks_the_reference_ladder_from_scratch() {
    let cfg = golden_config();
    let graded = {
        let mut graded = Card::new(0);
        graded.review(Rating::Good, 0, &cfg);
        graded.review(Rating::Good, 1, &cfg);
        graded
    };
    let mut fresh = Card {
        memory: None,
        interval_days: 0,
        due_day: 0,
        reps: 0,
        lapses: 0,
        reviews: Vec::new(),
    };
    fresh.review(Rating::Good, 0, &cfg);
    fresh.review(Rating::Good, 1, &cfg);
    assert_eq!(fresh.memory, graded.memory);
    let replayed = replay_sm2(&fresh.reviews, &cfg)
        .expect("replay")
        .expect("state");
    assert_eq!(fresh.memory, Some(replayed));
}

proptest! {
    #[test]
    fn invariants_hold_over_random_histories(
        grades in prop::collection::vec(0u8..4, 1..40),
        start in 0i64..10_000,
        gaps in prop::collection::vec(0i64..30, 1..40),
    ) {
        let cfg = golden_config();
        let mut card = Card::new(start);
        let mut day = start;
        let mut lapses = 0u32;
        for (i, &code) in grades.iter().enumerate() {
            let rating = match code { 0 => Rating::Again, 1 => Rating::Hard, 2 => Rating::Good, _ => Rating::Easy };
            day += gaps[i % gaps.len()];
            card.review(rating, day, &cfg);
            let memory = card.memory.expect("memory after review");
            prop_assert!(memory.is_valid());
            prop_assert!((1..=36500).contains(&card.interval_days));
            prop_assert_eq!(card.due_day, day + card.interval_days);
            prop_assert_eq!(card.reps as usize, i + 1);
            if rating == Rating::Again {
                lapses += 1;
            }
            prop_assert_eq!(card.lapses, lapses);
            let entry = card.reviews.last().unwrap();
            prop_assert_eq!(entry.ts_days, day);
            prop_assert_eq!(entry.scheduled_days, card.interval_days);
            prop_assert_eq!(entry.quality, rating.to_code() as u32 + 1);
            prop_assert_eq!(
                card.memory,
                replay_sm2(&card.reviews, &cfg).expect("graded log replays")
            );
        }
    }
}
