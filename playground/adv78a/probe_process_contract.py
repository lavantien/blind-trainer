import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SCRIPT = REPO / "scripts" / "ci_badge.py"


def run_cli(out, extra, path_dirs):
    env = dict(os.environ)
    env["PATH"] = os.pathsep.join(path_dirs)
    proc = subprocess.run(
        [sys.executable, str(SCRIPT), "--out", str(out), *extra],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="backslashreplace",
        env=env,
        cwd=str(REPO),
        timeout=120,
    )
    return proc


with tempfile.TemporaryDirectory() as tmp:
    empty = Path(tmp) / "no_tools_here"
    empty.mkdir()
    out = Path(tmp) / "ci.svg"
    proc = run_cli(out, [], [str(empty)])
    print("missing-gh returncode:", proc.returncode)
    print("missing-gh traced-fail:", "[fail] ci-badge" in proc.stderr)
    print("missing-gh badge-written:", out.exists())
    print("missing-gh exit-line:", "[exit] ci-badge code=1" in proc.stderr)

with tempfile.TemporaryDirectory() as tmp:
    empty = Path(tmp) / "no_tools_here"
    empty.mkdir()
    out = Path(tmp) / "ci.svg"
    proc = run_cli(out, ["--offline"], [str(empty)])
    print("missing-gh-offline returncode:", proc.returncode)
    print("missing-gh-offline offline-line:", "[offline] ci-badge" in proc.stderr)
    sys.path.insert(0, str(REPO / "scripts"))
    import ci_badge
    from badgesvg import badge_svg

    want = badge_svg("CI", "unknown", color=ci_badge.VERDICT_COLORS["unknown"]).encode("utf-8")
    print("missing-gh-offline badge-is-grey-unknown:", "PASS" if out.read_bytes() == want else "FAIL")

live_out = REPO / "playground" / "adv78a" / "ci_live.svg"
proc = subprocess.run(
    [sys.executable, str(SCRIPT), "--out", str(live_out)],
    capture_output=True,
    text=True,
    encoding="utf-8",
    errors="backslashreplace",
    cwd=str(REPO),
    timeout=120,
)
print("live returncode:", proc.returncode)
print("live stdout:", proc.stdout.strip())
print("live exit-line:", "[exit] ci-badge code=0" in proc.stderr)

truth = subprocess.run(
    [
        "gh",
        "run",
        "list",
        "--workflow",
        "ci.yml",
        "--branch",
        "main",
        "--limit",
        "1",
        "--json",
        "status,conclusion,headSha,databaseId",
    ],
    capture_output=True,
    text=True,
    encoding="utf-8",
    errors="backslashreplace",
    cwd=str(REPO),
    timeout=60,
)
runs = json.loads(truth.stdout)
print("live ground-truth:", json.dumps(runs[0], sort_keys=True) if runs else "none")
if runs:
    print("live verdict-matches-truth:", "PASS" if f"ci: {ci_badge.verdict_of(runs[0])}" in proc.stdout else "FAIL")
