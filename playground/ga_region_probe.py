import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
EXPORT = REPO / "artifacts" / "lane" / "lane-cov-ga" / "cov-build" / "export.json"


def main():
    wanted = sys.argv[1:] if len(sys.argv) > 1 else None
    payload = json.loads(EXPORT.read_text(encoding="utf-8"))
    rows = []
    for data in payload.get("data", []):
        for fn in data.get("functions", []):
            names = fn.get("filenames") or []
            if not names:
                continue
            rel = str(names[0]).replace("\\", "/")
            if "/ga/" not in rel and "/ga-run/" not in rel and "/memofreq/" not in rel:
                continue
            if "/cuda/" in rel:
                continue
            if wanted and not any(w in rel for w in wanted):
                continue
            for r in fn.get("regions", []):
                if int(r[4]) == 0 and int(r[5]) == 0 and int(r[7] or 0) in (0, 1):
                    rows.append((rel.split("/")[-1], r[0], r[1], r[2], r[3], fn["name"][:36]))
    for row in sorted(rows, key=lambda x: (x[0], x[1], x[2])):
        print(f"{row[0]:14} L{row[1]}:C{row[2]}-L{row[3]}:C{row[4]} {row[5]}")
    print(f"{len(rows)} uncovered code regions")


if __name__ == "__main__":
    sys.exit(main())
