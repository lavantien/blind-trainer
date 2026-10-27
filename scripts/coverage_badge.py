import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import slog
from badgesvg import badge_svg

TOOL = slog.Tool("coverage-badge")
REPO = Path(__file__).resolve().parent.parent
AREA_PATTERN = re.compile(r"^(?:src|tools)/([^/]+)/")
WORST_PATTERN = re.compile(r"^(?:(?:src|tools)/[^/]+/|tests/fuzz/)")


def fail(message):
    return TOOL.line("fail", message)


def rel_path(filename):
    rel = str(filename).replace("\\", "/")
    prefix = str(REPO).replace("\\", "/") + "/"
    if rel.lower().startswith(prefix.lower()):
        return rel[len(prefix) :]
    return rel


def area_of(rel):
    match = AREA_PATTERN.match(rel)
    return match.group(1) if match else None


def area_rows(files):
    rows = {}
    for entry in files:
        rel = rel_path(entry["filename"])
        area = area_of(rel)
        if area is None:
            continue
        regions = entry["summary"]["regions"]
        row = rows.setdefault(area, {"area": area, "covered": 0, "total": 0})
        row["covered"] += int(regions["covered"])
        row["total"] += int(regions["count"])
    for row in rows.values():
        row["pct"] = 100.0 * row["covered"] / row["total"] if row["total"] else 100.0
    return [rows[area] for area in sorted(rows, key=lambda a: (rows[a]["pct"], a))]


def worst_files(files, count=25):
    picked = []
    for entry in files:
        rel = rel_path(entry["filename"])
        if not WORST_PATTERN.match(rel):
            continue
        if int(entry["summary"]["regions"]["count"]) <= 0:
            continue
        picked.append(entry)
    picked.sort(key=lambda entry: float(entry["summary"]["regions"]["percent"]))
    return picked[:count]


def dead_by_file(functions):
    dead = {}
    for fn in functions:
        if int(fn.get("count", 0)) != 0:
            continue
        filenames = fn.get("filenames") or []
        if not filenames:
            continue
        rel = rel_path(filenames[0])
        dead.setdefault(rel, []).append(fn["name"][:60])
    return dead


def report_lines(data, stamp):
    totals = data["totals"]["regions"]
    percent = float(totals["percent"])
    lines = [
        "# workspace coverage report",
        "",
        f"generated: {stamp}",
        f"regions: {int(totals['covered'])} of {int(totals['count'])} ({percent:.1f}%)",
        "",
        "| area | regions | percent |",
        "| --- | --- | --- |",
    ]
    for row in area_rows(data["files"]):
        lines.append(f"| {row['area']} | {row['covered']}/{row['total']} | {row['pct']:.1f}% |")
    lines += ["", "worst 25 files:", ""]
    dead = dead_by_file(data.get("functions", []))
    for entry in worst_files(data["files"]):
        regions = entry["summary"]["regions"]
        rel = rel_path(entry["filename"])
        lines.append(f"- {rel}: {int(regions['covered'])}/{int(regions['count'])} regions ({float(regions['percent']):.1f}%)")
        if rel in dead:
            lines.append(f"  uncovered: {', '.join(dead[rel][:5])}")
    return lines


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="coverage_badge.py",
        description="turn an llvm-cov json export into the coverage report and badge",
        allow_abbrev=False,
    )
    parser.add_argument("export", help="path to the llvm-cov json export")
    args = parser.parse_args(argv)
    TOOL.entry("coverage-badge", f"export={args.export}")
    source = Path(args.export)
    if not source.is_file():
        fail(f"llvm-cov export not found: {source}, run the coverage build and export first")
        return 2
    try:
        report = json.loads(source.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        fail(f"llvm-cov export unreadable: {exc}")
        return 1
    data = report["data"][0]
    totals = data["totals"]["regions"]
    percent = float(totals["percent"])
    report_dir = REPO / "artifacts" / "coverage"
    badge_dir = REPO / "docs" / "badges"
    report_dir.mkdir(parents=True, exist_ok=True)
    badge_dir.mkdir(parents=True, exist_ok=True)
    import datetime

    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    (report_dir / "report.md").write_text("\n".join(report_lines(data, stamp)), encoding="utf-8", newline="\n")
    value = f"{percent:.1f}%"
    (badge_dir / "coverage.svg").write_text(badge_svg("coverage", value, percent), encoding="utf-8", newline="\n")
    print(f"coverage regions: {percent:.1f}%")
    TOOL.exit("coverage-badge", 0)
    return 0


if __name__ == "__main__":
    sys.exit(main())
