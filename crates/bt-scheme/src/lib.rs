pub mod classifier;
pub mod memo;
pub mod pairs;
pub mod scheme;

pub use classifier::{Classification, Defects, Report, Trace, classify, classify_alg, trace};
pub use memo::{
    Memo, Parity, Residue, ResidueClass, Twist, apply_memo, apply_residue, apply_sticker_cycles,
    corner_twist_state, edge_flipped, ideal_cycle, ideal_cycle_at, memo_from_array, state_array,
};
pub use pairs::{CaseId, enumerate_cases};
pub use scheme::{
    CORNER_BUFFER_STICKER, CORNER_FLOATING, EDGE_BUFFER_STICKER, EDGE_FLOATING, Orbit,
    corner_floating_heads, edge_floating_heads, head_sticker, letter_of, name_of, same_piece,
    sticker_of,
};

pub fn crate_id() -> &'static str {
    "bt-scheme"
}

#[cfg(test)]
mod tests {
    use super::crate_id;

    #[test]
    fn crate_id_matches_package() {
        assert_eq!(crate_id(), "bt-scheme");
    }
}
