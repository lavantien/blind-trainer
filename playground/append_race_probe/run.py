import os
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parent
children = 8
per_child = 500

def trial(mode, log_name):
    log = root / log_name
    if log.exists():
        log.unlink()
    procs = [
        subprocess.Popen(
            [sys.executable, "-I", str(root / "child.py"), mode, str(per_child), log_name],
            cwd=str(root),
        )
        for _ in range(children)
    ]
    for proc in procs:
        proc.wait()
    text = log.read_text(encoding="ascii", errors="replace")
    lines = [line for line in text.splitlines() if line.endswith("line-end")]
    intact = sum(1 for line in lines if line.split()[0].isdigit() and line.split()[1].isdigit())
    return len(text.splitlines()), intact

for mode in ("text", "os"):
    total, intact = trial(mode, f"log-{mode}.txt")
    expected = children * per_child
    print(f"mode={mode} lines_on_disk={total} intact={intact} expected={expected} lost={expected - intact}")
