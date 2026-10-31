import json
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
data = json.loads((REPO / "artifacts" / "coverage" / "coverage.json").read_text(encoding="utf-8"))["data"][0]
prefix = str(REPO).replace("\\", "/") + "/"

rows = []
for f in data["files"]:
    rel = f["filename"].replace("\\", "/")
    if rel.lower().startswith(prefix.lower()):
        rel = rel[len(prefix):]
    r = f["summary"]["regions"]
    rows.append((rel, int(r["covered"]), int(r["count"])))

unit = [r for r in rows if r[0].startswith("tests/unit")]
fuzz = [r for r in rows if r[0].startswith("tests/fuzz")]
src = [r for r in rows if r[0].startswith("src/")]
tools = [r for r in rows if r[0].startswith("tools/")]

def pct(rows):
    c = sum(r[1] for r in rows)
    n = sum(r[2] for r in rows)
    return c, n, (100.0 * c / n if n else 0.0)

for name, group in (("all", rows), ("src", src), ("tools", tools), ("tests/unit", unit), ("tests/fuzz", fuzz)):
    c, n, p = pct(group)
    print(f"{name}: {c}/{n} = {p:.2f}%")

c, n, p = pct([r for r in rows if not r[0].startswith("tests/unit")])
print(f"stated scope (src+tools+fuzz only): {c}/{n} = {p:.2f}% -> badge would read {p:.1f}%")

print("\nlowest covered tests/unit files:")
for rel, c, n in sorted(unit, key=lambda r: r[1] / r[2] if r[2] else 1)[:8]:
    print(f"  {rel}: {c}/{n}")
