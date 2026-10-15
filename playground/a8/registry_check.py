import collections
import json
from pathlib import Path

packs = Path(r"C:\Users\lavantien\dev\github\blind-trainer\ref_ground\packs")
reg = json.loads((packs / "registry.json").read_text(encoding="utf-8"))
counts = collections.Counter()
for e in reg:
    rows = [l.split("\t") for l in (packs / e["file"]).read_text(encoding="utf-8").split("\n") if l][1:]
    assert len(rows) == e["expected_rows"], (e, len(rows))
    assert all(len(r) == 5 and all(r) for r in rows), e
    keys = [r[0] for r in rows]
    assert len(set(keys)) == len(keys), e["file"]
    counts[e["file"]] = len(rows)
print("all", len(reg), "registry rows verified against files")
print("cfop rows:", {k: v for k, v in counts.items() if k.startswith("cfop")})
urls = set()
for f in ["cfop_pll.tsv", "cfop_oll.tsv", "cfop_f2l.tsv"]:
    for l in (packs / f).read_text(encoding="utf-8").split("\n")[1:]:
        if l:
            urls.add(l.split("\t")[3])
print("source urls:", sorted(urls))
