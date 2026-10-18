use std::process::ExitCode;
fn main() -> ExitCode {
    let code: u32 = 7;
    ExitCode::from_u32(code)
}

