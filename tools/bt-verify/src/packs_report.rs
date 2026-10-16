use crate::packs::{RowOutcome, SectionResult, Tier2Outcome, pinned_vs_registry, verify_rows};
use bt_data::seedpack::{PackEntry, expected_failure, load_pack, load_registry};
use std::path::Path;

pub const PROSE_ALG: &str = "prose row holds no mechanical alg";

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct PackSectionSummary {
    pub event: String,
    pub method: String,
    pub stage: String,
    pub file: String,
    pub rows: usize,
    pub passed: usize,
    pub failed: usize,
    pub skipped: usize,
    pub excused: usize,
    pub ruled: usize,
    pub honesty: Vec<String>,
}

#[derive(Clone, PartialEq, Eq, Debug, Default)]
pub struct PackAudit {
    pub drift: Vec<String>,
    pub offenders: Vec<String>,
    pub sections: Vec<PackSectionSummary>,
}

fn failure_text(row: &RowOutcome) -> String {
    let mut parts = Vec::new();
    if let Err(err) = &row.tier1 {
        parts.push(format!("tier1: {err}"));
    }
    if let Tier2Outcome::Fail(msg) = &row.tier2 {
        parts.push(format!("tier2: {msg}"));
    }
    parts.join("; ")
}

pub fn row_offender(event: &str, method: &str, stage: &str, row: &RowOutcome) -> Option<String> {
    let prose = matches!(&row.tier1, Err(err) if err == PROSE_ALG);
    if prose {
        return None;
    }
    let failed = row.tier1.is_err() || matches!(row.tier2, Tier2Outcome::Fail(_));
    let ruling = expected_failure(event, method, stage, &row.case_key);
    match (failed, ruling) {
        (true, Some(_)) => None,
        (true, None) => Some(format!(
            "{event}/{method}/{stage}/{}: {}",
            row.case_key,
            failure_text(row)
        )),
        (false, Some(reason)) => Some(format!(
            "{event}/{method}/{stage}/{}: stale ruling ({reason}), row passes",
            row.case_key
        )),
        (false, None) => None,
    }
}

fn summarize(entry: &PackEntry, section: &SectionResult) -> (PackSectionSummary, Vec<String>) {
    let mut summary = PackSectionSummary {
        event: entry.event.clone(),
        method: entry.method.clone(),
        stage: entry.stage.clone(),
        file: entry.file.clone(),
        rows: section.rows.len(),
        passed: 0,
        failed: 0,
        skipped: 0,
        excused: 0,
        ruled: 0,
        honesty: section.honesty.clone(),
    };
    let mut offenders = Vec::new();
    for row in &section.rows {
        let prose = matches!(&row.tier1, Err(err) if err == PROSE_ALG);
        let failed = row.tier1.is_err() || matches!(row.tier2, Tier2Outcome::Fail(_));
        let ruled =
            expected_failure(&entry.event, &entry.method, &entry.stage, &row.case_key).is_some();
        if prose {
            summary.excused += 1;
        } else if failed {
            summary.failed += 1;
            if ruled {
                summary.ruled += 1;
            }
        } else if matches!(row.tier2, Tier2Outcome::Skipped(_)) {
            summary.skipped += 1;
        } else {
            summary.passed += 1;
        }
        if let Some(offender) = row_offender(&entry.event, &entry.method, &entry.stage, row) {
            offenders.push(offender);
        }
    }
    (summary, offenders)
}

pub fn audit_packs(packs_root: &Path) -> PackAudit {
    let drift = pinned_vs_registry(packs_root);
    let mut audit = PackAudit {
        drift,
        ..Default::default()
    };
    audit.offenders.extend(audit.drift.iter().cloned());
    let Ok(entries) = load_registry(packs_root) else {
        return audit;
    };
    for entry in &entries {
        let rows = match load_pack(packs_root, entry) {
            Ok(rows) => rows,
            Err(err) => {
                audit.offenders.push(format!(
                    "{}/{}/{}: pack {} fails to load: {err}",
                    entry.event, entry.method, entry.stage, entry.file
                ));
                continue;
            }
        };
        let section = verify_rows(entry, &rows);
        let (summary, offenders) = summarize(entry, &section);
        audit.offenders.extend(offenders);
        audit.sections.push(summary);
    }
    audit
}

pub fn render_packs_markdown(audit: &PackAudit) -> String {
    let mut md = String::new();
    md.push_str("# pack verification report\n\n");
    md.push_str("fail closed: every tier 1 or tier 2 failure without an expected_failure ruling is an offender, every ruled row failing nothing is a stale ruling\n\n");
    md.push_str("## sections\n\n");
    md.push_str(
        "| event | method | stage | file | rows | passed | failed | skipped | excused | ruled |\n",
    );
    md.push_str("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n");
    for section in &audit.sections {
        md.push_str(&format!(
            "| {} | {} | {} | {} | {} | {} | {} | {} | {} | {} |\n",
            section.event,
            section.method,
            section.stage,
            section.file,
            section.rows,
            section.passed,
            section.failed,
            section.skipped,
            section.excused,
            section.ruled
        ));
    }
    md.push_str("\n## honesty\n\n");
    let mut honesty_lines = 0;
    for section in &audit.sections {
        for note in &section.honesty {
            md.push_str(&format!(
                "- {}/{}/{}: {note}\n",
                section.event, section.method, section.stage
            ));
            honesty_lines += 1;
        }
    }
    if honesty_lines == 0 {
        md.push_str("none\n");
    }
    md.push_str("\n## offenders\n\n");
    if audit.offenders.is_empty() {
        md.push_str("none\n");
    } else {
        for offender in &audit.offenders {
            md.push_str(&format!("- {offender}\n"));
        }
    }
    md
}

#[cfg(test)]
mod tests {
    use super::*;

    fn outcome(case_key: &str, tier1: Result<(), String>, tier2: Tier2Outcome) -> RowOutcome {
        RowOutcome {
            case_key: case_key.to_string(),
            tier1,
            tier2,
        }
    }

    #[test]
    fn an_unruled_failure_is_an_offender() {
        let row = outcome(
            "ep.99",
            Err("unexecutable slash sequence".to_string()),
            Tier2Outcome::Skipped("tier 1 failed"),
        );
        assert_eq!(
            row_offender("sq1", "lbl", "ep", &row),
            Some("sq1/lbl/ep/ep.99: tier1: unexecutable slash sequence".to_string())
        );
    }

    #[test]
    fn a_ruled_failure_is_excused() {
        let row = outcome(
            "cs.119",
            Err("unexecutable slash sequence: hand traced".to_string()),
            Tier2Outcome::Skipped("tier 1 failed"),
        );
        assert_eq!(row_offender("sq1", "lbl", "cubeshape", &row), None);
    }

    #[test]
    fn a_ruling_on_a_passing_row_is_stale() {
        let row = outcome("cs.075", Ok(()), Tier2Outcome::Pass);
        let offender =
            row_offender("sq1", "lbl", "cubeshape", &row).expect("stale ruling surfaces");
        assert!(offender.starts_with("sq1/lbl/cubeshape/cs.075: stale ruling ("));
        assert!(offender.ends_with("), row passes"));
    }

    #[test]
    fn prose_rows_and_clean_passes_never_offend() {
        let prose = outcome(
            "op.09",
            Err(PROSE_ALG.to_string()),
            Tier2Outcome::Skipped("tier 1 failed"),
        );
        assert_eq!(
            row_offender("3bld", "old_pochmann", "concept", &prose),
            None
        );
        let clean = outcome("op.01", Ok(()), Tier2Outcome::Pass);
        assert_eq!(
            row_offender("3bld", "old_pochmann", "concept", &clean),
            None
        );
    }

    #[test]
    fn tier2_failures_carry_their_message() {
        let row = outcome(
            "l4e.99",
            Ok(()),
            Tier2Outcome::Fail("five edges displaced".to_string()),
        );
        assert_eq!(
            row_offender("pyraminx", "l4e", "l4e", &row),
            Some("pyraminx/l4e/l4e/l4e.99: tier2: five edges displaced".to_string())
        );
    }

    fn entry() -> PackEntry {
        PackEntry {
            file: "probe.tsv".to_string(),
            event: "sq1".to_string(),
            method: "lbl".to_string(),
            stage: "cubeshape".to_string(),
            n: bt_data::seedpack::NSpec::Unset,
            expected_rows: 5,
        }
    }

    #[test]
    fn summarize_counts_each_row_kind_once() {
        let section = SectionResult {
            rows: vec![
                outcome("a", Ok(()), Tier2Outcome::Pass),
                outcome("cs.075", Ok(()), Tier2Outcome::Fail("flipped".to_string())),
                outcome("c", Ok(()), Tier2Outcome::Skipped("letter scheme")),
                outcome(
                    "op.09",
                    Err(PROSE_ALG.to_string()),
                    Tier2Outcome::Skipped("tier 1 failed"),
                ),
                outcome(
                    "cs.119",
                    Err("unexecutable".to_string()),
                    Tier2Outcome::Skipped("tier 1 failed"),
                ),
            ],
            honesty: vec!["note".to_string()],
        };
        let entry = entry();
        let (summary, offenders) = summarize(&entry, &section);
        assert_eq!(
            (
                summary.rows,
                summary.passed,
                summary.failed,
                summary.skipped,
                summary.excused,
                summary.ruled
            ),
            (5, 1, 2, 1, 1, 2)
        );
        assert_eq!(summary.honesty, vec!["note".to_string()]);
        assert!(offenders.is_empty(), "{}", offenders.join("; "));
    }

    #[test]
    fn markdown_renders_sections_honesty_and_offenders() {
        let audit = PackAudit {
            drift: vec!["pin missing".to_string()],
            offenders: vec![
                "pin missing".to_string(),
                "pyraminx/l4e/l4e/l4e.07: tier2: red".to_string(),
            ],
            sections: vec![PackSectionSummary {
                event: "pyraminx".to_string(),
                method: "l4e".to_string(),
                stage: "l4e".to_string(),
                file: "pyraminx_l4e.tsv".to_string(),
                rows: 35,
                passed: 26,
                failed: 9,
                skipped: 0,
                excused: 0,
                ruled: 0,
                honesty: vec!["edges only".to_string()],
            }],
        };
        let md = render_packs_markdown(&audit);
        assert!(md.starts_with("# pack verification report\n\n"));
        assert!(
            md.contains("| pyraminx | l4e | l4e | pyraminx_l4e.tsv | 35 | 26 | 9 | 0 | 0 | 0 |\n")
        );
        assert!(md.contains("- pyraminx/l4e/l4e: edges only\n"));
        assert!(md.contains("- pin missing\n"));
        assert!(md.contains("- pyraminx/l4e/l4e/l4e.07: tier2: red\n"));
    }

    #[test]
    fn markdown_with_nothing_to_say_says_none() {
        let md = render_packs_markdown(&PackAudit::default());
        assert!(md.contains("## honesty\n\nnone\n"));
        assert!(md.contains("## offenders\n\nnone\n"));
    }
}
