import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import slog
from badgesvg import GREY, GREEN, RED, YELLOW, badge_svg

TOOL = slog.Tool("ci-badge")
REPO = Path(__file__).resolve().parent.parent
GH_ARGS = [
    "run",
    "list",
    "--workflow",
    "ci.yml",
    "--branch",
    "main",
    "--limit",
    "1",
    "--json",
    "status,conclusion,headSha,createdAt,databaseId",
]
GH_TIMEOUT_SECS = 30
LIVE_STATUSES = ("in_progress", "queued")
FAILED_CONCLUSIONS = ("failure", "cancelled", "startup_failure")
VERDICT_COLORS = {"passed": GREEN, "failed": RED, "running": YELLOW, "unknown": GREY}


def fail(message):
    return TOOL.line("fail", message)


def verdict_of(run):
    status = run.get("status")
    if status in LIVE_STATUSES:
        return "running"
    if status == "completed":
        conclusion = run.get("conclusion")
        if conclusion == "success":
            return "passed"
        if conclusion in FAILED_CONCLUSIONS:
            return "failed"
    return "unknown"


def query_latest_run(gh):
    proc = subprocess.run(
        [gh, *GH_ARGS],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="backslashreplace",
        timeout=GH_TIMEOUT_SECS,
    )
    if proc.returncode != 0:
        reason = (proc.stderr or proc.stdout or "no output").strip().splitlines()[-1]
        raise RuntimeError(f"gh exited {proc.returncode}: {reason}")
    return json.loads(proc.stdout)


def run_detail(run):
    sha = str(run.get("headSha") or "")
    return f"run={run.get('databaseId')} sha={sha[:7]}"


def render(out, verdict):
    out.parent.mkdir(parents=True, exist_ok=True)
    svg = badge_svg("CI", verdict, color=VERDICT_COLORS[verdict])
    out.write_text(svg, encoding="utf-8", newline="\n")
    return svg


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="ci_badge.py",
        description="refresh the ci badge from the latest ci run on main through gh",
        allow_abbrev=False,
    )
    parser.add_argument("--out", default=None, help="svg path, default docs/badges/ci.svg")
    parser.add_argument(
        "--offline",
        action="store_true",
        help="render the unknown badge and exit 0 when gh cannot answer",
    )
    args = parser.parse_args(argv)
    TOOL.line(
        "entry", f"out={args.out or 'docs/badges/ci.svg'} offline={args.offline}"
    )
    out = Path(args.out) if args.out else REPO / "docs" / "badges" / "ci.svg"
    verdict = "unknown"
    detail = "no matching run"
    try:
        gh = shutil.which("gh")
        if gh is None:
            raise RuntimeError("gh binary not found on PATH")
        runs = query_latest_run(gh)
        if not isinstance(runs, list):
            raise ValueError("gh answer is not a run list")
        if runs:
            if not isinstance(runs[0], dict):
                raise ValueError("gh run entry is not an object")
            verdict = verdict_of(runs[0])
            detail = run_detail(runs[0])
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as exc:
        if not args.offline:
            fail(str(exc))
            TOOL.line("exit", "code=1")
            return 1
        TOOL.line("offline", f"gh query failed, rendering unknown: {exc}")
    render(out, verdict)
    print(f"ci: {verdict} ({detail})")
    TOOL.line("exit", "code=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
