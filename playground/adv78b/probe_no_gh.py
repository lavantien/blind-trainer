import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPT = Path(r"C:\Users\lavantien\dev\github\blind-trainer\scripts\ci_badge.py")
D = HERE / "cases" / "no_gh_clean"
D.mkdir(parents=True, exist_ok=True)

for tag, argv in (("plain", []), ("offline", ["--offline"])):
    out = D / f"{tag}.svg"
    e = {
        "PATH": os.environ["WINDIR"] + "\\System32",
        "SYSTEMROOT": os.environ["SYSTEMROOT"],
        "COMSPEC": os.environ["COMSPEC"],
        "SYSTEMDRIVE": os.environ["SYSTEMDRIVE"],
    }
    p = subprocess.run(
        [sys.executable, str(SCRIPT), "--out", str(out), *argv],
        capture_output=True, text=True, encoding="utf-8", errors="backslashreplace", env=e, timeout=60,
    )
    print(f"{tag}: rc={p.returncode} svg_exists={out.exists()} stdout={p.stdout.strip().splitlines()[:1]}")
