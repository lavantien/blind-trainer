use crate::Finding;

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct SlocConfig {
    pub max_file_lines: u64,
}

pub fn sloc_from_config(config: &serde_json::Value) -> Result<SlocConfig, String> {
    let Some(raw) = config.pointer("/sloc/max_file_lines") else {
        return Err("config section missing: sloc.max_file_lines".to_string());
    };
    let max_file_lines = raw
        .as_u64()
        .ok_or_else(|| "config value malformed: sloc.max_file_lines".to_string())?;
    if max_file_lines < 1 {
        return Err("sloc.max_file_lines must be >= 1".to_string());
    }
    Ok(SlocConfig { max_file_lines })
}

pub fn cap_finding(text: &str, rel: &str, cap: u64) -> Option<Finding> {
    let mut non_blank = 0u64;
    let mut crossing = 0usize;
    for (index, line) in text.lines().enumerate() {
        if line.trim().is_empty() {
            continue;
        }
        non_blank += 1;
        if non_blank == cap + 1 {
            crossing = index + 1;
        }
    }
    if non_blank <= cap {
        return None;
    }
    Some(Finding {
        path: rel.to_string(),
        line: crossing,
        kind: "sloc",
        detail: format!("{non_blank} non-blank lines over cap {cap}"),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn config_section_parses_when_present() {
        let config = serde_json::json!({"sloc": {"max_file_lines": 1000}});
        assert_eq!(
            sloc_from_config(&config).unwrap(),
            SlocConfig {
                max_file_lines: 1000
            }
        );
    }

    #[test]
    fn missing_or_malformed_sections_are_errors_naming_the_key() {
        assert_eq!(
            sloc_from_config(&serde_json::json!({})).unwrap_err(),
            "config section missing: sloc.max_file_lines"
        );
        assert_eq!(
            sloc_from_config(&serde_json::json!({"sloc": {}})).unwrap_err(),
            "config section missing: sloc.max_file_lines"
        );
        assert_eq!(
            sloc_from_config(&serde_json::json!({"sloc": {"max_file_lines": "1000"}})).unwrap_err(),
            "config value malformed: sloc.max_file_lines"
        );
        assert_eq!(
            sloc_from_config(&serde_json::json!({"sloc": {"max_file_lines": 0}})).unwrap_err(),
            "sloc.max_file_lines must be >= 1"
        );
    }

    #[test]
    fn blank_lines_do_not_count_and_the_cap_is_inclusive() {
        let at_cap = "fn a() {}\n\nfn b() {}\n\n\nlet x = 1;\nlet y = 2;\nlet z = 3;\n";
        assert_eq!(non_blank_count(at_cap), 5);
        assert!(cap_finding(at_cap, "src/a.rs", 5).is_none());
    }

    #[test]
    fn one_line_over_cap_fires_with_the_crossing_line() {
        let over = "fn a() {}\nfn b() {}\nfn c() {}\nfn d() {}\nfn e() {}\nfn f() {}\n";
        let finding = cap_finding(over, "src/big.rs", 5).expect("over cap fires");
        assert_eq!(finding.kind, "sloc");
        assert_eq!(finding.path, "src/big.rs");
        assert_eq!(finding.line, 6);
        assert_eq!(finding.detail, "6 non-blank lines over cap 5");
    }

    #[test]
    fn the_crossing_line_skips_blank_lines() {
        let over = "a\n\nb\nc\nd\ne\n\nf\n";
        let finding = cap_finding(over, "src/x.rs", 5).expect("over cap fires");
        assert_eq!(finding.line, 8);
        assert_eq!(finding.detail, "6 non-blank lines over cap 5");
    }

    #[test]
    fn an_empty_file_stays_silent() {
        assert!(cap_finding("", "src/e.rs", 1).is_none());
        assert!(cap_finding("\n\n  \n", "src/e.rs", 1).is_none());
    }

    fn non_blank_count(text: &str) -> u64 {
        text.lines().filter(|line| !line.trim().is_empty()).count() as u64
    }
}
