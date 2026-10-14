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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::model::CorpusFileEntry;
    use crate::synth::{SynthLimits, synth_case};
    use bt_core::Alg;
    use bt_data::corpus::{Row, Solver};

    fn limits() -> SynthLimits {
        SynthLimits {
            max_setup: 2,
            max_insert: 4,
            node_budget: 100_000_000,
        }
    }

    fn row(solver: Solver, sheet: SheetClass, rowid: usize, position: &str, alg: &Alg) -> Row {
        Row {
            solver,
            sheet,
            rowid,
            position: position.to_string(),
            alg_text: alg.format(),
            alg: alg.clone(),
            commutator: String::new(),
            recon_link: String::new(),
            youtube_link: String::new(),
        }
    }

    fn position_of(orbit: Orbit, t1: u8, t2: u8) -> String {
        let buffer = match orbit {
            Orbit::Edge => EDGE_BUFFER_STICKER,
            Orbit::Corner => CORNER_BUFFER_STICKER,
        };
        format!("{}-{}-{}", name_of(buffer), name_of(t1), name_of(t2))
    }

    #[test]
    fn build_catalogs_the_whole_pair_space_from_an_empty_corpus() {
        let corpus = Corpus::from_rows(Vec::new());
        let built = build(&corpus);
        assert_eq!(built.cases.cases.len(), crate::TOTAL_CASES);
        assert!(
            built
                .cases
                .cases
                .iter()
                .all(|case| !case.covered && case.vendor_algs.is_empty())
        );
        assert!(built.specials.specials.is_empty());
        assert_eq!(built.rows_in_pair_space, 0);
        assert_eq!(built.rows_other_buffer, 0);
        assert!(built.offenders.is_empty(), "{}", built.offenders.join("\n"));
        let first = &built.cases.cases[0];
        assert_eq!(
            (
                first.orbit.as_str(),
                first.t1,
                first.t2,
                first.t1_letter,
                first.t2_letter
            ),
            ("edge", 24, 25, 'A', 'B')
        );
        assert_eq!(
            (first.t1_name.as_str(), first.t2_name.as_str()),
            ("UB", "UR")
        );
        let boundary = &built.cases.cases[440];
        assert_eq!(
            (boundary.orbit.as_str(), boundary.t1, boundary.t2),
            ("corner", 0, 1)
        );
        let last = built.cases.cases.last().unwrap();
        assert_eq!(
            (
                last.orbit.as_str(),
                last.t1,
                last.t2,
                last.t1_letter,
                last.t2_letter
            ),
            ("corner", 23, 22, 'X', 'W')
        );
    }

    #[test]
    fn build_attaches_a_verified_edge_three_style_to_its_case() {
        let case = enumerate_cases(Orbit::Edge)[0];
        let hit =
            synth_case(Orbit::Edge, case.t1, case.t2, &limits()).expect("edge AB synthesizes");
        let corpus = Corpus::from_rows(vec![row(
            Solver::Cherry,
            SheetClass::Edge,
            1,
            &position_of(Orbit::Edge, case.t1, case.t2),
            &hit.alg,
        )]);
        let built = build(&corpus);
        assert!(built.offenders.is_empty(), "{}", built.offenders.join("\n"));
        assert_eq!(built.rows_in_pair_space, 1);
        assert_eq!(built.rows_other_buffer, 0);
        let slot = built
            .cases
            .cases
            .iter()
            .position(|entry| entry.t1 == case.t1 && entry.t2 == case.t2)
            .unwrap();
        let entry = &built.cases.cases[slot];
        assert!(entry.covered);
        assert_eq!(entry.vendor_algs.len(), 1);
        let alg = &entry.vendor_algs[0];
        assert_eq!(alg.solver, "cherry");
        assert_eq!(alg.rowid, 1);
        assert_eq!(alg.position, position_of(Orbit::Edge, case.t1, case.t2));
        assert_eq!(alg.alg, hit.alg.format());
        assert_eq!(alg.qtm, hit.alg.qtm());
        assert_eq!(alg.htm, hit.alg.htm());
        assert!(alg.ok);
        assert!(alg.problems.is_empty());
        assert_eq!(
            built
                .cases
                .cases
                .iter()
                .filter(|entry| entry.covered)
                .count(),
            1
        );
    }

    #[test]
    fn build_attaches_a_verified_corner_three_style_to_its_case() {
        let case = enumerate_cases(Orbit::Corner)[0];
        let hit =
            synth_case(Orbit::Corner, case.t1, case.t2, &limits()).expect("corner AB synthesizes");
        let corpus = Corpus::from_rows(vec![row(
            Solver::Eggins,
            SheetClass::Corner,
            1,
            &position_of(Orbit::Corner, case.t1, case.t2),
            &hit.alg,
        )]);
        let built = build(&corpus);
        assert!(built.offenders.is_empty(), "{}", built.offenders.join("\n"));
        assert_eq!(built.rows_in_pair_space, 1);
        let slot = built
            .cases
            .cases
            .iter()
            .position(|entry| entry.t1 == case.t1 && entry.t2 == case.t2)
            .unwrap();
        let entry = &built.cases.cases[slot];
        assert_eq!(entry.orbit, "corner");
        assert!(entry.covered);
        assert_eq!(entry.vendor_algs[0].solver, "eggins");
        assert_eq!(
            built
                .cases
                .cases
                .iter()
                .filter(|entry| entry.covered)
                .count(),
            1
        );
    }

    #[test]
    fn build_routes_special_sheets_with_their_verdicts() {
        let alg = Alg::parse("").unwrap();
        let corpus = Corpus::from_rows(vec![row(
            Solver::Cherry,
            SheetClass::Flips,
            1,
            "UF & zz",
            &alg,
        )]);
        let built = build(&corpus);
        assert_eq!(built.specials.specials.len(), 1);
        let special = &built.specials.specials[0];
        assert_eq!(
            (
                special.class.as_str(),
                special.solver.as_str(),
                special.rowid
            ),
            ("flips", "cherry", 1)
        );
        assert_eq!(special.alg, "");
        assert!(!special.ok);
        assert!(
            special
                .problems
                .iter()
                .any(|problem| problem.contains("bad position"))
        );
        assert_eq!(built.rows_in_pair_space, 0);
        assert_eq!(built.offenders.len(), 1);
        assert!(built.offenders[0].starts_with("cherry/flips/1 UF & zz: "));
        assert!(built.cases.cases.iter().all(|entry| !entry.covered));
    }

    #[test]
    fn build_flags_non_three_style_and_wrong_orbit_rows() {
        let plain = Alg::parse("R").unwrap();
        let corner_case = enumerate_cases(Orbit::Corner)[0];
        let corner_hit = synth_case(Orbit::Corner, corner_case.t1, corner_case.t2, &limits())
            .expect("corner AB synthesizes");
        let corpus = Corpus::from_rows(vec![
            row(Solver::Cherry, SheetClass::Edge, 1, "UF-UB-UR", &plain),
            row(
                Solver::Eggins,
                SheetClass::Edge,
                2,
                "UF-UB-UR",
                &corner_hit.alg,
            ),
        ]);
        let built = build(&corpus);
        assert_eq!(built.offenders.len(), 4, "{}", built.offenders.join("\n"));
        assert!(
            built.offenders.iter().any(|offender| {
                offender.starts_with("cherry/edge/1 is not a three style case: ")
            })
        );
        assert!(
            built
                .offenders
                .iter()
                .any(|offender| offender.starts_with("eggins/edge/2 walks the wrong orbit Corner"))
        );
        assert!(
            built
                .offenders
                .iter()
                .any(|offender| offender.starts_with("cherry/edge/1 UF-UB-UR: "))
        );
        assert!(
            built
                .offenders
                .iter()
                .any(|offender| offender.starts_with("eggins/edge/2 UF-UB-UR: "))
        );
        assert_eq!(built.rows_in_pair_space, 0);
        assert_eq!(built.rows_other_buffer, 0);
        assert!(built.cases.cases.iter().all(|entry| !entry.covered));
    }

    #[test]
    fn build_counts_other_buffer_walks_without_covering() {
        let case = enumerate_cases(Orbit::Edge)[0];
        let hit =
            synth_case(Orbit::Edge, case.t1, case.t2, &limits()).expect("edge AB synthesizes");
        let conjugated =
            Alg::parse(&format!("U {} U'", hit.alg.format())).expect("conjugated alg parses");
        let position = classify_alg(&conjugated)
            .position()
            .expect("conjugated alg stays a three style");
        assert_eq!(position, "UB-UR-UL");
        let corpus = Corpus::from_rows(vec![row(
            Solver::Cherry,
            SheetClass::Edge,
            1,
            &position,
            &conjugated,
        )]);
        let built = build(&corpus);
        assert!(built.offenders.is_empty(), "{}", built.offenders.join("\n"));
        assert_eq!(built.rows_other_buffer, 1);
        assert_eq!(built.rows_in_pair_space, 0);
        assert!(built.cases.cases.iter().all(|entry| !entry.covered));
    }

    #[test]
    fn manifest_counts_orbits_and_special_classes() {
        let case = enumerate_cases(Orbit::Edge)[0];
        let hit =
            synth_case(Orbit::Edge, case.t1, case.t2, &limits()).expect("edge AB synthesizes");
        let plain = Alg::parse("").unwrap();
        let corpus = Corpus::from_rows(vec![
            row(
                Solver::Cherry,
                SheetClass::Edge,
                1,
                &position_of(Orbit::Edge, case.t1, case.t2),
                &hit.alg,
            ),
            row(Solver::Cherry, SheetClass::Flips, 1, "UF & zz", &plain),
            row(Solver::Eggins, SheetClass::Twists, 1, "zz & yy", &plain),
        ]);
        let built = build(&corpus);
        let digest = CorpusDigest {
            sha256: "d".repeat(64),
            rows: 3,
            files: vec![CorpusFileEntry {
                solver: "cherry".to_string(),
                sheet: "edge".to_string(),
                rows: 1,
                sha256: "e".repeat(64),
            }],
        };
        let manifest = manifest(
            &built,
            &digest,
            &"c".repeat(64),
            &"p".repeat(64),
            "stamp".to_string(),
        );
        assert_eq!(manifest.generated_at, "stamp");
        assert_eq!(manifest.generator, "bt-gendata");
        assert_eq!(manifest.corpus_sha256, "d".repeat(64));
        assert_eq!(manifest.corpus_rows, 3);
        assert_eq!(manifest.corpus_files, digest.files);
        assert_eq!(manifest.rows_in_pair_space, 1);
        assert_eq!(manifest.rows_other_buffer, 0);
        assert_eq!(manifest.cases.path, "cases.json");
        assert_eq!(manifest.cases.sha256, "c".repeat(64));
        assert_eq!(manifest.cases.entries, crate::TOTAL_CASES);
        assert_eq!(
            manifest.cases.edge,
            OrbitCoverage {
                total: 440,
                covered: 1,
                uncovered: 439
            }
        );
        assert_eq!(
            manifest.cases.corner,
            OrbitCoverage {
                total: 378,
                covered: 0,
                uncovered: 378
            }
        );
        assert_eq!(manifest.specials.path, "specials.json");
        assert_eq!(manifest.specials.sha256, "p".repeat(64));
        assert_eq!(manifest.specials.entries, 2);
        let counts = &manifest.specials.counts;
        assert_eq!(counts.len(), crate::SPECIAL_COUNTS.len());
        for (count, &(solver, class, _)) in counts.iter().zip(crate::SPECIAL_COUNTS.iter()) {
            assert_eq!(
                (count.solver.as_str(), count.class.as_str()),
                (solver, class)
            );
        }
        assert_eq!(
            (
                counts[0].solver.as_str(),
                counts[0].class.as_str(),
                counts[0].rows
            ),
            ("cherry", "flips", 1)
        );
        assert_eq!(
            (
                counts[4].solver.as_str(),
                counts[4].class.as_str(),
                counts[4].rows
            ),
            ("eggins", "flips", 0)
        );
        assert_eq!(
            (
                counts[5].solver.as_str(),
                counts[5].class.as_str(),
                counts[5].rows
            ),
            ("eggins", "twists", 1)
        );
    }
}
