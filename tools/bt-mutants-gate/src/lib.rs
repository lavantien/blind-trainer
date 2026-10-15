use serde_json::Value;
use std::collections::BTreeMap;
use std::fmt;

#[derive(Clone, Debug, Eq, Ord, PartialEq, PartialOrd)]
pub struct GateKey {
    pub file: String,
    pub function: String,
    pub genre: String,
    pub replacement: String,
}

impl fmt::Display for GateKey {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "file={} function={} genre={} replacement={}",
            self.file, self.function, self.genre, self.replacement
        )
    }
}

#[derive(Debug)]
pub enum GateError {
    Usage(String),
    Violation(String),
}

impl fmt::Display for GateError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            GateError::Usage(message) | GateError::Violation(message) => write!(f, "{message}"),
        }
    }
}

pub fn gate(outcomes_json: &str, allowlist_text: &str) -> Result<String, GateError> {
    let survivors = survivor_multiset(outcomes_json)?;
    let allowlist = parse_allowlist(allowlist_text)?;
    let mut problems = Vec::new();
    for (key, seen) in &survivors {
        match allowlist.get(key) {
            None => problems.push(format!("surviving mutant not in allowlist: {key}")),
            Some(allowed) if allowed != seen => problems.push(format!(
                "count mismatch: {key} outcomes={seen} allowlist={allowed}"
            )),
            _ => {}
        }
    }
    for key in allowlist.keys() {
        if !survivors.contains_key(key) {
            problems.push(format!(
                "stale allowlist entry with no surviving outcome: {key}"
            ));
        }
    }
    if !problems.is_empty() {
        return Err(GateError::Violation(problems.join("\n")));
    }
    let gated: u64 = survivors.values().sum();
    Ok(format!("survivors gated: {gated}, all covered"))
}

fn survivor_multiset(json: &str) -> Result<BTreeMap<GateKey, u64>, GateError> {
    let root: Value = serde_json::from_str(json)
        .map_err(|e| GateError::Usage(format!("invalid outcomes JSON: {e}")))?;
    let outcomes = root
        .get("outcomes")
        .and_then(Value::as_array)
        .ok_or_else(|| malformed_file("missing outcomes array"))?;
    let missed_counter = uint_counter(&root, "missed")?;
    let timeout_counter = uint_counter(&root, "timeout")?;
    let mut survivors = BTreeMap::new();
    let mut missed_seen = 0u64;
    let mut timeout_seen = 0u64;
    for outcome in outcomes {
        let scenario = outcome
            .get("scenario")
            .ok_or_else(|| malformed_outcome("missing scenario"))?;
        if scenario.as_str() == Some("Baseline") {
            continue;
        }
        if let Some(label) = scenario.as_str() {
            return Err(malformed_outcome(format!(
                "scenario string '{label}' is not Baseline"
            )));
        }
        let summary = outcome
            .get("summary")
            .and_then(Value::as_str)
            .ok_or_else(|| malformed_outcome("missing summary string"))?;
        let mutant = scenario
            .get("Mutant")
            .and_then(Value::as_object)
            .ok_or_else(|| malformed_outcome("scenario object without Mutant"))?;
        let key = parse_key(mutant)?;
        match summary {
            "CaughtMutant" => {}
            "MissedMutant" => {
                *survivors.entry(key).or_insert(0) += 1;
                missed_seen += 1;
            }
            "Timeout" => {
                *survivors.entry(key).or_insert(0) += 1;
                timeout_seen += 1;
            }
            other => {
                return Err(GateError::Violation(format!(
                    "unknown summary '{other}' for {key} (allowed: CaughtMutant, MissedMutant, Timeout)"
                )));
            }
        }
    }
    if missed_seen != missed_counter {
        return Err(GateError::Violation(format!(
            "missed counter mismatch: outcomes count {missed_seen} != file counter {missed_counter}"
        )));
    }
    if timeout_seen != timeout_counter {
        return Err(GateError::Violation(format!(
            "timeout counter mismatch: outcomes count {timeout_seen} != file counter {timeout_counter}"
        )));
    }
    Ok(survivors)
}

fn parse_key(mutant: &serde_json::Map<String, Value>) -> Result<GateKey, GateError> {
    let file = string_field(mutant, "file")?;
    let function = mutant
        .get("function")
        .and_then(Value::as_object)
        .ok_or_else(|| malformed_outcome("mutant without function object"))?;
    let function_name = string_field(function, "function_name")?;
    let genre = string_field(mutant, "genre")?;
    let replacement = string_field(mutant, "replacement")?;
    Ok(GateKey {
        file,
        function: function_name,
        genre,
        replacement,
    })
}

fn string_field(map: &serde_json::Map<String, Value>, name: &str) -> Result<String, GateError> {
    map.get(name)
        .and_then(Value::as_str)
        .map(str::to_owned)
        .ok_or_else(|| malformed_outcome(format!("missing string field '{name}'")))
}

fn uint_counter(root: &Value, name: &str) -> Result<u64, GateError> {
    root.get(name)
        .and_then(Value::as_u64)
        .ok_or_else(|| malformed_file(format!("missing integer counter '{name}'")))
}

fn parse_allowlist(text: &str) -> Result<BTreeMap<GateKey, u64>, GateError> {
    let mut entries = BTreeMap::new();
    for (idx, line) in text.lines().enumerate() {
        let row = idx + 1;
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        let cols: Vec<&str> = line.split('\t').collect();
        if cols.len() != 6 {
            return Err(row_error(
                row,
                format!("expected 6 tab-separated columns, found {}", cols.len()),
            ));
        }
        if cols[0].is_empty() || cols[1].is_empty() || cols[2].is_empty() {
            return Err(row_error(row, "file, function and genre must be non-empty"));
        }
        let replacement = if cols[3].contains("->") {
            cols[3]
                .rsplit("->")
                .next()
                .filter(|token| !token.is_empty())
                .ok_or_else(|| {
                    row_error(row, "replacement must have a non-empty token after '->'")
                })?
        } else if cols[3].is_empty() {
            return Err(row_error(row, "replacement must be non-empty"));
        } else {
            cols[3]
        };
        let count: u64 = cols[4]
            .trim()
            .parse()
            .map_err(|_| row_error(row, format!("count '{}' is not an integer", cols[4])))?;
        if count < 1 {
            return Err(row_error(row, "count must be >= 1"));
        }
        if cols[5].is_empty() {
            return Err(row_error(row, "justification must be non-empty"));
        }
        let key = GateKey {
            file: cols[0].to_owned(),
            function: cols[1].to_owned(),
            genre: cols[2].to_owned(),
            replacement: replacement.to_owned(),
        };
        if entries.contains_key(&key) {
            return Err(row_error(row, format!("duplicate entry for {key}")));
        }
        entries.insert(key, count);
    }
    Ok(entries)
}

fn row_error(row: usize, detail: impl Into<String>) -> GateError {
    GateError::Violation(format!("allowlist row {row}: {}", detail.into()))
}

fn malformed_outcome(detail: impl Into<String>) -> GateError {
    GateError::Violation(format!("malformed outcome: {}", detail.into()))
}

fn malformed_file(detail: impl Into<String>) -> GateError {
    GateError::Violation(format!("malformed outcomes file: {}", detail.into()))
}

#[cfg(test)]
mod tests {
    use crate::{GateError, gate};

    const CAUGHT_ONLY: &str = r#"{"total_mutants":1,"caught":1,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[
    {"scenario":"Baseline","summary":"Success"},
    {"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"}]}"#;

    const MISSED_ONE: &str = r#"{"total_mutants":1,"caught":0,"missed":1,"timeout":0,"unviable":0,"success":0,"outcomes":[
    {"scenario":"Baseline","summary":"Success"},
    {"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"MissedMutant"}]}"#;

    const MISSED_ONE_WRONG_COUNTER: &str = r#"{"total_mutants":1,"caught":0,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[
    {"scenario":"Baseline","summary":"Success"},
    {"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"MissedMutant"}]}"#;

    const TIMEOUT_ONE: &str = r#"{"total_mutants":1,"caught":0,"missed":0,"timeout":1,"unviable":0,"success":0,"outcomes":[
    {"scenario":{"Mutant":{"name":"src/b.rs:100:5: replace += with *= in g","package":"bt-x","file":"src/b.rs","function":{"function_name":"g","return_type":"","span":{"start":{"line":90,"column":1},"end":{"line":110,"column":2}}},"span":{"start":{"line":100,"column":5},"end":{"line":100,"column":7}},"replacement":"*=","genre":"BinaryOperator"}},"summary":"Timeout"}]}"#;

    const TIMEOUT_TWICE: &str = r#"{"total_mutants":2,"caught":0,"missed":0,"timeout":2,"unviable":0,"success":0,"outcomes":[
    {"scenario":{"Mutant":{"name":"src/b.rs:100:5: replace += with *= in g","package":"bt-x","file":"src/b.rs","function":{"function_name":"g","return_type":"","span":{"start":{"line":90,"column":1},"end":{"line":110,"column":2}}},"span":{"start":{"line":100,"column":5},"end":{"line":100,"column":7}},"replacement":"*=","genre":"BinaryOperator"}},"summary":"Timeout"},
    {"scenario":{"Mutant":{"name":"src/b.rs:200:9: replace += with *= in g","package":"bt-x","file":"src/b.rs","function":{"function_name":"g","return_type":"","span":{"start":{"line":90,"column":1},"end":{"line":110,"column":2}}},"span":{"start":{"line":200,"column":9},"end":{"line":200,"column":11}},"replacement":"*=","genre":"BinaryOperator"}},"summary":"Timeout"}]}"#;

    const UNKNOWN_SUMMARY: &str = r#"{"total_mutants":1,"caught":0,"missed":0,"timeout":0,"unviable":1,"success":0,"outcomes":[
    {"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"UnviableMutant"}]}"#;

    const MALFORMED_SCENARIO: &str = r#"{"total_mutants":0,"caught":0,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[
    {"scenario":{"NotMutant":{}},"summary":"CaughtMutant"}]}"#;

    const ALIEN_SCENARIO_STRING: &str = r#"{"total_mutants":0,"caught":0,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[
    {"scenario":"Weird","summary":"CaughtMutant"}]}"#;

    const MISSING_REPLACEMENT: &str = r#"{"total_mutants":1,"caught":1,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[
    {"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"genre":"FnValue"}},"summary":"CaughtMutant"}]}"#;

    const MISSING_COUNTER: &str = r#"{"total_mutants":1,"caught":1,"timeout":0,"unviable":0,"success":0,"outcomes":[
    {"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"}]}"#;

    const INVALID_JSON: &str = "{ not json";

    const COMMENTED_ALLOWLIST: &str =
        "# columns: file\tfunction\tgenre\treplacement\tcount\tjustification\n";

    #[test]
    fn baseline_and_caught_only_pass_with_empty_allowlist() {
        let got = gate(CAUGHT_ONLY, COMMENTED_ALLOWLIST);
        assert_eq!(got.unwrap(), "survivors gated: 0, all covered");
    }

    #[test]
    fn missed_survivor_without_allowlist_row_fails() {
        let err = gate(MISSED_ONE, COMMENTED_ALLOWLIST).unwrap_err();
        assert!(matches!(err, GateError::Violation(_)));
        let msg = err.to_string();
        assert!(msg.contains("not in allowlist"), "got: {msg}");
        assert!(
            msg.contains("file=src/a.rs function=f genre=FnValue replacement=y"),
            "got: {msg}"
        );
    }

    #[test]
    fn timeout_survivor_covered_passes() {
        let allowlist = "src/b.rs\tg\tBinaryOperator\t*=\t1\taccepted";
        let got = gate(TIMEOUT_ONE, allowlist);
        assert_eq!(got.unwrap(), "survivors gated: 1, all covered");
    }

    #[test]
    fn repeated_key_matches_count_two_row() {
        let allowlist = "src/b.rs\tg\tBinaryOperator\t*=\t2\taccepted";
        let got = gate(TIMEOUT_TWICE, allowlist);
        assert_eq!(got.unwrap(), "survivors gated: 2, all covered");
    }

    #[test]
    fn count_mismatch_fails() {
        let allowlist = "src/b.rs\tg\tBinaryOperator\t*=\t2\taccepted";
        let msg = gate(TIMEOUT_ONE, allowlist).unwrap_err().to_string();
        assert!(msg.contains("count mismatch"), "got: {msg}");
        assert!(msg.contains("outcomes=1 allowlist=2"), "got: {msg}");
    }

    #[test]
    fn stale_allowlist_entry_fails() {
        let allowlist = "src/zz.rs\th\tFnValue\t()\t1\taccepted";
        let msg = gate(CAUGHT_ONLY, allowlist).unwrap_err().to_string();
        assert!(msg.contains("stale allowlist entry"), "got: {msg}");
        assert!(msg.contains("file=src/zz.rs function=h"), "got: {msg}");
    }

    #[test]
    fn duplicate_allowlist_rows_fail() {
        let row = "src/b.rs\tg\tBinaryOperator\t*=\t1\taccepted";
        let allowlist = format!("{row}\n{row}");
        let msg = gate(TIMEOUT_ONE, &allowlist).unwrap_err().to_string();
        assert!(msg.contains("allowlist row 2"), "got: {msg}");
        assert!(msg.contains("duplicate"), "got: {msg}");
    }

    #[test]
    fn unknown_summary_fails() {
        let err = gate(UNKNOWN_SUMMARY, COMMENTED_ALLOWLIST).unwrap_err();
        assert!(matches!(err, GateError::Violation(_)));
        let msg = err.to_string();
        assert!(
            msg.contains("unknown summary 'UnviableMutant'"),
            "got: {msg}"
        );
    }

    #[test]
    fn counter_mismatch_fails() {
        let msg = gate(MISSED_ONE_WRONG_COUNTER, COMMENTED_ALLOWLIST)
            .unwrap_err()
            .to_string();
        assert!(msg.contains("missed counter mismatch"), "got: {msg}");
        assert!(msg.contains("1") && msg.contains("0"), "got: {msg}");
    }

    #[test]
    fn malformed_allowlist_row_fails() {
        let allowlist = "src/b.rs\tg\tBinaryOperator\t*=\taccepted";
        let msg = gate(TIMEOUT_ONE, allowlist).unwrap_err().to_string();
        assert!(msg.contains("allowlist row 1"), "got: {msg}");
        assert!(msg.contains("6"), "got: {msg}");
    }

    #[test]
    fn non_numeric_count_fails() {
        let allowlist = "src/b.rs\tg\tBinaryOperator\t*=\ttwo\taccepted";
        let msg = gate(TIMEOUT_ONE, allowlist).unwrap_err().to_string();
        assert!(msg.contains("allowlist row 1"), "got: {msg}");
        assert!(msg.contains("count"), "got: {msg}");
    }

    #[test]
    fn zero_count_fails() {
        let allowlist = "src/b.rs\tg\tBinaryOperator\t*=\t0\taccepted";
        let msg = gate(TIMEOUT_ONE, allowlist).unwrap_err().to_string();
        assert!(msg.contains("allowlist row 1"), "got: {msg}");
        assert!(msg.contains("count"), "got: {msg}");
    }

    #[test]
    fn empty_justification_fails() {
        let allowlist = "src/b.rs\tg\tBinaryOperator\t*=\t1\t";
        let msg = gate(TIMEOUT_ONE, allowlist).unwrap_err().to_string();
        assert!(msg.contains("allowlist row 1"), "got: {msg}");
        assert!(msg.contains("justification"), "got: {msg}");
    }

    #[test]
    fn arrow_notation_replacement_matches_bare_token() {
        let allowlist = "src/a.rs\tf\tFnValue\tx->y\t1\taccepted";
        let got = gate(MISSED_ONE, allowlist);
        assert_eq!(got.unwrap(), "survivors gated: 1, all covered");
    }

    #[test]
    fn arrow_notation_with_empty_suffix_fails() {
        let allowlist = "src/a.rs\tf\tFnValue\tx->\t1\taccepted";
        let msg = gate(MISSED_ONE, allowlist).unwrap_err().to_string();
        assert!(msg.contains("allowlist row 1"), "got: {msg}");
        assert!(msg.contains("replacement"), "got: {msg}");
    }

    #[test]
    fn malformed_scenario_object_fails() {
        let msg = gate(MALFORMED_SCENARIO, COMMENTED_ALLOWLIST)
            .unwrap_err()
            .to_string();
        assert!(msg.contains("malformed outcome"), "got: {msg}");
        assert!(msg.contains("Mutant"), "got: {msg}");
    }

    #[test]
    fn alien_scenario_string_fails() {
        let msg = gate(ALIEN_SCENARIO_STRING, COMMENTED_ALLOWLIST)
            .unwrap_err()
            .to_string();
        assert!(msg.contains("malformed outcome"), "got: {msg}");
        assert!(msg.contains("Weird"), "got: {msg}");
    }

    #[test]
    fn missing_replacement_field_fails() {
        let msg = gate(MISSING_REPLACEMENT, COMMENTED_ALLOWLIST)
            .unwrap_err()
            .to_string();
        assert!(msg.contains("malformed outcome"), "got: {msg}");
        assert!(msg.contains("replacement"), "got: {msg}");
    }

    #[test]
    fn missing_counter_fails() {
        let msg = gate(MISSING_COUNTER, COMMENTED_ALLOWLIST)
            .unwrap_err()
            .to_string();
        assert!(msg.contains("counter 'missed'"), "got: {msg}");
    }

    #[test]
    fn invalid_json_is_usage_error() {
        let err = gate(INVALID_JSON, COMMENTED_ALLOWLIST).unwrap_err();
        assert!(matches!(err, GateError::Usage(_)));
    }

    const REAL_SHAPE: &str = r#"{
  "cargo_mutants_version": "27.1.0",
  "start_time": "2026-10-14T10:00:00Z",
  "end_time": "2026-10-14T10:30:00Z",
  "total_mutants": 6,
  "caught": 2,
  "missed": 2,
  "timeout": 2,
  "unviable": 0,
  "success": 0,
  "outcomes": [
    {
      "scenario": "Baseline",
      "summary": "Success",
      "log_path": "log/baseline.log",
      "diff_path": null,
      "phase_results": [
        {
          "phase": "Build",
          "duration": 3.0,
          "process_status": "Success",
          "argv": ["cargo", "test", "--no-run", "--verbose", "--package=bt-scheme@0.0.0"]
        }
      ]
    },
    {
      "scenario": {
        "Mutant": {
          "name": "crates/bt-scheme/src/classifier.rs:151:57: replace && with || in Report::position",
          "package": "bt-scheme",
          "file": "crates/bt-scheme/src/classifier.rs",
          "function": {
            "function_name": "Report::position",
            "return_type": "-> Option<String>",
            "span": { "start": { "line": 127, "column": 5 }, "end": { "line": 181, "column": 6 } }
          },
          "span": { "start": { "line": 151, "column": 57 }, "end": { "line": 151, "column": 59 } },
          "replacement": "||",
          "genre": "BinaryOperator"
        }
      },
      "summary": "CaughtMutant",
      "log_path": "log/crates__bt-scheme__src__classifier.rs_line_151_col_57.log",
      "diff_path": "diff/crates__bt-scheme__src__classifier.rs_line_151_col_57.diff",
      "phase_results": [
        {
          "phase": "Build",
          "duration": 2.1,
          "process_status": "Success",
          "argv": ["cargo", "test", "--no-run", "--verbose", "--package=bt-scheme@0.0.0"]
        }
      ]
    },
    {
      "scenario": {
        "Mutant": {
          "name": "crates/bt-scheme/src/memo.rs:347:42: replace || with && in OrbitWalk<'a>::main_scan",
          "package": "bt-scheme",
          "file": "crates/bt-scheme/src/memo.rs",
          "function": {
            "function_name": "OrbitWalk<'a>::main_scan",
            "return_type": "-> Option<u8>",
            "span": { "start": { "line": 345, "column": 5 }, "end": { "line": 357, "column": 6 } }
          },
          "span": { "start": { "line": 347, "column": 42 }, "end": { "line": 347, "column": 44 } },
          "replacement": "&&",
          "genre": "BinaryOperator"
        }
      },
      "summary": "CaughtMutant",
      "log_path": "log/crates__bt-scheme__src__memo.rs_line_347_col_42.log",
      "diff_path": "diff/crates__bt-scheme__src__memo.rs_line_347_col_42.diff",
      "phase_results": [
        {
          "phase": "Build",
          "duration": 2.2,
          "process_status": "Success",
          "argv": ["cargo", "test", "--no-run", "--verbose", "--package=bt-scheme@0.0.0"]
        }
      ]
    },
    {
      "scenario": {
        "Mutant": {
          "name": "crates/bt-scheme/src/scramble.rs:74:39: replace % with + in build_occ",
          "package": "bt-scheme",
          "file": "crates/bt-scheme/src/scramble.rs",
          "function": {
            "function_name": "build_occ",
            "return_type": "",
            "span": { "start": { "line": 59, "column": 1 }, "end": { "line": 79, "column": 2 } }
          },
          "span": { "start": { "line": 74, "column": 39 }, "end": { "line": 74, "column": 41 } },
          "replacement": "+",
          "genre": "BinaryOperator"
        }
      },
      "summary": "MissedMutant",
      "log_path": "log/crates__bt-scheme__src__scramble.rs_line_74_col_39.log",
      "diff_path": "diff/crates__bt-scheme__src__scramble.rs_line_74_col_39.diff",
      "phase_results": [
        {
          "phase": "Build",
          "duration": 2.3,
          "process_status": "Success",
          "argv": ["cargo", "test", "--no-run", "--verbose", "--package=bt-scheme@0.0.0"]
        }
      ]
    },
    {
      "scenario": {
        "Mutant": {
          "name": "crates/bt-scheme/src/scramble.rs:91:36: replace % with + in random_occ",
          "package": "bt-scheme",
          "file": "crates/bt-scheme/src/scramble.rs",
          "function": {
            "function_name": "random_occ",
            "return_type": "-> [u8; 54]",
            "span": { "start": { "line": 81, "column": 1 }, "end": { "line": 113, "column": 2 } }
          },
          "span": { "start": { "line": 91, "column": 36 }, "end": { "line": 91, "column": 38 } },
          "replacement": "+",
          "genre": "BinaryOperator"
        }
      },
      "summary": "MissedMutant",
      "log_path": "log/crates__bt-scheme__src__scramble.rs_line_91_col_36.log",
      "diff_path": "diff/crates__bt-scheme__src__scramble.rs_line_91_col_36.diff",
      "phase_results": [
        {
          "phase": "Build",
          "duration": 2.4,
          "process_status": "Success",
          "argv": ["cargo", "test", "--no-run", "--verbose", "--package=bt-scheme@0.0.0"]
        }
      ]
    },
    {
      "scenario": {
        "Mutant": {
          "name": "crates/bt-scheme/src/memo.rs:384:19: replace += with *= in OrbitWalk<'a>::run",
          "package": "bt-scheme",
          "file": "crates/bt-scheme/src/memo.rs",
          "function": {
            "function_name": "OrbitWalk<'a>::run",
            "return_type": "-> WalkOutcome",
            "span": { "start": { "line": 379, "column": 5 }, "end": { "line": 449, "column": 6 } }
          },
          "span": { "start": { "line": 384, "column": 19 }, "end": { "line": 384, "column": 21 } },
          "replacement": "*=",
          "genre": "BinaryOperator"
        }
      },
      "summary": "Timeout",
      "log_path": "log/crates__bt-scheme__src__memo.rs_line_384_col_19.log",
      "diff_path": "diff/crates__bt-scheme__src__memo.rs_line_384_col_19.diff",
      "phase_results": [
        {
          "phase": "Build",
          "duration": 2.5,
          "process_status": "Success",
          "argv": ["cargo", "test", "--no-run", "--verbose", "--package=bt-scheme@0.0.0"]
        }
      ]
    },
    {
      "scenario": {
        "Mutant": {
          "name": "crates/bt-scheme/src/memo.rs:423:24: replace += with *= in OrbitWalk<'a>::run",
          "package": "bt-scheme",
          "file": "crates/bt-scheme/src/memo.rs",
          "function": {
            "function_name": "OrbitWalk<'a>::run",
            "return_type": "-> WalkOutcome",
            "span": { "start": { "line": 379, "column": 5 }, "end": { "line": 449, "column": 6 } }
          },
          "span": { "start": { "line": 423, "column": 24 }, "end": { "line": 423, "column": 26 } },
          "replacement": "*=",
          "genre": "BinaryOperator"
        }
      },
      "summary": "Timeout",
      "log_path": "log/crates__bt-scheme__src__memo.rs_line_423_col_24.log",
      "diff_path": "diff/crates__bt-scheme__src__memo.rs_line_423_col_24.diff",
      "phase_results": [
        {
          "phase": "Build",
          "duration": 2.6,
          "process_status": "Success",
          "argv": ["cargo", "test", "--no-run", "--verbose", "--package=bt-scheme@0.0.0"]
        }
      ]
    }
  ]
}"#;

    #[test]
    fn real_shape_with_seeded_allowlist_passes() {
        let got = gate(REAL_SHAPE, include_str!("../allowlist.tsv"));
        assert_eq!(got.unwrap(), "survivors gated: 4, all covered");
    }
}
