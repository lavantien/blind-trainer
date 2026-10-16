use bt_mutants_gate::{GateError, gate, merge_outcomes};
use std::process::ExitCode;

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().collect();
    if args.get(1).map(String::as_str) == Some("merge") {
        return run_merge(&args);
    }
    if args.len() != 3 {
        eprintln!("usage: bt-mutants-gate <outcomes.json> <allowlist.tsv>");
        eprintln!("       bt-mutants-gate merge <out.json> <in1.json> <in2.json> ...");
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

fn run_merge(args: &[String]) -> ExitCode {
    if args.len() < 4 {
        eprintln!("usage: bt-mutants-gate merge <out.json> <in1.json> <in2.json> ...");
        return ExitCode::from(2);
    }
    let mut texts = Vec::with_capacity(args.len() - 3);
    for path in &args[3..] {
        match read_arg(path) {
            Ok(text) => texts.push(text),
            Err((bad, e)) => {
                eprintln!("cannot read {bad}: {e}");
                return ExitCode::from(2);
            }
        }
    }
    let refs: Vec<&str> = texts.iter().map(String::as_str).collect();
    match merge_outcomes(&refs) {
        Ok(merged) => match std::fs::write(&args[2], &merged.json) {
            Ok(()) => {
                println!(
                    "merged {} inputs, {} mutants, wrote {}",
                    merged.inputs, merged.total_mutants, args[2]
                );
                ExitCode::SUCCESS
            }
            Err(e) => {
                eprintln!("cannot write {}: {e}", args[2]);
                ExitCode::from(2)
            }
        },
        Err(err) => {
            eprintln!("mutants merge: {err}");
            let code = if matches!(err, GateError::Usage(_)) {
                2
            } else {
                1
            };
            ExitCode::from(code)
        }
    }
}
