use crate::scheme::{CORNER_BUFFER_STICKER, EDGE_BUFFER_STICKER, Orbit, letter_of, same_piece};

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct CaseId {
    pub orbit: Orbit,
    pub t1: u8,
    pub t2: u8,
}

impl CaseId {
    pub fn letters(&self) -> (char, char) {
        (letter_of(self.t1), letter_of(self.t2))
    }
}

pub fn enumerate_cases(orbit: Orbit) -> Vec<CaseId> {
    let buffer = match orbit {
        Orbit::Edge => EDGE_BUFFER_STICKER,
        Orbit::Corner => CORNER_BUFFER_STICKER,
    };
    let mut cases = Vec::new();
    for t1 in orbit.stickers() {
        if same_piece(t1, buffer) {
            continue;
        }
        for t2 in orbit.stickers() {
            if same_piece(t2, buffer) || same_piece(t2, t1) {
                continue;
            }
            cases.push(CaseId { orbit, t1, t2 });
        }
    }
    cases
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn edge_case_count_is_440() {
        assert_eq!(enumerate_cases(Orbit::Edge).len(), 440);
    }

    #[test]
    fn corner_case_count_is_378() {
        assert_eq!(enumerate_cases(Orbit::Corner).len(), 378);
    }

    #[test]
    fn cases_are_sorted_and_unique() {
        for orbit in [Orbit::Edge, Orbit::Corner] {
            let cases = enumerate_cases(orbit);
            let keys: Vec<(u8, u8)> = cases.iter().map(|c| (c.t1, c.t2)).collect();
            let mut sorted = keys.clone();
            sorted.sort_unstable();
            assert_eq!(keys, sorted);
            let unique: std::collections::HashSet<(u8, u8)> = keys.into_iter().collect();
            assert_eq!(unique.len(), cases.len());
        }
    }

    #[test]
    fn buffer_piece_and_own_piece_stickers_never_appear() {
        for (orbit, buffer) in [
            (Orbit::Edge, EDGE_BUFFER_STICKER),
            (Orbit::Corner, CORNER_BUFFER_STICKER),
        ] {
            for case in enumerate_cases(orbit) {
                assert!(orbit.stickers().contains(&case.t1));
                assert!(orbit.stickers().contains(&case.t2));
                assert!(!same_piece(case.t1, buffer));
                assert!(!same_piece(case.t2, buffer));
                assert!(!same_piece(case.t1, case.t2));
            }
        }
    }

    #[test]
    fn letters_match_scheme_lookup() {
        for orbit in [Orbit::Edge, Orbit::Corner] {
            for case in enumerate_cases(orbit) {
                assert_eq!(case.letters(), (letter_of(case.t1), letter_of(case.t2)));
            }
        }
    }

    #[test]
    fn letter_pairs_are_injective() {
        for orbit in [Orbit::Edge, Orbit::Corner] {
            let cases = enumerate_cases(orbit);
            let pairs: std::collections::HashSet<(char, char)> =
                cases.iter().map(|c| c.letters()).collect();
            assert_eq!(pairs.len(), cases.len());
        }
    }

    #[test]
    fn edge_enumeration_bounds_pinned() {
        let cases = enumerate_cases(Orbit::Edge);
        assert_eq!(
            (cases[0].t1, cases[0].t2, cases[0].letters()),
            (24, 25, ('A', 'B'))
        );
        let last = *cases.last().unwrap();
        assert_eq!((last.t1, last.t2, last.letters()), (47, 46, ('X', 'W')));
    }

    #[test]
    fn corner_enumeration_bounds_pinned() {
        let cases = enumerate_cases(Orbit::Corner);
        assert_eq!(
            (cases[0].t1, cases[0].t2, cases[0].letters()),
            (0, 1, ('A', 'B'))
        );
        let last = *cases.last().unwrap();
        assert_eq!((last.t1, last.t2, last.letters()), (23, 22, ('X', 'W')));
    }

    #[test]
    fn buffer_letters_never_appear() {
        for case in enumerate_cases(Orbit::Edge) {
            let (l1, l2) = case.letters();
            assert!(!matches!(l1, 'C' | 'I'));
            assert!(!matches!(l2, 'C' | 'I'));
        }
        for case in enumerate_cases(Orbit::Corner) {
            let (l1, l2) = case.letters();
            assert!(!matches!(l1, 'C' | 'J' | 'M'));
            assert!(!matches!(l2, 'C' | 'J' | 'M'));
        }
    }
}
