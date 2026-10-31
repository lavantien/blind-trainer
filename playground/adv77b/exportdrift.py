import json
import sys

old = json.load(open(sys.argv[1], encoding="utf-8"))["data"][0]
new = json.load(open("artifacts/coverage/coverage.json", encoding="utf-8"))["data"][0]
old_map = {e["filename"]: e for e in old["files"]}
new_map = {e["filename"]: e for e in new["files"]}
print("files old/new:", len(old_map), len(new_map))
print("file sets equal:", set(old_map) == set(new_map))
drift = []
for name in new_map:
    if name not in old_map:
        continue
    a = old_map[name]["summary"]["regions"]
    b = new_map[name]["summary"]["regions"]
    if a["covered"] != b["covered"] or a["count"] != b["count"]:
        drift.append((name.replace("\\", "/").split("/")[-1], a["covered"], b["covered"], a["count"], b["count"]))
print("files with region drift:", len(drift))
for row in drift[:20]:
    print("  DRIFT", row)
oa = old["totals"]["regions"]
nb = new["totals"]["regions"]
print(f"totals old {oa['covered']}/{oa['count']} new {nb['covered']}/{nb['count']}")
