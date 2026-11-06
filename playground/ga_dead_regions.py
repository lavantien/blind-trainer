import json
import sys
from pathlib import Path

EXPORT = Path(sys.argv[1] if len(sys.argv) > 1 else "artifacts/lane/lane-cov-ga/cov-build/export.json")
PROBES = ("ga-run/out.c", "ga-run/lib.c", "memofreq/lib.c", "memofreq/main.c", "ga/ga.c", "ga/inventory.c", "ga/dist.c")

payload = json.loads(EXPORT.read_text(encoding="utf-8"))
for data in payload.get("data", []):
    for fn in data.get("functions", []):
        names = fn.get("filenames") or []
        if not names:
            continue
        rel = str(names[0]).replace("\\", "/")
        if not any(rel.endswith(probe) for probe in PROBES):
            continue
        dead = [r for r in fn.get("regions", []) if int(r[4]) == 0 and len(r) > 7 and int(r[7]) == 0]
        if dead:
            lines = sorted(set(int(r[0]) for r in dead))
            name = fn["name"].split(":")[-1]
            print(f"{fn['count']:>8}  {rel.split('trainer/')[-1]:<28} {name[:36]:<36} dead {len(dead):>3} at {lines[:16]}")
