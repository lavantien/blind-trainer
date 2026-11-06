import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LANE_DIR = REPO / "artifacts" / "lane" / "lane-cov-app"
LANE_CONFIG = LANE_DIR / "config.json"
COV_CONFIG = LANE_DIR / "cov-config.json"
COV_PROFILES = LANE_DIR / "cov-profiles"
COV_PROFDATA = LANE_DIR / "cov.profdata"
COV_EXPORT = LANE_DIR / "cov-export.json"
SCOPE_PATTERN = re.compile(r"[\\/]src[\\/]app[\\/]")


def sh(argv, env=None, capture=True):
    proc = subprocess.run(
        argv,
        cwd=REPO,
        env=env,
        capture_output=capture,
        text=True,
        encoding="utf-8",
        errors="backslashreplace",
    )
    if proc.returncode != 0:
        tail = ((proc.stderr or "") + (proc.stdout or ""))[-3000:]
        sys.stderr.write(f"cov_scope_app: command failed: {' '.join(map(str, argv))}\n{tail}\n")
        raise SystemExit(1)
    return proc


def write_cov_config():
    clone = json.loads(LANE_CONFIG.read_text(encoding="utf-8"))
    flags = clone["build"]["cflags"]
    for flag in ("-fprofile-instr-generate", "-fcoverage-mapping"):
        if flag not in flags:
            flags.append(flag)
    clone["build"]["test_timeout_secs"] = 900
    base = LANE_DIR / "covbuild"
    for key in ("bin", "bin_cov", "obj", "obj_cov", "gen", "test_tmp", "env"):
        clone["build"]["dirs"][key] = f"{base / key}".replace("\\", "/")
    clone["build"]["raylib_build"]["obj_dir"] = f"{base / 'raylib-obj'}".replace("\\", "/")
    COV_CONFIG.write_text(json.dumps(clone, indent=1) + "\n", encoding="utf-8", newline="\n")
    return COV_CONFIG


def run_suites(only):
    env = os.environ.copy()
    env["BT_CONFIG"] = str(COV_CONFIG)
    env["BT_BIN_DIR"] = str(LANE_DIR / "covbuild" / "bin")
    env["LLVM_PROFILE_FILE"] = str(COV_PROFILES / "suite.%p.profraw")
    argv = [sys.executable, "bt.py", "test-unit", "app"]
    if only:
        argv += ["--only", only]
    proc = sh(argv, env=env, capture=False if not only else True)
    return proc


def merge_and_export():
    profraws = sorted(COV_PROFILES.glob("*.profraw"))
    if not profraws:
        raise SystemExit("cov_scope_app: no profiles landed")
    sh(["llvm-profdata", "merge", f"--output={COV_PROFDATA}", *[str(p) for p in profraws]])
    bin_dir = LANE_DIR / "covbuild" / "bin"
    exes = sorted(bin_dir.glob("*.exe"))
    if not exes:
        raise SystemExit("cov_scope_app: no suite exes built")
    sep = "[\\\\/]"
    root = sep.join(re.escape(part) for part in str(REPO).replace("\\", "/").split("/"))
    argv = [
        "llvm-cov",
        "export",
        f"--instr-profile={COV_PROFDATA}",
        f"--include-filename-regex={root}{sep}src{sep}app{sep}",
        str(exes[0]),
    ]
    for exe in exes[1:]:
        argv.append(f"--object={exe}")
    proc = sh(argv)
    COV_EXPORT.write_text(proc.stdout + "\n", encoding="utf-8", newline="\n")


def report():
    data = json.loads(COV_EXPORT.read_text(encoding="utf-8"))["data"][0]
    files = {}
    for entry in data["files"]:
        name = str(entry["filename"]).replace("\\", "/")
        if "/src/app/" not in name:
            continue
        files[name.split("/src/app/")[-1]] = entry
    dark = {}
    for fn in data.get("functions", []):
        if int(fn.get("count", 0)) != 0:
            continue
        for name in fn.get("filenames") or []:
            fixed = str(name).replace("\\", "/")
            if "/src/app/" not in fixed:
                continue
            dark.setdefault(fixed.split("/src/app/")[-1], []).append(fn["name"])
    total_covered = 0
    total_count = 0
    lines = []
    for rel in sorted(files):
        entry = files[rel]
        regions = entry["summary"]["regions"]
        covered = int(regions["covered"])
        count = int(regions["count"])
        total_covered += covered
        total_count += count
        if covered == count:
            continue
        uncovered_ranges = []
        for fn in data.get("functions", []):
            names = [str(f).replace("\\", "/") for f in fn.get("filenames") or []]
            if not any(rel in n for n in names):
                continue
            for region in fn.get("regions", []):
                if int(region[4]) != 0 or int(region[6]) != 0 or int(region[7]) != 0:
                    continue
                region_file = names[int(region[5])] if int(region[5]) < len(names) else ""
                if not region_file.endswith("/" + rel) and region_file != rel:
                    continue
                uncovered_ranges.append((region[0], region[1], region[2], region[3]))
        uncovered_ranges = sorted(set(uncovered_ranges))
        shown = " ".join(f"{a}" if a == c else f"{a}-{c}" for a, b, c, d in uncovered_ranges[:40])
        lines.append(f"src/app/{rel}: {covered}/{count}")
        if uncovered_ranges:
            lines.append(f"  dark lines: {shown}")
        fns = dark.get(rel)
        if fns:
            lines.append(f"  dark fns: {', '.join(sorted(set(fns))[:8])}")
    percent = 100.0 * total_covered / total_count if total_count else 0.0
    print(f"src/app regions: {total_covered}/{total_count} ({percent:.1f}%)")
    for line in lines:
        print(line)
    return 0 if not lines else 2


def main():
    parser = argparse.ArgumentParser(
        prog="cov_scope_app.py",
        description="instrumented app-area run with per-file dark region report",
    )
    parser.add_argument("--only", default="", help="stem filter passed to test-unit")
    parser.add_argument("--report-only", action="store_true", help="skip build and run")
    parser.add_argument("--no-clean", action="store_true", help="keep existing profiles and append")
    args = parser.parse_args()
    write_cov_config()
    if not args.report_only:
        if not args.no_clean:
            for stale in (COV_PROFILES,):
                if stale.is_dir():
                    import shutil

                    shutil.rmtree(stale)
        COV_PROFILES.mkdir(parents=True, exist_ok=True)
        run_suites(args.only)
        merge_and_export()
    return report()


if __name__ == "__main__":
    sys.exit(main())
