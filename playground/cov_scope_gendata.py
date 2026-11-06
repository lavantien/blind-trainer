import copy
import json
import os
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]


def tool_fail(message):
    print(f"cov-scope-gendata: {message}", file=sys.stderr)
lane = root / "artifacts/lane/lane-cov-gendata"
plain_config = lane / "config.json"
cov_config = lane / "cov-config.json"
cov_dir = lane / "cov-build"
profraw_dir = lane / "profraw"
profdata = lane / "gendata.profdata"
llvm = "C:/Program Files/LLVM/bin"


def write_cov_config():
    hub = json.loads(plain_config.read_text(encoding="utf-8"))
    clone = copy.deepcopy(hub)
    clone["build"]["cflags"] = [*clone["build"]["cflags"], "-fprofile-instr-generate",
                                "-fcoverage-mapping"]
    clone["build"]["test_timeout_secs"] = 1800
    for key in ("bin", "obj", "gen", "test_tmp"):
        clone["build"]["dirs"][key] = f"{cov_dir / key}".replace("\\", "/")
    cov_config.write_text(json.dumps(clone, indent=1) + "\n", encoding="utf-8", newline="\n")


def run(argv, label, env=None, cwd=root, check=True):
    proc = subprocess.run(argv, cwd=str(cwd), capture_output=True, text=True,
                          env={**os.environ, **(env or {})})
    if check and proc.returncode != 0:
        tool_fail(f"{label} failed: {(proc.stderr or proc.stdout)[-800:]}")
        raise SystemExit(1)
    return proc


def main():
    write_cov_config()
    env = {
        "BT_CONFIG": str(cov_config),
        "BT_BIN_DIR": str(cov_dir / "bin"),
    }
    run([sys.executable, "bt.py", "test-unit", "tools", "--only", "gendata"], "build", env=env)
    exes = sorted((cov_dir / "bin").glob("*gendata*_test.exe"))
    if not exes:
        tool_fail("no instrumented gendata suite exes")
        raise SystemExit(1)
    profraw_dir.mkdir(parents=True, exist_ok=True)
    for stale in profraw_dir.glob("*.profraw"):
        stale.unlink()
    child_env = {
        **os.environ,
        "BT_CONFIG": "",
        "LLVM_PROFILE_FILE": str(profraw_dir / "%p.profraw"),
        "BT_BIN_DIR": str(cov_dir / "bin"),
        "BT_REF_GROUND": "ref_ground",
        "BT_ARTIFACTS": "artifacts",
    }
    failures = []
    for exe in exes:
        proc = run([str(exe)], f"run {exe.name}", env=child_env, check=False)
        if proc.returncode != 0:
            failures.append(f"{exe.name} exit {proc.returncode}: {proc.stdout[-400:]}")
    if failures:
        for line in failures:
            tool_fail(line)
        raise SystemExit(1)
    profraws = sorted(profraw_dir.glob("*.profraw"))
    run([f"{llvm}/llvm-profdata.exe", "merge", f"--output={profdata}", *map(str, profraws)],
        "merge")
    proc = run(
        [f"{llvm}/llvm-cov.exe", "export", f"--instr-profile={profdata}",
         "--include-filename-regex=tools[\\\\/]gendata", *map(str, exes)], "export")
    data = json.loads(proc.stdout)["data"][0]
    print(f"{'file':28} {'regions':>14}")
    total_dark = 0
    for entry in data["files"]:
        rel = entry["filename"].replace("\\", "/").split("tools/gendata/")[-1]
        regions = entry["summary"]["regions"]
        dark = int(regions["count"]) - int(regions["covered"])
        total_dark += dark
        print(f"{rel:28} {int(regions['covered']):>6}/{int(regions['count']):<7} dark={dark}")
        if dark:
            dark_lines = sorted({seg[0] for seg in entry["segments"]
                                 if len(seg) > 2 and seg[2] == 0})
            if dark_lines:
                print(f"  dark lines: {dark_lines}")
    print(f"total dark regions: {total_dark}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
