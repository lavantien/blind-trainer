use image::GenericImageView;
use serde_json::Value;
use std::collections::BTreeSet;
use std::env;
use std::fs;
use std::io::Read;
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::time::{Duration, Instant};

const SMOKE_TIMEOUT: Duration = Duration::from_secs(180);
const EXPECTED_SHOTS: [&str; 8] = [
    "menu", "drill", "memo", "solve", "algs", "stats", "settings", "fatal",
];

#[derive(Debug, PartialEq, Eq)]
struct Args {
    binary: Option<PathBuf>,
    out_dir: Option<PathBuf>,
}

#[derive(Debug, PartialEq, Eq)]
enum ArgsError {
    Usage,
}

fn parse_args(argv: &[String]) -> Result<Args, ArgsError> {
    if argv.len() > 2 {
        return Err(ArgsError::Usage);
    }
    let mut items = argv.iter().cloned();
    let binary = items.next().map(PathBuf::from);
    let out_dir = items.next().map(PathBuf::from);
    Ok(Args { binary, out_dir })
}

fn workspace_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .and_then(Path::parent)
        .expect("workspace root from manifest dir")
        .to_path_buf()
}

fn resolve_from_env_or_config(
    env_value: Option<String>,
    config_value: Option<&str>,
    root: &Path,
) -> PathBuf {
    let relative = env_value
        .filter(|raw| !raw.is_empty())
        .map(PathBuf::from)
        .or_else(|| config_value.map(PathBuf::from))
        .unwrap_or_else(|| PathBuf::from("artifacts"));
    if relative.is_absolute() {
        relative
    } else {
        root.join(relative)
    }
}

fn config_path_from(env_value: Option<String>, root: &Path) -> PathBuf {
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
struct WindowDims {
    width: u32,
    height: u32,
}

fn window_dims_from(value: &Value) -> Result<WindowDims, String> {
    let section = value
        .pointer("/app/window")
        .ok_or_else(|| "config app.window missing".to_string())?;
    let width = section
        .pointer("/width")
        .and_then(Value::as_u64)
        .and_then(|number| u32::try_from(number).ok())
        .ok_or_else(|| "config app.window.width must be an unsigned integer".to_string())?;
    let height = section
        .pointer("/height")
        .and_then(Value::as_u64)
        .and_then(|number| u32::try_from(number).ok())
        .ok_or_else(|| "config app.window.height must be an unsigned integer".to_string())?;
    Ok(WindowDims { width, height })
}

fn out_dir_from(env_value: Option<String>, value: &Value, root: &Path) -> PathBuf {
    let artifacts = resolve_from_env_or_config(
        env_value,
        value.pointer("/paths/artifacts").and_then(Value::as_str),
        root,
    );
    artifacts.join("e2e")
}

fn clean_pngs(dir: &Path) -> Result<(), String> {
    fs::create_dir_all(dir).map_err(|err| format!("cannot create {}: {err}", dir.display()))?;
    if !dir.is_dir() {
        return Err(format!("not a directory: {}", dir.display()));
    }
    for entry in fs::read_dir(dir).map_err(|err| format!("cannot list {}: {err}", dir.display()))? {
        let entry = entry.map_err(|err| format!("cannot list {}: {err}", dir.display()))?;
        let path = entry.path();
        if path.is_file()
            && path
                .extension()
                .is_some_and(|ext| ext.eq_ignore_ascii_case("png"))
        {
            fs::remove_file(&path)
                .map_err(|err| format!("cannot remove {}: {err}", path.display()))?;
        }
    }
    Ok(())
}

fn expected_names() -> BTreeSet<String> {
    EXPECTED_SHOTS
        .iter()
        .map(|slug| format!("{slug}.png"))
        .collect()
}

fn classify(found: &BTreeSet<String>, expected: &BTreeSet<String>) -> (Vec<String>, Vec<String>) {
    let missing = expected.difference(found).cloned().collect();
    let unexpected = found.difference(expected).cloned().collect();
    (missing, unexpected)
}

fn png_names(dir: &Path) -> Result<BTreeSet<String>, String> {
    let mut names = BTreeSet::new();
    for entry in fs::read_dir(dir).map_err(|err| format!("cannot list {}: {err}", dir.display()))? {
        let entry = entry.map_err(|err| format!("cannot list {}: {err}", dir.display()))?;
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

enum ShotVerdict {
    Ok,
    Fail(String),
}

fn check_shot(path: &Path, dims: WindowDims) -> ShotVerdict {
    let meta = match fs::metadata(path) {
        Ok(meta) => meta,
        Err(err) => return ShotVerdict::Fail(format!("missing: {err}")),
    };
    if !meta.is_file() || meta.len() == 0 {
        return ShotVerdict::Fail("empty or not a file".to_string());
    }
    match image::open(path) {
        Ok(image) => {
            let (width, height) = image.dimensions();
            if (width, height) == (dims.width, dims.height) {
                ShotVerdict::Ok
            } else {
                ShotVerdict::Fail(format!(
                    "dimensions {width}x{height}, expected {}x{}",
                    dims.width, dims.height
                ))
            }
        }
        Err(err) => ShotVerdict::Fail(format!("not a png: {err}")),
    }
}

struct SmokeRun {
    code: Option<i32>,
    stdout: Vec<u8>,
    stderr: Vec<u8>,
}

fn spawn_smoke(binary: &Path, out_dir: &Path) -> std::io::Result<Child> {
    Command::new(binary)
        .arg("--smoke")
        .arg(out_dir)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
}

fn run_smoke(binary: &Path, out_dir: &Path, deadline: Instant) -> Result<SmokeRun, String> {
    let mut child = spawn_smoke(binary, out_dir)
        .map_err(|err| format!("cannot spawn {}: {err}", binary.display()))?;
    let mut stdout_pipe = child.stdout.take();
    let mut stderr_pipe = child.stderr.take();
    let stdout_handle = std::thread::spawn(move || {
        let mut buf = Vec::new();
        if let Some(pipe) = stdout_pipe.as_mut() {
            let _ = pipe.read_to_end(&mut buf);
        }
        buf
    });
    let stderr_handle = std::thread::spawn(move || {
        let mut buf = Vec::new();
        if let Some(pipe) = stderr_pipe.as_mut() {
            let _ = pipe.read_to_end(&mut buf);
        }
        buf
    });
    let code = loop {
        if Instant::now() >= deadline {
            let _ = child.kill();
            let _ = child.wait();
            return Err("smoke run timed out".to_string());
        }
        match child
            .try_wait()
            .map_err(|err| format!("cannot wait for smoke process: {err}"))?
        {
            Some(status) => break status.code(),
            None => std::thread::sleep(Duration::from_millis(100)),
        }
    };
    let stdout = stdout_handle.join().unwrap_or_default();
    let stderr = stderr_handle.join().unwrap_or_default();
    Ok(SmokeRun {
        code,
        stdout,
        stderr,
    })
}

fn tail(bytes: &[u8]) -> String {
    let text = String::from_utf8_lossy(bytes);
    let lines: Vec<&str> = text.lines().collect();
    let start = lines.len().saturating_sub(20);
    lines[start..].join("\n")
}

fn real_main() -> Result<u8, String> {
    let argv: Vec<String> = env::args().skip(1).collect();
    let args = parse_args(&argv)
        .map_err(|ArgsError::Usage| "usage: bt-e2e [blind-trainer.exe] [out-dir]".to_string())?;
    let root = workspace_root();
    let config = read_config(&config_path_from(env::var("BT_CONFIG").ok(), &root))?;
    let dims = window_dims_from(&config)?;
    let out_dir = args
        .out_dir
        .unwrap_or_else(|| out_dir_from(env::var("BT_ARTIFACTS").ok(), &config, &root));
    let binary = args.binary.unwrap_or_else(|| {
        root.join("target")
            .join("release")
            .join("blind-trainer.exe")
    });
    if !binary.is_file() {
        return Err(format!("release binary missing: {}", binary.display()));
    }
    println!("binary: {}", binary.display());
    println!("out dir: {}", out_dir.display());
    println!("window: {}x{}", dims.width, dims.height);
    clean_pngs(&out_dir)?;
    let deadline = Instant::now() + SMOKE_TIMEOUT;
    let run = run_smoke(&binary, &out_dir, deadline)?;
    match run.code {
        Some(0) => {}
        Some(code) => {
            return Err(format!(
                "smoke run failed with exit code {code}\nstdout tail:\n{}\nstderr tail:\n{}",
                tail(&run.stdout),
                tail(&run.stderr)
            ));
        }
        None => {
            return Err(format!(
                "smoke process terminated without exit code\nstdout tail:\n{}\nstderr tail:\n{}",
                tail(&run.stdout),
                tail(&run.stderr)
            ));
        }
    }
    let found = png_names(&out_dir)?;
    let expected = expected_names();
    let (missing, unexpected) = classify(&found, &expected);
    let mut failures = 0;
    for name in &missing {
        println!("FAIL {name}: missing screenshot");
        failures += 1;
    }
    for name in &unexpected {
        println!("FAIL {name}: unexpected screenshot");
        failures += 1;
    }
    for name in &expected {
        if missing.contains(name) {
            continue;
        }
        match check_shot(&out_dir.join(name), dims) {
            ShotVerdict::Ok => println!("ok {name}"),
            ShotVerdict::Fail(reason) => {
                println!("FAIL {name}: {reason}");
                failures += 1;
            }
        }
    }
    println!(
        "{} shots expected, {} found, {failures} failed",
        expected.len(),
        found.len()
    );
    if failures > 0 {
        return Ok(1);
    }
    Ok(0)
}

fn main() -> std::process::ExitCode {
    match real_main() {
        Ok(code) => std::process::ExitCode::from(code),
        Err(error) => {
            eprintln!("bt-e2e: {error}");
            std::process::ExitCode::from(2)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use image::RgbaImage;
    use std::sync::atomic::{AtomicU64, Ordering};

    fn temp_dir(tag: &str) -> PathBuf {
        static COUNTER: AtomicU64 = AtomicU64::new(0);
        let dir = env::temp_dir().join("bt-e2e-tests").join(format!(
            "{tag}-{}-{}",
            std::process::id(),
            COUNTER.fetch_add(1, Ordering::Relaxed)
        ));
        fs::create_dir_all(&dir).expect("temp dir");
        dir
    }

    fn argv(items: &[&str]) -> Vec<String> {
        items.iter().map(|item| item.to_string()).collect()
    }

    #[test]
    fn parse_args_takes_optional_binary_and_out_dir() {
        assert_eq!(
            parse_args(&argv(&[])),
            Ok(Args {
                binary: None,
                out_dir: None
            })
        );
        assert_eq!(
            parse_args(&argv(&["bin.exe"])),
            Ok(Args {
                binary: Some(PathBuf::from("bin.exe")),
                out_dir: None
            })
        );
        assert_eq!(
            parse_args(&argv(&["bin.exe", "out"])),
            Ok(Args {
                binary: Some(PathBuf::from("bin.exe")),
                out_dir: Some(PathBuf::from("out"))
            })
        );
        assert_eq!(parse_args(&argv(&["a", "b", "c"])), Err(ArgsError::Usage));
    }

    #[test]
    fn window_dims_from_reads_config_and_rejects_bad_values() {
        let ok: Value = serde_json::from_str(
            r#"{"app": {"window": {"width": 1280, "height": 800, "title": "t"}}}"#,
        )
        .expect("config");
        assert_eq!(
            window_dims_from(&ok),
            Ok(WindowDims {
                width: 1280,
                height: 800
            })
        );
        let missing: Value = serde_json::from_str(r#"{"app": {}}"#).expect("config");
        assert!(window_dims_from(&missing).is_err());
        let malformed: Value =
            serde_json::from_str(r#"{"app": {"window": {"width": "wide", "height": 800}}}"#)
                .expect("config");
        assert!(window_dims_from(&malformed).is_err());
        let negative: Value =
            serde_json::from_str(r#"{"app": {"window": {"width": -1, "height": 800}}}"#)
                .expect("config");
        assert!(window_dims_from(&negative).is_err());
    }

    #[test]
    fn out_dir_from_prefers_env_then_config_then_default() {
        let root = PathBuf::from("/repo");
        let config: Value =
            serde_json::from_str(r#"{"paths": {"artifacts": "out"}}"#).expect("config");
        assert_eq!(
            out_dir_from(Some("env-out".into()), &config, &root),
            PathBuf::from("/repo/env-out/e2e")
        );
        assert_eq!(
            out_dir_from(None, &config, &root),
            PathBuf::from("/repo/out/e2e")
        );
        assert_eq!(
            out_dir_from(None, &Value::Null, &root),
            PathBuf::from("/repo/artifacts/e2e")
        );
    }

    #[test]
    fn expected_names_cover_every_tour_slug_once() {
        let names = expected_names();
        assert_eq!(names.len(), EXPECTED_SHOTS.len());
        for slug in EXPECTED_SHOTS {
            assert!(names.contains(&format!("{slug}.png")));
        }
    }

    #[test]
    fn classify_splits_missing_and_unexpected() {
        let found: BTreeSet<String> = ["menu.png", "drill.png", "stray.png"]
            .iter()
            .map(|name| name.to_string())
            .collect();
        let (missing, unexpected) = classify(&found, &expected_names());
        assert!(missing.contains(&"memo.png".to_string()));
        assert!(!missing.contains(&"menu.png".to_string()));
        assert_eq!(unexpected, vec!["stray.png".to_string()]);
    }

    #[test]
    fn clean_pngs_removes_only_png_files() {
        let dir = temp_dir("clean");
        fs::write(dir.join("old.png"), b"").expect("png");
        fs::write(dir.join("keep.txt"), b"").expect("txt");
        clean_pngs(&dir).expect("clean");
        assert!(!dir.join("old.png").exists());
        assert!(dir.join("keep.txt").is_file());
        let fresh = temp_dir("clean-absent");
        clean_pngs(&fresh.join("nested")).expect("clean creates dir");
    }

    #[test]
    fn png_names_lists_sorted_png_files() {
        let dir = temp_dir("names");
        fs::write(dir.join("b.png"), b"").expect("png");
        fs::write(dir.join("a.png"), b"").expect("png");
        fs::write(dir.join("notes.txt"), b"").expect("txt");
        let names = png_names(&dir).expect("names");
        assert_eq!(
            names.into_iter().collect::<Vec<_>>(),
            vec!["a.png".to_string(), "b.png".to_string()]
        );
    }

    #[test]
    fn check_shot_accepts_valid_png_and_rejects_defects() {
        let dir = temp_dir("shots");
        let dims = WindowDims {
            width: 3,
            height: 2,
        };
        let good = dir.join("good.png");
        RgbaImage::from_pixel(3, 2, image::Rgba([9, 9, 9, 255]))
            .save(&good)
            .expect("png");
        assert!(matches!(check_shot(&good, dims), ShotVerdict::Ok));
        let wrong_dims = dir.join("wrong.png");
        RgbaImage::from_pixel(2, 2, image::Rgba([9, 9, 9, 255]))
            .save(&wrong_dims)
            .expect("png");
        assert!(matches!(
            check_shot(&wrong_dims, dims),
            ShotVerdict::Fail(_)
        ));
        let empty = dir.join("empty.png");
        fs::write(&empty, b"").expect("png");
        assert!(matches!(check_shot(&empty, dims), ShotVerdict::Fail(_)));
        assert!(matches!(
            check_shot(&dir.join("absent.png"), dims),
            ShotVerdict::Fail(_)
        ));
    }

    #[test]
    fn tail_keeps_last_lines() {
        let text: Vec<u8> = (1..=50)
            .map(|line| format!("line {line}\n"))
            .collect::<Vec<_>>()
            .concat()
            .into_bytes();
        let tailed = tail(&text);
        assert!(tailed.starts_with("line 31"));
        assert!(tailed.ends_with("line 50"));
    }
}
