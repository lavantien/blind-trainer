import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LANE = REPO / "artifacts" / "lane" / "lane-cov-verify"
TARGETS = {
    "tools/release-verify/exec.c": [20, 95, 96, 154, 177, 188, 276, 402, 403],
    "tools/release-verify/zip.c": [176, 177],
    "tools/release-verify/inflate.c": [346],
}

data = json.loads((LANE / "scoped-export.json").read_text(encoding="utf-8"))["data"][0]
for entry in data["files"]:
    rel = str(entry["filename"]).replace("\\", "/")
    prefix = str(REPO).replace("\\", "/") + "/"
    if rel.lower().startswith(prefix.lower()):
        rel = rel[len(prefix):]
    if rel not in TARGETS:
        continue
    wanted = set(TARGETS[rel])
    seen = set()
    for seg in entry.get("segments", []):
        line, col, count, has_count = seg[0], seg[1], seg[2], seg[3]
        is_gap = len(seg) > 5 and bool(seg[5])
        if has_count and count == 0 and not is_gap and line in wanted and (line, col) not in seen:
            seen.add((line, col))
            print(f"{rel}:{line}:{col}")
