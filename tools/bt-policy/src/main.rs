use bt_policy::{CheckMode, load_config, run_checks, selftest, workspace_root};
use std::path::Path;

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    std::process::exit(match args.as_slice() {
        [] => real(CheckMode::Both),
        [mode] if mode == "comments" => real(CheckMode::Comments),
        [mode] if mode == "constants" => real(CheckMode::Constants),
        [mode, workdir] if mode == "selftest" => match selftest(Path::new(workdir)) {
            Ok(true) => 0,
            Ok(false) => 1,
            Err(message) => {
                eprintln!("bt-policy: {message}");
                2
            }
        },
        _ => {
            eprintln!("usage: bt-policy [comments|constants|selftest <workdir>]");
            2
        }
    });
}

fn real(mode: CheckMode) -> i32 {
    let root = workspace_root();
    let config = match load_config() {
        Ok(value) => value,
        Err(message) => {
            eprintln!("bt-policy: {message}");
            return 2;
        }
    };
    let allowlist = root
        .join("tools")
        .join("bt-policy")
        .join("constants_allow.txt");
    match run_checks(&root, mode, &config, &allowlist) {
        Ok(outcome) => {
            for finding in &outcome.findings {
                println!(
                    "bt-policy: {}:{}: {}",
                    finding.path, finding.line, finding.detail
                );
            }
            for stale in &outcome.stale_allowlist {
                eprintln!("bt-policy: stale allowlist entry: {stale}");
            }
            i32::from(!outcome.findings.is_empty())
        }
        Err(message) => {
            eprintln!("bt-policy: {message}");
            2
        }
    }
}
