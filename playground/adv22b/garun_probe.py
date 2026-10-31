import copy
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
EXE = REPO / "build" / "bin" / "ga-run.exe"

BASE = json.loads((REPO / "config" / "config.json").read_text(encoding="utf-8"))


def run_with_config(name, mutate):
    config = copy.deepcopy(BASE)
    mutate(config)
    path = OUT / f"garun-{name}.json"
    path.write_text(json.dumps(config, indent=2), encoding="utf-8")
    env = {
        "BT_CONFIG": str(path),
        "BT_ARTIFACTS": str(REPO / "artifacts"),
        "BT_REF_GROUND": str(REPO / "ref_ground"),
    }
    proc = subprocess.run([str(EXE)], capture_output=True, text=True, env=env, cwd=str(REPO),
                          timeout=300)
    print(f"{name} exit={proc.returncode}")
    for line in (proc.stderr or "").splitlines()[:6]:
        print(f"  stderr: {line[:160]}")
    for line in (proc.stdout or "").splitlines()[:6]:
        print(f"  stdout: {line[:160]}")
    return proc.returncode


def wrong_type(config):
    config["ga"]["islands"]["count"] = "2"


def wrap(config):
    config["ga"]["islands"]["count"] = 4611686018427387904


def abort_shaped_fraction(config):
    config["ga"]["elite_carry"] = {"policy": "fraction", "fraction": 0.004}


def abort_shaped_migration(config):
    config["ga"]["islands"] = {"count": 2, "migrate_every": 1, "migrate_count": 199}


def main():
    run_with_config("wrongtype", wrong_type)
    run_with_config("wrapcount", wrap)
    run_with_config("abortfrac", abort_shaped_fraction)
    run_with_config("abortmigr", abort_shaped_migration)
    return 0


if __name__ == "__main__":
    sys.exit(main())
