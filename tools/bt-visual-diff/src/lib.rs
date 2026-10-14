use image::RgbaImage;
use serde_json::Value;
use std::collections::BTreeSet;
use std::env;
use std::fs;
use std::path::{Path, PathBuf};

pub const EXIT_USAGE: u8 = 2;
const EXIT_DIFF: u8 = 1;

#[derive(Debug, PartialEq, Eq)]
struct Args {
    screenshots: PathBuf,
    baselines: PathBuf,
    update: bool,
}

#[derive(Debug, PartialEq, Eq)]
enum ArgsError {
    Usage,
}

fn parse_args(argv: &[String]) -> Result<Args, ArgsError> {
    let mut positional: Vec<PathBuf> = Vec::new();
    let mut update = false;
    for arg in argv {
        if arg == "--update" {
            update = true;
        } else {
            positional.push(PathBuf::from(arg));
        }
    }
    if positional.len() != 2 {
        return Err(ArgsError::Usage);
    }
    let screenshots = positional.first().expect("two positionals").clone();
    let baselines = positional.get(1).expect("two positionals").clone();
    Ok(Args {
        screenshots,
        baselines,
        update,
    })
}

fn workspace_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .and_then(Path::parent)
        .expect("workspace root from manifest dir")
        .to_path_buf()
}

fn config_path_from(env_value: Option<&str>, root: &Path) -> PathBuf {
    match env_value.filter(|value| !value.is_empty()) {
        Some(raw) => {
            let path = PathBuf::from(raw);
            if path.is_absolute() {
                path
            } else {
                root.join(path)
            }
        }
        None => root.join("config").join("config.json"),
    }
}

fn read_config(path: &Path) -> Result<Value, String> {
    let text = fs::read_to_string(path)
        .map_err(|err| format!("cannot read config {}: {err}", path.display()))?;
    serde_json::from_str(&text)
        .map_err(|err| format!("cannot parse config {}: {err}", path.display()))
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct VisualCfg {
    tolerance: u16,
}

fn visual_cfg_from(value: &Value) -> Result<VisualCfg, String> {
    let raw = value
        .pointer("/visual/tolerance")
        .ok_or_else(|| "config visual.tolerance missing".to_string())?;
    let tolerance = raw
        .as_u64()
        .and_then(|number| u16::try_from(number).ok())
        .filter(|number| *number <= u8::MAX as u16)
        .ok_or_else(|| "config visual.tolerance must be an integer 0..=255".to_string())?;
    Ok(VisualCfg { tolerance })
}

fn artifacts_root_from(env_value: Option<&str>, value: &Value, root: &Path) -> PathBuf {
    let relative = env_value
        .filter(|raw| !raw.is_empty())
        .or_else(|| {
            value
                .pointer("/paths/artifacts")
                .and_then(|raw| raw.as_str())
        })
        .unwrap_or("artifacts");
    let path = PathBuf::from(relative);
    if path.is_absolute() {
        path
    } else {
        root.join(path)
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Side {
    Shot,
    Base,
}

impl Side {
    fn label(self) -> &'static str {
        match self {
            Side::Shot => "screenshots",
            Side::Base => "baselines",
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
enum Verdict {
    Ok,
    Over { pixels_over: u64 },
    DimMismatch { shot: (u32, u32), base: (u32, u32) },
    Missing { side: Side },
    Decode { side: Side, error: String },
}

impl Verdict {
    fn is_failure(&self) -> bool {
        !matches!(self, Verdict::Ok)
    }

    fn label(&self) -> String {
        match self {
            Verdict::Ok => "ok".to_string(),
            Verdict::Over { pixels_over } => format!("over tolerance ({pixels_over} px)"),
            Verdict::DimMismatch { shot, base } => {
                format!(
                    "dimension mismatch {}x{} vs {}x{}",
                    shot.0, shot.1, base.0, base.1
                )
            }
            Verdict::Missing { side } => format!("missing on {} side", side.label()),
            Verdict::Decode { side, error } => {
                format!("decode failed on {} side: {error}", side.label())
            }
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
struct PairReport {
    name: String,
    verdict: Verdict,
    max_delta: Option<u16>,
    pixels_over: Option<u64>,
    pixels: Option<u64>,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
struct Comparison {
    max_delta: u16,
    pixels_over: u64,
}

fn compare_images(shot: &RgbaImage, base: &RgbaImage, tolerance: u16) -> Comparison {
    let mut comparison = Comparison::default();
    for (a, b) in shot.pixels().zip(base.pixels()) {
        let mut pixel_over = false;
        for (channel_a, channel_b) in a.0.iter().zip(b.0.iter()) {
            let delta = channel_a.abs_diff(*channel_b);
            comparison.max_delta = comparison.max_delta.max(delta as u16);
            pixel_over |= u16::from(delta) > tolerance;
        }
        if pixel_over {
            comparison.pixels_over += 1;
        }
    }
    comparison
}

fn decode(path: &Path) -> Result<RgbaImage, String> {
    image::open(path)
        .map(|dynamic| dynamic.to_rgba8())
        .map_err(|err| format!("{}: {err}", path.display()))
}

fn compare_pair(name: &str, shot: &Path, base: &Path, tolerance: u16) -> PairReport {
    let missing = |side: Side| PairReport {
        name: name.to_string(),
        verdict: Verdict::Missing { side },
        max_delta: None,
        pixels_over: None,
        pixels: None,
    };
    if !shot.is_file() {
        return missing(Side::Shot);
    }
    if !base.is_file() {
        return missing(Side::Base);
    }
    let shot_img = match decode(shot) {
        Ok(img) => img,
        Err(error) => {
            return PairReport {
                name: name.to_string(),
                verdict: Verdict::Decode {
                    side: Side::Shot,
                    error,
                },
                max_delta: None,
                pixels_over: None,
                pixels: None,
            };
        }
    };
    let base_img = match decode(base) {
        Ok(img) => img,
        Err(error) => {
            return PairReport {
                name: name.to_string(),
                verdict: Verdict::Decode {
                    side: Side::Base,
                    error,
                },
                max_delta: None,
                pixels_over: None,
                pixels: None,
            };
        }
    };
    let shot_dims = shot_img.dimensions();
    let base_dims = base_img.dimensions();
    if shot_dims != base_dims {
        return PairReport {
            name: name.to_string(),
            verdict: Verdict::DimMismatch {
                shot: shot_dims,
                base: base_dims,
            },
            max_delta: None,
            pixels_over: None,
            pixels: None,
        };
    }
    let comparison = compare_images(&shot_img, &base_img, tolerance);
    let verdict = if comparison.pixels_over > 0 {
        Verdict::Over {
            pixels_over: comparison.pixels_over,
        }
    } else {
        Verdict::Ok
    };
    let pixels = shot_dims.0 as u64 * shot_dims.1 as u64;
    PairReport {
        name: name.to_string(),
        verdict,
        max_delta: Some(comparison.max_delta),
        pixels_over: Some(comparison.pixels_over),
        pixels: Some(pixels),
    }
}

fn png_names(dir: &Path) -> Result<BTreeSet<String>, std::io::Error> {
    if !dir.is_dir() {
        return Ok(BTreeSet::new());
    }
    let mut names = BTreeSet::new();
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();
        if path.is_file()
            && path
                .extension()
                .is_some_and(|ext| ext.eq_ignore_ascii_case("png"))
            && let Some(name) = path.file_name().and_then(|raw| raw.to_str())
        {
            names.insert(name.to_string());
        }
    }
    Ok(names)
}

fn pair_names(shot_dir: &Path, base_dir: &Path) -> Result<Vec<String>, std::io::Error> {
    let mut names = png_names(shot_dir)?;
    names.extend(png_names(base_dir)?);
    Ok(names.into_iter().collect())
}

fn run_compare(args: &Args, tolerance: u16) -> Result<Vec<PairReport>, std::io::Error> {
    let mut reports = Vec::new();
    for name in pair_names(&args.screenshots, &args.baselines)? {
        let shot = args.screenshots.join(&name);
        let base = args.baselines.join(&name);
        reports.push(compare_pair(&name, &shot, &base, tolerance));
    }
    Ok(reports)
}

fn run_update(args: &Args) -> Result<Vec<String>, std::io::Error> {
    fs::create_dir_all(&args.baselines)?;
    let current = png_names(&args.screenshots)?;
    let mut updated = Vec::new();
    for name in &current {
        fs::copy(args.screenshots.join(name), args.baselines.join(name))?;
        updated.push(name.clone());
    }
    let stale = png_names(&args.baselines)?;
    for name in stale.difference(&current) {
        fs::remove_file(args.baselines.join(name))?;
    }
    Ok(updated)
}

fn render_report(
    reports: &[PairReport],
    tolerance: u16,
    shot_dir: &Path,
    base_dir: &Path,
) -> String {
    let mut text = String::new();
    text.push_str("# visual diff report\n\n");
    text.push_str(&format!("screenshots: {}\n", shot_dir.display()));
    text.push_str(&format!("baselines: {}\n", base_dir.display()));
    text.push_str(&format!("tolerance: {tolerance}\n\n"));
    text.push_str("| pair | verdict | max delta | pixels over | pixels |\n");
    text.push_str("| ---- | ------- | --------- | ----------- | ------ |\n");
    for report in reports {
        let max_delta = report
            .max_delta
            .map(|value| value.to_string())
            .unwrap_or_else(|| "-".to_string());
        let pixels_over = report
            .pixels_over
            .map(|value| value.to_string())
            .unwrap_or_else(|| "-".to_string());
        let pixels = report
            .pixels
            .map(|value| value.to_string())
            .unwrap_or_else(|| "-".to_string());
        text.push_str(&format!(
            "| {} | {} | {} | {} | {} |\n",
            report.name.replace('|', "/"),
            report.verdict.label().replace('|', "/"),
            max_delta,
            pixels_over,
            pixels
        ));
    }
    let ok_count = reports
        .iter()
        .filter(|report| !report.verdict.is_failure())
        .count();
    let failed = reports.len() - ok_count;
    text.push_str(&format!(
        "\n{} pairs, {ok_count} ok, {failed} failed\n",
        reports.len()
    ));
    text
}

fn write_report(text: &str, artifacts: &Path) -> Result<PathBuf, std::io::Error> {
    let dir = artifacts.join("visual");
    fs::create_dir_all(&dir)?;
    let path = dir.join("report.md");
    fs::write(&path, text)?;
    Ok(path)
}

pub struct Env {
    argv: Vec<String>,
    config: Option<String>,
    artifacts: Option<String>,
    root: PathBuf,
}

impl Env {
    pub fn from_process() -> Self {
        Self {
            argv: env::args().skip(1).collect(),
            config: env::var("BT_CONFIG").ok(),
            artifacts: env::var("BT_ARTIFACTS").ok(),
            root: workspace_root(),
        }
    }
}

pub fn run(env: &Env) -> Result<u8, String> {
    let args = parse_args(&env.argv).map_err(|ArgsError::Usage| {
        "usage: bt-visual-diff <screenshots-dir> <baselines-dir> [--update]".to_string()
    })?;
    let config = read_config(&config_path_from(env.config.as_deref(), &env.root))?;
    let cfg = visual_cfg_from(&config)?;
    if args.update {
        let updated = run_update(&args).map_err(|err| format!("update failed: {err}"))?;
        println!("updated {} baselines", updated.len());
        for name in updated {
            println!("updated {name}");
        }
        return Ok(0);
    }
    let reports =
        run_compare(&args, cfg.tolerance).map_err(|err| format!("compare failed: {err}"))?;
    let text = render_report(&reports, cfg.tolerance, &args.screenshots, &args.baselines);
    let report_path = write_report(
        &text,
        &artifacts_root_from(env.artifacts.as_deref(), &config, &env.root),
    )
    .map_err(|err| format!("cannot write report: {err}"))?;
    println!("report: {}", report_path.display());
    for report in &reports {
        println!("{}: {}", report.name, report.verdict.label());
    }
    if reports.iter().any(|report| report.verdict.is_failure()) {
        return Ok(EXIT_DIFF);
    }
    Ok(0)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU64, Ordering};

    fn temp_dir(tag: &str) -> PathBuf {
        static COUNTER: AtomicU64 = AtomicU64::new(0);
        let dir = env::temp_dir().join("bt-visual-diff-tests").join(format!(
            "{tag}-{}-{}",
            std::process::id(),
            COUNTER.fetch_add(1, Ordering::Relaxed)
        ));
        fs::create_dir_all(&dir).expect("temp dir");
        dir
    }

    fn solid_image(width: u32, height: u32, rgba: [u8; 4]) -> RgbaImage {
        RgbaImage::from_pixel(width, height, image::Rgba(rgba))
    }

    fn image_with_pixel(
        width: u32,
        height: u32,
        base: [u8; 4],
        at: (u32, u32),
        rgba: [u8; 4],
    ) -> RgbaImage {
        let mut img = solid_image(width, height, base);
        img.put_pixel(at.0, at.1, image::Rgba(rgba));
        img
    }

    fn write_png(path: &Path, img: &RgbaImage) {
        img.save(path).expect("write png");
    }

    fn args_for(shot: &Path, base: &Path, update: bool) -> Args {
        Args {
            screenshots: shot.to_path_buf(),
            baselines: base.to_path_buf(),
            update,
        }
    }

    fn argv(items: &[&str]) -> Vec<String> {
        items.iter().map(|item| item.to_string()).collect()
    }

    fn write_config(root: &Path, tolerance: u16) -> String {
        let path = root.join("config.json");
        fs::write(
            &path,
            format!(r#"{{"visual": {{"tolerance": {tolerance}}}}}"#),
        )
        .expect("write config");
        path.to_string_lossy().into_owned()
    }

    fn env_for(root: &Path, config: Option<String>, items: &[&str]) -> Env {
        Env {
            argv: argv(items),
            config,
            artifacts: None,
            root: root.to_path_buf(),
        }
    }

    #[test]
    fn parse_args_reads_two_dirs_and_update_flag() {
        let parsed = parse_args(&argv(&["shots", "base"])).expect("args");
        assert_eq!(parsed.screenshots, PathBuf::from("shots"));
        assert_eq!(parsed.baselines, PathBuf::from("base"));
        assert!(!parsed.update);
        let updated = parse_args(&argv(&["shots", "base", "--update"])).expect("args");
        assert!(updated.update);
        let flag_first = parse_args(&argv(&["--update", "shots", "base"])).expect("args");
        assert!(flag_first.update);
    }

    #[test]
    fn parse_args_rejects_wrong_counts() {
        assert_eq!(parse_args(&argv(&[])), Err(ArgsError::Usage));
        assert_eq!(parse_args(&argv(&["shots"])), Err(ArgsError::Usage));
        assert_eq!(
            parse_args(&argv(&["shots", "base", "extra"])),
            Err(ArgsError::Usage)
        );
    }

    #[test]
    fn config_path_from_honors_env_and_default() {
        let root = PathBuf::from("/repo");
        assert_eq!(
            config_path_from(Some("cfg/alt.json"), &root),
            PathBuf::from("/repo/cfg/alt.json")
        );
        assert_eq!(
            config_path_from(Some("C:/abs/config.json"), &root),
            PathBuf::from("C:/abs/config.json")
        );
        assert_eq!(
            config_path_from(None, &root),
            PathBuf::from("/repo/config/config.json")
        );
        assert_eq!(
            config_path_from(Some(""), &root),
            PathBuf::from("/repo/config/config.json")
        );
    }

    #[test]
    fn read_config_rejects_missing_file_and_bad_json() {
        let dir = temp_dir("cfg");
        let err = read_config(&dir.join("missing.json")).expect_err("missing");
        assert!(err.starts_with("cannot read config"));
        let bad = dir.join("bad.json");
        fs::write(&bad, "{not json").expect("write");
        let err = read_config(&bad).expect_err("bad");
        assert!(err.starts_with("cannot parse config"));
    }

    #[test]
    fn visual_cfg_from_reads_tolerance_and_rejects_bad_values() {
        let ok: Value = serde_json::from_str(r#"{"visual": {"tolerance": 8}}"#).expect("config");
        assert_eq!(visual_cfg_from(&ok), Ok(VisualCfg { tolerance: 8 }));
        let missing: Value = serde_json::from_str(r#"{"app": {}}"#).expect("config");
        assert!(visual_cfg_from(&missing).is_err());
        let too_big: Value =
            serde_json::from_str(r#"{"visual": {"tolerance": 256}}"#).expect("config");
        assert!(visual_cfg_from(&too_big).is_err());
        let negative: Value =
            serde_json::from_str(r#"{"visual": {"tolerance": -1}}"#).expect("config");
        assert!(visual_cfg_from(&negative).is_err());
        let text: Value =
            serde_json::from_str(r#"{"visual": {"tolerance": "8"}}"#).expect("config");
        assert!(visual_cfg_from(&text).is_err());
    }

    #[test]
    fn artifacts_root_from_prefers_env_then_config_then_default() {
        let root = PathBuf::from("/repo");
        let config: Value =
            serde_json::from_str(r#"{"paths": {"artifacts": "out"}}"#).expect("config");
        assert_eq!(
            artifacts_root_from(Some("env-out"), &config, &root),
            PathBuf::from("/repo/env-out")
        );
        assert_eq!(
            artifacts_root_from(None, &config, &root),
            PathBuf::from("/repo/out")
        );
        assert_eq!(
            artifacts_root_from(None, &Value::Null, &root),
            PathBuf::from("/repo/artifacts")
        );
        assert_eq!(
            artifacts_root_from(Some("D:/abs"), &config, &root),
            PathBuf::from("D:/abs")
        );
    }

    #[test]
    fn compare_images_flags_pixel_only_when_a_channel_exceeds_tolerance() {
        let base = solid_image(4, 4, [10, 10, 10, 255]);
        let same = solid_image(4, 4, [10, 10, 10, 255]);
        assert_eq!(
            compare_images(&same, &base, 0),
            Comparison {
                max_delta: 0,
                pixels_over: 0
            }
        );
        let within = solid_image(4, 4, [12, 8, 10, 255]);
        assert_eq!(
            compare_images(&within, &base, 2),
            Comparison {
                max_delta: 2,
                pixels_over: 0
            }
        );
        let over = image_with_pixel(4, 4, [10, 10, 10, 255], (1, 1), [10, 13, 10, 255]);
        assert_eq!(
            compare_images(&over, &base, 2),
            Comparison {
                max_delta: 3,
                pixels_over: 1
            }
        );
    }

    #[test]
    fn compare_images_alpha_channel_participates() {
        let base = solid_image(2, 2, [0, 0, 0, 255]);
        let alpha_shift = solid_image(2, 2, [0, 0, 0, 250]);
        assert_eq!(
            compare_images(&alpha_shift, &base, 4),
            Comparison {
                max_delta: 5,
                pixels_over: 4
            }
        );
    }

    #[test]
    fn compare_pair_passes_when_delta_equals_tolerance() {
        let dir = temp_dir("boundary-eq");
        let shot = dir.join("shot.png");
        let base = dir.join("base.png");
        write_png(&shot, &solid_image(2, 1, [0, 0, 0, 255]));
        write_png(
            &base,
            &image_with_pixel(2, 1, [0, 0, 0, 255], (0, 0), [48, 0, 0, 255]),
        );
        let report = compare_pair("pair.png", &shot, &base, 48);
        assert_eq!(report.verdict, Verdict::Ok);
        assert_eq!(report.max_delta, Some(48));
        assert_eq!(report.pixels_over, Some(0));
        assert_eq!(report.pixels, Some(2));
    }

    #[test]
    fn compare_pair_fails_when_delta_is_one_above_tolerance() {
        let dir = temp_dir("boundary-over");
        let shot = dir.join("shot.png");
        let base = dir.join("base.png");
        write_png(&shot, &solid_image(2, 1, [0, 0, 0, 255]));
        write_png(
            &base,
            &image_with_pixel(2, 1, [0, 0, 0, 255], (0, 0), [49, 0, 0, 255]),
        );
        let report = compare_pair("pair.png", &shot, &base, 48);
        assert_eq!(report.verdict, Verdict::Over { pixels_over: 1 });
        assert_eq!(report.max_delta, Some(49));
    }

    #[test]
    fn compare_pair_identical_images_pass() {
        let dir = temp_dir("identical");
        let shot = dir.join("shot.png");
        let base = dir.join("base.png");
        write_png(&shot, &solid_image(3, 2, [7, 8, 9, 255]));
        write_png(&base, &solid_image(3, 2, [7, 8, 9, 255]));
        let report = compare_pair("pair.png", &shot, &base, 0);
        assert_eq!(report.verdict, Verdict::Ok);
        assert_eq!(report.max_delta, Some(0));
        assert_eq!(report.pixels_over, Some(0));
        assert_eq!(report.pixels, Some(6));
    }

    #[test]
    fn compare_pair_reports_missing_sides() {
        let dir = temp_dir("missing");
        let shot = dir.join("shot.png");
        write_png(&shot, &solid_image(2, 2, [0, 0, 0, 255]));
        let report = compare_pair("shot.png", &shot, &dir.join("base.png"), 0);
        assert_eq!(report.verdict, Verdict::Missing { side: Side::Base });
        let report = compare_pair("shot.png", &dir.join("absent.png"), &shot, 0);
        assert_eq!(report.verdict, Verdict::Missing { side: Side::Shot });
    }

    #[test]
    fn compare_pair_reports_dimension_mismatch_and_over() {
        let dir = temp_dir("dims");
        let shot = dir.join("shot.png");
        let base = dir.join("base.png");
        write_png(&shot, &solid_image(4, 2, [0, 0, 0, 255]));
        write_png(&base, &solid_image(2, 4, [0, 0, 0, 255]));
        let report = compare_pair("pair.png", &shot, &base, 0);
        assert_eq!(
            report.verdict,
            Verdict::DimMismatch {
                shot: (4, 2),
                base: (2, 4)
            }
        );
        write_png(
            &base,
            &image_with_pixel(4, 2, [0, 0, 0, 255], (3, 1), [9, 0, 0, 255]),
        );
        let report = compare_pair("pair.png", &shot, &base, 5);
        assert_eq!(report.verdict, Verdict::Over { pixels_over: 1 });
        assert_eq!(report.max_delta, Some(9));
        assert_eq!(report.pixels, Some(8));
    }

    #[test]
    fn png_names_lists_only_png_files_sorted() {
        let dir = temp_dir("names");
        fs::write(dir.join("b.png"), b"").expect("png");
        fs::write(dir.join("a.png"), b"").expect("png");
        fs::write(dir.join("notes.txt"), b"").expect("txt");
        fs::create_dir(dir.join("nested.png")).expect("dir");
        let names = png_names(&dir).expect("names");
        assert_eq!(
            names.into_iter().collect::<Vec<_>>(),
            vec!["a.png".to_string(), "b.png".to_string()]
        );
        assert!(png_names(&dir.join("absent")).expect("names").is_empty());
    }

    #[test]
    fn run_compare_unions_names_from_both_sides() {
        let shot_dir = temp_dir("union-shot");
        let base_dir = temp_dir("union-base");
        write_png(
            &shot_dir.join("both.png"),
            &solid_image(2, 2, [0, 0, 0, 255]),
        );
        write_png(
            &base_dir.join("both.png"),
            &solid_image(2, 2, [0, 0, 0, 255]),
        );
        write_png(
            &shot_dir.join("extra-shot.png"),
            &solid_image(2, 2, [0, 0, 0, 255]),
        );
        write_png(
            &base_dir.join("extra-base.png"),
            &solid_image(2, 2, [0, 0, 0, 255]),
        );
        let reports = run_compare(&args_for(&shot_dir, &base_dir, false), 0).expect("reports");
        let names: Vec<&str> = reports.iter().map(|r| r.name.as_str()).collect();
        assert_eq!(names, vec!["both.png", "extra-base.png", "extra-shot.png"]);
        assert_eq!(reports[0].verdict, Verdict::Ok);
        assert_eq!(reports[1].verdict, Verdict::Missing { side: Side::Shot });
        assert_eq!(reports[2].verdict, Verdict::Missing { side: Side::Base });
    }

    #[test]
    fn run_update_mirrors_screenshots_into_baselines() {
        let shot_dir = temp_dir("update-shot");
        let base_dir = temp_dir("update-base");
        write_png(
            &base_dir.join("stale.png"),
            &solid_image(1, 1, [0, 0, 0, 255]),
        );
        fs::write(base_dir.join("keep.txt"), b"").expect("txt");
        write_png(
            &shot_dir.join("fresh.png"),
            &solid_image(2, 1, [7, 7, 7, 255]),
        );
        let updated = run_update(&args_for(&shot_dir, &base_dir, true)).expect("update");
        assert_eq!(updated, vec!["fresh.png".to_string()]);
        assert!(base_dir.join("fresh.png").is_file());
        assert!(!base_dir.join("stale.png").exists());
        assert!(base_dir.join("keep.txt").is_file());
        let reloaded = decode(&base_dir.join("fresh.png")).expect("png");
        assert_eq!(reloaded.dimensions(), (2, 1));
    }

    #[test]
    fn render_report_lists_pairs_and_totals() {
        let reports = vec![
            PairReport {
                name: "ok.png".to_string(),
                verdict: Verdict::Ok,
                max_delta: Some(3),
                pixels_over: Some(0),
                pixels: Some(4),
            },
            PairReport {
                name: "bad.png".to_string(),
                verdict: Verdict::Missing { side: Side::Base },
                max_delta: None,
                pixels_over: None,
                pixels: None,
            },
        ];
        let text = render_report(&reports, 7, Path::new("shots"), Path::new("base"));
        assert!(text.contains("# visual diff report"));
        assert!(text.contains("tolerance: 7"));
        assert!(text.contains("| ok.png | ok | 3 | 0 | 4 |"));
        assert!(text.contains("missing on baselines side"));
        assert!(text.contains("2 pairs, 1 ok, 1 failed"));
    }

    #[test]
    fn render_report_prints_dirs_and_escapes_pipes() {
        let reports = vec![PairReport {
            name: "a|b.png".to_string(),
            verdict: Verdict::Over { pixels_over: 2 },
            max_delta: Some(9),
            pixels_over: Some(2),
            pixels: Some(4),
        }];
        let text = render_report(&reports, 5, Path::new("shot-dir"), Path::new("base-dir"));
        assert!(text.contains("screenshots: shot-dir\n"));
        assert!(text.contains("baselines: base-dir\n"));
        assert!(text.contains("| pair | verdict | max delta | pixels over | pixels |"));
        assert!(text.contains("| a/b.png | over tolerance (2 px) | 9 | 2 | 4 |"));
    }

    #[test]
    fn write_report_creates_visual_dir_under_artifacts() {
        let artifacts = temp_dir("report-artifacts");
        let path = write_report("body", &artifacts).expect("report");
        assert_eq!(path, artifacts.join("visual").join("report.md"));
        assert_eq!(fs::read_to_string(&path).expect("text"), "body");
    }

    #[test]
    fn run_rejects_wrong_arg_count_with_usage_message() {
        let env = env_for(&PathBuf::from("/repo"), None, &["only"]);
        assert_eq!(
            run(&env),
            Err("usage: bt-visual-diff <screenshots-dir> <baselines-dir> [--update]".to_string())
        );
    }

    #[test]
    fn run_fails_on_config_without_tolerance() {
        let root = temp_dir("run-no-tol");
        let config = root.join("config.json");
        fs::write(&config, r#"{"visual": {}}"#).expect("write config");
        let env = env_for(
            &root,
            Some(config.to_string_lossy().into_owned()),
            &["shots", "base"],
        );
        assert_eq!(
            run(&env),
            Err("config visual.tolerance missing".to_string())
        );
    }

    #[test]
    fn run_returns_zero_and_writes_report_for_identical_dirs() {
        let root = temp_dir("run-ok");
        let shots = root.join("shots");
        let bases = root.join("bases");
        fs::create_dir_all(&shots).expect("dir");
        fs::create_dir_all(&bases).expect("dir");
        write_png(&shots.join("scene.png"), &solid_image(2, 2, [1, 2, 3, 255]));
        write_png(&bases.join("scene.png"), &solid_image(2, 2, [1, 2, 3, 255]));
        let env = Env {
            argv: argv(&[&shots.to_string_lossy(), &bases.to_string_lossy()]),
            config: Some(write_config(&root, 0)),
            artifacts: Some(root.join("out").to_string_lossy().into_owned()),
            root: root.clone(),
        };
        assert_eq!(run(&env), Ok(0));
        let text =
            fs::read_to_string(root.join("out").join("visual").join("report.md")).expect("report");
        assert!(text.contains("| scene.png | ok | 0 | 0 | 4 |"));
        assert!(text.contains("1 pairs, 1 ok, 0 failed"));
    }

    #[test]
    fn run_returns_diff_code_for_over_tolerance_pair() {
        let root = temp_dir("run-diff");
        let shots = root.join("shots");
        let bases = root.join("bases");
        fs::create_dir_all(&shots).expect("dir");
        fs::create_dir_all(&bases).expect("dir");
        write_png(&shots.join("scene.png"), &solid_image(2, 2, [0, 0, 0, 255]));
        write_png(
            &bases.join("scene.png"),
            &image_with_pixel(2, 2, [0, 0, 0, 255], (1, 1), [1, 0, 0, 255]),
        );
        let env = Env {
            argv: argv(&[&shots.to_string_lossy(), &bases.to_string_lossy()]),
            config: Some(write_config(&root, 0)),
            artifacts: Some(root.join("out").to_string_lossy().into_owned()),
            root: root.clone(),
        };
        assert_eq!(run(&env), Ok(EXIT_DIFF));
        let text =
            fs::read_to_string(root.join("out").join("visual").join("report.md")).expect("report");
        assert!(text.contains("over tolerance (1 px)"));
        assert!(text.contains("1 pairs, 0 ok, 1 failed"));
    }

    #[test]
    fn run_update_mode_mirrors_and_returns_zero() {
        let root = temp_dir("run-update");
        let shots = root.join("shots");
        let bases = root.join("bases");
        fs::create_dir_all(&shots).expect("dir");
        fs::create_dir_all(&bases).expect("dir");
        write_png(&shots.join("fresh.png"), &solid_image(2, 1, [5, 5, 5, 255]));
        write_png(&bases.join("stale.png"), &solid_image(1, 1, [0, 0, 0, 255]));
        let env = Env {
            argv: argv(&[
                &shots.to_string_lossy(),
                &bases.to_string_lossy(),
                "--update",
            ]),
            config: Some(write_config(&root, 0)),
            artifacts: None,
            root: root.clone(),
        };
        assert_eq!(run(&env), Ok(0));
        assert!(bases.join("fresh.png").is_file());
        assert!(!bases.join("stale.png").exists());
    }
}
