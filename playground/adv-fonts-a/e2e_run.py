import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DRIVER = REPO / "build/bin/bt-e2e.exe"
APP = REPO / "build/bin/bt-app.exe"
OUT = REPO / "playground/adv-fonts-a"


def run_pass(tag):
    shots = OUT / tag / "e2e"
    user = OUT / tag / "e2e-user"
    for path in (shots, user):
        if path.exists():
            shutil.rmtree(path)
        path.mkdir(parents=True)
    env = dict(os.environ)
    env["BT_REF_GROUND"] = "ref_ground"
    env["BT_CONFIG"] = str(REPO / "config/config.json")
    env["BT_ARTIFACTS"] = "artifacts"
    env["APPDATA"] = str(user)
    proc = subprocess.run([str(DRIVER), str(APP), str(shots)], cwd=str(REPO), env=env,
                          capture_output=True, text=True, timeout=600)
    print(f"[{tag}] e2e exit={proc.returncode} shots={len(list(shots.glob('*.png')))}")
    if proc.returncode != 0:
        print((proc.stdout or "")[-800:])
        print((proc.stderr or "")[-800:])
    return {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(shots.glob("*.png"))}


def main():
    first = run_pass("e2e1")
    second = run_pass("e2e2")
    print()
    print("== e2e two-capture determinism ==")
    for name in sorted(set(first) | set(second)):
        if first.get(name) == second.get(name):
            print(name, "identical")
        else:
            print(name, "DIFFERS")
    print()
    print("== fresh e2e capture vs committed baseline ==")
    for name in sorted(first):
        base = REPO / "tests/visual/baselines" / name
        if not base.is_file():
            print(name, "no baseline file")
            continue
        bhash = hashlib.sha256(base.read_bytes()).hexdigest()
        print(name, "byte-identical" if bhash == first[name] else "differs from baseline")
    return 0


if __name__ == "__main__":
    sys.exit(main())
