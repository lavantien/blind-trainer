import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
SHOTS = REPO / "playground" / "adv-fonts-b" / "shots"
DOCS = REPO / "docs" / "screenshots"


def diff_map(path_a, path_b):
    a = Image.open(path_a).convert("RGB")
    b = Image.open(path_b).convert("RGB")
    pa, pb = a.load(), b.load()
    w, h = a.size
    rows = []
    total = 0
    minx, maxx, miny, maxy = w, 0, h, 0
    for y in range(h):
        n = 0
        for x in range(w):
            if pa[x, y] != pb[x, y]:
                n += 1
                if x < minx:
                    minx = x
                if x > maxx:
                    maxx = x
                if y < miny:
                    miny = y
                if y > maxy:
                    maxy = y
        rows.append(n)
        total += n
    return total, (minx, miny, maxx, maxy), rows


def span(rows, min_n=1):
    spans = []
    at = 0
    while at < len(rows):
        if rows[at] >= min_n:
            start = at
            while at < len(rows) and rows[at] >= min_n:
                at += 1
            spans.append((start, at - 1, sum(rows[start:at])))
        else:
            at += 1
    return spans


def main():
    for name in ["drill-card.png", "drill-reps.png", "drill-rate-a.png", "drill-rate-b.png"]:
        total, bbox, rows = diff_map(SHOTS / name, DOCS / name)
        spans = span(rows, 5)
        print(f"{name}: total={total} bbox={bbox}")
        for s in spans:
            print(f"  diff rows y {s[0]}-{s[1]} px={s[2]}")
    a = Image.open(SHOTS / "algs.png").convert("RGB")
    b = Image.open(SHOTS / "algs-search.png").convert("RGB")
    pa, pb = a.load(), b.load()
    print("algs rows region per-band diff detail:")
    for y in range(90, 780):
        n = 0
        for x in range(20, 510):
            if pa[x, y] != pb[x, y]:
                n += 1
        if n >= 20:
            print(f"  y={y} diff={n}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
