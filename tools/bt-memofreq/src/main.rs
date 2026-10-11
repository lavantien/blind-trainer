use bt_data::paths::{load_config, workspace_root};
use bt_memofreq::{count_frequencies, zero_hit_cases};
use std::path::PathBuf;

fn main() {
    if let Err(offenders) = run() {
        for offender in offenders {
            eprintln!("memofreq: {offender}");
        }
        std::process::exit(1);
    }
}

fn run() -> Result<(), Vec<String>> {
    let config = load_config().map_err(|e| vec![e.to_string()])?;
    let (samples, seed) = bt_ga::dist::config_memofreq(&config).map_err(|e| vec![e.to_string()])?;
    let out = data_ga_path(&config).map_err(|e| vec![e])?;
    let data = count_frequencies(samples, seed);
    let zeros = zero_hit_cases(&data);
    if !zeros.is_empty() {
        return Err([
            vec![format!(
                "{} of {} cases never appeared over {} draws",
                zeros.len(),
                bt_memofreq::TOTAL_CASES,
                samples
            )],
            zeros,
        ]
        .concat());
    }
    let bytes = serde_json::to_vec_pretty(&data).map_err(|e| vec![e.to_string()])?;
    let path = out.join("freq.json");
    bt_data::paths::write_json_atomic(&path, &data).map_err(|e| vec![e.to_string()])?;
    println!(
        "memofreq: {} draws seed {} -> {} cases all hit ({} pair events, {} adjacent events), sha256 {}, wrote {}",
        samples,
        seed,
        data.cases.len(),
        data.cooccurring.iter().map(|p| p.count).sum::<u64>(),
        data.adjacent.iter().map(|p| p.count).sum::<u64>(),
        bt_ga::hash::sha256_hex(&bytes),
        path.display()
    );
    Ok(())
}

fn data_ga_path(config: &serde_json::Value) -> Result<PathBuf, String> {
    let relative = config
        .pointer("/paths/data_ga")
        .and_then(|v| v.as_str())
        .ok_or_else(|| "config section missing: paths.data_ga".to_string())?;
    let path = PathBuf::from(relative);
    Ok(if path.is_absolute() {
        path
    } else {
        workspace_root().join(path)
    })
}
