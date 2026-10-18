use std::fmt;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum FailureClass {
    Parse,
    Apply,
    Tier2,
}

impl fmt::Display for FailureClass {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let name = match self {
            FailureClass::Parse => "parse",
            FailureClass::Apply => "apply",
            FailureClass::Tier2 => "tier2",
        };
        write!(f, "{name}")
    }
}

pub fn classify_failure(tier1_err: Option<&str>, tier2_failed: bool) -> Option<FailureClass> {
    match tier1_err {
        Some(text) => {
            let parse = text.starts_with("unknown token") || text.contains("parse failed");
            Some(if parse {
                FailureClass::Parse
            } else {
                FailureClass::Apply
            })
        }
        None if tier2_failed => Some(FailureClass::Tier2),
        None => None,
    }
}

pub const EXPECTED_FAILURES: &[(&str, &str, &str, &str, FailureClass, &str)] = &[
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.119",
        FailureClass::Apply,
        "unexecutable slash sequence: hand traced, cs.122 differs by one twist sign and executes",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.075",
        FailureClass::Tier2,
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.078",
        FailureClass::Tier2,
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.084",
        FailureClass::Tier2,
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.089",
        FailureClass::Tier2,
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.097",
        FailureClass::Tier2,
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.109",
        FailureClass::Tier2,
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.118",
        FailureClass::Tier2,
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
    (
        "sq1",
        "lbl",
        "cubeshape",
        "cs.129",
        FailureClass::Tier2,
        "flipped layer convention in the source data, recorded defect, row kept verbatim",
    ),
    (
        "sq1",
        "lbl",
        "eo",
        "eo.01",
        FailureClass::Apply,
        "unexecutable slash sequence: hand traced, no state maps to solved through the stored alg",
    ),
    (
        "sq1",
        "lbl",
        "eo",
        "eo.04",
        FailureClass::Apply,
        "unexecutable slash sequence: hand traced, no state maps to solved through the stored alg",
    ),
    (
        "sq1",
        "lbl",
        "eo",
        "eo.06",
        FailureClass::Apply,
        "unexecutable slash sequence: hand traced, no state maps to solved through the stored alg",
    ),
    (
        "sq1",
        "lbl",
        "ep",
        "ep.40",
        FailureClass::Apply,
        "unexecutable slash sequence: hand traced, no state maps to solved through the stored alg",
    ),
    (
        "sq1",
        "lbl",
        "ep",
        "ep.66",
        FailureClass::Apply,
        "unexecutable slash sequence: hand traced, no state maps to solved through the stored alg",
    ),
    (
        "sq1",
        "lbl",
        "ep",
        "ep.80",
        FailureClass::Apply,
        "unexecutable slash sequence: hand traced, no state maps to solved through the stored alg",
    ),
    (
        "sq1",
        "lbl",
        "ep",
        "ep.05",
        FailureClass::Apply,
        "AUF-dependent executability: the stripped prefix plausibly restores legality, in-engine conversion rests on an unpinned sign convention",
    ),
    (
        "sq1",
        "lbl",
        "ep",
        "ep.18",
        FailureClass::Apply,
        "AUF-dependent executability: the stripped prefix plausibly restores legality, in-engine conversion rests on an unpinned sign convention",
    ),
    (
        "sq1",
        "lbl",
        "ep",
        "ep.50",
        FailureClass::Apply,
        "AUF-dependent executability: the stripped prefix plausibly restores legality, in-engine conversion rests on an unpinned sign convention",
    ),
    (
        "megaminx",
        "4lll",
        "4lll",
        "4lll.20",
        FailureClass::Tier2,
        "EP row displaces corners beyond every center-pinned y-spin frame, prime-slip suspect family of the R2 U2-prime cubeskills rows, no single-token suffix flip reconciles it (pinned probe), recorded source defect",
    ),
    (
        "megaminx",
        "4lll",
        "4lll",
        "4lll.21",
        FailureClass::Tier2,
        "EP row displaces corners beyond every center-pinned y-spin frame, prime-slip suspect family of the R2 U2-prime cubeskills rows, no single-token suffix flip reconciles it (pinned probe), recorded source defect",
    ),
    (
        "megaminx",
        "4lll",
        "4lll",
        "4lll.22",
        FailureClass::Tier2,
        "EP row displaces corners beyond every center-pinned y-spin frame, prime-slip suspect family of the R2 U2-prime cubeskills rows, no single-token suffix flip reconciles it (pinned probe), recorded source defect",
    ),
    (
        "megaminx",
        "4lll",
        "4lll",
        "4lll.24",
        FailureClass::Tier2,
        "EP row displaces corners beyond every center-pinned y-spin frame, prime-slip suspect family of the R2 U2-prime cubeskills rows, no single-token suffix flip reconciles it (pinned probe), recorded source defect",
    ),
    (
        "pyraminx",
        "l4e",
        "l4e",
        "l4e.06",
        FailureClass::Tier2,
        "disturbs five or more edge facelets outside last four edges scope once the U layer frame is composed out, 27 of 35 rows fit the exact l4e shape, hand diff on l4e.11 vertex bands, recorded source defect",
    ),
    (
        "pyraminx",
        "l4e",
        "l4e",
        "l4e.11",
        FailureClass::Tier2,
        "disturbs five or more edge facelets outside last four edges scope once the U layer frame is composed out, 27 of 35 rows fit the exact l4e shape, hand diff on l4e.11 vertex bands, recorded source defect",
    ),
    (
        "pyraminx",
        "l4e",
        "l4e",
        "l4e.21",
        FailureClass::Tier2,
        "disturbs five or more edge facelets outside last four edges scope once the U layer frame is composed out, 27 of 35 rows fit the exact l4e shape, hand diff on l4e.11 vertex bands, recorded source defect",
    ),
    (
        "pyraminx",
        "l4e",
        "l4e",
        "l4e.22",
        FailureClass::Tier2,
        "disturbs five or more edge facelets outside last four edges scope once the U layer frame is composed out, 27 of 35 rows fit the exact l4e shape, hand diff on l4e.11 vertex bands, recorded source defect",
    ),
    (
        "pyraminx",
        "l4e",
        "l4e",
        "l4e.24",
        FailureClass::Tier2,
        "disturbs five or more edge facelets outside last four edges scope once the U layer frame is composed out, 27 of 35 rows fit the exact l4e shape, hand diff on l4e.11 vertex bands, recorded source defect",
    ),
    (
        "pyraminx",
        "l4e",
        "l4e",
        "l4e.25",
        FailureClass::Tier2,
        "disturbs five or more edge facelets outside last four edges scope once the U layer frame is composed out, 27 of 35 rows fit the exact l4e shape, hand diff on l4e.11 vertex bands, recorded source defect",
    ),
    (
        "pyraminx",
        "l4e",
        "l4e",
        "l4e.28",
        FailureClass::Tier2,
        "disturbs five or more edge facelets outside last four edges scope once the U layer frame is composed out, 27 of 35 rows fit the exact l4e shape, hand diff on l4e.11 vertex bands, recorded source defect",
    ),
    (
        "pyraminx",
        "l4e",
        "l4e",
        "l4e.35",
        FailureClass::Tier2,
        "disturbs five or more edge facelets outside last four edges scope once the U layer frame is composed out, 27 of 35 rows fit the exact l4e shape, hand diff on l4e.11 vertex bands, recorded source defect",
    ),
    (
        "bigcube",
        "yau",
        "parity",
        "parity.oll4",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "yau",
        "parity",
        "parity.oll4flip",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "yau",
        "parity",
        "parity.pll4",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "yau",
        "parity",
        "parity.pll4adj",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "yau",
        "parity",
        "parity.wing5",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "reduction",
        "parity",
        "l2e.01",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "reduction",
        "parity",
        "l2e.02",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "reduction",
        "parity",
        "l2e.03",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "reduction",
        "parity",
        "l2e.04",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "reduction",
        "parity",
        "l2e.05",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "reduction",
        "parity",
        "l2e.06",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "reduction",
        "parity",
        "l2e.07",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "reduction",
        "parity",
        "l2e.08",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "reduction",
        "parity",
        "l2e.09",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "reduction",
        "parity",
        "l2e.10",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "reduction",
        "parity",
        "l2e.11",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "bigcube",
        "reduction",
        "parity",
        "l2e.12",
        FailureClass::Tier2,
        "engine semantics proven canonical by the wide-turn identity pins (Rw equals R 2R and kin), the stored alg solves no reduction state under canonical semantics in either direction, no global conjugation of six reconciles it (pinned probe), recorded source defect",
    ),
    (
        "3bld",
        "m2",
        "concept",
        "m2.18",
        FailureClass::Tier2,
        "pack written in the speedsolving wiki M E convention (M like L-prime, E like D-prime) while bt-core M follows L and E follows D, single-direction M and E tokens act inverted, M2 tokens are convention-invariant which is why only 4 of 24 rows fail, m2.18 under the wiki reading delivers DB into the DF buffer (convention proof), recorded convention defect",
    ),
    (
        "3bld",
        "m2",
        "concept",
        "m2.20",
        FailureClass::Tier2,
        "pack written in the speedsolving wiki M E convention (M like L-prime, E like D-prime) while bt-core M follows L and E follows D, single-direction M and E tokens act inverted, M2 tokens are convention-invariant which is why only 4 of 24 rows fail, m2.18 under the wiki reading delivers DB into the DF buffer (convention proof), recorded convention defect",
    ),
    (
        "3bld",
        "m2",
        "concept",
        "m2.22",
        FailureClass::Tier2,
        "pack written in the speedsolving wiki M E convention (M like L-prime, E like D-prime) while bt-core M follows L and E follows D, single-direction M and E tokens act inverted, M2 tokens are convention-invariant which is why only 4 of 24 rows fail, m2.18 under the wiki reading delivers DB into the DF buffer (convention proof), recorded convention defect",
    ),
    (
        "3bld",
        "m2",
        "concept",
        "m2.19",
        FailureClass::Tier2,
        "fails under both the wiki and bt-core M E readings, label defect on top of the M E convention divergence, recorded source defect",
    ),
];

pub fn expected_failure(
    event: &str,
    method: &str,
    stage: &str,
    case_key: &str,
) -> Option<&'static str> {
    EXPECTED_FAILURES
        .iter()
        .find(|row| row.0 == event && row.1 == method && row.2 == stage && row.3 == case_key)
        .map(|row| row.5)
}

pub fn expected_failure_class(
    event: &str,
    method: &str,
    stage: &str,
    case_key: &str,
) -> Option<FailureClass> {
    EXPECTED_FAILURES
        .iter()
        .find(|row| row.0 == event && row.1 == method && row.2 == stage && row.3 == case_key)
        .map(|row| row.4)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn classify_failure_splits_parse_apply_and_tier2() {
        assert_eq!(
            classify_failure(Some("unknown token: q"), false),
            Some(FailureClass::Parse)
        );
        assert_eq!(
            classify_failure(Some("n=4: tier 1 parse failed: bad"), false),
            Some(FailureClass::Parse)
        );
        assert_eq!(
            classify_failure(Some("illegal move: /"), false),
            Some(FailureClass::Apply)
        );
        assert_eq!(
            classify_failure(Some("start state round trip misses solved"), false),
            Some(FailureClass::Apply)
        );
        assert_eq!(classify_failure(None, true), Some(FailureClass::Tier2));
        assert_eq!(classify_failure(None, false), None);
    }

    #[test]
    fn failure_class_display_names_are_stable() {
        assert_eq!(FailureClass::Parse.to_string(), "parse");
        assert_eq!(FailureClass::Apply.to_string(), "apply");
        assert_eq!(FailureClass::Tier2.to_string(), "tier2");
    }

    #[test]
    fn every_ruling_row_pairs_a_class_with_its_reason() {
        for row in EXPECTED_FAILURES {
            assert_eq!(expected_failure(row.0, row.1, row.2, row.3), Some(row.5));
            assert_eq!(
                expected_failure_class(row.0, row.1, row.2, row.3),
                Some(row.4)
            );
        }
        assert_eq!(
            expected_failure_class("sq1", "lbl", "cubeshape", "cs.001"),
            None
        );
    }

    #[test]
    fn representative_rulings_pin_their_recorded_classes() {
        assert_eq!(
            expected_failure_class("sq1", "lbl", "cubeshape", "cs.119"),
            Some(FailureClass::Apply)
        );
        assert_eq!(
            expected_failure_class("sq1", "lbl", "cubeshape", "cs.075"),
            Some(FailureClass::Tier2)
        );
        assert_eq!(
            expected_failure_class("3bld", "m2", "concept", "m2.18"),
            Some(FailureClass::Tier2)
        );
    }
}
