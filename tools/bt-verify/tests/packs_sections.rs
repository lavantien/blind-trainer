mod common;

use bt_data::seedpack::{PackEntry, load_pack, load_registry, parse_pack};
use bt_verify::packs::{PACK_EXPECTED_ROWS, Tier2Outcome, pinned_vs_registry, verify_rows};
use bt_verify::packs_report::audit_packs;
use common::ref_ground;

const PYRA_OPEN: &[&str] = &[
    "l4e.06", "l4e.11", "l4e.21", "l4e.22", "l4e.24", "l4e.25", "l4e.28", "l4e.35",
];
const MEGA_OPEN: &[&str] = &["4lll.20", "4lll.21", "4lll.22", "4lll.24"];
const BIGCUBE_YAU_OPEN: &[&str] = &[
    "parity.oll4",
    "parity.oll4flip",
    "parity.pll4",
    "parity.pll4adj",
    "parity.wing5",
];
const BIGCUBE_REDUCTION_OPEN: &[&str] = &[
    "l2e.01", "l2e.02", "l2e.03", "l2e.04", "l2e.05", "l2e.06", "l2e.07", "l2e.08", "l2e.09",
    "l2e.10", "l2e.11", "l2e.12",
];
const M2_OPEN: &[&str] = &["m2.18", "m2.19", "m2.20", "m2.22"];

fn packs() -> std::path::PathBuf {
    ref_ground().join("packs")
}

fn entry_of(event: &str, method: &str, stage: &str) -> PackEntry {
    load_registry(&packs())
        .expect("registry loads")
        .into_iter()
        .find(|entry| entry.event == event && entry.method == method && entry.stage == stage)
        .unwrap_or_else(|| panic!("registry entry {event}/{method}/{stage} exists"))
}

#[test]
fn pinned_table_covers_the_registry_exactly() {
    assert_eq!(PACK_EXPECTED_ROWS.len(), 21);
    assert_eq!(
        PACK_EXPECTED_ROWS.iter().map(|pin| pin.rows).sum::<usize>(),
        1115
    );
    let drift = pinned_vs_registry(&packs());
    assert!(drift.is_empty(), "{}", drift.join("\n"));
}

fn probe_entry() -> PackEntry {
    PackEntry {
        file: "probe.tsv".to_string(),
        event: "2x2".to_string(),
        method: "cll".to_string(),
        stage: "cll".to_string(),
        n: bt_data::seedpack::NSpec::Unset,
        expected_rows: 1,
    }
}

fn probe_rows(alg: &str) -> Vec<bt_data::seedpack::SeedRow> {
    let text = format!(
        "case_key\talg\tfront_json\tsource_url\tsource_at\ncll.probe\t{alg}\t{{\"case\":\"P\"}}\thttps://example.test\t2026-10-10\n"
    );
    parse_pack(&probe_entry(), &text).expect("probe pack parses")
}

#[test]
fn twoby2_cll_rows_pass_tier1_and_the_bottom_layer_predicate() {
    let entry = entry_of("2x2", "cll", "cll");
    let rows = load_pack(&packs(), &entry).expect("cll pack loads");
    let result = verify_rows(&entry, &rows);
    assert_eq!(result.rows.len(), 42);
    assert!(result.honesty.is_empty(), "{}", result.honesty.join("\n"));
    for row in &result.rows {
        assert!(
            row.tier1.is_ok(),
            "{} tier 1: {:?}",
            row.case_key,
            row.tier1.as_ref().err()
        );
        assert!(
            matches!(row.tier2, Tier2Outcome::Pass),
            "{} tier 2: {:?}",
            row.case_key,
            row.tier2
        );
    }
}

#[test]
fn twoby2_tier2_rejects_a_case_with_no_solved_layer() {
    let rows = probe_rows("R U R'");
    let result = verify_rows(&probe_entry(), &rows);
    assert_eq!(result.rows.len(), 1);
    assert!(result.rows[0].tier1.is_ok());
    assert!(matches!(result.rows[0].tier2, Tier2Outcome::Fail(_)));
}

#[test]
fn sq1_cubeshape_fails_exactly_the_nine_ruled_rows() {
    let entry = entry_of("sq1", "lbl", "cubeshape");
    let rows = load_pack(&packs(), &entry).expect("cubeshape pack loads");
    let result = verify_rows(&entry, &rows);
    assert_eq!(result.rows.len(), 169);
    let flipped = [
        "cs.075", "cs.078", "cs.084", "cs.089", "cs.097", "cs.109", "cs.118", "cs.129",
    ];
    for row in &result.rows {
        if row.case_key == "cs.119" {
            assert!(row.tier1.is_err(), "cs.119 tier 1: {:?}", row.tier1);
            assert!(matches!(row.tier2, Tier2Outcome::Skipped(_)));
        } else if flipped.contains(&row.case_key.as_str()) {
            assert!(
                row.tier1.is_ok(),
                "{} tier 1: {:?}",
                row.case_key,
                row.tier1
            );
            assert!(
                matches!(row.tier2, Tier2Outcome::Fail(_)),
                "{} tier 2: {:?}",
                row.case_key,
                row.tier2
            );
        } else {
            assert!(
                row.tier1.is_ok(),
                "{} tier 1: {:?}",
                row.case_key,
                row.tier1
            );
            assert!(
                matches!(row.tier2, Tier2Outcome::Pass),
                "{} tier 2: {:?}",
                row.case_key,
                row.tier2
            );
        }
    }
}

#[test]
fn sq1_stage_rows_hold_their_stage_start_state() {
    use bt_data::seedpack::expected_failure;
    let mut failures = Vec::new();
    let mut ruled = 0;
    for (stage, count) in [("co", 7), ("eo", 7), ("cp", 16), ("ep", 99)] {
        let entry = entry_of("sq1", "lbl", stage);
        let rows = load_pack(&packs(), &entry).expect("stage pack loads");
        let result = verify_rows(&entry, &rows);
        assert_eq!(result.rows.len(), count, "{stage} row count");
        for row in &result.rows {
            let marked = expected_failure(&entry.event, &entry.method, &entry.stage, &row.case_key);
            if row.tier1.is_err() || !matches!(row.tier2, Tier2Outcome::Pass) {
                match marked {
                    Some(reason) => {
                        assert!(row.tier1.is_err(), "{} must fail tier 1", row.case_key);
                        assert!(!reason.is_empty());
                        ruled += 1;
                    }
                    None => failures.push(format!(
                        "{}/{}: {:?} {:?}",
                        stage, row.case_key, row.tier1, row.tier2
                    )),
                }
            } else if marked.is_some() {
                failures.push(format!(
                    "{}/{} is ruled failing but passes",
                    stage, row.case_key
                ));
            }
        }
    }
    assert_eq!(ruled, 9, "exactly the nine ruled rows fail");
    assert!(failures.is_empty(), "{}", failures.join("\n"));
}

#[test]
fn skewb_sarah_rows_pass_tier1_with_tier2_declared_skipped() {
    let mut failures = Vec::new();
    let entry = entry_of("skewb", "sarah_intermediate", "sarah");
    let rows = load_pack(&packs(), &entry).expect("skewb pack loads");
    let result = verify_rows(&entry, &rows);
    assert_eq!(result.rows.len(), 15);
    assert_eq!(result.honesty.len(), 1);
    for row in &result.rows {
        if !matches!(row.tier2, Tier2Outcome::Skipped(_)) || row.tier1.is_err() {
            failures.push(format!("{}: {:?} {:?}", row.case_key, row.tier1, row.tier2));
        }
    }
    assert!(failures.is_empty(), "{}", failures.join("\n"));
}

#[test]
fn pyraminx_l4e_rows_hold_the_four_edge_case_shape() {
    let open_findings = PYRA_OPEN;
    let entry = entry_of("pyraminx", "l4e", "l4e");
    let rows = load_pack(&packs(), &entry).expect("pyra pack loads");
    let result = verify_rows(&entry, &rows);
    assert_eq!(result.rows.len(), 35);
    assert_eq!(result.honesty.len(), 1);
    let mut failing = Vec::new();
    for row in &result.rows {
        let passing = row.tier1.is_ok() && matches!(row.tier2, Tier2Outcome::Pass);
        if open_findings.contains(&row.case_key.as_str()) {
            assert!(
                !passing,
                "{} is an open finding and must stay red",
                row.case_key
            );
            failing.push(row.case_key.clone());
        } else {
            assert!(passing, "{}: {:?} {:?}", row.case_key, row.tier1, row.tier2);
        }
    }
    assert_eq!(failing.len(), open_findings.len());
}

#[test]
fn megaminx_4lll_rows_hold_their_stage_start_state() {
    let open_findings = MEGA_OPEN;
    let entry = entry_of("megaminx", "4lll", "4lll");
    let rows = load_pack(&packs(), &entry).expect("mega pack loads");
    let result = verify_rows(&entry, &rows);
    assert_eq!(result.rows.len(), 39);
    assert_eq!(result.honesty.len(), 1);
    let mut failing = Vec::new();
    for row in &result.rows {
        let passing = row.tier1.is_ok() && matches!(row.tier2, Tier2Outcome::Pass);
        if open_findings.contains(&row.case_key.as_str()) {
            assert!(
                !passing,
                "{} is an open finding and must stay red",
                row.case_key
            );
            failing.push(row.case_key.clone());
        } else {
            assert!(passing, "{}: {:?} {:?}", row.case_key, row.tier1, row.tier2);
        }
    }
    assert_eq!(failing.len(), open_findings.len());
}

#[test]
fn fto_l4t_rows_pass_tier1_with_tier2_declared_skipped() {
    let mut failures = Vec::new();
    let entry = entry_of("fto", "l4t_redux", "l4t");
    let rows = load_pack(&packs(), &entry).expect("fto pack loads");
    let result = verify_rows(&entry, &rows);
    assert_eq!(result.rows.len(), 36);
    assert_eq!(result.honesty.len(), 1);
    for row in &result.rows {
        if !matches!(row.tier2, Tier2Outcome::Skipped(_)) || row.tier1.is_err() {
            failures.push(format!("{}: {:?} {:?}", row.case_key, row.tier1, row.tier2));
        }
    }
    assert!(failures.is_empty(), "{}", failures.join("\n"));
}

#[test]
fn yau_parity_rows_hold_the_reduction_invariants() {
    let open_findings = BIGCUBE_YAU_OPEN;
    let entry = entry_of("bigcube", "yau", "parity");
    let rows = load_pack(&packs(), &entry).expect("bigcube pack loads");
    let result = verify_rows(&entry, &rows);
    let mut failing = Vec::new();
    for row in &result.rows {
        let passing = row.tier1.is_ok() && matches!(row.tier2, Tier2Outcome::Pass);
        if open_findings.contains(&row.case_key.as_str()) {
            assert!(
                !passing,
                "{} is an open finding and must stay red",
                row.case_key
            );
            failing.push(row.case_key.clone());
        } else {
            assert!(passing, "{}: {:?} {:?}", row.case_key, row.tier1, row.tier2);
        }
    }
    assert_eq!(failing.len(), open_findings.len());
}

#[test]
fn reduction_l2e_rows_hold_the_reduction_invariants() {
    let open_findings = BIGCUBE_REDUCTION_OPEN;
    let entry = entry_of("bigcube", "reduction", "parity");
    let rows = load_pack(&packs(), &entry).expect("bigcube pack loads");
    let result = verify_rows(&entry, &rows);
    for row in &result.rows {
        let passing = row.tier1.is_ok() && matches!(row.tier2, Tier2Outcome::Pass);
        if open_findings.contains(&row.case_key.as_str()) {
            assert!(
                !passing,
                "{} is an open finding and must stay red",
                row.case_key
            );
        } else {
            assert!(passing, "{}: {:?} {:?}", row.case_key, row.tier1, row.tier2);
        }
    }
}

#[test]
fn oldpoch_rows_split_into_swaps_specials_letters_and_prose() {
    let entry = entry_of("3bld", "old_pochmann", "concept");
    let rows = load_pack(&packs(), &entry).expect("oldpoch pack loads");
    let result = verify_rows(&entry, &rows);
    assert_eq!(result.rows.len(), 40);
    let letter_keys: Vec<&str> = result
        .rows
        .iter()
        .filter(|row| matches!(row.tier2, Tier2Outcome::Skipped(_)))
        .map(|row| row.case_key.as_str())
        .collect();
    let prose: Vec<&&str> = letter_keys
        .iter()
        .filter(|key| row_of(&rows, key).alg.is_empty())
        .collect();
    assert_eq!(prose.len(), 2, "exactly op.09 and op.32 hold prose algs");
    for row in &result.rows {
        let raw = row_of(&rows, &row.case_key);
        if raw.alg.is_empty() {
            assert!(row.tier1.is_err());
            continue;
        }
        assert!(row.tier1.is_ok(), "{}: {:?}", row.case_key, row.tier1);
        let letter = serde_json::from_str::<serde_json::Value>(&raw.front_json)
            .expect("front json parses")
            .get("letter")
            .is_some();
        if letter {
            assert!(
                matches!(row.tier2, Tier2Outcome::Skipped(_)),
                "{}: {:?}",
                row.case_key,
                row.tier2
            );
        } else {
            assert!(
                matches!(row.tier2, Tier2Outcome::Pass),
                "{}: {:?}",
                row.case_key,
                row.tier2
            );
        }
    }
}

fn row_of<'a>(rows: &'a [bt_data::seedpack::SeedRow], key: &str) -> &'a bt_data::seedpack::SeedRow {
    rows.iter()
        .find(|row| row.case_key == key)
        .unwrap_or_else(|| panic!("row {key} exists"))
}

#[test]
fn m2_rows_deliver_their_targets_into_the_buffer() {
    let open_findings = M2_OPEN;
    let entry = entry_of("3bld", "m2", "concept");
    let rows = load_pack(&packs(), &entry).expect("m2 pack loads");
    let result = verify_rows(&entry, &rows);
    assert_eq!(result.rows.len(), 24);
    let mut failing = Vec::new();
    for row in &result.rows {
        let passing = row.tier1.is_ok() && matches!(row.tier2, Tier2Outcome::Pass);
        if open_findings.contains(&row.case_key.as_str()) {
            assert!(
                !passing,
                "{} is an open finding and must stay red",
                row.case_key
            );
            failing.push(row.case_key.clone());
        } else {
            assert!(passing, "{}: {:?} {:?}", row.case_key, row.tier1, row.tier2);
        }
    }
    assert_eq!(failing.len(), open_findings.len());
}

#[test]
fn style_specials_hold_their_declared_shapes() {
    let mut failures = Vec::new();
    let entry = entry_of("3bld", "3style", "parity");
    let rows = load_pack(&packs(), &entry).expect("3style pack loads");
    let result = verify_rows(&entry, &rows);
    assert_eq!(result.rows.len(), 85);
    for row in &result.rows {
        if !matches!(row.tier2, Tier2Outcome::Pass) || row.tier1.is_err() {
            failures.push(format!("{}: {:?} {:?}", row.case_key, row.tier1, row.tier2));
        }
    }
    assert!(failures.is_empty(), "{}", failures.join("\n"));
}

#[test]
fn cfop_rows_hold_their_step_start_states() {
    for (event, stage, count) in [
        ("3x3", "pll", 21),
        ("3x3", "oll", 57),
        ("3x3", "f2l", 164),
        ("oh", "pll", 21),
        ("oh", "oll", 57),
        ("oh", "f2l", 164),
    ] {
        let entry = entry_of(event, "cfop", stage);
        let rows = load_pack(&packs(), &entry).expect("cfop pack loads");
        let result = verify_rows(&entry, &rows);
        assert_eq!(result.rows.len(), count, "{event}/{stage} row count");
        let mut failing = Vec::new();
        for row in &result.rows {
            if row.tier1.is_err() || !matches!(row.tier2, Tier2Outcome::Pass) {
                failing.push(format!(
                    "{event}/{stage}/{}: {:?} {:?}",
                    row.case_key, row.tier1, row.tier2
                ));
            }
        }
        assert!(failing.is_empty(), "{}", failing.join("\n"));
    }
}

#[test]
fn the_live_pack_audit_is_fail_closed_clean() {
    let audit = audit_packs(&packs());
    assert!(audit.drift.is_empty(), "{}", audit.drift.join("\n"));
    assert!(
        audit.offenders.is_empty(),
        "offenders beyond the ruled table:\n{}",
        audit.offenders.join("\n")
    );
    assert_eq!(audit.sections.len(), PACK_EXPECTED_ROWS.len());
    let ruled: usize = audit.sections.iter().map(|s| s.ruled).sum();
    assert_eq!(ruled, 51, "18 sq1 plus the ruled families");
    for summary in &audit.sections {
        assert_eq!(
            summary.rows,
            summary.passed + summary.failed + summary.skipped + summary.excused,
            "{}/{}/{} tallies",
            summary.event,
            summary.method,
            summary.stage
        );
    }
    let ruled_keys = [
        ("megaminx", "4lll", "4lll", MEGA_OPEN),
        ("pyraminx", "l4e", "l4e", PYRA_OPEN),
        ("bigcube", "yau", "parity", BIGCUBE_YAU_OPEN),
        ("bigcube", "reduction", "parity", BIGCUBE_REDUCTION_OPEN),
        ("3bld", "m2", "concept", M2_OPEN),
    ];
    for (event, method, stage, keys) in ruled_keys {
        let summary = audit
            .sections
            .iter()
            .find(|s| s.event == event && s.method == method && s.stage == stage)
            .unwrap_or_else(|| panic!("{event}/{method}/{stage} summary exists"));
        assert_eq!(summary.ruled, keys.len(), "{event}/{method}/{stage}");
        assert_eq!(summary.failed, keys.len(), "{event}/{method}/{stage}");
    }
}

fn suffix_flips(token: &str) -> Vec<String> {
    let letter: String = token
        .chars()
        .take_while(|ch| ch.is_ascii_alphabetic() || *ch == 'w')
        .collect();
    if letter.is_empty() {
        return Vec::new();
    }
    ["", "'", "2", "2'"]
        .iter()
        .map(|suffix| format!("{letter}{suffix}"))
        .filter(|candidate| candidate != token)
        .collect()
}

#[test]
fn no_single_token_flip_reconciles_the_ruled_mega_rows() {
    let entry = entry_of("megaminx", "4lll", "4lll");
    let rows = load_pack(&packs(), &entry).expect("mega pack loads");
    for key in ["4lll.20", "4lll.21", "4lll.22", "4lll.24"] {
        let row = row_of(&rows, key);
        let tokens: Vec<String> = row.alg.split_whitespace().map(String::from).collect();
        let mut reconciled = 0;
        for index in 0..tokens.len() {
            for flip in suffix_flips(&tokens[index]) {
                let mut variant = tokens.clone();
                variant[index] = flip.clone();
                let mut probe_row = row.clone();
                probe_row.alg = variant.join(" ");
                let result = verify_rows(&entry, &[probe_row]);
                let outcome = &result.rows[0];
                if outcome.tier1.is_ok() && matches!(outcome.tier2, Tier2Outcome::Pass) {
                    reconciled += 1;
                }
            }
        }
        assert_eq!(
            reconciled, 0,
            "{key}: a single token flip reconciles, ruling stale"
        );
    }
}

#[test]
fn no_global_conjugation_reconciles_the_ruled_bigcube_rows() {
    let mut all_rows = Vec::new();
    let mut entries = Vec::new();
    for method in ["yau", "reduction"] {
        let entry = entry_of("bigcube", method, "parity");
        let rows = load_pack(&packs(), &entry).expect("bigcube pack loads");
        entries.push(entry);
        all_rows.push(rows);
    }
    for (rotation, inverse) in [
        ("x", "x'"),
        ("x'", "x"),
        ("x2", "x2"),
        ("y", "y'"),
        ("y2", "y2"),
        ("z", "z'"),
    ] {
        let mut reconciled = 0;
        for (rows, entry) in all_rows.iter().zip(entries.iter()) {
            for row in rows {
                let mut probe_row = row.clone();
                probe_row.alg = format!("{rotation} {} {inverse}", row.alg);
                let result = verify_rows(entry, &[probe_row]);
                let outcome = &result.rows[0];
                if outcome.tier1.is_ok() && matches!(outcome.tier2, Tier2Outcome::Pass) {
                    reconciled += 1;
                }
            }
        }
        assert_eq!(
            reconciled, 0,
            "conjugation {rotation} reconciles rows, ruling stale"
        );
    }
}
