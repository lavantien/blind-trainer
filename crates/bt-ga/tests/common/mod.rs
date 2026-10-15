use std::thread;
use std::time::Duration;

pub const WATCHDOG_SECS: u64 = 120;

pub fn arm_hang_watchdog() {
    thread::spawn(|| {
        thread::sleep(Duration::from_secs(WATCHDOG_SECS));
        std::process::exit(101);
    });
}
