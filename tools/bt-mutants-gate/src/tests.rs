use crate::{GateError, Value, gate, merge_outcomes};

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

const UNVIABLE_PRESENT: &str = r#"{"total_mutants":2,"caught":1,"missed":0,"timeout":0,"unviable":1,"success":0,"outcomes":[
{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"},
{"scenario":{"Mutant":{"name":"src/c.rs:3:1: replace g with ()","package":"bt-x","file":"src/c.rs","function":{"function_name":"g","return_type":"","span":{"start":{"line":1,"column":1},"end":{"line":5,"column":2}}},"span":{"start":{"line":3,"column":1},"end":{"line":3,"column":2}},"replacement":"()","genre":"FnValue"}},"summary":"Unviable"}]}"#;

const INVALID_JSON: &str = "{ not json";

const NULL_FUNCTION_CAUGHT: &str = r#"{"total_mutants":1,"caught":1,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[
{"scenario":{"Mutant":{"name":"src/a.rs:7:5: delete -","package":"bt-x","file":"src/a.rs","function":null,"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"","genre":"UnaryOperator"}},"summary":"CaughtMutant"}]}"#;

const NULL_FUNCTION_MISSED: &str = r#"{"total_mutants":1,"caught":0,"missed":1,"timeout":0,"unviable":0,"success":0,"outcomes":[
{"scenario":{"Mutant":{"name":"src/a.rs:7:5: delete -","package":"bt-x","file":"src/a.rs","function":null,"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"","genre":"UnaryOperator"}},"summary":"MissedMutant"}]}"#;

const NULL_FUNCTION_MISSED_BINOP: &str = r#"{"total_mutants":1,"caught":0,"missed":1,"timeout":0,"unviable":0,"success":0,"outcomes":[
{"scenario":{"Mutant":{"name":"crates/bt-core/src/moves.rs:40:9: replace > with <= in -","package":"bt-core","file":"crates/bt-core/src/moves.rs","function":null,"span":{"start":{"line":40,"column":9},"end":{"line":40,"column":10}},"replacement":"<=","genre":"BinaryOperator"}},"summary":"MissedMutant"}]}"#;

const FAILED_BASELINE: &str = r#"{"total_mutants":1,"caught":1,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[
{"scenario":"Baseline","summary":"Failure"},
{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"}]}"#;

const BASELINE_ONLY: &str = r#"{"total_mutants":0,"caught":0,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[
{"scenario":"Baseline","summary":"Success"}]}"#;

const CAUGHT_COUNTER_LIE: &str = r#"{"total_mutants":1,"caught":0,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[
{"scenario":"Baseline","summary":"Success"},
{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"}]}"#;

const TOTAL_COUNTER_LIE: &str = r#"{"total_mutants":2,"caught":1,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[
{"scenario":"Baseline","summary":"Success"},
{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"}]}"#;

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
fn unviable_summary_is_ignored() {
    let got = gate(UNVIABLE_PRESENT, COMMENTED_ALLOWLIST);
    assert_eq!(got.unwrap(), "survivors gated: 0, all covered");
}

#[test]
fn invalid_json_is_usage_error() {
    let err = gate(INVALID_JSON, COMMENTED_ALLOWLIST).unwrap_err();
    assert!(matches!(err, GateError::Usage(_)));
}

#[test]
fn module_level_mutants_without_a_function_pass_when_caught() {
    let got = gate(NULL_FUNCTION_CAUGHT, COMMENTED_ALLOWLIST);
    assert_eq!(got.unwrap(), "survivors gated: 0, all covered");
}

#[test]
fn module_level_mutant_surviving_without_an_allowlist_row_fails() {
    let err = gate(NULL_FUNCTION_MISSED, COMMENTED_ALLOWLIST).unwrap_err();
    assert!(matches!(err, GateError::Violation(_)));
    let msg = err.to_string();
    assert!(msg.contains("not in allowlist"), "got: {msg}");
    assert!(
        msg.contains("file=src/a.rs function= genre=UnaryOperator"),
        "got: {msg}"
    );
}

#[test]
fn module_level_survivor_is_allowlistable_through_the_sentinel() {
    let allowlist = "src/a.rs\t(empty)\tUnaryOperator\t(empty)\t1\taccepted";
    let got = gate(NULL_FUNCTION_MISSED, allowlist);
    assert_eq!(got.unwrap(), "survivors gated: 1, all covered");
}

#[test]
fn module_level_binop_survivor_with_a_real_token_matches_the_sentinel_row() {
    let allowlist = "crates/bt-core/src/moves.rs\t(empty)\tBinaryOperator\t<=\t1\taccepted";
    let got = gate(NULL_FUNCTION_MISSED_BINOP, allowlist);
    assert_eq!(got.unwrap(), "survivors gated: 1, all covered");
}

#[test]
fn duplicate_sentinel_rows_still_collide() {
    let row = "src/a.rs\t(empty)\tUnaryOperator\t(empty)\t1\taccepted";
    let allowlist = format!("{row}\n{row}");
    let msg = gate(NULL_FUNCTION_MISSED, &allowlist)
        .unwrap_err()
        .to_string();
    assert!(msg.contains("allowlist row 2"), "got: {msg}");
    assert!(msg.contains("duplicate"), "got: {msg}");
}

#[test]
fn sentinel_row_without_a_surviving_outcome_is_stale() {
    let allowlist = "src/a.rs\t(empty)\tUnaryOperator\t(empty)\t1\taccepted";
    let msg = gate(CAUGHT_ONLY, allowlist).unwrap_err().to_string();
    assert!(msg.contains("stale allowlist entry"), "got: {msg}");
    assert!(msg.contains("function= "), "got: {msg}");
}

#[test]
fn an_empty_function_column_is_rejected_naming_the_sentinel() {
    let allowlist = "src/a.rs\t\tUnaryOperator\t(empty)\t1\taccepted";
    let msg = gate(NULL_FUNCTION_MISSED, allowlist)
        .unwrap_err()
        .to_string();
    assert!(msg.contains("allowlist row 1"), "got: {msg}");
    assert!(msg.contains("(empty)"), "got: {msg}");
}

#[test]
fn failed_baseline_fails_even_with_every_mutant_caught() {
    let err = gate(FAILED_BASELINE, COMMENTED_ALLOWLIST).unwrap_err();
    assert!(matches!(err, GateError::Violation(_)));
    let msg = err.to_string();
    assert!(
        msg.contains("baseline summary is 'Failure', not Success"),
        "got: {msg}"
    );
}

#[test]
fn outcomes_without_any_mutant_fail() {
    let err = gate(BASELINE_ONLY, COMMENTED_ALLOWLIST).unwrap_err();
    assert!(matches!(err, GateError::Violation(_)));
    let msg = err.to_string();
    assert!(msg.contains("no mutant outcomes"), "got: {msg}");
}

#[test]
fn caught_counter_mismatch_fails() {
    let msg = gate(CAUGHT_COUNTER_LIE, COMMENTED_ALLOWLIST)
        .unwrap_err()
        .to_string();
    assert!(msg.contains("caught counter mismatch"), "got: {msg}");
    assert!(
        msg.contains("outcomes count 1 != file counter 0"),
        "got: {msg}"
    );
}

#[test]
fn total_mutants_counter_mismatch_fails() {
    let msg = gate(TOTAL_COUNTER_LIE, COMMENTED_ALLOWLIST)
        .unwrap_err()
        .to_string();
    assert!(msg.contains("total_mutants counter mismatch"), "got: {msg}");
    assert!(
        msg.contains("outcomes count 1 != file counter 2"),
        "got: {msg}"
    );
}

const REAL_ALLOWLIST: &str = "crates/bt-scheme/src/scramble.rs\tbuild_occ\tBinaryOperator\t%->+\t1\tthe offset only feeds a modulo 3 before use so every occupancy byte is unchanged\n\
crates/bt-scheme/src/scramble.rs\trandom_occ\tBinaryOperator\t%->+\t1\tmutant twist values carry the same residues modulo 3 so every generated scramble is unchanged\n\
crates/bt-scheme/src/memo.rs\tOrbitWalk<'a>::run\tBinaryOperator\t+=->*=\t2\tthe counters only feed panic guards and a quiet guard loops forever so only a hang can reveal it\n";

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
fn real_shape_with_fixture_allowlist_passes() {
    let got = gate(REAL_SHAPE, REAL_ALLOWLIST);
    assert_eq!(got.unwrap(), "survivors gated: 4, all covered");
}

const PKG_A: &str = r#"{"cargo_mutants_version":"27.1.0","start_time":"2026-10-16T09:40:00Z","end_time":"2026-10-16T10:10:00Z","total_mutants":2,"caught":1,"missed":1,"timeout":0,"unviable":0,"success":0,"outcomes":[
{"scenario":"Baseline","summary":"Success"},
{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"pkg-a","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"},
{"scenario":{"Mutant":{"name":"src/a.rs:12:5: replace x with z in f","package":"pkg-a","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":10,"column":1},"end":{"line":14,"column":2}}},"span":{"start":{"line":12,"column":5},"end":{"line":12,"column":6}},"replacement":"z","genre":"FnValue"}},"summary":"MissedMutant"}]}"#;

const PKG_B: &str = r#"{"cargo_mutants_version":"27.1.0","start_time":"2026-10-16T10:20:00Z","end_time":"2026-10-16T10:50:00Z","total_mutants":2,"caught":0,"missed":0,"timeout":1,"unviable":1,"success":0,"outcomes":[
{"scenario":"Baseline","summary":"Success"},
{"scenario":{"Mutant":{"name":"src/b.rs:7:5: replace += with *= in g","package":"pkg-b","file":"src/b.rs","function":{"function_name":"g","return_type":"","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":7}},"replacement":"*=","genre":"BinaryOperator"}},"summary":"Timeout"},
{"scenario":{"Mutant":{"name":"src/c.rs:3:1: replace g with ()","package":"pkg-b","file":"src/c.rs","function":{"function_name":"g","return_type":"","span":{"start":{"line":1,"column":1},"end":{"line":5,"column":2}}},"span":{"start":{"line":3,"column":1},"end":{"line":3,"column":2}},"replacement":"()","genre":"FnValue"}},"summary":"Unviable"}]}"#;

const KILLED_RUN: &str = r#"{"cargo_mutants_version":"27.1.0","start_time":"2026-10-16T09:40:00Z","end_time":"","total_mutants":2,"caught":1,"missed":1,"timeout":0,"unviable":0,"success":0,"outcomes":[
{"scenario":"Baseline","summary":"Success"},
{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"pkg-a","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"},
{"scenario":{"Mutant":{"name":"src/a.rs:12:5: replace x with z in f","package":"pkg-a","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":10,"column":1},"end":{"line":14,"column":2}}},"span":{"start":{"line":12,"column":5},"end":{"line":12,"column":6}},"replacement":"z","genre":"FnValue"}},"summary":"MissedMutant"}]}"#;

const COUNTER_LIE: &str = r#"{"cargo_mutants_version":"27.1.0","start_time":"2026-10-16T09:40:00Z","end_time":"2026-10-16T10:10:00Z","total_mutants":2,"caught":0,"missed":1,"timeout":0,"unviable":0,"success":0,"outcomes":[
{"scenario":"Baseline","summary":"Success"},
{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"pkg-a","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"},
{"scenario":{"Mutant":{"name":"src/a.rs:12:5: replace x with z in f","package":"pkg-a","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":10,"column":1},"end":{"line":14,"column":2}}},"span":{"start":{"line":12,"column":5},"end":{"line":12,"column":6}},"replacement":"z","genre":"FnValue"}},"summary":"MissedMutant"}]}"#;

const MERGE_FAILED_BASELINE: &str = r#"{"cargo_mutants_version":"27.1.0","start_time":"2026-10-16T09:40:00Z","end_time":"2026-10-16T10:10:00Z","total_mutants":1,"caught":1,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[
{"scenario":"Baseline","summary":"Failure"},
{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"pkg-a","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"}]}"#;

const OTHER_VERSION: &str = r#"{"cargo_mutants_version":"27.0.0","start_time":"2026-10-16T10:20:00Z","end_time":"2026-10-16T10:50:00Z","total_mutants":1,"caught":1,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[
{"scenario":"Baseline","summary":"Success"},
{"scenario":{"Mutant":{"name":"src/b.rs:7:5: replace += with *= in g","package":"pkg-b","file":"src/b.rs","function":{"function_name":"g","return_type":"","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":7}},"replacement":"*=","genre":"BinaryOperator"}},"summary":"CaughtMutant"}]}"#;

#[test]
fn merge_sums_counters_concatenates_outcomes_and_extends_times() {
    let merged = merge_outcomes(&[PKG_A, PKG_B]).unwrap();
    let root: Value = serde_json::from_str(&merged.json).unwrap();
    assert_eq!(root["total_mutants"], 4);
    assert_eq!(root["caught"], 1);
    assert_eq!(root["missed"], 1);
    assert_eq!(root["timeout"], 1);
    assert_eq!(root["unviable"], 1);
    assert_eq!(root["outcomes"].as_array().unwrap().len(), 6);
    assert_eq!(root["start_time"], "2026-10-16T09:40:00Z");
    assert_eq!(root["end_time"], "2026-10-16T10:50:00Z");
    assert_eq!(root["cargo_mutants_version"], "27.1.0");
    let names: Vec<&str> = root["outcomes"]
        .as_array()
        .unwrap()
        .iter()
        .filter_map(|o| o["scenario"]["Mutant"]["package"].as_str())
        .collect();
    assert_eq!(names, ["pkg-a", "pkg-a", "pkg-b", "pkg-b"]);
    assert_eq!(merged.total_mutants, 4);
    assert_eq!(merged.inputs, 2);
}

#[test]
fn merged_output_feeds_the_gate() {
    let merged = merge_outcomes(&[PKG_A, PKG_B]).unwrap();
    let allowlist =
        "src/a.rs\tf\tFnValue\tz\t1\taccepted\nsrc/b.rs\tg\tBinaryOperator\t*=\t1\taccepted";
    assert_eq!(
        gate(&merged.json, allowlist).unwrap(),
        "survivors gated: 2, all covered"
    );
}

#[test]
fn merge_refuses_a_killed_run_without_end_time() {
    let err = merge_outcomes(&[PKG_A, KILLED_RUN])
        .unwrap_err()
        .to_string();
    assert!(err.contains("end_time"), "got: {err}");
    assert!(err.contains("input 2"), "got: {err}");
}

#[test]
fn merge_refuses_a_counter_mismatch_input() {
    let err = merge_outcomes(&[PKG_A, COUNTER_LIE])
        .unwrap_err()
        .to_string();
    assert!(err.contains("counter"), "got: {err}");
}

#[test]
fn merge_refuses_a_failed_baseline_input() {
    let err = merge_outcomes(&[PKG_A, MERGE_FAILED_BASELINE])
        .unwrap_err()
        .to_string();
    assert!(err.contains("baseline"), "got: {err}");
}

#[test]
fn merge_refuses_version_disagreement() {
    let err = merge_outcomes(&[PKG_A, OTHER_VERSION])
        .unwrap_err()
        .to_string();
    assert!(err.contains("version"), "got: {err}");
}

#[test]
fn merge_refuses_a_single_input() {
    let err = merge_outcomes(&[PKG_A]).unwrap_err().to_string();
    assert!(err.contains("two"), "got: {err}");
}
