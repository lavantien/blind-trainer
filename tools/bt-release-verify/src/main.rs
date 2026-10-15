use std::process::ExitCode;

fn main() -> ExitCode {
    match bt_release_verify::run(&bt_release_verify::Env::from_process()) {
        Ok(code) => ExitCode::from(code),
        Err(error) => {
            eprintln!("bt-release-verify: {error}");
            ExitCode::from(bt_release_verify::EXIT_USAGE)
        }
    }
}
