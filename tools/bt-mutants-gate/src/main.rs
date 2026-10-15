use bt_mutants_gate::{GateError, gate};
use std::process::ExitCode;

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        eprintln!("usage: bt-mutants-gate <outcomes.json> <allowlist.tsv>");
        return ExitCode::from(2);
    }
    let outcomes = match read_arg(&args[1]) {
        Ok(text) => text,
        Err((path, e)) => {
            eprintln!("cannot read {path}: {e}");
            return ExitCode::from(2);
        }
    };
    let allowlist = match read_arg(&args[2]) {
        Ok(text) => text,
        Err((path, e)) => {
            eprintln!("cannot read {path}: {e}");
            return ExitCode::from(2);
        }
    };
    match gate(&outcomes, &allowlist) {
        Ok(summary) => {
            println!("{summary}");
            ExitCode::SUCCESS
        }
        Err(err) => {
            eprintln!("mutants gate: {err}");
            let code = if matches!(err, GateError::Usage(_)) {
                2
            } else {
                1
            };
            ExitCode::from(code)
        }
    }
}

fn read_arg(path: &str) -> Result<String, (String, std::io::Error)> {
    std::fs::read_to_string(path).map_err(|e| (path.to_owned(), e))
}
