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
    if rel.lower().startswith(prefix.lower()):
        rel = rel[len(prefix):]
    return rel

extras = sorted(p for p in BIN.glob("*.exe") if not p.name.endswith("_test.exe"))
argv = [str(LLVM_COV), "export", "-empty-profile", f"--include-filename-regex={INCLUDE}", str(extras[0])]
argv += [f"--object={exe}" for exe in extras[1:]]
tool_files = {}
for f in json.loads(subprocess.run(argv, cwd=REPO, capture_output=True, text=True, encoding="utf-8").stdout)["data"][0]["files"]:
    r = f["summary"]["regions"]
    tool_files[rel_of(f["filename"])] = int(r["count"])

landed_files = {}
for f in json.loads((REPO / "artifacts" / "coverage" / "coverage.json").read_text(encoding="utf-8"))["data"][0]["files"]:
    r = f["summary"]["regions"]
    landed_files[rel_of(f["filename"])] = (int(r["covered"]), int(r["count"]))

only_in_tools = {k: v for k, v in tool_files.items() if k not in landed_files}
print("files present only in tool and app exes (absent from the landed export):")
total = 0
for k in sorted(only_in_tools):
    print(f"  {k}: {only_in_tools[k]} regions")
    total += only_in_tools[k]
print(f"omitted regions total: {total}")

lc, ln = sum(v[0] for v in landed_files.values()), sum(v[1] for v in landed_files.values())
print(f"landed: {lc}/{ln}")
print(f"stated scope with mains appended at 0 covered: {lc}/{ln + total} = {100.0 * lc / (ln + total):.2f}%")
