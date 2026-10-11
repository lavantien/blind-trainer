pub mod dist;
pub mod features;
pub mod ga;
pub mod hash;
pub mod inventory;

pub fn crate_id() -> &'static str {
    "bt-ga"
}

#[cfg(test)]
mod tests {
    use super::crate_id;

    #[test]
    fn crate_id_matches_package() {
        assert_eq!(crate_id(), "bt-ga");
    }
}
