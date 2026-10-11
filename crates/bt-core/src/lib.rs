pub fn crate_id() -> &'static str {
    "bt-core"
}

#[cfg(test)]
mod tests {
    use super::crate_id;

    #[test]
    fn crate_id_matches_package() {
        assert_eq!(crate_id(), "bt-core");
    }
}
