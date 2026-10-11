use std::path::PathBuf;

pub fn ref_ground() -> PathBuf {
    let raw = std::env::var("BT_REF_GROUND")
        .unwrap_or_else(|_| panic!("BT_REF_GROUND is unset: run tests through make"));
    let path = PathBuf::from(raw);
    if path.is_absolute() {
        path
    } else {
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("../..")
            .join(path)
    }
}
