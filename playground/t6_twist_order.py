import sys
import re


def p_of_head(cyc_pairs, piece):
    h = piece
    for stk, pos in cyc_pairs:
        if pos == h:
            return stk
    return None


def main(path):
    edges = []
    rows = 0
    for line in open(path, encoding="utf-8"):
        cols = line.rstrip("\n").split("\t")
        if len(cols) < 8 or cols[1] != "twists":
            continue
        rows += 1
        pos, note = cols[3], cols[7]
        names = [n.strip() for n in pos.split("&")]
        cycs = re.findall(r"cyc\d=\[([^\]]+)\]", note)
        ds = re.findall(r"d\d=([+-]\d)", note)
        pairs_per = []
        for cyc in cycs:
            pairs = []
            for tok in cyc.split():
                stk, pos2 = tok.split(">")
                pairs.append((stk, pos2))
            pairs_per.append(pairs)
        for i, name in enumerate(names):
            p = p_of_head(pairs_per[i], name)
            assert p is not None, (pos, note, name)
            if i > 0:
                prev = p_of_head(pairs_per[i - 1], names[i - 1])
                edges.append((prev, p))
        print(names, [p_of_head(pairs_per[i], names[i]) for i in range(2)], ds)
    print("rows", rows, "edges", len(edges))
    return edges


if __name__ == "__main__":
    main(sys.argv[1])
