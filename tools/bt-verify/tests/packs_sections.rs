mod common;

use bt_verify::packs::{PACK_EXPECTED_ROWS, pinned_vs_registry};
use common::ref_ground;

fn packs() -> std::path::PathBuf {
    ref_ground().join("packs")
}

#[test]
fn pinned_table_covers_the_registry_exactly() {
    assert_eq!(PACK_EXPECTED_ROWS.len(), 21);
    assert_eq!(
        PACK_EXPECTED_ROWS.iter().map(|pin| pin.rows).sum::<usize>(),
        1115
    );
    let drift = pinned_vs_registry(&packs());
    assert!(drift.is_empty(), "{}", drift.join("\n"));
}
