import json
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
LLVM_COV = Path(r"C:\Program Files\LLVM\bin\llvm-cov.exe")
BIN = REPO / "build" / "bin-cov"
HUB = json.loads((REPO / "config" / "config.json").read_text(encoding="utf-8"))
INCLUDE = HUB["coverage"]["include_regex"]
prefix = str(REPO).replace("\\", "/") + "/"

def rel_of(filename):
    rel = filename.replace("\\", "/")
    return rel[len(prefix):] if rel.lower().startswith(prefix.lower()) else rel

suites = sorted(p for p in BIN.glob("*_test.exe"))
argv = [str(LLVM_COV), "export", "-empty-profile", f"--include-filename-regex={INCLUDE}", str(suites[0])]
argv += [f"--object={exe}" for exe in suites[1:]]
proc = subprocess.run(argv, cwd=REPO, capture_output=True, text=True, encoding="utf-8")
static_files = {rel_of(f["filename"]): int(f["summary"]["regions"]["count"])
                for f in json.loads(proc.stdout)["data"][0]["files"]}

landed = json.loads((REPO / "artifacts" / "coverage" / "coverage.json").read_text(encoding="utf-8"))["data"][0]["files"]
landed_files = {rel_of(f["filename"]): int(f["summary"]["regions"]["count"]) for f in landed}

print(f"suite exes probed: {len(suites)}")
print(f"static file set from suite exes: {len(static_files)}")
print(f"landed export file set: {len(landed_files)}")
missing = sorted(set(static_files) - set(landed_files))
print(f"in suite exes but absent from landed export: {missing}")
mismatched = {k: (static_files[k], landed_files[k]) for k in static_files if k in landed_files and static_files[k] != landed_files[k]}
print(f"static region count mismatches: {mismatched}")
