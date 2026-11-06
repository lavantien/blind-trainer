import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.getcwd()
EXE = os.path.join(ROOT, "artifacts", "lane", "lane-cov-verify", "build", "bin", "verify.exe")
SHEETS = [
    ("cherry", "edge"), ("cherry", "corner"), ("cherry", "flips"), ("cherry", "twists"),
    ("cherry", "parity"), ("cherry", "ltct"), ("eggins", "edge"), ("eggins", "corner"),
    ("eggins", "flips"), ("eggins", "twists"), ("eggins", "parity"), ("eggins", "ltct"),
]


def header_only(path):
    with open(path, "r", encoding="utf-8", newline="") as handle:
        header = handle.readline()
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write(header)


def green_clone(base):
    clone = os.path.join(base, "ground")
    shutil.copytree(os.path.join(ROOT, "ref_ground"), clone)
    for solver, sheet in SHEETS:
        header_only(os.path.join(clone, solver, sheet + ".tsv"))
    header_only(os.path.join(clone, "simcheck.tsv"))
    return clone


def write_config(base, clone, artifacts):
    config = os.path.join(base, "minimal.json")
    body = {
        "paths": {
            "ref_ground": clone.replace("\\", "/"),
            "packs": os.path.join(clone, "packs").replace("\\", "/"),
            "artifacts": artifacts.replace("\\", "/"),
        }
    }
    with open(config, "w", encoding="utf-8", newline="") as handle:
        json.dump(body, handle)
    return config


def spawn(k, config, artifacts_env=None):
    env = os.environ.copy()
    for key in ("BT_CONFIG", "BT_REF_GROUND", "BT_ARTIFACTS"):
        env.pop(key, None)
    env["BT_CONFIG"] = config
    if artifacts_env is not None:
        env["BT_ARTIFACTS"] = artifacts_env
    env["BT_VERIFY_ALLOC_FAIL_OK"] = str(k)
    proc = subprocess.run([EXE], capture_output=True, text=True, env=env, timeout=180)
    lines = tuple(line for line in proc.stderr.splitlines() if line.strip())
    return proc.returncode, lines, proc.stdout.strip().splitlines()


def classify(result):
    code, lines, out = result
    first = lines[0] if lines else ""
    if code == 0:
        return "ok"
    if "config unreadable" in first:
        return "config-unreadable"
    if "serialize report.json" in first:
        return "serialize-failed"
    if first.startswith("verify: write") or first.startswith("verify: read"):
        return "io:" + first.split()[1]
    if first == "":
        return "silent-exit"
    return "err:" + first[:70]


def sweep(config, lo, hi):
    prev = None
    for k in range(lo, hi):
        result = spawn(k, config)
        bucket = classify(result)
        if bucket != prev:
            code, lines, out = result
            print(f"k={k:5d} {bucket:22s} code={code} {lines[0][:110] if lines else ''}")
            prev = bucket


def probe(config, lo, hi):
    prev = None
    for k in range(lo, hi):
        code, lines, out = spawn(k, config)
        if lines != prev:
            print(f"k={k:5d} code={code}")
            for line in lines[:4]:
                print(f"    {line[:135]}")
            prev = lines


def gallop(config, artifacts_env):
    k = 4096
    while k <= (1 << 21):
        code, lines, out = spawn(k, config, artifacts_env)
        first = lines[0] if lines else ""
        print(f"k={k:8d} code={code} {first[:110]}")
        if code == 0:
            return k
        k *= 2
    return None


def bisect(config, artifacts_env, lo, hi):
    bad = lo
    good = hi
    while good - bad > 1:
        mid = (bad + good) // 2
        code, _, _ = spawn(mid, config, artifacts_env)
        if code == 0:
            good = mid
        else:
            bad = mid
        print(f"  window [{bad}, {good})")
    return good


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "sweep"
    lo = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    hi = int(sys.argv[3]) if len(sys.argv) > 3 else 1200
    base = tempfile.mkdtemp(prefix="bt-seam-band-")
    try:
        artifacts = os.path.join(base, "artifacts")
        if mode in ("gallop", "fullprobe"):
            clone = os.path.join(ROOT, "ref_ground")
            config = write_config(base, clone, artifacts)
            if mode == "gallop":
                ceiling = gallop(config, artifacts)
                if ceiling is not None:
                    floor = bisect(config, artifacts, ceiling // 2, ceiling)
                    print(f"happy threshold {floor}")
            else:
                for k in range(lo, hi):
                    code, lines, out = spawn(k, config, artifacts)
                    print(f"k={k:8d} code={code}")
                    for line in lines[:3]:
                        print(f"    {line[:135]}")
        else:
            clone = green_clone(base)
            config = write_config(base, clone, artifacts)
            if mode == "probe":
                probe(config, lo, hi)
            else:
                sweep(config, lo, hi)
    finally:
        shutil.rmtree(base, ignore_errors=True)


if __name__ == "__main__":
    main()
