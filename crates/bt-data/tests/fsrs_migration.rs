mod common;

use bt_data::fsrs::{FsrsConfig, MemoryState, next_interval_days};
use bt_data::srs::{ReviewLogEntry, grade_from_quality, replay_sm2};
use common::ref_ground;

fn golden_config() -> FsrsConfig {
    FsrsConfig::defaults()
}

fn assert_close(actual: f64, expected: f64, what: &str, case: &str, step: usize) {
    let tolerance = 1e-12 * expected.abs().max(1.0);
    assert!(
        (actual - expected).abs() <= tolerance,
        "{what} mismatch in {case} step {step}: got {actual}, expected {expected}"
    );
}

#[test]
fn quality_mapping_is_the_pinned_sm2_table() {
    assert_eq!(grade_from_quality(2).unwrap().to_code(), 1);
    assert_eq!(grade_from_quality(3).unwrap().to_code(), 2);
    assert_eq!(grade_from_quality(4).unwrap().to_code(), 3);
    assert_eq!(grade_from_quality(5).unwrap().to_code(), 4);
    for quality in [0u32, 1, 6, 9, u32::MAX] {
        assert!(grade_from_quality(quality).is_err(), "quality {quality}");
    }
}

#[test]
fn replay_of_empty_history_is_none() {
    let cfg = golden_config();
    assert!(replay_sm2(&[], &cfg).expect("empty replay").is_none());
}

#[test]
fn migration_vectors_replay_exactly_at_every_prefix() {
    let cfg = golden_config();
    let text = std::fs::read_to_string(ref_ground().join("fsrs/sm2_migration.tsv"))
        .expect("migration vectors file");
    let mut lines = text.trim_end_matches('\n').split('\n');
    let header = lines.next().expect("header row");
    assert_eq!(
        header,
        "case\tstep\tquality\telapsed_days\tstability\tdifficulty\tinterval_days"
    );
    let mut current_case = "";
    let mut history: Vec<ReviewLogEntry> = Vec::new();
    for line in lines {
        let cols: Vec<&str> = line.split('\t').collect();
        assert_eq!(cols.len(), 7, "row columns in {line}");
        let case = cols[0];
        let step: usize = cols[1].parse().expect("step");
        let quality: u32 = cols[2].parse().expect("quality");
        let elapsed: i64 = cols[3].parse().expect("elapsed");
        if case != current_case {
            current_case = case;
            assert_eq!(step, 0, "first row of case {case} must be step 0");
            history.clear();
        }
        history.push(ReviewLogEntry {
            ts_days: history.len() as i64,
            quality,
            elapsed_days: elapsed,
            scheduled_days: 0,
        });
        let expected_s: f64 = cols[4].parse().expect("stability");
        let expected_d: f64 = cols[5].parse().expect("difficulty");
        let expected_ivl: i64 = cols[6].parse().expect("interval");
        let MemoryState {
            stability,
            difficulty,
        } = replay_sm2(&history, &cfg)
            .expect("replay")
            .unwrap_or_else(|| panic!("case {case} step {step} replayed to no state"));
        assert_close(stability, expected_s, "stability", case, step);
        assert_close(difficulty, expected_d, "difficulty", case, step);
        assert_eq!(
            next_interval_days(stability, &cfg),
            expected_ivl,
            "interval mismatch in {case} step {step}"
        );
    }
    assert_eq!(current_case, "long_gap", "last case reached");
}
