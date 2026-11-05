import json
import re
import sys
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
EXPORT = REPO / "artifacts" / "coverage" / "coverage.json"
OUT = REPO / "artifacts" / "coverage" / "gaps.tsv"
AREA_PATTERN = re.compile(r"^(?:src|tools)/([^/]+)/")
FUZZ_PATTERN = re.compile(r"^tests/fuzz/")


def rel_path(filename):
    rel = str(filename).replace("\\", "/")
    prefix = str(REPO).replace("\\", "/") + "/"
    if rel.lower().startswith(prefix.lower()):
        return rel[len(prefix):]
    return rel


def main():
    data = json.loads(EXPORT.read_text(encoding="utf-8"))["data"][0]
    dead = defaultdict(list)
    for fn in data.get("functions", []):
        if int(fn.get("count", 0)) != 0:
            continue
        names = fn.get("filenames") or []
        if not names:
            continue
        dead[rel_path(names[0])].append(fn["name"])
    rows = []
    for entry in data["files"]:
        rel = rel_path(entry["filename"])
        regions = entry["summary"]["regions"]
        count = int(regions["count"])
        covered = int(regions["covered"])
        if count <= 0 or covered == count:
            continue
        area = AREA_PATTERN.match(rel)
        area = area.group(1) if area else ("fuzz" if FUZZ_PATTERN.match(rel) else "?")
        rows.append((area, rel, covered, count, sorted(dead.get(rel, []))))
    rows.sort(key=lambda r: (r[0], -(r[3] - r[2]), r[1]))
    with OUT.open("w", encoding="utf-8", newline="\n") as sink:
        sink.write("area\tfile\tcovered\tcount\tuncovered_functions\n")
        for area, rel, covered, count, fns in rows:
            sink.write(f"{area}\t{rel}\t{covered}\t{count}\t{';'.join(fns)}\n")
    by_area = defaultdict(lambda: [0, 0])
    for area, rel, covered, count, fns in rows:
        by_area[area][0] += count - covered
        by_area[area][1] += 1
    print(f"{len(rows)} files with gaps, written to {OUT}")
    for area in sorted(by_area, key=lambda a: -by_area[a][0]):
        missing, files = by_area[area]
        print(f"{area}: {missing} uncovered regions across {files} files")


if __name__ == "__main__":
    sys.exit(main())
