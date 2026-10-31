import json
import subprocess
import sys
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
EXPORT = REPO / "artifacts" / "coverage" / "coverage.json"

data = json.loads(EXPORT.read_text(encoding="utf-8"))["data"][0]
files = data["files"]
totals = data["totals"]["regions"]

covered = sum(int(f["summary"]["regions"]["covered"]) for f in files)
count = sum(int(f["summary"]["regions"]["count"]) for f in files)
print(f"files entries: {len(files)}")
print(f"recomputed from files: {covered}/{count} = {100.0 * covered / count:.4f}%")
print(f"totals block: {int(totals['covered'])}/{int(totals['count'])} = {totals['percent']}%")
print(f"badge rounding: {float(totals['percent']):.1f}%")

names = [f["filename"].replace("\\", "/") for f in files]
dups = [n for n, c in Counter(names).items() if c > 1]
print(f"duplicate filenames in export: {dups}")

repo_prefix = str(REPO).replace("\\", "/") + "/"
rel = [n[len(repo_prefix):] if n.lower().startswith(repo_prefix.lower()) else n for n in names]
print(f"files outside repo prefix: {[r for r in rel if r.startswith(('C:', '/', '~'))][:5]}")

tracked = subprocess.run(
    ["git", "ls-files", "src", "tools", "tests/fuzz"],
    cwd=REPO, capture_output=True, text=True, check=True,
).stdout.splitlines()
c_like = [t for t in tracked if t.endswith((".c", ".h"))]
sources = [t for t in c_like if t.endswith(".c")]
missing = sorted(set(sources) - set(rel))
extra = sorted(set(rel) - set(tracked))
print(f"tracked .c under src/tools/tests-fuzz: {len(sources)}")
print(f"missing from export ({len(missing)}): {missing}")
print(f"in export but untracked ({len(extra)}): {extra}")

cases = Counter()
for r in rel:
    top = r.split("/")[0]
    second = r.split("/")[1] if "/" in r[len(top):] else ""
    cases[f"{top}/{second}"] += 1
print("coverage by dir:")
for k in sorted(cases):
    print(f"  {k}: {cases[k]}")
