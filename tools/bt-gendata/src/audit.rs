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
