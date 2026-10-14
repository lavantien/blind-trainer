use std::process::ExitCode;

fn main() -> ExitCode {
    match bt_visual_diff::run(&bt_visual_diff::Env::from_process()) {
        Ok(code) => ExitCode::from(code),
        Err(error) => {
            eprintln!("bt-visual-diff: {error}");
            ExitCode::from(bt_visual_diff::EXIT_USAGE)
        }
    }
}
