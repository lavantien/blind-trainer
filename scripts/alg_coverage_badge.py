import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import slog
from badgesvg import badge_svg

TOOL = slog.Tool("alg-coverage-badge")
REPO = Path(__file__).resolve().parent.parent


def fail(message):
    return TOOL.line("fail", message)


def compute(manifest):
    cases = manifest["cases"]
    total = int(cases["entries"])
    covered = int(cases["edge"]["covered"]) + int(cases["corner"]["covered"])
    if total <= 0:
        raise ValueError("manifest reports no cases")
    percent = min(100.0, max(0.0, 100.0 * covered / total))
    return percent, covered, total


def missing_manifest_error(manifest_path):
    return f"manifest missing: {manifest_path}, run python bt.py gen-data first"


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="alg_coverage_badge.py",
        description="refresh the alg coverage badge from the vendor manifest",
        allow_abbrev=False,
    )
    args = parser.parse_args(argv)
    TOOL.entry("alg-coverage-badge")
    hub = json.loads((REPO / "config" / "config.json").read_text(encoding="utf-8"))
    data_vendor = REPO / hub["paths"]["data_vendor"]
    manifest_path = data_vendor / "manifest.json"
    if not manifest_path.is_file():
        fail(missing_manifest_error(manifest_path))
        return 1
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        percent, covered, total = compute(manifest)
    except (OSError, ValueError, KeyError) as exc:
        fail(str(exc))
        return 1
    badge_dir = REPO / "docs" / "badges"
    badge_dir.mkdir(parents=True, exist_ok=True)
    value = f"{percent:.1f}%"
    (badge_dir / "alg-coverage.svg").write_text(badge_svg("alg coverage", value, percent), encoding="utf-8", newline="\n")
    print(f"alg coverage: {percent:.1f}% ({covered} of {total} cases)")
    TOOL.exit("alg-coverage-badge", 0)
    return 0


if __name__ == "__main__":
    sys.exit(main())
