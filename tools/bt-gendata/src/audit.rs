use crate::build::Built;
use bt_data::corpus::Corpus;

pub fn audit(built: &Built, corpus: &Corpus, digest_rows: usize) -> Vec<String> {
    let mut offenders = built.offenders.clone();
    if corpus.total_rows() != crate::TOTAL_ROWS {
        offenders.push(format!(
            "corpus holds {} rows, expected {}",
            corpus.total_rows(),
            crate::TOTAL_ROWS
        ));
    }
    if digest_rows != crate::TOTAL_ROWS {
        offenders.push(format!(
            "corpus digest counted {} rows, expected {}",
            digest_rows,
            crate::TOTAL_ROWS
        ));
    }
    if built.cases.cases.len() != crate::TOTAL_CASES {
        offenders.push(format!(
            "case catalog holds {} entries, expected {}",
            built.cases.cases.len(),
            crate::TOTAL_CASES
        ));
    }
    for (orbit, total, covered) in [
        ("edge", crate::EDGE_CASES, crate::EDGE_COVERED),
        ("corner", crate::CORNER_CASES, crate::CORNER_COVERED),
    ] {
        let entries: Vec<_> = built
            .cases
            .cases
            .iter()
            .filter(|case| case.orbit == orbit)
            .collect();
        if entries.len() != total {
            offenders.push(format!(
                "{orbit} case catalog holds {} entries, expected {total}",
                entries.len()
            ));
            continue;
        }
        let hit = entries.iter().filter(|case| case.covered).count();
        if hit != covered {
            offenders.push(format!(
                "{orbit} covered cases are {hit}, expected {covered}"
            ));
        }
    }
    if built.specials.specials.len() != crate::TOTAL_SPECIALS {
        offenders.push(format!(
            "specials hold {} entries, expected {}",
            built.specials.specials.len(),
            crate::TOTAL_SPECIALS
        ));
    }
    for &(solver, class, expected) in crate::SPECIAL_COUNTS.iter() {
        let found = built
            .specials
            .specials
            .iter()
            .filter(|entry| entry.solver == solver && entry.class == class)
            .count();
        if found != expected {
            offenders.push(format!(
                "{solver}/{class} holds {found} entries, expected {expected}"
            ));
        }
    }
    offenders
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::build::build;
    use crate::model::{CaseEntry, CasesFile, SpecialEntry, SpecialsFile};
    use bt_core::Alg;
    use bt_data::corpus::{Row, SheetClass, Solver};

    fn filler_row(rowid: usize) -> Row {
        Row {
            solver: Solver::Cherry,
            sheet: SheetClass::Edge,
            rowid,
            position: String::new(),
            alg: Alg::parse("").unwrap(),
            alg_text: String::new(),
            commutator: String::new(),
            recon_link: String::new(),
            youtube_link: String::new(),
        }
    }

    #[test]
    fn audit_lists_every_shortfall_on_an_empty_build() {
        let corpus = Corpus::from_rows(Vec::new());
        let built = build(&corpus);
        let offenders = audit(&built, &corpus, 3);
        assert_eq!(
            offenders,
            vec![
                "corpus holds 0 rows, expected 1077",
                "corpus digest counted 3 rows, expected 1077",
                "edge covered cases are 0, expected 372",
                "corner covered cases are 0, expected 288",
                "specials hold 0 entries, expected 89",
                "cherry/flips holds 0 entries, expected 25",
                "cherry/twists holds 0 entries, expected 20",
                "cherry/parity holds 0 entries, expected 26",
                "cherry/ltct holds 0 entries, expected 8",
                "eggins/twists holds 0 entries, expected 5",
                "eggins/parity holds 0 entries, expected 2",
                "eggins/ltct holds 0 entries, expected 3",
            ]
        );
    }

    #[test]
    fn audit_accepts_a_build_that_matches_every_constant() {
        let mut cases = Vec::new();
        for (orbit, total, covered) in
            [("edge", 440usize, 372usize), ("corner", 378usize, 288usize)]
        {
            for index in 0..total {
                let t1 = index as u8;
                let t2 = (index + 1) as u8;
                cases.push(CaseEntry {
                    orbit: orbit.to_string(),
                    t1,
                    t2,
                    t1_letter: 'A',
                    t2_letter: 'B',
                    t1_name: String::new(),
                    t2_name: String::new(),
                    covered: index < covered,
                    vendor_algs: Vec::new(),
                });
            }
        }
        let mut specials = Vec::new();
        let mut rowid = 0usize;
        for &(solver, class, count) in crate::SPECIAL_COUNTS.iter() {
            for _ in 0..count {
                rowid += 1;
                specials.push(SpecialEntry {
                    class: class.to_string(),
                    solver: solver.to_string(),
                    rowid,
                    position: String::new(),
                    alg: String::new(),
                    qtm: 0,
                    htm: 0,
                    ok: true,
                    problems: Vec::new(),
                    notes: Vec::new(),
                    commutator: String::new(),
                    recon_link: String::new(),
                    youtube_link: String::new(),
                });
            }
        }
        let built = Built {
            cases: CasesFile { cases },
            specials: SpecialsFile { specials },
            rows_in_pair_space: 0,
            rows_other_buffer: 0,
            offenders: Vec::new(),
        };
        let corpus = Corpus::from_rows((0..crate::TOTAL_ROWS).map(filler_row).collect());
        let offenders = audit(&built, &corpus, crate::TOTAL_ROWS);
        assert!(offenders.is_empty(), "{}", offenders.join("\n"));
    }
}
