import os
import subprocess
import sys
import tempfile
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parents[1]
GIT_STUB = (
    "@echo off\r\n"
    'echo %*>>"%~dp0git_calls.txt"\r\n'
    'if "%1"=="status" (if exist "%~dp0changed" echo  M docs/badges/ci.svg)\r\n'
    "exit /b 0\r\n"
)
EXPECTED_UNCHANGED = [
    "config user.name",
    "config user.email",
    "status --porcelain -- docs/badges/ci.svg",
]
EXPECTED_CHANGED = EXPECTED_UNCHANGED + [
    "add docs/badges/ci.svg",
    "commit -m",
    "push origin HEAD:main",
]


def workflow_steps():
    doc = yaml.safe_load(
        (REPO / ".github" / "workflows" / "ci.yml").read_text(encoding="utf-8")
    )
    steps = doc["jobs"]["badge"]["steps"]
    return steps[2]["run"], steps[3]["run"]


def run_pwsh(script, cwd, env):
    target = cwd / "probe_step.ps1"
    target.write_text(script, encoding="utf-8", newline="\r\n")
    return subprocess.run(
        ["pwsh", "-NoProfile", "-File", str(target)],
        cwd=str(cwd),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="backslashreplace",
        env=env,
        timeout=120,
    )


def scratch_repo(root):
    for rel in ("scripts/ci_badge.py", "scripts/badgesvg.py", "scripts/slog.py"):
        target = root / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes((REPO / rel).read_bytes())
    (root / "docs" / "badges").mkdir(parents=True, exist_ok=True)


def stubbed_env(root, changed):
    stub_dir = root / "gitstub"
    stub_dir.mkdir(parents=True, exist_ok=True)
    (stub_dir / "git.bat").write_text(GIT_STUB, encoding="ascii", newline="")
    if changed:
        (stub_dir / "changed").write_text("1", encoding="ascii")
    env = dict(os.environ)
    env["PATH"] = str(stub_dir) + os.pathsep + env["PATH"]
    return env, stub_dir


refresh_run, land_run = workflow_steps()
print("workflow badge step scripts extracted from ci.yml")
with tempfile.TemporaryDirectory() as tmp:
    for result in ("success", "failure", "cancelled"):
        root = Path(tmp) / f"refresh-{result}"
        scratch_repo(root)
        line = refresh_run.replace("${{ needs.ci.result }}", result)
        proc = run_pwsh(line, root, dict(os.environ))
        svg = (root / "docs" / "badges" / "ci.svg").read_text(encoding="utf-8")
        want = "passed" if result == "success" else "failed"
        print(
            f"refresh {result}: exit={proc.returncode} verdict={'CI: ' + want in svg}"
        )
for scenario in ("unchanged", "changed"):
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / scenario
        root.mkdir()
        env, stub_dir = stubbed_env(root, scenario == "changed")
        proc = run_pwsh(land_run, root, env)
        calls = (stub_dir / "git_calls.txt").read_text(encoding="utf-8").splitlines()
        expected = (
            EXPECTED_CHANGED if scenario == "changed" else EXPECTED_UNCHANGED
        )
        matched = all(any(call.startswith(part) for call in calls) for part in expected)
        extra = [c for c in calls if not any(c.startswith(p) for p in expected)]
        print(
            f"land {scenario}: exit={proc.returncode} calls={len(calls)} "
            f"expected_met={matched} unexpected={extra}"
        )
        if scenario == "changed":
            commit = [c for c in calls if c.startswith("commit")][0]
            print(f"commit message: {commit[commit.find('chore') :]}")
