use crate::lexer::line_of;

pub const SUFFIXES: [&str; 14] = [
    "f32", "f64", "i8", "i16", "i32", "i64", "i128", "isize", "u8", "u16", "u32", "u64", "u128",
    "usize",
];

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Occurrence {
    pub line: usize,
    pub core: String,
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Recurring {
    pub core: String,
    pub count: usize,
    pub line: usize,
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct PolicyConfig {
    pub literal_recurrence: u64,
    pub small_literal_allow: Vec<String>,
}

pub fn policy_from_config(config: &serde_json::Value) -> Result<PolicyConfig, String> {
    let Some(section) = config.pointer("/policy") else {
        return Err("config section missing: policy".to_string());
    };
    let Some(raw) = section.pointer("/literal_recurrence") else {
        return Err("config section missing: policy.literal_recurrence".to_string());
    };
    let literal_recurrence = raw
        .as_u64()
        .ok_or_else(|| "config value malformed: policy.literal_recurrence".to_string())?;
    if literal_recurrence < 1 {
        return Err("policy.literal_recurrence must be >= 1".to_string());
    }
    let Some(list) = section.pointer("/small_literal_allow") else {
        return Err("config section missing: policy.small_literal_allow".to_string());
    };
    let entries = list
        .as_array()
        .ok_or_else(|| "config value malformed: policy.small_literal_allow".to_string())?;
    let mut small_literal_allow = Vec::new();
    for entry in entries {
        let Some(value) = entry.as_str() else {
            return Err("config value malformed: policy.small_literal_allow".to_string());
        };
        small_literal_allow.push(value.to_string());
    }
    Ok(PolicyConfig {
        literal_recurrence,
        small_literal_allow,
    })
}

pub fn scan_literals(masked_text: &str) -> Vec<Occurrence> {
    let bytes = masked_text.as_bytes();
    let len = bytes.len();
    let mut hits = Vec::new();
    let mut i = 0;
    while i < len {
        if !bytes[i].is_ascii_digit() {
            i += 1;
            continue;
        }
        if i > 0 {
            let prev = bytes[i - 1];
            let prev_dots_range = prev == b'.' && bytes.get(i - 2) == Some(&b'.');
            if (is_word(prev) || prev == b'$' || prev == b'.') && !prev_dots_range {
                i += 1;
                continue;
            }
        }
        let start = i;
        let mut j = i;
        if let Some(radix) = radix_prefix(bytes, j) {
            j += 2;
            while j < len && (radix_digit(radix, bytes[j]) || bytes[j] == b'_') {
                j += 1;
            }
        } else {
            while j < len && (bytes[j].is_ascii_digit() || bytes[j] == b'_') {
                j += 1;
            }
            if j < len && bytes[j] == b'.' && bytes.get(j + 1) != Some(&b'.') {
                j += 1;
                while j < len && (bytes[j].is_ascii_digit() || bytes[j] == b'_') {
                    j += 1;
                }
            }
            if j < len && (bytes[j] == b'e' || bytes[j] == b'E') {
                let mut k = j + 1;
                if k < len && (bytes[k] == b'+' || bytes[k] == b'-') {
                    k += 1;
                }
                if k < len && (bytes[k].is_ascii_digit() || bytes[k] == b'_') {
                    j = k;
                    while j < len && (bytes[j].is_ascii_digit() || bytes[j] == b'_') {
                        j += 1;
                    }
                }
            }
        }
        let mut s = j;
        while s < len && is_word(bytes[s]) {
            s += 1;
        }
        let end = if s > j && SUFFIXES.contains(&&masked_text[j..s]) {
            s
        } else {
            j
        };
        let boundary_ok = end >= len || {
            let after = bytes[end];
            if is_word(after) || after == b'.' {
                after == b'.' && bytes.get(end + 1) == Some(&b'.')
            } else {
                true
            }
        };
        if boundary_ok {
            let core: String = masked_text[start..j]
                .chars()
                .filter(|ch| *ch != '_')
                .collect();
            hits.push(Occurrence {
                line: line_of(masked_text, start),
                core,
            });
        }
        i = end;
    }
    hits
}

pub fn recurring_literals(masked_text: &str, threshold: u64) -> Vec<Recurring> {
    let mut counts: std::collections::BTreeMap<String, (usize, usize)> =
        std::collections::BTreeMap::new();
    for hit in scan_literals(masked_text) {
        let entry = counts.entry(hit.core).or_insert((0, hit.line));
        entry.0 += 1;
    }
    let mut recurring: Vec<Recurring> = counts
        .into_iter()
        .filter(|(_, (count, _))| *count as u64 >= threshold)
        .map(|(core, (count, line))| Recurring { core, count, line })
        .collect();
    recurring.sort_by(|a, b| (a.line, &a.core).cmp(&(b.line, &b.core)));
    recurring
}

fn is_word(byte: u8) -> bool {
    byte.is_ascii_alphanumeric() || byte == b'_'
}

fn radix_prefix(bytes: &[u8], at: usize) -> Option<u8> {
    let radix = *bytes.get(at + 1)?;
    let first = *bytes.get(at + 2)?;
    let known = matches!(radix, b'x' | b'X' | b'o' | b'O' | b'b' | b'B') && bytes[at] == b'0';
    if known && (radix_digit(radix, first) || first == b'_') {
        Some(radix)
    } else {
        None
    }
}

fn radix_digit(radix: u8, byte: u8) -> bool {
    match radix {
        b'x' | b'X' => byte.is_ascii_hexdigit(),
        b'o' | b'O' => (b'0'..=b'7').contains(&byte),
        _ => byte == b'0' || byte == b'1',
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lexer::mask_source;

    fn cores(text: &str) -> Vec<String> {
        scan_literals(text)
            .into_iter()
            .map(|hit| hit.core)
            .collect()
    }

    fn masked_cores(text: &str) -> Vec<String> {
        cores(&mask_source(text).text)
    }

    #[test]
    fn exclusive_range_yields_both_ends() {
        assert_eq!(
            cores("for i in 0..8 {}"),
            ["0".to_string(), "8".to_string()]
        );
    }

    #[test]
    fn inclusive_range_yields_both_ends() {
        assert_eq!(
            cores("for i in 0..=9 {}"),
            ["0".to_string(), "9".to_string()]
        );
    }

    #[test]
    fn trailing_dot_float_stays_one_literal() {
        assert_eq!(cores("let x = 1.;"), ["1.".to_string()]);
        assert_eq!(cores("let x = 1.5;"), ["1.5".to_string()]);
    }

    #[test]
    fn underscores_normalize_to_the_same_core() {
        assert_eq!(
            cores("let sum = 1_000 + 1000;"),
            ["1000".to_string(), "1000".to_string()]
        );
    }

    #[test]
    fn radix_prefixes_stay_in_the_core() {
        assert_eq!(cores("0x1F"), ["0x1F".to_string()]);
        assert_eq!(cores("0o17"), ["0o17".to_string()]);
        assert_eq!(cores("0b1010"), ["0b1010".to_string()]);
    }

    #[test]
    fn exponents_stay_in_the_core() {
        assert_eq!(cores("1.5e3"), ["1.5e3".to_string()]);
        assert_eq!(cores("2e10"), ["2e10".to_string()]);
        assert_eq!(cores("7E-2"), ["7E-2".to_string()]);
    }

    #[test]
    fn type_suffixes_are_stripped() {
        assert_eq!(cores("1u8"), ["1".to_string()]);
        assert_eq!(cores("2usize"), ["2".to_string()]);
        assert_eq!(cores("1.5f32"), ["1.5".to_string()]);
        assert_eq!(cores("0xFFu8"), ["0xFF".to_string()]);
    }

    #[test]
    fn member_and_tuple_access_are_excluded() {
        assert!(cores("let a = point.0;").is_empty());
        assert!(cores("let b = nested.0.1;").is_empty());
    }

    #[test]
    fn identifier_adjacent_digits_are_excluded() {
        assert!(cores("sha256 of md5 at v2").is_empty());
    }

    #[test]
    fn literals_inside_strings_and_comments_are_invisible() {
        assert!(masked_cores("let s = \"7 7 7\";").is_empty());
        let hits = masked_cores("let a = 7; // 9 9\nlet b = 7;");
        assert_eq!(hits, ["7".to_string(), "7".to_string()]);
    }

    #[test]
    fn char_literals_hide_their_digits() {
        assert!(masked_cores("let c = '\\u{1F600}';").is_empty());
    }

    #[test]
    fn occurrences_carry_one_based_lines() {
        let hits = scan_literals("let a = 1;\nlet b = 2;\nlet c = 1;");
        assert_eq!(hits[0].line, 1);
        assert_eq!(hits[1].line, 2);
        assert_eq!(hits[2].line, 3);
    }

    #[test]
    fn recurring_counts_per_core_within_one_file() {
        let text = "let a = 7;\nlet b = 7;\nlet c = 3;\nlet d = 3;\nlet e = 3;";
        let at_two = recurring_literals(text, 2);
        assert_eq!(at_two.len(), 2);
        let seven = at_two.iter().find(|hit| hit.core == "7").unwrap();
        assert_eq!((seven.count, seven.line), (2, 1));
        let three = at_two.iter().find(|hit| hit.core == "3").unwrap();
        assert_eq!((three.count, three.line), (3, 3));
        assert_eq!(recurring_literals(text, 3).len(), 1);
        assert!(recurring_literals(text, 4).is_empty());
    }

    #[test]
    fn policy_section_parses_when_present() {
        let config = serde_json::json!({
            "policy": {"literal_recurrence": 2, "small_literal_allow": ["1", "2"]}
        });
        assert_eq!(
            policy_from_config(&config).unwrap(),
            PolicyConfig {
                literal_recurrence: 2,
                small_literal_allow: vec!["1".to_string(), "2".to_string()],
            }
        );
    }

    #[test]
    fn missing_policy_section_is_an_error_naming_the_key() {
        assert_eq!(
            policy_from_config(&serde_json::json!({})).unwrap_err(),
            "config section missing: policy"
        );
    }

    #[test]
    fn missing_policy_keys_are_errors_naming_the_keys() {
        let config = serde_json::json!({"policy": {"small_literal_allow": []}});
        assert_eq!(
            policy_from_config(&config).unwrap_err(),
            "config section missing: policy.literal_recurrence"
        );
        let config = serde_json::json!({"policy": {"literal_recurrence": 2}});
        assert_eq!(
            policy_from_config(&config).unwrap_err(),
            "config section missing: policy.small_literal_allow"
        );
    }

    #[test]
    fn malformed_or_zero_recurrence_is_an_error() {
        let bad_type =
            serde_json::json!({"policy": {"literal_recurrence": "2", "small_literal_allow": []}});
        assert_eq!(
            policy_from_config(&bad_type).unwrap_err(),
            "config value malformed: policy.literal_recurrence"
        );
        let zero =
            serde_json::json!({"policy": {"literal_recurrence": 0, "small_literal_allow": []}});
        assert_eq!(
            policy_from_config(&zero).unwrap_err(),
            "policy.literal_recurrence must be >= 1"
        );
        let bad_list =
            serde_json::json!({"policy": {"literal_recurrence": 2, "small_literal_allow": [1]}});
        assert_eq!(
            policy_from_config(&bad_list).unwrap_err(),
            "config value malformed: policy.small_literal_allow"
        );
        let not_a_list =
            serde_json::json!({"policy": {"literal_recurrence": 2, "small_literal_allow": "1"}});
        assert_eq!(
            policy_from_config(&not_a_list).unwrap_err(),
            "config value malformed: policy.small_literal_allow"
        );
    }
}
