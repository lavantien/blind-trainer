import argparse
import copy
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LANE = REPO / "artifacts" / "lane" / "lane-cov-verify"
BASE_CONFIG = LANE / "config.json"
COV_CONFIG = LANE / "config-cov.json"
PROFILE_PATTERN = "%p.profraw"
OWNED = re.compile(r"tools/(verify|visual-diff|release-verify|jobwrap|sleeper)/")
GROUPS = {
    "verify": ["verify"],
    "visual": ["visual"],
    "jobwrap": ["jobwrap"],
    "sleeper": ["sleeper"],
    "release": ["release"],
}
LLVM = Path(os.environ.get("BT_LLVM", r"C:\Program Files\LLVM\bin"))


def sh(argv, env=None, cwd=None, check=True):
    proc = subprocess.run(
        argv,
        cwd=cwd or REPO,
        env=env,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="backslashreplace",
    )
    if check and proc.returncode != 0:
        sys.stderr.write(proc.stdout[-4000:] + proc.stderr[-4000:])
        raise SystemExit(f"cov-scope: failed: {' '.join(map(str, argv[:6]))}")
    return proc


def write_cov_config():
    clone = json.loads(BASE_CONFIG.read_text(encoding="utf-8"))
    cov = copy.deepcopy(clone)
    flags = cov["build"]["cflags"]
    for flag in ("-fprofile-instr-generate", "-fcoverage-mapping"):
        if flag not in flags:
            flags.append(flag)
    dirs = cov["build"]["dirs"]
    dirs["bin"] = dirs["bin_cov"]
    dirs["obj"] = dirs["obj_cov"]
    COV_CONFIG.write_text(json.dumps(cov, indent=1) + "\n", encoding="utf-8", newline="\n")
    return cov


def rm_tree(path):
    shutil.rmtree(path, ignore_errors=True)


def run_suites(cov, groups):
    bin_cov = Path(cov["build"]["dirs"]["bin_cov"])
    profdata = LANE / "scoped.profdata"
    env = os.environ.copy()
    env["BT_CONFIG"] = str(COV_CONFIG)
    env["BT_REF_GROUND"] = "ref_ground"
    env["BT_ARTIFACTS"] = "artifacts"
    env["BT_BIN_DIR"] = str(bin_cov)
    for group in groups:
        profiles = LANE / f"prof-{group}"
        rm_tree(profiles)
        profiles.mkdir(parents=True, exist_ok=True)
        env["LLVM_PROFILE_FILE"] = str(profiles / PROFILE_PATTERN)
        for sub in GROUPS[group]:
            sh([sys.executable, "bt.py", "test-unit", "tools", "--only", sub], env=env)
    raws = sorted(LANE.glob("prof-*/*.profraw"))
    if not raws:
        raise SystemExit("cov-scope: no profraw files landed")
    sh([str(LLVM / "llvm-profdata.exe"), "merge", f"--output={profdata}", *[str(r) for r in raws]])
    exes = sorted(bin_cov.glob("*.exe"))
    argv = [str(LLVM / "llvm-cov.exe"), "export", f"--instr-profile={profdata}", str(exes[0])]
    for exe in exes[1:]:
        argv.append(f"--object={exe}")
    proc = sh(argv, check=False)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout[-2000:] + proc.stderr[-2000:])
        raise SystemExit("cov-scope: llvm-cov export failed")
    return proc.stdout


def file_rows(export):
    data = json.loads(export)["data"][0]
    rows = {}
    for entry in data["files"]:
        rel = str(entry["filename"]).replace("\\", "/")
        prefix = str(REPO).replace("\\", "/") + "/"
        if rel.lower().startswith(prefix.lower()):
            rel = rel[len(prefix):]
        if not OWNED.match(rel):
            continue
        regions = entry["summary"]["regions"]
        uncovered = []
        seen = set()
        for seg in entry.get("segments", []):
            line, col, count, has_count = seg[0], seg[1], seg[2], seg[3]
            if has_count and count == 0 and (line, col) not in seen:
                seen.add((line, col))
                uncovered.append(line)
        rows[rel] = {
            "covered": int(regions["covered"]),
            "count": int(regions["count"]),
            "lines": uncovered,
        }
    return rows


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="cov_scope_verify.py",
        description="build instrumented suites, run them, and report region coverage for the owned tool files",
        allow_abbrev=False,
    )
    parser.add_argument("--groups", default="verify,visual,jobwrap,sleeper,release")
    parser.add_argument("--skip-run", action="store_true", help="reuse the last export")
    args = parser.parse_args(argv)
    export_path = LANE / "scoped-export.json"
    if args.skip_run and export_path.is_file():
        export = export_path.read_text(encoding="utf-8")
    else:
        cov = write_cov_config()
        export = run_suites(cov, [g.strip() for g in args.groups.split(",") if g.strip()])
        export_path.write_text(export, encoding="utf-8")
    rows = file_rows(export)
    worst = sorted(rows.items(), key=lambda kv: kv[1]["count"] - kv[1]["covered"])
    for rel, row in worst:
        gap = row["count"] - row["covered"]
        pct = 100.0 * row["covered"] / row["count"] if row["count"] else 100.0
        lines = ",".join(map(str, row["lines"]))
        print(f"{rel}: {row['covered']}/{row['count']} ({pct:.1f}%) gap={gap} lines=[{lines}]")
    total_gap = sum(r["count"] - r["covered"] for r in rows.values())
    print(f"total gap: {total_gap} regions across {len(rows)} files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
