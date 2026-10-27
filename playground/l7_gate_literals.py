CAUGHT_ONLY = '{"total_mutants":1,"caught":1,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":"Baseline","summary":"Success"},\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"}]}'

MISSED_ONE = '{"total_mutants":1,"caught":0,"missed":1,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":"Baseline","summary":"Success"},\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"MissedMutant"}]}'

MISSED_ONE_WRONG_COUNTER = '{"total_mutants":1,"caught":0,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":"Baseline","summary":"Success"},\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"MissedMutant"}]}'

TIMEOUT_ONE = '{"total_mutants":1,"caught":0,"missed":0,"timeout":1,"unviable":0,"success":0,"outcomes":[\n{"scenario":{"Mutant":{"name":"src/b.rs:100:5: replace += with *= in g","package":"bt-x","file":"src/b.rs","function":{"function_name":"g","return_type":"","span":{"start":{"line":90,"column":1},"end":{"line":110,"column":2}}},"span":{"start":{"line":100,"column":5},"end":{"line":100,"column":7}},"replacement":"*=","genre":"BinaryOperator"}},"summary":"Timeout"}]}'

TIMEOUT_TWICE = '{"total_mutants":2,"caught":0,"missed":0,"timeout":2,"unviable":0,"success":0,"outcomes":[\n{"scenario":{"Mutant":{"name":"src/b.rs:100:5: replace += with *= in g","package":"bt-x","file":"src/b.rs","function":{"function_name":"g","return_type":"","span":{"start":{"line":90,"column":1},"end":{"line":110,"column":2}}},"span":{"start":{"line":100,"column":5},"end":{"line":100,"column":7}},"replacement":"*=","genre":"BinaryOperator"}},"summary":"Timeout"},\n{"scenario":{"Mutant":{"name":"src/b.rs:200:9: replace += with *= in g","package":"bt-x","file":"src/b.rs","function":{"function_name":"g","return_type":"","span":{"start":{"line":90,"column":1},"end":{"line":110,"column":2}}},"span":{"start":{"line":200,"column":9},"end":{"line":200,"column":11}},"replacement":"*=","genre":"BinaryOperator"}},"summary":"Timeout"}]}'

UNKNOWN_SUMMARY = '{"total_mutants":1,"caught":0,"missed":0,"timeout":0,"unviable":1,"success":0,"outcomes":[\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"UnviableMutant"}]}'

MALFORMED_SCENARIO = '{"total_mutants":0,"caught":0,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":{"NotMutant":{}},"summary":"CaughtMutant"}]}'

ALIEN_SCENARIO_STRING = '{"total_mutants":0,"caught":0,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":"Weird","summary":"CaughtMutant"}]}'

MISSING_REPLACEMENT = '{"total_mutants":1,"caught":1,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"genre":"FnValue"}},"summary":"CaughtMutant"}]}'

MISSING_COUNTER = '{"total_mutants":1,"caught":1,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"}]}'

UNVIABLE_PRESENT = '{"total_mutants":2,"caught":1,"missed":0,"timeout":0,"unviable":1,"success":0,"outcomes":[\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"},\n{"scenario":{"Mutant":{"name":"src/c.rs:3:1: replace g with ()","package":"bt-x","file":"src/c.rs","function":{"function_name":"g","return_type":"","span":{"start":{"line":1,"column":1},"end":{"line":5,"column":2}}},"span":{"start":{"line":3,"column":1},"end":{"line":3,"column":2}},"replacement":"()","genre":"FnValue"}},"summary":"Unviable"}]}'

INVALID_JSON = "{ not json"

NULL_FUNCTION_CAUGHT = '{"total_mutants":1,"caught":1,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: delete -","package":"bt-x","file":"src/a.rs","function":null,"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"","genre":"UnaryOperator"}},"summary":"CaughtMutant"}]}'

NULL_FUNCTION_MISSED = '{"total_mutants":1,"caught":0,"missed":1,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: delete -","package":"bt-x","file":"src/a.rs","function":null,"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"","genre":"UnaryOperator"}},"summary":"MissedMutant"}]}'

NULL_FUNCTION_MISSED_BINOP = '{"total_mutants":1,"caught":0,"missed":1,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":{"Mutant":{"name":"crates/bt-core/src/moves.rs:40:9: replace > with <= in -","package":"bt-core","file":"crates/bt-core/src/moves.rs","function":null,"span":{"start":{"line":40,"column":9},"end":{"line":40,"column":10}},"replacement":"<=","genre":"BinaryOperator"}},"summary":"MissedMutant"}]}'

FAILED_BASELINE = '{"total_mutants":1,"caught":1,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":"Baseline","summary":"Failure"},\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"}]}'

BASELINE_ONLY = '{"total_mutants":0,"caught":0,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":"Baseline","summary":"Success"}]}'

CAUGHT_COUNTER_LIE = '{"total_mutants":1,"caught":0,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":"Baseline","summary":"Success"},\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"}]}'

TOTAL_COUNTER_LIE = '{"total_mutants":2,"caught":1,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":"Baseline","summary":"Success"},\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"bt-x","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"}]}'

REAL_ALLOWLIST = "crates/bt-scheme/src/scramble.rs\tbuild_occ\tBinaryOperator\t%->+\t1\tthe offset only feeds a modulo 3 before use so every occupancy byte is unchanged\n" "crates/bt-scheme/src/scramble.rs\trandom_occ\tBinaryOperator\t%->+\t1\tmutant twist values carry the same residues modulo 3 so every generated scramble is unchanged\n" "crates/bt-scheme/src/memo.rs\tOrbitWalk<'a>::run\tBinaryOperator\t+=->*=\t2\tthe counters only feed panic guards and a quiet guard loops forever so only a hang can reveal it\n"

REAL_SHAPE = """{
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
}"""

PKG_A = '{"cargo_mutants_version":"27.1.0","start_time":"2026-10-16T09:40:00Z","end_time":"2026-10-16T10:10:00Z","total_mutants":2,"caught":1,"missed":1,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":"Baseline","summary":"Success"},\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"pkg-a","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"},\n{"scenario":{"Mutant":{"name":"src/a.rs:12:5: replace x with z in f","package":"pkg-a","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":10,"column":1},"end":{"line":14,"column":2}}},"span":{"start":{"line":12,"column":5},"end":{"line":12,"column":6}},"replacement":"z","genre":"FnValue"}},"summary":"MissedMutant"}]}'

PKG_B = '{"cargo_mutants_version":"27.1.0","start_time":"2026-10-16T10:20:00Z","end_time":"2026-10-16T10:50:00Z","total_mutants":2,"caught":0,"missed":0,"timeout":1,"unviable":1,"success":0,"outcomes":[\n{"scenario":"Baseline","summary":"Success"},\n{"scenario":{"Mutant":{"name":"src/b.rs:7:5: replace += with *= in g","package":"pkg-b","file":"src/b.rs","function":{"function_name":"g","return_type":"","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":7}},"replacement":"*=","genre":"BinaryOperator"}},"summary":"Timeout"},\n{"scenario":{"Mutant":{"name":"src/c.rs:3:1: replace g with ()","package":"pkg-b","file":"src/c.rs","function":{"function_name":"g","return_type":"","span":{"start":{"line":1,"column":1},"end":{"line":5,"column":2}}},"span":{"start":{"line":3,"column":1},"end":{"line":3,"column":2}},"replacement":"()","genre":"FnValue"}},"summary":"Unviable"}]}'

KILLED_RUN = '{"cargo_mutants_version":"27.1.0","start_time":"2026-10-16T09:40:00Z","end_time":"","total_mutants":2,"caught":1,"missed":1,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":"Baseline","summary":"Success"},\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"pkg-a","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"},\n{"scenario":{"Mutant":{"name":"src/a.rs:12:5: replace x with z in f","package":"pkg-b","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":10,"column":1},"end":{"line":14,"column":2}}},"span":{"start":{"line":12,"column":5},"end":{"line":12,"column":6}},"replacement":"z","genre":"FnValue"}},"summary":"MissedMutant"}]}'

COUNTER_LIE = '{"cargo_mutants_version":"27.1.0","start_time":"2026-10-16T09:40:00Z","end_time":"2026-10-16T10:10:00Z","total_mutants":2,"caught":0,"missed":1,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":"Baseline","summary":"Success"},\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"pkg-a","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"},\n{"scenario":{"Mutant":{"name":"src/a.rs:12:5: replace x with z in f","package":"pkg-a","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":10,"column":1},"end":{"line":14,"column":2}}},"span":{"start":{"line":12,"column":5},"end":{"line":12,"column":6}},"replacement":"z","genre":"FnValue"}},"summary":"MissedMutant"}]}'

MERGE_FAILED_BASELINE = '{"cargo_mutants_version":"27.1.0","start_time":"2026-10-16T09:40:00Z","end_time":"2026-10-16T10:10:00Z","total_mutants":1,"caught":1,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":"Baseline","summary":"Failure"},\n{"scenario":{"Mutant":{"name":"src/a.rs:7:5: replace x with y in f","package":"pkg-a","file":"src/a.rs","function":{"function_name":"f","return_type":"-> u8","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":6}},"replacement":"y","genre":"FnValue"}},"summary":"CaughtMutant"}]}'

OTHER_VERSION = '{"cargo_mutants_version":"27.0.0","start_time":"2026-10-16T10:20:00Z","end_time":"2026-10-16T10:50:00Z","total_mutants":1,"caught":1,"missed":0,"timeout":0,"unviable":0,"success":0,"outcomes":[\n{"scenario":"Baseline","summary":"Success"},\n{"scenario":{"Mutant":{"name":"src/b.rs:7:5: replace += with *= in g","package":"pkg-b","file":"src/b.rs","function":{"function_name":"g","return_type":"","span":{"start":{"line":5,"column":1},"end":{"line":9,"column":2}}},"span":{"start":{"line":7,"column":5},"end":{"line":7,"column":7}},"replacement":"*=","genre":"BinaryOperator"}},"summary":"CaughtMutant"}]}'

FIXTURES = [
    ("FX_CAUGHT_ONLY", CAUGHT_ONLY),
    ("FX_MISSED_ONE", MISSED_ONE),
    ("FX_MISSED_ONE_WRONG_COUNTER", MISSED_ONE_WRONG_COUNTER),
    ("FX_TIMEOUT_ONE", TIMEOUT_ONE),
    ("FX_TIMEOUT_TWICE", TIMEOUT_TWICE),
    ("FX_UNKNOWN_SUMMARY", UNKNOWN_SUMMARY),
    ("FX_MALFORMED_SCENARIO", MALFORMED_SCENARIO),
    ("FX_ALIEN_SCENARIO_STRING", ALIEN_SCENARIO_STRING),
    ("FX_MISSING_REPLACEMENT", MISSING_REPLACEMENT),
    ("FX_MISSING_COUNTER", MISSING_COUNTER),
    ("FX_UNVIABLE_PRESENT", UNVIABLE_PRESENT),
    ("FX_INVALID_JSON", INVALID_JSON),
    ("FX_NULL_FUNCTION_CAUGHT", NULL_FUNCTION_CAUGHT),
    ("FX_NULL_FUNCTION_MISSED", NULL_FUNCTION_MISSED),
    ("FX_NULL_FUNCTION_MISSED_BINOP", NULL_FUNCTION_MISSED_BINOP),
    ("FX_FAILED_BASELINE", FAILED_BASELINE),
    ("FX_BASELINE_ONLY", BASELINE_ONLY),
    ("FX_CAUGHT_COUNTER_LIE", CAUGHT_COUNTER_LIE),
    ("FX_TOTAL_COUNTER_LIE", TOTAL_COUNTER_LIE),
    ("FX_REAL_ALLOWLIST", REAL_ALLOWLIST),
    ("FX_REAL_SHAPE", REAL_SHAPE),
    ("FX_PKG_A", PKG_A),
    ("FX_PKG_B", PKG_B),
    ("FX_KILLED_RUN", KILLED_RUN),
    ("FX_COUNTER_LIE", COUNTER_LIE),
    ("FX_MERGE_FAILED_BASELINE", MERGE_FAILED_BASELINE),
    ("FX_OTHER_VERSION", OTHER_VERSION),
]


def c_escape(text):
    out = []
    for ch in text:
        if ch == '"':
            out.append('\\"')
        elif ch == "\\":
            out.append("\\\\")
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\t":
            out.append("\\t")
        else:
            out.append(ch)
    return "".join(out)


def is_alnum(ch):
    return ch.isascii() and (ch.isalnum() or ch == "_")


def units(escaped):
    out = []
    i = 0
    while i < len(escaped):
        take = 2 if escaped[i] == "\\" and i + 1 < len(escaped) else 1
        out.append((escaped[i : i + take], take == 2))
        i += take
    return out


def emit(name, text):
    pieces = units(c_escape(text))
    lines = [f"static const char {name}[] ="]
    current = []
    length = 0
    index = 0
    while index < len(pieces):
        unit, escaped_unit = pieces[index]
        current.append((unit, escaped_unit))
        length += len(unit)
        index += 1
        if length >= 88:
            while current and length > 40 and not current[-1][1] and is_alnum(current[-1][0]):
                length -= len(current.pop()[0])
                index -= 1
            lines.append('    "' + "".join(unit for unit, _ in current) + '"')
            current = []
            length = 0
    if current or len(lines) == 1:
        lines.append('    "' + "".join(unit for unit, _ in current) + '"')
    lines[-1] += ";"
    return "\n".join(lines)


def main():
    for name, text in FIXTURES:
        print(emit(name, text))
        print()


if __name__ == "__main__":
    main()
