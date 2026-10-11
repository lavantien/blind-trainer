pub mod corpus;
pub mod paths;
pub mod srs;
pub mod tsv;
pub mod verify;

pub fn crate_id() -> &'static str {
    "bt-data"
}

#[cfg(test)]
mod tests {
    use super::crate_id;

    #[test]
    fn crate_id_matches_package() {
        assert_eq!(crate_id(), "bt-data");
    }
}
