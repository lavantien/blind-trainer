use std::collections::BTreeMap;
use std::io::Read;
use std::path::PathBuf;
use std::process::{Child, Command, Stdio};
use std::thread;
use std::time::{Duration, Instant};

pub type EnvMap = BTreeMap<String, String>;

const POLL_MS: u64 = 50;

pub fn scrub_env<I>(vars: I) -> EnvMap
where
    I: IntoIterator<Item = (String, String)>,
{
    vars.into_iter()
        .filter(|(name, _)| !is_bt_prefixed(name))
        .collect()
}

fn is_bt_prefixed(name: &str) -> bool {
    name.get(..3)
        .is_some_and(|prefix| prefix.eq_ignore_ascii_case("BT_"))
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ChildSpec {
    pub program: PathBuf,
    pub args: Vec<String>,
    pub cwd: PathBuf,
    pub env: EnvMap,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Outcome {
    Exit(i32),
    Timeout,
    Spawn(String),
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct RunResult {
    pub outcome: Outcome,
    pub stdout: String,
    pub stderr: String,
    pub elapsed: Duration,
    pub pid: u32,
}

pub fn run_child(spec: &ChildSpec, deadline: Duration) -> RunResult {
    let start = Instant::now();
    let mut command = Command::new(&spec.program);
    command
        .args(&spec.args)
        .current_dir(&spec.cwd)
        .env_clear()
        .envs(&spec.env)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    let (pid, outcome, stdout, stderr) = match command.spawn() {
        Ok(mut child) => {
            let pid = child.id();
            let stdout_reader = child.stdout.take().map(spawn_pipe_reader);
            let stderr_reader = child.stderr.take().map(spawn_pipe_reader);
            let outcome = poll_or_kill(&mut child, deadline, &start);
            let stdout = stdout_reader
                .and_then(|handle| handle.join().ok())
                .unwrap_or_default();
            let stderr = stderr_reader
                .and_then(|handle| handle.join().ok())
                .unwrap_or_default();
            (Some(pid), outcome, stdout, stderr)
        }
        Err(err) => (
            None,
            Outcome::Spawn(format!("cannot spawn {}: {err}", spec.program.display())),
            String::new(),
            String::new(),
        ),
    };
    RunResult {
        outcome,
        stdout,
        stderr,
        elapsed: start.elapsed(),
        pid: pid.unwrap_or(0),
    }
}

fn spawn_pipe_reader(mut pipe: impl Read + Send + 'static) -> thread::JoinHandle<String> {
    thread::spawn(move || {
        let mut bytes = Vec::new();
        pipe.read_to_end(&mut bytes).ok();
        String::from_utf8_lossy(&bytes).into_owned()
    })
}

fn poll_or_kill(child: &mut Child, deadline: Duration, start: &Instant) -> Outcome {
    loop {
        match child.try_wait() {
            Ok(Some(status)) => {
                return match status.code() {
                    Some(code) => Outcome::Exit(code),
                    None => Outcome::Spawn("process ended without an exit code".to_string()),
                };
            }
            Ok(None) => {
                if start.elapsed() >= deadline {
                    kill_tree(child);
                    let reaped = child.wait();
                    return match reaped {
                        Ok(_) => Outcome::Timeout,
                        Err(err) => Outcome::Spawn(format!("kill did not reap the child: {err}")),
                    };
                }
                thread::sleep(Duration::from_millis(POLL_MS));
            }
            Err(err) => return Outcome::Spawn(format!("cannot wait for the child: {err}")),
        }
    }
}

#[cfg(windows)]
fn kill_tree(child: &mut Child) {
    let pid = child.id().to_string();
    let killed = Command::new("taskkill")
        .args(["/PID", &pid, "/T", "/F"])
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status()
        .map(|status| status.success())
        .unwrap_or(false);
    if !killed {
        let _ = child.kill();
    }
}

#[cfg(not(windows))]
fn kill_tree(child: &mut Child) {
    let _ = child.kill();
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::Path;

    fn spec(program: &str, args: &[&str], cwd: &Path, env: &[(&str, &str)]) -> ChildSpec {
        ChildSpec {
            program: PathBuf::from(program),
            args: args.iter().map(|arg| arg.to_string()).collect(),
            cwd: cwd.to_path_buf(),
            env: env
                .iter()
                .map(|(k, v)| (k.to_string(), v.to_string()))
                .collect(),
        }
    }

    #[test]
    fn scrub_env_removes_only_bt_prefixed_vars() {
        let scrubbed = scrub_env([
            ("BT_CONFIG".to_string(), "x".to_string()),
            ("BT_ARTIFACTS".to_string(), "y".to_string()),
            ("bt_probe".to_string(), "z".to_string()),
            ("PATH".to_string(), "p".to_string()),
            ("APPDATA".to_string(), "a".to_string()),
            ("BAT_CONFIG".to_string(), "b".to_string()),
            ("BT".to_string(), "c".to_string()),
        ]);
        let names: Vec<&str> = scrubbed.keys().map(String::as_str).collect();
        assert_eq!(names, vec!["APPDATA", "BAT_CONFIG", "BT", "PATH"]);
    }

    #[cfg(windows)]
    fn pwsh_dir(tag: &str) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("bt-release-verify-exec-{tag}"));
        std::fs::create_dir_all(&dir).expect("cwd dir");
        dir
    }

    #[cfg(windows)]
    #[test]
    fn run_child_reports_exit_code_and_captured_output() {
        let cwd = pwsh_dir("exit");
        let result = run_child(
            &spec(
                "pwsh",
                &["-NoProfile", "-Command", "Write-Output hi; exit 7"],
                &cwd,
                &[],
            ),
            Duration::from_secs(30),
        );
        assert_eq!(result.outcome, Outcome::Exit(7));
        assert_eq!(result.stdout.trim(), "hi");
    }

    #[cfg(windows)]
    #[test]
    fn run_child_honors_cwd_and_env_map() {
        let cwd = pwsh_dir("env");
        let sandbox = pwsh_dir("appdata");
        let env = scrub_env([
            ("BT_PROBE".to_string(), "leak".to_string()),
            (
                "PATH".to_string(),
                std::env::var("PATH").unwrap_or_default(),
            ),
        ])
        .into_iter()
        .chain([(
            "APPDATA".to_string(),
            sandbox.to_string_lossy().into_owned(),
        )])
        .collect::<EnvMap>();
        let child = ChildSpec {
            program: PathBuf::from("pwsh"),
            args: vec![
                "-NoProfile".to_string(),
                "-Command".to_string(),
                "(Get-Location).Path; $env:APPDATA; if ($env:BT_PROBE) { 'leaked' } else { 'clean' }"
                    .to_string(),
            ],
            cwd: cwd.clone(),
            env,
        };
        let result = run_child(&child, Duration::from_secs(30));
        assert_eq!(result.outcome, Outcome::Exit(0));
        let lines: Vec<&str> = result.stdout.lines().collect();
        assert_eq!(lines.len(), 3, "stdout: {}", result.stdout);
        let printed_cwd = std::fs::canonicalize(lines[0]).expect("canonical cwd");
        let wanted_cwd = std::fs::canonicalize(&cwd).expect("canonical wanted cwd");
        assert_eq!(printed_cwd, wanted_cwd);
        let printed_appdata = std::fs::canonicalize(lines[1]).expect("canonical appdata");
        let wanted_appdata = std::fs::canonicalize(&sandbox).expect("canonical wanted appdata");
        assert_eq!(printed_appdata, wanted_appdata);
        assert_eq!(lines[2], "clean");
    }

    #[test]
    fn run_child_reports_spawn_failure() {
        let cwd = std::env::temp_dir();
        let result = run_child(
            &spec("definitely-not-a-program-bt-rv", &[], &cwd, &[]),
            Duration::from_secs(30),
        );
        assert!(matches!(result.outcome, Outcome::Spawn(_)));
    }

    #[cfg(windows)]
    #[test]
    fn run_child_kills_the_tree_on_timeout() {
        let cwd = pwsh_dir("timeout");
        let start = std::time::Instant::now();
        let result = run_child(
            &spec(
                "pwsh",
                &["-NoProfile", "-Command", "Start-Sleep -Seconds 30"],
                &cwd,
                &[],
            ),
            Duration::from_secs(3),
        );
        assert_eq!(result.outcome, Outcome::Timeout);
        assert!(
            result.elapsed.as_secs() < 25,
            "elapsed {:?}",
            result.elapsed
        );
        assert!(start.elapsed().as_secs() < 25);
        let listing = std::process::Command::new("tasklist")
            .args(["/FI", &format!("PID eq {}", result.pid)])
            .output()
            .expect("tasklist");
        let text = String::from_utf8_lossy(&listing.stdout).into_owned();
        assert!(
            !text.contains(&result.pid.to_string()),
            "pid {} still listed: {text}",
            result.pid
        );
    }
}
