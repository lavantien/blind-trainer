import importlib.util, sys
spec = importlib.util.spec_from_file_location("old_rail2", r"playground\adv-24a\old_rail2.py")
src = open(r"playground\adv-24a\old_rail2.py").read()
head = src.split("for path in sys.argv[1:]")[0]
ns = {}
exec(head, ns)
load = ns["load"]
pairs = [
 (r"playground\adv-24a\old_algs.png", r"docs\screenshots\algs.png"),
 (r"playground\adv-24a\old_algs-search.png", r"docs\screenshots\algs-search.png"),
 (r"playground\adv-24a\old_case-drill-show.png", r"docs\screenshots\case-drill-show.png"),
 (r"playground\adv-24a\old_case-drill-grade.png", r"docs\screenshots\case-drill-grade.png"),
 (r"playground\adv-24a\old_solve-memo.png", r"docs\screenshots\solve-memo.png"),
 (r"playground\adv-24a\old_solve-exec.png", r"docs\screenshots\solve-exec.png"),
]
for old, new in pairs:
    wo, ho, po, cto = load(old)
    wn, hn, pn, ctn = load(new)
    def count(px, w, x0, x1):
        n = 0
        for y in range(100, 800):
            for x in range(x0, x1):
                if px(x, y) != (23, 26, 31):
                    n += 1
        return n
    lo = count(po, wo, 0, 570)
    ln = count(pn, wn, 0, 570)
    ro = count(po, wo, 570, 1280)
    rn = count(pn, wn, 570, 1280)
    name = old.split("\\")[-1]
    print(f"{name:28s} left nonbg old={lo:7d} new={ln:7d}   right nonbg old={ro:7d} new={rn:7d}")
