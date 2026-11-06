import argparse
import copy
import json
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LANE = REPO / "artifacts" / "lane" / "lane-cov-data"
COV = LANE / "cov"
PROFILE_DIR = COV / "profiles"
INCLUDE = "src[\\\\/]data[\\\\/]"
AREA = "data"


def sh(argv, **kw):
    proc = subprocess.run(argv, cwd=REPO, capture_output=True, text=True,
                          encoding="utf-8", errors="backslashreplace", **kw)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout[-4000:] + proc.stderr[-4000:])
        raise SystemExit(f"cov-scope: command failed: {' '.join(map(str, argv))}")
    return proc


def write_cov_config():
    hub = json.loads((REPO / "config" / "config.json").read_text(encoding="utf-8"))
    clone = copy.deepcopy(hub)
    for key in ("bin", "bin_cov", "obj", "obj_cov", "gen", "test_tmp", "env"):
        clone["build"]["dirs"][key] = f"{COV / key}".replace("\\", "/")
    clone["build"]["raylib_build"]["obj_dir"] = f"{COV / 'raylib-obj'}".replace("\\", "/")
    clone["build"]["cflags"] = [*clone["build"]["cflags"],
                                "-fprofile-instr-generate", "-fcoverage-mapping"]
    path = COV / "config.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(clone, indent=1) + "\n", encoding="utf-8", newline="\n")
    return path


def main():
    parser = argparse.ArgumentParser(prog="cov_scope_data.py")
    parser.add_argument("--keep-profiles", action="store_true")
    args = parser.parse_args()
    config = write_cov_config()
    PROFILE_DIR.mkdir(parents=True, exist_ok=True)
    env = {
        **os.environ,
        "BT_CONFIG": str(config),
        "BT_BIN_DIR": str(COV / "bin"),
        "LLVM_PROFILE_FILE": str(PROFILE_DIR / f"{AREA}.%p.profraw"),
    }
    proc = subprocess.run(
        [sys.executable, "bt.py", "test-unit", AREA], cwd=REPO, env=env,
        capture_output=True, text=True, encoding="utf-8", errors="backslashreplace",
    )
    tail = (proc.stdout or "")[-800:]
    if proc.returncode != 0:
        sys.stderr.write((proc.stdout or "")[-6000:] + (proc.stderr or "")[-6000:])
        raise SystemExit(f"cov-scope: test-unit {AREA} failed\n{tail}")
    profraws = sorted(PROFILE_DIR.glob("*.profraw"))
    if not profraws:
        raise SystemExit("cov-scope: no profraws landed")
    profdata = COV / "coverage.profdata"
    sh(["llvm-profdata", "merge", f"--output={profdata}", *map(str, profraws)])
    exes = sorted((COV / "bin").glob("*.exe"))
    export = sh(["llvm-cov", "export", f"--instr-profile={profdata}",
                 f"--include-filename-regex={INCLUDE}", str(exes[0]),
                 *[f"--object={e}" for e in exes[1:]]])
    data = json.loads(export.stdout)["data"][0]
    rows = []
    dead_lines = {}
    for entry in data["files"]:
        rel = str(entry["filename"]).replace("\\", "/")
        marker = "/src/data/"
        if marker not in rel:
            continue
        regions = entry["summary"]["regions"]
        rows.append((rel, int(regions["covered"]), int(regions["count"])))
        counts = {}
        for seg in entry.get("segments", []):
            line, col, count = seg[0], seg[1], seg[2]
            counts[line] = max(counts.get(line, 0), int(count))
        dead = [line for line, count in sorted(counts.items()) if count == 0]
        if dead:
            dead_lines[rel] = dead
    rows.sort()
    worst = 0
    for rel, covered, count in rows:
        pct = 100.0 * covered / count if count else 100.0
        flag = "" if covered == count else "  GAP"
        if covered != count:
            worst += count - covered
        print(f"{pct:6.1f}%  {covered:4d}/{count:<4d} {rel}{flag}")
        if rel in dead_lines:
            packed = []
            for line in dead_lines[rel]:
                if packed and line == packed[-1][1] + 1:
                    packed[-1][1] = line
                else:
                    packed.append([line, line])
            print(f"          dead lines: {', '.join(f'{a}-{b}' if a != b else f'{a}' for a, b in packed)}")
    print(f"total uncovered in scope: {worst} regions across {sum(1 for r in rows if r[1] != r[2])} files")
    if not args.keep_profiles:
        for raw in profraws:
            raw.unlink(missing_ok=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
