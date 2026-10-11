fn main() {
    if std::env::var_os("RAYON_NUM_THREADS").is_none() {
        unsafe { std::env::set_var("RAYON_NUM_THREADS", "8") };
    }
    if let Err(offenders) = bt_ga_run::run_all() {
        for offender in offenders {
            eprintln!("ga: {offender}");
        }
        std::process::exit(1);
    }
}
