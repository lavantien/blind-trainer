import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
MUTANTS = {
    "ignore_gh_exit_code": (
        "if proc.returncode != 0:",
        "if proc.returncode == 0:",
    ),
    "drop_branch_filter": ('    "--branch",\n    "main",\n', ""),
    "drop_timed_out": ('    "timed_out",\n', ""),
    "drop_offline_detail": ("        detail = str(exc)\n", ""),
}


def build_scratch(root, mutant):
    for rel in ("scripts/ci_badge.py", "scripts/badgesvg.py", "scripts/slog.py"):
        target = root / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(REPO / rel, target)
    tests = root / "tests" / "python"
    tests.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(REPO / "tests" / "python" / "test_ci_badge.py", tests / "test_ci_badge.py")
    badges = root / "docs" / "badges"
    badges.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(REPO / "docs" / "badges" / "ci.svg", badges / "ci.svg")
    source = (root / "scripts" / "ci_badge.py").read_text(encoding="utf-8")
    old, new = MUTANTS[mutant]
    count = source.count(old)
    if count != 1:
        return f"mutant {mutant}: anchor found {count} times, expected 1"
    (root / "scripts" / "ci_badge.py").write_text(
        source.replace(old, new), encoding="utf-8"
    )
    return None


with tempfile.TemporaryDirectory() as tmp:
    for mutant in MUTANTS:
        root = Path(tmp) / mutant
        problem = build_scratch(root, mutant)
        if problem:
            print(problem)
            continue
        proc = subprocess.run(
            [sys.executable, "-m", "unittest", "discover", "-s", "tests/python"],
            cwd=str(root),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="backslashreplace",
            timeout=300,
        )
        tail = [line for line in proc.stderr.splitlines() if line.startswith(("OK", "FAILED"))]
        verdict = "killed" if proc.returncode != 0 else "SURVIVED"
        print(f"{mutant}: {verdict} ({' '.join(tail)})")
