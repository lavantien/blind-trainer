use bt_data::corpus::Corpus;
use bt_data::paths::{load_config, vendor_root, workspace_root};
use bt_gendata::audit::audit;
use bt_gendata::build::{build, manifest};
use bt_gendata::hash::{digest_corpus, sha256_hex};
use bt_gendata::out::write_atomic;
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
    let built = build(&corpus);
    let offenders = audit(&built, &corpus, digest.rows);
    if !offenders.is_empty() {
        return Err(offenders);
    }
    let out = data_vendor_path().map_err(|e| vec![e])?;
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
        "gendata: {} cases ({} covered), {} specials, manifest at {}",
        record.cases.entries,
        record.cases.edge.covered + record.cases.corner.covered,
        record.specials.entries,
        out.join("manifest.json").display()
    );
    Ok(())
}

fn data_vendor_path() -> Result<PathBuf, String> {
    let config = load_config().map_err(|e| e.to_string())?;
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
