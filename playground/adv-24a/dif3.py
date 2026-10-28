import importlib.util
src = open(r"playground\adv-24a\diffpx_probe.py").read()
head = src.split("diffpx(r")[0]
ns = {}
exec(head, ns)
load = ns["load"]
pairs = [
 ("docs/screenshots/drill-card.png", "docs/screenshots/drill-reps.png", "card vs reps"),
 ("docs/screenshots/memo-stream.png", "docs/screenshots/memo-recall.png", "stream vs recall"),
 ("docs/screenshots/memo-recall.png", "docs/screenshots/memo-review.png", "recall vs review"),
]
for a, b, label in pairs:
    wa, ha, pa = load(a)
    wb, hb, pb = load(b)
    d = 0
    for y in range(ha):
        for x in range(wa):
            oa = (y*wa + x)*4
            if pa[oa:oa+3] != pb[oa:oa+3]:
                d += 1
    print(label, d, "px differ")
