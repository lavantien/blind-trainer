use crate::SimMap;
use bt_data::corpus::{Corpus, SheetClass, Solver};
use bt_data::verify::{Note, verify};
use serde::{Deserialize, Serialize};

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct RowOutcome {
    pub solver: String,
    pub sheet: String,
    pub rowid: usize,
    pub position: String,
    pub alg: String,
    pub qtm: u32,
    pub htm: u32,
    pub simcheck_qtm: Option<u32>,
    pub simcheck_position: Option<String>,
    pub verify_ok: bool,
    pub problems: Vec<String>,
    pub notes: Vec<String>,
    pub qtm_match: bool,
    pub position_match: bool,
    pub pass: bool,
}

impl RowOutcome {
    pub fn reasons(&self) -> Vec<String> {
        let mut reasons = self.problems.clone();
        if !self.qtm_match {
            reasons.push(format!(
                "qtm {} vs simcheck {:?}",
                self.qtm, self.simcheck_qtm
            ));
        }
        if !self.position_match {
            reasons.push(format!(
                "position {} vs simcheck {:?}",
                self.position, self.simcheck_position
            ));
        }
        reasons
    }
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct SheetSummary {
    pub solver: String,
    pub sheet: String,
    pub rows: usize,
    pub passed: usize,
    pub failed: usize,
    pub qtm_agreed: usize,
    pub position_agreed: usize,
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct Report {
    pub generated_at: String,
    pub generator: String,
    pub corpus_sha256: String,
    pub corpus_rows: usize,
    pub simcheck_total: usize,
    pub simcheck_matched: usize,
    pub sheets: Vec<SheetSummary>,
    pub rows: Vec<RowOutcome>,
    pub failures: Vec<RowOutcome>,
}

fn note_text(note: &Note) -> String {
    match note {
        Note::Flip { piece } => format!("flip:{piece}"),
        Note::Twist { piece, twist } => format!("twist:{piece}:{twist:+}"),
    }
}

pub fn build_report(
    corpus: &Corpus,
    sim: &SimMap,
    generated_at: String,
    corpus_sha256: String,
) -> Report {
    let mut sheets: Vec<SheetSummary> = Vec::new();
    for solver in Solver::VENDOR {
        for sheet in SheetClass::ALL {
            sheets.push(SheetSummary {
                solver: solver.to_string(),
                sheet: sheet.file().to_string(),
                rows: 0,
                passed: 0,
                failed: 0,
                qtm_agreed: 0,
                position_agreed: 0,
            });
        }
    }
    let mut rows = Vec::new();
    let mut failures = Vec::new();
    let mut matched = 0;
    for row in corpus.rows() {
        let verdict = verify(&row.alg, &row.position, row.sheet);
        let record = sim.get(&(row.solver, row.sheet, row.rowid));
        if record.is_some() {
            matched += 1;
        }
        let qtm_match = record.is_some_and(|r| r.qtm == row.qtm());
        let position_match = record.is_some_and(|r| r.position == row.position);
        let mut problems: Vec<String> = verdict.problems.iter().map(|p| p.to_string()).collect();
        if record.is_none() {
            problems.push("missing simcheck record".to_string());
        }
        let verify_ok = verdict.ok();
        let outcome = RowOutcome {
            solver: row.solver.to_string(),
            sheet: row.sheet.file().to_string(),
            rowid: row.rowid,
            position: row.position.clone(),
            alg: row.alg_text.clone(),
            qtm: row.qtm(),
            htm: row.htm(),
            simcheck_qtm: record.map(|r| r.qtm),
            simcheck_position: record.map(|r| r.position.clone()),
            verify_ok,
            problems,
            notes: verdict.notes.iter().map(note_text).collect(),
            qtm_match,
            position_match,
            pass: verify_ok && qtm_match && position_match,
        };
        let summary = sheets
            .iter_mut()
            .find(|s| s.solver == outcome.solver && s.sheet == outcome.sheet)
            .expect("summary slot exists");
        summary.rows += 1;
        if outcome.pass {
            summary.passed += 1;
        } else {
            summary.failed += 1;
            failures.push(outcome.clone());
        }
        if outcome.qtm_match {
            summary.qtm_agreed += 1;
        }
        if outcome.position_match {
            summary.position_agreed += 1;
        }
        rows.push(outcome);
    }
    Report {
        generated_at,
        generator: "bt-verify".to_string(),
        corpus_sha256,
        corpus_rows: corpus.total_rows(),
        simcheck_total: sim.len(),
        simcheck_matched: matched,
        sheets,
        rows,
        failures,
    }
}

pub fn audit(report: &Report, simcheck_total: usize) -> Vec<String> {
    let mut offenders = Vec::new();
    for failure in &report.failures {
        offenders.push(format!(
            "{}/{}/{} {}: {}",
            failure.solver,
            failure.sheet,
            failure.rowid,
            failure.position,
            failure.reasons().join("; ")
        ));
    }
    if report.corpus_rows != bt_gendata::TOTAL_ROWS {
        offenders.push(format!(
            "corpus holds {} rows, expected {}",
            report.corpus_rows,
            bt_gendata::TOTAL_ROWS
        ));
    }
    if report.simcheck_matched != report.corpus_rows {
        offenders.push(format!(
            "simcheck matched {} of {} corpus rows",
            report.simcheck_matched, report.corpus_rows
        ));
    }
    if simcheck_total != report.simcheck_matched {
        offenders.push(format!(
            "simcheck holds {simcheck_total} rows, {} matched",
            report.simcheck_matched
        ));
    }
    for &(solver, sheet, expected) in crate::SHEET_EXPECTED_ROWS.iter() {
        let Some(summary) = report
            .sheets
            .iter()
            .find(|s| s.solver == solver && s.sheet == sheet)
        else {
            offenders.push(format!("{solver}/{sheet} summary is missing"));
            continue;
        };
        if summary.rows != expected {
            offenders.push(format!(
                "{solver}/{sheet} holds {} rows, expected {expected}",
                summary.rows
            ));
        }
    }
    offenders
}

pub fn render_markdown(report: &Report) -> String {
    let mut md = String::new();
    md.push_str("# alg verification report\n\n");
    md.push_str(&format!("generator: {}\n", report.generator));
    md.push_str(&format!("generated_at: {}\n", report.generated_at));
    md.push_str(&format!("corpus sha256: {}\n", report.corpus_sha256));
    md.push_str(&format!("corpus rows: {}\n", report.corpus_rows));
    md.push_str(&format!(
        "simcheck rows: {} matched: {}\n\n",
        report.simcheck_total, report.simcheck_matched
    ));
    md.push_str("## per solver and sheet\n\n");
    md.push_str("| solver | sheet | rows | passed | failed | qtm agreed | position agreed |\n");
    md.push_str("| --- | --- | --- | --- | --- | --- | --- |\n");
    for sheet in &report.sheets {
        md.push_str(&format!(
            "| {} | {} | {} | {} | {} | {} | {} |\n",
            sheet.solver,
            sheet.sheet,
            sheet.rows,
            sheet.passed,
            sheet.failed,
            sheet.qtm_agreed,
            sheet.position_agreed
        ));
    }
    md.push_str("\n## checks\n\n");
    md.push_str("verify: every row passes geometric verification on the emulated cube\n");
    md.push_str("qtm: quarter turn counts agree with the independent simcheck oracle\n");
    md.push_str("position: sheet positions agree with simcheck\n\n");
    md.push_str("## failures\n\n");
    if report.failures.is_empty() {
        md.push_str("none\n\n");
    } else {
        for failure in &report.failures {
            md.push_str(&format!(
                "- {}/{}/{} {}: {}\n",
                failure.solver,
                failure.sheet,
                failure.rowid,
                failure.position,
                failure.reasons().join("; ")
            ));
        }
        md.push('\n');
    }
    md.push_str("## summary\n\n");
    md.push_str(&format!(
        "{}/{} rows pass\n",
        report.rows.len() - report.failures.len(),
        report.rows.len()
    ));
    md
}
