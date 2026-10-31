import json
import os
import sys

repo = os.getcwd().replace("\\", "/").lower()
export = sys.argv[1] if len(sys.argv) > 1 else "artifacts/coverage/coverage.json"
data = json.load(open(export, encoding="utf-8"))["data"][0]
buckets = {}
for entry in data["files"]:
    raw = entry["filename"].replace("\\", "/")
    low = raw.lower()
    rel = raw[len(repo) + 1:] if low.startswith(repo + "/") else raw
    if rel.startswith("tests/unit/"):
        bucket = "tests/unit (leaked test code)"
    elif rel.startswith("tests/fuzz/"):
        bucket = "tests/fuzz (claimed)"
    elif rel.startswith(("src/", "tools/")):
        bucket = "src+tools (claimed)"
    else:
        bucket = "other"
    regions = entry["summary"]["regions"]
    row = buckets.setdefault(bucket, {"files": 0, "covered": 0, "count": 0})
    row["files"] += 1
    row["covered"] += int(regions["covered"])
    row["count"] += int(regions["count"])
for name in sorted(buckets):
    row = buckets[name]
    pct = 100.0 * row["covered"] / row["count"] if row["count"] else 0.0
    print(f"{name}: files={row['files']} regions={row['covered']}/{row['count']} ({pct:.1f}%)")
claimed = buckets.get("src+tools (claimed)", {"covered": 0, "count": 0})
fuzz = buckets.get("tests/fuzz (claimed)", {"covered": 0, "count": 0})
sc = claimed["covered"] + fuzz["covered"]
sn = claimed["count"] + fuzz["count"]
print(f"stated scope only (src+tools+tests/fuzz): {sc}/{sn} ({100.0 * sc / sn:.1f}%)")
