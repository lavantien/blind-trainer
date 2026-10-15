pub fn parse_args(args: &[String]) -> Result<f64, String> {
    if args.len() != 1 {
        return Err(format!("expected 1 argument <secs>, got {}", args.len()));
    }
    let raw = &args[0];
    let secs: f64 = raw
        .parse()
        .map_err(|_| format!("secs is not a number: {raw}"))?;
    if !secs.is_finite() || secs < 0.0 {
        return Err(format!("secs must be finite and non-negative: {raw}"));
    }
    Ok(secs)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn arg(raw: &str) -> Vec<String> {
        vec![raw.to_string()]
    }

    #[test]
    fn parses_positive_decimals() {
        assert_eq!(parse_args(&arg("3.5")), Ok(3.5));
        assert_eq!(parse_args(&arg("0.1")), Ok(0.1));
        assert_eq!(parse_args(&arg("0")), Ok(0.0));
    }

    #[test]
    fn rejects_non_numeric() {
        assert!(parse_args(&arg("abc")).is_err());
    }

    #[test]
    fn rejects_negative_and_non_finite() {
        assert!(parse_args(&arg("-1")).is_err());
        assert!(parse_args(&arg("inf")).is_err());
        assert!(parse_args(&arg("NaN")).is_err());
    }

    #[test]
    fn rejects_missing_and_extra_args() {
        assert!(parse_args(&[]).is_err());
        let two = vec!["1".to_string(), "2".to_string()];
        assert!(parse_args(&two).is_err());
    }
}
