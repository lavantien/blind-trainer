pub mod commutator;
pub mod corpus;
pub mod db;
pub mod fsrs;
pub mod import;
pub mod paths;
pub mod repo_algs;
pub mod repo_seeds;
pub mod repo_settings;
pub mod repo_solves;
pub mod repo_srs;
pub mod seedpack;
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
