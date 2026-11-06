import argparse
import json
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LANE = REPO / "artifacts" / "lane" / "lane-macro-restruct"


def load(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))["rows"]


def main():
    parser = argparse.ArgumentParser(prog="cov_delta_macro.py")
    parser.add_argument("--before", default=str(LANE / "cov-report-baseline.json"))
    parser.add_argument("--after", default=str(LANE / "cov-report-after.json"))
    args = parser.parse_args()
    before = load(args.before)
    after = load(args.after)
    names = sorted(set(before) | set(after))
    total_dark_before = 0
    total_dark_after = 0
    rows = []
    for name in names:
        b = before.get(name, {"covered": 0, "count": 0})
        a = after.get(name, {"covered": 0, "count": 0})
        dark_b = b["count"] - b["covered"]
        dark_a = a["count"] - a["covered"]
        total_dark_before += dark_b
        total_dark_after += dark_a
        if dark_b != dark_a or b["count"] != a["count"]:
            rows.append((name, b, a, dark_b, dark_a))
    for name, b, a, dark_b, dark_a in rows:
        delta = dark_a - dark_b
        print(f"{name}: dark {dark_b} -> {dark_a} ({delta:+d}) regions {b['count']} -> {a['count']}")
    print(f"total dark regions: {total_dark_before} -> {total_dark_after} "
          f"({total_dark_after - total_dark_before:+d})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
