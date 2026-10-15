use bt_data::corpus::Corpus;
use bt_data::paths::{load_config, vendor_root, workspace_root};
use bt_gendata::audit::audit;
use bt_gendata::build::{build, manifest, regen_needed};
use bt_gendata::hash::{digest_corpus, sha256_hex};
use bt_gendata::model::Manifest;
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
    let digest = digest_corpus(&root).map_err(|e| vec![e])?;
    let config = load_config().map_err(|e| vec![e.to_string()])?;
    let limits = synth_limits(&config).map_err(|e| vec![e])?;
    let scoring = scoring_from_config(&config).map_err(|e| vec![e])?;
    let params_sha256 = synth_params_sha256(&config).map_err(|e| vec![e])?;
    let out = data_vendor_path(&config).map_err(|e| vec![e])?;
    let existing: Option<Manifest> = std::fs::read_to_string(out.join("manifest.json"))
        .ok()
        .and_then(|text| serde_json::from_str(&text).ok());
    let outputs_present = out.join("cases.json").is_file() && out.join("specials.json").is_file();
    if outputs_present && !regen_needed(existing.as_ref(), &params_sha256, &digest.sha256) {
        println!("gendata: skip regeneration, synth params and corpus digest unchanged");
        return Ok(());
    }
    let corpus = Corpus::load(&root).map_err(|e| vec![e.to_string()])?;
    let mut built = build(&corpus);
    let offenders = audit(&built, &corpus, digest.rows);
    if !offenders.is_empty() {
        return Err(offenders);
    }
    let (edge_added, corner_added) = synthesize(&mut built.cases, &limits, &scoring);
    let cases_bytes = serde_json::to_vec_pretty(&built.cases).map_err(|e| vec![e.to_string()])?;
    let specials_bytes =
        serde_json::to_vec_pretty(&built.specials).map_err(|e| vec![e.to_string()])?;
    let record = manifest(
        &built,
        &digest,
        &sha256_hex(&cases_bytes),
        &sha256_hex(&specials_bytes),
        &params_sha256,
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

fn synth_params_sha256(config: &serde_json::Value) -> Result<String, String> {
    let section = config
        .pointer("/synth")
        .ok_or_else(|| "config section missing: synth".to_string())?;
    let canonical = serde_json::to_string(section).map_err(|e| e.to_string())?;
    Ok(sha256_hex(canonical.as_bytes()))
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

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn synth_params_hash_ignores_key_order_and_tracks_values() {
        let config = json!({"synth": {"max_setup": 3, "scoring": {"penalty_other": 2.0}}});
        let reordered = json!({"synth": {"scoring": {"penalty_other": 2.0}, "max_setup": 3}});
        assert_eq!(
            synth_params_sha256(&config).unwrap(),
            synth_params_sha256(&reordered).unwrap()
        );
        let changed = json!({"synth": {"max_setup": 4, "scoring": {"penalty_other": 2.0}}});
        assert_ne!(
            synth_params_sha256(&config).unwrap(),
            synth_params_sha256(&changed).unwrap()
        );
        assert_eq!(
            synth_params_sha256(&json!({})).unwrap_err(),
            "config section missing: synth"
        );
    }
}
