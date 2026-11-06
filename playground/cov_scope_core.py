import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LANE = REPO / "artifacts" / "lane" / "lane-cov-core"
BASE_CONFIG = LANE / "config.json"
COV_CONFIG = LANE / "cov-config.json"
COV_BUILD = LANE / "cov-build"
RAW = LANE / "covraw"
PROFDATA = LANE / "cov.profdata"
EXPORT = LANE / "cov-export.json"
AREAS = ("core", "common", "scheme", "puzzles")
EXTRA_SUITES = (("data", "fuzz_db"), ("data", "fuzz_fsrs"), ("ga", "fuzz_ga"))
INCLUDE = r"src[\\\\/](core|common|scheme|puzzles)[\\\\/]|[\\\\/]tests[\\\\/]fuzz[\\\\/]"
AREA_PATTERN = re.compile(r"^(?:src|tools)/([^/]+)/")
FUZZ_PATTERN = re.compile(r"^tests/fuzz/")


def sh(argv, env=None, cwd=None, check=True):
    proc = subprocess.run(
        argv, cwd=cwd or REPO, env=env, capture_output=True, text=True,
        encoding="utf-8", errors="backslashreplace",
    )
    if check and proc.returncode != 0:
        sys.stdout.write(proc.stdout[-4000:])
        sys.stderr.write(proc.stderr[-4000:])
        raise SystemExit(f"cov-scope: command failed: {' '.join(map(str, argv[:6]))}")
    return proc


def write_cov_config():
    hub = json.loads(BASE_CONFIG.read_text(encoding="utf-8"))
    flags = hub["build"]["cflags"]
    for flag in ("-fprofile-instr-generate", "-fcoverage-mapping"):
        if flag not in flags:
            flags.append(flag)
    for key in ("bin", "obj", "gen", "test_tmp"):
        hub["build"]["dirs"][key] = f"{COV_BUILD / key}".replace("\\", "/")
    COV_CONFIG.write_text(json.dumps(hub, indent=1) + "\n", encoding="utf-8", newline="\n")


def run_suites(env):
    RAW.mkdir(parents=True, exist_ok=True)
    for stale in RAW.glob("*.profraw"):
        stale.unlink()
    for area in AREAS:
        sh([sys.executable, "bt.py", "test-unit", area], env=env)
    for area, stem in EXTRA_SUITES:
        sh([sys.executable, "bt.py", "test-unit", area, "--only", stem], env=env)


def merge_and_export(env):
    raws = sorted(RAW.glob("*.profraw"))
    if not raws:
        raise SystemExit("cov-scope: no profraw files landed")
    profdata = "llvm-profdata"
    llvm_cov = "llvm-cov"
    sh([profdata, "merge", f"--output={PROFDATA}", *[str(p) for p in raws]], env=env)
    exes = sorted((COV_BUILD / "bin").glob("*.exe"))
    if not exes:
        raise SystemExit("cov-scope: no suite exes under the cov bin")
    argv = [llvm_cov, "export", f"--instr-profile={PROFDATA}",
            f"--include-filename-regex={INCLUDE}", str(exes[0])]
    for exe in exes[1:]:
        argv.append(f"--object={exe}")
    proc = sh(argv, env=env, check=False)
    if proc.returncode != 0:
        sys.stderr.write(proc.stderr[-3000:])
        raise SystemExit("cov-scope: llvm-cov export failed")
    EXPORT.write_text(proc.stdout, encoding="utf-8", newline="\n")


def rel_path(filename):
    rel = str(filename).replace("\\", "/")
    prefix = str(REPO).replace("\\", "/") + "/"
    if rel.lower().startswith(prefix.lower()):
        return rel[len(prefix):]
    return rel


def report():
    data = json.loads(EXPORT.read_text(encoding="utf-8"))["data"][0]
    dead = {}
    for fn in data.get("functions", []):
        if int(fn.get("count", 0)) != 0:
            continue
        names = fn.get("filenames") or []
        if not names:
            continue
        dead.setdefault(rel_path(names[0]), []).append(fn["name"])
    rows = []
    for entry in data["files"]:
        rel = rel_path(entry["filename"])
        match = AREA_PATTERN.match(rel)
        area = match.group(1) if match else ("fuzz" if FUZZ_PATTERN.match(rel) else None)
        if area is None:
            continue
        regions = entry["summary"]["regions"]
        if int(regions["count"]) <= 0:
            continue
        rows.append((area, rel, int(regions["covered"]), int(regions["count"]),
                     sorted(dead.get(rel, []))))
    rows.sort(key=lambda r: (r[0], -(r[3] - r[2]), r[1]))
    total_missing = 0
    for area, rel, covered, count, fns in rows:
        missing = count - covered
        total_missing += missing
        flag = "DARK" if missing else "full"
        print(f"{flag:4} {area:8} {rel:34} {covered}/{count}")
        if missing and fns:
            head = ", ".join(fns[:4])
            print(f"     dead: {head}" + (" ..." if len(fns) > 4 else ""))
    print(f"files with gaps: {sum(1 for r in rows if r[3] - r[2])} of {len(rows)}")
    print(f"uncovered regions: {total_missing}")
    return 0 if total_missing == 0 else 1


def report_lines(pattern):
    data = json.loads(EXPORT.read_text(encoding="utf-8"))["data"][0]
    for entry in data["files"]:
        rel = rel_path(entry["filename"])
        if not re.search(pattern, rel):
            continue
        dark = []
        for seg in entry.get("segments", []):
            line, col, count, has_count, is_region = seg[0], seg[1], seg[2], seg[3], seg[4]
            if has_count and is_region and count == 0:
                dark.append(f"{line}:{col}")
        if dark:
            print(f"{rel}: {len(dark)} dark regions at {', '.join(dark)}")


def main():
    parser = argparse.ArgumentParser(prog="cov_scope_core.py")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--skip-export", action="store_true")
    parser.add_argument("--lines", default="", help="regex, print dark region positions per matched file")
    args = parser.parse_args()
    if args.lines:
        report_lines(args.lines)
        return 0
    write_cov_config()
    env = os.environ.copy()
    env["BT_CONFIG"] = str(COV_CONFIG)
    env["LLVM_PROFILE_FILE"] = str(RAW / "%p.profraw")
    if not args.skip_build:
        run_suites(env)
    if not args.skip_export:
        merge_and_export(env)
    return report()


if __name__ == "__main__":
    sys.exit(main())
