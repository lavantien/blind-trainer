use bt_jobwrap::{Invocation, USAGE};
use std::process::ExitCode;

const ALLOC_CHUNK_BYTES: usize = 1048576;
const TOUCH_STRIDE_BYTES: usize = 4096;

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let invocation = match bt_jobwrap::parse_invocation(&args) {
        Ok(parsed) => parsed,
        Err(message) => return usage_exit(&message),
    };
    match invocation {
        Invocation::AllocMb(mb) => alloc_and_touch(mb),
        Invocation::HoldSecs(secs) => hold(secs),
        wrapped => match bt_jobwrap::run(wrapped) {
            Ok(code) => std::process::exit(code as i32),
            Err(bt_jobwrap::WrapError::Usage(message)) => usage_exit(&message),
            Err(bt_jobwrap::WrapError::Failure(_)) => ExitCode::from(3),
        },
    }
}

fn alloc_and_touch(mb: u64) -> ExitCode {
    eprintln!("bt-jobwrap: selftest alloc {mb} MiB starting");
    let mut held: Vec<Vec<u8>> = Vec::with_capacity(mb as usize);
    for _ in 0..mb {
        let mut block = vec![0u8; ALLOC_CHUNK_BYTES];
        for at in (0..ALLOC_CHUNK_BYTES).step_by(TOUCH_STRIDE_BYTES) {
            block[at] = 1;
        }
        held.push(block);
    }
    eprintln!("bt-jobwrap: selftest alloc touched {mb} MiB and holds it");
    ExitCode::SUCCESS
}

fn hold(secs: u64) -> ExitCode {
    eprintln!("bt-jobwrap: selftest hold {secs}s starting");
    std::thread::sleep(std::time::Duration::from_secs(secs));
    eprintln!("bt-jobwrap: selftest hold {secs}s done");
    ExitCode::SUCCESS
}

fn usage_exit(message: &str) -> ExitCode {
    eprintln!("{USAGE}");
    eprintln!("bt-jobwrap: {message}");
    ExitCode::from(2)
}
