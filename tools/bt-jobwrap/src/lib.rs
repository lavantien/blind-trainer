use std::path::PathBuf;

pub mod ffi;

pub const USAGE: &str = "usage: bt-jobwrap [--anchor NAME] -- <command> <args...>\n       bt-jobwrap --selftest-alloc-mb <N>\n       bt-jobwrap --selftest-hold-secs <N>";

pub const DOTNET_TICKS_AT_FILETIME_ZERO: u64 = 504911232000000000;

#[derive(Clone, PartialEq, Eq, Debug)]
pub enum Invocation {
    Wrap {
        anchor: Option<String>,
        payload: Vec<String>,
    },
    AllocMb(u64),
    HoldSecs(u64),
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub enum WrapError {
    Usage(String),
    Failure(String),
}

pub fn dotnet_ticks(filetime: u64) -> u64 {
    filetime + DOTNET_TICKS_AT_FILETIME_ZERO
}

pub fn is_anchor_name(raw: &str) -> bool {
    !raw.is_empty()
        && raw
            .chars()
            .all(|ch| ch.is_ascii_alphanumeric() || matches!(ch, '.' | '_' | '-'))
}

pub fn parse_invocation(args: &[String]) -> Result<Invocation, String> {
    let Some(first) = args.first().map(String::as_str) else {
        return Err("no arguments given".to_string());
    };
    match first {
        "--selftest-alloc-mb" => return parse_selftest_number(args, 1, "alloc mb"),
        "--selftest-hold-secs" => return parse_selftest_number(args, 0, "hold secs"),
        _ => {}
    }
    let mut rest = args;
    let mut anchor = None;
    if first == "--anchor" {
        let name = rest
            .get(1)
            .ok_or_else(|| "--anchor needs a name".to_string())?;
        if !is_anchor_name(name) {
            return Err(format!(
                "--anchor name must be alnum dot underscore dash: {name}"
            ));
        }
        anchor = Some(name.clone());
        rest = &rest[2..];
    }
    if rest.first().map(String::as_str) != Some("--") {
        return Err("payload must follow --".to_string());
    }
    let payload = rest[1..].to_vec();
    if payload.is_empty() {
        return Err("payload command is empty".to_string());
    }
    Ok(Invocation::Wrap { anchor, payload })
}

fn parse_selftest_number(args: &[String], minimum: u64, label: &str) -> Result<Invocation, String> {
    if args.len() != 2 {
        return Err(format!("selftest {label} takes exactly one argument"));
    }
    let raw = &args[1];
    let value: u64 = raw
        .parse()
        .map_err(|_| format!("selftest {label} is not a number: {raw}"))?;
    if value < minimum {
        return Err(format!("selftest {label} must be >= {minimum}"));
    }
    if args[0] == "--selftest-alloc-mb" {
        Ok(Invocation::AllocMb(value))
    } else {
        Ok(Invocation::HoldSecs(value))
    }
}

pub fn quote_argument(raw: &str) -> String {
    if raw.is_empty() {
        return "\"\"".to_string();
    }
    if raw.chars().any(|ch| ch.is_whitespace()) {
        format!("\"{raw}\"")
    } else {
        raw.to_string()
    }
}

pub fn command_line(payload: &[String]) -> String {
    payload
        .iter()
        .map(|arg| quote_argument(arg))
        .collect::<Vec<String>>()
        .join(" ")
}

pub fn workspace_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .and_then(|path| path.parent())
        .expect("workspace root from manifest dir")
        .to_path_buf()
}

pub fn config_path_from(env: Option<&str>) -> PathBuf {
    match env.filter(|value| !value.is_empty()) {
        Some(raw) => {
            let path = PathBuf::from(raw);
            if path.is_absolute() {
                path
            } else {
                workspace_root().join(path)
            }
        }
        None => workspace_root().join("config").join("config.json"),
    }
}

pub fn load_config() -> Result<serde_json::Value, String> {
    let path = config_path_from(std::env::var("BT_CONFIG").ok().as_deref());
    let text = std::fs::read_to_string(&path)
        .map_err(|source| format!("read {}: {source}", path.display()))?;
    serde_json::from_str(&text).map_err(|source| format!("parse {}: {source}", path.display()))
}

pub fn per_process_bytes(config: &serde_json::Value) -> Result<u64, String> {
    let raw = config
        .pointer("/job/per_process_bytes")
        .ok_or_else(|| "config section missing: job.per_process_bytes".to_string())?;
    let value = raw
        .as_u64()
        .ok_or_else(|| "config value malformed: job.per_process_bytes".to_string())?;
    if value == 0 {
        return Err("config value invalid: job.per_process_bytes must be > 0".to_string());
    }
    Ok(value)
}

pub fn pid_dir_from(config: &serde_json::Value) -> Result<PathBuf, String> {
    let raw = config
        .pointer("/memguard/pid_dir")
        .and_then(|value| value.as_str())
        .ok_or_else(|| "config section missing: memguard.pid_dir".to_string())?;
    if raw.trim().is_empty() {
        return Err("config value invalid: memguard.pid_dir".to_string());
    }
    let path = PathBuf::from(raw);
    if path.is_absolute() {
        Ok(path)
    } else {
        Ok(workspace_root().join(path))
    }
}

pub fn run(invocation: Invocation) -> Result<u32, WrapError> {
    let Invocation::Wrap { anchor, payload } = invocation else {
        return Err(WrapError::Usage("run needs a wrap invocation".to_string()));
    };
    let config = load_config().map_err(WrapError::Failure)?;
    let ceiling = per_process_bytes(&config).map_err(WrapError::Failure)?;
    let guard = match &anchor {
        Some(name) => Some(
            write_pid_file(&config, name)
                .map_err(|source| WrapError::Failure(format!("anchor {name}: {source}")))?,
        ),
        None => None,
    };
    let line = command_line(&payload);
    let program = payload.first().cloned().unwrap_or_default();
    eprintln!(
        "bt-jobwrap: anchor={} ceiling={ceiling} payload={program}",
        anchor.as_deref().unwrap_or("none")
    );
    let started = std::time::Instant::now();
    let outcome = execute(&line, ceiling);
    drop(guard);
    match outcome {
        Ok(code) => {
            eprintln!(
                "bt-jobwrap: payload {program} exited {code} after {:.2}s",
                started.elapsed().as_secs_f64()
            );
            Ok(code)
        }
        Err(message) => {
            eprintln!("bt-jobwrap: {message}");
            Err(WrapError::Failure(message))
        }
    }
}

fn execute(line: &str, ceiling: u64) -> Result<u32, String> {
    let job = ffi::create_ceiling_job(ceiling as usize)
        .map_err(|code| format!("job create failed: win32 error {code}"))?;
    let spawned =
        ffi::spawn_suspended(line).map_err(|code| format!("spawn failed: win32 error {code}"))?;
    let pid = spawned.process_id;
    if let Err(code) = ffi::assign_and_resume(job, &spawned) {
        ffi::close(spawned.process);
        ffi::close(spawned.thread);
        ffi::close(job);
        return Err(format!(
            "assign or resume failed for pid {pid}: win32 error {code}"
        ));
    }
    ffi::close(spawned.thread);
    eprintln!("bt-jobwrap: spawned pid {pid} resumed under the job ceiling");
    let waited = ffi::wait_exit_code(spawned.process)
        .map_err(|code| format!("wait failed for pid {pid}: win32 error {code}"));
    ffi::close(spawned.process);
    ffi::close(job);
    waited
}

struct PidFileGuard {
    path: PathBuf,
}

impl Drop for PidFileGuard {
    fn drop(&mut self) {
        let _ = std::fs::remove_file(&self.path);
    }
}

fn write_pid_file(config: &serde_json::Value, anchor: &str) -> Result<PidFileGuard, String> {
    let dir = pid_dir_from(config)?;
    std::fs::create_dir_all(&dir)
        .map_err(|source| format!("create {}: {source}", dir.display()))?;
    let ticks = ffi::dotnet_start_ticks()
        .map_err(|code| format!("read own start time: win32 error {code}"))?;
    let pid = std::process::id();
    let path = dir.join(format!("{anchor}-{pid}.pid"));
    std::fs::write(&path, format!("{pid} {ticks}"))
        .map_err(|source| format!("write {}: {source}", path.display()))?;
    eprintln!(
        "bt-jobwrap: anchor pidfile {} holds pid {pid}",
        path.display()
    );
    Ok(PidFileGuard { path })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn arg(raw: &str) -> String {
        raw.to_string()
    }

    fn args(raw: &[&str]) -> Vec<String> {
        raw.iter().map(|value| arg(value)).collect()
    }

    #[test]
    fn parses_anchor_and_payload() {
        assert_eq!(
            parse_invocation(&args(&["--anchor", "mutation", "--", "cargo", "test"])),
            Ok(Invocation::Wrap {
                anchor: Some("mutation".to_string()),
                payload: vec!["cargo".to_string(), "test".to_string()]
            })
        );
        assert_eq!(
            parse_invocation(&args(&["--", "make", "chain"])),
            Ok(Invocation::Wrap {
                anchor: None,
                payload: vec!["make".to_string(), "chain".to_string()]
            })
        );
    }

    #[test]
    fn rejects_missing_payload_markers() {
        assert!(parse_invocation(&args(&[])).is_err());
        assert!(parse_invocation(&args(&["--"])).is_err());
        assert!(parse_invocation(&args(&["cargo", "test"])).is_err());
        assert!(parse_invocation(&args(&["--anchor", "x"])).is_err());
        assert!(parse_invocation(&args(&["--anchor", "x", "--"])).is_err());
    }

    #[test]
    fn rejects_anchor_names_outside_the_memguard_alphabet() {
        assert!(is_anchor_name("mutation-pkg-bt-core"));
        assert!(is_anchor_name("a.b_c"));
        assert!(!is_anchor_name(""));
        assert!(!is_anchor_name("bad name"));
        assert!(!is_anchor_name("a/b"));
        assert!(parse_invocation(&args(&["--anchor", "bad name", "--", "cargo"])).is_err());
    }

    #[test]
    fn parses_selftest_modes() {
        assert_eq!(
            parse_invocation(&args(&["--selftest-alloc-mb", "640"])),
            Ok(Invocation::AllocMb(640))
        );
        assert_eq!(
            parse_invocation(&args(&["--selftest-hold-secs", "0"])),
            Ok(Invocation::HoldSecs(0))
        );
        assert!(parse_invocation(&args(&["--selftest-alloc-mb", "0"])).is_err());
        assert!(parse_invocation(&args(&["--selftest-alloc-mb", "abc"])).is_err());
        assert!(parse_invocation(&args(&["--selftest-alloc-mb"])).is_err());
        assert!(parse_invocation(&args(&["--selftest-hold-secs", "3", "4"])).is_err());
    }

    #[test]
    fn quotes_only_arguments_with_whitespace() {
        assert_eq!(quote_argument("cargo"), "cargo");
        assert_eq!(quote_argument("a b"), "\"a b\"");
        assert_eq!(quote_argument(""), "\"\"");
        assert_eq!(command_line(&args(&["a b", "c"])), "\"a b\" c".to_string());
    }

    #[test]
    fn dotnet_ticks_anchor_at_known_epochs() {
        assert_eq!(dotnet_ticks(0), 504911232000000000);
        assert_eq!(dotnet_ticks(116444736000000000), 621355968000000000);
    }

    #[test]
    fn per_process_bytes_validates_the_hub_section() {
        assert_eq!(
            per_process_bytes(&serde_json::json!({"job": {"per_process_bytes": 4294967296u64}})),
            Ok(4294967296)
        );
        assert_eq!(
            per_process_bytes(&serde_json::json!({})).unwrap_err(),
            "config section missing: job.per_process_bytes"
        );
        assert_eq!(
            per_process_bytes(&serde_json::json!({"job": {"per_process_bytes": "big"}}))
                .unwrap_err(),
            "config value malformed: job.per_process_bytes"
        );
        assert_eq!(
            per_process_bytes(&serde_json::json!({"job": {"per_process_bytes": 0}})).unwrap_err(),
            "config value invalid: job.per_process_bytes must be > 0"
        );
    }

    #[test]
    fn pid_dir_resolves_absolute_and_repo_relative() {
        let absolute = std::env::temp_dir().join("pids");
        let text = absolute.to_string_lossy().replace('\\', "/");
        assert_eq!(
            pid_dir_from(&serde_json::json!({"memguard": {"pid_dir": text}})).unwrap(),
            absolute
        );
        assert_eq!(
            pid_dir_from(&serde_json::json!({"memguard": {"pid_dir": "artifacts/progress/pids"}}))
                .unwrap(),
            workspace_root().join("artifacts/progress/pids")
        );
        assert!(pid_dir_from(&serde_json::json!({})).is_err());
        assert!(pid_dir_from(&serde_json::json!({"memguard": {"pid_dir": "  "}})).is_err());
    }

    #[test]
    fn config_path_honors_env_and_default() {
        assert_eq!(
            config_path_from(None),
            workspace_root().join("config").join("config.json")
        );
        assert_eq!(
            config_path_from(Some("other/conf.json")),
            workspace_root().join("other").join("conf.json")
        );
        let absolute = std::env::temp_dir().join("x.json");
        assert_eq!(config_path_from(Some(absolute.to_str().unwrap())), absolute);
    }
}
