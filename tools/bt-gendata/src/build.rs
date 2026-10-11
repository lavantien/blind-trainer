use crate::hash::CorpusDigest;
use crate::model::{
    CasesFile, CasesInfo, ClassCount, Manifest, OrbitCoverage, SpecialEntry, SpecialsFile,
    SpecialsInfo, VendorAlg,
};
use bt_data::corpus::{Corpus, Row, SheetClass};
use bt_data::verify::{Note, verify};
use bt_scheme::{
    CORNER_BUFFER_STICKER, Classification, EDGE_BUFFER_STICKER, Orbit, classify_alg,
    enumerate_cases, letter_of, name_of,
};
use std::collections::HashMap;

pub struct Built {
    pub cases: CasesFile,
    pub specials: SpecialsFile,
    pub rows_in_pair_space: usize,
    pub rows_other_buffer: usize,
    pub offenders: Vec<String>,
}

fn orbit_name(orbit: Orbit) -> &'static str {
    match orbit {
        Orbit::Edge => "edge",
        Orbit::Corner => "corner",
    }
}

fn buffer_sticker(orbit: Orbit) -> u8 {
    match orbit {
        Orbit::Edge => EDGE_BUFFER_STICKER,
        Orbit::Corner => CORNER_BUFFER_STICKER,
    }
}

fn note_text(note: &Note) -> String {
    match note {
        Note::Flip { piece } => format!("flip:{piece}"),
        Note::Twist { piece, twist } => format!("twist:{piece}:{twist:+}"),
    }
}

fn verdict_of(row: &Row) -> (bool, Vec<String>, Vec<String>) {
    let verdict = verify(&row.alg, &row.position, row.sheet);
    (
        verdict.ok(),
        verdict.problems.iter().map(|p| p.to_string()).collect(),
        verdict.notes.iter().map(note_text).collect(),
    )
}

pub fn build(corpus: &Corpus) -> Built {
    let mut cases: Vec<crate::model::CaseEntry> = Vec::with_capacity(crate::TOTAL_CASES);
    let mut index: HashMap<(Orbit, u8, u8), usize> = HashMap::new();
    for orbit in [Orbit::Edge, Orbit::Corner] {
        for case in enumerate_cases(orbit) {
            index.insert((orbit, case.t1, case.t2), cases.len());
            let (t1_letter, t2_letter) = (letter_of(case.t1), letter_of(case.t2));
            cases.push(crate::model::CaseEntry {
                orbit: orbit_name(orbit).to_string(),
                t1: case.t1,
                t2: case.t2,
                t1_letter,
                t2_letter,
                t1_name: name_of(case.t1).to_string(),
                t2_name: name_of(case.t2).to_string(),
                covered: false,
                vendor_algs: Vec::new(),
            });
        }
    }
    let mut specials = Vec::new();
    let mut offenders = Vec::new();
    let mut rows_in_pair_space = 0;
    let mut rows_other_buffer = 0;
    for row in corpus.rows() {
        let (ok, problems, notes) = verdict_of(row);
        if !ok {
            offenders.push(format!(
                "{}/{}/{} {}: {}",
                row.solver,
                row.sheet,
                row.rowid,
                row.position,
                problems.join("; ")
            ));
        }
        let alg = VendorAlg {
            solver: row.solver.to_string(),
            rowid: row.rowid,
            position: row.position.clone(),
            alg: row.alg_text.clone(),
            qtm: row.qtm(),
            htm: row.htm(),
            ok,
            problems,
            notes,
            commutator: row.commutator.clone(),
            recon_link: row.recon_link.clone(),
            youtube_link: row.youtube_link.clone(),
        };
        match row.sheet {
            SheetClass::Edge | SheetClass::Corner => {
                let expected = if row.sheet == SheetClass::Edge {
                    Orbit::Edge
                } else {
                    Orbit::Corner
                };
                let report = classify_alg(&row.alg);
                let Classification::ThreeStyle {
                    orbit,
                    cases: row_cases,
                } = report.class
                else {
                    offenders.push(format!(
                        "{}/{}/{} is not a three style case: {:?}",
                        row.solver, row.sheet, row.rowid, report.class
                    ));
                    continue;
                };
                if orbit != expected {
                    offenders.push(format!(
                        "{}/{}/{} walks the wrong orbit {:?}",
                        row.solver, row.sheet, row.rowid, orbit
                    ));
                    continue;
                }
                if row_cases.len() != 1 {
                    offenders.push(format!(
                        "{}/{}/{} walks {} cases at once",
                        row.solver,
                        row.sheet,
                        row.rowid,
                        row_cases.len()
                    ));
                    continue;
                }
                let trace = match orbit {
                    Orbit::Edge => &report.edge,
                    Orbit::Corner => &report.corner,
                };
                let Some(block) = trace.blocks.first() else {
                    offenders.push(format!(
                        "{}/{}/{} has no walk block",
                        row.solver, row.sheet, row.rowid
                    ));
                    continue;
                };
                if block[0] != buffer_sticker(orbit) {
                    rows_other_buffer += 1;
                    continue;
                }
                let case = row_cases[0];
                let Some(&slot) = index.get(&(orbit, case.t1, case.t2)) else {
                    offenders.push(format!(
                        "{}/{}/{} walks outside the pair space: {}-{}",
                        row.solver,
                        row.sheet,
                        row.rowid,
                        name_of(case.t1),
                        name_of(case.t2)
                    ));
                    continue;
                };
                rows_in_pair_space += 1;
                cases[slot].vendor_algs.push(alg);
            }
            sheet => {
                let VendorAlg {
                    solver,
                    rowid,
                    position,
                    alg,
                    qtm,
                    htm,
                    ok,
                    problems,
                    notes,
                    commutator,
                    recon_link,
                    youtube_link,
                } = alg;
                specials.push(SpecialEntry {
                    class: sheet.file().to_string(),
                    solver,
                    rowid,
                    position,
                    alg,
                    qtm,
                    htm,
                    ok,
                    problems,
                    notes,
                    commutator,
                    recon_link,
                    youtube_link,
                });
            }
        }
    }
    let entries: Vec<crate::model::CaseEntry> = cases
        .into_iter()
        .map(|mut case| {
            case.covered = !case.vendor_algs.is_empty();
            case
        })
        .collect();
    Built {
        cases: CasesFile { cases: entries },
        specials: SpecialsFile { specials },
        rows_in_pair_space,
        rows_other_buffer,
        offenders,
    }
}

pub fn manifest(
    built: &Built,
    digest: &CorpusDigest,
    cases_sha256: &str,
    specials_sha256: &str,
    generated_at: String,
) -> Manifest {
    let mut edge = OrbitCoverage {
        total: 0,
        covered: 0,
        uncovered: 0,
    };
    let mut corner = OrbitCoverage {
        total: 0,
        covered: 0,
        uncovered: 0,
    };
    for case in &built.cases.cases {
        let slot = if case.orbit == "edge" {
            &mut edge
        } else {
            &mut corner
        };
        slot.total += 1;
        if case.covered {
            slot.covered += 1;
        } else {
            slot.uncovered += 1;
        }
    }
    let counts = crate::SPECIAL_COUNTS
        .iter()
        .map(|&(solver, class, _)| ClassCount {
            solver: solver.to_string(),
            class: class.to_string(),
            rows: built
                .specials
                .specials
                .iter()
                .filter(|entry| entry.solver == solver && entry.class == class)
                .count(),
        })
        .collect();
    Manifest {
        generated_at,
        generator: "bt-gendata".to_string(),
        corpus_sha256: digest.sha256.clone(),
        corpus_rows: digest.rows,
        corpus_files: digest.files.clone(),
        rows_in_pair_space: built.rows_in_pair_space,
        rows_other_buffer: built.rows_other_buffer,
        cases: CasesInfo {
            path: "cases.json".to_string(),
            sha256: cases_sha256.to_string(),
            entries: built.cases.cases.len(),
            edge,
            corner,
        },
        specials: SpecialsInfo {
            path: "specials.json".to_string(),
            sha256: specials_sha256.to_string(),
            entries: built.specials.specials.len(),
            counts,
        },
    }
}
