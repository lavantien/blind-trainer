import json
import os
import sys

repo = os.getcwd().replace("\\", "/").lower()
export = sys.argv[1] if len(sys.argv) > 1 else "artifacts/coverage/coverage.json"
data = json.load(open(export, encoding="utf-8"))["data"][0]
names = []
for entry in data["files"]:
    raw = entry["filename"].replace("\\", "/")
    low = raw.lower()
    names.append(raw[len(repo) + 1:] if low.startswith(repo + "/") else raw)
export_c = {n for n in names if n.endswith(".c")}
print("files in export:", len(names), "c files:", len(export_c))
on_disk = []
for base in ("src", "tools", "tests/fuzz"):
    for dirpath, _, files in os.walk(base):
        for n in files:
            if n.endswith(".c"):
                on_disk.append(os.path.join(dirpath, n).replace("\\", "/"))
missing = sorted(p for p in on_disk if p not in export_c)
extra = sorted(p for p in export_c if not p.startswith(("src/", "tools/", "tests/fuzz/")))
print("c files on disk under src, tools, tests/fuzz:", len(on_disk))
print("missing from the export denominator:", len(missing))
for m in missing:
    print("  MISSING", m)
print("export files outside the claimed scope:", len(extra))
for e in extra[:20]:
    print("  EXTRA", e)
