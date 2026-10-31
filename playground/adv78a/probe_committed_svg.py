import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "scripts"))
import ci_badge
from badgesvg import badge_svg

raw = (REPO / "docs" / "badges" / "ci.svg").read_bytes()
expected = badge_svg("CI", "passed", color=ci_badge.VERDICT_COLORS["passed"]).encode("utf-8")
print("committed-svg-equals-renderer-passed:", "PASS" if raw == expected else "FAIL")
match = re.search(rb'aria-label="CI: ([^"]+)"', raw)
print("aria-label-verdict:", match.group(1).decode("utf-8") if match else "MISSING")
print("newline-style-lf-only:", "PASS" if b"\r" not in raw else "FAIL")

readme = (REPO / "README.md").read_text(encoding="utf-8")
row = "[![CI](docs/badges/ci.svg)](https://github.com/lavantien/blind-trainer/actions/workflows/ci.yml)"
print("readme-badge-row:", "PASS" if row in readme else "FAIL")

doctored = badge_svg("CI", "failed", color=ci_badge.VERDICT_COLORS["failed"])
value = re.search(r'aria-label="CI: ([^"]+)"', doctored).group(1)
regen = badge_svg("CI", value, color=ci_badge.VERDICT_COLORS[value])
print(
    "committed-badge-test-accepts-doctored-verdict:",
    "GAP-CONFIRMED" if doctored == regen else "unexpected",
)
