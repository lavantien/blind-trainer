#include "mutants-gate/lib.h"

#include <stdlib.h>
#include <string.h>

#include "common/bt_harness.h"
#include "common/json.h"

static const char FX_CAUGHT_ONLY[] =
    "{\"total_mutants\":1,\"caught\":1,\"missed\":0,\"timeout\":0,\"unviable\":0,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":\"Baseline\",\"summary\":\"Success\"},\n{\"scenario\":{\""
    "Mutant\":{\"name\":\"src/a.rs:7:5: replace x with y in f\",\"package\":\"bt-x\",\"file\""
    ":\"src/a.rs\",\"function\":{\"function_name\":\"f\",\"return_type\":\"-> u8\",\"span\":{"
    "\"start\":{\"line\":5,\"column\":1},\"end\":{\"line\":9,\"column\":2}}},\"span\":{\""
    "start\":{\"line\":7,\"column\":5},\"end\":{\"line\":7,\"column\":6}},\"replacement\":\""
    "y\",\"genre\":\"FnValue\"}},\"summary\":\"CaughtMutant\"}]}";

static const char FX_MISSED_ONE[] =
    "{\"total_mutants\":1,\"caught\":0,\"missed\":1,\"timeout\":0,\"unviable\":0,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":\"Baseline\",\"summary\":\"Success\"},\n{\"scenario\":{\""
    "Mutant\":{\"name\":\"src/a.rs:7:5: replace x with y in f\",\"package\":\"bt-x\",\"file\""
    ":\"src/a.rs\",\"function\":{\"function_name\":\"f\",\"return_type\":\"-> u8\",\"span\":{"
    "\"start\":{\"line\":5,\"column\":1},\"end\":{\"line\":9,\"column\":2}}},\"span\":{\""
    "start\":{\"line\":7,\"column\":5},\"end\":{\"line\":7,\"column\":6}},\"replacement\":\""
    "y\",\"genre\":\"FnValue\"}},\"summary\":\"MissedMutant\"}]}";

static const char FX_MISSED_ONE_WRONG_COUNTER[] =
    "{\"total_mutants\":1,\"caught\":0,\"missed\":0,\"timeout\":0,\"unviable\":0,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":\"Baseline\",\"summary\":\"Success\"},\n{\"scenario\":{\""
    "Mutant\":{\"name\":\"src/a.rs:7:5: replace x with y in f\",\"package\":\"bt-x\",\"file\""
    ":\"src/a.rs\",\"function\":{\"function_name\":\"f\",\"return_type\":\"-> u8\",\"span\":{"
    "\"start\":{\"line\":5,\"column\":1},\"end\":{\"line\":9,\"column\":2}}},\"span\":{\""
    "start\":{\"line\":7,\"column\":5},\"end\":{\"line\":7,\"column\":6}},\"replacement\":\""
    "y\",\"genre\":\"FnValue\"}},\"summary\":\"MissedMutant\"}]}";

static const char FX_TIMEOUT_ONE[] =
    "{\"total_mutants\":1,\"caught\":0,\"missed\":0,\"timeout\":1,\"unviable\":0,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":{\"Mutant\":{\"name\":\"src/b.rs:100:5: replace += with "
    "*= in g\",\"package\":\"bt-x\",\"file\":\"src/b.rs\",\"function\":{\"function_name\":\""
    "g\",\"return_type\":\"\",\"span\":{\"start\":{\"line\":90,\"column\":1},\"end\":{\"line\""
    ":110,\"column\":2}}},\"span\":{\"start\":{\"line\":100,\"column\":5},\"end\":{\"line\":"
    "100,\"column\":7}},\"replacement\":\"*=\",\"genre\":\"BinaryOperator\"}},\"summary\":\""
    "Timeout\"}]}";

static const char FX_TIMEOUT_TWICE[] =
    "{\"total_mutants\":2,\"caught\":0,\"missed\":0,\"timeout\":2,\"unviable\":0,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":{\"Mutant\":{\"name\":\"src/b.rs:100:5: replace += with "
    "*= in g\",\"package\":\"bt-x\",\"file\":\"src/b.rs\",\"function\":{\"function_name\":\""
    "g\",\"return_type\":\"\",\"span\":{\"start\":{\"line\":90,\"column\":1},\"end\":{\"line\""
    ":110,\"column\":2}}},\"span\":{\"start\":{\"line\":100,\"column\":5},\"end\":{\"line\":"
    "100,\"column\":7}},\"replacement\":\"*=\",\"genre\":\"BinaryOperator\"}},\"summary\":\""
    "Timeout\"},\n{\"scenario\":{\"Mutant\":{\"name\":\"src/b.rs:200:9: replace += with *= "
    "in g\",\"package\":\"bt-x\",\"file\":\"src/b.rs\",\"function\":{\"function_name\":\"g\","
    "\"return_type\":\"\",\"span\":{\"start\":{\"line\":90,\"column\":1},\"end\":{\"line\":"
    "110,\"column\":2}}},\"span\":{\"start\":{\"line\":200,\"column\":9},\"end\":{\"line\":"
    "200,\"column\":11}},\"replacement\":\"*=\",\"genre\":\"BinaryOperator\"}},\"summary\":\""
    "Timeout\"}]}";

static const char FX_UNKNOWN_SUMMARY[] =
    "{\"total_mutants\":1,\"caught\":0,\"missed\":0,\"timeout\":0,\"unviable\":1,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":{\"Mutant\":{\"name\":\"src/a.rs:7:5: replace x with y "
    "in f\",\"package\":\"bt-x\",\"file\":\"src/a.rs\",\"function\":{\"function_name\":\"f\","
    "\"return_type\":\"-> u8\",\"span\":{\"start\":{\"line\":5,\"column\":1},\"end\":{\"line\""
    ":9,\"column\":2}}},\"span\":{\"start\":{\"line\":7,\"column\":5},\"end\":{\"line\":7,\""
    "column\":6}},\"replacement\":\"y\",\"genre\":\"FnValue\"}},\"summary\":\"UnviableMutant\""
    "}]}";

static const char FX_MALFORMED_SCENARIO[] =
    "{\"total_mutants\":0,\"caught\":0,\"missed\":0,\"timeout\":0,\"unviable\":0,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":{\"NotMutant\":{}},\"summary\":\"CaughtMutant\"}]}";

static const char FX_ALIEN_SCENARIO_STRING[] =
    "{\"total_mutants\":0,\"caught\":0,\"missed\":0,\"timeout\":0,\"unviable\":0,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":\"Weird\",\"summary\":\"CaughtMutant\"}]}";

static const char FX_MISSING_REPLACEMENT[] =
    "{\"total_mutants\":1,\"caught\":1,\"missed\":0,\"timeout\":0,\"unviable\":0,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":{\"Mutant\":{\"name\":\"src/a.rs:7:5: replace x with y "
    "in f\",\"package\":\"bt-x\",\"file\":\"src/a.rs\",\"function\":{\"function_name\":\"f\","
    "\"return_type\":\"-> u8\",\"span\":{\"start\":{\"line\":5,\"column\":1},\"end\":{\"line\""
    ":9,\"column\":2}}},\"span\":{\"start\":{\"line\":7,\"column\":5},\"end\":{\"line\":7,\""
    "column\":6}},\"genre\":\"FnValue\"}},\"summary\":\"CaughtMutant\"}]}";

static const char FX_MISSING_COUNTER[] =
    "{\"total_mutants\":1,\"caught\":1,\"timeout\":0,\"unviable\":0,\"success\":0,\"outcomes\""
    ":[\n{\"scenario\":{\"Mutant\":{\"name\":\"src/a.rs:7:5: replace x with y in f\",\""
    "package\":\"bt-x\",\"file\":\"src/a.rs\",\"function\":{\"function_name\":\"f\",\""
    "return_type\":\"-> u8\",\"span\":{\"start\":{\"line\":5,\"column\":1},\"end\":{\"line\":"
    "9,\"column\":2}}},\"span\":{\"start\":{\"line\":7,\"column\":5},\"end\":{\"line\":7,\""
    "column\":6}},\"replacement\":\"y\",\"genre\":\"FnValue\"}},\"summary\":\"CaughtMutant\"}"
    "]}";

static const char FX_UNVIABLE_PRESENT[] =
    "{\"total_mutants\":2,\"caught\":1,\"missed\":0,\"timeout\":0,\"unviable\":1,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":{\"Mutant\":{\"name\":\"src/a.rs:7:5: replace x with y "
    "in f\",\"package\":\"bt-x\",\"file\":\"src/a.rs\",\"function\":{\"function_name\":\"f\","
    "\"return_type\":\"-> u8\",\"span\":{\"start\":{\"line\":5,\"column\":1},\"end\":{\"line\""
    ":9,\"column\":2}}},\"span\":{\"start\":{\"line\":7,\"column\":5},\"end\":{\"line\":7,\""
    "column\":6}},\"replacement\":\"y\",\"genre\":\"FnValue\"}},\"summary\":\"CaughtMutant\"}"
    ",\n{\"scenario\":{\"Mutant\":{\"name\":\"src/c.rs:3:1: replace g with ()\",\"package\":\""
    "bt-x\",\"file\":\"src/c.rs\",\"function\":{\"function_name\":\"g\",\"return_type\":\"\","
    "\"span\":{\"start\":{\"line\":1,\"column\":1},\"end\":{\"line\":5,\"column\":2}}},\""
    "span\":{\"start\":{\"line\":3,\"column\":1},\"end\":{\"line\":3,\"column\":2}},\""
    "replacement\":\"()\",\"genre\":\"FnValue\"}},\"summary\":\"Unviable\"}]}";

static const char FX_INVALID_JSON[] = "{ not json";

static const char FX_NULL_FUNCTION_CAUGHT[] =
    "{\"total_mutants\":1,\"caught\":1,\"missed\":0,\"timeout\":0,\"unviable\":0,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":{\"Mutant\":{\"name\":\"src/a.rs:7:5: delete -\",\""
    "package\":\"bt-x\",\"file\":\"src/a.rs\",\"function\":null,\"span\":{\"start\":{\"line\""
    ":7,\"column\":5},\"end\":{\"line\":7,\"column\":6}},\"replacement\":\"\",\"genre\":\""
    "UnaryOperator\"}},\"summary\":\"CaughtMutant\"}]}";

static const char FX_NULL_FUNCTION_MISSED[] =
    "{\"total_mutants\":1,\"caught\":0,\"missed\":1,\"timeout\":0,\"unviable\":0,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":{\"Mutant\":{\"name\":\"src/a.rs:7:5: delete -\",\""
    "package\":\"bt-x\",\"file\":\"src/a.rs\",\"function\":null,\"span\":{\"start\":{\"line\""
    ":7,\"column\":5},\"end\":{\"line\":7,\"column\":6}},\"replacement\":\"\",\"genre\":\""
    "UnaryOperator\"}},\"summary\":\"MissedMutant\"}]}";

static const char FX_NULL_FUNCTION_MISSED_BINOP[] =
    "{\"total_mutants\":1,\"caught\":0,\"missed\":1,\"timeout\":0,\"unviable\":0,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":{\"Mutant\":{\"name\":\"crates/bt-core/src/moves.rs:40:"
    "9: replace > with <= in -\",\"package\":\"bt-core\",\"file\":\"crates/bt-core/src/moves."
    "rs\",\"function\":null,\"span\":{\"start\":{\"line\":40,\"column\":9},\"end\":{\"line\":"
    "40,\"column\":10}},\"replacement\":\"<=\",\"genre\":\"BinaryOperator\"}},\"summary\":\""
    "MissedMutant\"}]}";

static const char FX_FAILED_BASELINE[] =
    "{\"total_mutants\":1,\"caught\":1,\"missed\":0,\"timeout\":0,\"unviable\":0,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":\"Baseline\",\"summary\":\"Failure\"},\n{\"scenario\":{\""
    "Mutant\":{\"name\":\"src/a.rs:7:5: replace x with y in f\",\"package\":\"bt-x\",\"file\""
    ":\"src/a.rs\",\"function\":{\"function_name\":\"f\",\"return_type\":\"-> u8\",\"span\":{"
    "\"start\":{\"line\":5,\"column\":1},\"end\":{\"line\":9,\"column\":2}}},\"span\":{\""
    "start\":{\"line\":7,\"column\":5},\"end\":{\"line\":7,\"column\":6}},\"replacement\":\""
    "y\",\"genre\":\"FnValue\"}},\"summary\":\"CaughtMutant\"}]}";

static const char FX_BASELINE_ONLY[] =
    "{\"total_mutants\":0,\"caught\":0,\"missed\":0,\"timeout\":0,\"unviable\":0,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":\"Baseline\",\"summary\":\"Success\"}]}";

static const char FX_CAUGHT_COUNTER_LIE[] =
    "{\"total_mutants\":1,\"caught\":0,\"missed\":0,\"timeout\":0,\"unviable\":0,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":\"Baseline\",\"summary\":\"Success\"},\n{\"scenario\":{\""
    "Mutant\":{\"name\":\"src/a.rs:7:5: replace x with y in f\",\"package\":\"bt-x\",\"file\""
    ":\"src/a.rs\",\"function\":{\"function_name\":\"f\",\"return_type\":\"-> u8\",\"span\":{"
    "\"start\":{\"line\":5,\"column\":1},\"end\":{\"line\":9,\"column\":2}}},\"span\":{\""
    "start\":{\"line\":7,\"column\":5},\"end\":{\"line\":7,\"column\":6}},\"replacement\":\""
    "y\",\"genre\":\"FnValue\"}},\"summary\":\"CaughtMutant\"}]}";

static const char FX_TOTAL_COUNTER_LIE[] =
    "{\"total_mutants\":2,\"caught\":1,\"missed\":0,\"timeout\":0,\"unviable\":0,\"success\":"
    "0,\"outcomes\":[\n{\"scenario\":\"Baseline\",\"summary\":\"Success\"},\n{\"scenario\":{\""
    "Mutant\":{\"name\":\"src/a.rs:7:5: replace x with y in f\",\"package\":\"bt-x\",\"file\""
    ":\"src/a.rs\",\"function\":{\"function_name\":\"f\",\"return_type\":\"-> u8\",\"span\":{"
    "\"start\":{\"line\":5,\"column\":1},\"end\":{\"line\":9,\"column\":2}}},\"span\":{\""
    "start\":{\"line\":7,\"column\":5},\"end\":{\"line\":7,\"column\":6}},\"replacement\":\""
    "y\",\"genre\":\"FnValue\"}},\"summary\":\"CaughtMutant\"}]}";

static const char FX_REAL_ALLOWLIST[] =
    "crates/bt-scheme/src/scramble.rs\tbuild_occ\tBinaryOperator\t%->+\t1\tthe offset only "
    "feeds a modulo 3 before use so every occupancy byte is unchanged\ncrates/bt-scheme/src/"
    "scramble.rs\trandom_occ\tBinaryOperator\t%->+\t1\tmutant twist values carry the same "
    "residues modulo 3 so every generated scramble is unchanged\ncrates/bt-scheme/src/memo."
    "rs\tOrbitWalk<'a>::run\tBinaryOperator\t+=->*=\t2\tthe counters only feed panic guards "
    "and a quiet guard loops forever so only a hang can reveal it\n";

static const char FX_REAL_SHAPE[] =
    "{\n  \"cargo_mutants_version\": \"27.1.0\",\n  \"start_time\": \"2026-10-14T10:00:00Z\","
    "\n  \"end_time\": \"2026-10-14T10:30:00Z\",\n  \"total_mutants\": 6,\n  \"caught\": 2,\n"
    "  \"missed\": 2,\n  \"timeout\": 2,\n  \"unviable\": 0,\n  \"success\": 0,\n  \""
    "outcomes\": [\n{\n  \"scenario\": \"Baseline\",\n  \"summary\": \"Success\",\n  \""
    "log_path\": \"log/baseline.log\",\n  \"diff_path\": null,\n  \"phase_results\": [\n    {"
    "\n      \"phase\": \"Build\",\n      \"duration\": 3.0,\n      \"process_status\": \""
    "Success\",\n      \"argv\": [\"cargo\", \"test\", \"--no-run\", \"--verbose\", \"--"
    "package=bt-scheme@0.0.0\"]\n    }\n  ]\n},\n{\n  \"scenario\": {\n    \"Mutant\": {\n   "
    "   \"name\": \"crates/bt-scheme/src/classifier.rs:151:57: replace && with || in Report::"
    "position\",\n      \"package\": \"bt-scheme\",\n      \"file\": \"crates/bt-scheme/src/"
    "classifier.rs\",\n      \"function\": {\n        \"function_name\": \"Report::position\""
    ",\n        \"return_type\": \"-> Option<String>\",\n        \"span\": { \"start\": { \""
    "line\": 127, \"column\": 5 }, \"end\": { \"line\": 181, \"column\": 6 } }\n      },\n   "
    "   \"span\": { \"start\": { \"line\": 151, \"column\": 57 }, \"end\": { \"line\": 151, \""
    "column\": 59 } },\n      \"replacement\": \"||\",\n      \"genre\": \"BinaryOperator\"\n"
    "    }\n  },\n  \"summary\": \"CaughtMutant\",\n  \"log_path\": \"log/crates__bt-"
    "scheme__src__classifier.rs_line_151_col_57.log\",\n  \"diff_path\": \"diff/crates__bt-"
    "scheme__src__classifier.rs_line_151_col_57.diff\",\n  \"phase_results\": [\n    {\n     "
    " \"phase\": \"Build\",\n      \"duration\": 2.1,\n      \"process_status\": \"Success\","
    "\n      \"argv\": [\"cargo\", \"test\", \"--no-run\", \"--verbose\", \"--package=bt-"
    "scheme@0.0.0\"]\n    }\n  ]\n},\n{\n  \"scenario\": {\n    \"Mutant\": {\n      \"name\""
    ": \"crates/bt-scheme/src/memo.rs:347:42: replace || with && in OrbitWalk<'a>::main_scan\""
    ",\n      \"package\": \"bt-scheme\",\n      \"file\": \"crates/bt-scheme/src/memo.rs\",\n"
    "      \"function\": {\n        \"function_name\": \"OrbitWalk<'a>::main_scan\",\n       "
    " \"return_type\": \"-> Option<u8>\",\n        \"span\": { \"start\": { \"line\": 345, \""
    "column\": 5 }, \"end\": { \"line\": 357, \"column\": 6 } }\n      },\n      \"span\": { "
    "\"start\": { \"line\": 347, \"column\": 42 }, \"end\": { \"line\": 347, \"column\": 44 }"
    " },\n      \"replacement\": \"&&\",\n      \"genre\": \"BinaryOperator\"\n    }\n  },\n "
    " \"summary\": \"CaughtMutant\",\n  \"log_path\": \"log/crates__bt-scheme__src__memo."
    "rs_line_347_col_42.log\",\n  \"diff_path\": \"diff/crates__bt-scheme__src__memo."
    "rs_line_347_col_42.diff\",\n  \"phase_results\": [\n    {\n      \"phase\": \"Build\",\n"
    "      \"duration\": 2.2,\n      \"process_status\": \"Success\",\n      \"argv\": [\""
    "cargo\", \"test\", \"--no-run\", \"--verbose\", \"--package=bt-scheme@0.0.0\"]\n    }\n "
    " ]\n},\n{\n  \"scenario\": {\n    \"Mutant\": {\n      \"name\": \"crates/bt-scheme/src/"
    "scramble.rs:74:39: replace % with + in build_occ\",\n      \"package\": \"bt-scheme\",\n"
    "      \"file\": \"crates/bt-scheme/src/scramble.rs\",\n      \"function\": {\n        \""
    "function_name\": \"build_occ\",\n        \"return_type\": \"\",\n        \"span\": { \""
    "start\": { \"line\": 59, \"column\": 1 }, \"end\": { \"line\": 79, \"column\": 2 } }\n  "
    "    },\n      \"span\": { \"start\": { \"line\": 74, \"column\": 39 }, \"end\": { \""
    "line\": 74, \"column\": 41 } },\n      \"replacement\": \"+\",\n      \"genre\": \""
    "BinaryOperator\"\n    }\n  },\n  \"summary\": \"MissedMutant\",\n  \"log_path\": \"log/"
    "crates__bt-scheme__src__scramble.rs_line_74_col_39.log\",\n  \"diff_path\": \"diff/"
    "crates__bt-scheme__src__scramble.rs_line_74_col_39.diff\",\n  \"phase_results\": [\n    "
    "{\n      \"phase\": \"Build\",\n      \"duration\": 2.3,\n      \"process_status\": \""
    "Success\",\n      \"argv\": [\"cargo\", \"test\", \"--no-run\", \"--verbose\", \"--"
    "package=bt-scheme@0.0.0\"]\n    }\n  ]\n},\n{\n  \"scenario\": {\n    \"Mutant\": {\n   "
    "   \"name\": \"crates/bt-scheme/src/scramble.rs:91:36: replace % with + in random_occ\","
    "\n      \"package\": \"bt-scheme\",\n      \"file\": \"crates/bt-scheme/src/scramble.rs\""
    ",\n      \"function\": {\n        \"function_name\": \"random_occ\",\n        \""
    "return_type\": \"-> [u8; 54]\",\n        \"span\": { \"start\": { \"line\": 81, \""
    "column\": 1 }, \"end\": { \"line\": 113, \"column\": 2 } }\n      },\n      \"span\": { "
    "\"start\": { \"line\": 91, \"column\": 36 }, \"end\": { \"line\": 91, \"column\": 38 } }"
    ",\n      \"replacement\": \"+\",\n      \"genre\": \"BinaryOperator\"\n    }\n  },\n  \""
    "summary\": \"MissedMutant\",\n  \"log_path\": \"log/crates__bt-scheme__src__scramble."
    "rs_line_91_col_36.log\",\n  \"diff_path\": \"diff/crates__bt-scheme__src__scramble."
    "rs_line_91_col_36.diff\",\n  \"phase_results\": [\n    {\n      \"phase\": \"Build\",\n "
    "     \"duration\": 2.4,\n      \"process_status\": \"Success\",\n      \"argv\": [\""
    "cargo\", \"test\", \"--no-run\", \"--verbose\", \"--package=bt-scheme@0.0.0\"]\n    }\n "
    " ]\n},\n{\n  \"scenario\": {\n    \"Mutant\": {\n      \"name\": \"crates/bt-scheme/src/"
    "memo.rs:384:19: replace += with *= in OrbitWalk<'a>::run\",\n      \"package\": \"bt-"
    "scheme\",\n      \"file\": \"crates/bt-scheme/src/memo.rs\",\n      \"function\": {\n   "
    "     \"function_name\": \"OrbitWalk<'a>::run\",\n        \"return_type\": \"-> "
    "WalkOutcome\",\n        \"span\": { \"start\": { \"line\": 379, \"column\": 5 }, \"end\""
    ": { \"line\": 449, \"column\": 6 } }\n      },\n      \"span\": { \"start\": { \"line\":"
    " 384, \"column\": 19 }, \"end\": { \"line\": 384, \"column\": 21 } },\n      \""
    "replacement\": \"*=\",\n      \"genre\": \"BinaryOperator\"\n    }\n  },\n  \"summary\":"
    " \"Timeout\",\n  \"log_path\": \"log/crates__bt-scheme__src__memo.rs_line_384_col_19."
    "log\",\n  \"diff_path\": \"diff/crates__bt-scheme__src__memo.rs_line_384_col_19.diff\",\n"
    "  \"phase_results\": [\n    {\n      \"phase\": \"Build\",\n      \"duration\": 2.5,\n  "
    "    \"process_status\": \"Success\",\n      \"argv\": [\"cargo\", \"test\", \"--no-run\""
    ", \"--verbose\", \"--package=bt-scheme@0.0.0\"]\n    }\n  ]\n},\n{\n  \"scenario\": {\n "
    "   \"Mutant\": {\n      \"name\": \"crates/bt-scheme/src/memo.rs:423:24: replace += "
    "with *= in OrbitWalk<'a>::run\",\n      \"package\": \"bt-scheme\",\n      \"file\": \""
    "crates/bt-scheme/src/memo.rs\",\n      \"function\": {\n        \"function_name\": \""
    "OrbitWalk<'a>::run\",\n        \"return_type\": \"-> WalkOutcome\",\n        \"span\": {"
    " \"start\": { \"line\": 379, \"column\": 5 }, \"end\": { \"line\": 449, \"column\": 6 } "
    "}\n      },\n      \"span\": { \"start\": { \"line\": 423, \"column\": 24 }, \"end\": { "
    "\"line\": 423, \"column\": 26 } },\n      \"replacement\": \"*=\",\n      \"genre\": \""
    "BinaryOperator\"\n    }\n  },\n  \"summary\": \"Timeout\",\n  \"log_path\": \"log/"
    "crates__bt-scheme__src__memo.rs_line_423_col_24.log\",\n  \"diff_path\": \"diff/"
    "crates__bt-scheme__src__memo.rs_line_423_col_24.diff\",\n  \"phase_results\": [\n    {\n"
    "      \"phase\": \"Build\",\n      \"duration\": 2.6,\n      \"process_status\": \""
    "Success\",\n      \"argv\": [\"cargo\", \"test\", \"--no-run\", \"--verbose\", \"--"
    "package=bt-scheme@0.0.0\"]\n    }\n  ]\n}\n  ]\n}";

static const char FX_PKG_A[] =
    "{\"cargo_mutants_version\":\"27.1.0\",\"start_time\":\"2026-10-16T09:40:00Z\",\""
    "end_time\":\"2026-10-16T10:10:00Z\",\"total_mutants\":2,\"caught\":1,\"missed\":1,\""
    "timeout\":0,\"unviable\":0,\"success\":0,\"outcomes\":[\n{\"scenario\":\"Baseline\",\""
    "summary\":\"Success\"},\n{\"scenario\":{\"Mutant\":{\"name\":\"src/a.rs:7:5: replace x "
    "with y in f\",\"package\":\"pkg-a\",\"file\":\"src/a.rs\",\"function\":{\"function_name\""
    ":\"f\",\"return_type\":\"-> u8\",\"span\":{\"start\":{\"line\":5,\"column\":1},\"end\":{"
    "\"line\":9,\"column\":2}}},\"span\":{\"start\":{\"line\":7,\"column\":5},\"end\":{\""
    "line\":7,\"column\":6}},\"replacement\":\"y\",\"genre\":\"FnValue\"}},\"summary\":\""
    "CaughtMutant\"},\n{\"scenario\":{\"Mutant\":{\"name\":\"src/a.rs:12:5: replace x with z "
    "in f\",\"package\":\"pkg-a\",\"file\":\"src/a.rs\",\"function\":{\"function_name\":\"f\""
    ",\"return_type\":\"-> u8\",\"span\":{\"start\":{\"line\":10,\"column\":1},\"end\":{\""
    "line\":14,\"column\":2}}},\"span\":{\"start\":{\"line\":12,\"column\":5},\"end\":{\""
    "line\":12,\"column\":6}},\"replacement\":\"z\",\"genre\":\"FnValue\"}},\"summary\":\""
    "MissedMutant\"}]}";

static const char FX_PKG_B[] =
    "{\"cargo_mutants_version\":\"27.1.0\",\"start_time\":\"2026-10-16T10:20:00Z\",\""
    "end_time\":\"2026-10-16T10:50:00Z\",\"total_mutants\":2,\"caught\":0,\"missed\":0,\""
    "timeout\":1,\"unviable\":1,\"success\":0,\"outcomes\":[\n{\"scenario\":\"Baseline\",\""
    "summary\":\"Success\"},\n{\"scenario\":{\"Mutant\":{\"name\":\"src/b.rs:7:5: replace += "
    "with *= in g\",\"package\":\"pkg-b\",\"file\":\"src/b.rs\",\"function\":{\""
    "function_name\":\"g\",\"return_type\":\"\",\"span\":{\"start\":{\"line\":5,\"column\":1}"
    ",\"end\":{\"line\":9,\"column\":2}}},\"span\":{\"start\":{\"line\":7,\"column\":5},\""
    "end\":{\"line\":7,\"column\":7}},\"replacement\":\"*=\",\"genre\":\"BinaryOperator\"}},\""
    "summary\":\"Timeout\"},\n{\"scenario\":{\"Mutant\":{\"name\":\"src/c.rs:3:1: replace g "
    "with ()\",\"package\":\"pkg-b\",\"file\":\"src/c.rs\",\"function\":{\"function_name\":\""
    "g\",\"return_type\":\"\",\"span\":{\"start\":{\"line\":1,\"column\":1},\"end\":{\"line\""
    ":5,\"column\":2}}},\"span\":{\"start\":{\"line\":3,\"column\":1},\"end\":{\"line\":3,\""
    "column\":2}},\"replacement\":\"()\",\"genre\":\"FnValue\"}},\"summary\":\"Unviable\"}]}";

static const char FX_KILLED_RUN[] =
    "{\"cargo_mutants_version\":\"27.1.0\",\"start_time\":\"2026-10-16T09:40:00Z\",\""
    "end_time\":\"\",\"total_mutants\":2,\"caught\":1,\"missed\":1,\"timeout\":0,\"unviable\""
    ":0,\"success\":0,\"outcomes\":[\n{\"scenario\":\"Baseline\",\"summary\":\"Success\"},\n{"
    "\"scenario\":{\"Mutant\":{\"name\":\"src/a.rs:7:5: replace x with y in f\",\"package\":\""
    "pkg-a\",\"file\":\"src/a.rs\",\"function\":{\"function_name\":\"f\",\"return_type\":\"->"
    " u8\",\"span\":{\"start\":{\"line\":5,\"column\":1},\"end\":{\"line\":9,\"column\":2}}},"
    "\"span\":{\"start\":{\"line\":7,\"column\":5},\"end\":{\"line\":7,\"column\":6}},\""
    "replacement\":\"y\",\"genre\":\"FnValue\"}},\"summary\":\"CaughtMutant\"},\n{\"scenario\""
    ":{\"Mutant\":{\"name\":\"src/a.rs:12:5: replace x with z in f\",\"package\":\"pkg-b\",\""
    "file\":\"src/a.rs\",\"function\":{\"function_name\":\"f\",\"return_type\":\"-> u8\",\""
    "span\":{\"start\":{\"line\":10,\"column\":1},\"end\":{\"line\":14,\"column\":2}}},\""
    "span\":{\"start\":{\"line\":12,\"column\":5},\"end\":{\"line\":12,\"column\":6}},\""
    "replacement\":\"z\",\"genre\":\"FnValue\"}},\"summary\":\"MissedMutant\"}]}";

static const char FX_COUNTER_LIE[] =
    "{\"cargo_mutants_version\":\"27.1.0\",\"start_time\":\"2026-10-16T09:40:00Z\",\""
    "end_time\":\"2026-10-16T10:10:00Z\",\"total_mutants\":2,\"caught\":0,\"missed\":1,\""
    "timeout\":0,\"unviable\":0,\"success\":0,\"outcomes\":[\n{\"scenario\":\"Baseline\",\""
    "summary\":\"Success\"},\n{\"scenario\":{\"Mutant\":{\"name\":\"src/a.rs:7:5: replace x "
    "with y in f\",\"package\":\"pkg-a\",\"file\":\"src/a.rs\",\"function\":{\"function_name\""
    ":\"f\",\"return_type\":\"-> u8\",\"span\":{\"start\":{\"line\":5,\"column\":1},\"end\":{"
    "\"line\":9,\"column\":2}}},\"span\":{\"start\":{\"line\":7,\"column\":5},\"end\":{\""
    "line\":7,\"column\":6}},\"replacement\":\"y\",\"genre\":\"FnValue\"}},\"summary\":\""
    "CaughtMutant\"},\n{\"scenario\":{\"Mutant\":{\"name\":\"src/a.rs:12:5: replace x with z "
    "in f\",\"package\":\"pkg-a\",\"file\":\"src/a.rs\",\"function\":{\"function_name\":\"f\""
    ",\"return_type\":\"-> u8\",\"span\":{\"start\":{\"line\":10,\"column\":1},\"end\":{\""
    "line\":14,\"column\":2}}},\"span\":{\"start\":{\"line\":12,\"column\":5},\"end\":{\""
    "line\":12,\"column\":6}},\"replacement\":\"z\",\"genre\":\"FnValue\"}},\"summary\":\""
    "MissedMutant\"}]}";

static const char FX_MERGE_FAILED_BASELINE[] =
    "{\"cargo_mutants_version\":\"27.1.0\",\"start_time\":\"2026-10-16T09:40:00Z\",\""
    "end_time\":\"2026-10-16T10:10:00Z\",\"total_mutants\":1,\"caught\":1,\"missed\":0,\""
    "timeout\":0,\"unviable\":0,\"success\":0,\"outcomes\":[\n{\"scenario\":\"Baseline\",\""
    "summary\":\"Failure\"},\n{\"scenario\":{\"Mutant\":{\"name\":\"src/a.rs:7:5: replace x "
    "with y in f\",\"package\":\"pkg-a\",\"file\":\"src/a.rs\",\"function\":{\"function_name\""
    ":\"f\",\"return_type\":\"-> u8\",\"span\":{\"start\":{\"line\":5,\"column\":1},\"end\":{"
    "\"line\":9,\"column\":2}}},\"span\":{\"start\":{\"line\":7,\"column\":5},\"end\":{\""
    "line\":7,\"column\":6}},\"replacement\":\"y\",\"genre\":\"FnValue\"}},\"summary\":\""
    "CaughtMutant\"}]}";

static const char FX_OTHER_VERSION[] =
    "{\"cargo_mutants_version\":\"27.0.0\",\"start_time\":\"2026-10-16T10:20:00Z\",\""
    "end_time\":\"2026-10-16T10:50:00Z\",\"total_mutants\":1,\"caught\":1,\"missed\":0,\""
    "timeout\":0,\"unviable\":0,\"success\":0,\"outcomes\":[\n{\"scenario\":\"Baseline\",\""
    "summary\":\"Success\"},\n{\"scenario\":{\"Mutant\":{\"name\":\"src/b.rs:7:5: replace += "
    "with *= in g\",\"package\":\"pkg-b\",\"file\":\"src/b.rs\",\"function\":{\""
    "function_name\":\"g\",\"return_type\":\"\",\"span\":{\"start\":{\"line\":5,\"column\":1}"
    ",\"end\":{\"line\":9,\"column\":2}}},\"span\":{\"start\":{\"line\":7,\"column\":5},\""
    "end\":{\"line\":7,\"column\":7}},\"replacement\":\"*=\",\"genre\":\"BinaryOperator\"}},\""
    "summary\":\"CaughtMutant\"}]}";

static const char FX_COMMENTED_ALLOWLIST[] =
    "# columns: file\tfunction\tgenre\treplacement\tcount\tjustification\n";

static unsigned long long fx_u64(const bt_json_value *root, const char *key) {
    const bt_json_value *value = bt_json_get(root, key);
    if (value == NULL || value->type != BT_JSON_NUMBER) {
        return (unsigned long long)-1;
    }
    return (unsigned long long)value->as.number;
}

static const char *fx_str(const bt_json_value *root, const char *key) {
    const bt_json_value *value = bt_json_get(root, key);
    if (value == NULL || value->type != BT_JSON_STRING) {
        return NULL;
    }
    return value->as.string;
}

BT_TEST(gate, an_empty_function_column_is_rejected_naming_the_sentinel) {
    const char *allowlist = "src/a.rs\t\tUnaryOperator\t(empty)\t1\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_NULL_FUNCTION_MISSED, allowlist, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "allowlist row 1") != NULL);
    BT_CHECK(strstr(msg, "(empty)") != NULL);
}

BT_TEST(gate, alien_scenario_string_fails) {
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_ALIEN_SCENARIO_STRING, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "malformed outcome") != NULL);
    BT_CHECK(strstr(msg, "Weird") != NULL);
}

BT_TEST(gate, arrow_notation_replacement_matches_bare_token) {
    const char *allowlist = "src/a.rs\tf\tFnValue\tx->y\t1\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(FX_MISSED_ONE, allowlist, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_OK);
    BT_CHECK_EQ(summary, "survivors gated: 1, all covered");
}

BT_TEST(gate, arrow_notation_with_empty_suffix_fails) {
    const char *allowlist = "src/a.rs\tf\tFnValue\tx->\t1\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_MISSED_ONE, allowlist, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "allowlist row 1") != NULL);
    BT_CHECK(strstr(msg, "replacement") != NULL);
}

BT_TEST(gate, baseline_and_caught_only_pass_with_empty_allowlist) {
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(FX_CAUGHT_ONLY, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_OK);
    BT_CHECK_EQ(summary, "survivors gated: 0, all covered");
}

BT_TEST(gate, caught_counter_mismatch_fails) {
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_CAUGHT_COUNTER_LIE, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "caught counter mismatch") != NULL);
    BT_CHECK(strstr(msg, "outcomes count 1 != file counter 0") != NULL);
}

BT_TEST(gate, count_mismatch_fails) {
    const char *allowlist = "src/b.rs\tg\tBinaryOperator\t*=\t2\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_TIMEOUT_ONE, allowlist, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "count mismatch") != NULL);
    BT_CHECK(strstr(msg, "outcomes=1 allowlist=2") != NULL);
}

BT_TEST(gate, counter_mismatch_fails) {
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_MISSED_ONE_WRONG_COUNTER, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "missed counter mismatch") != NULL);
    BT_CHECK(strstr(msg, "1") != NULL && strstr(msg, "0") != NULL);
}

BT_TEST(gate, duplicate_allowlist_rows_fail) {
    const char *allowlist = "src/b.rs\tg\tBinaryOperator\t*=\t1\taccepted\n"
                            "src/b.rs\tg\tBinaryOperator\t*=\t1\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_TIMEOUT_ONE, allowlist, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "allowlist row 2") != NULL);
    BT_CHECK(strstr(msg, "duplicate") != NULL);
}

BT_TEST(gate, duplicate_sentinel_rows_still_collide) {
    const char *allowlist = "src/a.rs\t(empty)\tUnaryOperator\t(empty)\t1\taccepted\n"
                            "src/a.rs\t(empty)\tUnaryOperator\t(empty)\t1\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_NULL_FUNCTION_MISSED, allowlist, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "allowlist row 2") != NULL);
    BT_CHECK(strstr(msg, "duplicate") != NULL);
}

BT_TEST(gate, empty_justification_fails) {
    const char *allowlist = "src/b.rs\tg\tBinaryOperator\t*=\t1\t";
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_TIMEOUT_ONE, allowlist, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "allowlist row 1") != NULL);
    BT_CHECK(strstr(msg, "justification") != NULL);
}

BT_TEST(gate, failed_baseline_fails_even_with_every_mutant_caught) {
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(FX_FAILED_BASELINE, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_VIOLATION);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "baseline summary is 'Failure', not Success") != NULL);
}

BT_TEST(gate, invalid_json_is_usage_error) {
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(FX_INVALID_JSON, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_USAGE);
}

BT_TEST(gate, malformed_allowlist_row_fails) {
    const char *allowlist = "src/b.rs\tg\tBinaryOperator\t*=\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_TIMEOUT_ONE, allowlist, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "allowlist row 1") != NULL);
    BT_CHECK(strstr(msg, "6") != NULL);
}

BT_TEST(gate, malformed_scenario_object_fails) {
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_MALFORMED_SCENARIO, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "malformed outcome") != NULL);
    BT_CHECK(strstr(msg, "Mutant") != NULL);
}

BT_TEST(gate, merge_refuses_a_counter_mismatch_input) {
    const char *inputs[2] = {FX_PKG_A, FX_COUNTER_LIE};
    char *json = NULL;
    unsigned long long total = 0;
    int rc = bt_gate_merge(inputs, 2, &json, &total);
    BT_CHECK_EQ(rc, BT_GATE_VIOLATION);
    BT_CHECK(strstr(bt_gate_last_error(), "counter") != NULL);
    BT_CHECK(json == NULL);
}

BT_TEST(gate, merge_refuses_a_failed_baseline_input) {
    const char *inputs[2] = {FX_PKG_A, FX_MERGE_FAILED_BASELINE};
    char *json = NULL;
    unsigned long long total = 0;
    int rc = bt_gate_merge(inputs, 2, &json, &total);
    BT_CHECK_EQ(rc, BT_GATE_VIOLATION);
    BT_CHECK(strstr(bt_gate_last_error(), "baseline") != NULL);
}

BT_TEST(gate, merge_refuses_a_killed_run_without_end_time) {
    const char *inputs[2] = {FX_PKG_A, FX_KILLED_RUN};
    char *json = NULL;
    unsigned long long total = 0;
    int rc = bt_gate_merge(inputs, 2, &json, &total);
    BT_CHECK_EQ(rc, BT_GATE_VIOLATION);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "end_time") != NULL);
    BT_CHECK(strstr(msg, "input 2") != NULL);
}

BT_TEST(gate, merge_refuses_a_single_input) {
    const char *inputs[1] = {FX_PKG_A};
    char *json = NULL;
    unsigned long long total = 0;
    int rc = bt_gate_merge(inputs, 1, &json, &total);
    BT_CHECK_EQ(rc, BT_GATE_USAGE);
    BT_CHECK(strstr(bt_gate_last_error(), "two") != NULL);
}

BT_TEST(gate, merge_refuses_version_disagreement) {
    const char *inputs[2] = {FX_PKG_A, FX_OTHER_VERSION};
    char *json = NULL;
    unsigned long long total = 0;
    int rc = bt_gate_merge(inputs, 2, &json, &total);
    BT_CHECK_EQ(rc, BT_GATE_VIOLATION);
    BT_CHECK(strstr(bt_gate_last_error(), "version") != NULL);
}

BT_TEST(gate, merge_sums_counters_concatenates_outcomes_and_extends_times) {
    const char *inputs[2] = {FX_PKG_A, FX_PKG_B};
    char *json = NULL;
    unsigned long long total = 0;
    int rc = bt_gate_merge(inputs, 2, &json, &total);
    BT_CHECK_EQ(rc, BT_GATE_OK);
    BT_CHECK(json != NULL);
    bt_json_value *root = NULL;
    bt_json_error error;
    BT_CHECK_EQ(bt_json_parse(json, strlen(json), &root, &error), 0);
    BT_CHECK(root != NULL);
    BT_CHECK_EQ(fx_u64(root, "total_mutants"), 4ULL);
    BT_CHECK_EQ(fx_u64(root, "caught"), 1ULL);
    BT_CHECK_EQ(fx_u64(root, "missed"), 1ULL);
    BT_CHECK_EQ(fx_u64(root, "timeout"), 1ULL);
    BT_CHECK_EQ(fx_u64(root, "unviable"), 1ULL);
    const bt_json_value *outcomes = bt_json_get(root, "outcomes");
    BT_CHECK(outcomes != NULL && outcomes->type == BT_JSON_ARRAY);
    if (outcomes != NULL && outcomes->type == BT_JSON_ARRAY) {
        BT_CHECK_EQ(outcomes->as.array.count, 6ULL);
    }
    BT_CHECK_EQ(fx_str(root, "start_time"), "2026-10-16T09:40:00Z");
    BT_CHECK_EQ(fx_str(root, "end_time"), "2026-10-16T10:50:00Z");
    BT_CHECK_EQ(fx_str(root, "cargo_mutants_version"), "27.1.0");
    const char *want[4] = {"pkg-a", "pkg-a", "pkg-b", "pkg-b"};
    size_t seen = 0;
    if (outcomes != NULL && outcomes->type == BT_JSON_ARRAY) {
        for (size_t i = 0; i < outcomes->as.array.count && seen < 4; i++) {
            const bt_json_value *scenario = bt_json_get(outcomes->as.array.items[i], "scenario");
            const bt_json_value *mutant = bt_json_get(scenario, "Mutant");
            if (mutant == NULL) {
                continue;
            }
            const bt_json_value *package = bt_json_get(mutant, "package");
            BT_CHECK(package != NULL && package->type == BT_JSON_STRING);
            if (package != NULL && package->type == BT_JSON_STRING) {
                BT_CHECK_EQ(package->as.string, want[seen]);
                seen++;
            }
        }
    }
    BT_CHECK_EQ(seen, 4ULL);
    BT_CHECK_EQ(total, 4ULL);
    bt_json_free(root);
    free(json);
}

BT_TEST(gate, merged_output_feeds_the_gate) {
    const char *inputs[2] = {FX_PKG_A, FX_PKG_B};
    char *json = NULL;
    unsigned long long total = 0;
    int rc = bt_gate_merge(inputs, 2, &json, &total);
    BT_CHECK_EQ(rc, BT_GATE_OK);
    BT_CHECK(json != NULL);
    const char *allowlist = "src/a.rs\tf\tFnValue\tz\t1\taccepted\n"
                            "src/b.rs\tg\tBinaryOperator\t*=\t1\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    rc = bt_gate_run(json, allowlist, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_OK);
    BT_CHECK_EQ(summary, "survivors gated: 2, all covered");
    free(json);
}

BT_TEST(gate, missed_survivor_without_allowlist_row_fails) {
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(FX_MISSED_ONE, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_VIOLATION);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "not in allowlist") != NULL);
    BT_CHECK(strstr(msg, "file=src/a.rs function=f genre=FnValue replacement=y") != NULL);
}

BT_TEST(gate, module_level_binop_survivor_with_a_real_token_matches_the_sentinel_row) {
    const char *allowlist = "crates/bt-core/src/moves.rs\t(empty)\tBinaryOperator\t<=\t1\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(FX_NULL_FUNCTION_MISSED_BINOP, allowlist, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_OK);
    BT_CHECK_EQ(summary, "survivors gated: 1, all covered");
}

BT_TEST(gate, module_level_mutant_surviving_without_an_allowlist_row_fails) {
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(FX_NULL_FUNCTION_MISSED, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_VIOLATION);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "not in allowlist") != NULL);
    BT_CHECK(strstr(msg, "file=src/a.rs function= genre=UnaryOperator") != NULL);
}

BT_TEST(gate, module_level_mutants_without_a_function_pass_when_caught) {
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(FX_NULL_FUNCTION_CAUGHT, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_OK);
    BT_CHECK_EQ(summary, "survivors gated: 0, all covered");
}

BT_TEST(gate, module_level_survivor_is_allowlistable_through_the_sentinel) {
    const char *allowlist = "src/a.rs\t(empty)\tUnaryOperator\t(empty)\t1\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(FX_NULL_FUNCTION_MISSED, allowlist, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_OK);
    BT_CHECK_EQ(summary, "survivors gated: 1, all covered");
}

BT_TEST(gate, non_numeric_count_fails) {
    const char *allowlist = "src/b.rs\tg\tBinaryOperator\t*=\ttwo\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_TIMEOUT_ONE, allowlist, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "allowlist row 1") != NULL);
    BT_CHECK(strstr(msg, "count") != NULL);
}

BT_TEST(gate, outcomes_without_any_mutant_fail) {
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(FX_BASELINE_ONLY, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_VIOLATION);
    BT_CHECK(strstr(bt_gate_last_error(), "no mutant outcomes") != NULL);
}

BT_TEST(gate, real_shape_with_fixture_allowlist_passes) {
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(FX_REAL_SHAPE, FX_REAL_ALLOWLIST, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_OK);
    BT_CHECK_EQ(summary, "survivors gated: 4, all covered");
}

BT_TEST(gate, repeated_key_matches_count_two_row) {
    const char *allowlist = "src/b.rs\tg\tBinaryOperator\t*=\t2\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(FX_TIMEOUT_TWICE, allowlist, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_OK);
    BT_CHECK_EQ(summary, "survivors gated: 2, all covered");
}

BT_TEST(gate, stale_allowlist_entry_fails) {
    const char *allowlist = "src/zz.rs\th\tFnValue\t()\t1\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_CAUGHT_ONLY, allowlist, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "stale allowlist entry") != NULL);
    BT_CHECK(strstr(msg, "file=src/zz.rs function=h") != NULL);
}

BT_TEST(gate, sentinel_row_without_a_surviving_outcome_is_stale) {
    const char *allowlist = "src/a.rs\t(empty)\tUnaryOperator\t(empty)\t1\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_CAUGHT_ONLY, allowlist, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "stale allowlist entry") != NULL);
    BT_CHECK(strstr(msg, "function= ") != NULL);
}

BT_TEST(gate, timeout_survivor_covered_passes) {
    const char *allowlist = "src/b.rs\tg\tBinaryOperator\t*=\t1\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(FX_TIMEOUT_ONE, allowlist, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_OK);
    BT_CHECK_EQ(summary, "survivors gated: 1, all covered");
}

BT_TEST(gate, total_mutants_counter_mismatch_fails) {
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_TOTAL_COUNTER_LIE, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "total_mutants counter mismatch") != NULL);
    BT_CHECK(strstr(msg, "outcomes count 1 != file counter 2") != NULL);
}

BT_TEST(gate, unknown_summary_fails) {
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(FX_UNKNOWN_SUMMARY, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_VIOLATION);
    BT_CHECK(strstr(bt_gate_last_error(), "unknown summary 'UnviableMutant'") != NULL);
}

BT_TEST(gate, unviable_summary_is_ignored) {
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(FX_UNVIABLE_PRESENT, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    BT_CHECK_EQ(rc, BT_GATE_OK);
    BT_CHECK_EQ(summary, "survivors gated: 0, all covered");
}

BT_TEST(gate, zero_count_fails) {
    const char *allowlist = "src/b.rs\tg\tBinaryOperator\t*=\t0\taccepted";
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_TIMEOUT_ONE, allowlist, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "allowlist row 1") != NULL);
    BT_CHECK(strstr(msg, "count") != NULL);
}

BT_TEST(gate, missing_counter_fails) {
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_MISSING_COUNTER, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    BT_CHECK(strstr(bt_gate_last_error(), "counter 'missed'") != NULL);
}

BT_TEST(gate, missing_replacement_field_fails) {
    char summary[BT_GATE_SUMMARY_MAX];
    bt_gate_run(FX_MISSING_REPLACEMENT, FX_COMMENTED_ALLOWLIST, summary, sizeof summary);
    const char *msg = bt_gate_last_error();
    BT_CHECK(strstr(msg, "malformed outcome") != NULL);
    BT_CHECK(strstr(msg, "replacement") != NULL);
}

int bt_mutants_gate_selftest(void) {
    bt_test_entry tests[] = {
        {
            "gate.an_empty_function_column_is_rejected_naming_the_sentinel",
            bt_test_gate_an_empty_function_column_is_rejected_naming_the_sentinel,
        },
        {"gate.alien_scenario_string_fails", bt_test_gate_alien_scenario_string_fails},
        {
            "gate.arrow_notation_replacement_matches_bare_token",
            bt_test_gate_arrow_notation_replacement_matches_bare_token,
        },
        {
            "gate.arrow_notation_with_empty_suffix_fails",
            bt_test_gate_arrow_notation_with_empty_suffix_fails,
        },
        {
            "gate.baseline_and_caught_only_pass_with_empty_allowlist",
            bt_test_gate_baseline_and_caught_only_pass_with_empty_allowlist,
        },
        {"gate.caught_counter_mismatch_fails", bt_test_gate_caught_counter_mismatch_fails},
        {"gate.count_mismatch_fails", bt_test_gate_count_mismatch_fails},
        {"gate.counter_mismatch_fails", bt_test_gate_counter_mismatch_fails},
        {"gate.duplicate_allowlist_rows_fail", bt_test_gate_duplicate_allowlist_rows_fail},
        {
            "gate.duplicate_sentinel_rows_still_collide",
            bt_test_gate_duplicate_sentinel_rows_still_collide,
        },
        {"gate.empty_justification_fails", bt_test_gate_empty_justification_fails},
        {
            "gate.failed_baseline_fails_even_with_every_mutant_caught",
            bt_test_gate_failed_baseline_fails_even_with_every_mutant_caught,
        },
        {"gate.invalid_json_is_usage_error", bt_test_gate_invalid_json_is_usage_error},
        {"gate.malformed_allowlist_row_fails", bt_test_gate_malformed_allowlist_row_fails},
        {"gate.malformed_scenario_object_fails", bt_test_gate_malformed_scenario_object_fails},
        {"gate.merged_output_feeds_the_gate", bt_test_gate_merged_output_feeds_the_gate},
        {
            "gate.missed_survivor_without_allowlist_row_fails",
            bt_test_gate_missed_survivor_without_allowlist_row_fails,
        },
        {"gate.missing_counter_fails", bt_test_gate_missing_counter_fails},
        {"gate.missing_replacement_field_fails", bt_test_gate_missing_replacement_field_fails},
        {
            "gate.merge_refuses_a_counter_mismatch_input",
            bt_test_gate_merge_refuses_a_counter_mismatch_input,
        },
        {
            "gate.merge_refuses_a_failed_baseline_input",
            bt_test_gate_merge_refuses_a_failed_baseline_input,
        },
        {
            "gate.merge_refuses_a_killed_run_without_end_time",
            bt_test_gate_merge_refuses_a_killed_run_without_end_time,
        },
        {"gate.merge_refuses_a_single_input", bt_test_gate_merge_refuses_a_single_input},
        {
            "gate.merge_refuses_version_disagreement",
            bt_test_gate_merge_refuses_version_disagreement,
        },
        {
            "gate.merge_sums_counters_concatenates_outcomes_and_extends_times",
            bt_test_gate_merge_sums_counters_concatenates_outcomes_and_extends_times,
        },
        {
            "gate.module_level_binop_survivor_with_a_real_token_matches_the_sentinel_row",
            bt_test_gate_module_level_binop_survivor_with_a_real_token_matches_the_sentinel_row,
        },
        {
            "gate.module_level_mutant_surviving_without_an_allowlist_row_fails",
            bt_test_gate_module_level_mutant_surviving_without_an_allowlist_row_fails,
        },
        {
            "gate.module_level_mutants_without_a_function_pass_when_caught",
            bt_test_gate_module_level_mutants_without_a_function_pass_when_caught,
        },
        {
            "gate.module_level_survivor_is_allowlistable_through_the_sentinel",
            bt_test_gate_module_level_survivor_is_allowlistable_through_the_sentinel,
        },
        {"gate.non_numeric_count_fails", bt_test_gate_non_numeric_count_fails},
        {"gate.outcomes_without_any_mutant_fail", bt_test_gate_outcomes_without_any_mutant_fail},
        {
            "gate.real_shape_with_fixture_allowlist_passes",
            bt_test_gate_real_shape_with_fixture_allowlist_passes,
        },
        {
            "gate.repeated_key_matches_count_two_row",
            bt_test_gate_repeated_key_matches_count_two_row,
        },
        {
            "gate.sentinel_row_without_a_surviving_outcome_is_stale",
            bt_test_gate_sentinel_row_without_a_surviving_outcome_is_stale,
        },
        {"gate.stale_allowlist_entry_fails", bt_test_gate_stale_allowlist_entry_fails},
        {"gate.timeout_survivor_covered_passes", bt_test_gate_timeout_survivor_covered_passes},
        {
            "gate.total_mutants_counter_mismatch_fails",
            bt_test_gate_total_mutants_counter_mismatch_fails,
        },
        {"gate.unknown_summary_fails", bt_test_gate_unknown_summary_fails},
        {"gate.unviable_summary_is_ignored", bt_test_gate_unviable_summary_is_ignored},
        {"gate.zero_count_fails", bt_test_gate_zero_count_fails},
    };
    return bt_harness_run("mutants-gate", tests, sizeof tests / sizeof tests[0]);
}
