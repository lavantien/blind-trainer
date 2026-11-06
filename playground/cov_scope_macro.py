import argparse
import glob
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LANE = REPO / "artifacts" / "lane" / "lane-macro-restruct"
PLAIN_CONFIG = LANE / "config.json"
COV_CONFIG = LANE / "cov-config.json"
COV_BIN = LANE / "build" / "cov-bin"
COV_OBJ = LANE / "build" / "cov-obj"
COV_PROF = LANE / "cov-prof"
PROFDATA = LANE / "cov.profdata"
REPORT = LANE / "cov-report.json"
FILE_FILTER = (
    "tests/unit/tools/",
    "tools/mutate/",
    "tools/mutants-gate/",
    "tools/policy/",
    "src/common/bt_harness.c",
)
AREA_RUNS = (
    ["tools"],
    ["common", "--only", "bt_harness"],
)
EXPORT_REGEX = (
    r"(tests[\\/]unit[\\/]tools[\\/]|tools[\\/]mutate[\\/]|"
    r"tools[\\/]mutants-gate[\\/]|tools[\\/]policy[\\/]|src[\\/]common[\\/]bt_harness\.c)"
)


def sh(argv, env, check=True):
    proc = subprocess.run(
        argv, cwd=REPO, env=env, capture_output=True, text=True,
        encoding="utf-8", errors="backslashreplace",
    )
    tail = (proc.stdout or "")[-800:] + (proc.stderr or "")[-800:]
    print(f"[run] {' '.join(str(a) for a in argv[:6])}... rc={proc.returncode}")
    if proc.returncode != 0:
        print(tail)
    if check and proc.returncode != 0:
        raise SystemExit(f"cov-scope-macro: command failed: {argv[0]}")
    return proc


def make_cov_config():
    clone = json.loads(PLAIN_CONFIG.read_text(encoding="utf-8"))
    clone["build"]["cflags"] = list(clone["build"]["cflags"]) + [
        "-fprofile-instr-generate",
        "-fcoverage-mapping",
    ]
    clone["build"]["dirs"]["obj"] = f"{COV_OBJ}".replace("\\", "/")
    clone["build"]["dirs"]["bin"] = f"{COV_BIN}".replace("\\", "/")
    COV_CONFIG.write_text(json.dumps(clone, indent=1) + "\n", encoding="utf-8", newline="\n")


def lane_env(profile_dir):
    env = os.environ.copy()
    env["BT_CONFIG"] = str(COV_CONFIG)
    env["BT_BIN_DIR"] = str(COV_BIN)
    env["LLVM_PROFILE_FILE"] = str(Path(profile_dir) / "run.%p.profraw")
    return env


def build_and_run():
    COV_PROF.mkdir(parents=True, exist_ok=True)
    for area_argv in AREA_RUNS:
        argv = [sys.executable, "bt.py", "test-unit", *area_argv]
        proc = sh(argv, lane_env(COV_PROF), check=False)
        if proc.returncode != 0:
            print(f"[retry] {' '.join(area_argv)} after rc={proc.returncode}")
            sh(argv, lane_env(COV_PROF), check=False)
            print(f"[retry] {' '.join(area_argv)} settled rc={proc.returncode}, continuing")


def merge_and_export():
    profraws = sorted(glob.glob(f"{COV_PROF}/*.profraw"))
    if not profraws:
        raise SystemExit("cov-scope-macro: no profraw files collected")
    sh(["llvm-profdata", "merge", f"--output={PROFDATA}", *profraws], os.environ.copy())
    exes = sorted(COV_BIN.glob("*.exe"))
    if not exes:
        raise SystemExit("cov-scope-macro: no exes under the cov bin")
    argv = [
        "llvm-cov", "export", f"--instr-profile={PROFDATA}",
        f"--include-filename-regex={EXPORT_REGEX}",
        str(exes[0]),
    ]
    for exe in exes[1:]:
        argv.append(f"--object={exe}")
    proc = sh(argv, os.environ.copy())
    payload = proc.stdout.strip()
    if not payload:
        raise SystemExit("cov-scope-macro: llvm-cov export empty")
    data = json.loads(payload)["data"][0]
    return data


def rel(path):
    text = str(path).replace("\\", "/")
    prefix = str(REPO).replace("\\", "/") + "/"
    return text[len(prefix):] if text.startswith(prefix) else text


def file_rows(data):
    rows = {}
    for entry in data["files"]:
        name = rel(entry["filename"])
        if not any(part in name + "/" for part in FILE_FILTER):
            continue
        regions = entry["summary"]["regions"]
        rows[name] = {
            "covered": int(regions["covered"]),
            "count": int(regions["count"]),
        }
    return rows


def print_report(rows):
    order = sorted(rows, key=lambda n: (-(rows[n]["count"] - rows[n]["covered"]), n))
    total_dark = 0
    for name in order:
        row = rows[name]
        dark_count = row["count"] - row["covered"]
        total_dark += dark_count
        pct = 100.0 * row["covered"] / row["count"] if row["count"] else 100.0
        print(f"{name}: {row['covered']}/{row['count']} ({pct:.1f}%) dark={dark_count}")
    print(f"total dark regions: {total_dark}")


def show_dark(rel_target):
    exes = sorted(COV_BIN.glob("*.exe"))
    if not exes:
        raise SystemExit("cov-scope-macro: no exes under the cov bin")
    argv = [
        "llvm-cov", "show", f"--instr-profile={PROFDATA}",
        "--show-branches=count",
        str(exes[0]), str(REPO / rel_target),
    ]
    for exe in exes[1:]:
        argv.append(f"--object={exe}")
    proc = subprocess.run(
        argv, cwd=REPO, capture_output=True, text=True,
        encoding="utf-8", errors="backslashreplace",
    )
    print(proc.stdout or "")


def main():
    parser = argparse.ArgumentParser(prog="cov_scope_macro.py")
    parser.add_argument("--keep-prof", action="store_true")
    parser.add_argument("--show", default=None, help="print the llvm-cov line view for one file")
    args = parser.parse_args()
    if args.show is not None:
        if not PROFDATA.is_file():
            raise SystemExit("cov-scope-macro: no profdata, run a measurement first with --keep-prof")
        show_dark(args.show)
        return 0
    make_cov_config()
    if COV_PROF.is_dir():
        shutil.rmtree(COV_PROF)
    build_and_run()
    data = merge_and_export()
    rows = file_rows(data)
    REPORT.write_text(json.dumps({"rows": rows}, indent=1), encoding="utf-8", newline="\n")
    print_report(rows)
    if not args.keep_prof:
        shutil.rmtree(COV_PROF, ignore_errors=True)
        PROFDATA.unlink(missing_ok=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
