import importlib.util
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPTS = HERE.parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))
import badgesvg as new_mod

spec = importlib.util.spec_from_file_location("old_badgesvg", HERE / "old_badgesvg.py")
old_mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(old_mod)

labels = ["CI", "coverage", "alg coverage", "x", ""]
values = [
    "passed",
    "failed",
    "running",
    "unknown",
    "10.0%",
    "99.9%",
    "0%",
    "100.0%",
    "59.9%",
    "60.0%",
    "79.9%",
    "80.0%",
    "",
]
percents = [p / 4 for p in range(401)] + [59.999, 60, 79.999, 80, 0, 100, -1, 1000]

mismatches = []
count = 0
for label in labels:
    for value in values:
        for percent in percents:
            old = old_mod.badge_svg(label, value, percent)
            new = new_mod.badge_svg(label, value, percent)
            count += 1
            if old != new:
                mismatches.append((label, value, percent))
print(
    f"byte-identity across {count} percent-style calls:",
    "PASS" if not mismatches else f"FAIL {mismatches[:5]}",
)

explicit_matches = all(
    new_mod.badge_svg(label, value, percent, color=new_mod.GREEN if percent >= 80 else new_mod.YELLOW if percent >= 60 else new_mod.RED)
    == old_mod.badge_svg(label, value, percent)
    for label in labels
    for value in values
    for percent in percents
)
print("explicit-color-equals-derived:", "PASS" if explicit_matches else "FAIL")
