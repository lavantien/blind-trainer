use bt_data::seedpack::{expected_failure, load_pack, load_registry};
use bt_verify::packs::{Tier2Outcome, verify_rows};
use std::path::Path;

fn main() {
    let packs = Path::new("../../ref_ground/packs");
    let entries = load_registry(packs).expect("registry");
    for entry in &entries {
        let rows = load_pack(packs, entry).expect("pack");
        let section = verify_rows(entry, &rows);
        for row in &section.rows {
            if expected_failure(&entry.event, &entry.method, &entry.stage, &row.case_key).is_some()
            {
                let tier1 = match &row.tier1 {
                    Ok(()) => "OK".to_string(),
                    Err(err) => format!("ERR {err}"),
                };
                let tier2 = match &row.tier2 {
                    Tier2Outcome::Pass => "PASS".to_string(),
                    Tier2Outcome::Fail(msg) => format!("FAIL {msg}"),
                    Tier2Outcome::Skipped(reason) => format!("SKIP {reason}"),
                };
                println!(
                    "{}/{}/{}/{}\ttier1={tier1}\ttier2={tier2}",
                    entry.event, entry.method, entry.stage, row.case_key
                );
            }
        }
    }
}
