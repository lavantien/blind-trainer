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
        if record.is_some() && outcome.qtm_match {
            summary.qtm_agreed += 1;
        }
        if record.is_some() && outcome.position_match {
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::SimRow;
    use bt_core::Alg;
    use bt_data::corpus::Row;
    use bt_gendata::synth::{SynthLimits, synth_case};
    use bt_scheme::{CORNER_BUFFER_STICKER, EDGE_BUFFER_STICKER, Orbit, enumerate_cases, name_of};
    use std::collections::HashMap;

    fn limits() -> SynthLimits {
        SynthLimits {
            max_setup: 2,
            max_insert: 4,
            node_budget: 100_000_000,
        }
    }

    fn row(solver: Solver, sheet: SheetClass, rowid: usize, position: &str, alg_text: &str) -> Row {
        Row {
            solver,
            sheet,
            rowid,
            position: position.to_string(),
            alg: Alg::parse(alg_text).expect("alg parses"),
            alg_text: alg_text.to_string(),
            commutator: String::new(),
            recon_link: String::new(),
            youtube_link: String::new(),
        }
    }

    fn three_style(orbit: Orbit) -> (String, String) {
        let case = enumerate_cases(orbit)[0];
        let hit = synth_case(orbit, case.t1, case.t2, &limits()).expect("case AB synthesizes");
        let buffer = match orbit {
            Orbit::Edge => EDGE_BUFFER_STICKER,
            Orbit::Corner => CORNER_BUFFER_STICKER,
        };
        (
            hit.alg.format(),
            format!(
                "{}-{}-{}",
                name_of(buffer),
                name_of(case.t1),
                name_of(case.t2)
            ),
        )
    }

    fn outcome_row(solver: &str, sheet: &str, rowid: usize, position: &str) -> RowOutcome {
        RowOutcome {
            solver: solver.to_string(),
            sheet: sheet.to_string(),
            rowid,
            position: position.to_string(),
            alg: String::new(),
            qtm: 0,
            htm: 0,
            simcheck_qtm: None,
            simcheck_position: None,
            verify_ok: true,
            problems: Vec::new(),
            notes: Vec::new(),
            qtm_match: true,
            position_match: true,
            pass: true,
        }
    }

    #[test]
    fn note_text_renders_flip_and_twist_with_signs() {
        assert_eq!(
            note_text(&Note::Flip {
                piece: "UF".to_string()
            }),
            "flip:UF"
        );
        assert_eq!(
            note_text(&Note::Twist {
                piece: "UFR".to_string(),
                twist: 2,
            }),
            "twist:UFR:+2"
        );
        assert_eq!(
            note_text(&Note::Twist {
                piece: "UFL".to_string(),
                twist: -1,
            }),
            "twist:UFL:-1"
        );
        assert_eq!(
            note_text(&Note::Twist {
                piece: "UBL".to_string(),
                twist: 0,
            }),
            "twist:UBL:+0"
        );
    }

    #[test]
    fn build_report_groups_rows_and_tallies_sheets() {
        let (edge_alg, edge_pos) = three_style(Orbit::Edge);
        let (corner_alg, corner_pos) = three_style(Orbit::Corner);
        let edge_qtm = Alg::parse(&edge_alg).unwrap().qtm();
        let corner_qtm = Alg::parse(&corner_alg).unwrap().qtm();
        let mut sim: SimMap = HashMap::new();
        sim.insert(
            (Solver::Cherry, SheetClass::Edge, 1),
            SimRow {
                position: edge_pos.clone(),
                qtm: edge_qtm,
            },
        );
        sim.insert(
            (Solver::Cherry, SheetClass::Edge, 2),
            SimRow {
                position: edge_pos.clone(),
                qtm: 4,
            },
        );
        sim.insert(
            (Solver::Eggins, SheetClass::Corner, 1),
            SimRow {
                position: corner_pos.clone(),
                qtm: corner_qtm,
            },
        );
        let corpus = Corpus::from_rows(vec![
            row(Solver::Cherry, SheetClass::Edge, 1, &edge_pos, &edge_alg),
            row(
                Solver::Eggins,
                SheetClass::Corner,
                1,
                &corner_pos,
                &corner_alg,
            ),
            row(Solver::Cherry, SheetClass::Edge, 2, &edge_pos, "R U R' U'"),
            row(Solver::Cherry, SheetClass::Twists, 1, "UFR & UFL", ""),
        ]);
        let report = build_report(&corpus, &sim, "stamp".to_string(), "cafe".repeat(8));
        assert_eq!(report.generator, "bt-verify");
        assert_eq!(report.generated_at, "stamp");
        assert_eq!(report.corpus_sha256, "cafe".repeat(8));
        assert_eq!(report.corpus_rows, 4);
        assert_eq!(report.simcheck_total, 3);
        assert_eq!(report.simcheck_matched, 3);
        assert_eq!(report.rows.len(), 4);
        assert_eq!(report.failures.len(), 2);
        assert_eq!(report.sheets.len(), 12);

        let passing = &report.rows[0];
        assert_eq!(
            (
                passing.solver.as_str(),
                passing.sheet.as_str(),
                passing.rowid
            ),
            ("cherry", "edge", 1)
        );
        assert_eq!(passing.position, edge_pos);
        assert_eq!(passing.alg, edge_alg);
        assert_eq!(passing.qtm, edge_qtm);
        assert_eq!(passing.htm, Alg::parse(&edge_alg).unwrap().htm());
        assert_eq!(passing.simcheck_qtm, Some(edge_qtm));
        assert_eq!(
            passing.simcheck_position.as_deref(),
            Some(edge_pos.as_str())
        );
        assert!(passing.verify_ok);
        assert!(passing.problems.is_empty());
        assert!(passing.notes.is_empty());
        assert!(passing.qtm_match);
        assert!(passing.position_match);
        assert!(passing.pass);

        let wrong_cycle = &report.rows[2];
        assert!(!wrong_cycle.verify_ok);
        assert!(!wrong_cycle.problems.is_empty());
        assert_eq!(wrong_cycle.qtm, 4);
        assert!(wrong_cycle.qtm_match);
        assert!(wrong_cycle.position_match);
        assert!(!wrong_cycle.pass);

        let twists = &report.rows[3];
        assert!(!twists.verify_ok);
        assert_eq!(twists.notes, vec!["twist:UFR:+0", "twist:UFL:+0"]);
        assert!(
            twists
                .problems
                .contains(&"missing simcheck record".to_string())
        );
        assert!(!twists.qtm_match);
        assert!(!twists.position_match);
        assert!(!twists.pass);
        assert!(
            twists
                .reasons()
                .iter()
                .any(|reason| reason == "qtm 0 vs simcheck None")
        );
        assert!(
            twists
                .reasons()
                .iter()
                .any(|reason| reason == "position UFR & UFL vs simcheck None")
        );

        let summary = |solver: &str, sheet: &str| -> &SheetSummary {
            report
                .sheets
                .iter()
                .find(|entry| entry.solver == solver && entry.sheet == sheet)
                .unwrap()
        };
        let cherry_edge = summary("cherry", "edge");
        assert_eq!(
            (
                cherry_edge.rows,
                cherry_edge.passed,
                cherry_edge.failed,
                cherry_edge.qtm_agreed,
                cherry_edge.position_agreed
            ),
            (2, 1, 1, 2, 2)
        );
        let eggins_corner = summary("eggins", "corner");
        assert_eq!(
            (
                eggins_corner.rows,
                eggins_corner.passed,
                eggins_corner.failed,
                eggins_corner.qtm_agreed,
                eggins_corner.position_agreed
            ),
            (1, 1, 0, 1, 1)
        );
        let cherry_twists = summary("cherry", "twists");
        assert_eq!(
            (
                cherry_twists.rows,
                cherry_twists.passed,
                cherry_twists.failed,
                cherry_twists.qtm_agreed,
                cherry_twists.position_agreed
            ),
            (1, 0, 1, 0, 0)
        );
        let untouched = summary("cherry", "parity");
        assert_eq!(
            (
                untouched.rows,
                untouched.passed,
                untouched.failed,
                untouched.qtm_agreed,
                untouched.position_agreed
            ),
            (0, 0, 0, 0, 0)
        );
        assert_eq!(report.failures[0].rowid, 2);
        assert_eq!(report.failures[1].sheet, "twists");
    }

    #[test]
    fn simcheck_mismatches_surface_in_reasons() {
        let (edge_alg, edge_pos) = three_style(Orbit::Edge);
        let qtm = Alg::parse(&edge_alg).unwrap().qtm();
        let mut sim: SimMap = HashMap::new();
        sim.insert(
            (Solver::Cherry, SheetClass::Edge, 1),
            SimRow {
                position: "UF-UB-UL".to_string(),
                qtm: qtm + 1,
            },
        );
        let corpus = Corpus::from_rows(vec![row(
            Solver::Cherry,
            SheetClass::Edge,
            1,
            &edge_pos,
            &edge_alg,
        )]);
        let report = build_report(&corpus, &sim, "t".to_string(), "0".repeat(64));
        let outcome = &report.rows[0];
        assert!(outcome.verify_ok);
        assert!(!outcome.qtm_match);
        assert!(!outcome.position_match);
        assert!(!outcome.pass);
        assert_eq!(report.failures.len(), 1);
        assert_eq!(
            outcome.reasons(),
            vec![
                format!("qtm {qtm} vs simcheck Some({})", qtm + 1),
                format!("position {edge_pos} vs simcheck Some(\"UF-UB-UL\")"),
            ]
        );
    }

    #[test]
    fn audit_lists_failures_and_population_gaps_in_order() {
        let mut failure = outcome_row("cherry", "edge", 7, "UB");
        failure.qtm = 5;
        failure.verify_ok = false;
        failure.problems = vec!["bad position: unknown sticker name UB".to_string()];
        failure.qtm_match = false;
        failure.position_match = false;
        failure.pass = false;
        let report = Report {
            generated_at: "t".to_string(),
            generator: "bt-verify".to_string(),
            corpus_sha256: "0".repeat(64),
            corpus_rows: 5,
            simcheck_total: 9,
            simcheck_matched: 4,
            sheets: vec![SheetSummary {
                solver: "cherry".to_string(),
                sheet: "edge".to_string(),
                rows: 3,
                passed: 2,
                failed: 1,
                qtm_agreed: 3,
                position_agreed: 3,
            }],
            rows: Vec::new(),
            failures: vec![failure],
        };
        let offenders = audit(&report, 7);
        let mut expected = vec![
            "cherry/edge/7 UB: bad position: unknown sticker name UB; qtm 5 vs simcheck None; position UB vs simcheck None".to_string(),
            "corpus holds 5 rows, expected 1077".to_string(),
            "simcheck matched 4 of 5 corpus rows".to_string(),
            "simcheck holds 7 rows, 4 matched".to_string(),
            "cherry/edge holds 3 rows, expected 551".to_string(),
        ];
        for &(solver, sheet, _) in crate::SHEET_EXPECTED_ROWS.iter().skip(1) {
            expected.push(format!("{solver}/{sheet} summary is missing"));
        }
        assert_eq!(offenders, expected);
    }

    #[test]
    fn audit_names_a_planted_wrong_alg_row() {
        let (edge_alg, edge_pos) = three_style(Orbit::Edge);
        let planted = "R U R' U'";
        let planted_qtm = Alg::parse(planted).unwrap().qtm();
        let mut sim: SimMap = HashMap::new();
        sim.insert(
            (Solver::Cherry, SheetClass::Edge, 1),
            SimRow {
                position: edge_pos.clone(),
                qtm: Alg::parse(&edge_alg).unwrap().qtm(),
            },
        );
        sim.insert(
            (Solver::Cherry, SheetClass::Edge, 2),
            SimRow {
                position: edge_pos.clone(),
                qtm: planted_qtm,
            },
        );
        let corpus = Corpus::from_rows(vec![
            row(Solver::Cherry, SheetClass::Edge, 1, &edge_pos, &edge_alg),
            row(Solver::Cherry, SheetClass::Edge, 2, &edge_pos, planted),
        ]);
        let report = build_report(&corpus, &sim, "t".to_string(), "0".repeat(64));
        assert_eq!(report.failures.len(), 1);
        let offender = &report.failures[0];
        assert_eq!(offender.rowid, 2);
        assert_eq!(offender.alg, planted);
        assert!(!offender.verify_ok);
        assert!(!offender.problems.is_empty());
        assert!(offender.problems.iter().all(|problem| !problem.is_empty()));
        assert!(offender.qtm_match);
        assert!(offender.position_match);
        assert_eq!(offender.reasons(), offender.problems);
        let offenders = audit(&report, 2);
        let prefix = format!("cherry/edge/2 {edge_pos}: ");
        assert!(offenders[0].starts_with(&prefix));
        assert!(offenders[0].len() > prefix.len());
    }

    #[test]
    fn audit_accepts_a_fully_consistent_report() {
        let sheets: Vec<SheetSummary> = crate::SHEET_EXPECTED_ROWS
            .iter()
            .map(|&(solver, sheet, rows)| SheetSummary {
                solver: solver.to_string(),
                sheet: sheet.to_string(),
                rows,
                passed: rows,
                failed: 0,
                qtm_agreed: rows,
                position_agreed: rows,
            })
            .collect();
        let report = Report {
            generated_at: "t".to_string(),
            generator: "bt-verify".to_string(),
            corpus_sha256: "0".repeat(64),
            corpus_rows: bt_gendata::TOTAL_ROWS,
            simcheck_total: bt_gendata::TOTAL_ROWS,
            simcheck_matched: bt_gendata::TOTAL_ROWS,
            sheets,
            rows: Vec::new(),
            failures: Vec::new(),
        };
        assert!(audit(&report, bt_gendata::TOTAL_ROWS).is_empty());
    }

    #[test]
    fn render_markdown_prints_tables_checks_and_the_summary() {
        let mut failure = outcome_row("eggins", "corner", 4, "pos");
        failure.verify_ok = false;
        failure.problems = vec!["problems here".to_string()];
        failure.pass = false;
        let report = Report {
            generated_at: "2026-10-14T00:00:00Z".to_string(),
            generator: "bt-verify".to_string(),
            corpus_sha256: "ab".repeat(32),
            corpus_rows: 3,
            simcheck_total: 2,
            simcheck_matched: 2,
            sheets: vec![
                SheetSummary {
                    solver: "cherry".to_string(),
                    sheet: "edge".to_string(),
                    rows: 2,
                    passed: 1,
                    failed: 1,
                    qtm_agreed: 2,
                    position_agreed: 2,
                },
                SheetSummary {
                    solver: "eggins".to_string(),
                    sheet: "ltct".to_string(),
                    rows: 1,
                    passed: 1,
                    failed: 0,
                    qtm_agreed: 1,
                    position_agreed: 1,
                },
            ],
            rows: vec![
                outcome_row("cherry", "edge", 1, "a"),
                outcome_row("cherry", "edge", 2, "b"),
                outcome_row("eggins", "ltct", 3, "c"),
            ],
            failures: vec![failure],
        };
        let md = render_markdown(&report);
        assert!(md.starts_with("# alg verification report\n\n"));
        assert!(md.contains("generator: bt-verify\n"));
        assert!(md.contains("generated_at: 2026-10-14T00:00:00Z\n"));
        assert!(md.contains(&format!("corpus sha256: {}\n", "ab".repeat(32))));
        assert!(md.contains("corpus rows: 3\n"));
        assert!(md.contains("simcheck rows: 2 matched: 2\n"));
        assert!(md.contains(
            "| solver | sheet | rows | passed | failed | qtm agreed | position agreed |\n| --- | --- | --- | --- | --- | --- | --- |\n"
        ));
        assert!(md.contains("| cherry | edge | 2 | 1 | 1 | 2 | 2 |\n"));
        assert!(md.contains("| eggins | ltct | 1 | 1 | 0 | 1 | 1 |\n"));
        assert!(
            md.contains("verify: every row passes geometric verification on the emulated cube\n")
        );
        assert!(md.contains("- eggins/corner/4 pos: problems here\n"));
        assert!(md.ends_with("## summary\n\n2/3 rows pass\n"));
    }

    #[test]
    fn render_markdown_prints_none_when_nothing_failed() {
        let report = Report {
            generated_at: "t".to_string(),
            generator: "bt-verify".to_string(),
            corpus_sha256: "0".repeat(64),
            corpus_rows: 1,
            simcheck_total: 1,
            simcheck_matched: 1,
            sheets: Vec::new(),
            rows: vec![outcome_row("cherry", "edge", 1, "a")],
            failures: Vec::new(),
        };
        let md = render_markdown(&report);
        assert!(md.contains("## failures\n\nnone\n\n"));
        assert!(md.ends_with("## summary\n\n1/1 rows pass\n"));
    }
}
