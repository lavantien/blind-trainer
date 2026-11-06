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


def spawn(k, ref_ground, artifacts):
    env = os.environ.copy()
    env.pop("BT_CONFIG", None)
    env.pop("BT_REF_GROUND", None)
    env.pop("BT_ARTIFACTS", None)
    if ref_ground is not None:
        env["BT_REF_GROUND"] = ref_ground
    if artifacts is not None:
        env["BT_ARTIFACTS"] = artifacts
    env["BT_VERIFY_ALLOC_FAIL_OK"] = str(k)
    proc = subprocess.run([EXE], capture_output=True, text=True, env=env, timeout=180)
    first = proc.stderr.splitlines()[0] if proc.stderr else ""
    return proc.returncode, first, proc.stdout.strip().splitlines()


def classify(triple):
    code, first, out = triple
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
    return "err:" + first[:60]


def trimmed_clone():
    base = tempfile.mkdtemp(prefix="bt-seam-trim-")
    clone = os.path.join(base, "ground")
    shutil.copytree(os.path.join(ROOT, "ref_ground"), clone)
    for solver, sheet in SHEETS:
        path = os.path.join(clone, solver, sheet + ".tsv")
        with open(path, "r", encoding="utf-8", newline="") as handle:
            header = handle.readline()
        with open(path, "w", encoding="utf-8", newline="") as handle:
            handle.write(header)
    for extra in ("simcheck.tsv",):
        path = os.path.join(clone, extra)
        if os.path.isfile(path):
            with open(path, "r", encoding="utf-8", newline="") as handle:
                header = handle.readline()
            with open(path, "w", encoding="utf-8", newline="") as handle:
                handle.write(header)
    registry = os.path.join(clone, "packs", "registry.json")
    if os.path.isfile(registry):
        with open(registry, "w", encoding="utf-8", newline="") as handle:
            handle.write("[]")
    return base, clone


def sweep(ref_ground, artifacts, lo, hi):
    prev = None
    rows = []
    for k in range(lo, hi):
        triple = spawn(k, ref_ground, artifacts)
        bucket = classify(triple)
        rows.append((k, bucket, triple[1][:110]))
        if bucket != prev:
            print(f"k={k:5d} {bucket:20s} {triple[1][:110]}")
            prev = bucket
    return rows


def bucket_at(k, ref_ground, artifacts):
    return classify(spawn(k, ref_ground, artifacts))


def bisect_transition(lo, hi, ref_ground, artifacts):
    blo = bucket_at(lo, ref_ground, artifacts)
    bhi = bucket_at(hi, ref_ground, artifacts)
    if blo == bhi:
        return None
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if bucket_at(mid, ref_ground, artifacts) == blo:
            lo = mid
        else:
            hi = mid
    return lo, hi, blo, bucket_at(hi, ref_ground, artifacts)


def map_boundaries(artifacts, ceiling=1 << 20):
    points = [0]
    k = 1024
    while k <= ceiling:
        points.append(k)
        if bucket_at(k, None, artifacts) == "ok":
            break
        k *= 2
    print("samples:")
    for point in points:
        code, first, out = spawn(point, None, artifacts)
        print(f"  k={point:7d} {classify((code, first, out)):20s} {first[:90]}")
    print("transitions:")
    for lo, hi in zip(points, points[1:]):
        found = bisect_transition(lo, hi, None, artifacts)
        if found is not None:
            left, right, blo, bhi = found
            print(f"  [{left}, {right}) {blo} -> {bhi}")


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "trim"
    if mode == "trim":
        base, clone = trimmed_clone()
        try:
            sweep(clone, None, 0, 170)
        finally:
            shutil.rmtree(base, ignore_errors=True)
        return
    if mode == "gallop":
        k = 512
        while True:
            code, first, out = spawn(k, None, "artifacts")
            print(f"k={k} code={code} first={first[:90]}")
            if code == 0:
                print("happy threshold at or below", k)
                return
            k *= 2
            if k > 1 << 22:
                print("no happy threshold found")
                return
    if mode == "map":
        artifacts = None if len(sys.argv) > 2 and sys.argv[2] == "noenv" else "artifacts"
        map_boundaries(artifacts)


if __name__ == "__main__":
    main()
