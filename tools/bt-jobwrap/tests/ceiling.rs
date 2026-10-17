use std::path::{Path, PathBuf};
use std::process::{Child, Command, ExitStatus};
use std::sync::atomic::{AtomicU32, Ordering};
use std::thread::sleep;
use std::time::{Duration, Instant};

const HUB_CEILING: u64 = 4294967296;

static COUNTER: AtomicU32 = AtomicU32::new(0);

fn exe() -> &'static str {
    env!("CARGO_BIN_EXE_bt-jobwrap")
}

struct Fixture {
    dir: PathBuf,
    pids: PathBuf,
    config: PathBuf,
}

fn new_dirs() -> (PathBuf, PathBuf, PathBuf) {
    let dir = std::env::temp_dir().join(format!(
        "bt-jobwrap-{}-{}",
        std::process::id(),
        COUNTER.fetch_add(1, Ordering::Relaxed)
    ));
    let pids = dir.join("pids");
    std::fs::create_dir_all(&pids).expect("pid dir");
    let config = dir.join("config.json");
    (dir, pids, config)
}

fn raw_fixture(body: &str) -> Fixture {
    let (dir, pids, config) = new_dirs();
    std::fs::write(&config, body).expect("config write");
    Fixture { dir, pids, config }
}

fn fixture(per_process_bytes: u64) -> Fixture {
    let (dir, pids, config) = new_dirs();
    let pid_text = pids.to_string_lossy().replace('\\', "/");
    std::fs::write(
        &config,
        format!("{{\"job\":{{\"per_process_bytes\":{per_process_bytes}}},\"memguard\":{{\"pid_dir\":\"{pid_text}\"}}}}"),
    )
    .expect("config write");
    Fixture { dir, pids, config }
}

fn wrapped(fixture: &Fixture, args: &[&str]) -> Command {
    let mut command = Command::new(exe());
    command.env("BT_CONFIG", &fixture.config);
    command.args(args);
    command
}

fn pwsh_text(script: &str) -> String {
    let out = Command::new("pwsh")
        .args(["-NoProfile", "-Command", script])
        .output()
        .expect("pwsh runs");
    String::from_utf8_lossy(&out.stdout).trim().to_string()
}

fn pid_alive(pid: u32) -> bool {
    pwsh_text(&format!(
        "[bool](Get-Process -Id {pid} -ErrorAction SilentlyContinue)"
    )) == "True"
}

fn wait_for_file(path: &Path) -> bool {
    let deadline = Instant::now() + Duration::from_secs(10);
    while Instant::now() < deadline {
        if path.is_file() {
            return true;
        }
        sleep(Duration::from_millis(100));
    }
    path.is_file()
}

fn wait_exit(child: &mut Child) -> Option<ExitStatus> {
    let deadline = Instant::now() + Duration::from_secs(60);
    loop {
        if let Some(status) = child.try_wait().expect("try wait") {
            return Some(status);
        }
        if Instant::now() > deadline {
            return None;
        }
        sleep(Duration::from_millis(150));
    }
}

fn taskkill(pid: u32, tree: bool) {
    let mut command = Command::new("taskkill");
    command.args(["/PID", &pid.to_string(), "/F"]);
    if tree {
        command.arg("/T");
    }
    let _ = command.output();
}

#[test]
fn forwards_payload_exit_codes() {
    let fx = fixture(268435456);
    let out = wrapped(&fx, &["--", "pwsh", "-NoProfile", "-Command", "exit 0"])
        .output()
        .expect("wrap runs");
    assert_eq!(out.status.code(), Some(0));
    let out = wrapped(&fx, &["--", "pwsh", "-NoProfile", "-Command", "exit 7"])
        .output()
        .expect("wrap runs");
    assert_eq!(out.status.code(), Some(7));
}

#[test]
fn payload_under_ceiling_exits_zero() {
    let fx = fixture(536870912);
    let started = Instant::now();
    let out = wrapped(&fx, &["--", exe(), "--selftest-alloc-mb", "64"])
        .output()
        .expect("wrap runs");
    let elapsed = started.elapsed();
    assert_eq!(out.status.code(), Some(0));
    eprintln!("bt-jobwrap test: 64 MiB under 512 MiB ceiling exited 0 in {elapsed:?}");
}

#[test]
fn payload_past_ceiling_dies_fast() {
    let fx = fixture(100663296);
    let started = Instant::now();
    let out = wrapped(&fx, &["--", exe(), "--selftest-alloc-mb", "640"])
        .output()
        .expect("wrap runs");
    let elapsed = started.elapsed();
    let code = out.status.code();
    assert_ne!(code, Some(0));
    assert_ne!(code, Some(259));
    assert!(
        elapsed < Duration::from_secs(30),
        "runaway payload took {elapsed:?} to die"
    );
    eprintln!(
        "bt-jobwrap test: 640 MiB ask under 96 MiB ceiling died in {elapsed:?} with code {code:?}"
    );
}

#[test]
fn ceiling_comes_from_the_config_at_runtime() {
    let small = fixture(67108864);
    let big = fixture(HUB_CEILING);
    let denied = wrapped(&small, &["--", exe(), "--selftest-alloc-mb", "96"])
        .output()
        .expect("wrap runs");
    assert_ne!(denied.status.code(), Some(0));
    let allowed = wrapped(&big, &["--", exe(), "--selftest-alloc-mb", "96"])
        .output()
        .expect("wrap runs");
    assert_eq!(allowed.status.code(), Some(0));
}

#[test]
fn usage_and_config_failures_exit_2_and_3() {
    let fx = fixture(268435456);
    let cases: Vec<Vec<&str>> = vec![
        vec![],
        vec!["--"],
        vec!["--anchor"],
        vec!["--anchor", "bad name", "--", "pwsh"],
    ];
    for case in cases {
        let out = wrapped(&fx, &case).output().expect("wrap runs");
        assert_eq!(out.status.code(), Some(2), "args {case:?}");
    }
    let mut missing = wrapped(&fx, &["--", "pwsh", "-NoProfile", "-Command", "exit 0"]);
    missing.env("BT_CONFIG", fx.dir.join("absent.json"));
    let out = missing.output().expect("wrap runs");
    assert_eq!(out.status.code(), Some(3));
    let nojob = raw_fixture("{\"memguard\":{\"pid_dir\":\"unused\"}}");
    let out = wrapped(&nojob, &["--", "pwsh", "-NoProfile", "-Command", "exit 0"])
        .output()
        .expect("wrap runs");
    assert_eq!(out.status.code(), Some(3));
}

#[test]
fn anchor_pidfile_carries_dotnet_start_ticks_and_clears_on_exit() {
    let fx = fixture(268435456);
    let mut child = wrapped(
        &fx,
        &[
            "--anchor",
            "gw-stamp",
            "--",
            exe(),
            "--selftest-hold-secs",
            "12",
        ],
    )
    .spawn()
    .expect("wrap spawns");
    let pidfile = fx.pids.join(format!("gw-stamp-{}.pid", child.id()));
    assert!(
        wait_for_file(&pidfile),
        "pidfile never appeared: {}",
        pidfile.display()
    );
    let raw = std::fs::read_to_string(&pidfile).expect("pidfile read");
    let parts: Vec<&str> = raw.split_whitespace().collect();
    assert_eq!(parts.len(), 2, "pidfile must hold pid then ticks: {raw}");
    assert_eq!(parts[0].parse::<u32>().expect("pid number"), child.id());
    let live = pwsh_text(&format!(
        "(Get-Process -Id {}).StartTime.ToUniversalTime().Ticks",
        child.id()
    ));
    assert_eq!(
        parts[1], live,
        "stamp must equal the dotnet utc ticks of the wrapper start"
    );
    let status = wait_exit(&mut child).expect("wrapper exits");
    assert_eq!(status.code(), Some(0));
    assert!(!pidfile.exists(), "pidfile must be removed after exit");
}

#[test]
fn killing_the_wrapper_takes_the_grandchild_with_it() {
    let fx = fixture(HUB_CEILING);
    let mut outer = wrapped(
        &fx,
        &[
            "--anchor",
            "gw-outer",
            "--",
            exe(),
            "--anchor",
            "gw-mid",
            "--",
            exe(),
            "--selftest-hold-secs",
            "90",
        ],
    )
    .spawn()
    .expect("outer spawns");
    let mut mid = 0;
    let mut deadline = Instant::now() + Duration::from_secs(15);
    while Instant::now() < deadline {
        let hits: Vec<u32> = std::fs::read_dir(&fx.pids)
            .expect("pid dir readable")
            .flatten()
            .filter_map(|entry| {
                let name = entry.file_name().to_string_lossy().to_string();
                name.strip_prefix("gw-mid-")?
                    .strip_suffix(".pid")?
                    .parse::<u32>()
                    .ok()
            })
            .collect();
        if let Some(found) = hits.first() {
            mid = *found;
            break;
        }
        sleep(Duration::from_millis(150));
    }
    assert_ne!(mid, 0, "mid wrapper pidfile never appeared");
    let mut leaf = 0;
    deadline = Instant::now() + Duration::from_secs(15);
    while Instant::now() < deadline {
        let text = pwsh_text(&format!(
            "(Get-CimInstance Win32_Process -Filter \"ParentProcessId = {mid}\").ProcessId"
        ));
        if let Ok(found) = text.parse::<u32>() {
            leaf = found;
            break;
        }
        sleep(Duration::from_millis(200));
    }
    assert_ne!(leaf, 0, "leaf hold process never appeared under mid {mid}");
    assert!(pid_alive(mid));
    assert!(pid_alive(leaf));
    taskkill(outer.id(), false);
    let mut both_gone = false;
    deadline = Instant::now() + Duration::from_secs(20);
    while Instant::now() < deadline {
        if !pid_alive(mid) && !pid_alive(leaf) {
            both_gone = true;
            break;
        }
        sleep(Duration::from_millis(250));
    }
    if !both_gone {
        taskkill(outer.id(), true);
        taskkill(mid, true);
    }
    assert!(
        both_gone,
        "mid {mid} or leaf {leaf} survived the wrapper death"
    );
    let _ = wait_exit(&mut outer);
}
