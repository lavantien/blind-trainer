import sys
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parents[2] / "scripts"
sys.path.insert(0, str(SCRIPTS))

import ci_badge

STATUSES = [
    "queued",
    "in_progress",
    "completed",
    "waiting",
    "requested",
    "action_required",
    "pending",
    None,
    "",
    7,
]
CONCLUSIONS = [
    "success",
    "failure",
    "cancelled",
    "startup_failure",
    "timed_out",
    "skipped",
    "stale",
    "neutral",
    "action_required",
    None,
    "",
    7,
]

violations = []
for status in STATUSES:
    for conclusion in CONCLUSIONS:
        verdict = ci_badge.verdict_of({"status": status, "conclusion": conclusion})
        if verdict == "passed" and not (status == "completed" and conclusion == "success"):
            violations.append((status, conclusion, verdict))
print(
    "green-only-for-completed-success:",
    "PASS" if not violations else f"FAIL {violations}",
)

expected = {
    ("completed", "success"): "passed",
    ("completed", "failure"): "failed",
    ("completed", "cancelled"): "failed",
    ("completed", "startup_failure"): "failed",
    ("in_progress", None): "running",
    ("queued", None): "running",
}
mismatches = []
for (status, conclusion), want in expected.items():
    got = ci_badge.verdict_of({"status": status, "conclusion": conclusion})
    if got != want:
        mismatches.append((status, conclusion, got, want))
print("named-mappings:", "PASS" if not mismatches else f"FAIL {mismatches}")

grey = sorted(
    {
        (status, conclusion)
        for status in STATUSES
        for conclusion in CONCLUSIONS
        if ci_badge.verdict_of({"status": status, "conclusion": conclusion}) == "unknown"
    },
    key=repr,
)
print(f"grey-zone-size: {len(grey)}")
for pair in (
    ("completed", "timed_out"),
    ("completed", "skipped"),
    ("completed", "stale"),
    ("completed", "neutral"),
    ("completed", "action_required"),
):
    print(f"verdict {pair}:", ci_badge.verdict_of({"status": pair[0], "conclusion": pair[1]}))
print(
    "missing-fields:",
    ci_badge.verdict_of({}),
    ci_badge.verdict_of({"status": None, "conclusion": None}),
)
