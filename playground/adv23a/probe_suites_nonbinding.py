import inspect
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO))
sys.path.insert(0, str(REPO / "scripts"))

import ga_parity

HERE = Path(__file__).resolve().parent

suites = [{"stem": "ga_eval", "code": 7, "tail": "suite ga_ga_eval_test: 9 passed, 1 failed"}]
golden_drift = []
drift = []
failures = golden_drift + drift
verdict = {"ok": not failures, "suites": suites}

source = inspect.getsource(ga_parity.run_parity)
out = {
    "suite_result": suites[0],
    "failures_composition": failures,
    "verdict_ok_despite_suite_failure": verdict["ok"],
    "run_suite_stem_raises_only_on_missing_exe": "parity suite exe missing" in inspect.getsource(
        ga_parity.run_suite_stem
    ),
    "run_parity_source_bind_check": [
        line.strip() for line in source.splitlines() if "suites" in line or "failures" in line
    ],
    "verdict": "ga-parity exits 0 with a failing parity suite: run_parity builds failures from golden_drift + drift only, suite exit codes are recorded but never bound to ok (scripts/ga_parity.py:199-205, 247)",
}
(HERE / "suites_nonbinding.json").write_text(json.dumps(out, indent=2), encoding="utf-8")
print(json.dumps(out, indent=2))
