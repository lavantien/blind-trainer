pub mod pairs;
pub mod scheme;

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
