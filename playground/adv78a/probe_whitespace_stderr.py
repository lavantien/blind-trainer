import os
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
STUB = "@echo off\r\necho. 1>&2\r\n@exit /b 1\r\n"


def run_case(tmp, extra):
    stub = Path(tmp) / "gh.bat"
    stub.write_text(STUB, encoding="ascii", newline="")
    out = Path(tmp) / "ci.svg"
    env = dict(os.environ)
    env["PATH"] = str(tmp) + os.pathsep + env.get("PATH", "")
    proc = subprocess.run(
        [sys.executable, str(REPO / "scripts" / "ci_badge.py"), "--out", str(out), *extra],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="backslashreplace",
        env=env,
        cwd=str(REPO),
        timeout=60,
    )
    return proc, out


with tempfile.TemporaryDirectory() as tmp:
    proc, out = run_case(tmp, [])
    print("plain returncode:", proc.returncode)
    print("plain traceback:", "IndexError" in proc.stderr)
    print("plain last-stderr-line:", proc.stderr.strip().splitlines()[-1] if proc.stderr.strip() else "<empty>")
    print("plain badge-written:", out.exists())

with tempfile.TemporaryDirectory() as tmp:
    proc, out = run_case(tmp, ["--offline"])
    print("offline returncode:", proc.returncode)
    print("offline traceback:", "IndexError" in proc.stderr)
    print("offline badge-written:", out.exists())
