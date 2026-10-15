use bt_data::corpus::Corpus;
use bt_data::paths::{load_config, vendor_root, workspace_root};
use bt_gendata::audit::audit;
use bt_gendata::build::{build, manifest};
use bt_gendata::hash::{digest_corpus, sha256_hex};
use bt_gendata::out::write_atomic;
use bt_gendata::scoring::scoring_from_config;
use bt_gendata::synth::{SynthLimits, synthesize};
use bt_gendata::when::now_iso8601;
use std::path::PathBuf;

fn main() {
    if let Err(offenders) = run() {
        for offender in offenders {
            eprintln!("gendata: {offender}");
        }
        std::process::exit(1);
    }
}

fn run() -> Result<(), Vec<String>> {
    let root = vendor_root().map_err(|e| vec![e.to_string()])?;
    let corpus = Corpus::load(&root).map_err(|e| vec![e.to_string()])?;
    let digest = digest_corpus(&root).map_err(|e| vec![e])?;
    let mut built = build(&corpus);
    let offenders = audit(&built, &corpus, digest.rows);
    if !offenders.is_empty() {
        return Err(offenders);
    }
    let config = load_config().map_err(|e| vec![e.to_string()])?;
    let limits = synth_limits(&config).map_err(|e| vec![e])?;
    let scoring = scoring_from_config(&config).map_err(|e| vec![e])?;
    let (edge_added, corner_added) = synthesize(&mut built.cases, &limits, &scoring);
    let out = data_vendor_path(&config).map_err(|e| vec![e])?;
    let cases_bytes = serde_json::to_vec_pretty(&built.cases).map_err(|e| vec![e.to_string()])?;
    let specials_bytes =
        serde_json::to_vec_pretty(&built.specials).map_err(|e| vec![e.to_string()])?;
    let record = manifest(
        &built,
        &digest,
        &sha256_hex(&cases_bytes),
        &sha256_hex(&specials_bytes),
        now_iso8601(),
    );
    let manifest_bytes = serde_json::to_vec_pretty(&record).map_err(|e| vec![e.to_string()])?;
    write_atomic(&out.join("cases.json"), &cases_bytes).map_err(|e| vec![e])?;
    write_atomic(&out.join("specials.json"), &specials_bytes).map_err(|e| vec![e])?;
    write_atomic(&out.join("manifest.json"), &manifest_bytes).map_err(|e| vec![e])?;
    println!(
        "gendata: {} cases ({} covered, {} generated: {} edge, {} corner), {} specials, manifest at {}",
        record.cases.entries,
        record.cases.edge.covered + record.cases.corner.covered,
        edge_added + corner_added,
        edge_added,
        corner_added,
        record.specials.entries,
        out.join("manifest.json").display()
    );
    Ok(())
}

fn synth_limits(config: &serde_json::Value) -> Result<SynthLimits, String> {
    let max_setup = config
        .pointer("/synth/max_setup")
        .and_then(|value| value.as_u64())
        .ok_or_else(|| "config section missing: synth.max_setup".to_string())?
        as usize;
    let max_insert = config
        .pointer("/synth/max_insert")
        .and_then(|value| value.as_u64())
        .ok_or_else(|| "config section missing: synth.max_insert".to_string())?
        as usize;
    let node_budget = config
        .pointer("/synth/node_budget")
        .and_then(|value| value.as_u64())
        .ok_or_else(|| "config section missing: synth.node_budget".to_string())?
        as usize;
    Ok(SynthLimits {
        max_setup,
        max_insert,
        node_budget,
    })
}

fn data_vendor_path(config: &serde_json::Value) -> Result<PathBuf, String> {
    let relative = config
        .pointer("/paths/data_vendor")
        .and_then(|value| value.as_str())
        .ok_or_else(|| "config section missing: paths.data_vendor".to_string())?;
    let path = PathBuf::from(relative);
    Ok(if path.is_absolute() {
        path
    } else {
        workspace_root().join(path)
    })
}
