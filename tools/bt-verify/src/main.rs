use bt_data::corpus::Corpus;
use bt_data::paths::{load_config, packs_root, vendor_root, workspace_root};
use bt_gendata::hash::digest_corpus;
use bt_gendata::out::write_atomic;
use bt_gendata::when::now_iso8601;
use bt_verify::load_simcheck;
use bt_verify::packs_report::{audit_packs, render_packs_markdown};
use bt_verify::report::{audit, build_report, render_markdown};
use std::path::PathBuf;

fn main() {
    if let Err(offenders) = run() {
        for offender in offenders {
            eprintln!("verify: {offender}");
        }
        std::process::exit(1);
    }
}

fn run() -> Result<(), Vec<String>> {
    let root = vendor_root().map_err(|e| vec![e.to_string()])?;
    let corpus = Corpus::load(&root).map_err(|e| vec![e.to_string()])?;
    let sim = load_simcheck(&root).map_err(|e| vec![e])?;
    let digest = digest_corpus(&root).map_err(|e| vec![e])?;
    let report = build_report(&corpus, &sim, now_iso8601(), digest.sha256.clone());
    let mut offenders = audit(&report, sim.len());
    let dir = artifacts_verify_dir().map_err(|e| vec![e])?;
    let packs = packs_root().map_err(|e| vec![e.to_string()])?;
    let pack_audit = audit_packs(&packs);
    write_atomic(
        &dir.join("packs.md"),
        render_packs_markdown(&pack_audit).as_bytes(),
    )
    .map_err(|e| vec![e])?;
    let pack_rows: usize = pack_audit.sections.iter().map(|s| s.rows).sum();
    let pack_clean: usize = pack_audit.sections.iter().map(|s| s.passed + s.ruled).sum();
    offenders.extend(pack_audit.offenders);
    if !offenders.is_empty() {
        return Err(offenders);
    }
    let json = serde_json::to_vec_pretty(&report).map_err(|e| vec![e.to_string()])?;
    let markdown = render_markdown(&report);
    write_atomic(&dir.join("report.json"), &json).map_err(|e| vec![e])?;
    write_atomic(&dir.join("report.md"), markdown.as_bytes()).map_err(|e| vec![e])?;
    println!(
        "verify: {}/{} rows pass, report at {}",
        report.rows.len() - report.failures.len(),
        report.rows.len(),
        dir.join("report.md").display()
    );
    println!(
        "packs: {pack_clean}/{pack_rows} rows pass or ruled, report at {}",
        dir.join("packs.md").display()
    );
    Ok(())
}

fn artifacts_verify_dir() -> Result<PathBuf, String> {
    let root = match std::env::var("BT_ARTIFACTS") {
        Ok(raw) if !raw.is_empty() => {
            let path = PathBuf::from(raw);
            if path.is_absolute() {
                path
            } else {
                workspace_root().join(path)
            }
        }
        _ => {
            let config = load_config().map_err(|e| e.to_string())?;
            let relative = config
                .pointer("/paths/artifacts")
                .and_then(|value| value.as_str())
                .ok_or_else(|| "config section missing: paths.artifacts".to_string())?;
            let path = PathBuf::from(relative);
            if path.is_absolute() {
                path
            } else {
                workspace_root().join(path)
            }
        }
    };
    Ok(root.join("verify"))
}
