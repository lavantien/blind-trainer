import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
LLVM_COV = Path(r"C:\Program Files\LLVM\bin\llvm-cov.exe")
BIN = REPO / "build" / "bin-cov"
HUB = json.loads((REPO / "config" / "config.json").read_text(encoding="utf-8"))
INCLUDE = HUB["coverage"]["include_regex"]

extras = sorted(p for p in BIN.glob("*.exe") if not p.name.endswith("_test.exe"))
print(f"non-suite exes: {[p.name for p in extras]}")

argv = [
    str(LLVM_COV), "export", "-empty-profile",
    f"--include-filename-regex={INCLUDE}",
    str(extras[0]),
]
for exe in extras[1:]:
    argv.append(f"--object={exe}")
proc = subprocess.run(argv, cwd=REPO, capture_output=True, text=True, encoding="utf-8")
if proc.returncode != 0:
    print("export failed:", proc.stderr[:500])
    sys.exit(1)
data = json.loads(proc.stdout)["data"][0]
files = data["files"]
prefix = str(REPO).replace("\\", "/") + "/"
covered = 0
count = 0
for f in files:
    rel = f["filename"].replace("\\", "/")
    if rel.lower().startswith(prefix.lower()):
        rel = rel[len(prefix):]
    r = f["summary"]["regions"]
    covered += int(r["covered"])
    count += int(r["count"])
    print(f"  {rel}: {int(r['covered'])}/{int(r['count'])}")
print(f"regions only in tool and app exes: {covered}/{count}")

main_data = json.loads((REPO / "artifacts" / "coverage" / "coverage.json").read_text(encoding="utf-8"))["data"][0]
landed = main_data["totals"]["regions"]
true_total = int(landed["count"]) + count
true_covered = int(landed["covered"]) + covered
print(f"landed badge denominator: {int(landed['covered'])}/{int(landed['count'])}")
print(f"with tool and app exe regions appended: {true_covered}/{true_total} = {100.0 * true_covered / true_total:.2f}%")
