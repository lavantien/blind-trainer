import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
EXE = REPO / "build" / "bin" / "bt-app.exe"
OUT = REPO / "artifacts" / "tmp-t80-usage"
USER = REPO / "artifacts" / "tmp-t80-user"
TIMEOUT_SECS = 300
EXPECTED_SHOTS = 20


def sandbox_env():
    env = os.environ.copy()
    env["APPDATA"] = str(USER)
    env["BT_CONFIG"] = str(REPO / "config" / "config.json")
    env["BT_REF_GROUND"] = "ref_ground"
    env["BT_ARTIFACTS"] = "artifacts"
    return env


def shot_is_distinct(path):
    from PIL import Image

    with Image.open(path) as image:
        raw = image.convert("RGB").tobytes()
    first = raw[:3]
    for at in range(3, len(raw) - 2, 3):
        if raw[at : at + 3] != first:
            return True
    return False


def main():
    for path in (OUT, USER):
        if path.is_dir():
            shutil.rmtree(path)
        path.mkdir(parents=True)
    started = time.time()
    try:
        proc = subprocess.run(
            [str(EXE), "--usage", str(OUT)],
            cwd=REPO,
            env=sandbox_env(),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="backslashreplace",
            timeout=TIMEOUT_SECS,
        )
    except subprocess.TimeoutExpired:
        print(f"FAIL tour exceeded {TIMEOUT_SECS}s")
        return 1
    elapsed = time.time() - started
    print(f"exit {proc.returncode} in {elapsed:.1f}s")
    if proc.returncode != 0:
        print("stdout tail:", proc.stdout[-2000:])
        print("stderr tail:", proc.stderr[-2000:])
        return 1
    pngs = sorted(OUT.glob("*.png"))
    print(f"{len(pngs)} shots: {[p.stem for p in pngs]}")
    if len(pngs) != EXPECTED_SHOTS:
        print(f"FAIL want {EXPECTED_SHOTS} shots, found {len(pngs)}")
        return 1
    replay = OUT / "replay.png"
    if not replay.is_file() or replay.stat().st_size <= 0:
        print("FAIL replay.png missing or empty")
        return 1
    if not shot_is_distinct(replay):
        print("FAIL replay.png is blank")
        return 1
    print(f"ok replay.png {replay.stat().st_size} bytes, distinct content")
    return 0


if __name__ == "__main__":
    code = main()
    for path in (OUT, USER):
        if path.is_dir():
            shutil.rmtree(path)
    print("throwaway dirs purged")
    sys.exit(code)
