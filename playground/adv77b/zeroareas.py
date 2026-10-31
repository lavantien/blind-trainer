import json
import os
import sys

repo = os.getcwd().replace("\\", "/").lower()
data = json.load(open(sys.argv[1], encoding="utf-8"))["data"][0]
for entry in data["files"]:
    raw = entry["filename"].replace("\\", "/")
    low = raw.lower()
    rel = raw[len(repo) + 1:] if low.startswith(repo + "/") else raw
    if "/mutants-gate/" in rel or rel.startswith("tools/mutants-gate/"):
        r = entry["summary"]["regions"]
        print(f"{rel}: {int(r['covered'])}/{int(r['count'])} ({float(r['percent']):.1f}%)")
