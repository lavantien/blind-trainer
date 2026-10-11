pub fn crate_id() -> &'static str {
    "bt-verify"
}

fn main() {
    eprintln!("not implemented: {}", crate_id());
    std::process::exit(1);
}

#[cfg(test)]
mod tests {
    use super::crate_id;

    #[test]
    fn crate_id_matches_package() {
        assert_eq!(crate_id(), "bt-verify");
    }
}
