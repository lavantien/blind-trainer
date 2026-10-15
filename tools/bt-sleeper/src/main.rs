use std::process::ExitCode;
use std::thread::sleep;
use std::time::Duration;

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    match bt_sleeper::parse_args(&args) {
        Ok(secs) => {
            sleep(Duration::from_secs_f64(secs));
            ExitCode::SUCCESS
        }
        Err(message) => {
            eprintln!("usage: bt-sleeper <secs>");
            eprintln!("bt-sleeper: {message}");
            ExitCode::from(2)
        }
    }
}
