import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ROOT = Path(__file__).resolve().parent
REAL_BASES = REPO / "tests" / "visual" / "baselines"
REAL_SHOTS = REPO / "artifacts" / "e2e"


def fresh(name):
    path = ROOT / name
    if path.is_dir():
        shutil.rmtree(path)
    path.mkdir(parents=True)
    return path


def copy_bases(name, drift):
    target = fresh(name)
    for png in sorted(REAL_BASES.glob("*.png")):
        shutil.copyfile(png, target / png.name)
    if drift:
        shutil.copyfile(REAL_BASES / "solve.png", target / "menu.png")
    return target


def hub(name, bases, artifacts, timeout=None):
    config = json.loads((REPO / "config" / "config.json").read_text(encoding="utf-8"))
    config["paths"]["baselines"] = str(bases).replace("\\", "/")
    config["paths"]["artifacts"] = str(artifacts).replace("\\", "/")
    if timeout is not None:
        config["e2e"]["timeout_secs"] = timeout
    path = ROOT / f"hub-{name}.json"
    path.write_text(json.dumps(config, indent=2), encoding="utf-8")
    return path


def run_visual(hub_path, label):
    env = dict(os.environ)
    env["BT_CONFIG"] = str(hub_path)
    env.pop("BT_ARTIFACTS", None)
    proc = subprocess.run(
        [sys.executable, "bt.py", "visual"],
        capture_output=True, text=True, env=env, cwd=str(REPO), timeout=540,
    )
    print(f"[{label}] exit={proc.returncode}")
    tail = (proc.stdout + proc.stderr).strip().splitlines()
    for line in tail[-14:]:
        print(f"[{label}]   {line}")
    return proc.returncode


def verdict_of(artifacts_dir):
    report = Path(artifacts_dir) / "visual" / "report.md"
    if not report.is_file():
        return None
    lines = [line for line in report.read_text(encoding="utf-8").splitlines() if line.strip()]
    return lines[-1] if lines else None


def case_drift():
    print("entry case_drift")
    bases = copy_bases("bases-drift", drift=True)
    arts = fresh("artifacts-drift")
    hub_path = hub("drift", bases, arts)
    code = run_visual(hub_path, "drift")
    print(f"exit case_drift code={code} verdict={verdict_of(arts)}")


def case_infra():
    print("entry case_infra")
    bases = copy_bases("bases-infra", drift=True)
    arts = fresh("artifacts-infra")
    hub_path = hub("infra", bases, arts, timeout=0)
    code = run_visual(hub_path, "infra")
    print(f"exit case_infra code={code} verdict={verdict_of(arts)}")


def case_flake():
    print("entry case_flake")
    shot = REAL_SHOTS / "menu.png"
    backup = ROOT / "menu.png.bak"
    shutil.copyfile(shot, backup)
    data = bytearray(shot.read_bytes())
    data[len(data) // 2] ^= 0xFF
    shot.write_bytes(data)
    bases = copy_bases("bases-flake", drift=False)
    arts = fresh("artifacts-flake")
    hub_path = hub("flake", bases, arts)
    try:
        code = run_visual(hub_path, "flake")
    finally:
        if shot.read_bytes() != backup.read_bytes():
            shutil.copyfile(backup, shot)
            print("[flake] restored corrupted shot from backup")
    print(f"exit case_flake code={code} verdict={verdict_of(arts)}")


def main():
    which = sys.argv[1] if len(sys.argv) > 1 else "all"
    print(f"entry probe_tgt_matrix case={which}")
    if which in ("all", "drift"):
        case_drift()
    if which in ("all", "infra"):
        case_infra()
    if which in ("all", "flake"):
        case_flake()
    print("exit probe_tgt_matrix 0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
