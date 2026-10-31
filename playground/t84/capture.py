import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
EXE = REPO / "build" / "bin" / "bt-app.exe"
OUT = Path(__file__).resolve().parent / "shots"
USER = Path(__file__).resolve().parent / "user"


def run():
    if OUT.is_dir():
        for png in OUT.glob("*.png"):
            png.unlink()
    OUT.mkdir(parents=True, exist_ok=True)
    if USER.is_dir():
        shutil.rmtree(USER)
    USER.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env["BT_CONFIG"] = str(REPO / "config" / "config.json")
    env["BT_REF_GROUND"] = "ref_ground"
    env["BT_ARTIFACTS"] = "artifacts"
    env["APPDATA"] = str(USER)
    proc = subprocess.run([str(EXE), "--usage", str(OUT)], cwd=REPO, env=env,
                          capture_output=True, text=True, encoding="utf-8",
                          errors="backslashreplace", timeout=300)
    print("exit", proc.returncode)
    if proc.returncode != 0:
        print(proc.stdout[-2000:])
        print(proc.stderr[-2000:])
        return 1
    for png in sorted(OUT.glob("*.png")):
        print(png.name, png.stat().st_size)
    return 0


if __name__ == "__main__":
    sys.exit(run())
