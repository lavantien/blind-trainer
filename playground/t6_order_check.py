import sys

EDGE_FLOAT = ["UF", "UB", "UR", "UL", "FR", "FL", "DF", "DB", "DR", "DL", "BR", "BL"]
EDGE_CH = ["UR", "UF", "UL", "UB", "DR", "DL", "FR", "FL", "BR", "DF", "BL", "DB"]
CORNER_FLOAT = ["UFR", "UFL", "UBR", "UBL", "DFR", "DFL", "DBR", "DBL"]
CORNER_CH = ["UFR", "UBR", "UFL", "UBL", "DFR", "DFL", "DBR", "DBL"]


def piece(tok):
    return "".join(sorted(tok))


def main(path):
    stats = {}
    for line in open(path, encoding="utf-8"):
        parts = line.rstrip("\n").split("\t")
        if len(parts) != 5:
            continue
        solver, sheet, rowid, pos, alg = parts
        if sheet not in ("edge", "corner"):
            continue
        order = [piece(p) for p in (EDGE_FLOAT if sheet == "edge" else CORNER_FLOAT)]
        order2 = [piece(p) for p in (EDGE_CH if sheet == "edge" else CORNER_CH)]
        toks = pos.split("-")
        pieces = [piece(t) for t in toks]
        key = sheet
        bad, bad2, n = stats.get(key, (0, 0, 0))
        n += 1
        ranks = [order.index(p) for p in pieces]
        ranks2 = [order2.index(p) for p in pieces]
        if ranks[0] != min(ranks):
            bad += 1
            if bad <= 5:
                print("VIOL float", solver, sheet, rowid, pos, ranks)
        if ranks2[0] != min(ranks2):
            bad2 += 1
            if bad2 <= 5:
                print("VIOL ch   ", solver, sheet, rowid, pos, ranks2)
        stats[key] = (bad, bad2, n)
    for key, (bad, bad2, n) in sorted(stats.items()):
        print(key, "rows", n, "float_violations", bad, "ch_violations", bad2)


if __name__ == "__main__":
    main(sys.argv[1])
