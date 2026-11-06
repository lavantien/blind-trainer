import copy
import datetime
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LANE = REPO / "artifacts" / "lane" / "lane-cov-ga"
COV = LANE / "cov-build"
SUITES = [["ga"], ["tools", "--only", "ga_run_lib"], ["tools", "--only", "ga_run_out"],
          ["tools", "--only", "ga_run_flow"], ["tools", "--only", "ga_run_inputs"],
          ["tools", "--only", "memofreq"]]
OWNED = (re.compile(r"src[\\/]ga[\\/]"), re.compile(r"tools[\\/]ga-run[\\/]"),
         re.compile(r"tools[\\/]memofreq[\\/]"))
COVERAGE_FLAGS = ["-fprofile-instr-generate", "-fcoverage-mapping"]


def stamp():
    return datetime.datetime.now().strftime("%H:%M:%S")


def line(tag, text):
    print(f"{stamp()} [{tag}] {text}", flush=True)


def fail(tag, text):
    line(tag, text)
    raise SystemExit(1)


def write_cov_config():
    hub = json.loads((LANE / "config.json").read_text(encoding="utf-8"))
    clone = copy.deepcopy(hub)
    clone["build"]["cflags"] = [*clone["build"]["cflags"], *COVERAGE_FLAGS]
    for key in ("bin", "bin_cov", "obj", "obj_cov", "gen", "test_tmp"):
        clone["build"]["dirs"][key] = f"{COV / key}".replace("\\", "/")
    clone["build"]["raylib_build"]["obj_dir"] = f"{COV / 'raylib-obj'}".replace("\\", "/")
    COV.mkdir(parents=True, exist_ok=True)
    (COV / "config.json").write_text(json.dumps(clone, indent=1) + "\n", encoding="utf-8")
    return COV / "config.json"


def run_suites(cov_config):
    env = dict(os.environ)
    env["BT_CONFIG"] = str(cov_config)
    env["BT_BIN_DIR"] = str(COV / "bin")
    env["LLVM_PROFILE_FILE"] = str(COV / "profiles" / "%p.profraw")
    profiles = COV / "profiles"
    shutil.rmtree(profiles, ignore_errors=True)
    profiles.mkdir(parents=True, exist_ok=True)
    for suite in SUITES:
        proc = subprocess.run(
            [sys.executable, str(REPO / "bt.py"), "test-unit", *suite],
            cwd=REPO, env=env, capture_output=True, text=True, encoding="utf-8",
            errors="backslashreplace",
        )
        tail = (proc.stdout or "")[-600:] + (proc.stderr or "")[-600:]
        if proc.returncode != 0:
            fail("suite", f"test-unit {' '.join(suite)} failed:\n{tail}")
        line("suite", f"test-unit {' '.join(suite)} green")


def merge_and_export():
    profraws = sorted((COV / "profiles").glob("*.profraw"))
    if not profraws:
        fail("merge", "no profraw files collected")
    profdata = COV / "coverage.profdata"
    proc = subprocess.run(
        [shutil.which("llvm-profdata"), "merge", f"--output={profdata}",
         *[str(p) for p in profraws]],
        cwd=REPO, capture_output=True, text=True,
    )
    if proc.returncode != 0:
        fail("merge", proc.stderr[-500:])
    exes = sorted((COV / "bin").glob("*.exe"))
    if not exes:
        fail("export", "no exes built")
    root_pattern = "[\\\\/]".join(re.escape(part.rstrip("\\/")) for part in REPO.parts if part)
    include = (
        f"{root_pattern}[\\\\/](src[\\\\/]ga[\\\\/]|tools[\\\\/]ga-run[\\\\/]|"
        f"tools[\\\\/]memofreq[\\\\/])"
    )
    argv = [shutil.which("llvm-cov"), "export", f"--instr-profile={profdata}",
            f"--include-filename-regex={include}", str(exes[0])]
    for exe in exes[1:]:
        argv.append(f"--object={exe}")
    proc = subprocess.run(argv, cwd=REPO, capture_output=True, text=True, encoding="utf-8",
                          errors="backslashreplace")
    if proc.returncode != 0 or not (proc.stdout or "").strip():
        fail("export", (proc.stderr or "no output")[-500:])
    (COV / "export.json").write_text(proc.stdout, encoding="utf-8")
    line("export", f"{len(exes)} exes, {len(profraws)} profraws")


def owned_rel(filename):
    rel = str(filename).replace("\\", "/")
    prefix = str(REPO).replace("\\", "/") + "/"
    if rel.lower().startswith(prefix.lower()):
        rel = rel[len(prefix):]
    for pattern in OWNED:
        if pattern.search(rel + "/") or pattern.search(rel):
            if "/cuda/" in rel:
                return None
            return rel
    return None


def report():
    payload = json.loads((COV / "export.json").read_text(encoding="utf-8"))
    files = {}
    for data in payload.get("data", []):
        for entry in data["files"]:
            rel = owned_rel(entry["filename"])
            if rel is None:
                continue
            regions = entry["summary"]["regions"]
            row = files.setdefault(rel, {"covered": 0, "count": 0, "lines": set()})
            row["covered"] += int(regions["covered"])
            row["count"] += int(regions["count"])
        for fn in data.get("functions", []):
            names = fn.get("filenames") or []
            if not names:
                continue
            rel = owned_rel(names[0])
            if rel is None:
                continue
            for region in fn.get("regions", []):
                if int(region[4]) == 0 and int(region[5]) == 0 and int(region[7] or 0) in (0, 1):
                    files[rel]["lines"].add(int(region[0]))
    total_missing = 0
    for rel in sorted(files):
        row = files[rel]
        missing = row["count"] - row["covered"]
        total_missing += missing
        pct = 100.0 * row["covered"] / row["count"] if row["count"] else 100.0
        line("file", f"{rel}: {row['covered']}/{row['count']} ({pct:.1f}%) missing {missing}")
        if row["lines"]:
            wanted = sorted(row["lines"])
            line("lines", f"{rel}: uncovered start lines {wanted}")
    line("total", f"{total_missing} uncovered regions remain")


def main():
    line("entry", "cov_scope_ga")
    cov_config = write_cov_config()
    run_suites(cov_config)
    merge_and_export()
    report()
    line("exit", "cov_scope_ga done")


if __name__ == "__main__":
    sys.exit(main())
