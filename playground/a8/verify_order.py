import json

cases = json.load(open("f2l_cases.json", encoding="utf-8"))
mine = []
for c in cases[:41]:
    mine.extend(c["algs"])

doc = open("bestf2l_text.txt", encoding="utf-8").read().splitlines()
raw = []
page = 0
started = False
for line in doc:
    line = line.strip()
    if line.startswith("===== page"):
        page = int(line.split()[2])
        continue
    if page < 2 or page > 5:
        continue
    if line.startswith("Section 1"):
        started = True
    if not line or not started:
        continue
    import re

    def is_move(tok):
        tok = tok.rstrip("0123456789").strip("()")
        return re.fullmatch(r"2?[UDLRFBudlrfMESxyz](2?'?)?", tok) is not None

    if not all(is_move(t) for t in line.split()):
        continue
    raw.append(line)

print("mine", len(mine), "raw", len(raw))
ok = len(mine) == len(raw)
for i, (a, b) in enumerate(zip(mine, raw)):
    if " ".join(a.split()) != " ".join(b.split()):
        ok = False
        print("DIFF", i, repr(a), repr(b))
print("exact match:", ok)
