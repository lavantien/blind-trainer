#![no_main]

use libfuzzer_sys::fuzz_target;

fuzz_target!(|data: &[u8]| {
    if !bt_fuzz::within_bounds() {
        std::process::exit(0);
    }
    bt_fuzz::db_once(data);
});
