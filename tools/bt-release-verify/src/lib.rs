pub mod exec;
pub mod zipcheck;

use exec::{ChildSpec, Outcome, scrub_env};
use image::GenericImageView;
use serde_json::Value;
use std::collections::BTreeSet;
use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::Duration;

pub const EXIT_USAGE: u8 = 2;
const EXIT_FAIL: u8 = 1;
const USAGE: &str = "usage: bt-release-verify <staging-dir> <zip> [--run]";

static WORK_COUNTER: AtomicU64 = AtomicU64::new(0);

#[derive(Clone, Debug, PartialEq, Eq)]
struct ReleaseCfg {
    staging_dir: PathBuf,
    exe: String,
    config_file: String,
    fonts_dir: String,
    run_timeout_secs: u64,
    neg_fonts_exit: i32,
    usage_exit: i32,
    shots: Vec<String>,
}

pub struct Env {
    argv: Vec<String>,
    config: Option<String>,
    vars: Vec<(String, String)>,
    root: PathBuf,
}

impl Env {
    pub fn from_process() -> Self {
        Self {
            argv: env::args().skip(1).collect(),
            config: env::var("BT_CONFIG").ok(),
            vars: env::vars_os()
                .map(|(key, value)| {
                    (
                        key.to_string_lossy().into_owned(),
                        value.to_string_lossy().into_owned(),
                    )
                })
                .collect(),
            root: workspace_root(),
        }
    }
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

pub fn run(env: &Env) -> Result<u8, String> {
    let args = parse_args(&env.argv).map_err(|_| USAGE.to_string())?;
    let config = read_config(&config_path_from(env.config.as_deref(), &env.root))?;
    let cfg = release_cfg_from(&config)?;
    let root = zip_root_from(&args.staging)?;
    match verify(env, &args, &cfg, &root) {
        Ok(()) => Ok(0),
        Err(code) => Ok(code),
    }
}

fn verify(env: &Env, args: &ParsedArgs, cfg: &ReleaseCfg, root: &str) -> Result<(), u8> {
    let dims = stage_ok("zip check", zip_stage(&args.staging, &args.zip, root, cfg))?;
    if !args.run {
        return Ok(());
    }
    let work = work_root();
    let extract_dir = work.join("extract");
    stage_ok("extract", extract_stage(&args.zip, &extract_dir, root))?;
    let exe_path = extract_dir.join(&cfg.exe);
    stage_ok(
        "p1 smoke",
        smoke_stage(&exe_path, &work.join("p1"), cfg, dims, &env.vars, None),
    )?;
    let packaged = extract_dir.join(&cfg.config_file);
    stage_ok(
        "p2 smoke",
        smoke_stage(
            &exe_path,
            &work.join("p2"),
            cfg,
            dims,
            &env.vars,
            Some(&packaged),
        ),
    )?;
    stage_ok(
        "n1 missing fonts",
        n1_stage(&exe_path, &extract_dir, &work.join("n1"), cfg, &env.vars),
    )?;
    stage_ok(
        "n2 usage",
        n2_stage(&exe_path, &work.join("n2"), cfg, &env.vars),
    )?;
    Ok(())
}

fn stage_ok<T>(name: &str, outcome: Result<(T, String), String>) -> Result<T, u8> {
    match outcome {
        Ok((value, detail)) => {
            println!("ok: {name} ({detail})");
            Ok(value)
        }
        Err(detail) => {
            eprintln!("fail: {name}: {detail}");
            Err(EXIT_FAIL)
        }
    }
}

fn work_root() -> PathBuf {
    env::temp_dir().join("bt-release-verify-run").join(format!(
        "{}-{}",
        process::id(),
        WORK_COUNTER.fetch_add(1, Ordering::Relaxed)
    ))
}

fn parse_args(argv: &[String]) -> Result<ParsedArgs, String> {
    let mut positional: Vec<PathBuf> = Vec::new();
    let mut run = false;
    for arg in argv {
        match arg.as_str() {
            "--run" => run = true,
            other if other.starts_with("--") => return Err(format!("unknown flag {other}")),
            other => positional.push(PathBuf::from(other)),
        }
    }
    if positional.len() != 2 {
        return Err(USAGE.to_string());
    }
    let staging = positional.first().expect("two positionals").clone();
    let zip = positional.get(1).expect("two positionals").clone();
    Ok(ParsedArgs { staging, zip, run })
}

#[derive(Debug, PartialEq, Eq)]
struct ParsedArgs {
    staging: PathBuf,
    zip: PathBuf,
    run: bool,
}

fn cfg_str(section: &Value, key: &str) -> Result<String, String> {
    section
        .pointer(&format!("/{key}"))
        .and_then(Value::as_str)
        .map(str::to_string)
        .ok_or_else(|| format!("config release.{key} missing"))
}

fn cfg_u64(section: &Value, key: &str) -> Result<u64, String> {
    section
        .pointer(&format!("/{key}"))
        .and_then(Value::as_u64)
        .filter(|value| *value >= 1)
        .ok_or_else(|| format!("config release.{key} must be a positive integer"))
}

fn cfg_i32(section: &Value, key: &str) -> Result<i32, String> {
    section
        .pointer(&format!("/{key}"))
        .and_then(Value::as_i64)
        .and_then(|value| i32::try_from(value).ok())
        .ok_or_else(|| format!("config release.{key} must be an exit code integer"))
}

fn release_cfg_from(value: &Value) -> Result<ReleaseCfg, String> {
    let section = value
        .pointer("/release")
        .ok_or("config release section missing")?;
    let shots = value
        .pointer("/e2e/shots")
        .ok_or("config e2e.shots missing")?
        .as_array()
        .ok_or("config e2e.shots must be a list")?
        .iter()
        .map(|shot| {
            shot.as_str()
                .map(str::to_string)
                .ok_or_else(|| "config e2e.shots must hold names".to_string())
        })
        .collect::<Result<Vec<String>, String>>()?;
    Ok(ReleaseCfg {
        staging_dir: PathBuf::from(cfg_str(section, "staging_dir")?),
        exe: cfg_str(section, "exe")?,
        config_file: cfg_str(section, "config_file")?,
        fonts_dir: cfg_str(section, "fonts_dir")?,
        run_timeout_secs: cfg_u64(section, "run_timeout_secs")?,
        neg_fonts_exit: cfg_i32(section, "neg_fonts_exit")?,
        usage_exit: cfg_i32(section, "usage_exit")?,
        shots,
    })
}

fn packaged_window_dims(staging: &Path, config_file: &str) -> Result<(u32, u32), String> {
    let path = staging.join(config_file);
    let text = fs::read_to_string(&path)
        .map_err(|err| format!("cannot read packaged config {}: {err}", path.display()))?;
    let value: Value = serde_json::from_str(&text)
        .map_err(|err| format!("cannot parse packaged config {}: {err}", path.display()))?;
    let dim = |key: &str| -> Result<u32, String> {
        value
            .pointer(&format!("/app/window/{key}"))
            .and_then(Value::as_u64)
            .and_then(|raw| u32::try_from(raw).ok())
            .ok_or_else(|| format!("packaged config app.window.{key} missing"))
    };
    Ok((dim("width")?, dim("height")?))
}

fn zip_root_from(staging: &Path) -> Result<String, String> {
    staging
        .file_name()
        .and_then(|name| name.to_str())
        .map(str::to_string)
        .ok_or_else(|| format!("staging dir {} has no folder name", staging.display()))
}

fn zip_stage(
    staging: &Path,
    zip: &Path,
    root: &str,
    cfg: &ReleaseCfg,
) -> Result<((u32, u32), String), String> {
    let set = zipcheck::staging_file_set(staging, &cfg.exe, &cfg.config_file, &cfg.fonts_dir)?;
    let count = zipcheck::check_zip(zip, root, &set)?;
    let dims = packaged_window_dims(staging, &cfg.config_file)?;
    Ok((
        dims,
        format!("{count} files byte-identical, window {}x{}", dims.0, dims.1),
    ))
}

fn extract_stage(zip: &Path, dest: &Path, root: &str) -> Result<((), String), String> {
    fs::create_dir_all(dest).map_err(|err| format!("mkdir {}: {err}", dest.display()))?;
    let count = zipcheck::extract_zip(zip, dest, root)?;
    Ok(((), format!("{count} files at {}", dest.display())))
}

fn check_exit(outcome: &Outcome, expected: i32) -> Result<(), String> {
    match outcome {
        Outcome::Exit(code) if *code == expected => Ok(()),
        Outcome::Exit(code) => Err(format!("exit code {code}, expected {expected}")),
        Outcome::Timeout => Err("timed out".to_string()),
        Outcome::Spawn(detail) => Err(format!("did not run: {detail}")),
    }
}

fn check_shot_dir(dir: &Path, shots: &[String], dims: (u32, u32)) -> Result<usize, String> {
    let expected: BTreeSet<String> = shots.iter().map(|shot| format!("{shot}.png")).collect();
    let actual: BTreeSet<String> = fs::read_dir(dir)
        .map_err(|err| format!("cannot read shots dir {}: {err}", dir.display()))?
        .filter_map(|entry| entry.ok())
        .filter(|entry| entry.path().is_file())
        .filter_map(|entry| entry.file_name().to_str().map(str::to_string))
        .collect();
    for name in &expected {
        if !actual.contains(name) {
            return Err(format!("missing shot {name}"));
        }
    }
    for name in &actual {
        if !expected.contains(name) {
            return Err(format!("unexpected file {name} in shots dir"));
        }
    }
    for name in &actual {
        let path = dir.join(name);
        let shot = image::open(&path)
            .map_err(|err| format!("shot {name} is not a decodable image: {err}"))?;
        let got = shot.dimensions();
        if got != dims {
            return Err(format!(
                "shot {name} is {}x{}, expected {}x{}",
                got.0, got.1, dims.0, dims.1
            ));
        }
    }
    Ok(actual.len())
}

fn stage_spec(
    program: &Path,
    args: &[&str],
    cwd: &Path,
    vars: &[(String, String)],
    extras: &[(&str, &str)],
) -> ChildSpec {
    let mut env_map = scrub_env(vars.iter().cloned());
    for (key, value) in extras {
        env_map.insert(key.to_string(), value.to_string());
    }
    ChildSpec {
        program: program.to_path_buf(),
        args: args.iter().map(|arg| arg.to_string()).collect(),
        cwd: cwd.to_path_buf(),
        env: env_map,
    }
}

fn prep_sandbox(sandbox_root: &Path) -> Result<(PathBuf, PathBuf), String> {
    let cwd = sandbox_root.join("cwd");
    let appdata = sandbox_root.join("appdata");
    for dir in [&cwd, &appdata] {
        fs::create_dir_all(dir).map_err(|err| format!("mkdir {}: {err}", dir.display()))?;
    }
    Ok((cwd, appdata))
}

fn smoke_stage(
    exe: &Path,
    sandbox_root: &Path,
    cfg: &ReleaseCfg,
    dims: (u32, u32),
    vars: &[(String, String)],
    bt_config: Option<&Path>,
) -> Result<(usize, String), String> {
    let (cwd, appdata) = prep_sandbox(sandbox_root)?;
    let shots = sandbox_root.join("shots");
    let shots_arg = shots.to_string_lossy().into_owned();
    let appdata_arg = appdata.to_string_lossy().into_owned();
    let config_arg = bt_config.map(|path| path.to_string_lossy().into_owned());
    let mut extras: Vec<(&str, &str)> = vec![("APPDATA", appdata_arg.as_str())];
    if let Some(config) = config_arg.as_deref() {
        extras.push(("BT_CONFIG", config));
    }
    let spec = stage_spec(exe, &["--smoke", &shots_arg], &cwd, vars, &extras);
    let result = exec::run_child(&spec, Duration::from_secs(cfg.run_timeout_secs));
    check_exit(&result.outcome, 0)?;
    let count = check_shot_dir(&shots, &cfg.shots, dims)?;
    Ok((
        count,
        format!("exit 0, {count} shots at {}x{}", dims.0, dims.1),
    ))
}

fn n1_stage(
    exe: &Path,
    extract_dir: &Path,
    sandbox_root: &Path,
    cfg: &ReleaseCfg,
    vars: &[(String, String)],
) -> Result<((), String), String> {
    let fonts = extract_dir.join(&cfg.fonts_dir);
    fs::remove_dir_all(&fonts).ok();
    if fonts.exists() {
        return Err(format!(
            "cannot remove packaged fonts dir {}",
            fonts.display()
        ));
    }
    let (cwd, appdata) = prep_sandbox(sandbox_root)?;
    let shots_arg = sandbox_root.join("shots").to_string_lossy().into_owned();
    let appdata_arg = appdata.to_string_lossy().into_owned();
    let extras: Vec<(&str, &str)> = vec![("APPDATA", appdata_arg.as_str())];
    let spec = stage_spec(exe, &["--smoke", &shots_arg], &cwd, vars, &extras);
    let result = exec::run_child(&spec, Duration::from_secs(cfg.run_timeout_secs));
    check_exit(&result.outcome, cfg.neg_fonts_exit)?;
    Ok(((), format!("exit {}", cfg.neg_fonts_exit)))
}

fn n2_stage(
    exe: &Path,
    sandbox_root: &Path,
    cfg: &ReleaseCfg,
    vars: &[(String, String)],
) -> Result<((), String), String> {
    let (cwd, appdata) = prep_sandbox(sandbox_root)?;
    let appdata_arg = appdata.to_string_lossy().into_owned();
    let extras: Vec<(&str, &str)> = vec![("APPDATA", appdata_arg.as_str())];
    let spec = stage_spec(exe, &["--smoke"], &cwd, vars, &extras);
    let result = exec::run_child(&spec, Duration::from_secs(cfg.run_timeout_secs));
    check_exit(&result.outcome, cfg.usage_exit)?;
    Ok(((), format!("exit {}", cfg.usage_exit)))
}

#[cfg(test)]
mod tests {
    use super::*;
    use exec::Outcome;
    use image::RgbaImage;
    use std::fs;
    use std::fs::File;
    use std::io::Write;
    use std::sync::atomic::{AtomicU64, Ordering};
    use zip::ZipWriter;
    use zip::write::SimpleFileOptions;

    static DIR_COUNTER: AtomicU64 = AtomicU64::new(0);

    fn temp_dir(tag: &str) -> PathBuf {
        let dir = std::env::temp_dir()
            .join("bt-release-verify-lib-tests")
            .join(format!(
                "{tag}-{}-{}",
                std::process::id(),
                DIR_COUNTER.fetch_add(1, Ordering::Relaxed)
            ));
        fs::create_dir_all(&dir).expect("temp dir");
        dir
    }

    fn argv(items: &[&str]) -> Vec<String> {
        items.iter().map(|item| item.to_string()).collect()
    }

    fn full_config_json() -> String {
        r#"{
          "release": {
            "staging_dir": "artifacts/dist/blind-trainer",
            "exe": "blind-trainer.exe",
            "config_file": "config/config.json",
            "fonts_dir": "assets/fonts",
            "run_timeout_secs": 120,
            "neg_fonts_exit": 1,
            "usage_exit": 2
          },
          "e2e": {"shots": ["menu", "drill", "memo", "solve", "algs", "stats", "settings", "fatal"]}
        }"#
        .to_string()
    }

    fn parse_cfg(text: &str) -> ReleaseCfg {
        let value: Value = serde_json::from_str(text).expect("config json");
        release_cfg_from(&value).expect("release cfg")
    }

    fn make_staging(dir: &Path) {
        fs::create_dir_all(dir.join("assets/fonts")).expect("fonts dir");
        fs::create_dir_all(dir.join("config")).expect("config dir");
        fs::write(dir.join("blind-trainer.exe"), b"fake exe").expect("exe");
        fs::write(
            dir.join("config/config.json"),
            br#"{"app": {"window": {"width": 1280, "height": 800}}}"#,
        )
        .expect("config");
        fs::write(dir.join("assets/fonts/main.ttf"), b"font").expect("font");
        fs::write(dir.join("assets/fonts/bold.ttf"), b"font").expect("font");
    }

    fn write_dist_zip(zip_path: &Path, staging: &Path) {
        let file = File::create(zip_path).expect("zip file");
        let mut zip = ZipWriter::new(file);
        let options = SimpleFileOptions::default();
        zip.add_directory("blind-trainer/", options).expect("dir");
        for name in [
            "blind-trainer\\blind-trainer.exe",
            "blind-trainer\\config\\config.json",
            "blind-trainer/assets/fonts/main.ttf",
            "blind-trainer/assets/fonts/bold.ttf",
        ] {
            let slashed = name.replace('\\', "/");
            let relative = slashed.strip_prefix("blind-trainer/").unwrap_or(&slashed);
            zip.start_file(name, options).expect("entry");
            zip.write_all(&fs::read(staging.join(relative)).expect("staged"))
                .expect("entry bytes");
        }
        zip.finish().expect("zip finish");
    }

    fn env_for(dir: &Path, config: Option<String>, items: &[&str]) -> Env {
        Env {
            argv: argv(items),
            config,
            vars: Vec::new(),
            root: dir.to_path_buf(),
        }
    }

    #[test]
    fn parse_args_reads_two_positionals_and_the_run_flag() {
        let plain = parse_args(&argv(&["staging", "dist.zip"])).expect("plain");
        assert!(!plain.run);
        let flagged = parse_args(&argv(&["staging", "dist.zip", "--run"])).expect("flagged");
        assert!(flagged.run);
        let first = parse_args(&argv(&["--run", "staging", "dist.zip"])).expect("first");
        assert!(first.run);
        assert_eq!(first.staging, PathBuf::from("staging"));
        assert_eq!(first.zip, PathBuf::from("dist.zip"));
    }

    #[test]
    fn parse_args_rejects_wrong_counts_and_unknown_flags() {
        assert!(parse_args(&argv(&[])).is_err());
        assert!(parse_args(&argv(&["staging"])).is_err());
        assert!(parse_args(&argv(&["a", "b", "c"])).is_err());
        assert!(parse_args(&argv(&["a", "b", "--wat"])).is_err());
        assert!(parse_args(&argv(&["a", "b", "--run", "--run"])).is_ok());
    }

    #[test]
    fn release_cfg_from_reads_every_tunable() {
        let cfg = parse_cfg(&full_config_json());
        assert_eq!(cfg.exe, "blind-trainer.exe");
        assert_eq!(cfg.config_file, "config/config.json");
        assert_eq!(cfg.fonts_dir, "assets/fonts");
        assert_eq!(cfg.run_timeout_secs, 120);
        assert_eq!(cfg.neg_fonts_exit, 1);
        assert_eq!(cfg.usage_exit, 2);
        assert_eq!(
            cfg.shots,
            vec![
                "menu", "drill", "memo", "solve", "algs", "stats", "settings", "fatal"
            ]
        );
        assert_eq!(
            cfg.staging_dir,
            PathBuf::from("artifacts/dist/blind-trainer")
        );
    }

    #[test]
    fn release_cfg_from_names_the_missing_key_or_section() {
        let err = release_cfg_from(&Value::Null).expect_err("no release");
        assert_eq!(err, "config release section missing");
        let missing_exe: Value = serde_json::from_str(&full_config_json().replace(
            "\"exe\": \"blind-trainer.exe\",\n            \"config_file\"",
            "\"config_file\"",
        ))
        .expect("json");
        let err = release_cfg_from(&missing_exe).expect_err("no exe");
        assert_eq!(err, "config release.exe missing");
        let missing_shots: Value = serde_json::from_str(&full_config_json().replace(
            ",\n          \"e2e\": {\"shots\": [\"menu\", \"drill\", \"memo\", \"solve\", \"algs\", \"stats\", \"settings\", \"fatal\"]}",
            "",
        ))
        .expect("json");
        let err = release_cfg_from(&missing_shots).expect_err("no shots");
        assert_eq!(err, "config e2e.shots missing");
    }

    #[test]
    fn release_cfg_from_rejects_bad_value_types() {
        let bad_timeout: Value =
            serde_json::from_str(&full_config_json().replace("120", "\"soon\"")).expect("json");
        let err = release_cfg_from(&bad_timeout).expect_err("bad timeout");
        assert!(err.contains("run_timeout_secs"), "err: {err}");
        let bad_exit: Value = serde_json::from_str(
            &full_config_json().replace("\"usage_exit\": 2", "\"usage_exit\": \"two\""),
        )
        .expect("json");
        let err = release_cfg_from(&bad_exit).expect_err("bad exit");
        assert!(err.contains("usage_exit"), "err: {err}");
        let bad_shots: Value =
            serde_json::from_str(&full_config_json().replace("[\"menu\",", "[7,")).expect("json");
        let err = release_cfg_from(&bad_shots).expect_err("bad shots");
        assert!(err.contains("e2e.shots"), "err: {err}");
        let zero_timeout: Value = serde_json::from_str(
            &full_config_json().replace("\"run_timeout_secs\": 120", "\"run_timeout_secs\": 0"),
        )
        .expect("json");
        let err = release_cfg_from(&zero_timeout).expect_err("zero timeout");
        assert!(err.contains("run_timeout_secs"), "err: {err}");
    }

    #[test]
    fn packaged_window_dims_reads_the_staged_config() {
        let dir = temp_dir("dims");
        make_staging(&dir);
        assert_eq!(
            packaged_window_dims(&dir, "config/config.json"),
            Ok((1280, 800))
        );
        fs::write(dir.join("config/config.json"), r#"{"app": {}}"#).expect("config");
        let err = packaged_window_dims(&dir, "config/config.json").expect_err("no window");
        assert!(err.contains("app.window.width"), "err: {err}");
    }

    #[test]
    fn zip_root_from_uses_the_staging_folder_name() {
        assert_eq!(
            zip_root_from(Path::new("out/dist/blind-trainer")),
            Ok("blind-trainer".to_string())
        );
        assert!(zip_root_from(Path::new("/")).is_err());
    }

    #[test]
    fn check_exit_classifies_each_outcome() {
        assert_eq!(check_exit(&Outcome::Exit(0), 0), Ok(()));
        assert_eq!(check_exit(&Outcome::Exit(1), 1), Ok(()));
        let wrong = check_exit(&Outcome::Exit(2), 1).expect_err("wrong code");
        assert!(wrong.contains("exit code 2"), "err: {wrong}");
        let timeout = check_exit(&Outcome::Timeout, 0).expect_err("timeout");
        assert!(timeout.contains("timed out"), "err: {timeout}");
        let spawn = check_exit(&Outcome::Spawn("nope".to_string()), 0).expect_err("spawn");
        assert!(spawn.contains("did not run"), "err: {spawn}");
    }

    fn write_png(path: &Path, width: u32, height: u32) {
        RgbaImage::from_pixel(width, height, image::Rgba([9, 9, 9, 255]))
            .save(path)
            .expect("png");
    }

    #[test]
    fn check_shot_dir_requires_the_exact_shot_set_at_the_right_dims() {
        let dir = temp_dir("shots");
        write_png(&dir.join("menu.png"), 1280, 800);
        write_png(&dir.join("drill.png"), 1280, 800);
        let shots = vec!["menu".to_string(), "drill".to_string()];
        assert_eq!(check_shot_dir(&dir, &shots, (1280, 800)), Ok(2));
        let wanted = vec!["menu".to_string(), "drill".to_string(), "memo".to_string()];
        let missing = check_shot_dir(&dir, &wanted, (1280, 800)).expect_err("missing");
        assert!(
            missing.contains("missing") && missing.contains("memo.png"),
            "err: {missing}"
        );
        fs::write(dir.join("stray.txt"), b"").expect("stray");
        let stray = check_shot_dir(&dir, &shots, (1280, 800)).expect_err("stray");
        assert!(
            stray.contains("unexpected") && stray.contains("stray.txt"),
            "err: {stray}"
        );
        fs::remove_file(dir.join("stray.txt")).expect("remove stray");
        let wrong = check_shot_dir(&dir, &shots, (640, 480)).expect_err("wrong dims");
        assert!(
            wrong.contains("640x480") && wrong.contains("1280x800"),
            "err: {wrong}"
        );
    }

    #[test]
    fn check_shot_dir_names_every_shot_missing_from_an_empty_dir() {
        let dir = temp_dir("shots-empty");
        fs::create_dir_all(&dir).expect("dir");
        let err = check_shot_dir(&dir, &["menu".to_string()], (10, 10)).expect_err("empty");
        assert!(
            err.contains("missing") && err.contains("menu.png"),
            "err: {err}"
        );
        let absent = temp_dir("shots-absent");
        let err = check_shot_dir(&absent.join("none"), &["menu".to_string()], (10, 10))
            .expect_err("absent");
        assert!(err.contains("cannot read shots dir"), "err: {err}");
    }

    #[test]
    fn stage_spec_scrubs_bt_vars_and_applies_extras() {
        let program = PathBuf::from("dist/blind-trainer.exe");
        let spec = stage_spec(
            &program,
            &["--smoke", "shots"],
            Path::new("foreign"),
            &[
                ("BT_CONFIG".to_string(), "leak.json".to_string()),
                ("PATH".to_string(), "c:/bin".to_string()),
            ],
            &[
                ("APPDATA", "C:/sandbox"),
                ("BT_CONFIG", "C:/dist/config/config.json"),
            ],
        );
        assert_eq!(spec.program, program);
        assert_eq!(spec.args, vec!["--smoke", "shots"]);
        assert_eq!(spec.cwd, PathBuf::from("foreign"));
        assert_eq!(
            spec.env.get("BT_CONFIG").map(String::as_str),
            Some("C:/dist/config/config.json")
        );
        assert_eq!(
            spec.env.get("APPDATA").map(String::as_str),
            Some("C:/sandbox")
        );
        assert_eq!(spec.env.get("PATH").map(String::as_str), Some("c:/bin"));
    }

    #[test]
    fn run_rejects_wrong_arg_count_with_the_usage_line() {
        let dir = temp_dir("run-usage");
        let env = env_for(
            &dir,
            Some(dir.join("config.json").to_string_lossy().into_owned()),
            &["only"],
        );
        assert_eq!(
            run(&env),
            Err("usage: bt-release-verify <staging-dir> <zip> [--run]".to_string())
        );
    }

    #[test]
    fn run_names_the_missing_release_section() {
        let dir = temp_dir("run-no-release");
        let config = dir.join("config.json");
        fs::write(&config, r#"{"app": {}}"#).expect("config");
        let env = env_for(
            &dir,
            Some(config.to_string_lossy().into_owned()),
            &["staging", "dist.zip"],
        );
        assert_eq!(run(&env), Err("config release section missing".to_string()));
    }

    fn dist_fixture(tag: &str) -> (PathBuf, PathBuf, String) {
        let dir = temp_dir(tag);
        let staging = dir.join("blind-trainer");
        make_staging(&staging);
        let zip = dir.join("blind-trainer-v0.0.0-windows-x64.zip");
        write_dist_zip(&zip, &staging);
        let config = dir.join("verify-config.json");
        fs::write(&config, full_config_json()).expect("config");
        (staging, zip, config.to_string_lossy().into_owned())
    }

    fn run_dist(tag: &str, extra: &[&str]) -> Result<u8, String> {
        let (staging, zip, config) = dist_fixture(tag);
        let base = staging.parent().expect("parent").to_path_buf();
        let items: Vec<String> = std::iter::once(staging.to_string_lossy().into_owned())
            .chain(std::iter::once(zip.to_string_lossy().into_owned()))
            .chain(extra.iter().map(|item| item.to_string()))
            .collect();
        let refs: Vec<&str> = items.iter().map(String::as_str).collect();
        run(&env_for(&base, Some(config), &refs))
    }

    #[test]
    fn run_passes_the_zip_stage_without_run_flag() {
        assert_eq!(run_dist("run-pass", &[]), Ok(0));
    }

    #[test]
    fn run_fails_the_whole_tool_when_the_zip_is_tampered() {
        let dir = temp_dir("run-tamper");
        let staging = dir.join("blind-trainer");
        make_staging(&staging);
        let zip = dir.join("dist.zip");
        write_dist_zip(&zip, &staging);
        fs::write(staging.join("assets/fonts/main.ttf"), b"different bytes").expect("tamper");
        let config = dir.join("verify-config.json");
        fs::write(&config, full_config_json()).expect("config");
        let env = env_for(
            &dir,
            Some(config.to_string_lossy().into_owned()),
            &[&staging.to_string_lossy(), &zip.to_string_lossy()],
        );
        assert_eq!(run(&env), Ok(EXIT_FAIL));
    }

    #[test]
    fn run_reports_the_spawn_failure_when_the_packaged_exe_is_not_runnable() {
        assert_eq!(run_dist("run-spawn", &["--run"]), Ok(EXIT_FAIL));
    }
}
