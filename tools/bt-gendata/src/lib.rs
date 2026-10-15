pub mod audit;
pub mod build;
pub mod hash;
pub mod model;
pub mod out;
pub mod scoring;
pub mod synth;
pub mod when;

pub use hash::sha256_hex;

pub const TOTAL_ROWS: usize = 1077;
pub const EDGE_CASES: usize = 440;
pub const CORNER_CASES: usize = 378;
pub const TOTAL_CASES: usize = 818;
pub const EDGE_COVERED: usize = 372;
pub const CORNER_COVERED: usize = 288;
pub const EDGE_GENERATED: usize = 68;
pub const CORNER_GENERATED: usize = 89;
pub const EDGE_TOTAL_COVERED: usize = 440;
pub const CORNER_TOTAL_COVERED: usize = 377;
pub const TOTAL_COVERED: usize = 817;
pub const TOTAL_SPECIALS: usize = 89;
pub const SPECIAL_COUNTS: [(&str, &str, usize); 8] = [
    ("cherry", "flips", 25),
    ("cherry", "twists", 20),
    ("cherry", "parity", 26),
    ("cherry", "ltct", 8),
    ("eggins", "flips", 0),
    ("eggins", "twists", 5),
    ("eggins", "parity", 2),
    ("eggins", "ltct", 3),
];

pub const SYNTH_ALGORITHM: &str = "first-setup-plateau-1";

pub fn crate_id() -> &'static str {
    "bt-gendata"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn crate_id_matches_package() {
        assert_eq!(crate_id(), "bt-gendata");
    }

    #[test]
    fn coverage_constants_are_self_consistent() {
        assert_eq!(EDGE_CASES + CORNER_CASES, TOTAL_CASES);
        assert_eq!(EDGE_CASES - EDGE_COVERED, 68);
        assert_eq!(CORNER_CASES - CORNER_COVERED, 90);
        assert_eq!(EDGE_COVERED + EDGE_GENERATED, EDGE_TOTAL_COVERED);
        assert_eq!(CORNER_COVERED + CORNER_GENERATED, CORNER_TOTAL_COVERED);
        assert_eq!(EDGE_TOTAL_COVERED + CORNER_TOTAL_COVERED, TOTAL_COVERED);
        assert_eq!(TOTAL_CASES - TOTAL_COVERED, 1);
        assert_eq!(
            SPECIAL_COUNTS.iter().map(|&(_, _, n)| n).sum::<usize>(),
            TOTAL_SPECIALS
        );
        assert_eq!(TOTAL_ROWS - TOTAL_SPECIALS, 988);
    }
}
