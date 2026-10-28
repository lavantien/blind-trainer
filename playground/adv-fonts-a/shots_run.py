import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
EXE = REPO / "build/bin/bt-app.exe"
OUT = REPO / "playground/adv-fonts-a"


def run_pass(tag):
    shots = OUT / tag / "shots"
    user = OUT / tag / "usage-user"
    if shots.exists():
        shutil.rmtree(shots)
    if user.exists():
        shutil.rmtree(user)
    shots.mkdir(parents=True)
    env = dict(os.environ)
    env["BT_REF_GROUND"] = "ref_ground"
    env["BT_CONFIG"] = str(REPO / "config/config.json")
    env["BT_ARTIFACTS"] = "artifacts"
    env["APPDATA"] = str(user)
    proc = subprocess.run([str(EXE), "--usage", str(shots)], cwd=str(REPO), env=env,
                          capture_output=True, text=True, timeout=300)
    print(f"[{tag}] usage exit={proc.returncode} shots={len(list(shots.glob('*.png')))}")
    if proc.returncode not in (0, 2):
        print(proc.stdout[-500:], proc.stderr[-500:])
    fatal_env = dict(env)
    fatal_env["BT_CONFIG"] = str(REPO / "artifacts/usage-absent-config.json")
    proc = subprocess.run([str(EXE), "--fatal-shot", str(shots)], cwd=str(REPO),
                          env=fatal_env, capture_output=True, text=True, timeout=300)
    print(f"[{tag}] fatal exit={proc.returncode} shots={len(list(shots.glob('*.png')))}")
    if proc.returncode not in (0, 2):
        print(proc.stdout[-500:], proc.stderr[-500:])
    return {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(shots.glob("*.png"))}


def main():
    first = run_pass("pass1")
    second = run_pass("pass2")
    print()
    print("== two-capture determinism ==")
    for name in first:
        if first[name] == second.get(name):
            print(name, "identical")
        else:
            print(name, "DIFFERS between captures")
    print()
    print("== fresh capture vs committed docs shot ==")
    for name in first:
        committed = REPO / "docs/screenshots" / name
        if not committed.is_file():
            print(name, "missing in docs")
            continue
        chash = hashlib.sha256(committed.read_bytes()).hexdigest()
        if chash == first[name]:
            print(name, "byte-identical to committed")
        else:
            print(name, "differs from committed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
