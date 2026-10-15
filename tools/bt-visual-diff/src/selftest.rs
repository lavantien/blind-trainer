use crate::{
    Args, EXIT_DIFF, PairReport, Side, Verdict, read_config, run_compare, visual_cfg_from,
};
use image::RgbaImage;
use std::fs;
use std::path::{Path, PathBuf};

const SEEDED_TOLERANCE: u16 = 16;
const WIDTH: u32 = 4;
const HEIGHT: u32 = 4;
const OVER_AT: (u32, u32) = (2, 1);

pub(crate) struct SelftestCheck {
    pub(crate) name: String,
    expected: &'static str,
    actual: &'static str,
    passed: bool,
}

fn scenarios() -> Vec<(&'static str, &'static str)> {
    vec![
        ("identical.png", "ok"),
        ("over.png", "over"),
        ("missing.png", "missing-baseline"),
    ]
}

fn verdict_kind(verdict: &Verdict) -> &'static str {
    match verdict {
        Verdict::Ok => "ok",
        Verdict::Over { .. } => "over",
        Verdict::DimMismatch { .. } => "dim-mismatch",
        Verdict::Missing { side: Side::Shot } => "missing-shot",
        Verdict::Missing { side: Side::Base } => "missing-baseline",
        Verdict::Decode { .. } => "decode",
    }
}

fn evaluate(reports: &[PairReport]) -> Vec<SelftestCheck> {
    scenarios()
        .into_iter()
        .map(|(name, expected)| {
            let actual = reports
                .iter()
                .find(|report| report.name == name)
                .map(|report| verdict_kind(&report.verdict))
                .unwrap_or("absent");
            SelftestCheck {
                name: name.to_string(),
                expected,
                actual,
                passed: actual == expected,
            }
        })
        .collect()
}

fn exit_code(checks: &[SelftestCheck]) -> u8 {
    if checks.iter().all(|check| check.passed) {
        0
    } else {
        EXIT_DIFF
    }
}

fn solid_image() -> RgbaImage {
    RgbaImage::from_pixel(WIDTH, HEIGHT, image::Rgba([0, 0, 0, 255]))
}

fn over_pixel(tolerance: u16) -> [u8; 4] {
    let delta = tolerance.saturating_add(1).min(u8::MAX as u16) as u8;
    [delta, 0, 0, 255]
}

fn config_file(workdir: &Path) -> PathBuf {
    workdir.join("config.json")
}

fn write_config(workdir: &Path) -> Result<(), String> {
    fs::create_dir_all(workdir)
        .map_err(|err| format!("cannot create {}: {err}", workdir.display()))?;
    let text = format!(r#"{{"visual": {{"tolerance": {SEEDED_TOLERANCE}}}}}"#);
    fs::write(config_file(workdir), text).map_err(|err| format!("cannot write config: {err}"))
}

fn tolerance_from_config(workdir: &Path) -> Result<u16, String> {
    let config = read_config(&config_file(workdir))?;
    Ok(visual_cfg_from(&config)?.tolerance)
}

pub(crate) fn materialize(workdir: &Path, tolerance: u16) -> Result<(PathBuf, PathBuf), String> {
    let shots = workdir.join("screenshots");
    let bases = workdir.join("baselines");
    for dir in [&shots, &bases] {
        if dir.is_dir() {
            fs::remove_dir_all(dir)
                .map_err(|err| format!("cannot clear {}: {err}", dir.display()))?;
        }
        fs::create_dir_all(dir).map_err(|err| format!("cannot create {}: {err}", dir.display()))?;
    }
    let write = |path: PathBuf, img: &RgbaImage| -> Result<(), String> {
        img.save(&path)
            .map_err(|err| format!("cannot write {}: {err}", path.display()))
    };
    write(shots.join("identical.png"), &solid_image())?;
    write(bases.join("identical.png"), &solid_image())?;
    let mut over_base = solid_image();
    over_base.put_pixel(OVER_AT.0, OVER_AT.1, image::Rgba(over_pixel(tolerance)));
    write(shots.join("over.png"), &solid_image())?;
    write(bases.join("over.png"), &over_base)?;
    write(shots.join("missing.png"), &solid_image())?;
    Ok((shots, bases))
}

pub(crate) fn execute(
    shots: &Path,
    bases: &Path,
    tolerance: u16,
) -> Result<Vec<SelftestCheck>, String> {
    let args = Args {
        screenshots: shots.to_path_buf(),
        baselines: bases.to_path_buf(),
        update: false,
    };
    let reports = run_compare(&args, tolerance).map_err(|err| format!("compare failed: {err}"))?;
    Ok(evaluate(&reports))
}

pub(crate) fn run_selftest(workdir: &Path) -> Result<u8, String> {
    write_config(workdir)?;
    let tolerance = tolerance_from_config(workdir)?;
    let (shots, bases) = materialize(workdir, tolerance)?;
    let checks = execute(&shots, &bases, tolerance)?;
    for check in &checks {
        println!(
            "{}: expected {}, got {}",
            check.name, check.expected, check.actual
        );
    }
    let passed = checks.iter().filter(|check| check.passed).count();
    println!("selftest: {}/{} checks passed", passed, checks.len());
    Ok(exit_code(&checks))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU64, Ordering};

    fn temp_dir(tag: &str) -> PathBuf {
        static COUNTER: AtomicU64 = AtomicU64::new(0);
        let dir = std::env::temp_dir()
            .join("bt-visual-diff-selftests")
            .join(format!(
                "{tag}-{}-{}",
                std::process::id(),
                COUNTER.fetch_add(1, Ordering::Relaxed)
            ));
        fs::create_dir_all(&dir).expect("temp dir");
        dir
    }

    fn report(name: &str, verdict: Verdict) -> PairReport {
        PairReport {
            name: name.to_string(),
            verdict,
            max_delta: None,
            pixels_over: None,
            pixels: None,
        }
    }

    fn matching_reports() -> Vec<PairReport> {
        vec![
            report("identical.png", Verdict::Ok),
            report("over.png", Verdict::Over { pixels_over: 1 }),
            report("missing.png", Verdict::Missing { side: Side::Base }),
        ]
    }

    #[test]
    fn verdict_kind_names_every_verdict() {
        assert_eq!(verdict_kind(&Verdict::Ok), "ok");
        assert_eq!(verdict_kind(&Verdict::Over { pixels_over: 9 }), "over");
        assert_eq!(
            verdict_kind(&Verdict::DimMismatch {
                shot: (1, 2),
                base: (2, 1)
            }),
            "dim-mismatch"
        );
        assert_eq!(
            verdict_kind(&Verdict::Missing { side: Side::Shot }),
            "missing-shot"
        );
        assert_eq!(
            verdict_kind(&Verdict::Missing { side: Side::Base }),
            "missing-baseline"
        );
        assert_eq!(
            verdict_kind(&Verdict::Decode {
                side: Side::Shot,
                error: "boom".to_string()
            }),
            "decode"
        );
    }

    #[test]
    fn evaluate_passes_when_reports_match_expectations() {
        let checks = evaluate(&matching_reports());
        assert_eq!(checks.len(), 3);
        assert!(checks.iter().all(|check| check.passed));
        assert_eq!(checks[0].name, "identical.png");
        assert_eq!(checks[1].name, "over.png");
        assert_eq!(checks[2].name, "missing.png");
    }

    #[test]
    fn evaluate_flags_absent_and_unexpected_verdicts() {
        let absent = evaluate(&[]);
        assert_eq!(absent.len(), 3);
        assert!(absent.iter().all(|check| !check.passed));
        assert!(absent.iter().all(|check| check.actual == "absent"));
        let mut reports = matching_reports();
        reports[1] = report("over.png", Verdict::Ok);
        let checks = evaluate(&reports);
        assert!(checks[0].passed);
        assert!(!checks[1].passed);
        assert_eq!(checks[1].expected, "over");
        assert_eq!(checks[1].actual, "ok");
        assert!(checks[2].passed);
    }

    #[test]
    fn exit_code_is_zero_only_when_every_check_passes() {
        let passing = evaluate(&matching_reports());
        assert_eq!(exit_code(&passing), 0);
        let mut reports = matching_reports();
        reports[2] = report("missing.png", Verdict::Ok);
        assert_eq!(exit_code(&evaluate(&reports)), EXIT_DIFF);
    }

    #[test]
    fn over_pixel_lands_exactly_one_above_tolerance() {
        let pixel = over_pixel(SEEDED_TOLERANCE);
        assert_eq!(pixel[0], (SEEDED_TOLERANCE + 1) as u8);
        assert_eq!(pixel[1], 0);
        assert_eq!(pixel[2], 0);
        assert_eq!(pixel[3], 255);
        assert_eq!(over_pixel(254)[0], 255);
        assert_eq!(over_pixel(255)[0], 255);
    }

    #[test]
    fn write_config_round_trips_tolerance_from_workdir() {
        let dir = temp_dir("config");
        write_config(&dir).expect("config");
        assert!(config_file(&dir).is_file());
        assert_eq!(tolerance_from_config(&dir), Ok(SEEDED_TOLERANCE));
    }

    #[test]
    fn materialize_writes_the_three_scenarios() {
        let dir = temp_dir("materialize");
        let (shots, bases) = materialize(&dir, SEEDED_TOLERANCE).expect("fixtures");
        for path in [
            shots.join("identical.png"),
            bases.join("identical.png"),
            shots.join("over.png"),
            bases.join("over.png"),
            shots.join("missing.png"),
        ] {
            assert!(path.is_file(), "{} missing", path.display());
        }
        assert!(!bases.join("missing.png").exists());
        let shot = image::open(shots.join("over.png")).expect("png").to_rgba8();
        let base = image::open(bases.join("over.png")).expect("png").to_rgba8();
        let differing = shot
            .pixels()
            .zip(base.pixels())
            .filter(|(a, b)| a != b)
            .count();
        assert_eq!(differing, 1);
        assert_ne!(
            shot.get_pixel(OVER_AT.0, OVER_AT.1),
            base.get_pixel(OVER_AT.0, OVER_AT.1)
        );
        assert_eq!(shot.get_pixel(0, 0), base.get_pixel(0, 0));
    }

    #[test]
    fn materialize_is_idempotent_and_clears_stale_files() {
        let dir = temp_dir("idempotent");
        materialize(&dir, SEEDED_TOLERANCE).expect("fixtures");
        fs::write(dir.join("baselines").join("missing.png"), b"").expect("stale");
        materialize(&dir, SEEDED_TOLERANCE).expect("fixtures again");
        assert!(!dir.join("baselines").join("missing.png").exists());
        assert!(dir.join("screenshots").join("missing.png").is_file());
    }

    #[test]
    fn execute_detects_every_planted_scenario() {
        let dir = temp_dir("execute");
        let (shots, bases) = materialize(&dir, SEEDED_TOLERANCE).expect("fixtures");
        let checks = execute(&shots, &bases, SEEDED_TOLERANCE).expect("checks");
        assert!(checks.iter().all(|check| check.passed));
    }

    #[test]
    fn a_broken_over_fixture_fails_the_selftest() {
        let dir = temp_dir("broken");
        let (shots, bases) = materialize(&dir, SEEDED_TOLERANCE).expect("fixtures");
        solid_image()
            .save(bases.join("over.png"))
            .expect("break fixture");
        let checks = execute(&shots, &bases, SEEDED_TOLERANCE).expect("checks");
        assert!(checks[0].passed);
        assert!(!checks[1].passed);
        assert_eq!(checks[1].actual, "ok");
        assert_eq!(exit_code(&checks), EXIT_DIFF);
    }

    #[test]
    fn a_broken_missing_fixture_fails_the_selftest() {
        let dir = temp_dir("broken-missing");
        let (shots, bases) = materialize(&dir, SEEDED_TOLERANCE).expect("fixtures");
        solid_image()
            .save(bases.join("missing.png"))
            .expect("break fixture");
        let checks = execute(&shots, &bases, SEEDED_TOLERANCE).expect("checks");
        assert!(checks[0].passed);
        assert!(checks[1].passed);
        assert!(!checks[2].passed);
        assert_eq!(exit_code(&checks), EXIT_DIFF);
    }

    #[test]
    fn run_selftest_passes_and_leaves_only_workdir_state() {
        let dir = temp_dir("run");
        assert_eq!(run_selftest(&dir), Ok(0));
        assert!(config_file(&dir).is_file());
        assert!(dir.join("screenshots").join("identical.png").is_file());
        assert!(dir.join("baselines").join("over.png").is_file());
        assert_eq!(run_selftest(&dir), Ok(0));
    }
}
