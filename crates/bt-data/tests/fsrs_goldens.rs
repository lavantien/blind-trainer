mod common;

#[path = "../src/fsrs.rs"]
mod fsrs;

use common::ref_ground;
use fsrs::{DEFAULT_WEIGHTS, FsrsConfig, Grade, MemoryState, next_interval_days};
use serde_json::Value;

fn golden_config() -> FsrsConfig {
    let text = std::fs::read_to_string(ref_ground().join("fsrs/reference_meta.json"))
        .expect("reference meta file");
    let meta: Value = serde_json::from_str(&text).expect("reference meta json");
    let scheduler = &meta["scheduler"];
    let parameters = scheduler["parameters"].as_array().expect("parameters");
    assert_eq!(parameters.len(), 19);
    let mut weights = [0.0f64; 19];
    for (i, value) in parameters.iter().enumerate() {
        weights[i] = value.as_f64().expect("weight value");
    }
    FsrsConfig {
        weights,
        request_retention: scheduler["desired_retention"].as_f64().expect("retention"),
        interval_clamp_days: [
            1,
            scheduler["maximum_interval"]
                .as_i64()
                .expect("max interval"),
        ],
    }
}

fn grade_from_code(code: u8) -> Grade {
    match code {
        1 => Grade::Again,
        2 => Grade::Hard,
        3 => Grade::Good,
        _ => Grade::Easy,
    }
}

fn assert_close(actual: f64, expected: f64, what: &str, case: &str, step: usize) {
    let tolerance = 1e-12 * expected.abs().max(1.0);
    assert!(
        (actual - expected).abs() <= tolerance,
        "{what} mismatch in {case} step {step}: got {actual}, expected {expected}"
    );
}

#[test]
fn default_settings_match_the_pinned_reference() {
    let cfg = golden_config();
    for (i, (actual, expected)) in cfg.weights.iter().zip(DEFAULT_WEIGHTS).enumerate() {
        assert_eq!(actual, &expected, "weight {i} diverges from the reference");
    }
    assert_eq!(cfg, FsrsConfig::defaults());
}

#[test]
fn reference_vectors_replay_exactly() {
    let cfg = golden_config();
    let text = std::fs::read_to_string(ref_ground().join("fsrs/reference_vectors.tsv"))
        .expect("reference vectors file");
    let mut lines = text.trim_end_matches('\n').split('\n');
    let header = lines.next().expect("header row");
    assert_eq!(
        header,
        "case\tstep\trating\telapsed_days\tretrievability\tstability\tdifficulty\tinterval_days"
    );
    let mut current_case = "";
    let mut state = MemoryState {
        stability: 0.0,
        difficulty: 0.0,
    };
    let mut step_index = 0usize;
    for line in lines {
        let cols: Vec<&str> = line.split('\t').collect();
        assert_eq!(cols.len(), 8, "row columns in {line}");
        let case = cols[0];
        let step: usize = cols[1].parse().expect("step");
        let rating: u8 = cols[2].parse().expect("rating");
        let grade = grade_from_code(rating);
        if case != current_case {
            assert_eq!(step, 0, "first row of case {case} must be step 0");
            current_case = case;
            step_index = 0;
            state = MemoryState::initial(grade, &cfg);
        } else {
            step_index += 1;
            assert_eq!(step, step_index, "steps must be contiguous in {case}");
            let elapsed: f64 = cols[3].parse().expect("elapsed days");
            if !cols[4].is_empty() {
                let expected_r: f64 = cols[4].parse().expect("retrievability");
                assert_close(
                    state.retrievability(elapsed),
                    expected_r,
                    "retrievability",
                    case,
                    step,
                );
            }
            state = state.review(grade, elapsed, &cfg);
        }
        let expected_s: f64 = cols[5].parse().expect("stability");
        let expected_d: f64 = cols[6].parse().expect("difficulty");
        let expected_ivl: i64 = cols[7].parse().expect("interval");
        assert_close(state.stability, expected_s, "stability", case, step);
        assert_close(state.difficulty, expected_d, "difficulty", case, step);
        assert_eq!(
            next_interval_days(state.stability, &cfg),
            expected_ivl,
            "interval mismatch in {case} step {step}"
        );
    }
    assert_eq!(current_case, "huge_gap_after_easy", "last case reached");
    assert!(step_index >= 1, "at least one non-first step ran");
}
